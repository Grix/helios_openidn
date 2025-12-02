// Standard libraries
#include <stdlib.h>
#include <netinet/in.h>   /* INET constants and stuff */
#include <arpa/inet.h>    /* IP address conversion stuff */
#include <netdb.h>        /* gethostbyname */
#include <pthread.h>      /* POSIX Threads */
#include <string>
#include <csignal>
#include <vector>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <atomic>
#include  <optional>
#include <iostream>

#include <cstdarg> 
#include <cstdio>

// Project headers
#include "../shared/DACHWInterface.hpp"
#include "../shared/HWBridge.hpp"

#include "../hardware/Helios/HeliosAdapter.hpp"
#include "../hardware/HeliosPro/HeliosProAdapter.hpp"
#include "../dummy/DummyAdapter.hpp"

#include "../output/V1LaproGraphOut.hpp"

#include "../server/IDNLaproService.hpp"
#include "SockIDNServer.hpp"

#include "../ManagementInterface.hpp"
#include "../UsbInterface.hpp"

std::vector<std::shared_ptr<HWBridge>> driverObjects;
std::vector<RTOutput *> rtOutputs;
LLNode<ServiceNode> *firstService = nullptr;
std::shared_ptr<SockIDNServer> idnServer = nullptr;
std::vector<pthread_t> driverThreads;

// Helios adapter management
pthread_t management_thread = 0;
ManagementInterface* management = nullptr;

bool debug = false;
int debug_ctr = 0;

inline void debug_printf(bool critical, const char* fmt, ...) {
    if (debug) {
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
    } else if(critical) {
        debug_ctr += 1;
    }
}

// Signal handler function for SIGINT: Cancel and join all driver threads.
void sig_handler(int sig) {
    char message[256]; // A buffer to hold the message with the count
    snprintf(message, sizeof(message), "\nCaught SIGINT (Ctrl+C). Exiting gracefully.\n");
    write(STDOUT_FILENO, message, strlen(message));
    if (idnServer != nullptr) 
        idnServer->stopServer();
    if (management != nullptr)
    {
        if (management->getHardwareType() == HARDWARE_ROCKPIS)
            system("echo 'heartbeat' > /sys/class/leds/rockpis:blue:user/trigger");
        management->unmountUsbDrive();
    }
}

void* driverThreadFunction(void* args) {
    HWBridge* driver = static_cast<HWBridge*>(args);

    // Make all writes in other threads visible in the current thread (process cache invalidate queue)
    #if __cplusplus >= 201103L
        atomic_thread_fence(std::memory_order_acquire);
    #endif

    // High priority thread
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR) + (sched_get_priority_max(SCHED_RR) - sched_get_priority_min(SCHED_RR)) / 3;
    sched_setscheduler(0, SCHED_RR, &sp);

    driver->driverLoop();

    return nullptr;
}

void createDriverThreads() {
    driverThreads.clear();

    // Make all writes in the current thread visible in other threads
    #if __cplusplus >= 201103L
        atomic_thread_fence(std::memory_order_release);
    #endif

    debug_printf(false, "Starting %d drivers", driverObjects.size());
    for (size_t i = 0; i < driverObjects.size(); ++i) {
        pthread_t thread;
        if (pthread_create(&thread, nullptr, &driverThreadFunction, driverObjects[i].get()) != 0) {
            
            printf("ERROR CREATING DRIVER THREAD FOR driver %zu\n", i);
        } else {
            debug_printf(false, "Driver thread created for driver %zu\n", i);
            driverThreads.push_back(thread);
        }
    }
}

// ===================================================================
// Helper: Simple INI parser for services.ini
// ===================================================================

struct ServiceConfig {
    int serviceID;
    std::string serviceType;
    bool isDefault;
    std::string serviceName;
    std::string dacType;   // Should be one of "Helios", "EtherDream", "Dummy"
    std::string deviceId;  // Identifier to be looked-up for Helios/EtherDream
    int maxPointRate = -1;
    int bufferTargetMs = -1;
    int chunkLengthUs = -1;
 
};

