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

/*
 DMX channels:

 0: Master brightness / enable: 0-31: Blackout , 255: Full brightness
 1: File ID MSB 0-127. 128-223 reserved - 224+: blackout
 2: File ID LSB: 0: None
 3: Speed: 0-7: 100%, 8-31: 0%, 128: 50%, 255: 300%
 4: Red brightness. 255: full brightness
 5: Green brightness. 255: full brightness
 6: Blue brightness. 255: full brightness
 7: Hue shift. 128: no shift, 0: no shift, 255: no shift
 8: X position offset. 128: no shift
 9: X position offset LSB
10: Y position offset. 128: no shift
11: Y position offset LSB
12: X size. 128: normal size
13: Y size. 128: normal size
14: Rotation: 128: normal angle
15: Reserved. Keep at 128 for forward compatibility

*/

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
	unsigned short previousFileId = 0;

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
