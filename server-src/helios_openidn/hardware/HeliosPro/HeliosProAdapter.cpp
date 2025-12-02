#include "HeliosProAdapter.hpp"

// Interface to internal HeliosPro output buffer microcontroller via SPIdev

HeliosProAdapter::HeliosProAdapter() 
{
	this->spidevFd = open("/dev/heliospro-spi", O_RDWR);
	if (!this->spidevFd)
	{
		printf("HeliosPRO: Couldn't open /dev/heliospro-spi: %s", strerror(errno));
		exit(1);
	}

	this->maximumPointrate = HELIOSPRO_MCU_MAXSPEED;

	/*unsigned int speed = 50000000;
	int ret = ioctl(this->spidevFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed); // Max speed, in practice around 27 MHz
	if (ret == -1)
		perror("HeliosPRO: SPIdev: Can't set speed hz");*/

	// Map GPIO2 registers for reading status
	/*mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (mem_fd == -1) {
		perror("HeliosPRO: Cannot open /dev/mem");
		exit(1);
	}

	gpio_mem_map = mmap(0, 0x80, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, 0xff240000);
	if (gpio_mem_map == MAP_FAILED) {
		perror("HeliosPRO: mmap() failed");
		exit(1);
	}
	gpio = (volatile uint32_t*)gpio_mem_map;

	GPIO_DIR_IN(GPIOPIN_STATUS);

	// Reset MCU
	GPIO_DIR_OUT(GPIOPIN_MCURESET);
	GPIO_CLR(GPIOPIN_MCURESET);
	struct timespec delay, dummy;
	delay.tv_sec = 0;
	delay.tv_nsec = 1000000;
	nanosleep(&delay, &dummy);
	GPIO_SET(GPIOPIN_MCURESET);
	GPIO_DIR_IN(GPIOPIN_MCURESET);
	//struct timespec delay, dummy;*/

	// Check if MCU is available
	uint8_t status[10];
	int ret = read(this->spidevFd, status, 10);
	if (ret == 10)
	{
		if (status[0] != 'G' && status[1] != 1)
		{
			printf("HeliosPRO: No response from buffer MCU, is it flashed? %d %d", status[0], status[1]);
			exit(1);
		}
	}
	else
	{
		printf("HeliosPRO: No response from buffer MCU, is it flashed? %d", ret);
		exit(1);
	}

	// Wait for ready signal from MCU after reset
	/*struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);
	delay.tv_sec = 0;
	delay.tv_nsec = 5000000;
	nanosleep(&delay, &dummy);
	while (!GPIO_LEV(GPIOPIN_STATUS))
	{

		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;
		if (tdif > 3000000000) // 3 seconds
		{
			perror("HeliosPRO: No response from buffer MCU, is it flashed?");
			exit(1);
		}

		delay.tv_nsec = 500000;
		nanosleep(&delay, &dummy);
	}*/



	//delay.tv_sec = 5;
	//delay.tv_nsec = 0;
	//nanosleep(&delay, &dummy);

	/// DEBUG TEST
	/*TimeSlice** frames = new TimeSlice * [5];
	const int numPointsPerFrame = 208;
	int x = 0;
	int y = 0;
	for (int i = 0; i < 5; i++)
	{
		frames[i] = new TimeSlice();
		frames[i]->durationUs = 1690;

		std::vector<ISPDB25Point> points;

		y = i * 0xFFFF / 5;
		for (int j = 0; j < numPointsPerFrame; j++)
		{
			if (j < (numPointsPerFrame / 2))
				x = j * 0xFFFF / (numPointsPerFrame / 2);
			else
				x = 0xFFFF - ((j - (numPointsPerFrame / 2)) * 0xFFFF / (numPointsPerFrame / 2));

			ISPDB25Point point;

			point.x = x;
			point.y = y;
			point.r = 0xAFFF;
			point.g = 0xFFFF;
			point.b = 0x7FFF;
			point.intensity = 0x2FFF;
			point.u1 = 0x3FFF;
			point.u2 = 0x4FFF;
			point.u3 = 0x5FFF;
			point.u4 = 0x6FFF;

			points.push_back(point);
		}

		frames[i]->dataChunk = convertPoints(points);
	}
	int i = 0;
	while (1)
	{
		i++;
		if (i == 10300)
		{
			//break;
			//sleep(20);
		}

		delay.tv_sec = 0;
		delay.tv_nsec = 100000*(rand() % 12);
		nanosleep(&delay, &dummy);

		writeFrame(*frames[i % 5], frames[i % 5]->durationUs);
	}*/

}

HeliosProAdapter::~HeliosProAdapter() 
{
	// munmap GPIO
	/*int ret = munmap(gpio_mem_map, 0x80);
	if (ret == -1) {
		perror("munmap() failed");
		exit(1);
	}

	// close /dev/mem 
	ret = close(mem_fd);
	if (ret == -1) {
		perror("Cannot close /dev/mem");
		exit(1);
	}*/
}

