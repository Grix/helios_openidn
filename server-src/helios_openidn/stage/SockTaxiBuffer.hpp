// -------------------------------------------------------------------------------------------------
//  File SockTaxiBufferLRaw.hpp
//
//  Taxi buffer for socket-type (threaded) network interfaces
//
//  04/2025 Dirk Apitz, created
// -------------------------------------------------------------------------------------------------


#ifndef SOCK_TAXI_BUFFER_HPP
#define SOCK_TAXI_BUFFER_HPP


// Standard libraries
#include <stdint.h>



// -------------------------------------------------------------------------------------------------
//  Typedefs
// -------------------------------------------------------------------------------------------------

// OpenIDN uses taxi buffers as an abstraction for the underlaying network interface. They allow for
// dynamic allocation, passing and storage. This is necessary for reassembly and latency Queues.
// Please note that this struct shall not have derivations or virtual functions because of casts.
typedef struct _ODF_TAXI_BUFFER
{
    friend class SockIDNServer;

    ////////////////////////////////////////////////////////////////////////////////////////////////
    private:

    uint16_t payloadLen;                            // Length of the payload (payloadPtr points to)
    void *payloadPtr;                               // Pointer to the message payload

    uint32_t sourceRefTime;                         // Reference time of being sourced/created


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    void discard()
    {
    }

    int coalesce(unsigned len)
    {
        return (payloadLen < len) ? -1 : 0;
    }

    void *getPayloadPtr()
    {
        return payloadPtr;
    }

    unsigned getPayloadLen()
    {
        return payloadLen;
    }

    void adjustFront(int offset)
    {
        // Negative: Drop header; Positive: Add header
        payloadPtr = (void *)((uintptr_t)payloadPtr - offset);
        payloadLen += offset;
    }

    void cropPayload(unsigned len)
    {
        payloadLen = len;
    }

    uint32_t getSourceRefTime()
    {
        return sourceRefTime;
    }

} ODF_TAXI_BUFFER;


#endif
