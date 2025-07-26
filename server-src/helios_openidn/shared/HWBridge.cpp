#include "./HWBridge.hpp"

#include "../ManagementInterface.hpp"
extern ManagementInterface* management;


double HWBridge::calculateSpeedfactor(double currentSpeed, std::shared_ptr<SliceBuf> buffer) {
	double sm = 5;
	if(buffer->size() != 0) {
		double center = this->bufferTargetMs;
		//bufusage in ms = bufsize * avg slice duration
		double bufUsageMs = (double)buffer->size()*(double)buffer->front()->durationUs / 1000.0;
		double offCenter = (center - bufUsageMs) / center;
		const double hysteresis = 10.0 / center; // +/- 10ms is acceptable
		if (offCenter < hysteresis && offCenter > -hysteresis)
			offCenter = 0;
		this->accumOC += offCenter;

		double newSpeed = (1.0 + 0.3*offCenter + 0.000*accumOC); // Accumulator entirely nullified for now
		newSpeed = (newSpeed + ((sm-1)*currentSpeed))/sm;

		
		if (debug == DEBUGSIMPLE)
			printf("Calculating speed factor: center %.2f, bufUsageMs %.2f, buffer->size() %.2f, buffer->front()->durationUs %.2f, accumOC %.2f, newSpeed %.2f \n", center, bufUsageMs, (double)buffer->size(), (double)buffer->front()->durationUs, this->accumOC, newSpeed);
		

		return std::min(1.3, std::max(0.83, newSpeed));
	} else {
		return 1.0;
	}
}


void HWBridge::clearStats() {
	writeTimingMeasurements.clear();
	writeDuration.clear();
	numberOfPoints.clear();
	waveBufUsage.clear();
	speedFactors.clear();
}


HWBridge::HWBridge(std::shared_ptr<DACHWInterface> hwDeviceInterface) : device(hwDeviceInterface)
{
}

void HWBridge::outputEmptyPoint()
{
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

void HWBridge::driverLoop()
{
    unsigned driverMode = DRIVER_INACTIVE;
    unsigned sliceCounter = 0;

    std::shared_ptr<SliceBuf> currentBufPtr(new SliceBuf);
    double speedFactor = 1.0;

    TransformEnv tfEnv;
    tfEnv.usPerSlice = usPerSlice;
    tfEnv.currentSliceTime = usPerSlice;

    struct timespec lastDebugTime;
	clock_gettime(CLOCK_MONOTONIC, &lastDebugTime);

	while (true) {
		//before anything else, output some simple debugging if requested
		if(debug == DEBUGSIMPLE) {
            struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);
			if(now.tv_sec - lastDebugTime.tv_sec) {
				printf("SIMPLE DEBUG: ");
				if(driverMode == DRIVER_INACTIVE)
					printf("Idle ");
				else if(driverMode == DRIVER_FRAMEMODE)
					printf("Frame Mode ");
				else if(driverMode == DRIVER_WAVEMODE)
					printf("Wave Mode ");

				//calculate the average point speed of the previous second
				if(writeDuration.size() > 0 && numberOfPoints.size() > 0)
					printf("%.2f kpps ", 1000.0*(double)numberOfPoints.front()/(double)writeDuration.front());

				//calculate the average wave buffer usage of the previous second
				if(driverMode == DRIVER_WAVEMODE) {
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

		std::shared_ptr<SliceBuf> nextBufPtr = device->getNextBuffer(tfEnv, driverMode);
		if((nextBufPtr.get() != nullptr) && (nextBufPtr->size() > 0))
		{
			currentBufPtr = nextBufPtr;

			//only trim and adjust speed in wave mode
			if(driverMode == DRIVER_WAVEMODE) {
				speedFactor = calculateSpeedfactor(speedFactor, currentBufPtr);
			} else if (driverMode == DRIVER_FRAMEMODE) {
				speedFactor = 1.0;
			}
		}
		else if(driverMode == DRIVER_WAVEMODE || driverMode == DRIVER_INACTIVE)
		{
			//write an empty point if there is a buffer underrun in wave mode or
			//the driver is set to inactive
			//printf("I am printing empty frames because I am annoying");
			//soutputEmptyPoint();

			if (!hasUnderrun)
				printf("Underrun\n");
			hasUnderrun = true;
			struct timespec delay, dummy; // Prevents hogging 100% CPU use when idle
			delay.tv_sec = 0;
			delay.tv_nsec = 500000; //0.5ms
			nanosleep(&delay, &dummy);

			if (driverMode == DRIVER_INACTIVE)
			{
				//outputEmptyPoint();
				//device->bexResetBuffers();
				this->accumOC = 0;
				struct timespec delay, dummy; // Prevents hogging 100% CPU use when idle
				delay.tv_sec = 0;
				delay.tv_nsec = 2000000; //2ms
				nanosleep(&delay, &dummy);

				management->stopOutput(OUTPUT_MODE_IDN);
			}
			else
				std::this_thread::yield();

			continue;
		}
		//else
		//	std::this_thread::yield();

		hasUnderrun = false;

		//rotate through the buffer once
		unsigned currentBufSize = currentBufPtr->size();
		for(int i = 0; i < currentBufSize; i++)
		{
			struct timespec then;
			clock_gettime(CLOCK_MONOTONIC, &then);


			std::shared_ptr<TimeSlice> nextSlice = currentBufPtr->front();
			currentBufPtr->pop_front();

			if (!management->requestOutput(OUTPUT_MODE_IDN))
			{
				struct timespec delay, dummy; // Prevents hogging 100% CPU
				delay.tv_sec = 0;
				delay.tv_nsec = 2000000; //2ms
				nanosleep(&delay, &dummy);

				continue;
			}

			//if we're in frame mode, put the slice back
			//wave mode just discards
			if(driverMode == DRIVER_FRAMEMODE) {
				currentBufPtr->push_back(nextSlice);
			}

			this->device->writeFrame(*nextSlice, speedFactor*nextSlice->durationUs);


			//measure timing
			struct timespec now;
			clock_gettime(CLOCK_MONOTONIC, &now);

			unsigned sdif = now.tv_sec - then.tv_sec;
			unsigned nsdif = now.tv_nsec - then.tv_nsec;
			unsigned tdif = sdif * 1000000000 + nsdif;

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
	}
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

