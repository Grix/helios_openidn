#include "./HWBridge.h"


HWBridge::HWBridge(std::shared_ptr<DACHWInterface> hwDeviceInterface, std::shared_ptr<BEX> bufferExchange) : device(hwDeviceInterface), bex(bufferExchange) {
}

void HWBridge::outputEmptyPoint() {
	ISPDB25Point point;
	point.x = 0x8000;
	point.y = 0x8000; 
	point.r = 0x0000;
	point.g = 0x0000;
	point.b = 0x0000;
	point.intensity = 0x0000;
	point.shutter = 0x0000;
	point.u1 = 0x0000;
	point.u2 = 0x0000;
	point.u3 = 0x0000;
	point.u4 = 0x0000;

	std::vector<ISPDB25Point> emptyPointSlice;
	emptyPointSlice.push_back(point);

	TimeSlice emptySlice;
	emptySlice.dataChunk = this->device->convertPoints(emptyPointSlice);
	emptySlice.durationUs = 1000;
	this->device->writeFrame(emptySlice, emptySlice.durationUs);
}

void HWBridge::trimBuffer(std::shared_ptr<SliceBuf> buf, unsigned n)
{
	while (buf->size() > n) {
		buf->pop_front();
	}
}

void HWBridge::driverLoop() {
	struct timespec now, then;
	struct timespec lastDebugTime;
	unsigned sdif, nsdif, tdif;
	std::shared_ptr<SliceBuf> currentBuf(new SliceBuf);
	unsigned sliceCounter = 0;

	clock_gettime(CLOCK_MONOTONIC, &lastDebugTime);

	while (true) {
		//before anything else, output some simple debugging if requested
		if(debug == DEBUGSIMPLE) {
			clock_gettime(CLOCK_MONOTONIC, &now);
			if(now.tv_sec - lastDebugTime.tv_sec) {
				printf("SIMPLE DEBUG: ");
				if(bex->getMode() == DRIVER_INACTIVE)
					printf("Idle ");
				else if(bex->getMode() == DRIVER_FRAMEMODE)
					printf("Frame Mode ");
				else if(bex->getMode() == DRIVER_WAVEMODE)
					printf("Wave Mode ");

				//calculate the average point speed of the previous second
				if(writeDuration.size() > 0 && numberOfPoints.size() > 0)
					printf("%.2f kpps ", 1000.0*(double)numberOfPoints.front()/(double)writeDuration.front());

				//calculate the average wave buffer usage of the previous second
				if(bex->getMode() == DRIVER_WAVEMODE) {
					double waveBufStatSize = waveBufUsage.size();
					double sum = 0;
					if(waveBufStatSize > 0) {
						for(double bufUsage : waveBufUsage) {
							sum += bufUsage * (double)writeDuration.front();
						}
					}

					printf("%.2f ms Buf. Usage ", sum / (1000.0*waveBufStatSize));
				}

				printf("\n");
				clearStats();
				lastDebugTime = now;
			}
		}

		std::shared_ptr<SliceBuf> newBuf = bex->driverSwapRequest();
		//if the new buffer was not ready yet, continue with the old one
		if(newBuf) {
			//if there is something new, play that
			currentBuf = newBuf;

			//only trim and adjust speed in wave mode
			if(bex->getMode() == DRIVER_WAVEMODE) {
				speedFactor = calculateSpeedfactor(speedFactor, currentBuf);
				if (debug == DEBUGSIMPLE) {
					printf("New buffer: %.2f Speed factor \n", speedFactor);
				}
			} else if (bex->getMode() == DRIVER_FRAMEMODE) {
				speedFactor = 1.0;
			}
		} else if(bex->getMode() == DRIVER_WAVEMODE || bex->getMode() == DRIVER_INACTIVE) {
			//write an empty point if there is a buffer underrun in wave mode or
			//the driver is set to inactive
			outputEmptyPoint();

			struct timespec delay, dummy; // Prevents hogging 100% CPU use
			delay.tv_sec = 0;
			delay.tv_nsec = 2000;
			nanosleep(&delay, &dummy);

			continue;
		}
		
		//rotate through the buffer once
		unsigned currentBufSize = currentBuf->size();
		for(int i = 0; i < currentBufSize; i++) {
			clock_gettime(CLOCK_MONOTONIC, &then);
			std::shared_ptr<TimeSlice> nextSlice = currentBuf->front();
			currentBuf->pop_front();

			//if we're in frame mode, put the slice back
			//wave mode just discards
			if(bex->getMode() == DRIVER_FRAMEMODE) {
				currentBuf->push_back(nextSlice);
			}

			this->device->writeFrame(*nextSlice, speedFactor*nextSlice->durationUs);

			//measure timing
			clock_gettime(CLOCK_MONOTONIC, &now);
			sdif = now.tv_sec - then.tv_sec;
			nsdif = now.tv_nsec - then.tv_nsec;
			tdif = sdif * 1000000000 + nsdif;


			if(debug != NODEBUG) {
				//add stats
				writeTimingMeasurements.push_back(tdif/1000);
				writeDuration.push_back(nextSlice->durationUs);
				numberOfPoints.push_back(nextSlice->dataChunk.size()/device->bytesPerPoint());
				speedFactors.push_back(speedFactor);
				waveBufUsage.push_back(currentBufSize);

				if(debug == DEBUGLIVE) {
					sliceCounter++;
					if(sliceCounter > 200) {
						printStats();
						sliceCounter = 0;
					}
				}
			}
		}

		struct timespec delay, dummy; // Prevents hogging 100% CPU use
		delay.tv_sec = 0;
		delay.tv_nsec = 1000;
		nanosleep(&delay, &dummy);
	}
}

