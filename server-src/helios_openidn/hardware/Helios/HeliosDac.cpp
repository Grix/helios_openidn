/*
SDK for Helios Laser DAC class
By Gitle Mikkelsen
gitlem@gmail.com
MIT License

Dependencies:
Libusb 1.0 (GNU Lesser General Public License, see libusb.h)

Standard: C++14
git repo: https://github.com/Grix/helios_dac.git

See header HeliosDac.h for function and usage documentation
*/


#include "HeliosDac.hpp"
#include "../../shared/types.h"

void logInfo(const char* fmt, ...)
{
#ifdef DEBUGOUTPUT
	va_list arg_ptr;
	va_start(arg_ptr, fmt);
	vprintf(fmt, arg_ptr);
#endif
}

int plt_monoValid = 0;
struct timespec plt_monoRef = { 0 };
uint64_t plt_monoTimeUS = 0;

inline static uint64_t plt_getMonoTimeUS()
{
	// Get current time
	struct timespec tsNow, tsDiff;
#ifdef __APPLE__
	mach_clock_gettime(SYSTEM_CLOCK, &tsNow);
#else
	clock_gettime(CLOCK_MONOTONIC, &tsNow);
#endif

	if (plt_monoRef.tv_sec == 0 && plt_monoRef.tv_nsec == 0)
		plt_monoRef = tsNow;

	// Determine difference to reference time
	if (tsNow.tv_nsec < plt_monoRef.tv_nsec)
	{
		tsDiff.tv_sec = (tsNow.tv_sec - plt_monoRef.tv_sec) - 1;
		tsDiff.tv_nsec = (1000000000 + tsNow.tv_nsec) - plt_monoRef.tv_nsec;
	}
	else
	{
		tsDiff.tv_sec = tsNow.tv_sec - plt_monoRef.tv_sec;
		tsDiff.tv_nsec = tsNow.tv_nsec - plt_monoRef.tv_nsec;
	}

	// Update internal time and system time reference
	plt_monoTimeUS = (uint64_t)((tsDiff.tv_sec * 1000000) + (tsDiff.tv_nsec / 1000));

	return plt_monoTimeUS;
}

HeliosDac::HeliosDac()
{
	inited = false;
}

HeliosDac::~HeliosDac()
{
	CloseDevices();
}

int HeliosDac::OpenDevices()
{
	if (inited)
		return (int)deviceList.size();

	unsigned int numDevices = _OpenUsbDevices(false);

	//_SortDeviceList();

	if (numDevices > 0)
		inited = true;

	return (int)deviceList.size();
}


int HeliosDac::OpenDevicesOnlyUsb()
{
	if (inited)
		return (int)deviceList.size();

	unsigned int numDevices = _OpenUsbDevices(false);

	//_SortDeviceList();

	if (numDevices > 0)
		inited = true;

	return (int)deviceList.size();
}


int HeliosDac::ReScanDevices()
{
	_OpenUsbDevices(true);

	inited = true;

	return (int)deviceList.size();
}

int HeliosDac::ReScanDevicesOnlyUsb()
{
	_OpenUsbDevices(true);

	inited = true;

	return (int)deviceList.size();
}

