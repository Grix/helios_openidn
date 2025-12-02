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

#define NODEBUG 0
#define DEBUG 1
#define DEBUGLIVE 2
#define DEBUGSIMPLE 3



class HWBridge
{
    private:

    std::shared_ptr<DACHWInterface> device;
    double usPerSlice = 15000;
    double bufferTargetMs = 40;
    double accumOC = 0.0;
    bool hasUnderrun = false;
    bool hasStopped = true;

    //stats
    int debug = NODEBUG;
    bool sendStats = false;
    std::vector<unsigned> writeTimingMeasurements, writeDuration, numberOfPoints;
    std::vector<double> speedFactors, waveBufUsage;

    double calculateSpeedfactor(double currentSpeed, std::shared_ptr<SliceBuf> buf);
    void clearStats();


    public:

    HWBridge(std::shared_ptr<DACHWInterface> hwDeviceInterface);
    void outputEmptyPoint();

    void driverLoop();
    void printStats();

    // -- Inline Methods ----------------
    std::shared_ptr<DACHWInterface> getDevice() { return this->device; }
    void setChunkLengthUs(double us) { this->usPerSlice = us; }
    void setBufferTargetMs(double targetMs) { if (targetMs >= 1) this->bufferTargetMs = targetMs; else this->bufferTargetMs = 1; }
    void setDebugging(int debug) { this->debug = debug; }
    int getDebugging() { return this->debug; }
};

#endif
