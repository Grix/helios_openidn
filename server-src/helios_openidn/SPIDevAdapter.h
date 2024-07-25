#include "./DACHWInterface.h"

#include <stdint.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "./types.h"

class SPIDevAdapter : public DACHWInterface {
public:
	int writeFrame(const TimeSlice& slice, double duration) override;
	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;

	SPIDevAdapter();
	SPIDevAdapter(unsigned);
	~SPIDevAdapter();

private:
	int spidevFd;
	unsigned maximumPointrate;
};

