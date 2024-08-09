#include "HeliosProAdapter.h"

// Interface to internal HeliosPro output microcontroller via SPIdev

HeliosProAdapter::HeliosProAdapter() 
{
	this->spidevFd = open("/dev/spidev2.0", O_WRONLY | O_NONBLOCK);
	if (!this->spidevFd)
	{
		printf("Couldn't open spidev2.0: %s", strerror(errno));
		exit(1);
	}

	this->maximumPointrate = 100000;

	unsigned int speed = 50000000;
	int ret = ioctl(this->spidevFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed); // Max speed, in practice around 27 MHz
	if (ret == -1)
		perror("SPIdev: Can't set speed hz");

	printf("LOADED HELIOS PRO TEST\n");

	// Map GPIO2 registers for reading status
	mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("Cannot open /dev/mem");
		exit(1);
	}
	// mmap SPI2 control registers
	/*spi2_ctrl_mem_map = mmap(0, 0x500, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0xff040000);
	if (spi2_ctrl_mem_map == MAP_FAILED) {
		perror("mmap() failed");
		exit(1);
	}
	spi2_ctrl = (volatile uint32_t*)spi2_ctrl_mem_map;*/

	gpio_mem_map = mmap(0, 0x80, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0xff240000);
	if (gpio_mem_map == MAP_FAILED) {
		perror("mmap() failed");
		exit(1);
	}
	gpio = (volatile uint32_t*)gpio_mem_map;

	GPIO_DIR_IN(GPIOPIN_STATUS);

	/*#define CTRL_BIT_SET(offset, bit)	*(spi2_ctrl + (offset / 4)) |= (1 << (bit & 0xFF))

	CTRL_BIT_SET(0x00, 18); // Set transmit only
	CTRL_BIT_SET(0x00, 0); // Set 8 bit transfer length
	CTRL_BIT_SET(0x00, 13); // Set 8 bit bus width
	CTRL_BIT_SET(0x10, 1); // Set clock divider = 2 (max hz)
	CTRL_BIT_SET(0x08, 0); // Enable SPI

	spi2_ctrl[0x0 / 4] = (1 << 18) | (1 << 0) | (1 << 13);
	spi2_ctrl[0x10 / 4] = 2;
	spi2_ctrl[0x08 / 4] = 1;*/

}

HeliosProAdapter::~HeliosProAdapter() 
{
	/* munmap GPIO */
	int ret = munmap(gpio_mem_map, 0x80);
	if (ret == -1) {
		perror("munmap() failed");
		exit(1);
	}

	/* close /dev/mem */
	ret = close(mem_fd);
	if (ret == -1) {
		perror("Cannot close /dev/mem");
		exit(1);
	}
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

	//printf("busy status: %d\n", GPIO_LEV(GPIOPIN_STATUS));

	//while (GPIO_LEV(GPIOPIN_STATUS)); // Wait for free space in buffer chip

	//printf("got ready\n");

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
	while (tdif < ((unsigned long)durationUs * 1000));


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
