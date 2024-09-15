#include "../DACHWInterface.hpp"

#include <stdint.h>
#include <unistd.h>

#include "../../types.h"

class DummyAdapter : public DACHWInterface {
public:
	int writeFrame(const TimeSlice& slice, double duration) override;
	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;
	void getName(char *nameBufferPtr, unsigned nameBufferSize) override;

	DummyAdapter();
	~DummyAdapter();

private:
	void writeDACPoint(const unsigned char* point);
	unsigned maximumPointRate;
};

