#include "DummyAdapter.hpp"

DummyAdapter::DummyAdapter() {
	this->maximumPointRate = -1;
}

DummyAdapter::~DummyAdapter() {
}

int DummyAdapter::writeFrame(const TimeSlice& slice, double duration) {
	const SliceType& data = slice.dataChunk;
	struct timespec now, pulse;
	unsigned long sdif, nsdif, tdif;
	unsigned long bpdelay = (unsigned long)(bytesPerPoint() * duration / (double)data.size());

	for(int i = 0; i < data.size(); i += 20) {
		//write the point to spi
		clock_gettime(CLOCK_MONOTONIC, &now);
		writeDACPoint((uint8_t*)&data.front() + i);

		//busy waiting
		do { 
			clock_gettime(CLOCK_MONOTONIC, &pulse);
			sdif = pulse.tv_sec - now.tv_sec;
			nsdif = pulse.tv_nsec - now.tv_nsec;
			tdif = sdif*1000000000 + nsdif ;
		} while (tdif < bpdelay * 1000); 
	}

	return 0;
}

void DummyAdapter::writeDACPoint(const unsigned char* bp) {

}

SliceType DummyAdapter::convertPoints(const std::vector<ISPDB25Point>& points) {
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


unsigned DummyAdapter::bytesPerPoint() {
	return 20;
}

unsigned DummyAdapter::maxBytesPerTransmission() {
	return -1;
}

unsigned DummyAdapter::maxPointrate() {
	return maximumPointRate;
}

void DummyAdapter::setMaxPointrate(unsigned newRate) {
	this->maximumPointRate = newRate;
}

void DummyAdapter::getName(char *nameBufferPtr, unsigned nameBufferSize) {
	snprintf(nameBufferPtr, nameBufferSize, "Dummy");
}
