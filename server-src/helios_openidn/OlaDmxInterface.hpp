#pragma once

#include "ManagementInterface.hpp"
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/ClientWrapper.h>
#include "ini.hpp"

#define RDM_ESTA_ID 0x09B9
#define RDM_DEVICE_ID 0x00000001
#define DMX_NUM_CHANNELS 32u


typedef struct OlaInputPort
{
	unsigned int device;
	unsigned int port;
}OlaInputPort;

class OlaDmxInterface
{
public:
	OlaDmxInterface();

	void run();
	// Change the DMX address of our device
	void SetDmxAddress(unsigned int universe, unsigned int channelOffset);
	unsigned int GetUniverse();
	unsigned int GetChannelOffset();
	void readSettings(mINI::INIStructure ini);
	uint8_t rawDmxData[DMX_NUM_CHANNELS];
	bool isOk = false;

private:
	
	unsigned int dmxUniverse = 1;
	unsigned int dmxChannelOffset = 0;
	pthread_t olaThread = 0; 
	ola::client::OlaClientWrapper wrapper;
	std::mutex threadLock;
	std::vector<OlaInputPort> inputPorts;
	int deviceFetchRetries = 5;
	unsigned int requestedDmxUniverse = 1;
	unsigned int requestedDmxChannelOffset = 0;

	void DoInitSetDmxAddress(unsigned int universe, unsigned int channelOffset);
	void DoSetDmxAddress(unsigned int universe, unsigned int channelOffset);
	void UpdateInputPorts();
	void UpdateInputPortsCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices);
	void UpdateInputPortsBeforeSetDmxAddressCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices);
	void StepComplete(const ola::client::Result& result);
	void RegisterCompleteCallback(const ola::client::Result& result);
	void NewDmxCallback(const ola::client::DMXMetadata& metadata, const ola::DmxBuffer& data);
};

void* runThread(void* args);
