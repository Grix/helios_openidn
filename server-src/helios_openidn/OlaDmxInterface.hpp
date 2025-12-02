#pragma once

#include "ManagementInterface.hpp"
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/ClientWrapper.h>

static const unsigned int DMX_UNIVERSE = 1;

class OlaDmxInterface
{
public:
	OlaDmxInterface();

	void loop();

private:


	pthread_t olaThread = 0; 
	ola::client::OlaClientWrapper wrapper;
};

void* loopThread(void* args);

// Called when universe registration completes.
void RegisterComplete(const ola::client::Result& result);

// Called when new DMX data arrives.
void NewDmx(const ola::client::DMXMetadata& metadata, const ola::DmxBuffer& data);
