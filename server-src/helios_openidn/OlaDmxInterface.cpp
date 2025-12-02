#include "OlaDmxInterface.hpp"

OlaDmxInterface::OlaDmxInterface()
{
	if (pthread_create(&olaThread, NULL, &loopThread, this) != 0) {
		printf("ERROR CREATING OLA DMX THREAD\n");
	}

	if (!wrapper.Setup())
		return;
}

void OlaDmxInterface::loop()
{
	ola::client::OlaClient* client = wrapper.GetClient();
	// Set the callback and register our interest in this universe
	client->SetDMXCallback(ola::NewCallback(&NewDmx));
	client->RegisterUniverse(DMX_UNIVERSE, ola::client::REGISTER, ola::NewSingleCallback(&RegisterComplete));
	wrapper.GetSelectServer()->Run();
}

// Called when universe registration completes.
void RegisterComplete(const ola::client::Result& result) {
	if (!result.Success()) {
		OLA_WARN << "Failed to register universe: " << result.Error();
	}
}
// Called when new DMX data arrives.
void NewDmx(const ola::client::DMXMetadata& metadata,
	const ola::DmxBuffer& data) {
	std::cout << "Received " << data.Size()
		<< " channels for universe " << metadata.universe
		<< ", priority " << static_cast<int>(metadata.priority)
		<< std::endl;
}

void* loopThread(void* args)
{
	OlaDmxInterface* interface = (OlaDmxInterface*)args;

	interface->loop();
}

