// -------------------------------------------------------------------------------------------------
//  File NOPLaproGraphOut.hpp
//
//  No OPeration Laser Projector Graphic Output
//
//  04/2025 Dirk Apitz, created
// -------------------------------------------------------------------------------------------------


#ifndef NOP_LAPRO_GRAPHIC_OUTPUT_HPP
#define NOP_LAPRO_GRAPHIC_OUTPUT_HPP


// Project headers
#include "RTLaproGraphOut.hpp"



// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class NOPLaproGraphicOutput: public RTLaproGraphicOutput
{
    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    NOPLaproGraphicOutput();
    ~NOPLaproGraphicOutput();

    // -- Inherited Members -------------
    virtual void getDeviceName(char *nameBufferPtr, unsigned nameBufferSize);
    virtual int open(OPMODE opMode);
    virtual void close();
    virtual void process(CHUNKDATA &chunkData, ODF_TAXI_BUFFER *taxiBuffer);
};


#endif