// Internal function. inPlace = Whether to keep the current opened devices in their device number slots and only scan for changes.
int HeliosDac::_OpenUsbDevices(bool inPlace)
{
	// Scanning for USB devices

	if (!inPlace || !inited)
	{
		int result = libusb_init(NULL);
		if (result < 0)
			return result;

		libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL);
	}

	if (inPlace && inited)
	{
		// Verify connection of existing devices, close them if they cannot be reached
		for (int i = 0; i < deviceList.size(); i++)
		{
			if (!deviceList[i]->GetIsUsb() || deviceList[i]->GetIsClosed())
				continue;

			if (deviceList[i]->GetDidSendFrameRecently())
				continue;

			if (deviceList[i]->GetStatus() < 0)
			{
				deviceList[i]->Close();
			}
		}
	}

	libusb_device** devs;
	ssize_t cnt = libusb_get_device_list(NULL, &devs);
	if (cnt < 0)
		return (int)cnt;

	unsigned int numDevices = 0;
	for (int i = 0; i < cnt; i++)
	{
		struct libusb_device_descriptor devDesc;
		int result = libusb_get_device_descriptor(devs[i], &devDesc);
		if (result < 0)
			continue;

		if ((devDesc.idProduct != HELIOS_PID) || (devDesc.idVendor != HELIOS_VID))
			continue;

		if (inPlace)
		{
			// Check if this is the same as an already opened device
			bool found = false;
			for (int j = 0; j < deviceList.size(); j++)
			{
				if (!deviceList[j]->GetIsUsb())
					continue;
				if (!deviceList[j]->GetIsClosed())
				{
					libusb_device_handle* handle = ((HeliosDacUsbDevice*)(deviceList[j].get()))->GetLibusbHandle();
					if (handle == NULL)
						continue;
					uint8_t newPortNumbers[7], existingPortNumbers[7];
					int existingPortNumberDepth = libusb_get_port_numbers(libusb_get_device(handle), existingPortNumbers, 16);
					int newPortNumberDepth = libusb_get_port_numbers(devs[i], newPortNumbers, 16);
					if (existingPortNumberDepth < 0)
						continue;
					if (newPortNumberDepth < 0)
						continue;
					bool match = true;
					if (existingPortNumberDepth == newPortNumberDepth)
					{
						for (int k = 0; k < existingPortNumberDepth; k++)
						{
							if (newPortNumbers[k] != existingPortNumbers[k])
							{
								match = false;
								break;
							}
						}
					}
					if (match)
					{
						found = true;
						break;
					}
				}
				if (found)
					break;
			}

			if (found)
				continue; // This Helios is already opened, no need to do anything
		}

		libusb_device_handle* devHandle;
		result = libusb_open(devs[i], &devHandle);
		if (result < 0)
			continue;

		result = libusb_claim_interface(devHandle, 0);
		if (result < 0)
			continue;

		result = libusb_set_interface_alt_setting(devHandle, 0, 1);
		if (result < 0)
			continue;

		// Successfully opened, add to device list

		std::unique_ptr<HeliosDacDevice> device = std::make_unique<HeliosDacUsbDevice>(devHandle);

		if (inPlace)
		{
			// Check if this is an already known device that was previously closed, if so insert it back to its previous device number
			bool found = false;
			for (int j = 0; j < deviceList.size(); j++)
			{
				if (!deviceList[j]->GetIsUsb())
					continue;
				if (!deviceList[j]->GetIsClosed())
					continue;
				char newName[32] = { 0 }, existingName[32] = { 0 };
				device->GetName(newName);
				deviceList[j]->GetName(existingName);
				if (strncmp(newName, existingName, 32) == 0)
				{
					logInfo("Found previous USB Helios DAC that was reconnected: %s\n", newName);

					deviceList[j] = std::move(device);
					found = true;
					break;
				}
			}
			if (!found)
			{
				//char name[32] = { 0 };
				//device->GetName(name);
				logInfo("Found new USB Helios DAC during rescan\n");

				deviceList.push_back(std::move(device));
				numDevices++;
			}
		}
		else
		{
			//char name[32] = { 0 };
			//device->GetName(name);
			logInfo("Found new USB Helios DAC\n");

			deviceList.push_back(std::move(device));
			numDevices++;
		}
	}

	libusb_free_device_list(devs, 1);

	return numDevices;
}


void HeliosDac::_SortDeviceList()
{
	int listSize = deviceList.size();
	for (int i = 0; i < listSize - 1; i++) // Bubble sort
	{
		for (int j = 0; j < listSize - i - 1; j++)
		{
			char name1[32];
			char name2[32];
			deviceList[j]->GetName(name1);
			deviceList[j + 1]->GetName(name2);
			if (strcmp(name1, name2) > 0)
				swap(deviceList[j], deviceList[j + 1]);
		}
	}
}

int HeliosDac::CloseDevices()
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::lock_guard<std::mutex> lock(threadLock);
	inited = false;
	deviceList.clear(); // Various destructors will clean all devices

	libusb_exit(NULL);

	printf("Freed USB Helios library\n");

	return HELIOS_SUCCESS;
}

int HeliosDac::WriteFrame(unsigned int devNum, unsigned int pps, std::uint8_t flags, HeliosPoint* points, unsigned int numOfPoints)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	if (points == NULL || numOfPoints == 0)
		return HELIOS_ERROR_NULL_POINTS;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();

	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->SendFrame(pps, flags, points, numOfPoints);
}

