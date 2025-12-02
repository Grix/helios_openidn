#include "HeliosAdapter.hpp"

bool HeliosAdapter::isInitialized = false;
int HeliosAdapter::numHeliosDevices = 0;
int HeliosAdapter::indexFirstDevice = -1;
int HeliosAdapter::indexSecondDevice = -1;
IDNLaproService* HeliosAdapter::serviceFirstDevice = NULL;
IDNLaproService* HeliosAdapter::serviceSecondDevice = NULL;
bool HeliosAdapter::firstServiceIsAlwaysVisible = false;
HeliosDac HeliosAdapter::helios;


void HeliosAdapter::initialize() {
    if (isInitialized) {
        printf("HeliosAdapter is already initialized.\n");
        return;
    }

    numHeliosDevices = helios.OpenDevices();

	refreshDacReferences(numHeliosDevices);

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
	this->maximumPointRate = 0xFFFF*0.8 - 1; // Actual max is 65535, but subtract to account for max time factor strecthing (0.8)
}

HeliosAdapter::~HeliosAdapter() {
}

int HeliosAdapter::writeFrame(const TimeSlice& slice, double duration) {

	isBusy = true;

	// This will be left in for now, unique hotplugging 1:1 scenario, technically outdated
	//if (!getHeliosConnected())
	//{
	//	numHeliosDevices = helios.OpenDevices(); // For hot-plugging
	//}

	int heliosId;
	if (this->id == 0)
		heliosId = indexFirstDevice;
	else if (this->id == 1)
		heliosId = indexSecondDevice;
	else
		heliosId = this->id;

	if (heliosId < 0)
		return -1;

	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	unsigned numPoints = data.size() / bytesPerPoint();
	unsigned framePointRate = 65500;

	if(duration > 0) {
		framePointRate = (unsigned)((1000000.0*(double)data.size()) / (duration*(double)sizeof(HeliosPoint)));
	}
	
	if (framePointRate <= HELIOS_MAX_PPS)
	{
		int status = 0;

		while (true)
		{
			status = helios.GetStatus(heliosId);
			if (status == 1)
				break;

			if (status < 0)
			{
				printf("Error checking Helios status: %d\n", status);
				if (status == HELIOS_ERROR_DEVICE_CLOSED)
				{
					if (this->id == 0)
						indexFirstDevice = -1;
					else if (this->id == 1)
						indexSecondDevice = -1;
				}
				break;
			}
		}

		//printf("Sent hel frame: samples %d, pps %d\n", numPoints, framePointRate);

		status = helios.WriteFrame(heliosId
			, framePointRate
			, this->heliosFlags, (HeliosPoint*)&data.front()
			, numPoints);

		if (status < 0)
		{
			printf("Error writing Helios frame: %d\n", status);
			if (status == HELIOS_ERROR_DEVICE_CLOSED)
			{
				if (this->id == 0)
					indexFirstDevice = -1;
				else if (this->id == 1)
					indexSecondDevice = -1;
			}
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
	} while (tdif < ((unsigned long)duration * 1000 * 0.1) );


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
	int heliosId;
	if (this->id == 0)
		heliosId = indexFirstDevice;
	else if (this->id == 1)
		heliosId = indexSecondDevice;
	else
		heliosId = this->id;

	char heliosName[32];
	if (heliosId >= 0)
	{
		if (helios.GetName(this->id, heliosName) != HELIOS_SUCCESS)
			strcpy(heliosName, "[Unknown Helios]");
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

void HeliosAdapter::updateDeviceList()
{
	int listSize = helios.ReScanDevices();

	refreshDacReferences(listSize);
}

/*bool HeliosAdapter::getIsBusy()
{
	return false;
}*/

void HeliosAdapter::refreshDacReferences(unsigned int numDevices)
{
#ifndef NDEBUG
	printf("Refreshing Helios references. %d, %d\n", indexFirstDevice, indexSecondDevice);
#endif

	if (indexFirstDevice >= numDevices)
		indexFirstDevice = -1;
	if (indexFirstDevice >= 0 && helios.GetIsClosed(indexFirstDevice))
		indexFirstDevice = -1;
	if (indexSecondDevice >= numDevices)
		indexSecondDevice = -1;
	if (indexSecondDevice >= 0 && helios.GetIsClosed(indexSecondDevice))
		indexSecondDevice = -1;

#ifndef NDEBUG
	printf("After clean. %d, %d\n", indexFirstDevice, indexSecondDevice);
#endif

	for (int i = 0; i < numDevices; i++)
	{
		if (indexFirstDevice == -1 && indexSecondDevice != i)
		{
			if (!helios.GetIsClosed(i))
			{
				indexFirstDevice = i;
				char name[32];
				helios.GetName(indexFirstDevice, name);
				printf("Adding Helios 1 reference. %d\n", indexFirstDevice);
				if (serviceFirstDevice != NULL)
					serviceFirstDevice->setServiceName(name, strlen(name));
			}
		}
		else if (indexSecondDevice == -1 && indexFirstDevice != i && !helios.GetIsClosed(i))
		{
			indexSecondDevice = i;
			char name[32];
			helios.GetName(indexFirstDevice, name);
			printf("Adding Helios 2 reference. %d\n", indexSecondDevice);
			if (serviceSecondDevice != NULL)
				serviceSecondDevice->setServiceName(name, strlen(name));
		}
	}

	if (serviceFirstDevice != NULL)
		serviceFirstDevice->isActive = firstServiceIsAlwaysVisible || indexFirstDevice >= 0;
	if (serviceSecondDevice != NULL)
		serviceSecondDevice->isActive = indexSecondDevice >= 0;
}

void HeliosAdapter::checkConnection()
{
	this->connectionRetries--;
	if (this->connectionRetries <= 0)
	{
		//TODO HANDLING
	}
}
