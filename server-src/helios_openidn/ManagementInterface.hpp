#pragma once

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include <string.h>
#include "ini.h"

/// <summary>
/// Class that exposes network and file system interfaces for managing the OpenIDN system, such as pinging, reading config files from USB drive, etc.
/// </summary>


class ManagementInterface
{
public:
	void readAndStoreNewSettingsFile();
	void readSettingsFile();
	void* networkThreadEntry();

	std::string settingIdnHostname = "OpenIDN";
	bool settingEnableIdnServer = true;
	bool settingEnableIdtfPlayer = false;
	const char softwareVersion[10] = "0.9.3";

private:
	void networkLoop(int socketFd);
	void mountUsbDrive();
	void unmountUsbDrive();

	const std::string newSettingsPath = "/media/usbdrive/settings.ini";
	const std::string settingsPath = "/home/laser/openidn/settings.ini";
};

