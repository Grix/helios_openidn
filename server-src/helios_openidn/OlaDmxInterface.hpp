#pragma once

#include "ManagementInterface.hpp"
#include <ola/DmxBuffer.h>
#include <ola/Logging.h>
#include <ola/client/ClientWrapper.h>
#include <atomic>
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
	void setDmxAddress(unsigned int universe, unsigned int channelOffset);
	void setName(std::string deviceName);
	unsigned int getUniverse();
	unsigned int getChannelOffset();
	void readSettings(mINI::INIStructure ini);
	void runThreaded();
	void close();
	uint8_t rawDmxData[DMX_NUM_CHANNELS];
	std::atomic<bool> isOk;
	std::atomic<bool> shutdown;

private:
	
	bool forceAddressUpdate = true;
	std::atomic<unsigned int> dmxUniverse;
	std::atomic<unsigned int> dmxChannelOffset;
	pthread_t olaThread = 0; 
	ola::client::OlaClientWrapper wrapper;
	std::mutex threadLock;
	std::vector<OlaInputPort> inputPorts;
	int deviceFetchRetries = 5;
	std::atomic<unsigned int> requestedDmxUniverse;
	std::atomic<unsigned int> requestedDmxChannelOffset;
	unsigned int artnetDeviceId = 2;
	std::string deviceName = "HeliosPRO";

	void DoInitSetDmxAddress(unsigned int universe, unsigned int channelOffset);
	void DoSetDmxAddress(unsigned int universe, unsigned int channelOffset);
	void UpdateInputPorts();
	void UpdateInputPortsCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices);
	void UpdateInputPortsBeforeSetDmxAddressCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices);
	void StepComplete(const ola::client::Result& result);
	void RegisterCompleteCallback(const ola::client::Result& result);
	void NewDmxCallback(const ola::client::DMXMetadata& metadata, const ola::DmxBuffer& data);
	void ProcessData();
};

void* runThread(void* args);
