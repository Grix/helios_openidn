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

    OPMODE opMode;


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    V1LaproGraphicOutput(std::shared_ptr<HWBridge> driver);
    ~V1LaproGraphicOutput();

    // -- Inherited Members -------------
    virtual void getDeviceName(char *nameBufferPtr, unsigned nameBufferSize);
    virtual int open(OPMODE opMode);
    virtual void close();
    virtual void process(CHUNKDATA &chunkData, ODF_TAXI_BUFFER *taxiBuffer);
    bool hasBufferedFrame();
};


#endif