int HeliosProAdapter::writeFrame(const TimeSlice& slice, double durationUs) 
{
	if (durationUs <= 0 || durationUs > 2000000)
		return 0;

	isBusy = true;
	//get time
	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	size_t dataSizeBytes = data.size();

	unsigned int numPoints = dataSizeBytes / bytesPerPoint();
	uint32_t pps = numPoints * 1001100l / durationUs; // manually adjusted 1000000 to 1001100 to account for observed actual clock speed

	// Calculate raw values for MCU timer. We do this here instead of in the MCU because the MCU doesn't handle 32-bit divisions well
	uint16_t timerRepeats = 0;
	if (pps > HELIOSPRO_MCU_MAXSPEED)
	{
		pps = HELIOSPRO_MCU_MAXSPEED;
	}
	else if (pps < HELIOSPRO_MCU_MINSPEED)
	{
		timerRepeats = HELIOSPRO_MCU_MINSPEED / pps + 1;
		timerRepeats *= 50;
		pps *= timerRepeats;
	}
	double desiredTimer = HELIOSPRO_MCU_TIMERSPEED / (double)pps;
	double roundedDesiredTimer = std::roundl(desiredTimer);
	timerRemainder += roundedDesiredTimer - desiredTimer;
	if (timerRemainder > 1)
	{
		roundedDesiredTimer -= 1;
		timerRemainder -= 1;
	}
	else if (timerRemainder < -1)
	{
		roundedDesiredTimer += 1;
		timerRemainder += 1;
	}
	uint16_t desiredTimerInt = (uint16_t)roundedDesiredTimer;

	unsigned int pointsPerFrame = numPoints;
	unsigned int maxPointsPerFrame = HELIOSPRO_CHUNKSIZE / bytesPerPoint();

	if (pointsPerFrame > maxPointsPerFrame) // This shouldn't happen because of maxBytesPerTransmission() limit, but it actually does, todo investigate
	{
		pointsPerFrame = (int)ceil(numPoints / ceil((double)numPoints / maxPointsPerFrame));
		if (pointsPerFrame < maxPointsPerFrame)
			pointsPerFrame += 1;
	}

	int pointsLeft = numPoints;
	unsigned int offsetPoints = 0;

	while (pointsLeft > 0)
	{
		unsigned int pointsThisFrame = pointsLeft < pointsPerFrame ? pointsLeft : pointsPerFrame;
		dataSizeBytes = pointsThisFrame * bytesPerPoint();


		writeBuffer[0] = 'H';
		writeBuffer[1] = 'P';
		writeBuffer[2] = txId;
		writeBuffer[3] = 0; // reserved
		writeBuffer[4] = ((dataSizeBytes + 16) >> 0) & 0xFF;
		writeBuffer[5] = ((dataSizeBytes + 16) >> 8) & 0xFF;
		writeBuffer[6] = ((0xFFFF) >> 4) & 0xFF; // shutter
		writeBuffer[7] = ((0xFFFF) >> 12) & 0xFF; // shutter
		writeBuffer[8] = ((desiredTimerInt) >> 0) & 0xFF;
		writeBuffer[9] = ((desiredTimerInt) >> 8) & 0xFF;
		writeBuffer[10] = ((timerRepeats) >> 0) & 0xFF;
		writeBuffer[11] = ((timerRepeats) >> 8) & 0xFF;
		writeBuffer[12] = 0; // reserved
		writeBuffer[13] = 0; // reserved
		writeBuffer[14] = 0; // reserved
		writeBuffer[15] = 0; // reserved

		memcpy(&writeBuffer[16], data.data() + offsetPoints * bytesPerPoint(), dataSizeBytes);

		writeBuffer[16 + dataSizeBytes + 0] = txId;
		writeBuffer[16 + dataSizeBytes + 1] = 'G';
		writeBuffer[16 + dataSizeBytes + 2] = 'R'; 
		writeBuffer[16 + dataSizeBytes + 3] = 'X';

		txId++;
		txNum++;

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

		/*while (!GPIO_LEV(GPIOPIN_STATUS)) // Wait for free space in buffer chip
		{
			clock_gettime(CLOCK_MONOTONIC, &now);
			sdif = now.tv_sec - then.tv_sec;
			nsdif = now.tv_nsec - then.tv_nsec;
			tdif = sdif * 1000000000 + nsdif;
			//printf("no: %d\n", tdif / 1000);
			// todo use duration of previous write instead of durationUs
			if (durationUs < 1000000 ? (tdif > 1000000000) : (tdif > (durationUs * 1.5)))//tdif > ((unsigned long)durationUs * 1000 * 3) || (durationUs < 200 && (tdif > durationUs * 1000))) 
			{
				printf("WARNING: Timeout waiting for Helios buffer chip: %d\n", tdif / 1000);

				//GPIO_DIR_OUT(GPIOPIN_MCURESET_LED);
				//GPIO_CLR(GPIOPIN_MCURESET_LED); // turn off resetting for now
				//GPIO_SET(GPIOPIN_MCURESET_LED);
				//GPIO_DIR_IN(GPIOPIN_MCURESET_LED);

				//struct timespec delay, dummy; // Prevents hogging 100% CPU use
				//delay.tv_sec = 0;
				//delay.tv_nsec = 500000;
				//nanosleep(&delay, &dummy);

				isBusy = false;
				return 0;
			}
			if (durationUs > 500) // todo use duration of previous write instead of durationUs
			{
				struct timespec delay, dummy; // Prevents hogging 100% CPU use
				delay.tv_sec = 0;
				delay.tv_nsec = 100000;
				nanosleep(&delay, &dummy);
			}
			//std::this_thread::yield(); //todo use sleep if RT scheduling?
		};*/
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;
		unsigned long rdyReceivedTdif = tdif;

		//printf("got ready\n");

		//write the whole block all at once
		int writeRet = write(this->spidevFd, writeBuffer, dataSizeBytes + 16 + 4);

		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif * 1000000000 + nsdif;

		//if (rdyReceivedTdif < 100000)
		//	printf("Helios sent frame: %d\n", tdif / 1000);

		// TODO REMOVE THIS PRINT
		//#ifndef NDEBUG
				//printf("Sent helPro frame: samples %d, left %d, txNum %d, pps %d, time %d, timerVal %d, rem %f, repeats %d, startY %d\n", pointsThisFrame, pointsLeft, txNum, pps, tdif / 1000, desiredTimerInt, timerRemainder, timerRepeats, data[1]);
		//#endif

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
		if (writeRet != (dataSizeBytes + 16 + 4))
		{
			isBusy = false;
			perror("spi write error");
			printf("msg size = %u, err %d\n", dataSizeBytes + 16 + 4, writeRet);
			return 0;
		}
#ifndef NDEBUG
		//printf("wrote to HelPro size = %u\n", dataSizeBytes + 16 + 4);
#endif

		/*if (false)//durationUs > 200)
		{
			struct timespec then2;
			clock_gettime(CLOCK_MONOTONIC, &then2);
			while (GPIO_LEV(GPIOPIN_STATUS)) // Wait for chip to signal transfer was received, to avoid race condition
			{
				clock_gettime(CLOCK_MONOTONIC, &now);
				sdif = now.tv_sec - then2.tv_sec;
				nsdif = now.tv_nsec - then2.tv_nsec;
				tdif = sdif * 1000000000 + nsdif;
				//printf("no2: %d\n", tdif / 1000);
				if (tdif > 100000)
				{
					printf("WARNING: Helios write ack NOT recvd: %d\n", tdif / 1000);
					break;
				}
				if (durationUs > 200)
					std::this_thread::yield(); //todo use sleep if RT scheduling?
			};
			clock_gettime(CLOCK_MONOTONIC, &now);
			sdif = now.tv_sec - then2.tv_sec;
			nsdif = now.tv_nsec - then2.tv_nsec;
			tdif = sdif * 1000000000 + nsdif;
			if (tdif < 10000)
				printf("Helios write ack recvd: %d\n", tdif / 1000);
		}*/

		/*do
		{
			if (durationUs > 300)
				std::this_thread::yield(); // todo sleep if RT scheduler

			clock_gettime(CLOCK_MONOTONIC, &now);
			sdif = now.tv_sec - then.tv_sec;
			nsdif = now.tv_nsec - then.tv_nsec;
			tdif = sdif * 1000000000 + nsdif;

		} while (tdif < ((unsigned long)durationUs * 1000 * 0.3));*/

		pointsLeft -= pointsThisFrame;
		offsetPoints += pointsThisFrame;
	}

	isBusy = false;

#ifndef NDEBUG
	//printf("Finished helPro frame\n");
#endif


	return 0;
}

