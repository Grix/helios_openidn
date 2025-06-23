#include "HeliosAdapter.hpp"

bool HeliosAdapter::isInitialized = false;
HeliosDac HeliosAdapter::helios; 
int HeliosAdapter::numHeliosDevices = 0;

void HeliosAdapter::initialize() {
    if (isInitialized) {
        printf("HeliosAdapter is already initialized.\n");
        return;
    }

    numHeliosDevices = helios.OpenDevices();

	if(numHeliosDevices == 0) {
		printf("No Helios device found!\n");
	}
    printf("HeliosAdapter initialized.\n");
    isInitialized = true;
}

void HeliosAdapter::shutdown() {
    if (!isInitialized) {
        printf("HeliosAdapter is not initialized.\n");
        return;
    }

    helios.CloseDevices();

    printf("HeliosAdapter shutdown.\n");
    isInitialized = false;
}

std::map<std::string, int> HeliosAdapter::getAvailableDevices() {
    if (!isInitialized) {
        throw std::runtime_error("HeliosAdapter::Initialize must be called before using this method.");
    }

    std::map<std::string, int> deviceMap;

    for (int i = 0; i < numHeliosDevices; ++i) {
        char deviceName[32];
        if (helios.GetName(i, deviceName)) {
            deviceMap[std::string(deviceName)] = i;
        }
    }

    return deviceMap;
}


HeliosAdapter::HeliosAdapter(int id) {
	if (!isInitialized) {
        throw std::runtime_error("HeliosAdapter::Initialize must be called before creating an instance.");
    }
	this->id = id;

	this->heliosFlags = HELIOS_FLAGS_SINGLE_MODE;
	this->maximumPointRate = 54000; // Actual max is 65535, but subtract around 1/1.2 to account for max time factor strecthing
}

HeliosAdapter::~HeliosAdapter() {
}

int HeliosAdapter::writeFrame(const TimeSlice& slice, double duration) {

	isBusy = true;

	// This will be left in for now, unique hotplugging 1:1 scenario, technically outdated
	if (!getHeliosConnected())
	{
		numHeliosDevices = helios.OpenDevices(); // For hot-plugging
	}

	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	unsigned numPoints = data.size() / bytesPerPoint();
	unsigned framePointRate = 65500;

	if(duration > 0) {
		framePointRate = (unsigned)((1000000.0*(double)data.size()) / (duration*(double)sizeof(HeliosPoint)));
	}
	
	if (framePointRate <= HELIOS_MAX_RATE)
	{
		int status = 0;

		if (getHeliosConnected())
		{
			while (true)
			{
				status = helios.GetStatus(this->id);
				if (status == 1)
					break;

				if (status < 0)
				{
					printf("Error checking Helios status: %d\n", status);
					checkConnection();
					break;
				}
				else
					this->connectionRetries = 50;
			}

			status = helios.WriteFrame(this->id
				, framePointRate
				, this->heliosFlags, (HeliosPoint*)&data.front()
				, numPoints);

			if (status < 0)
			{
				printf("Error writing Helios frame: %d\n", status);
				checkConnection();

			}
			else
				this->connectionRetries = 50;
		
		}
	}
	else
		printf("Too high helios rate, bypassing write: %d\n", framePointRate);

	isBusy = false;

	//waiting, not really required normally
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif*1000000000 + nsdif ;
	} while (tdif < ((unsigned long)duration * 1000 * 0.2) );


	return 0;
}

SliceType HeliosAdapter::convertPoints(const std::vector<ISPDB25Point>& points) {

	SliceType result(points.size()* bytesPerPoint());

	int index = 0;

	for(const auto& point : points) {

		HeliosPoint currentPoint;

		currentPoint.x = (std::uint16_t)(point.x >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.y = (std::uint16_t)(point.y >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.r = (std::uint8_t) (point.r >> 8);	//8 bit	(from 0 to 0xFF)
		currentPoint.g = (std::uint8_t) (point.g >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.b = (std::uint8_t) (point.b >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.i = (std::uint8_t) (point.intensity >> 8);	//8 bit (from 0 to 0xFF)

		for(int i = 0; i < bytesPerPoint(); i++) {
			result[index++] = (*((uint8_t*)&currentPoint + i));
		}
	}

	return result;
}

unsigned HeliosAdapter::bytesPerPoint() {
	return (unsigned)sizeof(HeliosPoint);
}

unsigned HeliosAdapter::maxBytesPerTransmission() {
	return 4096*bytesPerPoint();
}

unsigned HeliosAdapter::maxPointrate() {
	return this->maximumPointRate;
}

void HeliosAdapter::setMaxPointrate(unsigned newRate) {
	this->maximumPointRate = newRate;
}

void HeliosAdapter::getName(char *nameBufferPtr, unsigned nameBufferSize)
{
	char heliosName[32];
	if (getHeliosConnected())
	{
		helios.GetName(this->id, heliosName);
	}
	else
		strcpy(heliosName, "[Missing Helios]");

	size_t nameLength = strlen(heliosName);
	memcpy(nameBufferPtr, heliosName, nameLength > nameBufferSize ? nameBufferSize : nameLength);
}

bool HeliosAdapter::getHeliosConnected()
{
	return true;
	//return helios.GetStatus(this->id);
}

bool HeliosAdapter::getIsBusy()
{
	return false;
}

void HeliosAdapter::checkConnection()
{
	this->connectionRetries--;
	if (this->connectionRetries <= 0)
	{
		//TODO HANDLING
	}
}
