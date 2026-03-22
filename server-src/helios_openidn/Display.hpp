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
	void MenuGotoFilePlayer(std::vector<std::string> programs);
	void MenuGotoInformation();
	int MenuGetSelection();
	std::string MenuGetSelectedFile();

	void SetMode(int mode);
	void SetIpAddrEthernet(std::string ipAddrEthernet);
	void SetIpAddrWiFi(std::string ipAddrWifi);
	void SetDeviceName(std::string deviceName);
	void SetCurrentPlayingProgram(std::string currentPlayingProgram);

private:

	typedef DisplaySSD1306_128x64_I2C GraphicsDisplay;
	//typedef NanoEngine<NanoCanvas<32, 32, 1U>, GraphicsDisplay> GraphicsEngine;

	GraphicsDisplay* display = nullptr;
	//GraphicsEngine* graphicsEngine = nullptr;

	std::unique_ptr<LcdGfxMenu> menu;
	Menus currentMenu = Menus::MainMenu;
	std::vector<const char*> menuItemsFilePlayerVector;
	//SAppMenu menu;

	uint8_t canvasData[128 * (64 / 8)];
	NanoCanvas1 canvas;

	int mode;
	std::string ipAddrEthernet = "";
	std::string ipAddrWifi = "";
	std::string deviceName = "";
	std::string currentPlayingProgram = "";

};

