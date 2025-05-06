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


void V1LaproGraphicOutput::process(CHUNKDATA &chunkData, uint8_t *recvBuffer, unsigned recvLen)
{
    // Create and populate a new chink
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
        db25Chunk->once = chunkData.chunkFlags & IDNFLG_GRAPHIC_FRAME_ONCE;

        driverPtr->bexSetMode(DRIVER_FRAMEMODE);
    }
    else
    {
        return;
    }

    // Decode the input samples into the internally used struct
    db25Chunk->db25Samples.resize(chunkData.sampleCount);
    chunkData.decoder->decode((uint8_t *)db25Chunk->db25Samples.data(), recvBuffer, chunkData.sampleCount);

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

