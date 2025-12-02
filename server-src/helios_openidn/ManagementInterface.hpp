#pragma once

#include "ini.hpp"
#include "server/IDNServer.hpp"
#include "output/V1LaproGraphOut.hpp"
#include "shared/DACHWInterface.hpp"
#include "shared/HWBridge.hpp"
#include "Display.hpp"
#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include <sys/utsname.h>
#include <linux/input.h>

#define MANAGEMENT_PORT 7355

#define OUTPUT_MODE_IDN 0
#define OUTPUT_MODE_USB 1
#define OUTPUT_MODE_FILE 2
#define OUTPUT_MODE_DMX 3
#define OUTPUT_MODE_FORCESTOP 4
#define OUTPUT_MODE_MAX OUTPUT_MODE_FORCESTOP

#define HARDWARE_UNKNOWN 0
#define HARDWARE_ROCKPIS 1
#define HARDWARE_ROCKS0 2

void* keyboardThreadFunction(void* args);

/// <summary>
/// Class that exposes network and file system interfaces for managing the OpenIDN system, such as pinging, reading config files from USB drive, etc.
/// </summary>

class ManagementInterface
{
public:
	ManagementInterface();
	void readAndStoreUsbFiles();
	void readSettingsFile();
	void* networkThreadEntry();
	void* keyboardThreadEntry();
	//void setMode(unsigned int mode);
	//int getMode();
	static int getHardwareType();
	bool requestOutput(int outputMode);
	void relinquishOutput(int outputMode);
	void unmountUsbDrive();
	void runStartup();

	std::string settingIdnHostname = "HeliosPRO";
	const char softwareVersion[10] = "0.9.9";
	const unsigned char softwareVersionUsb = 99;
	std::shared_ptr<IDNServer> idnServer;
	int modePriority[OUTPUT_MODE_MAX + 1] = { 4, 3, 1, 2, 100 }; // If <=0, disable entirely
	std::vector<std::shared_ptr<DACHWInterface>> devices;
	std::vector<V1LaproGraphicOutput*> outputs; // not used right now
	std::vector<std::shared_ptr<HWBridge>> driverBridges; // only used for buffer duration setting, look into refactoring
	bool usbDriveMounted = false;


private:
	void networkLoop(int socketFd);
	void mountUsbDrive();
	void emitEnterButtonPressed();
	void emitEscButtonPressed();
	void emitUpButtonPressed();
	void emitDownButtonPressed();

	int writeTo(char* file, char* data, size_t numBytes);

	const std::string newSettingsPath = "/media/usbdrive/settings.ini";
	const std::string settingsPath = "/home/laser/openidn/settings.ini";
	const std::string usbDrivePath = "/media/usbdrive";

	static int hardwareType;
	int currentMode = -1;

	int keyboardFd;
	pthread_t keyboardThread = 0;
	//std::chrono::steady_clock::time_point lastPlayButtonPressedTime = std::chrono::steady_clock::now();

	Display* display = NULL;
};
