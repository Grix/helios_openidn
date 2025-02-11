#include "./BEX.hpp"

BEX::BEX() {
	hotBuf = new SliceBuf;
	atomicPtr.store(new SliceBuf);
	mode = DRIVER_INACTIVE;
}

void BEX::resetBuffers() {

	hotBuf->clear();
	atomicPtr.store(new SliceBuf);
}

void BEX::setMode(int mode) {

	if(mode != this->mode) {
		resetBuffers();
	}

	this->mode = mode;
}

unsigned BEX::getMode() {
	return this->mode;
}

//a function that atomically swaps buffers back and forth,
//alternatingly making a buffer visible to the driver
void BEX::networkAppendSlice(std::shared_ptr<TimeSlice> slice) {
	if(mode == DRIVER_INACTIVE)
		return;

	std::lock_guard<std::mutex> lock(threadLock);

/*#ifndef NDEBUG
	printf("Put buffer %p\n", slice.get());
#endif*/

	//store slice in current hotBuf
	hotBuf->push_back(slice);
	
	//only immediately publish in wave mode
	if(mode == DRIVER_WAVEMODE) {
		//here prevAdvertised is either the buffer that was
		//put there in the previous iteration or
		//a nullptr.
		hotBuf = atomicPtr.exchange(hotBuf);

		if(hotBuf == nullptr) {

/*#ifndef NDEBUG
			printf("Buffer was null after swapping\n");
#endif*/

			//here the previous buffer was accepted by
			//the driver and be just made the old
			//version with a new slice public. therefore we
			//need to replace the outdated version with
			//a new buffer with just the new slice.
			hotBuf = new SliceBuf;
			hotBuf->push_back(slice);

			//there is a small chance that the driver picks
			//up the outdated buffer in the delay between the
			//earlier exchange and now. either way, that buffer
			//is cleared (if not null) and not persued any further.
			//this is an extra exchange buf the cost is carried
			//by the network thread.
			publishReset();
		
		}

		//either way we end up with a hotBuf that does not
		//contain the current element, so we need to add it again
		hotBuf->push_back(slice);
	}


}

//publish the buffer but don't use the exchanged
//buffer as a mirror (as is done in wavemode) so
//throw the contained data away, if there is any
void BEX::publishReset() 
{
	hotBuf = atomicPtr.exchange(hotBuf);

	if(hotBuf == nullptr) {
		hotBuf = new SliceBuf;
	} else {
		hotBuf->clear();
	}
}


//the driver requests a buffer swap
//atomically getting the stored buffer
//ptr and indicating that this buffer
//is taken by swapping in a nullptr
std::shared_ptr<SliceBuf> BEX::driverSwapRequest() 
{
	std::lock_guard<std::mutex> lock(threadLock);
	return std::shared_ptr<SliceBuf>(atomicPtr.exchange(nullptr));
}

bool BEX::hasSliceInQueue()
{
	std::lock_guard<std::mutex> lock(threadLock);
	return !(hotBuf == nullptr || hotBuf->empty());
}
