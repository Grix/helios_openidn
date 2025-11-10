#include "ManagementInterface.hpp"

//class FilePlayer;

#include "FilePlayer.hpp"
#include <v2/gui/menu.h>

FilePlayer filePlayer;

#define UDP_MAXBUF 8192



ManagementInterface::ManagementInterface()
{
	//filePlayer.devices = &devices;
	//filePlayer.outputs = &outputs;

	mountUsbDrive();

	if (getHardwareType() == HARDWARE_ROCKS0)
	{
		//graphicsEngine = new GraphicsEngine(*display);
		/*graphicsEngine->begin();
		graphicsEngine->setFrameRate(20);
		graphicsEngine->getCanvas().clear();
		//display->drawLine(10, 10, 100, 40);
		graphicsEngine->getCanvas().setFixedFont(ssd1306xled_font8x16);
		graphicsEngine->getCanvas().printFixed(10, 10, "HelloWorld");
		graphicsEngine->getCanvas().drawLine(10, 30, 110, 30);
		//graphicsEngine->getCanvas().setTextCursor(10, 10);
		//graphicsEngine->getCanvas().write("Hello");
		graphicsEngine->refresh();*/

		// TODO: this is a stupid way of doing this:
		system("echo 'none' > /sys/class/leds/rock-s0:green:power/trigger"); // manual internal LED control, stops heartbeat blinking
		system("echo 0 > /sys/class/leds/rock-s0:green:power/brightness"); // turn LED off

		system("echo 0 > /sys/class/leds/rock-s0:green:user3/brightness"); // turn LED off.
		system("echo 0 > /sys/class/leds/rock-s0:red:user4/brightness"); // turn LED off
		system("echo 0 > /sys/class/leds/rock-s0:orange:user5/brightness"); // turn LED off

		if (pthread_create(&keyboardThread, NULL, &keyboardThreadFunction, NULL) != 0) {
			printf("WARNING: failed to create keyboard thread, buttons will not work\n");
		}
	}
	else if (getHardwareType() == HARDWARE_ROCKPIS)
	{
		system("echo 'none' > /sys/class/leds/rockpis:blue:user/trigger"); // manual blue LED control, stops heartbeat blinking
		system("echo 0 > /sys/class/leds/rockpis:blue:user/brightness"); // turn LED off
	}
}

/// <summary>
/// Looks for a file "settings.ini" on a USB drive connected to the computer, and if it exists, applies the settings there. 
/// See example_settings.ini in the source files for how the file could look. 
/// For this function to work, a folder must have been created at /media/usbdrive, and the USB drive must always be assigned by the system as /dev/sda1
/// </summary>
void ManagementInterface::readAndStoreUsbFiles()
{
	printf("Checking for new USB settings.\n");

	char command[256];

	try
	{
		if (!std::filesystem::exists(newSettingsPath))
		{
			return;
		}
	}
	catch (std::filesystem::filesystem_error)
	{
		unmountUsbDrive();
		return;
	}

	printf("New settings file found! Replacing previous settings...\n");
	//sprintf(command, "cp %s %s", newSettingsPath.c_str(), (newSettingsPath + "_backup").c_str());
	//system(command);
	sprintf(command, "cp %s %s", newSettingsPath.c_str(), settingsPath.c_str());
	system(command);


	printf("Finished checking new USB settings.\n");
}

void ManagementInterface::runStartup()
{
	filePlayer.startup();
}