int HeliosDac::WriteFrameHighResolution(unsigned int devNum, unsigned int pps, unsigned int flags, HeliosPointHighRes* points, unsigned int numOfPoints)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	if (points == NULL || numOfPoints == 0)
		return HELIOS_ERROR_NULL_POINTS;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();

	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->SendFrameHighResolution(pps, flags, points, numOfPoints);
}

int HeliosDac::WriteFrameExtended(unsigned int devNum, unsigned int pps, unsigned int flags, HeliosPointExt* points, unsigned int numOfPoints)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	if (points == NULL || numOfPoints == 0)
		return HELIOS_ERROR_NULL_POINTS;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();

	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->SendFrameExtended(pps, flags, points, numOfPoints);
}

int HeliosDac::GetIsClosed(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->GetIsClosed();
}

int HeliosDac::GetStatus(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->GetStatus();
}

int HeliosDac::GetFirmwareVersion(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->GetFirmwareVersion();
}

int HeliosDac::GetName(unsigned int devNum, char* name)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	char dacName[32];
	if (dev->GetName(dacName) < 0)
	{
		// The device didn't return a name so build a generic name
		memcpy(name, "Unknown Helios ", 16);
		name[15] = (char)((int)(devNum >= 10) + 48);
		name[16] = (char)((int)(devNum % 10) + 48);
		name[17] = '\0';
	}
	else
	{
		memcpy(name, dacName, 32);
	}
	return HELIOS_SUCCESS;
}

int HeliosDac::SetName(unsigned int devNum, char* name)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->SetName(name);
}

int HeliosDac::Stop(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->Stop();
}

int HeliosDac::SetShutter(unsigned int devNum, bool level)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->SetShutter(level);
}

int HeliosDac::GetSupportsHigherResolutions(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();

	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->GetSupportsHigherResolutions();
}

int HeliosDac::GetIsUsb(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();

	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->GetIsUsb();
}

int HeliosDac::SetLibusbDebugLogLevel(int logLevel)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	libusb_set_option(NULL, LIBUSB_OPTION_LOG_LEVEL, logLevel);

	return HELIOS_SUCCESS;
}

int HeliosDac::EraseFirmware(unsigned int devNum)
{
	if (!inited)
		return HELIOS_ERROR_NOT_INITIALIZED;

	std::unique_lock<std::mutex> lock(threadLock);
	HeliosDacDevice* dev = NULL;
	if (devNum < deviceList.size())
		dev = deviceList[devNum].get();
	lock.unlock();
	if (dev == NULL)
		return HELIOS_ERROR_INVALID_DEVNUM;

	return dev->EraseFirmware();
}


/// -----------------------------------------------------------------------
/// HeliosDacUsbDevice START (one instance for each connected USB DAC)
/// -----------------------------------------------------------------------

HeliosDac::HeliosDacUsbDevice::HeliosDacUsbDevice(libusb_device_handle* handle)
{
	closed = true;
	usbHandle = handle;
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	std::lock_guard<std::mutex>lock(frameLock);

	int actualLength = 0;
	for (int i = 0; i < 32; i++)
		name[i] = 0;

	//catch any lingering transfers
	std::uint8_t ctrlBuffer0[32];
	while (libusb_interrupt_transfer(usbHandle, EP_INT_IN, ctrlBuffer0, 32, &actualLength, 5) == LIBUSB_SUCCESS);

	//get firmware version
	firmwareVersion = 0;
	bool repeat = true;
	for (int i = 0; ((i < 2) && repeat); i++) //retry command if necessary
	{
		std::uint8_t ctrlBuffer[2] = { 0x04, 0 };
		int transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_OUT, ctrlBuffer, 2, &actualLength, 32);
		if ((transferResult == LIBUSB_SUCCESS) && (actualLength == 2))
		{
			for (int j = 0; ((j < 3) && repeat); j++) //retry response getting if necessary
			{
				std::uint8_t ctrlBuffer2[32];
				transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_IN, ctrlBuffer2, 32, &actualLength, 32);
				if (transferResult == LIBUSB_SUCCESS)
				{
					if (ctrlBuffer2[0] == 0x84)
					{
						firmwareVersion = ((ctrlBuffer2[1] << 0) |
							(ctrlBuffer2[2] << 8) |
							(ctrlBuffer2[3] << 16) |
							(ctrlBuffer2[4] << 24));
						repeat = false;
					}
				}
			}
		}
	}


	//send sdk firmware version
	repeat = true;
	for (int i = 0; ((i < 2) && repeat); i++) //retry command if necessary
	{
		std::uint8_t ctrlBuffer3[2] = { 0x07, HELIOS_SDK_VERSION };
		int transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_OUT, ctrlBuffer3, 2, &actualLength, 32);
		if ((transferResult == LIBUSB_SUCCESS) && (actualLength == 2))
			repeat = false;
	}

	frameBuffer = new std::uint8_t[HELIOS_MAX_POINTS * 7 + 5];
	frameBufferSize = 0;

	closed = false;
}

