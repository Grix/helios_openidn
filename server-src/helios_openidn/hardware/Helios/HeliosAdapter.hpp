#include "../../shared/DACHWInterface.hpp"

#include "./HeliosDac.hpp"
#include "../../shared/types.h"

#include <map>

class HeliosAdapter : public DACHWInterface {
public:
 	static void initialize();
    static std::map<std::string, int> getAvailableDevices();
    static void shutdown();

	SliceType convertPoints(const std::vector<ISPDB25Point>& points) override;
	unsigned bytesPerPoint() override;
	unsigned maxBytesPerTransmission() override;

	int writeFrame(const TimeSlice& slice, double duration) override;
	unsigned maxPointrate() override;
	void setMaxPointrate(unsigned) override;
	void getName(char *nameBufferPtr, unsigned nameBufferSize) override;
	bool getHeliosConnected();
	//bool getIsBusy() override;

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


	void checkConnection();
};

