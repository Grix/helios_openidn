

#ifndef SERVICE_HPP
#define SERVICE_HPP


// Standard libraries
#include <vector>

// Project headers
#include "HWBridge.h"
#include "types.h"
#include "DACHWInterface.h"
#include "BEX.h"
#include "idn-hello.h"



// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class LaproService
{
    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    private:

    std::shared_ptr<HWBridge> driverPtr = nullptr;
    std::shared_ptr<BEX> bex = nullptr;

    bool dacBusyFlag;

    IDNChannel* channel;
    double usPerSlice = 5000;
    double currentSliceTime = usPerSlice;
    unsigned convertedChunks = 0;
	std::vector<ISPDB25Point> currentSlicePoints;
	double skipCounter = 0;

    unsigned int buildDictionary(uint8_t *buf, unsigned int len, unsigned int offset, uint8_t scwc, IDNDescriptorTag** data);
    void resetChunkBuffer();
    void commitChunk();
    void addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata);


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    LaproService(std::shared_ptr<HWBridge> driver, std::shared_ptr<BEX> bufferExchange);
    ~LaproService();

    void getName(char *nameBufferPtr, unsigned nameBufferSize);

    void checkTimeout();

    void processChannelMessage(uint8_t *recvBuffer, unsigned recvLen);

    // -- Inline Methods ----------------
    void setChunkLengthUs(double us) { this->usPerSlice = us; }
    bool getDACBusy() { return dacBusyFlag; }
    void setDACBusy(bool dacBusyFlag) { this->dacBusyFlag = dacBusyFlag; }
};


#endif
