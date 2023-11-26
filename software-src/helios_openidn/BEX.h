#ifndef BEX_H_
#define BEX_H_

#include <memory>
#include <atomic>
#include <deque>

#include "./types.h"

//Buffer EXchange. Here, buffers are moved from the network thread to the
//driver thread as smoothly as possible
class BEX {
public:
	//add new data to the network side of the buffer
	BEX();
	void networkAppendSlice(std::shared_ptr<TimeSlice> slice);
	void publishReset();
	void setMode(int mode);
	unsigned getMode();
	//get the currently advertised buffer
	std::shared_ptr<SliceBuf> driverSwapRequest();

private:
	//an atomic pointer to a list of TimeSlices
	//hotBuf: actively being changed
	SliceBuf* hotBuf;
	std::atomic<SliceBuf*> atomicPtr;
	void resetBuffers();
	int mode = DRIVER_INACTIVE;
};

#endif
