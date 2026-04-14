#include "Display.hpp"

//const char* menuItems[1000];

const char* menuItemsMain[2] =
{
	"File Player",
	"Information",
};

const char* noFilesFoundText = "No files found";

const PROGMEM uint8_t playImage[8] =
{
	0b01111111,
	0b00111110,
	0b00011100,
	0b00001000,
	0b00000000,
	0b00000000,
	0b00000000,
	0b00000000
};

Display::Display()
{
	printf("Creating display at i2c-1\n");

	display = new GraphicsDisplay(-1, { 1, 0x3C, -1, -1, 0 });
	display->begin();

	display->getInterface().flipVertical(1);
	display->getInterface().flipHorizontal(1);

	canvas.begin(128, 64, canvasData);
	canvas.setFixedFont(ssd1306xled_font6x8);
	canvas.getFont().setSpacing(0);
	//canvas.setFreeFont(free_calibri11x12, free_calibri11x12);

	menuItemsFilePlayerVector.reserve(255);

	MenuGotoMain();
}

void Display::FinishInitialization()
{
	if (!display)
		return;
	
	printf("Display finishing initialization\n");
	//display->printFixed(10, 40, "InitFin");
	//display->showMenu(mainMenu);
	//graphicsEngine->getCanvas().printFixed(10, 40, "InitFin");
	//graphicsEngine->display();

}

void Display::MenuUpdateHeader(bool update)
{
	canvas.setBackground(0);
	canvas.setColor(0);
	canvas.fillRect(0, 0, 127, 11);
	canvas.setColor(1);
	if (mode == OUTPUT_MODE_IDN)
	{
		canvas.drawBitmap1(2, 2, 8, 8, playImage);
		canvas.printFixed(10, 2, "IDN");
	}
	else if (mode == OUTPUT_MODE_USB)
	{
		canvas.drawBitmap1(2, 2, 8, 8, playImage);
		canvas.printFixed(10, 2, "USB");
	}
	else if (mode == OUTPUT_MODE_FILE)
	{
		canvas.drawBitmap1(2, 2, 8, 8, playImage);
		canvas.printFixed(10, 2, "File");
	}
	else if (mode == OUTPUT_MODE_DMX)
	{
		canvas.drawBitmap1(2, 2, 8, 8, playImage);
		canvas.printFixed(10, 2, "DMX");
	}
	else
	{

	}
	canvas.drawLine(0, 12, 127, 12);
	const char* deviceNameChar = deviceName.c_str();
	canvas.printFixed(127 - 2 - canvas.getFont().getTextSize(deviceNameChar), 2, deviceNameChar);

	if (update)
	{
		display->drawCanvas(0, 0, canvas);
	}
}

void Display::MenuButtonUp()
{
	if (menu)
	{
		menu->up();
		menu->show(canvas);
		display->drawCanvas(0, 0, canvas);
	}
	//display->menuUp(&menu);
	//display->updateMenu(&menu);
}

void Display::MenuButtonDown()
{
	if (menu)
	{
		menu->down();
		menu->show(canvas);
		display->drawCanvas(0, 0, canvas);
	}
	//display->menuDown(&menu);
	//display->updateMenu(&menu);
}

void Display::MenuButtonEnter()
{

}

void Display::MenuGotoMain()
{
	std::lock_guard<std::mutex> lock(threadLock);

	const int numMenuItems = 2;
	//display->createMenu(&menu, menuItemsMain, numMenuItems, (NanoRect) { { 0, 19 }, { 0,0 } });  //new LcdGfxMenu(menuItems, sizeof(menuItems) / sizeof(char*), (NanoRect) { { 8, 24 }, {0,0} });
	menu = std::make_unique<LcdGfxMenu>(menuItemsMain, 2, (NanoRect) { { 0, 8 }, { 0,0 } });

	canvas.clear();
	MenuUpdateHeader(false);
	menu->show(canvas);
	//display->showMenu(&menu);
	display->drawCanvas(0, 0, canvas);

	currentMenu = Menus::MainMenu;
}

