#ifndef DACADAPTER_H_
#define DACADAPTER_H_

#include "./types.h"

class DACHWInterface {
public:
	//writes byte data to a hardware interface
	virtual int writeFrame(const TimeSlice& slice, double duration) = 0;

	//converts ISPDB25 Points to bytes in a way that the hardware expects
	//points the outputBuf pointer to a vector and returns the total
	//number of bytes.
	virtual SliceType convertPoints(const std::vector<ISPDB25Point>& points) = 0;

	//size of converted points in bytes
	virtual unsigned bytesPerPoint() = 0;

	//maximum possible amount of bytes per transmission
	//without fragmentation
	virtual unsigned maxBytesPerTransmission() = 0;

	//maximum pointrate that the device if capable of
	//in pps
	virtual unsigned maxPointrate() = 0;
	virtual void setMaxPointrate(unsigned) = 0;

	//Human-facing identifier for the dac. 
	//Buffer must be at least 20 bytes long (like the IDN hostname)
	//If no name is implemented, an empty string is returned.
	virtual void getName(char* name)
	{
		strcpy(name, "");
	}

};

#endif
