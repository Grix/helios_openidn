// -------------------------------------------------------------------------------------------------
//  File V1LaproGraphOut.hpp
//
//  Version 1 compatibility layer
//
//  01/2025 Dirk Apitz, moved to new output architecture
// -------------------------------------------------------------------------------------------------


#ifndef V1_LAPRO_GRAPHIC_OUTPUT_HPP
#define V1_LAPRO_GRAPHIC_OUTPUT_HPP


// Standard libraries
#include <vector>

// Project headers
#include "../shared/types.h"
#include "../shared/HWBridge.hpp"
#include "../shared/BEX.hpp"
#include "RTLaproGraphOut.hpp"



// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class V1LaproGraphicOutput: public RTLaproGraphicOutput
{
    // ------------------------------------------ Members ------------------------------------------


    ////////////////////////////////////////////////////////////////////////////////////////////////
    private:

    std::shared_ptr<HWBridge> driverPtr = nullptr;
    std::shared_ptr<BEX> bex = nullptr;

    OPMODE opMode;

    double usPerSlice = 10000;
    double currentSliceTime = usPerSlice;
    unsigned convertedChunks = 0;
	std::vector<ISPDB25Point> currentSlicePoints;
	double skipCounter = 0;

    void resetChunkBuffer();
    void commitChunk();
    void addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata);


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    V1LaproGraphicOutput(std::shared_ptr<HWBridge> driver, std::shared_ptr<BEX> bufferExchange);
    ~V1LaproGraphicOutput();

    // -- Inherited Members -------------
    virtual void getDeviceName(char *nameBufferPtr, unsigned nameBufferSize);
    virtual int open(OPMODE opMode);
    virtual void close();
    virtual void process(CHUNKDATA &chunkData, uint8_t *recvBuffer, unsigned recvLen);

    bool hasBufferedFrame();

    // -- Inline Methods ----------------
    void setChunkLengthUs(double us) { this->usPerSlice = us; }
};


#endif