SliceType HeliosProAdapter::convertPoints(const std::vector<ISPDB25Point>& points) 
{

	SliceType result(points.size() * bytesPerPoint());

	int index = 0;

	for (const auto& point : points) 
	{
		result[index++] = (point.y & 0xFF);
		result[index++] = ((point.y >> 8) & 0xFF);
		result[index++] = ((point.intensity >> 4) & 0xFF);
		result[index++] = ((point.intensity >> 12) & 0xFF);
		result[index++] = ((point.u3 >> 4) & 0xFF);
		result[index++] = ((point.u3 >> 12) & 0xFF);
		result[index++] = ((point.u2 >> 4) & 0xFF);
		result[index++] = ((point.u2 >> 12) & 0xFF);
		result[index++] = ((point.u1 >> 4) & 0xFF);
		result[index++] = ((point.u1 >> 12) & 0xFF);
		result[index++] = ((point.b >> 4) & 0xFF);
		result[index++] = ((point.b >> 12) & 0xFF);
		result[index++] = ((point.g >> 4) & 0xFF);
		result[index++] = ((point.g >> 12) & 0xFF);
		result[index++] = (point.x & 0xFF);
		result[index++] = ((point.x >> 8) & 0xFF);
		result[index++] = ((point.r >> 4) & 0xFF);
		result[index++] = ((point.r >> 12) & 0xFF);
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

void HeliosProAdapter::getName(char* nameBufferPtr, unsigned nameBufferSize) {
	snprintf(nameBufferPtr, nameBufferSize, "Main");
}




