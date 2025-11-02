#pragma once

#include "../../shared/DACHWInterface.hpp"

#include "./HeliosDac.hpp"
#include "../../shared/types.h"

#include <map>
#include "../../server/IDNLaproService.hpp"

class HeliosAdapter : public DACHWInterface {
public:
 	static void initialize();
    static std::map<std::string, int> getAvailableDevices();
    static void shutdown();
	static void updateDeviceList();

	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;

	int writeFrame(const TimeSlice& slice, double duration) override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;
	void getName(char *nameBufferPtr, unsigned nameBufferSize) override;
	bool getHeliosConnected();
	//bool getIsBusy() override;

	//static int getFirstDeviceIndex() { return indexFirstDevice; }
	//static int getSecondDeviceIndex() { return indexSecondDevice; }
	static void setFirstDeviceService(IDNLaproService* service) { serviceFirstDevice = service; }
	static void setSecondDeviceService(IDNLaproService* service) { serviceSecondDevice = service; }
	static bool setFirstServiceIsAlwaysVisible() { firstServiceIsAlwaysVisible = true; }

	HeliosAdapter(int id);
	~HeliosAdapter();

private:
	int id;
	std::uint8_t heliosFlags;
	unsigned maximumPointRate;
	int connectionRetries = 50;

	static bool isInitialized;
	static HeliosDac helios;
	static int numHeliosDevices;
	bool isBusy = false;

	// Special two-device hot-plugging system
	static int indexFirstDevice;
	static int indexSecondDevice;
	static IDNLaproService* serviceFirstDevice;
	static IDNLaproService* serviceSecondDevice;
	static bool firstServiceIsAlwaysVisible;

	static void refreshDacReferences(unsigned int numDevices);
	void checkConnection();
};

