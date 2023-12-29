#ifndef IDNNODE_H
#define IDNNODE_H

#include "./HWBridge.h"
#include "./types.h"
#include "./DACHWInterface.h"
#include "./BEX.h"
#include <string>


class IDNNode {
public:
	IDNNode(std::shared_ptr<HWBridge> driver);
	~IDNNode();
	int processIDNPacket(char* buf, unsigned int len, int sd, struct sockaddr *remote, unsigned int addr_len);
	void* networkThreadStart();
	unsigned int buildDictionary(char* buf, unsigned int len, unsigned int offset, uint8_t scwc, IDNDescriptorTag** data);
	void mainNetLoop(int sd);
	void setChunkLengthUs(double us);
	std::string hostname = "OpenIDN";

private:
	void addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata);
	void commitChunk(unsigned int deviceNumber = 0);
	void resetChunkBuffer();
	std::shared_ptr<HWBridge> driverPtr = nullptr;
	IDNChannel* channel;
	unsigned char mac_address[6];
	double usPerSlice = 5000;
	double currentSliceTime = usPerSlice;
	unsigned convertedChunks = 0;
	std::vector<ISPDB25Point> currentSlicePoints;
	double skipCounter = 0;
};




#endif /* IDNNODE_H */