/// <summary>
/// Reads and applies the settings file permanently stored on the device at path settingsPath.
/// </summary>
void ManagementInterface::readSettingsFile()
{
	printf("Reading main settings file.\n");

	char command[256];
	bool shouldRewrite = false;

	try
	{
		if (!std::filesystem::exists(settingsPath))
		{
			printf("WARNING: Could not find/open main settings file.\n");
			return;
		}
	}
	catch (std::filesystem::filesystem_error)
	{
		printf("WARNING: Could not find/open main settings file.\n");
		return;
	}
	mINI::INIFile file(settingsPath);
	mINI::INIStructure ini;
	if (!file.read(ini))
	{
		printf("WARNING: Could not find/open main settings file.\n");
		return;
	}

	printf("Main settings file found as expected. Applying...\n");

	std::string& ignore_network_settings = ini["network"]["already_applied"];

	if (ignore_network_settings != "true")
	{
		// Ethernet network config
		const char* ethernetConnectionId = "\"Wired connection 1\"";
		std::string& eth0_ip_addresses = ini["network"]["ethernet_ip_addresses"];
		if (!eth0_ip_addresses.empty())
		{
			sleep(10); // To make sure nmcli has started before we try to use it

			eth0_ip_addresses.erase(std::remove(eth0_ip_addresses.begin(), eth0_ip_addresses.end(), '\"'), eth0_ip_addresses.end()); // Remove quote marks

			sprintf(command, "nmcli connection down %s", ethernetConnectionId);
			system(command);
			if (eth0_ip_addresses == "auto" || eth0_ip_addresses == "dhcp" || eth0_ip_addresses == "default")
			{
				printf("eth0 DHCP\n");
				sprintf(command, "nmcli connection modify %s ipv4.method auto", ethernetConnectionId);
				system(command);
				sprintf(command, "nmcli connection modify %s ipv4.addresses \"\"", ethernetConnectionId);
				system(command);
			}
			else
			{
				if (eth0_ip_addresses.find('/') == std::string::npos)
					eth0_ip_addresses = eth0_ip_addresses.append("/24"); // 255.255.255.0 as default netmask

				printf("eth0 %s\n", eth0_ip_addresses.c_str());
				sprintf(command, "nmcli connection modify %s ipv4.method manual", ethernetConnectionId);
				system(command);
				sprintf(command, "nmcli connection modify %s ipv4.addresses \"%s\"", ethernetConnectionId, eth0_ip_addresses.c_str());
				system(command);
			}
			sprintf(command, "nmcli connection up %s", ethernetConnectionId);
			system(command);
		}

		// Wifi network config
		std::string& wlan0_enable = ini["network"]["wifi_enable"];
		std::string& wlan0_ssid = ini["network"]["wifi_ssid"];
		if ((wlan0_enable.empty() && !wlan0_ssid.empty()) || wlan0_enable == "true" || wlan0_enable == "True" || wlan0_enable == "\"true\"" || wlan0_enable == "\"True\"")
		{
			std::string& wlan0_ip_addresses = ini["network"]["wifi_ip_addresses"];
			if (!wlan0_ssid.empty() && !wlan0_ip_addresses.empty())
			{
				sleep(20); // To make sure nmcli has started before we try to use it. Extra long delay needed for wifi.

				std::string& wlan0_password = ini["network"]["wifi_password"];

				wlan0_ip_addresses.erase(std::remove(wlan0_ip_addresses.begin(), wlan0_ip_addresses.end(), '\"'), wlan0_ip_addresses.end()); // Remove quote marks

				if (wlan0_password.empty())
					sprintf(command, "nmcli device wifi connect \"%s\" name \"Wifi connection 1\"", wlan0_ssid.c_str());
				else
					sprintf(command, "nmcli device wifi connect \"%s\" password \"%s\" name \"Wifi connection 1\"", wlan0_ssid.c_str(), wlan0_password.c_str());
				system(command);

				if (wlan0_ip_addresses == "auto" || wlan0_ip_addresses == "dhcp" || wlan0_ip_addresses == "default")
				{
					printf("wlan0 DHCP, %s\n", wlan0_ssid.c_str());
					sprintf(command, "nmcli connection modify \"%s\" ipv4.method auto", wlan0_ssid.c_str());
					system(command);
					sprintf(command, "nmcli connection modify \"%s\" ipv4.addresses \"\"", wlan0_ssid.c_str());
					system(command);
				}
				else
				{
					if (wlan0_ip_addresses.find('/') == std::string::npos)
						wlan0_ip_addresses = wlan0_ip_addresses.append("/24"); // 255.255.255.0 as default netmask

					printf("wlan0 %s, %s\n", wlan0_ip_addresses.c_str(), wlan0_ssid.c_str());
					sprintf(command, "nmcli connection modify \"%s\" ipv4.method manual", wlan0_ssid.c_str());
					system(command);
					sprintf(command, "nmcli connection modify \"%s\" ipv4.addresses \"%s\"", wlan0_ssid.c_str(), wlan0_ip_addresses.c_str());
					system(command);
				}
				sprintf(command, "nmcli connection down \"%s\"", wlan0_ssid.c_str());
				system(command);
				sprintf(command, "nmcli connection up \"%s\"", wlan0_ssid.c_str());
				system(command);

			}
		}
		else
		{
			printf("wifi disabled\n");
			system("nmcli connection delete \"Wifi connection 1\"");
			if (!wlan0_ssid.empty())
			{
				sprintf(command, "nmcli connection delete \"%s\"", wlan0_ssid.c_str()); // Backwards compatibility, previously the connection name was the ssid
				system(command);
			}
		}

		ini["network"]["already_applied"] = std::string("true");
		shouldRewrite = true;
	}

	std::string& idn_hostname = ini["idn_server"]["name"];
	if (!idn_hostname.empty())
		settingIdnHostname = idn_hostname;

	std::string& buffer_duration = ini["output"]["buffer_duration"];
	try
	{
		if (!buffer_duration.empty())
		{
			for (int i = 0; i > driverBridges.size(); i++)
				driverBridges[i]->setBufferTargetMs(std::stoi(buffer_duration));
		}
	}
	catch (...)
	{
		printf("Failed to parse buffer_duration setting, must be a number\n");
	}

	try 
	{
		std::string& idn_mode_priority = ini["mode_priority"]["idn"];
		if (!idn_mode_priority.empty())
			modePriority[OUTPUT_MODE_IDN] = std::stoi(idn_mode_priority);

		std::string& usb_mode_priority = ini["mode_priority"]["usb"];
		if (!usb_mode_priority.empty())
			modePriority[OUTPUT_MODE_USB] = std::stoi(usb_mode_priority);

		std::string& dmx_mode_priority = ini["mode_priority"]["dmx"];
		if (!dmx_mode_priority.empty())
			modePriority[OUTPUT_MODE_DMX] = std::stoi(dmx_mode_priority);

		std::string& file_mode_priority = ini["mode_priority"]["file"];
		if (!file_mode_priority.empty())
			modePriority[OUTPUT_MODE_FILE] = std::stoi(file_mode_priority);
	}
	catch (...)
	{
		printf("Failed to parse mode priority settings, must be numbers\n");
	}

	filePlayer.readSettings(ini);

	if (shouldRewrite)
	{
		file.write(ini);
		// Todo reboot if network settings has changed
	}

	printf("Finished reading main settings.\n");

	display.FinishInitialization();
	system("echo 1 > /sys/class/leds/rock-s0:orange:user5/brightness");
}

