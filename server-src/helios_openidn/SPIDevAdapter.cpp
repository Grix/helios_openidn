#include "SPIDevAdapter.h"

SPIDevAdapter::SPIDevAdapter() {
	this->spidevFd = open("/dev/spidev1.0", O_WRONLY | O_NONBLOCK);
	if (!this->spidevFd) {
		printf("couldn't open spidev1.0: %s", strerror(errno));
	}
	this->maximumPointrate = 85000;
}

SPIDevAdapter::~SPIDevAdapter() {
}

int SPIDevAdapter::writeFrame(const TimeSlice& slice, double duration) {
	//get time
	struct timespec now, then;
	unsigned long sdif, nsdif, tdif;
	clock_gettime(CLOCK_MONOTONIC, &then);

	const SliceType& data = slice.dataChunk;
	unsigned speed;
	double bitsPerByte = 8.0;
	double TUNING = 450; //the spidev or the formula is imprecise, so here's an offset
			     //

	//TODO measure how many clocks per byte
	if(duration > 0.0) {
		speed = (unsigned)((1000000.0 * (double)data.size() * bitsPerByte) / (duration - TUNING));
	} else {
		speed = 100000000;
	}

	int ret = ioctl(this->spidevFd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	if (ret == -1)
		perror("can't set speed hz");

	//write the whole block all at once
	int writeErr = write(this->spidevFd, &data.front(), data.size());
	if(writeErr == -1) {
		perror("spi write error");
		printf("msg size = %u\n", data.size());
	}

	//busy waiting to adjust for timing inaccuracy
	do {
		clock_gettime(CLOCK_MONOTONIC, &now);
		sdif = now.tv_sec - then.tv_sec;
		nsdif = now.tv_nsec - then.tv_nsec;
		tdif = sdif*1000000000 + nsdif ;
	} while (tdif < ((unsigned long)duration * 1000) );


	return 0;
}

SliceType SPIDevAdapter::convertPoints(const std::vector<ISPDB25Point>& points) {
	SlicePrimitive currentConvertedPoint[bytesPerPoint()];
	SliceType result;

	for(const auto& point : points) {
		currentConvertedPoint[0] = 0x00;
		currentConvertedPoint[1] = 0x00 | ((point.x & 0xf000) >> 12) & 0x0f;
		currentConvertedPoint[2] = ((point.x & 0x0ff0) >> 4) & 0xff;
		currentConvertedPoint[3] = ((point.x & 0x000f) << 4) & 0xff;

		currentConvertedPoint[4] = 0x00;
		currentConvertedPoint[5] = 0x10 | ((point.y & 0xf000) >> 12) & 0x0f;
		currentConvertedPoint[6] = ((point.y & 0x0ff0) >> 4) & 0xff;
		currentConvertedPoint[7] = ((point.y & 0x000f) << 4) & 0xff;

		currentConvertedPoint[8] = 0x00;
		currentConvertedPoint[9] = 0x20 | ((point.r & 0xf000) >> 12) & 0x0f;
		currentConvertedPoint[10] = ((point.r & 0x0ff0) >> 4) & 0xff;
		currentConvertedPoint[11] = ((point.r & 0x000f) << 4) & 0xff;

		currentConvertedPoint[12] = 0x00;
		currentConvertedPoint[13] = 0x30 | ((point.g & 0xf000) >> 12) & 0x0f;
		currentConvertedPoint[14] = ((point.g & 0x0ff0) >> 4) & 0xff;
		currentConvertedPoint[15] = ((point.g & 0x000f) << 4) & 0xff;

		currentConvertedPoint[16] = 0x02;
		currentConvertedPoint[17] = 0x40 | ((point.b & 0xf000) >> 12) & 0x0f;
		currentConvertedPoint[18] = ((point.b & 0x0ff0) >> 4) & 0xff;
		currentConvertedPoint[19] = ((point.b & 0x000f) << 4) & 0xff;

		for(int i = 0; i < bytesPerPoint(); i++) {
			result.push_back(currentConvertedPoint[i]);
		}
	}

	return result;
}

unsigned SPIDevAdapter::bytesPerPoint()  {
	return 20;
}


unsigned SPIDevAdapter::maxBytesPerTransmission() {
	return 4096;
}

unsigned SPIDevAdapter::maxPointrate() {
	return maximumPointrate;
}

void SPIDevAdapter::setMaxPointrate(unsigned newRate) {
	this->maximumPointrate = newRate;
}
