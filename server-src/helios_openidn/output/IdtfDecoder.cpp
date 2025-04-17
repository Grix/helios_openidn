#include "IdtfDecoder.hpp"

void IdtfDecoder::decode(uint8_t* dstPtr, uint8_t* srcPtr)
{
    ISPDB25Point* dstPoint = (ISPDB25Point*)dstPtr;
    dstPoint->x = ((uint16_t*)srcPtr)[0];
    dstPoint->y = ((uint16_t*)srcPtr)[1];
    dstPoint->r = srcPtr[4];
    dstPoint->g = srcPtr[5];
    dstPoint->b = srcPtr[6];
    dstPoint->intensity = srcPtr[7];
    dstPoint->u1 = dstPoint->u2 = dstPoint->u3 = dstPoint->u4 = 0;
}

void IdtfDecoder::decode(uint8_t* dstPtr, uint8_t* srcPtr, unsigned sampleCount)
{
    // Copy sample data
    for (; sampleCount > 0; sampleCount--)
    {
        decode(dstPtr, srcPtr);

        // Next point
        dstPtr = &dstPtr[sizeof(ISPDB25Point)];
        srcPtr = &srcPtr[sampleSize];
    }
}

unsigned IdtfDecoder::getSampleSize()
{
    return sampleSize;
}
