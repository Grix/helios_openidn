#include "OlaDmxInterface.hpp"

extern EffectFilter effectFilter;

OlaDmxInterface::OlaDmxInterface()
	: wrapper(false)
{
	isOk.store(false);
	shutdown.store(false);
	requestedDmxUniverse.store(1);
	requestedDmxChannelOffset.store(0);
	dmxUniverse.store(1);
	dmxChannelOffset.store(0);
}

void OlaDmxInterface::run()
{
	if (pthread_create(&olaThread, NULL, &runThread, this) != 0)
	{
		fprintf(stderr, "ERROR creating OLA DMX thread\n");
	}
}

void OlaDmxInterface::setDmxAddress(unsigned int universe, unsigned int channelOffset)
{
	if (!isOk.load())
		return;

	wrapper.GetSelectServer()->Execute(ola::NewSingleCallback(this, &OlaDmxInterface::DoInitSetDmxAddress, universe, channelOffset));
}

void OlaDmxInterface::setName(std::string _deviceName)
{
	deviceName = _deviceName;
}

unsigned int OlaDmxInterface::getUniverse()
{
	return dmxUniverse.load();
}

unsigned int OlaDmxInterface::getChannelOffset()
{
	return dmxChannelOffset.load();
}

void OlaDmxInterface::NewDmxCallback(const ola::client::DMXMetadata& metadata, const ola::DmxBuffer& data)
{
	if (!isOk.load())
		return;

#ifndef NDEBUG
	std::cout << "Received " << data.Size() << " channels for universe " << metadata.universe << ", priority " << static_cast<int>(metadata.priority) << std::endl;
#endif

	//std::lock_guard<std::mutex> lockGuard(threadLock);

	if (metadata.universe != dmxUniverse.load() || data.Size() <= dmxChannelOffset.load() + DMX_NUM_CHANNELS)
		return;

	unsigned int length = DMX_NUM_CHANNELS;
	data.GetRange(dmxChannelOffset.load(), rawDmxData, &length);
	if (length == DMX_NUM_CHANNELS)
		ProcessData();
}

void OlaDmxInterface::ProcessData()
{

}

void OlaDmxInterface::runThreaded()
{
#ifndef NDEBUG
	ola::InitLogging(ola::OLA_LOG_INFO, ola::OLA_LOG_STDERR);
#endif

	int numErrors = 0;
	while (!shutdown.load())
	{
		std::this_thread::sleep_for(std::chrono::seconds(3));
		if (!wrapper.Setup())
		{
			fprintf(stderr, "Error creating OLA DMX client wrapper, retrying..\n");
			if (numErrors++ > 20)
				return;
		}
		else
			break;
	}

	ola::client::OlaClient* client = wrapper.GetClient();
	client->SetSourceUID(ola::rdm::UID(RDM_ESTA_ID, RDM_DEVICE_ID), NULL);
	client->SetDMXCallback(ola::NewCallback(this, &OlaDmxInterface::NewDmxCallback));
	printf("Successfully started OLA DMX client\n");
	isOk.store(true);
	setDmxAddress(requestedDmxUniverse.load(), requestedDmxChannelOffset.load());
	wrapper.GetSelectServer()->Run();
}

void OlaDmxInterface::DoInitSetDmxAddress(unsigned int universe, unsigned int channelOffset)
{
	requestedDmxUniverse.store(universe);
	requestedDmxChannelOffset.store(channelOffset);
	ola::client::OlaClient* client = wrapper.GetClient();
	client->FetchDeviceInfo(ola::OLA_PLUGIN_ALL, ola::NewSingleCallback(this, &OlaDmxInterface::UpdateInputPortsBeforeSetDmxAddressCallback));
}