double HWBridge::calculateSpeedfactor(double currentSpeed, std::shared_ptr<SliceBuf> buffer) {
	double sm = 5;
	if(buffer->size() != 0) {
		double center = this->bufferTargetMs;
		//bufusage in ms = bufsize * avg slice duration
		double bufUsageMs = (double)buffer->size()*(double)buffer->front()->durationUs / 1000.0;
		double offCenter = (center - bufUsageMs) / center;
		this->accumOC += offCenter;

		double newSpeed = (1.0 + offCenter + 0.005*accumOC);
		newSpeed = (newSpeed + ((sm-1)*currentSpeed))/sm;

		if (debug == DEBUGSIMPLE)
			printf("Calculating speed factor: center %.2f, bufUsageMs %.2f, buffer->size() %.2f, buffer->front()->durationUs %.2f, accumOC %.2f, newSpeed %.2f \n", center, bufUsageMs, (double)buffer->size(), (double)buffer->front()->durationUs, this->accumOC, newSpeed);

		return std::min(10.0, std::max(0.01, newSpeed));
	} else {
		return 1.0;
	}
}

std::shared_ptr<DACHWInterface> HWBridge::getDevice() {
	return this->device;
}

void HWBridge::setBufferTargetMs(double targetMs) {
	this->bufferTargetMs = targetMs;
}

void HWBridge::setDebugging(int debug) {
	this->debug = debug;
}

int HWBridge::getDebugging() {
	return this->debug;
}

void HWBridge::printStats() {
	fprintf(stderr, "writeTimingMeasurements ");
	for(const auto& elem : writeTimingMeasurements)
		fprintf(stderr, "%u ", elem);
	fprintf(stderr, "\n");

	fprintf(stderr, "writeDuration ");
	for(const auto& elem : writeDuration)
		fprintf(stderr, "%u ", elem);
	fprintf(stderr, "\n");

	fprintf(stderr, "numberOfPoints ");
	for(const auto& elem : numberOfPoints)
		fprintf(stderr, "%u ", elem);
	fprintf(stderr, "\n");

	fprintf(stderr, "waveBufUsage ");
	for(const auto& elem : waveBufUsage)
		fprintf(stderr, "%f ", elem);
	fprintf(stderr, "\n");

	fprintf(stderr, "speedFactors ");
	for(const auto& elem : speedFactors)
		fprintf(stderr, "%f ", elem);
	fprintf(stderr, "\n");
}

void HWBridge::clearStats() {
	writeTimingMeasurements.clear();
	writeDuration.clear();
	numberOfPoints.clear();
	waveBufUsage.clear();
	speedFactors.clear();
}
