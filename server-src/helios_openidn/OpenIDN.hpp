

// OpenIDN DAC Framework


#ifndef OPENIDN_HPP
#define OPENIDN_HPP


// Standard libraries
#include <stdint.h>


// -------------------------------------------------------------------------------------------------
//  Typedefs
// -------------------------------------------------------------------------------------------------

// Note: Taxi buffer design allows for dynamic allocation and passing of buffers. This might get
// necessary when a FIFO-Queue for latency is implemented
typedef struct _ODF_TAXI_BUFFER
{
    uint16_t payloadLen;                            // Length of the payload (payloadPtr points to)
    void *payloadPtr;                               // Pointer to the message payload

    // Diagnostics
    uint32_t sourceRefTime;                         // Reference time of being sourced/created
    char diagString[64];                            // A diagnostic string (optional)

} ODF_TAXI_BUFFER;


typedef struct _ODF_ENV
{
    // Currently taxi buffers are statically allocated - so - NOP
    void freeTaxiBuffer(ODF_TAXI_BUFFER *taxiBuffer)
    {
    }

} ODF_ENV;



// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class TracePrinter
{
    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:
    TracePrinter(ODF_ENV *env, const char *frameName);
    ~TracePrinter();

    void logError(const char *format, ...);
    void logWarn(const char *format, ...);
    void logInfo(const char *format, ...);
    void logDebug(const char *format, ...);
};



// -------------------------------------------------------------------------------------------------
//  Prototypes
// -------------------------------------------------------------------------------------------------

void logError(const char *fmt, ...);
void logWarn(const char *fmt, ...);
void logInfo(const char *fmt, ...);



// -------------------------------------------------------------------------------------------------
//  Private inline functions
// -------------------------------------------------------------------------------------------------

static inline uint16_t btoh16(uint16_t value)
{
    uint16_t result = 0;
    uint8_t *p = (uint8_t *)&value;

    result |= (uint16_t)(*p++) << 8;
    result |= (uint16_t)(*p++);

    return result;
}



#endif
