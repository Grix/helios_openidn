// -------------------------------------------------------------------------------------------------
//  File NOPLaproGraphOut.cpp
//
//  No OPeration Laser Projector Graphic Output
//
//  04/2025 Dirk Apitz, created
// -------------------------------------------------------------------------------------------------


// Module header
#include "NOPLaproGraphOut.hpp"



// =================================================================================================
//  Class NOPLaproGraphicOutput
//
// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

NOPLaproGraphicOutput::NOPLaproGraphicOutput()
{
}


NOPLaproGraphicOutput::~NOPLaproGraphicOutput()
{
}


void NOPLaproGraphicOutput::getDeviceName(char *nameBufferPtr, unsigned nameBufferSize)
{
}


int NOPLaproGraphicOutput::open(ODF_ENV *env, OPMODE opMode)
{
    return 0;
}


void NOPLaproGraphicOutput::close(ODF_ENV *env)
{
}


void NOPLaproGraphicOutput::process(ODF_ENV *env, CHUNKDATA &chunkData, ODF_TAXI_BUFFER *taxiBuffer)
{
    if(taxiBuffer != (ODF_TAXI_BUFFER *)0) taxiBuffer->discard();
}

