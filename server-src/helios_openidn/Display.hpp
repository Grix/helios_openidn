#pragma once

#include "thirdparty/lcdgfx/src/lcdgfx.h"
#include "lcdgfx_gui.h"

class Display
{
public:

	Display();

	void FinishInitialization();

private:

	typedef DisplaySSD1306_128x64_I2C GraphicsDisplay;
	//typedef NanoEngine<NanoCanvas<32, 32, 1U>, GraphicsDisplay> GraphicsEngine;

	GraphicsDisplay* display = nullptr;
	//GraphicsEngine* graphicsEngine = nullptr;

	//LcdGfxMenu* menu;
	SAppMenu* mainMenu;

};