// Sends a raw frame buffer (implemented as bulk transfer) to a dac device
// Returns 1 if success
int HeliosDac::HeliosDacUsbDevice::SendFrame(unsigned int pps, std::uint8_t flags, HeliosPoint* points, unsigned int numOfPoints)
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	if (frameReady)
		return HELIOS_ERROR_DEVICE_FRAME_READY;

	// If pps is too low, try to duplicate frames to simulate a higher pps rather than failing
	bool freePoints = false;
	if (pps < GetMinSampleRate())
	{
		if (pps == 0)
			return HELIOS_ERROR_PPS_TOO_LOW;

		unsigned int samplingFactor = GetMinSampleRate() / pps + 1;
		if (numOfPoints * samplingFactor > GetMaxFrameSize())
			return HELIOS_ERROR_PPS_TOO_LOW;

		HeliosPoint* duplicatedPoints = new HeliosPoint[numOfPoints * samplingFactor];
		freePoints = true;

		unsigned int adjustedBufferPos = 0;
		for (unsigned int i = 0; i < numOfPoints; i++)
		{
			for (unsigned int j = 0; j < samplingFactor; j++)
			{
				duplicatedPoints[adjustedBufferPos++] = points[i];
			}
		}
		numOfPoints = numOfPoints * samplingFactor;
		pps = pps * samplingFactor;
		points = duplicatedPoints;
	}

	// If pps is too high or frame size too high, subsample frames to simulate a lower pps/size rather than failing
	unsigned int samplingFactor = 1;
	if (pps > GetMaxSampleRate() || numOfPoints > GetMaxFrameSize())
	{
		samplingFactor = pps / GetMaxSampleRate() + 1;
		if ((numOfPoints / GetMaxFrameSize() + 1) > samplingFactor)
			samplingFactor = numOfPoints / GetMaxFrameSize() + 1;

		pps = pps / samplingFactor;
		numOfPoints = numOfPoints / samplingFactor;

		if (pps < GetMinSampleRate())
		{
			if (freePoints)
				delete[] points;
			return HELIOS_ERROR_TOO_MANY_POINTS;
		}
	}

	// This is a bug workaround, the mcu won't correctly receive transfers with these sizes
	unsigned int ppsActual = pps;
	unsigned int numOfPointsActual = numOfPoints;
	if ((((int)numOfPoints - 45) % 64) == 0)
	{
		numOfPointsActual--;
		//adjust pps to keep the same frame duration even with one less point
		ppsActual = (unsigned int)((pps * (double)numOfPointsActual / (double)numOfPoints) + 0.5);
	}

	unsigned int bufPos = 0;

	// Prepare frame buffer
	unsigned int loopLength = numOfPointsActual * samplingFactor;
	for (unsigned int i = 0; i < loopLength; i += samplingFactor)
	{
		frameBuffer[bufPos++] = (points[i].x >> 4);
		frameBuffer[bufPos++] = ((points[i].x & 0x0F) << 4) | (points[i].y >> 8);
		frameBuffer[bufPos++] = (points[i].y & 0xFF);
		frameBuffer[bufPos++] = points[i].r;
		frameBuffer[bufPos++] = points[i].g;
		frameBuffer[bufPos++] = points[i].b;
		frameBuffer[bufPos++] = points[i].i;
	}
	frameBuffer[bufPos++] = (ppsActual & 0xFF);
	frameBuffer[bufPos++] = (ppsActual >> 8);
	frameBuffer[bufPos++] = (numOfPointsActual & 0xFF);
	frameBuffer[bufPos++] = (numOfPointsActual >> 8);
	frameBuffer[bufPos++] = flags;

	frameBufferSize = bufPos;

	if (freePoints)
		delete[] points;

	lastSendTime = plt_getMonoTimeUS();

	if (!shutterIsOpen)
		SetShutter(1);

	if ((flags & HELIOS_FLAGS_DONT_BLOCK) != 0)
	{
		threadingHasBeenUsed = true;
		frameReady = true;
		return HELIOS_SUCCESS;
	}
	else
	{
		return DoFrame();
	}
}

