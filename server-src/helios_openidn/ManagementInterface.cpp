#include "ManagementInterface.hpp"

#define MAXBUF 128
#define MANAGEMENT_PORT 7355

/// <summary>
/// Looks for a file "settings.ini" on a USB drive connected to the computer, and if it exists, applies the settings there. 
/// See example_settings.ini in the source files for how the file could look. 
/// For this function to work, a folder must have been created at /media/usbdrive, and the USB drive must always be assigned by the system as /dev/sda1
/// </summary>
void ManagementInterface::readAndStoreNewSettingsFile()
{
	printf("Checking for new settings file.\n");

	mountUsbDrive();

	char command[256];

	try
	{
		if (!std::filesystem::exists(newSettingsPath))
		{
			unmountUsbDrive();
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

	unmountUsbDrive();

	printf("Finished checking new settings.\n");
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
		std::string& wlan0_ssid = ini["network"]["wifi_ssid"];
		std::string& wlan0_ip_addresses = ini["network"]["wifi_ip_addresses"];
		if (!wlan0_ssid.empty() && !wlan0_ip_addresses.empty())
		{
			sleep(20); // To make sure nmcli has started before we try to use it. Extra long delay needed for wifi.

			std::string& wlan0_password = ini["network"]["wifi_password"];

			wlan0_ip_addresses.erase(std::remove(wlan0_ip_addresses.begin(), wlan0_ip_addresses.end(), '\"'), wlan0_ip_addresses.end()); // Remove quote marks

			if (wlan0_password.empty())
				sprintf(command, "nmcli device wifi connect \"%s\" name \"%s\"", wlan0_ssid.c_str(), wlan0_ssid.c_str());
			else
				sprintf(command, "nmcli device wifi connect \"%s\" password \"%s\" name \"%s\"", wlan0_ssid.c_str(), wlan0_password.c_str(), wlan0_ssid.c_str());
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

		ini["network"]["already_applied"] = std::string("true");
		shouldRewrite = true;
	}

	std::string& idn_hostname = ini["idn_server"]["name"];
	if (!idn_hostname.empty())
		settingIdnHostname = idn_hostname;

	std::string& idn_enable = ini["idn_server"]["enable"];
	if (!idn_enable.empty())
		settingEnableIdnServer = !(idn_enable == "false" || idn_enable == "False" || idn_enable == "\"false\"" || idn_enable == "\"False\"");

	if (shouldRewrite)
	{
		file.write(ini);
	}

	printf("Finished reading main settings.\n");
}

void ManagementInterface::networkLoop(int sd) {
	unsigned int len;
	int num_bytes;
	char buffer_in[MAXBUF];
	struct sockaddr_in remote;

	len = sizeof(remote);

	while (1)
	{
		num_bytes = recvfrom(sd, buffer_in, MAXBUF, 0, (struct sockaddr*)&remote, &len);

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
						for (int i = 0; i < 20; i++)
							idnServer->hostName[i] = 0;
						memcpy(idnServer->hostName, settingIdnHostname.c_str(), settingIdnHostname.size() < HOST_NAME_SIZE ? settingIdnHostname.size() : HOST_NAME_SIZE);

						try
						{
							mINI::INIFile file(settingsPath);
							mINI::INIStructure ini;
							if (!file.read(ini))
							{
								printf("WARNING: Could not find/open main settings file when setting new name.\n");
								return;
							}
							ini["idn_server"]["name"] = settingIdnHostname;
							file.write(ini);
						}
						catch (std::exception ex)
						{
							printf("WARNING: Failed to save settings file with new name.\n");
							return;
						}
					}
				}
			}
		}

		struct timespec delay, dummy; // Prevents hogging 100% CPU use
		delay.tv_sec = 0;
		delay.tv_nsec = 500;
		nanosleep(&delay, &dummy);
	}
}

void ManagementInterface::mountUsbDrive()
{
	char command[256];
	const char* rootPassword = "pen_pineapple"; // Todo support custom password for user, for systems that need to be secure.
	sprintf(command, "echo \"%s\" | sudo -S mount /dev/sda1 /media/usbdrive -o uid=1000,gid=1000", rootPassword);
	system(command);
	usleep(500000);
}

void ManagementInterface::unmountUsbDrive()
{
	char command[256];
	const char* rootPassword = "pen_pineapple"; // Todo support custom password for user, for systems that need to be secure.
	sprintf(command, "echo \"%s\" | sudo -S umount /media/usbdrive", rootPassword);
	system(command);
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