void Display::MenuGotoFilePlayer(std::vector<std::string> programs)
{
	std::lock_guard<std::mutex> lock(threadLock);

	canvas.clear();
	MenuUpdateHeader(false);
	display->drawCanvas(0, 0, canvas);

	int i = 0;
	int selection = 0;
	size_t numFiles = menuItemsFilePlayerVector.size();
	if (numFiles != programs.size() || numFiles <= 3)
	{
		menuItemsFilePlayerVector.clear();
		for (auto& programEntry : programs)
		{
			if (programEntry == currentPlayingProgram)
				selection = i;
			menuItemsFilePlayerVector.push_back(programEntry.c_str());
			i++;
		}
	}
	if (menuItemsFilePlayerVector.size() == 0)
	{
		menuItemsFilePlayerVector.push_back(noFilesFoundText);
	}

	menu = std::make_unique<LcdGfxMenu>(menuItemsFilePlayerVector.data(), menuItemsFilePlayerVector.size(), (NanoRect) { { 0, 8 }, {0,0} });
	menu->show(canvas);
	//display->showMenu(&menu);
	display->drawCanvas(0, 0, canvas);

	currentMenu = Menus::FilePlayerMenu;
}

void Display::MenuGotoInformation()
{
	std::lock_guard<std::mutex> lock(threadLock);

	canvas.clear();
	MenuUpdateHeader(false);

	char ethInfo[64];
	snprintf(ethInfo, 64, "Eth: %s", ipAddrEthernet.empty() ? "Not connected" : ipAddrEthernet.c_str());
	char wifiInfo[64];
	snprintf(wifiInfo, 64, "WiFi: %s", ipAddrWifi.empty() ? "Not connected" : ipAddrWifi.c_str());
	canvas.printFixed(4, 24, ethInfo);
	canvas.printFixed(4, 32, wifiInfo);

	display->drawCanvas(0, 0, canvas);

	menu = NULL;

	currentMenu = Menus::InformationMenu;
}

int Display::MenuGetSelection()
{
	if (menu)
		return menu->selection();
	else
		return 0;
}

std::string Display::MenuGetSelectedFile()
{
	if (currentMenu == Menus::FilePlayerMenu)
	{
		if (menu->size() == 0)
			return std::string("");

		std::string selectedFile = std::string(menu->items()[menu->selection()]);
		if (selectedFile != std::string(noFilesFoundText))
		{
			return selectedFile;
		}
	}
	return std::string("");
}

void Display::SetMode(int _mode)
{
	if (_mode == mode)
		return;

	std::lock_guard<std::mutex> lock(threadLock);

	mode = _mode;
	MenuUpdateHeader(true);
}

void Display::SetIpAddrEthernet(std::string _ipAddrEthernet)
{
	if (_ipAddrEthernet == ipAddrEthernet)
		return;

	ipAddrEthernet = _ipAddrEthernet;
	if (currentMenu == Menus::InformationMenu)
		MenuGotoInformation();
}

void Display::SetIpAddrWiFi(std::string _ipAddrWifi)
{
	if (_ipAddrWifi == ipAddrWifi)
		return;

	ipAddrWifi = _ipAddrWifi;
	if (currentMenu == Menus::InformationMenu)
		MenuGotoInformation();
}

void Display::SetDeviceName(std::string _deviceName)
{
	if (_deviceName == deviceName)
		return;

	std::lock_guard<std::mutex> lock(threadLock);

	deviceName = _deviceName;
	MenuUpdateHeader(true);
}

void Display::SetCurrentPlayingProgram(std::string _currentPlayingProgram)
{
	if (_currentPlayingProgram == currentPlayingProgram)
		return;

	currentPlayingProgram = _currentPlayingProgram;
}



