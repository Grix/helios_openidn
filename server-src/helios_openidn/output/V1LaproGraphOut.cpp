// -------------------------------------------------------------------------------------------------
//  File V1LaproGraphOut.cpp
//
//  Version 1 compatibility layer
//
//  01/2025 Dirk Apitz, moved to new output architecture
// -------------------------------------------------------------------------------------------------


// Module header
#include "V1LaproGraphOut.hpp"



// =================================================================================================
//  Class V1LaproGraphicOutput
//
// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

V1LaproGraphicOutput::V1LaproGraphicOutput(std::shared_ptr<HWBridge> hwBridge)
{
    driverPtr = hwBridge;

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

    driverPtr->bexSetMode(DRIVER_INACTIVE);
    this->opMode = OPMODE_IDLE;
}


void V1LaproGraphicOutput::process(CHUNKDATA &chunkData, ODF_TAXI_BUFFER *taxiBuffer)
{
    // Create and populate a new chunk
    std::shared_ptr<DB25Chunk> db25Chunk(new DB25Chunk);
    db25Chunk->duration = chunkData.chunkDuration;

    // Populate chunk metadata
    if (opMode == OPMODE_WAVE)
    {
        db25Chunk->isWave = 1;
        db25Chunk->once = 0;

        driverPtr->bexSetMode(DRIVER_WAVEMODE);
    }
    else if (opMode == OPMODE_FRAME)
    {
        db25Chunk->isWave = 0;
        db25Chunk->once = ((chunkData.modFlags & MODFLAG_SCAN_ONCE) != 0) ? 1 : 0;

        driverPtr->bexSetMode(DRIVER_FRAMEMODE);
    }
    else
    {
        return;
    }


    // Decode the input samples into the internally used struct
    db25Chunk->db25Samples.resize(chunkData.sampleCount);
    ODF_TAXI_BUFFER *fragBuf = taxiBuffer;
    uint8_t *srcPtr = (uint8_t *)fragBuf->getPayloadPtr();
    unsigned srcLen = fragBuf->getFragmentLen();
    uint8_t *dstPtr = (uint8_t *)db25Chunk->db25Samples.data();
    while(fragBuf != (ODF_TAXI_BUFFER *)0)
    {
        unsigned fragSampleCount = srcLen / chunkData.sampleSize;
        chunkData.decoder->decode(dstPtr, srcPtr, fragSampleCount);
        srcPtr = &srcPtr[fragSampleCount * chunkData.sampleSize];
        srcLen -= fragSampleCount * chunkData.sampleSize;
        dstPtr = &dstPtr[fragSampleCount * sizeof(ISPDB25Point)];

        // Check for last sample in the fragment spanning across two data fragments
        if(srcLen != 0)
        {
            // Copy the remainder of the fragment
            uint8_t sample[chunkData.sampleSize];
            memcpy(sample, srcPtr, srcLen);
            uint8_t *contPtr = &sample[srcLen];
            unsigned leftover = chunkData.sampleSize - srcLen;

            // Get the next fragment
            fragBuf = fragBuf->getNext();
            if(fragBuf == (ODF_TAXI_BUFFER *)0)
            {
                taxiBuffer->discard();
                return;
            }

            srcPtr = (uint8_t *)fragBuf->getPayloadPtr();
            srcLen = fragBuf->getFragmentLen();
            if(srcLen < leftover)
            {
                taxiBuffer->discard();
                return;
            }

            // Copy the remainder of the sample
            memcpy(contPtr, srcPtr, leftover);
            srcPtr = &srcPtr[leftover];
            srcLen -= leftover;

            // Decode the sample
            chunkData.decoder->decode(dstPtr, sample);
            dstPtr = &dstPtr[sizeof(ISPDB25Point)];
        }
        else
        {
            fragBuf = fragBuf->getNext();
            if(fragBuf != (ODF_TAXI_BUFFER *)0)
            {
                srcPtr = (uint8_t *)fragBuf->getPayloadPtr();
                srcLen = fragBuf->getFragmentLen();
            }
        }
    }
    taxiBuffer->discard();


    // Send to Buffer EXchange
    driverPtr->bexNetworkAppendSlice(db25Chunk);

    // If it was a frame-mode chunk, publish the buffer to the driver
    if (!db25Chunk->isWave)
    {
        // Push final chunk even if it's not finished yet
        driverPtr->bexPublishReset();
    }
}

bool V1LaproGraphicOutput::hasBufferedFrame()
{
    return driverPtr->bex->hasBufferedFrame();
}

