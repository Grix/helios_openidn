
#include "../../shared/DACHWInterface.hpp"
#include "../../shared/types.h"
#include <stdint.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <thread>
#include <cmath>

#define HELIOSPRO_CHUNKSIZE 0xE00

// These must match the MCU firmware!
#define HELIOSPRO_MCU_CLOCKSPEED ((uint32_t)96050000)
#define HELIOSPRO_MCU_TIMERSPEED (HELIOSPRO_MCU_CLOCKSPEED/2)
#define HELIOSPRO_MCU_MAXSPEED 100000u			// In pps
#define HELIOSPRO_MCU_MINSPEED 647u				// In pps

#define GPIO_DIR_IN(g)		*(gpio + (0x04 / 4)) &= ~(1 << (g & 0xFF))
#define GPIO_DIR_OUT(g)		*(gpio + (0x04 / 4)) |= (1 << (g & 0xFF))
#define GPIO_SET(g)		*(gpio + (0x00 / 4)) |= (1 << (g & 0xFF))
#define GPIO_CLR(g)		*(gpio + (0x00 / 4)) &= ~(1 << (g & 0xFF))
#define GPIO_LEV(g)		((*(gpio + (0x50 / 4)) >> (g & 0xFF)) & 1)
#define GPIOPIN_STATUS    13    // B5
#define GPIOPIN_RED_LED    8    // B0
#define GPIOPIN_GREEN_LED 11	// B3
#define GPIOPIN_ON_LED    12    // B4
#define GPIOPIN_MCURESET_LED    14    // B6

class HeliosProAdapter : public DACHWInterface {
public:
	int writeFrame(const TimeSlice& slice, double duration) override;
	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;
	bool getIsBusy() override;
	//void stop() override;

	HeliosProAdapter();
	~HeliosProAdapter();

private:
	int spidevFd;
	unsigned maximumPointrate;

	int	mem_fd;
	//void* spi2_ctrl_mem_map;
	//volatile uint32_t* spi2_ctrl;
	//void* spi2_tx_mem_map;
	//volatile uint32_t* spi2_tx;
	void* gpio_mem_map;
	volatile uint32_t* gpio;
	bool isBusy = false;
	uint8_t txId = 1;
	int txNum = 0;

	uint8_t writeBuffer[HELIOSPRO_CHUNKSIZE + 16 + 4];
};

