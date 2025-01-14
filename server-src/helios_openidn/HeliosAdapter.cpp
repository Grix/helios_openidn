#include "HeliosAdapter.hpp"

HeliosAdapter::HeliosAdapter() {
	this->numHeliosDevices = this->helios.OpenDevices();
	if (this->numHeliosDevices == 0) {
		printf("No Helios device found!\n");
	}

	this->heliosFlags = HELIOS_FLAGS_SINGLE_MODE;
	this->maximumPointRate = 54000; // Actual max is 65535, but subtract around 1/1.2 to account for max time factor strecthing
}

HeliosAdapter::~HeliosAdapter() {
	this->helios.CloseDevices();
}

int HeliosAdapter::writeFrame(const TimeSlice& slice, double duration) {

	if (!getHeliosConnected())
	{
		this->numHeliosDevices = this->helios.OpenDevices(); // For hot-plugging
	}

	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	unsigned numPoints = data.size() / bytesPerPoint();
	unsigned framePointRate = 65500;

	if (duration > 0) {
		framePointRate = (unsigned)((1000000.0 * (double)data.size()) / (duration * (double)sizeof(HeliosPoint)));
	}

	if (framePointRate <= HELIOS_MAX_RATE)
	{
		int status = 0;

		if (getHeliosConnected())
		{
			while (true)
			{
				status = helios.GetStatus(numHeliosDevices - 1);
				if (status == 1)
					break;

				if (status < 0)
				{
					printf("Error checking Helios status: %d\n", status);
					checkConnection();
					break;
				}
				else
					connectionRetries = 50;
			}
			if (status == 1)
			{
				status = this->helios.WriteFrame(this->numHeliosDevices - 1
					, framePointRate
					, this->heliosFlags, (HeliosPoint*)&data.front()
					, numPoints);

				if (status < 0)
				{
					printf("Error writing Helios frame: %d\n", status);
					checkConnection();
				}
				else
					connectionRetries = 50;
			}
		}
	}
	else
	{
		printf("Too high helios rate, bypassing write: %d\n", framePointRate);
		do {
			clock_gettime(CLOCK_MONOTONIC, &now);
			sdif = now.tv_sec - then.tv_sec;
			nsdif = now.tv_nsec - then.tv_nsec;
			tdif = sdif * 1000000000 + nsdif;
		} while (tdif < ((unsigned long)duration * 1000 * 0.98));
	}


	/*struct timespec delay, dummy; // Waits with CPU idle for half the time, to free up cycles
	delay.tv_sec = 0;
	delay.tv_nsec = duration / 4 * 1000;
	nanosleep(&delay, &dummy);*/
	//busy waiting
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;
	} while (tdif < ((unsigned long)duration * 1000 * 0.8));


	return 0;
}

SliceType HeliosAdapter::convertPoints(const std::vector<ISPDB25Point>& points) {
	SliceType result;
	HeliosPoint currentPoint;

	for (const auto& point : points) {
		currentPoint.x = (std::uint16_t)(point.x >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.y = (std::uint16_t)(point.y >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.r = (std::uint8_t)(point.r >> 8);	//8 bit	(from 0 to 0xFF)
		currentPoint.g = (std::uint8_t)(point.g >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.b = (std::uint8_t)(point.b >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.i = (std::uint8_t)(point.intensity >> 8);	//8 bit (from 0 to 0xFF)

		for (int i = 0; i < bytesPerPoint(); i++) {
			result.push_back(*((uint8_t*)&currentPoint + i));
		}
	}

	return result;
}

unsigned HeliosAdapter::bytesPerPoint() {
	return (unsigned)sizeof(HeliosPoint);
}

unsigned HeliosAdapter::maxBytesPerTransmission() {
	return 4096 * bytesPerPoint();
}

unsigned HeliosAdapter::maxPointrate() {
	return maximumPointRate;
}

void HeliosAdapter::setMaxPointrate(unsigned newRate) {
	this->maximumPointRate = newRate;
}

void HeliosAdapter::getName(char* nameBufferPtr, unsigned nameBufferSize)
{
	char heliosName[32];
	if (getHeliosConnected())
	{
		helios.GetName(numHeliosDevices - 1, heliosName);
	}
	else
		strcpy(heliosName, "[Missing Helios]");

	size_t nameLength = strlen(heliosName);
	memcpy(nameBufferPtr, heliosName, nameLength > nameBufferSize ? nameBufferSize : nameLength);
}

bool HeliosAdapter::getHeliosConnected()
{
	return numHeliosDevices >= 1;
}

void HeliosAdapter::checkConnection()
{
	connectionRetries--;
	if (connectionRetries <= 0)
	{
		printf("Too many Helios errors, closing connection.\n");
		helios.CloseDevices();
		numHeliosDevices = 0;
	}
}