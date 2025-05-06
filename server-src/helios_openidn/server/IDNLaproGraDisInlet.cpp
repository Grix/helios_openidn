// -------------------------------------------------------------------------------------------------
//  File IDNLaproGraDisInlet.cpp
//
//  Copyright (c) 2013-2025 DexLogic, Dirk Apitz. All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
// -------------------------------------------------------------------------------------------------
//
//  Laser projector discrete graphic inlet
//
// -------------------------------------------------------------------------------------------------
//  Change History:
//
//  07/2013 Dirk Apitz, created
//  01/2025 Dirk Apitz, modifications and integration into OpenIDN
// -------------------------------------------------------------------------------------------------


// Standard libraries
#include <string.h>

// Project headers
#include "../shared/idn-stream.h"
#include "../shared/ODFTools.hpp"
#include "../shared/PEVFlags.h"

// Module header
#include "IDNLaproGraDisInlet.hpp"



// =================================================================================================
//  Class IDNLaproGraDisInlet
//
// -------------------------------------------------------------------------------------------------
//  scope: protected
// -------------------------------------------------------------------------------------------------

IDNLaproGraDisInlet::IDNLaproGraDisInlet(RTLaproGraphicOutput *rtOutput):
    Inherited(rtOutput)
{
}


IDNLaproGraDisInlet::~IDNLaproGraDisInlet()
{
}


uint8_t IDNLaproGraDisInlet::getServiceMode()
{
    return IDNVAL_SMOD_LPGRF_DISCRETE;
}


void IDNLaproGraDisInlet::process(ODF_ENV *env, ODF_TAXI_BUFFER *taxiBuffer)
{
    IDNHDR_CHANNEL_MESSAGE *channelMessageHdr = (IDNHDR_CHANNEL_MESSAGE *)taxiBuffer->getPayloadPtr();
    uint16_t contentID = btoh16(channelMessageHdr->contentID);
    uint16_t chunkType = (contentID & IDNMSK_CONTENTID_CNKTYPE);


// FIXME: Reassembly queue


    // Read the configuration and build a decoder
    readConfig(env, taxiBuffer);

    // Dispatch the chunk type
    do
    {
        if(chunkType == IDNVAL_CNKTYPE_VOID)
        {
            // Message does not contain data (may just open/close, absolutely valid)
            break;
        }
        else if(chunkType == IDNVAL_CNKTYPE_LPGRF_FRAME)
        {
            // Check for valid configuration
            if(!isConfigValid() || (decoder == (RTLaproDecoder *)0))
            { 
                pipelineEvents |= IDN_PEVFLG_INLET_CFGERR;
                //output->frameStatsModeConfig(); 
                break; 
            }

            // Unwrap data: Remove channel message header
            taxiBuffer->adjustFront(-1 * (int)sizeof(IDNHDR_CHANNEL_MESSAGE));

            // Unwrap data: Remove config header (in case present)
            if((chunkType <= 0xBF) && (contentID & IDNFLG_CONTENTID_CONFIG_LSTFRG))
            {
                IDNHDR_CHANNEL_CONFIG *channelConfigHdr = (IDNHDR_CHANNEL_CONFIG *)&channelMessageHdr[1];
                unsigned configSize = sizeof(IDNHDR_CHANNEL_CONFIG);
                configSize += channelConfigHdr->wordCount * sizeof(uint32_t);

                taxiBuffer->adjustFront(-1 * configSize);
            }

            // Unwrap data: Remove sample chunk header
            IDNHDR_SAMPLE_CHUNK *sampleChunkHdr = (IDNHDR_SAMPLE_CHUNK *)taxiBuffer->getPayloadPtr();
            uint32_t flagsDuration = btoh32(sampleChunkHdr->flagsDuration);
            taxiBuffer->adjustFront(-1 * (int)sizeof(IDNHDR_SAMPLE_CHUNK));

            // Check for integer sample count
            unsigned sampleSize = decoder->getSampleSize();
            if((taxiBuffer->getPayloadLen() % sampleSize) != 0)
            {
                pipelineEvents |= IDN_PEVFLG_OUTPUT_PVLERR;
                break;
            }

            // Build chunk data struct
            RTLaproGraphicOutput::CHUNKDATA chunkData;
            memset(&chunkData, 0, sizeof(chunkData));
            chunkData.chunkFlags = flagsDuration >> 24;
            chunkData.chunkDuration = flagsDuration & 0x00FFFFFF;
            chunkData.decoder = decoder;
            chunkData.sampleCount = taxiBuffer->getPayloadLen() / sampleSize;

            // Check for match between config version and chunk data version
            if((chunkData.chunkFlags & 0x30) != (getConfigFlags() & 0x30))
            {
                pipelineEvents |= IDN_PEVFLG_INLET_DCMERR;
                //output->waveStatsVersionMismatch(getConfigFlags(), chunkFlags);
                break;
            }

            // Check for minimum sample count / valid duration
            // Note: Empty chunk already checked
            if((chunkData.sampleCount < 2) || (chunkData.chunkDuration == 0))
            {
                pipelineEvents |= IDN_PEVFLG_INLET_MCLERR;
                //output->frameStatsLength();
                break;
            }

            // Send the buffer to the driver
            // Note: Preliminary solution: The buffer should be queued/appended (frames from multiple
            // inlets) and copied/passed to the output
            rtOutput->process(chunkData, (uint8_t *)taxiBuffer->getPayloadPtr(), taxiBuffer->getPayloadLen());
        }
        else
        {
            pipelineEvents |= IDN_PEVFLG_INLET_CKTERR;
            //output->frameStatsDataType(dataType);
            break;
        }
    }
    while(0);

    // In case the taxi buffer was not passed - free memory. No more access!
    if(taxiBuffer != (ODF_TAXI_BUFFER *)0)
    {
        taxiBuffer->discard();
    }
}