std::map<std::string, ServiceConfig> readServicesIni(const std::string& filename) {
    std::map<std::string, ServiceConfig> servicesConfig;
    std::ifstream infile(filename);
    if (!infile) {
        throw std::runtime_error("Failed to open " + filename);
    }

    std::string line;
    std::string currentSection;
    ServiceConfig currentConfig;
    while (std::getline(infile, line)) {
        // Trim whitespace.
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines or comments.
        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        if (line.front() == '[' && line.back() == ']') {
            // New section; if parsing a previous section, save it.
            if (!currentSection.empty()) {
                servicesConfig[currentSection] = currentConfig;
            }
            currentSection = line.substr(1, line.size() - 2);
            // Reset currentConfig defaults.
            currentConfig = ServiceConfig();
            continue;
        }

        // Parse key=value.
        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos)
            continue;
        std::string key = line.substr(0, equalsPos);
        std::string value = line.substr(equalsPos + 1);
        // Trim extra whitespace.
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "serviceID")
            currentConfig.serviceID = std::stoi(value);
        else if (key == "serviceType")
            currentConfig.serviceType = value;
        else if (key == "default")
            currentConfig.isDefault = (value == "true" || value == "1");
        else if (key == "serviceName")
            currentConfig.serviceName = value;
        else if (key == "dacType")
            currentConfig.dacType = value;
        else if (key == "deviceId")
            currentConfig.deviceId = value;
        else if (key == "maxPointRate")
            currentConfig.maxPointRate = std::stoi(value);
        else if (key == "bufferTargetMs")
            currentConfig.bufferTargetMs = std::stoi(value);
        else if (key == "chunkLengthUs")
            currentConfig.chunkLengthUs = std::stoi(value);
    }
    // Add the last section if it exists.
    if (!currentSection.empty()) {
        servicesConfig[currentSection] = currentConfig;
    }
    return servicesConfig;
}

IDNLaproService* createLaProService(std::shared_ptr<DACHWInterface> adapter, const std::string& name, const int id, const bool isDefaultService,
std::optional<int> maxPointRate = std::nullopt, std::optional<int> bufferTargetMs = std::nullopt, std::optional<int> chunkLengthUs = std::nullopt) {

    if(maxPointRate) {
        adapter->setMaxPointrate(maxPointRate.value());
        printf("[Service %d]: Starting with changed Point Rate: %d \n", id, maxPointRate.value());
    }

    auto driverObj = std::make_shared<HWBridge>(adapter);

    if(bufferTargetMs) {
        driverObj->setBufferTargetMs(bufferTargetMs.value());
        printf("[Service %d]: Starting with changed Buffer Target (MS): %d \n", id, bufferTargetMs.value());
    }

    if(chunkLengthUs) {
        driverObj->setChunkLengthUs(chunkLengthUs.value());
        printf("[Service %d]: Starting with changed Chunk Length (Us): %d \n", id, chunkLengthUs.value());
    }

    driverObjects.push_back(driverObj);

    auto laproGraphicOut = new V1LaproGraphicOutput(adapter);

    rtOutputs.push_back(laproGraphicOut);

    char* serviceName = strdup(name.c_str());

#ifndef NDEBUG
    printf("Creating service %s ID %d, default %d\n", serviceName, id, isDefaultService);
#endif
    auto laproService = new IDNLaproService(id, serviceName, isDefaultService, laproGraphicOut);

    laproService->linkinLast(&firstService);

    management->devices.push_back(adapter);
    management->outputs.push_back(laproGraphicOut);
    management->driverBridges.push_back(driverObj);

    return laproService;
}

void* managementThreadFunction(void* args) {

    management->networkThreadEntry();
    return nullptr;
}

