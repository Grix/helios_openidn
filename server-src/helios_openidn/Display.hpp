#pragma once

#include <map>
#include "thirdparty/lcdgfx/src/lcdgfx.h"
#include "lcdgfx_gui.h"
#include "FilePlayer.hpp"

class Display
{
public:

	Display();

	void FinishInitialization();
	void MenuUpdateHeader();
	void MenuButtonUp();
	void MenuButtonDown();
	void MenuButtonEnter();
	void MenuGotoMain();
	void MenuGotoFilePlayer(std::map<std::string, FilePlayer::Program> programs);
	void MenuGotoInformation();
	int MenuGetSelection();

private:

	typedef DisplaySSD1306_128x64_I2C GraphicsDisplay;
	//typedef NanoEngine<NanoCanvas<32, 32, 1U>, GraphicsDisplay> GraphicsEngine;

	GraphicsDisplay* display = nullptr;
	//GraphicsEngine* graphicsEngine = nullptr;

	std::unique_ptr<LcdGfxMenu> menu;
	//SAppMenu menu;

	uint8_t canvasData[128 * (64 / 8)];
	NanoCanvas1 canvas;

};

