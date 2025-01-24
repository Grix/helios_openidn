#include "HeliosProAdapter.hpp"

// Interface to internal HeliosPro output buffer microcontroller via SPIdev

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
	if (durationUs <= 0)
		return 0;

	//get time
	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	size_t dataSizeBytes = data.size();

	unsigned int numPoints = dataSizeBytes / bytesPerPoint();
	uint32_t pps = numPoints * 1000000l / durationUs;
	//printf("%d - %d - %f\n", pps, numPoints, durationUs);

	writeBuffer[0] = 'H';
	writeBuffer[1] = 'P';
	writeBuffer[2] = 'D'; // data
	writeBuffer[3] = 0; // reserved
	writeBuffer[4] = ((dataSizeBytes + 16) >> 0) & 0xFF;
	writeBuffer[5] = ((dataSizeBytes + 16) >> 8) & 0xFF;
	writeBuffer[6] = 0; // reserved
	writeBuffer[7] = 0; // reserved
	memcpy(&writeBuffer[8], &pps, 4);
	writeBuffer[12] = 0; // reserved
	writeBuffer[13] = 0; // reserved
	writeBuffer[14] = 0; // reserved
	writeBuffer[15] = 0; // reserved

	memcpy(&writeBuffer[16], &data.front(), dataSizeBytes);

	writeBuffer[16 + dataSizeBytes + 0] = 'G';
	writeBuffer[16 + dataSizeBytes + 1] = 'R';
	writeBuffer[16 + dataSizeBytes + 2] = 'I';
	writeBuffer[16 + dataSizeBytes + 3] = 'X';

	// TESTING
/*	static int frame = 0;
	for (int i = 0; i < numPoints; i++)
	{
		if ((frame & 3) == 1)
			*(uint16_t*)(&writeBuffer[16 + i * 10]) = i * 0xffff / numPoints;
		else if ((frame & 3) == 2)
			*(uint16_t*)(&writeBuffer[16 + i * 10]) = (numPoints-i) * 0xffff / numPoints;
		else if ((frame & 3) == 3)
			*(uint16_t*)(&writeBuffer[16 + i * 10]) = i * 0x7fff / numPoints;
		else
			*(uint16_t*)(&writeBuffer[16 + i * 10]) = (numPoints - i) * 0x7fff / numPoints;
	}
	frame++;
	*/

	//printf("busy status: %d\n", GPIO_LEV(GPIOPIN_STATUS));

	while (!GPIO_LEV(GPIOPIN_STATUS)) // Wait for free space in buffer chip
	{
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;
		if (tdif > ((unsigned long)durationUs * 1000 * 3))
		{
			printf("WARNING: Timeout waiting for Helios buffer chip\n");
			return 0;
		}
	};

	//printf("got ready\n");

	//write the whole block all at once
	int writeErr = write(this->spidevFd, writeBuffer, dataSizeBytes + 16 + 4);

	/*static uint16_t prevX = 0x8000;
	static uint16_t prevY = 0x8000;
	for (int i = 0; i < numPoints; i++)
	{
		uint16_t x = *(uint16_t*)(&writeBuffer[16 + i * 10]);
		uint16_t y = *(uint16_t*)(&writeBuffer[16 + i * 10 + 2]);
		if (x != 0x8000 && (abs(x - prevX) > 10000 || abs(y - prevY) > 10000))
		{
			printf("ERROR: Noncontinous data\n");
		}
		prevX = x;
		prevY = y;
	}*/

	//uint32_t test[128];
	//for (int i = 0; i < 128; i++)
	//{
	//	test[i] = 0xE5000000 + i;
	//}
	//int writeErr = write(this->spidevFd, test, 128 * 4);
	if (writeErr == -1) 
	{
		perror("spi write error");
		printf("msg size = %u\n", dataSizeBytes + 16 + 4);
	}

	//busy waiting to adjust for timing inaccuracy
	do
	{
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;

		//struct timespec delay, dummy; // Yield timeslice
		//delay.tv_sec = 0;
		//delay.tv_nsec = 0;
		//nanosleep(&delay, &dummy);
	}
	while (tdif < ((unsigned long)durationUs * 1000 * 0.5));


	return 0;
}

SliceType HeliosProAdapter::convertPoints(const std::vector<ISPDB25Point>& points) 
{

	SliceType result;

	for (const auto& point : points) 
	{
		result.push_back(point.y & 0xFF);
		result.push_back((point.y >> 8) & 0xFF);
		result.push_back((point.u3 >> 4) & 0xFF);
		result.push_back((point.u3 >> 12) & 0xFF);
		result.push_back((point.u2 >> 4) & 0xFF);
		result.push_back((point.u2 >> 12) & 0xFF);
		result.push_back((point.u1 >> 4) & 0xFF);
		result.push_back((point.u1 >> 12) & 0xFF);
		result.push_back((point.intensity >> 4) & 0xFF);
		result.push_back((point.intensity >> 12) & 0xFF);
		result.push_back((point.b >> 4) & 0xFF);
		result.push_back((point.b >> 12) & 0xFF);
		result.push_back((point.g >> 4) & 0xFF);
		result.push_back((point.g >> 12) & 0xFF);
		result.push_back(point.x & 0xFF);
		result.push_back((point.x >> 8) & 0xFF);
		result.push_back((point.r >> 4) & 0xFF);
		result.push_back((point.r >> 12) & 0xFF);
	}

	return result;
}

unsigned int HeliosProAdapter::bytesPerPoint() 
{
	return 18;
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
