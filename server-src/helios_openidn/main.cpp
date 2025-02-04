#include <stdlib.h>
#include <netinet/in.h> /* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <netdb.h>      /* gethostbyname */
#include <pthread.h>    /* POSIX Threads */
#include <string>
#include <signal.h>

// Project headers
#include "DACHWInterface.h"
#include "HWBridge.h"
#include "SPIDevAdapter.h"
#include "HeliosAdapter.hpp"
#include "DummyAdapter.h"
#include "Service.hpp"
#include "IDNServer.hpp"

#include "ManagementInterface.h"

pthread_t driver_thread = 0;
std::shared_ptr<HWBridge> driver = nullptr;
std::shared_ptr<LaproService> laproService = nullptr;
std::shared_ptr<IDNServer> idnServer = nullptr;


// Helios adapter management
pthread_t management_thread = 0;
std::shared_ptr<ManagementInterface> management = nullptr;


//make the program emit debug information on SIGINT
void sig_handler(int signum) {
	//kill driver
	pthread_cancel(driver_thread);
	pthread_join(driver_thread, NULL);

	system("echo 'heartbeat' > /sys/class/leds/rockpis:blue:user/trigger");

	//output dark point so the laser doesn't linger
	driver->outputEmptyPoint();

	//if we want debugging, give us that
	if (driver->getDebugging() == DEBUG) {
		driver->printStats();
	}

	signal(SIGINT, SIG_DFL);
	raise(SIGINT);
}

void* networkThreadFunction(void* args) {
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	//make the network thread real time but give the priority to the driver thread
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	sched_setscheduler(0, SCHED_RR, &sp);

	idnServer->networkThreadFunc();
	return nullptr;
}

void* driverThreadFunction(void* args) {
	//real time driver thread
	struct sched_param sp;
	memset(&sp, 0, sizeof(sp));
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	sched_setscheduler(0, SCHED_RR, &sp);

	driver->driverLoop();
	return nullptr;
}

void* managementThreadFunction(void* args) {
	management->networkThreadEntry();
	return nullptr;
}

int parseArguments(int argc, char** argv) {
	std::shared_ptr<BEX> bex = std::shared_ptr<BEX>(new BEX);
	double chunkUs = -1;
	double bufferTargetMs = -1;

	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--help") == 0) {
			printf("--<driver>\n");
			printf("\t--dummy\n");
#ifdef BUILD_PI
			printf("\t--bcm\n");
#endif
			printf("\t--spidev\n");
			printf("\t--helios\n");
			printf("--setMaxPointRate [pps]\n");
			printf("--setChunkLengthUs [microseconds]\n");
			printf("--setBufferTargetMs [milliseconds]\n");
			printf("--debug\n");
			printf("--debuglive\n");
			printf("--debugsimple\n");
			exit(0);
		}

#ifdef BUILD_PI
		if (strcmp(argv[i], "--bcm") == 0) {
			if (driver == nullptr) {
				printf("Using the BCM driver\n");
				driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<BCMAdapter>(new BCMAdapter), bex));
			}
			else {
				driver = nullptr;
				return -1;
			}

			continue;
		}
