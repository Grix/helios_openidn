#include "Display.hpp"

//const char* menuItems[1000];

const char* menuItemsMain[2] =
{
	"File Player",
	"Information",
};


Display::Display()
{
	// todo check if screen is available

	display = new GraphicsDisplay(-1, { 1, 0x3C, -1, -1, 0 });
	display->begin();

	display->getInterface().flipVertical(1);
	display->getInterface().flipHorizontal(1);

	canvas.begin(128, 64, canvasData);
	canvas.setFixedFont(ssd1306xled_font6x8);
	//canvas.setFreeFont(free_calibri11x12, free_calibri11x12);

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

void Display::MenuUpdateHeader()
{
	canvas.setBackground(0);
	canvas.setColor(0);
	canvas.fillRect(0, 0, 127, 16);
	canvas.setColor(1);
	canvas.printFixed(10, 4, "HeliosPRO");
	canvas.drawLine(0, 14, 127, 16);
	//display->printFixed(10, 30, "software v0.9.9");

}

void Display::MenuButtonUp()
{
	menu->up();
	menu->show(canvas);
	display->drawCanvas(0, 0, canvas);
	//display->menuUp(&menu);
	//display->updateMenu(&menu);
}

void Display::MenuButtonDown()
{
	menu->down();
	menu->show(canvas);
	display->drawCanvas(0, 0, canvas);
	//display->menuDown(&menu);
	//display->updateMenu(&menu);
}

void Display::MenuButtonEnter()
{

}

void Display::MenuGotoMain()
{
	const int numMenuItems = 2;
	//display->createMenu(&menu, menuItemsMain, numMenuItems, (NanoRect) { { 0, 19 }, { 0,0 } });  //new LcdGfxMenu(menuItems, sizeof(menuItems) / sizeof(char*), (NanoRect) { { 8, 24 }, {0,0} });
	menu = std::make_unique<LcdGfxMenu>(menuItemsMain, 2, (NanoRect) { { 0, 8 }, { 0,0 } });

	canvas.clear();
	MenuUpdateHeader();
	menu->show(canvas);
	//display->showMenu(&menu);
	display->drawCanvas(0, 0, canvas);
}

void Display::MenuGotoFilePlayer(std::map<std::string, FilePlayer::Program> programs)
{
	canvas.clear();
	MenuUpdateHeader();
	display->drawCanvas(0, 0, canvas);
}

void Display::MenuGotoInformation()
{
	canvas.clear();
	MenuUpdateHeader();
	display->drawCanvas(0, 0, canvas);
}

int Display::MenuGetSelection()
{
	return menu->selection();
}