// Internal threaded function for changing dmx address. Run SetDmxAddress() instead 
void OlaDmxInterface::DoSetDmxAddress(unsigned int universe, unsigned int channelOffset)
{
    if (channelOffset > 512u - DMX_NUM_CHANNELS) 
    {
        std::fprintf(stderr, "Invalid channel offset: %u\n", channelOffset);
        return;
    }

    if (inputPorts.empty()) 
    {
        std::fprintf(stderr, "DMX: No OLA input ports found\n");
        return;
    }

	printf("Setting DMX address: %d %d\n", universe, channelOffset);

    dmxChannelOffset.store(channelOffset);
    if (dmxUniverse.load() == universe && !forceAddressUpdate)
	{
        return;
    }

	ola::client::OlaClient* client = wrapper.GetClient();

	// Unregister and unpatch from old universe
	client->RegisterUniverse(dmxUniverse.load(), ola::client::UNREGISTER, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	for (OlaInputPort inputPort : inputPorts)
	{
		client->Patch(inputPort.device, inputPort.port, ola::client::INPUT_PORT, ola::client::UNPATCH, dmxUniverse.load(), ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	}

	dmxUniverse.store(universe);

	// Register and patch to new universe
	for (OlaInputPort inputPort : inputPorts)
	{
		client->Patch(inputPort.device, inputPort.port, ola::client::INPUT_PORT, ola::client::PATCH, dmxUniverse.load(), ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	}
	client->RegisterUniverse(dmxUniverse.load(), ola::client::REGISTER, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	
	if (artnetDeviceId >= 0)
	{
		// TODO replace system() call with protobuf plugin communication? See https://github.com/OpenLightingProject/ola/blob/master/examples/ola-artnet.cpp
		unsigned int net = dmxUniverse.load() / (256);
		unsigned int subnet = dmxUniverse.load() / (16);
		char command[128];
		sprintf(command, "ola_artnet -d %d --subnet %d --net %d --name \"%s\"", artnetDeviceId, subnet, net, deviceName.substr(0, 15).c_str());
		system(command);
	}

	forceAddressUpdate = false;
}

void OlaDmxInterface::UpdateInputPorts()
{
	if (!isOk.load())
		return;

	ola::client::OlaClient* client = wrapper.GetClient();
	client->FetchDeviceInfo(ola::OLA_PLUGIN_ALL, ola::NewSingleCallback(this, &OlaDmxInterface::UpdateInputPortsCallback));
}

void OlaDmxInterface::UpdateInputPortsCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices)
{
	if (!result.Success()) 
	{
		std::fprintf(stderr, "OLA FetchDeviceInfo failed: %s\n", result.Error().c_str());
	}

	inputPorts.clear();
	artnetDeviceId = -1;

	for (ola::client::OlaDevice device : devices)
	{
		// TODO check if this device should be enabled, in settings file
		if (device.InputPorts().size() > 0)
		{
			bool alreadyAdded = false;
			for (OlaInputPort inputPort : inputPorts)
			{
				if (inputPort.device == device.Alias() && inputPort.port == device.InputPorts()[0].Id())
				{
					alreadyAdded = true;
					break;
				}
			}
			if (!alreadyAdded)
			{
#ifndef NDEBUG
				printf("Found OLA input port: %d %s %d\n", device.Alias(), device.Name().c_str(), device.InputPorts()[0].Id());
#endif
				inputPorts.push_back(OlaInputPort{ device.Alias(), device.InputPorts()[0].Id() });
			}

			if (device.Name().find("ArtNet") != std::string::npos)
				artnetDeviceId = device.Alias();
		}
	}
}

void OlaDmxInterface::UpdateInputPortsBeforeSetDmxAddressCallback(const ola::client::Result& result, const std::vector<ola::client::OlaDevice>& devices)
{
	UpdateInputPortsCallback(result, devices);

	if (inputPorts.size() <= 0)
	{
		deviceFetchRetries--;
		if (deviceFetchRetries <= 0)
		{
			std::fprintf(stderr, "Warning: could not find any OLA DMX devices after max retries\n");
		}
		wrapper.GetSelectServer()->RegisterSingleTimeout(3000, ola::NewSingleCallback(this, &OlaDmxInterface::DoInitSetDmxAddress, requestedDmxUniverse.load(), requestedDmxUniverse.load()));
		return;
	}

	wrapper.GetSelectServer()->Execute(ola::NewSingleCallback(this, &OlaDmxInterface::DoSetDmxAddress, requestedDmxUniverse.load(), requestedDmxChannelOffset.load()));
}

void OlaDmxInterface::StepComplete(const ola::client::Result& result)
{
	if (!result.Success()) 
	{
		std::fprintf(stderr, "OLA address change failed: %s\n", result.Error().c_str());
		// TODO try again?
	}
}

// Called when universe registration completes.
void OlaDmxInterface::RegisterCompleteCallback(const ola::client::Result& result)
{
	if (!result.Success()) 
	{
		std::fprintf(stderr, "Failed to register OLA universe: %s\n", result.Error().c_str());
	}
}

void OlaDmxInterface::readSettings(mINI::INIStructure ini)
{
	// Parse general settings from main ini file
	try
	{
		bool newAddress = false;
		int universeParsed = 1;
		std::string& universe = ini["dmx"]["universe"];
		if (!universe.empty())
			universeParsed = std::stoi(universe);

		int channelOffsetParsed = 0;
		std::string& channelOffset = ini["dmx"]["channel_offset"];
		if (!channelOffset.empty())
			channelOffsetParsed = std::stoi(channelOffset);

		if (isOk.load())
		{
			setDmxAddress(universeParsed, channelOffsetParsed);
		}
		else
		{
			requestedDmxUniverse.store(universeParsed);
			requestedDmxChannelOffset.store(channelOffsetParsed);
		}
	}
	catch (std::exception& ex)
	{
		fprintf(stderr, "WARNING: Error during parsing of DMX interface ini file settings: %s.\n", ex.what());
	}
}

void OlaDmxInterface::close()
{
	shutdown.store(true);
	isOk.store(false);
	wrapper.GetSelectServer()->Terminate();
}

void* runThread(void* args)
{
	OlaDmxInterface* olaInterface = (OlaDmxInterface*)args;

	olaInterface->runThreaded();

	return NULL;
}