// Sends a raw frame buffer (implemented as bulk transfer) to a dac device, using high-res point structure (but simply convert down to low-res)
// Returns 1 if success
int HeliosDac::HeliosDacUsbDevice::SendFrameHighResolution(unsigned int pps, std::uint8_t flags, HeliosPointHighRes* points, unsigned int numOfPoints)
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	if (frameReady)
		return HELIOS_ERROR_DEVICE_FRAME_READY;

	// If pps is too low, try to duplicate frames to simulate a higher pps rather than failing
	bool freePoints = false;
	if (pps < GetMinSampleRate())
	{
		if (pps == 0)
			return HELIOS_ERROR_PPS_TOO_LOW;

		unsigned int samplingFactor = GetMinSampleRate() / pps + 1;
		if (numOfPoints * samplingFactor > GetMaxFrameSize())
			return HELIOS_ERROR_PPS_TOO_LOW;

		HeliosPointHighRes* duplicatedPoints = new HeliosPointHighRes[numOfPoints * samplingFactor];
		freePoints = true;

		unsigned int adjustedBufferPos = 0;
		for (unsigned int i = 0; i < numOfPoints; i++)
		{
			for (unsigned int j = 0; j < samplingFactor; j++)
			{
				duplicatedPoints[adjustedBufferPos++] = points[i];
			}
		}
		numOfPoints = numOfPoints * samplingFactor;
		pps = pps * samplingFactor;
		points = duplicatedPoints;
	}

	// If pps is too high or frame size too high, subsample frames to simulate a lower pps/size rather than failing
	unsigned int samplingFactor = 1;
	if (pps > GetMaxSampleRate() || numOfPoints > GetMaxFrameSize())
	{
		samplingFactor = pps / GetMaxSampleRate() + 1;
		if ((numOfPoints / GetMaxFrameSize() + 1) > samplingFactor)
			samplingFactor = numOfPoints / GetMaxFrameSize() + 1;

		pps = pps / samplingFactor;
		numOfPoints = numOfPoints / samplingFactor;

		if (pps < GetMinSampleRate())
		{
			if (freePoints)
				delete[] points;
			return HELIOS_ERROR_TOO_MANY_POINTS;
		}
	}


	// This is a bug workaround, the mcu won't correctly receive transfers with these sizes
	unsigned int ppsActual = pps;
	unsigned int numOfPointsActual = numOfPoints;
	if ((((int)numOfPoints - 45) % 64) == 0)
	{
		numOfPointsActual--;
		// Adjust pps to keep the same frame duration even with one less point
		ppsActual = (unsigned int)((pps * (double)numOfPointsActual / (double)numOfPoints) + 0.5);
	}

	unsigned int bufPos = 0;

	// Prepare frame buffer
	// Converting to non-high-res for unsupported models
	unsigned int loopLength = numOfPointsActual * samplingFactor;
	for (unsigned int i = 0; i < loopLength; i += samplingFactor)
	{
		std::uint16_t x = points[i].x >> 4;
		std::uint16_t y = points[i].y >> 4;
		frameBuffer[bufPos++] = (x >> 4);
		frameBuffer[bufPos++] = ((x & 0x0F) << 4) | (y >> 8);
		frameBuffer[bufPos++] = (y & 0xFF);
		frameBuffer[bufPos++] = points[i].r >> 8;
		frameBuffer[bufPos++] = points[i].g >> 8;
		frameBuffer[bufPos++] = points[i].b >> 8;
		frameBuffer[bufPos++] = 0xFF;
	}
	frameBuffer[bufPos++] = (ppsActual & 0xFF);
	frameBuffer[bufPos++] = (ppsActual >> 8);
	frameBuffer[bufPos++] = (numOfPointsActual & 0xFF);
	frameBuffer[bufPos++] = (numOfPointsActual >> 8);
	frameBuffer[bufPos++] = flags;

	frameBufferSize = bufPos;

	if (freePoints)
		delete[] points;

	lastSendTime = plt_getMonoTimeUS();

	if (!shutterIsOpen)
		SetShutter(1);

	if ((flags & HELIOS_FLAGS_DONT_BLOCK) != 0)
	{
		threadingHasBeenUsed = true;
		frameReady = true;
		return HELIOS_SUCCESS;
	}
	else
	{
		return DoFrame();
	}
}
// Sends a raw frame buffer (implemented as bulk transfer) to a dac device, using extended point structure (but simply convert down to low-res)
// Returns 1 if success
int HeliosDac::HeliosDacUsbDevice::SendFrameExtended(unsigned int pps, std::uint8_t flags, HeliosPointExt* points, unsigned int numOfPoints)
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	if (frameReady)
		return HELIOS_ERROR_DEVICE_FRAME_READY;

	// If pps is too low, try to duplicate frames to simulate a higher pps rather than failing
	bool freePoints = false;
	if (pps < GetMinSampleRate())
	{
		if (pps == 0)
			return HELIOS_ERROR_PPS_TOO_LOW;

		unsigned int samplingFactor = GetMinSampleRate() / pps + 1;
		if (numOfPoints * samplingFactor > GetMaxFrameSize())
			return HELIOS_ERROR_PPS_TOO_LOW;

		HeliosPointExt* duplicatedPoints = new HeliosPointExt[numOfPoints * samplingFactor];
		freePoints = true;

		unsigned int adjustedBufferPos = 0;
		for (unsigned int i = 0; i < numOfPoints; i++)
		{
			for (unsigned int j = 0; j < samplingFactor; j++)
			{
				duplicatedPoints[adjustedBufferPos++] = points[i];
			}
		}
		numOfPoints = numOfPoints * samplingFactor;
		pps = pps * samplingFactor;
		points = duplicatedPoints;
	}

	// If pps is too high or frame size too high, subsample frames to simulate a lower pps/size rather than failing
	unsigned int samplingFactor = 1;
	if (pps > GetMaxSampleRate() || numOfPoints > GetMaxFrameSize())
	{
		samplingFactor = pps / GetMaxSampleRate() + 1;
		if ((numOfPoints / GetMaxFrameSize() + 1) > samplingFactor)
			samplingFactor = numOfPoints / GetMaxFrameSize() + 1;

		pps = pps / samplingFactor;
		numOfPoints = numOfPoints / samplingFactor;

		if (pps < GetMinSampleRate())
		{
			if (freePoints)
				delete[] points;
			return HELIOS_ERROR_TOO_MANY_POINTS;
		}
	}


	// This is a bug workaround, the mcu won't correctly receive transfers with these sizes
	unsigned int ppsActual = pps;
	unsigned int numOfPointsActual = numOfPoints;
	if ((((int)numOfPoints - 45) % 64) == 0)
	{
		numOfPointsActual--;
		// Adjust pps to keep the same frame duration even with one less point
		ppsActual = (unsigned int)((pps * (double)numOfPointsActual / (double)numOfPoints) + 0.5);
	}

	unsigned int bufPos = 0;

	// Prepare frame buffer
	// Converting to non-high-res for unsupported models
	unsigned int loopLength = numOfPointsActual * samplingFactor;
	for (unsigned int i = 0; i < loopLength; i += samplingFactor)
	{
		std::uint16_t x = points[i].x >> 4;
		std::uint16_t y = points[i].y >> 4;
		frameBuffer[bufPos++] = (x >> 4);
		frameBuffer[bufPos++] = ((x & 0x0F) << 4) | (y >> 8);
		frameBuffer[bufPos++] = (y & 0xFF);
		frameBuffer[bufPos++] = points[i].r >> 8;
		frameBuffer[bufPos++] = points[i].g >> 8;
		frameBuffer[bufPos++] = points[i].b >> 8;
		frameBuffer[bufPos++] = points[i].i >> 8;
	}
	frameBuffer[bufPos++] = (ppsActual & 0xFF);
	frameBuffer[bufPos++] = (ppsActual >> 8);
	frameBuffer[bufPos++] = (numOfPointsActual & 0xFF);
	frameBuffer[bufPos++] = (numOfPointsActual >> 8);
	frameBuffer[bufPos++] = flags;

	frameBufferSize = bufPos;

	if (freePoints)
		delete[] points;

	lastSendTime = plt_getMonoTimeUS();

	if (!shutterIsOpen)
		SetShutter(1);

	if ((flags & HELIOS_FLAGS_DONT_BLOCK) != 0)
	{
		threadingHasBeenUsed = true;
		frameReady = true;
		return HELIOS_SUCCESS;
	}
	else
	{
		return DoFrame();
	}
}