void ManagementInterface::networkLoop(int sd) {
	unsigned int len;
	int num_bytes;
	struct sockaddr_in remote;
	char buffer_in[UDP_MAXBUF];

	len = sizeof(remote);

	while (1)
	{
		num_bytes = recvfrom(sd, buffer_in, UDP_MAXBUF, 0, (struct sockaddr*)&remote, &len);

		if (num_bytes >= 2)
		{
			if (buffer_in[0] == 0xE5) // Valid command
			{
				if (buffer_in[1] == 0x1)
				{
					// Got ping request, respond:
					char responseBuffer[2] = { 0xE6, 0x1 };
					sendto(sd, &responseBuffer, sizeof(responseBuffer), 0, (struct sockaddr*)&remote, len);
				}
				else if (buffer_in[1] == 0x2)
				{
					// Got software version query, respond:
					char responseBuffer[20] = { 0 };
					responseBuffer[0] = 0xE6;
					responseBuffer[1] = 0x2;
					strcpy(responseBuffer + 2, softwareVersion);
					sendto(sd, &responseBuffer, sizeof(responseBuffer), 0, (struct sockaddr*)&remote, len);
				}
				else if (buffer_in[1] == 0x3)
				{
					// Got set name command, respond:
					char responseBuffer[2] = { 0xE6, 0x3 };
					sendto(sd, &responseBuffer, sizeof(responseBuffer), 0, (struct sockaddr*)&remote, len);

					// Set name
					if (num_bytes > 3) // Name must not be empty
					{
						if (num_bytes > 22)
							num_bytes = 22; // Can't have longer name than 20 chars
						buffer_in[num_bytes] = '\0'; // Make sure we don't fuck up
						settingIdnHostname = std::string((char*)&buffer_in[2]);
						idnServer->setHostName((uint8_t*)settingIdnHostname.c_str(), settingIdnHostname.length() + 1);

						try
						{
							mINI::INIFile file(settingsPath);
							mINI::INIStructure ini;
							if (!file.read(ini))
							{
								printf("WARNING: Could not find/open main settings file when setting new name.\n");
								continue;
							}
							ini["idn_server"]["name"] = settingIdnHostname;
							file.write(ini);
						}
						catch (std::exception ex)
						{
							printf("WARNING: Failed to save settings file with new name.\n");
							continue;
						}
					}
				}
				else if (buffer_in[1] == 0x4)
				{
					// Got get settings file command, respond
					char responseBuffer[UDP_MAXBUF] = { 0xE6, 0x4, 0, 1 };
					size_t msgSize = 4;

					try
					{
						std::ifstream file(settingsPath);
						if (!file.is_open())
						{
							printf("WARNING: Could not find/open main settings file during get settings file command.\n");
							responseBuffer[2] = 0;
							responseBuffer[3] = 2;
							msgSize = 4;
						}
						else
						{
							std::stringstream buffer;
							buffer << file.rdbuf();
							std::string settingsString = buffer.str();
							size_t settingsStringLength = settingsString.length() + 1;
							if (settingsStringLength > UDP_MAXBUF - 2)
							{
								settingsStringLength = UDP_MAXBUF - 3;
								responseBuffer[UDP_MAXBUF - 1] = '\0';
							}
							strncpy(responseBuffer + 2, settingsString.c_str(), settingsStringLength);
							msgSize = 2 + settingsStringLength;
						}
					}
					catch (std::exception ex)
					{
						printf("WARNING: Other error during get settings file command: %s.\n", ex.what());
						responseBuffer[2] = 0;
						responseBuffer[3] = 3;
						msgSize = 4;
					}

					sendto(sd, &responseBuffer, msgSize, 0, (struct sockaddr*)&remote, len);
					continue;
				}
			}
		}

		struct timespec delay, dummy; // Prevents hogging 100% CPU use
		delay.tv_sec = 0;
		delay.tv_nsec = 500000;
		nanosleep(&delay, &dummy);
	}
}

