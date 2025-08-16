#include "Display.hpp"

const char* menuItems[] =
{
	"menu item 1",
	"menu item 2",
	"menu item 3",
	"menu item 4",
	"menu item N 5",
	"menu item N 6",
	"menu item N 7",
	"menu item last",
};

Display::Display()
{
	// todo check if screen is available

	display = new GraphicsDisplay(-1, { 1, 0x3C, -1, -1, 0 });
	display->begin();
	display->clear();
	display->getInterface().flipVertical(1);
	display->getInterface().flipHorizontal(1);
	display->setFixedFont(ssd1306xled_font6x8);
	display->printFixed(10, 10, "IDN");
	display->drawLine(10, 25, 110, 30);
	display->printFixed(10, 30, "alpha v0.9.8");
	display->drawRect(1, 1, 2, 2);
	display->drawRect(126, 62, 127, 63);

	//display->createMenu(mainMenu, menuItems, sizeof(menuItems) / sizeof(char*), (NanoRect) { { 8, 24 }, { 0,0 } });//new LcdGfxMenu(menuItems, sizeof(menuItems) / sizeof(char*), (NanoRect) { { 8, 24 }, {0,0} });
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
