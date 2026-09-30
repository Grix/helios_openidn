#pragma once

#include <map>
#include "thirdparty/lcdgfx/src/lcdgfx.h"
#include "lcdgfx_gui.h"
#include "FilePlayer.hpp"

// Todo move these into a common header, duplicated from ManagementInterface.hpp:
#define OUTPUT_MODE_IDN 0
#define OUTPUT_MODE_USB 1
#define OUTPUT_MODE_FILE 2
#define OUTPUT_MODE_DMX 3

#define NUM_MENU_ITEMS_INFORMATION 7

enum Menus 
{
	MainMenu,
	FilePlayerMenu,
	InformationMenu
};

class Display
{
public:

	Display();

	void FinishInitialization();
	void MenuUpdateHeader(bool update);
	void MenuButtonUp();
	void MenuButtonDown();
	void MenuButtonEnter();
	void MenuGotoMain();
	void MenuUpdateMainFooter(bool update);
	void MenuGotoFilePlayer(std::vector<std::string> programs);
	void MenuGotoInformation();
	int MenuGetSelection();
	std::string MenuGetSelectedFile();

	void SetMode(int mode);
	void SetIpAddrEthernet(std::string ipAddrEthernet, int _subnetCidr);
	void SetIpAddrWiFi(std::string ipAddrWifi, int _subnetCidr);
	void SetDeviceName(std::string deviceName);
	void SetFirmwareVersion(std::string _version);
	void SetDmxAddress(int _dmxChannel, int _dmxUniverse);
	void SetCurrentPlayingProgram(std::string currentPlayingProgram);



private:

	typedef DisplaySSD1306_128x64_I2C GraphicsDisplay;
	//typedef NanoEngine<NanoCanvas<32, 32, 1U>, GraphicsDisplay> GraphicsEngine;

	GraphicsDisplay* display = nullptr;
	//GraphicsEngine* graphicsEngine = nullptr;

	std::unique_ptr<LcdGfxMenu> menu;
	Menus currentMenu = Menus::MainMenu;
	std::vector<const char*> menuItemsFilePlayerVector;

	uint8_t canvasData[128 * (64 / 8)];
	NanoCanvas1 canvas;

	int mode = -1;
	std::string ipAddrEthernet = "";
	int ipAddrEthernetSubnetCidr = 0;
	std::string ipAddrWifi = "";
	int ipAddrWifiSubnetCidr = 0;
	int dmxUniverse = 1;
	int dmxChannel = 0;
	char ipAddrEthernetString[32] = "";
	char ipAddrWifiString[32] = "";
	char firmwareVersionString[24] = "";
	char dmxUniverseString[24] = "";
	char dmxChannelString[24] = "";
	std::string deviceName = "";
	std::string firmwareVersion = "";
	std::string currentPlayingProgram = "";
	std::mutex threadLock;
	int staticMenuPosition = 0;

	const char* menuItemsMain[2] =
	{
		"File Player",
		"Information",
	};

	const char* menuItemsInformation[NUM_MENU_ITEMS_INFORMATION]
	{
		"Ethernet:",
		ipAddrEthernetString,
		"Wi-Fi:",
		ipAddrWifiString,
		dmxChannelString,
		dmxUniverseString,
		firmwareVersionString
	};

	const char* noFilesFoundText = "No files found";

	void DrawScrollIndicator();

};

