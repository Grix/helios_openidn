// -------------------------------------------------------------------------------------------------
//  File V1LaproGraphOut.cpp
//
//  Version 1 compatibility layer
//
//  01/2025 Dirk Apitz, moved to new output architecture
// -------------------------------------------------------------------------------------------------


// Project headers
#include "../shared/idn.h"
#include "../shared/idn-stream.h"
#include "../shared/idn-hello.h"

// Module header
#include "V1LaproGraphOut.hpp"



// =================================================================================================
//  Class V1LaproGraphicOutput
//
// -------------------------------------------------------------------------------------------------
//  scope: private
// -------------------------------------------------------------------------------------------------

void V1LaproGraphicOutput::resetChunkBuffer()
{
    currentSlicePoints.clear();
    currentSliceTime = this->usPerSlice;
}


void V1LaproGraphicOutput::commitChunk()
{
    if(currentSlicePoints.size() != 0) {
        std::shared_ptr<DACHWInterface> device = driverPtr->getDevice();
        //if current slice is full, convert it to bytes, put it into
        //the ringbuffer, reset it and reset the currentSliceTime
        std::shared_ptr<TimeSlice> newSlice(new TimeSlice);
        newSlice->dataChunk = device->convertPoints(currentSlicePoints);
        newSlice->durationUs = this->usPerSlice - this->currentSliceTime;
        bex->networkAppendSlice(newSlice);
        resetChunkBuffer();
    }
}


//add a point to the current slice / filter it
void V1LaproGraphicOutput::addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata)
{
    unsigned pointsPerFrame = metadata.len;
    double pointDuration = (double)metadata.dur / pointsPerFrame;
    double targetPointRate = ((1000000.0*(double)pointsPerFrame) / (double)metadata.dur);
    //TODO: move this somewhere static
    std::shared_ptr<DACHWInterface> device = driverPtr->getDevice();
    double maxDevicePointRate = device->maxPointrate();
    double rateRatio = maxDevicePointRate / targetPointRate;


    //start downsampling if the maximum device pointrate is exceeded
    if(rateRatio < 1.0) {
        //skip point, but act like it was added
        //this keeps rateRatio% of points
        if(skipCounter >= rateRatio) {
            skipCounter += rateRatio;
            skipCounter -= (int)skipCounter;
            currentSliceTime -= pointDuration;
            return;
        }
        skipCounter += rateRatio;
        skipCounter -= (int)skipCounter;
    }

    //here the current slice has enough space,
    //so append the point and decrease
    //the currentSliceTime by the point duration
    currentSlicePoints.push_back(newPoint);
    currentSliceTime -= pointDuration;

    //chunk the points
    if(metadata.isWave) {
        //wave mode chunks into chunks of x ms that don't exceed the maximum amount of
        //bytes that the adapter accepts
        if(currentSliceTime < 0 ||
                device->bytesPerPoint()*(currentSlicePoints.size()+1) > device->maxBytesPerTransmission()) {
            commitChunk();
        }
    } else if(!metadata.isWave && device->maxBytesPerTransmission() != -1) {
        //frame mode does not need to chunk based on duration, but it still needs evenly sized chunks if
        //the device has a point limit, so it tries to equalize them
        unsigned numChunksOfBlockSize = ceil(((double)metadata.len * (double)device->bytesPerPoint())/ (double)device->maxBytesPerTransmission());
        unsigned equalizedSize = ceil((double)metadata.len / (double)numChunksOfBlockSize);

        if(currentSlicePoints.size()+1 > equalizedSize) {
            commitChunk();
        }
    }
}


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

V1LaproGraphicOutput::V1LaproGraphicOutput(std::shared_ptr<HWBridge> hwBridge, std::shared_ptr<BEX> bex)
{
    driverPtr = hwBridge;
    this->bex = bex;

    opMode = OPMODE_IDLE;
}


V1LaproGraphicOutput::~V1LaproGraphicOutput()
{
}


void V1LaproGraphicOutput::getDeviceName(char *nameBufferPtr, unsigned nameBufferSize)
{
    driverPtr->getDevice()->getName(nameBufferPtr, nameBufferSize);
}


int V1LaproGraphicOutput::open(OPMODE opMode)
{
    if (this->opMode != OPMODE_IDLE) return -1;

    if (opMode == OPMODE_WAVE)
    {
//FIXME: log
        this->opMode = opMode;
    }
    else if (opMode == OPMODE_FRAME)
    {
//FIXME: log
        this->opMode = opMode;
    }
    else
    {
//FIXME: log
        return -1;
    }

    return 0;
}


void V1LaproGraphicOutput::close()
{
    if (this->opMode == OPMODE_IDLE)
    {
//FIXME: log
        return;
    }

//FIXME: log

    bex->setMode(DRIVER_INACTIVE);
    this->opMode = OPMODE_IDLE;
}


void V1LaproGraphicOutput::process(CHUNKDATA &chunkData, uint8_t *recvBuffer, unsigned recvLen)
{
    // Create new frame
    ISPFrameMetadata frame;
    memset(&frame, 0, sizeof(ISPFrameMetadata));
    if (opMode == OPMODE_WAVE)
    {
        frame.isWave = 1;
    }
    else if (opMode == OPMODE_FRAME)
    {
        frame.isWave = 0;
        frame.once = chunkData.chunkFlags & IDNFLG_GRAPHIC_FRAME_ONCE;
    }
    frame.dur = chunkData.chunkDuration;


    //set BEX mode
    if (frame.isWave)
    {
        bex->setMode(DRIVER_WAVEMODE);
    }
    else
    {
        //reset in case there was part of a wave chunk in the buffers already
        resetChunkBuffer();
        bex->setMode(DRIVER_FRAMEMODE);
    }

    // Decode the input samples into the internally used struct
    std::vector<ISPDB25Point> currentFramePoints(chunkData.sampleCount);
    chunkData.decoder->decode((uint8_t *)currentFramePoints.data(), recvBuffer, chunkData.sampleCount);

    //feed the points to the driver
    frame.len = currentFramePoints.size();
    for(auto& point : currentFramePoints)
    {
        addPointToSlice(point, frame);
    }

    //frame is over
    //if it was a frame-mode frame, publish
    //the buffer to the driver
    if (!frame.isWave)
    {
        //push final chunk even if it's not finished yet
        commitChunk();
        bex->publishReset();
    }
}

bool V1LaproGraphicOutput::hasBufferedFrame()
{
    return bex->hasBufferedFrame();
}

