#include "OlaDmxInterface.hpp"

OlaDmxInterface::OlaDmxInterface()
{
	if (!wrapper.Setup())
	{
		fprintf(stderr, "ERROR creating OLA DMX client wrapper\n");
		return;
	}

	if (pthread_create(&olaThread, NULL, &runThread, this) != 0) 
	{
		fprintf(stderr, "ERROR creating OLA DMX thread\n");
	}
}

void OlaDmxInterface::run()
{
	ola::client::OlaClient* client = wrapper.GetClient();
	client->SetSourceUID(ola::rdm::UID(RDM_ESTA_ID, RDM_DEVICE_ID), NULL);
	client->SetDMXCallback(ola::NewCallback(this, &OlaDmxInterface::NewDmxCallback));
	isOk = true;
	wrapper.GetSelectServer()->Run();
}

void OlaDmxInterface::SetDmxAddress(unsigned int universe, unsigned int channelOffset)
{
	if (!isOk)
		return;

	wrapper.GetSelectServer()->Execute(ola::NewSingleCallback(this, &OlaDmxInterface::DoInitSetDmxAddress, universe, channelOffset));
}

unsigned int OlaDmxInterface::GetUniverse()
{
	return dmxUniverse;
}

unsigned int OlaDmxInterface::GetChannelOffset()
{
	return dmxChannelOffset;
}

void OlaDmxInterface::NewDmxCallback(const ola::client::DMXMetadata& metadata, const ola::DmxBuffer& data)
{
	if (!isOk)
		return;

#ifndef NDEBUG
	std::cout << "Received " << data.Size() << " channels for universe " << metadata.universe << ", priority " << static_cast<int>(metadata.priority) << std::endl;
#endif

	std::lock_guard<std::mutex> lockGuard(threadLock);

	if (metadata.universe != dmxUniverse || data.Size() <= dmxChannelOffset + DMX_NUM_CHANNELS)
		return;

	unsigned int length = DMX_NUM_CHANNELS;
	data.GetRange(dmxChannelOffset, rawDmxData, &length);

	// TODO process data
}

void OlaDmxInterface::DoInitSetDmxAddress(unsigned int universe, unsigned int channelOffset)
{
	requestedDmxUniverse = universe;
	requestedDmxChannelOffset = channelOffset;
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

#ifndef NDEBUG
	printf("Setting DMX address: %d %d\n", universe, channelOffset);
#endif

    dmxChannelOffset = channelOffset;
    if (dmxUniverse == universe) 
	{
        return;
    }

	unsigned int requestedDmxUniverse = universe;

	ola::client::OlaClient* client = wrapper.GetClient();

	// Unregister and unpatch from old universe
	client->RegisterUniverse(dmxUniverse, ola::client::UNREGISTER, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	for (OlaInputPort inputPort : inputPorts)
	{
		client->Patch(inputPort.device, inputPort.port, ola::client::INPUT_PORT, ola::client::UNPATCH, dmxUniverse, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	}

	dmxUniverse = universe;

	// Register and patch to new universe
	for (OlaInputPort inputPort : inputPorts)
	{
		client->Patch(inputPort.device, inputPort.port, ola::client::INPUT_PORT, ola::client::PATCH, dmxUniverse, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
	}
	client->RegisterUniverse(dmxUniverse, ola::client::REGISTER, ola::NewSingleCallback(this, &OlaDmxInterface::StepComplete));
}

void OlaDmxInterface::UpdateInputPorts()
{
	if (!isOk)
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
		wrapper.GetSelectServer()->RegisterSingleTimeout(3000, ola::NewSingleCallback(this, &OlaDmxInterface::DoInitSetDmxAddress, requestedDmxUniverse, requestedDmxUniverse));
		return;
	}

	wrapper.GetSelectServer()->Execute(ola::NewSingleCallback(this, &OlaDmxInterface::DoSetDmxAddress, requestedDmxUniverse, requestedDmxChannelOffset));
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

	}
	catch (std::exception& ex)
	{
		fprintf(stderr, "WARNING: Error during parsing of DMX interface ini file settings: %s.\n", ex.what());
	}
{

void* runThread(void* args)
{
	OlaDmxInterface* olaInterface = (OlaDmxInterface*)args;

	olaInterface->run();

	return NULL;
}