void ManagementInterface::mountUsbDrive()
{
	// The reason we don't let linux automatically mount drives at boot is that if the drive does not exist, checking for files takes way too long and
	// there isn't a way to reduce the timeout as far as I see. It's better to copy all files locally anyway to prevent issues with plugging out the drive
	// when files are being played etc.

	char command[256];
	const char* rootPassword = "pen_pineapple"; // Todo support custom password for user, for systems that need to be secure.
	sprintf(command, "echo \"%s\" | sudo -S mount /dev/sda1 %s -o uid=1000,gid=1000", rootPassword, usbDrivePath.c_str());
	system(command);
	usleep(500000);

	try
	{
		if (!std::filesystem::is_empty(usbDrivePath))
			usbDriveMounted = true;
		else
			unmountUsbDrive();
	}
	catch (std::filesystem::filesystem_error)
	{
		unmountUsbDrive();
		return;
	}
}

void ManagementInterface::emitEnterButtonPressed()
{
	printf("Enter button\n");
	filePlayer.playButtonPress();
}

void ManagementInterface::emitEscButtonPressed()
{
	printf("Esc button\n");
	filePlayer.stopButtonPress();
}

void ManagementInterface::emitUpButtonPressed()
{
	printf("Up button\n");
	filePlayer.upButtonPress();
}

void ManagementInterface::emitDownButtonPressed()
{
	printf("Down button\n");
	filePlayer.downButtonPress();
}

void ManagementInterface::unmountUsbDrive()
{
	usbDriveMounted = false;
	char command[256];
	const char* rootPassword = "pen_pineapple"; // Todo support custom password for user, for systems that need to be secure.
	sprintf(command, "echo \"%s\" | sudo -S umount %s", rootPassword, usbDrivePath.c_str());
	system(command);
}

int ManagementInterface::writeTo(char* file, char* data, size_t numBytes)
{
	int fd = open(file, O_WRONLY);
	if (fd < 0)
		return -1;

	ssize_t written = write(fd, data, numBytes);
	if (written == numBytes)
	{
		close(fd);
		return 0;
	}
	else
	{
		close(fd);
		return -1;
	}
}

