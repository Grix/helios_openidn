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

#include "./types.h"
#include "./DACHWInterface.h"
#include "./BEX.h"

#define NODEBUG 0
#define DEBUG 1
#define DEBUGLIVE 2
#define DEBUGSIMPLE 3

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
	void getName(char* name);

private:
	void waveIteration();
	void frameIteration();
	double calculateSpeedfactor(double currentSpeed, std::shared_ptr<SliceBuf> buf);
	void resetInternals();

	std::shared_ptr<DACHWInterface> device;
	std::shared_ptr<BEX> bex;
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