// Sends frame to DAC
int HeliosDac::HeliosDacUsbDevice::DoFrame()
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	int actualLength = 0;

	//auto then = std::chrono::high_resolution_clock::now();

	int transferResult = libusb_bulk_transfer(usbHandle, EP_BULK_OUT, frameBuffer, frameBufferSize, &actualLength, 8 + (frameBufferSize >> 5));

	//auto now = std::chrono::high_resolution_clock::now();
	//auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now - then);
	//printf("%d : Sent bulk frame, %d, time %d\n", std::chrono::duration_cast<std::chrono::milliseconds>(then.time_since_epoch()), transferResult, time.count());

	if (transferResult == LIBUSB_SUCCESS)
		return HELIOS_SUCCESS;
	else
		return HELIOS_ERROR_LIBUSB_BASE + transferResult;
}

//Gets firmware version of DAC
int HeliosDac::HeliosDacUsbDevice::GetFirmwareVersion()
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	return firmwareVersion;
}

//Gets name of DAC
int HeliosDac::HeliosDacUsbDevice::GetName(char* dacName)
{
	if (GetIsClosed())
	{
		memcpy(dacName, name, 32);
		return HELIOS_ERROR_DEVICE_CLOSED;
	}

	int errorCode;

	std::lock_guard<std::mutex> lock(frameLock);

	for (int i = 0; i < 2; i++) //retry command if necessary
	{
		int actualLength = 0;
		std::uint8_t ctrlBuffer4[2] = { 0x05, 0 };
		if (SendControl(ctrlBuffer4, 2) == HELIOS_SUCCESS)
		{
			std::uint8_t ctrlBuffer5[32];
			int transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_IN, ctrlBuffer5, sizeof(ctrlBuffer5), &actualLength, 32);

			if (transferResult == LIBUSB_SUCCESS)
			{
				if (ctrlBuffer5[0] == 0x85)
				{
					ctrlBuffer5[sizeof(ctrlBuffer5) - 1] = 0; // Just in case
					memcpy(name, &ctrlBuffer5[1], sizeof(ctrlBuffer5) - 2);
					memcpy(dacName, &ctrlBuffer5[1], sizeof(ctrlBuffer5) - 2);
					return HELIOS_SUCCESS;
				}
				else
				{
					errorCode = HELIOS_ERROR_DEVICE_RESULT;
				}
			}
			else
			{
				errorCode = HELIOS_ERROR_LIBUSB_BASE + transferResult;
			}
		}
		else
		{
			errorCode = HELIOS_ERROR_DEVICE_SEND_CONTROL;
		}
	}

	return errorCode;
}

