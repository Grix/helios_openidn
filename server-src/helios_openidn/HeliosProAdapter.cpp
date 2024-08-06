#include "HeliosProAdapter.h"

// Interface to internal HeliosPro output microcontroller via SPIdev

HeliosProAdapter::HeliosProAdapter() 
{
	this->spidevFd = open("/dev/spidev2.0", O_WRONLY | O_NONBLOCK);
	if (!this->spidevFd) 
		printf("Couldn't open spidev2.0: %s", strerror(errno));

	this->maximumPointrate = 100000;

	int ret = ioctl(this->spidevFd, SPI_IOC_WR_MAX_SPEED_HZ, 50000000); // Max speed, in practice around 27 MHz
	if (ret == -1)
		perror("SPIdev: Can't set speed hz");
}

HeliosProAdapter::~HeliosProAdapter() 
{
}

int HeliosProAdapter::writeFrame(const TimeSlice& slice, double durationUs) 
{
	//get time
	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;

	unsigned int numPoints = data.size() / bytesPerPoint();
	uint32_t pps = (uint32_t)(numPoints / (durationUs / 1000000));

	writeBuffer[0] = 'H';
	writeBuffer[1] = 'P';
	writeBuffer[2] = 'D'; // data
	writeBuffer[3] = 0; // reserved
	writeBuffer[4] = numPoints & 0xFF;
	writeBuffer[5] = (numPoints >> 8) & 0xFF;
	writeBuffer[6] = 0; // reserved
	writeBuffer[7] = 0; // reserved
	writeBuffer[8] = 0; // pps goes here later
	writeBuffer[9] = 0; // pps goes here later
	writeBuffer[10] = 0; // pps goes here later
	writeBuffer[11] = 0; // pps goes here later
	writeBuffer[12] = 0; // reserved
	writeBuffer[13] = 0; // reserved
	writeBuffer[14] = 0; // reserved
	writeBuffer[15] = 0; // reserved

	memcpy(&writeBuffer[16], &data.front(), data.size());

	//write the whole block all at once
	int writeErr = write(this->spidevFd, writeBuffer, data.size() + 16);
	if (writeErr == -1) 
	{
		perror("spi write error");
		printf("msg size = %u\n", data.size());
	}

	//busy waiting to adjust for timing inaccuracy
	do 
	{
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;
	}
	while (tdif < ((unsigned long)duration * 1000));


	return 0;
}

SliceType HeliosProAdapter::convertPoints(const std::vector<ISPDB25Point>& points) 
{

	SliceType result;

	size_t numPoints = points.size();
	result.push_back('H');
	result.push_back('P');
	result.push_back('D'); // data
	result.push_back(0); // reserved
	result.push_back(numPoints & 0xFF);
	result.push_back((numPoints >> 8) & 0xFF);
	result.push_back(0); // reserved
	result.push_back(0); // reserved
	result.push_back(0); // pps goes here later
	result.push_back(0); // pps goes here later
	result.push_back(0); // pps goes here later
	result.push_back(0); // pps goes here later
	result.push_back(0); // reserved
	result.push_back(0); // reserved
	result.push_back(0); // reserved
	result.push_back(0); // reserved

	for (const auto& point : points) 
	{

		result.push_back(point.x & 0xFF);
		result.push_back((point.x >> 8) & 0xFF);
		result.push_back(point.y & 0xFF);
		result.push_back((point.y >> 8) & 0xFF);
		result.push_back((point.r >> 8) & 0xFF);
		result.push_back((point.g >> 8) & 0xFF);
		result.push_back((point.b >> 8) & 0xFF);
		result.push_back((point.intensity >> 8) & 0xFF);
		result.push_back((point.u1 >> 8) & 0xFF);
		result.push_back((point.u2 >> 8) & 0xFF);
	}

	return result;
}

unsigned int HeliosProAdapter::bytesPerPoint() 
{
	return 10;
}


unsigned int HeliosProAdapter::maxBytesPerTransmission() 
{
	return HELIOSPRO_CHUNKSIZE;
}

unsigned int HeliosProAdapter::maxPointrate() 
{
	return maximumPointrate;
}

void HeliosProAdapter::setMaxPointrate(unsigned newRate) 
{
	this->maximumPointrate = newRate;
}