int parseArguments(int argc, char** argv) {

    // Process command-line options.
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("--<driver>\n");
            printf("\t--dummy\n");
            printf("\t--helios\n");
            if (management->getHardwareType() == HARDWARE_ROCKS0)
                printf("\t--heliospro\n");
            printf("\t--multiservice [filename / automap]\n");
            printf("--list-available-devices\n");
            printf("--dump\n");
            printf("--setMaxPointRate [pps]\n");
            printf("--setChunkLengthUs [microseconds]\n");
            printf("--setBufferTargetMs [milliseconds]\n");
            printf("--debug\n");
            printf("--debuglive\n");
            printf("--debugsimple\n");
            exit(0);
        }

        // ------------------------------------------------------------------
        // Multi-service mode: Read services.ini and create one service per section.
        // ------------------------------------------------------------------
        if (strcmp(argv[i], "--multiservice") == 0) {

            // Automap Mode
            if ((i + 1 < argc) && strcmp(argv[i + 1], "automap") == 0) {
                i++;

                //Helios

                try {
                    HeliosAdapter::initialize();

                    std::map<std::string, int> available = HeliosAdapter::getAvailableDevices();

                    if (!available.empty()) {
                        for(const auto& [name, id] : available) {
                            int serviceID = 1;
                            for(LLNode<ServiceNode> *node = firstService; node != nullptr; node = node->getNextNode()) serviceID++;
                            createLaProService(std::make_shared<HeliosAdapter>(id), name, serviceID, true);
                            printf("Started Helios device: %s \n", name.c_str());
                        }
                    } else {
                        HeliosAdapter::shutdown();
                    }
                    
                } catch (const std::exception &e) {
                    printf("HeliosAdapter initialization failed: %s\n", e.what());
                    continue;
                }


                continue;
                

            // Config Mode
            } else {
                std::map<std::string, ServiceConfig> iniServices;

                bool heliosInit = false;
                bool etherdreamInit = false;

                std::string filename = "services.ini";

                if ((i + 1 < argc) && !(std::string(argv[i + 1]).rfind("--", 0) == 0)) {
                    printf("Using custom file ", std::string(argv[i + 1]), "\n");
                    filename = std::string(argv[i + 1]);
                    i++;
                } 
                

                try {
                    iniServices = readServicesIni(filename);
                } catch (const std::exception &e) {
                    printf("Error reading services file: %s\n", e.what());
                    return -1;
                }

                // Process each service section.
                for (const auto& [section, config] : iniServices) {
                    std::shared_ptr<DACHWInterface> dachw = nullptr;

                    // Based on dacType, create the appropriate adapter.
                    if (config.dacType == "Helios") {

                        if(!heliosInit) {
                            try {
                                HeliosAdapter::initialize();
                                heliosInit = true;
                            } catch (const std::exception &e) {
                                printf("HeliosAdapter initialization failed: %s\n", e.what());
                                continue;
                            }
                            
                        }

                        std::map<std::string, int> available = HeliosAdapter::getAvailableDevices();
                        auto device = available.find(config.deviceId);

                        if (device == available.end()) {
                            printf("Service [%s]: Helios device '%s' not found!\n",
                                section.c_str(), config.deviceId.c_str());
                            continue; // Skip this service.
                        }

                        dachw =  std::make_shared<HeliosAdapter>(device->second);
                        
                    }

                    else if (config.dacType == "Dummy") {
                        dachw = std::make_shared<DummyAdapter>();
                    }

                    else {
                        printf("Service [%s]: Unknown dacType '%s'\n", section.c_str(), config.dacType.c_str());
                        continue;
                    }

                    // Duplicate serviceName string
                    char* serviceNameCStr = new char[config.serviceName.size() + 1];
                    std::strcpy(serviceNameCStr, config.serviceName.c_str());

                    std::optional<int> maxPointRate = (config.maxPointRate != -1) ? std::make_optional(config.maxPointRate) : std::nullopt;
                    std::optional<int> bufferTargetMs = (config.bufferTargetMs != -1) ? std::make_optional(config.bufferTargetMs) : std::nullopt;
                    std::optional<int> chunkLengthUs = (config.chunkLengthUs != -1) ? std::make_optional(config.chunkLengthUs) : std::nullopt;

                    createLaProService(dachw, serviceNameCStr, config.serviceID, config.serviceID == 1, maxPointRate, bufferTargetMs, chunkLengthUs);


                    printf("Added service [%s] with serviceID: %d using %s adapter\n",
                        section.c_str(), config.serviceID, config.dacType.c_str());
                }
                continue;
            }
        }

        if (strcmp(argv[i], "--list-available-devices") == 0) {
            try {
                // Initialize Helios devices
                HeliosAdapter::initialize();
                std::map<std::string, int> heliosDevices = HeliosAdapter::getAvailableDevices();
                printf("Available Helios Devices:\n");
                for (const auto& [name, index] : heliosDevices) {
                    printf("  Name: %s, Type: Helios\n", name.c_str());
                }
            } catch (const std::exception& e) {
                printf("HeliosAdapter initialization failed: %s\n", e.what());
            }

            // Exit after listing the devices
            exit(0);
        }

        if (strcmp(argv[i], "--dump") == 0) {
            try {
                // Initialize Helios devices
                HeliosAdapter::initialize();
                std::map<std::string, int> heliosDevices = HeliosAdapter::getAvailableDevices();
            } catch (const std::exception& e) {
                printf("HeliosAdapter initialization failed: %s\n", e.what());
            }
        
            printf("\n\n\n");
        
            std::map<std::string, int> heliosDevices = HeliosAdapter::getAvailableDevices();

            std::optional<HeliosAdapter> dummyHelios;

            if (heliosDevices.size() > 0) {
                dummyHelios.emplace(0);
            }
        
            int serviceCount = 1;
            
            // Print Helios devices
            int heliosIndex = 1;
            for (const auto& [name, index] : heliosDevices) {
                printf("[Service%d]\n", serviceCount);
                printf("serviceID = %d\n", serviceCount++);
                printf("serviceType = lapro\n");
                printf("default = false\n");
                printf("serviceName = Helios %d\n", heliosIndex++);
                printf("dacType = Helios\n");
                printf("deviceId = %s\n", name.c_str());
                if(dummyHelios) {
                    printf("maxPointRate = %d\n", dummyHelios.value().maxPointrate());
                }
                // TODO: Extract these values from HWBridge.hpp / DACHWInterface.hpp
                printf("bufferTargetMs = %d\n", 40);
                printf("chunkLengthUs = %d\n", 10000);
                printf("\n");
            }
        
        
            exit(0);
        }
        

        // ------------------------------------------------------------------
        // Single-service mode: Use --helios, --etherdream, or --dummy options.
        // Each driver flag creates one driver and one service.
        // ------------------------------------------------------------------

        //#ifdef INCLUDE_HELIOS
        if (strcmp(argv[i], "--helios") == 0 || strcmp(argv[i], "--heliospro") == 0) {
        
            bool isHeliosPro = (management->getHardwareType() == HARDWARE_ROCKS0) && (strcmp(argv[i], "--heliospro") == 0);

            if (isHeliosPro)
            {
                try {
                    printf("Using the HeliosPRO driver\n");
                    auto device = std::make_shared<HeliosProAdapter>();
                    char name[32];
                    device->getName(name, 32);
                    createLaProService(device, std::string(name), 1, true);
                }
                catch (const std::exception& e) {
                    printf("HeliosPRO driver error: %s\n", e.what());
                    //
                }
            }
            else
            {
                HeliosAdapter::firstServiceIsAlwaysVisible = true;//setFirstServiceIsAlwaysVisible();
            }

            try {

                HeliosAdapter::initialize();
                //std::map<std::string, int> available = HeliosAdapter::getAvailableDevices();

                printf("Using the Helios driver, creating two placeholder services.\n");

                char heliosName[32] = {0};
                auto heliosAdapter = std::make_shared<HeliosAdapter>(0);
                heliosAdapter->getName(heliosName, 32);
                HeliosAdapter::setFirstDeviceService( createLaProService(heliosAdapter, heliosName, isHeliosPro ? 2 : 1, !isHeliosPro) );
                heliosAdapter = std::make_shared<HeliosAdapter>(1);
                memset(heliosName, 0, 32);
                heliosAdapter->getName(heliosName, 32);
                HeliosAdapter::setSecondDeviceService( createLaProService(heliosAdapter, heliosName, isHeliosPro ? 3 : 2, false) );


                //createLaProService(std::make_shared<HeliosAdapter>(available.begin()->second), available.begin()->first, 1);

                /*if (available.empty()) {
                    printf("No Helios devices available!\n");
                    return -1;
                }
                else
                {
                    printf("Using the Helios driver, device: %s\n", available.begin()->first.c_str());
                    createLaProService(std::make_shared<HeliosAdapter>(available.begin()->second), available.begin()->first, 1);
                }*/
            } catch (const std::exception &e) {
                printf("Helios driver error: %s\n", e.what());
                return -1;
            }
            continue;
        }

        //#endif


        //#ifdef INCLUDE_DUMMY
        if (strcmp(argv[i], "--dummy") == 0) {
            printf("Using the Dummy driver, device: Dummy\n");
            createLaProService(std::make_shared<DummyAdapter>(), "Dummy", 1, true);
            continue;
        }
        //#endif

        // ------------------------------------------------------------------
        // Options applied to all drivers in driverObjects.
        // ------------------------------------------------------------------
        if (strcmp(argv[i], "--setMaxPointRate") == 0) {
            unsigned rate = std::stoi(argv[i + 1]);
            printf("Changed MaxPointRate to %d pps for all drivers\n", rate);
            for (auto &drv : driverObjects) {
                drv->getDevice()->setMaxPointrate(rate);
            }
            i++;
            continue;
        }

        if (strcmp(argv[i], "--setChunkLengthUs") == 0) {
            double chunkUs = (double)std::stoi(argv[i + 1]);
            printf("Changed ChunkLengthUs to %f us\n", chunkUs);

            for (auto &driverObj : driverObjects) {
                driverObj->setChunkLengthUs(chunkUs);
            }

            i++;
            continue;
        }

        if (strcmp(argv[i], "--setBufferTargetMs") == 0) {
            double bufferTargetMs = (double)std::stoi(argv[i + 1]);
            for (auto &drv : driverObjects) {
                drv->setBufferTargetMs(bufferTargetMs);
            }
            printf("Changed BufferTarget to %f ms for all drivers\n", bufferTargetMs);
            i++;
            continue;
        }

        if (strcmp(argv[i], "--debug") == 0) {
            debug = true;

            for (auto &drv : driverObjects) {
                drv->setDebugging(DEBUG);
            }
            printf("Activated driver DEBUG for all drivers\n");
            continue;
        }

        if (strcmp(argv[i], "--debuglive") == 0) {
            for (auto &drv : driverObjects) {
                drv->setDebugging(DEBUGLIVE);
            }
            printf("Activated driver DEBUGLIVE for all drivers\n");
            continue;
        }

        if (strcmp(argv[i], "--debugsimple") == 0) {
            for (auto &drv : driverObjects) {
                drv->setDebugging(DEBUGSIMPLE);
            }
            printf("Activated driver DEBUGSIMPLE for all drivers\n");
            continue;
        }

        printf("Unknown parameter: %s - exiting!\n", argv[i]);
        exit(-1);
    }

    // Default: If no driver was specified, use dummy.
    if (driverObjects.empty()) 
    {
        printf("No driver specified, ");
        if (management->getHardwareType() == HARDWARE_ROCKS0)
        {
            try {
                printf("using the HeliosPRO driver\n");
                auto device = std::make_shared<HeliosProAdapter>();
                char name[32];
                device->getName(name, 32);
                createLaProService(device, std::string(name), 1, true);
            }
            catch (const std::exception& e) {
                printf("HeliosPRO driver error: %s\n", e.what());
                return -1;
            }
        }
        else
        {
            try {
                printf("using the Helios driver\n");
                HeliosAdapter::initialize();
                std::map<std::string, int> available = HeliosAdapter::getAvailableDevices();

                if (available.empty()) {
                    printf("No Helios devices available!\n");
                    return -1;
                }
                createLaProService(std::make_shared<HeliosAdapter>(available.begin()->second), available.begin()->first, 1, true);
            }
            catch (const std::exception& e) {
                printf("Helios driver error: %s\n", e.what());
                return -1;
            }
        }
    }

    // Create the server, pass the list of services.
    idnServer = std::make_shared<SockIDNServer>(firstService);

    return 0;
}

