#pragma once

#include "idtf.hpp"
#include <string>
#include <map>

#define FILEPLAYER_MODE_REPEAT 0
#define FILEPLAYER_MODE_ONCE 1
#define FILEPLAYER_MODE_NEXT 2
#define FILEPLAYER_MODE_SHUFFLE 3

#define FILEPLAYER_STATE_STOP 0
#define FILEPLAYER_STATE_PAUSE 1
#define FILEPLAYER_STATE_PLAY 2

#define FILEPLAYER_PARAM_SPEEDTYPE_PPS 0
#define FILEPLAYER_PARAM_SPEEDTYPE_FPS 1

class FilePlayer
{
public:

	typedef struct FilePlayerFileParameters
	{
		int speedType = FILEPLAYER_PARAM_SPEEDTYPE_PPS;
		unsigned int speed = 30000;
		bool ignore = false;
		int palette = 0;
	} FilePlayerFileParameters;

	bool autoplay = false;
	std::string currentFile = "";
	int mode = FILEPLAYER_MODE_REPEAT;
	int state = FILEPLAYER_STATE_STOP;
	unsigned int frame = 0;
	std::map<std::string, FilePlayerFileParameters> fileParameters;
	FilePlayerFileParameters defaultParameters;

	void start();
	void stop();
	void pause();
	void playFile(std::string filename);
};