#endif

		if (strcmp(argv[i], "--spidev") == 0) {
			if (driver == nullptr) {
				printf("Using the Spidev driver\n");
				driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<SPIDevAdapter>(new SPIDevAdapter), bex));
			}
			else {
				driver = nullptr;
				return -1;
			}

			continue;
		}

		if (strcmp(argv[i], "--helios") == 0) {
			if (driver == nullptr) {
				printf("Using the helios driver\n");
				driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<HeliosAdapter>(new HeliosAdapter), bex));
			}
			else {
				driver = nullptr;
				return -1;
			}

			continue;
		}

		if (strcmp(argv[i], "--dummy") == 0) {
			if (driver == nullptr) {
				printf("Using the DUMMY driver!!!!!!!!!\n");
				driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<DummyAdapter>(new DummyAdapter), bex));
			}
			else {
				driver = nullptr;
				return -1;
			}

			continue;
		}

		if (driver == nullptr) {
			printf("Using the helios driver (by default)\n");
			driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<HeliosAdapter>(new HeliosAdapter), bex));
		}

		if (strcmp(argv[i], "--setMaxPointRate") == 0) {
			unsigned rate = std::stoi(argv[i + 1]);
			printf("Changed MaxPointRate to %d pps\n", rate);
			driver->getDevice()->setMaxPointrate(rate);
			i++;
			continue;
		}

		if (strcmp(argv[i], "--setChunkLengthUs") == 0) {
			chunkUs = (double)std::stoi(argv[i + 1]);
			printf("Changed ChunkLengthUs to %f us\n", chunkUs);
			i += 1;
			continue;
		}

		if (strcmp(argv[i], "--setBufferTargetMs") == 0) {
			bufferTargetMs = (double)std::stoi(argv[i + 1]);
			driver->setBufferTargetMs(bufferTargetMs);
			printf("Changed BufferTarget to %f ms\n", bufferTargetMs);
			i += 1;
			continue;
		}
		if (strcmp(argv[i], "--debug") == 0) {
			driver->setDebugging(DEBUG);
			printf("Activated driver DEBUG\n");
			continue;
		}

		if (strcmp(argv[i], "--debuglive") == 0) {
			driver->setDebugging(DEBUGLIVE);
			printf("Activated driver DEBUGLIVE\n");
			continue;
		}

		if (strcmp(argv[i], "--debugsimple") == 0) {
			driver->setDebugging(DEBUGSIMPLE);
			printf("Activated driver DEBUGSIMPLE\n");
			continue;
		}

		printf("Unknown parameter: %s - exiting!\n", argv[i]);
		exit(-1);
	}

	// For development
#ifndef NDEBUG
	driver->setDebugging(DEBUGSIMPLE);
	printf("Activated driver DEBUGSIMPLE\n");
#endif

	//default (won't execute in this build, helios is set as default earlier in this file)
	if (driver == nullptr) {
		printf("Using the DUMMY driver!!!!!!!!!\n");
		driver = std::shared_ptr<HWBridge>(new HWBridge(std::shared_ptr<DummyAdapter>(new DummyAdapter), bex));
	}

	laproService = std::shared_ptr<LaproService>(new LaproService(driver, bex));
	if (chunkUs != -1) laproService->setChunkLengthUs(chunkUs);

	idnServer = std::shared_ptr<IDNServer>(new IDNServer(laproService));


	return 0;
}

int main(int argc, char** argv) {
	signal(SIGINT, sig_handler);

	system("echo 'none' > /sys/class/leds/rockpis:blue:user/trigger"); // manual blue LED control, stops heartbeat blinking
	system("echo 0 > /sys/class/leds/rockpis:blue:user/brightness"); // turn LED off

	int parsingRet = parseArguments(argc, argv);
	if (parsingRet != 0) {
		printf("Argument error: ");
		if (parsingRet == -1) {
			printf("Specified more than one driver.\n");
		}

		if (parsingRet == -2) {
			printf("No driver specified.\n");
		}

		return -1;
	}

	// Management specific to Helios OpenIDN product
	management = std::shared_ptr<ManagementInterface>(new ManagementInterface());
	management->readAndStoreNewSettingsFile();
	management->readSettingsFile();
	if (pthread_create(&management_thread, NULL, &managementThreadFunction, NULL) != 0) {
		printf("ERROR CREATING MANAGEMENT THREAD\n");
		return -1;
	}

	management->idnServer = idnServer;
	memcpy(idnServer->hostName, management->settingIdnHostname.c_str(), management->settingIdnHostname.size() < HOST_NAME_SIZE ? management->settingIdnHostname.size() : HOST_NAME_SIZE);

	std::atomic<int> atom(1);
	printf("lockless atomics: ");
	printf(atom.is_lock_free() ? "true\n" : "false\n");

	if (management->settingEnableIdnServer)
	{
		/* Startup Driver Thread */
		if (pthread_create(&driver_thread, NULL, &driverThreadFunction, NULL) != 0) {
			printf("ERROR CREATING DRIVER THREAD\n");
			return -1;
		}

		system("echo 1 > /sys/class/leds/rockpis:blue:user/brightness"); // turn LED on continuously

		networkThreadFunction(NULL);
	}
	else
		while (1);

	return(0);
}