int main(int argc, char** argv) {

    // Register the signal handler for SIGINT.
    std::signal(SIGINT, sig_handler);

    // Management specific to Helios OpenIDN product
    management = new ManagementInterface();
    management->readAndStoreUsbFiles();

    int parsingRet = parseArguments(argc, argv);
    if (parsingRet != 0) {
        printf("Argument error: %d\n", parsingRet);
        return -1;
    }

    management->readSettingsFile();
    if (pthread_create(&management_thread, NULL, &managementThreadFunction, NULL) != 0) {
        printf("ERROR CREATING MANAGEMENT THREAD\n");
        return -1;
    }

    management->idnServer = idnServer;
    idnServer->setHostName((uint8_t*)management->settingIdnHostname.c_str(), management->settingIdnHostname.length() + 1);
    printf("Hostname is: %s\n", management->settingIdnHostname.c_str());


    std::atomic<int> atom(1);
    printf("lockless atomics: %s\n", atom.is_lock_free() ? "true" : "false");

    // Start driver threads for each HWBridge instance created.
    createDriverThreads();

    UsbInterface* usbInterface = new UsbInterface();

    // High priority thread
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR) + (sched_get_priority_max(SCHED_RR) - sched_get_priority_min(SCHED_RR)) / 4;
    sched_setscheduler(0, SCHED_RR, &sp);

    management->runStartup();

    // Run the network thread on the main thread.
    idnServer->networkThreadFunc();

    // Cancel and join each driver thread.
    for (size_t i = 0; i < driverThreads.size(); i++)
    {
        pthread_cancel(driverThreads[i]);
        pthread_join(driverThreads[i], nullptr);
    }

    printf("%d critical messages were generated.\n", debug_ctr);

    // Clear the driver list.
    driverObjects.clear();

    // Delete outputs
    for (auto rtOutput : rtOutputs) delete rtOutput;
    rtOutputs.clear();

    // Delete services
    while (firstService)
    {
        LLNode<ServiceNode>* node = firstService;
        IDNService* service = static_cast<IDNService*>(node);

        service->linkout();
        delete(service);
    }


    return 0;
}
