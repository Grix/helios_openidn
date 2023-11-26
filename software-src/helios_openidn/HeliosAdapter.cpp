#include "HeliosAdapter.h"

HeliosAdapter::HeliosAdapter() {
	this->numHeliosDevices = this->helios.OpenDevices();
	if(this->numHeliosDevices == 0) {
		printf("No Helios device found!\n");
	}

	this->heliosFlags = HELIOS_FLAGS_SINGLE_MODE;
	this->maximumPointRate = 60000;
}

HeliosAdapter::~HeliosAdapter() {
	this->helios.CloseDevices();
}

int HeliosAdapter::writeFrame(const TimeSlice& slice, double duration) {
	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	unsigned numPoints = data.size() / bytesPerPoint();
	unsigned framePointRate = 65500;

	if(duration > 0) {
		framePointRate = (unsigned)((1000000.0*(double)data.size()) / (duration*(double)sizeof(HeliosPoint)));
	}
	
	int status;

	while (true)
	{
		status = helios.GetStatus(numHeliosDevices-1);
		if (status == 1)
			break;
		
		if (status < 0)
			printf("Error checking Helios status: %d\n", status);
	}
	status = this->helios.WriteFrame(this->numHeliosDevices-1
		, framePointRate
		, this->heliosFlags, (HeliosPoint*)&data.front()
		, numPoints);
		
	if (status < 0)
		printf("Error writing Helios frame: %d\n", status);

	//busy waiting
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif*1000000000 + nsdif ;
	} while (tdif < ((unsigned long)duration * 1000) );


	return 0;
}

SliceType HeliosAdapter::convertPoints(const std::vector<ISPDB25Point>& points) {
	SliceType result;
	HeliosPoint currentPoint;

	for(const auto& point : points) {
		currentPoint.x = (std::uint16_t)(point.x >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.y = (std::uint16_t)(point.y >> 4); //12 bit (from 0 to 0xFFF)
		currentPoint.r = (std::uint8_t) (point.r >> 8);	//8 bit	(from 0 to 0xFF)
		currentPoint.g = (std::uint8_t) (point.g >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.b = (std::uint8_t) (point.b >> 8);	//8 bit (from 0 to 0xFF)
		currentPoint.i = (std::uint8_t) (point.intensity >> 8);	//8 bit (from 0 to 0xFF)

		for(int i = 0; i < bytesPerPoint(); i++) {
			result.push_back(*((uint8_t*)&currentPoint + i));
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
	return maximumPointRate;
}

void HeliosAdapter::setMaxPointrate(unsigned newRate) {
	this->maximumPointRate = newRate;
}