// Gets status of DAC, 1 means DAC is ready to receive frame, 0 means it's not
int HeliosDac::HeliosDacUsbDevice::GetStatus()
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	int errorCode;

	std::lock_guard<std::mutex> lock(frameLock);

	//auto then = std::chrono::high_resolution_clock::now();

	int actualLength = 0;
	std::uint8_t ctrlBuffer[2] = { 0x03, 0 };
	if (SendControl(ctrlBuffer, 2) == HELIOS_SUCCESS)
	{
		std::uint8_t ctrlBuffer2[32];
		int transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_IN, ctrlBuffer2, 32, &actualLength, 16);

		//auto now = std::chrono::high_resolution_clock::now();
		//auto time = std::chrono::duration_cast<std::chrono::milliseconds>(now - then);
		//printf("%d : Sent status req, %d, time %d, status %d\n", std::chrono::duration_cast<std::chrono::milliseconds>(then.time_since_epoch()), transferResult, time.count(), ctrlBuffer2[1]);


		if (transferResult == LIBUSB_SUCCESS)
		{
			if (ctrlBuffer2[0] == 0x83) //STATUS ID
			{
				if (ctrlBuffer2[1] == 0)
					return 0;
				else
					return 1;
			}
			else
			{
				errorCode = HELIOS_ERROR_DEVICE_RESULT;
			}
		}
		else
		{
			errorCode = HELIOS_ERROR_LIBUSB_BASE + transferResult;
			errorLimitCountdown--;
			if (errorLimitCountdown <= 0)
			{
				printf("Closing Helios DAC: too many errors.\n");
				closed = true;
			}
		}
	}
	else
	{
		errorCode = HELIOS_ERROR_DEVICE_SEND_CONTROL;
		errorLimitCountdown--;
		if (errorLimitCountdown <= 0)
		{
			printf("Closing Helios DAC: too many errors.\n");
			closed = true;
		}
	}

	return errorCode;
}

