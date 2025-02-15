#include "FilePlayer.hpp"

void FilePlayer::start()
{
	if (state == FILEPLAYER_STATE_STOP)
	{
		// Reset
		frame = 0;
	}
	state = FILEPLAYER_STATE_PLAY;
}

void FilePlayer::stop()
{
	state = FILEPLAYER_STATE_STOP;
}

void FilePlayer::pause()
{
	state = FILEPLAYER_STATE_PAUSE;
}

void FilePlayer::playFile(std::string filename)
{
	currentFile = filename;
	//if (idtfRead() < 0)
	{
		// File error

	}
}
