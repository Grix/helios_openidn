#include "./DACHWInterface.h"

#include "./HeliosDac.h"
#include "./types.h"

class HeliosAdapter : public DACHWInterface {
public:
	int writeFrame(const TimeSlice& slice, double duration) override;
	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;

	HeliosAdapter();
	~HeliosAdapter();

private:
	HeliosDac helios;
	int numHeliosDevices;
	std::uint8_t heliosFlags;
	unsigned maximumPointRate;
};