/// <summary>
/// Starts a thread which listens to commands over UDP, such as ping requests.
/// </summary>
/// <returns></returns>
void* ManagementInterface::networkThreadEntry() {
	printf("Starting network thread in management class\n");

	usleep(500000);

	// Setup socket
	int ld;
	struct sockaddr_in sockaddr;
	unsigned int length;

	if ((ld = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("Problem creating socket\n");
		exit(1);
	}

	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(MANAGEMENT_PORT);

	if (bind(ld, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) 
	{
		printf("Problem binding\n");
		exit(0);
	}

	// Set Socket Timeout
	// This allows to react on cancel requests by control thread every 1ms
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000;

	if (setsockopt(ld, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
		printf("setsockopt failed\n");

	if (setsockopt(ld, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
		printf("setsockopt failed\n");

	networkLoop(ld);

	close(ld);


	return NULL;
}

void* ManagementInterface::keyboardThreadEntry() {
	printf("Starting keyboard/button handling thread in management class\n");

	const char* keyboardDev = "/dev/input/event0";
	int keyboardFd = open(keyboardDev, O_RDONLY);
	if (keyboardFd < 0) {
		printf("WARNING: failed to open keyboard, buttons will not work");
		return NULL; // Todo retry?
	}

	auto lastPlayButtonPressedTime = std::chrono::steady_clock::now();
	struct input_event event;
	while (true) 
	{
		ssize_t bytesRead = read(keyboardFd, &event, sizeof(event));
		if (bytesRead == (ssize_t)sizeof(event) && event.type == EV_KEY)
		{
			if (event.code == KEY_ENTER) 
			{
				if (event.value == 1)
				{
					// play key pressed, check how long it is held to determine button function
					lastPlayButtonPressedTime = std::chrono::steady_clock::now();
				}
				else if (event.value == 0)
				{
					if (std::chrono::steady_clock::now() - lastPlayButtonPressedTime > std::chrono::milliseconds(500))
						emitEscButtonPressed();
					else
						emitEnterButtonPressed();
				}
				continue;
			}
			else if (event.code == KEY_UP && event.value == 1)
			{
				emitUpButtonPressed();
				continue;
			}
			else if (event.code == KEY_DOWN && event.value == 1)
			{
				emitDownButtonPressed();
				continue;
			}
		}
	}
	if (keyboardFd >= 0)
		close(keyboardFd);
	return NULL;
}



/*void ManagementInterface::setMode(unsigned int _mode)
{
	if (mode == _mode || _mode > OUTPUT_MODE_MAX)
		return;

	mode = _mode;
	
	// todo turn on and off modules
}

int ManagementInterface::getMode()
{
	return mode;
}*/

int ManagementInterface::hardwareType = -1;

int ManagementInterface::getHardwareType()
{
	if (hardwareType < 0)
	{
		struct utsname unameData;
		if (uname(&unameData) == 0)
		{
			if (strstr(unameData.nodename, "heliospro") != NULL || strstr(unameData.nodename, "rock-s0") != NULL)
				hardwareType = HARDWARE_ROCKS0;
			else if (strstr(unameData.nodename, "rockpi-s") != NULL)
				hardwareType = HARDWARE_ROCKPIS;
			else
				hardwareType = HARDWARE_UNKNOWN;
		}
		else
			hardwareType = -1;
	}

	return hardwareType;
}

bool ManagementInterface::requestOutput(int outputMode)
{
	if (outputMode < 0 || outputMode > OUTPUT_MODE_MAX)
		return false;

	if (currentMode != outputMode)
	{
		int currentPriority = (currentMode >= 0) ? modePriority[currentMode] : -1;
		int newPriority = modePriority[outputMode];
		if (currentPriority >= newPriority)
			return false;
	}
	else
		return true;

	printf("Switching output to mode %d\n", outputMode);

	if (outputMode == OUTPUT_MODE_IDN)
	{
		system("echo 1 > /sys/class/leds/rock-s0:green:user3/brightness");
		system("echo 0 > /sys/class/leds/rock-s0:red:user4/brightness");
		currentMode = outputMode;
		return true;
	}
	else if (outputMode == OUTPUT_MODE_USB)
	{
		system("echo 0 > /sys/class/leds/rock-s0:green:user3/brightness");
		system("echo 1 > /sys/class/leds/rock-s0:red:user4/brightness");
		currentMode = outputMode;
		return true;
	}
	else if (outputMode == OUTPUT_MODE_FILE)
	{
		system("echo 1 > /sys/class/leds/rock-s0:green:user3/brightness");
		system("echo 1 > /sys/class/leds/rock-s0:red:user4/brightness");
		currentMode = outputMode;
		return true;
	}
	else if (outputMode == OUTPUT_MODE_DMX)
	{
		system("echo 1 > /sys/class/leds/rock-s0:green:user3/brightness");
		system("echo 1 > /sys/class/leds/rock-s0:red:user4/brightness");
		currentMode = outputMode;
		return true;
	}
	else return false;
}

void ManagementInterface::stopOutput(int outputMode)
{
	if (currentMode != outputMode)
		return;

	printf("Stopping output in mode %d\n", currentMode);
	system("echo 0 > /sys/class/leds/rock-s0:green:user3/brightness");
	system("echo 0 > /sys/class/leds/rock-s0:red:user4/brightness");
	currentMode = -1;
	return;
}

void* keyboardThreadFunction(void* args) 
{
	ManagementInterface* management = (ManagementInterface*)args;
	management->keyboardThreadEntry();
	return nullptr;
}