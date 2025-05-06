#ifndef HWBRIDGE_H_
#define HWBRIDGE_H_

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>

#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <memory>
#include <deque>
#include <algorithm>
#include <signal.h>
#include <thread>

#include "types.h"
#include "DACHWInterface.hpp"
#include "BEX.hpp"

#define NODEBUG 0
#define DEBUG 1
#define DEBUGLIVE 2
#define DEBUGSIMPLE 3


class TransformEnv
{
public:
    double currentSliceTime;

    std::vector<ISPDB25Point> db25Accu;
    double skipCounter = 0;
    
};


class HWBridge {
public:
	HWBridge(std::shared_ptr<DACHWInterface> hwDeviceInterface, std::shared_ptr<BEX> bex);
	void driverLoop();
	void setDebugging(int debug);
	int getDebugging();
	void setBufferTargetMs(double targetMs);
	std::shared_ptr<DACHWInterface> getDevice();
	void printStats();
	void outputEmptyPoint();

    void bexSetMode(int mode);
    void bexPublishReset();
    void bexNetworkAppendSlice(std::shared_ptr<DB25Chunk> db25Chunk);

    // -- Inline Methods ----------------
    void setChunkLengthUs(double us) { this->usPerSlice = us; }


	std::shared_ptr<BEX> bex;

private:
	void waveIteration();
	void frameIteration();
	double calculateSpeedfactor(double currentSpeed, std::shared_ptr<SliceBuf> buf);
	void resetInternals();

    double usPerSlice = 10000;
    void commitChunk(TransformEnv &tfEnv, std::shared_ptr<SliceBuf> &sliceBuf);
    std::shared_ptr<SliceBuf> db25toDevice(TransformEnv &tfEnv, std::shared_ptr<DB25ChunkQueue> db25ChunkQueue);

	std::shared_ptr<DACHWInterface> device;
	double bufferTargetMs = 40;
	double speedFactor = 1.0;
	double accumOC = 0.0;
	std::shared_ptr<SliceBuf> ringBuffer;

	//stats
	int debug = NODEBUG;
	bool sendStats = false;
	std::vector<unsigned> writeTimingMeasurements, writeDuration, numberOfPoints;
	std::vector<double> speedFactors, waveBufUsage;
	void clearStats();

	void trimBuffer(std::shared_ptr<SliceBuf> buf, unsigned n);
};

#endif
