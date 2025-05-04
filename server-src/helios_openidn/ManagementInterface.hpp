#pragma once

#include "ini.hpp"
#include "server/IDNServer.hpp"
#include "FilePlayer.hpp"
#include "output/V1LaproGraphOut.hpp"
#include "thirdparty/lcdgfx/src/lcdgfx.h"
#include <string>
#include <cstdint>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include <sys/utsname.h>

#define MANAGEMENT_PORT 7355

#define OUTPUT_MODE_IDN 0
#define OUTPUT_MODE_USB 1
#define OUTPUT_MODE_FILE 2
#define OUTPUT_MODE_DMX 3
#define OUTPUT_MODE_MAX OUTPUT_MODE_DMX

#define HARDWARE_UNKNOWN 0
#define HARDWARE_ROCKPIS 1
#define HARDWARE_ROCKS0 2


// Uncomment the line you need for your display
//typedef DisplaySSD1331_96x64x8_SPI GraphicsDisplay;
typedef DisplaySSD1306_128x64_I2C GraphicsDisplay;
//typedef DisplaySSD1306_128x64_SPI GraphicsDisplay;
//typedef DisplayPCD8544_84x48_SPI GraphicsDisplay;
//typedef DisplayST7735_128x160_SPI GraphicsDisplay;
//typedef DisplayIL9163_128x160_SPI GraphicsDisplay;

typedef NanoEngine<NanoCanvas<32, 32, 1U>, GraphicsDisplay> GraphicsEngine;

/// <summary>
/// Class that exposes network and file system interfaces for managing the OpenIDN system, such as pinging, reading config files from USB drive, etc.
/// </summary>

class ManagementInterface
{
public:
	ManagementInterface();
	void readAndStoreNewSettingsFile();
	void readSettingsFile();
	void* networkThreadEntry();
	void setMode(unsigned int mode);
	int getMode();
	static int getHardwareType();

	std::string settingIdnHostname = "OpenIDN";
	const char softwareVersion[10] = "0.9.8";
	const unsigned char softwareVersionUsb = 98;
	std::shared_ptr<IDNServer> idnServer;
	FilePlayer filePlayer;
	int modePriority[OUTPUT_MODE_MAX + 1] = { 3, 4, 1, 2 }; // If <=0, disable entirely
	std::vector<std::shared_ptr<DACHWInterface>> devices;
	std::vector<std::shared_ptr<V1LaproGraphicOutput>> outputs;


private:
	void networkLoop(int socketFd);
	void mountUsbDrive();
	void unmountUsbDrive();

	const std::string newSettingsPath = "/media/usbdrive/settings.ini";
	const std::string settingsPath = "/home/laser/openidn/settings.ini";

	int mode = OUTPUT_MODE_IDN;

	static int hardwareType;

	GraphicsDisplay* display = nullptr;
	GraphicsEngine* graphicsEngine = nullptr;
};
