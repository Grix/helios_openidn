// -------------------------------------------------------------------------------------------------
//  File RTLaproGraphOut.hpp
//
//  01/2025 Dirk Apitz, created
// -------------------------------------------------------------------------------------------------


#ifndef RT_LAPRO_GRAPHIC_OUTPUT_HPP
#define RT_LAPRO_GRAPHIC_OUTPUT_HPP


// Standard libraries
#include <stdint.h>

// Project headers
#include "../shared/ODFTaxiBuffer.hpp"
#include "RTOutput.hpp"



// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class RTLaproDecoder
{
    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    RTLaproDecoder();
    virtual ~RTLaproDecoder();

    virtual unsigned getSampleSize() = 0;
    virtual void decode(uint8_t *dstPtr, uint8_t *srcPtr) = 0;
    virtual void decode(uint8_t *dstPtr, uint8_t *srcPtr, unsigned sampleCount) = 0;
};


class RTLaproGraphicOutput: public RTOutput
{
    public:

    enum OPMODE  { OPMODE_IDLE = 0, OPMODE_WAVE = 1, OPMODE_FRAME = 2 };
    enum MODFLAG { MODFLAG_SCAN_ONCE = 1, MODFLAG_DISCONTINUOUS = 2 };

    typedef struct
    {
        uint32_t chunkDuration;
        uint8_t modFlags;

        RTLaproDecoder *decoder;
        unsigned sampleSize;
        unsigned sampleCount;

    } CHUNKDATA;


    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    RTLaproGraphicOutput();
    virtual ~RTLaproGraphicOutput();

    virtual int open(OPMODE opMode) = 0;
    virtual void close() = 0;

    virtual RTLaproDecoder *createIDNDecoder(uint8_t serviceMode, void *paramPtr, unsigned paramLen);
    virtual void deleteDecoder(RTLaproDecoder *decoder);

    virtual void process(CHUNKDATA &chunkData, ODF_TAXI_BUFFER *taxiBuffer) = 0;
};


#endif