//Set shutter level of DAC
//Value 1 = shutter open, value 0 = shutter closed
int HeliosDac::HeliosDacUsbDevice::SetShutter(bool level)
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	std::lock_guard<std::mutex> lock(frameLock);

	std::uint8_t txBuffer[2] = { 0x02, level };
	if (SendControl(txBuffer, 2) == HELIOS_SUCCESS)
	{
		shutterIsOpen = level;
		return HELIOS_SUCCESS;
	}
	else
		return HELIOS_ERROR_DEVICE_SEND_CONTROL;
}

//Stops output of DAC
int HeliosDac::HeliosDacUsbDevice::Stop()
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	std::lock_guard<std::mutex> lock(frameLock);

	std::uint8_t txBuffer[2] = { 0x01, 0 };
	if (SendControl(txBuffer, 2) == HELIOS_SUCCESS)
	{
		struct timespec delay, dummy;
		delay.tv_sec = 0;
		delay.tv_nsec = 500000; // 500 us
		nanosleep(&delay, &dummy);
		return HELIOS_SUCCESS;
	}
	else
		return HELIOS_ERROR_DEVICE_SEND_CONTROL;
}

int HeliosDac::HeliosDacUsbDevice::Close()
{
	Stop();
	logInfo("Closing Helios USB DAC.\n");
	closed = true;
	return HELIOS_SUCCESS;
}

//Sets persistent name of DAC
int HeliosDac::HeliosDacUsbDevice::SetName(char* name)
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	std::lock_guard<std::mutex> lock(frameLock);

	std::uint8_t txBuffer[32] = { 0x06 };
	memcpy(&txBuffer[1], name, 30);
	txBuffer[31] = '\0';
	if (SendControl(txBuffer, 32) == HELIOS_SUCCESS)
		return HELIOS_SUCCESS;
	else
		return HELIOS_ERROR_DEVICE_SEND_CONTROL;
}

//Erases the firmware of the DAC, allowing it to be updated
int HeliosDac::HeliosDacUsbDevice::EraseFirmware()
{
	if (GetIsClosed())
		return HELIOS_ERROR_DEVICE_CLOSED;

	std::lock_guard<std::mutex> lock(frameLock);

	std::uint8_t txBuffer[2] = { 0xDE, 0 };
	if (SendControl(txBuffer, 2) == HELIOS_SUCCESS)
	{
		closed = true;
		return HELIOS_SUCCESS;
	}
	else
		return HELIOS_ERROR_DEVICE_SEND_CONTROL;
}

bool HeliosDac::HeliosDacUsbDevice::GetDidSendFrameRecently()
{
	if (GetIsClosed())
		return false;
	return plt_getMonoTimeUS() - lastSendTime < 500000; // less than 500 ms since last frame send
}

libusb_device_handle* HeliosDac::HeliosDacUsbDevice::GetLibusbHandle()
{
	if (GetIsClosed())
		return NULL;

	return usbHandle;
}

//sends a raw control signal (implemented as interrupt transfer) to a dac device
//returns 1 if successful
int HeliosDac::HeliosDacUsbDevice::SendControl(std::uint8_t* bufferAddress, unsigned int length)
{
	if (bufferAddress == NULL)
		return HELIOS_ERROR_DEVICE_NULL_BUFFER;

	if (length > 32)
		return HELIOS_ERROR_DEVICE_SIGNAL_TOO_LONG;

	//auto then = std::chrono::high_resolution_clock::now();

	int actualLength = 0;
	int transferResult = libusb_interrupt_transfer(usbHandle, EP_INT_OUT, bufferAddress, length, &actualLength, 16);

	if (transferResult == LIBUSB_SUCCESS)
		return HELIOS_SUCCESS;
	else
		return HELIOS_ERROR_LIBUSB_BASE + transferResult;
}

HeliosDac::HeliosDacUsbDevice::~HeliosDacUsbDevice()
{
	closed = true;
	std::lock_guard<std::mutex>lock(frameLock); //wait until all threads have closed

	libusb_close(usbHandle);
	delete frameBuffer;
}

