#pragma once

#include "./types.h"

/// <summary>
/// Class that exposes network and file system interfaces for managing the OpenIDN system, such as pinging, reading config files from USB drive, etc.
/// </summary>


class ManagementInterface
{
public:
	void* networkThreadEntry();
private:
	void networkLoop(int socketFd);
};

