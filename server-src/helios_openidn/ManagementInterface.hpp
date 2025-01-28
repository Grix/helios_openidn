#pragma once

#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include "ini.hpp"
#include "IDNServer.hpp"
#include "FilePlayer.hpp"

#define MANAGEMENT_PORT 7355

#define OUTPUT_MODE_IDN 0
#define OUTPUT_MODE_USB 1
#define OUTPUT_MODE_FILE 2
#define OUTPUT_MODE_DMX 3
#define OUTPUT_MODE_MAX OUTPUT_MODE_DMX

/// <summary>
/// Class that exposes network and file system interfaces for managing the OpenIDN system, such as pinging, reading config files from USB drive, etc.
/// </summary>


class ManagementInterface
{
public:
	void readAndStoreNewSettingsFile();
	void readSettingsFile();
	void* networkThreadEntry();
	void setMode(unsigned int mode);
	int getMode();

	std::string settingIdnHostname = "OpenIDN";
	const char softwareVersion[10] = "0.9.7";
	std::shared_ptr<IDNServer> idnServer;
	int modePriority[OUTPUT_MODE_MAX + 1] = { 3, 4, 1, 2 }; // If <=0, disable entirely
	FilePlayer filePlayer;


private:
	void networkLoop(int socketFd);
	void mountUsbDrive();
	void unmountUsbDrive();

	const std::string newSettingsPath = "/media/usbdrive/settings.ini";
	const std::string settingsPath = "/home/laser/openidn/settings.ini";

	int mode = OUTPUT_MODE_IDN;
};
