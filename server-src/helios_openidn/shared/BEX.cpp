#include "./BEX.hpp"

BEX::BEX() {
	hotBuf = new DB25ChunkQueue;
	atomicPtr.store(new DB25ChunkQueue);
	mode = DRIVER_INACTIVE;
}

void BEX::resetBuffers() {
	hotBuf->clear();
	atomicPtr.store(new DB25ChunkQueue);
}

bool BEX::hasBufferedFrame()
{
    std::lock_guard<std::mutex> lock(threadLock);
    DB25ChunkQueue* publishedBuffer = atomicPtr.load();
    return publishedBuffer != nullptr && !publishedBuffer->empty();
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
void BEX::networkAppendSlice(std::shared_ptr<DB25Chunk> db25Chunk) {
    if(mode == DRIVER_INACTIVE)
        return;

    std::lock_guard<std::mutex> lock(threadLock);

    //store chunk in current hotBuf
    hotBuf->push_back(db25Chunk);

    //only immediately publish in wave mode
    if(mode == DRIVER_WAVEMODE) {
        //here prevAdvertised is either the buffer that was
        //put there in the previous iteration or
        //a nullptr.
        hotBuf = atomicPtr.exchange(hotBuf);

        if(hotBuf == nullptr) {
            //here the previous buffer was accepted by
            //the driver and be just made the old
            //version with a new chunk public. therefore we
            //need to replace the outdated version with
            //a new buffer with just the new chunk.
            hotBuf = new DB25ChunkQueue;
            hotBuf->push_back(db25Chunk);

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
        hotBuf->push_back(db25Chunk);
    }
}

//publish the buffer but don't use the exchanged
//buffer as a mirror (as is done in wavemode) so
//throw the contained data away, if there is any
void BEX::publishReset() {
	hotBuf = atomicPtr.exchange(hotBuf);

	if(hotBuf == nullptr) {
		hotBuf = new DB25ChunkQueue;
	} else {
		hotBuf->clear();
	}
}


//the driver requests a buffer swap
//atomically getting the stored buffer
//ptr and indicating that this buffer
//is taken by swapping in a nullptr
std::shared_ptr<DB25ChunkQueue> BEX::driverSwapRequest() {
	std::lock_guard<std::mutex> lock(threadLock);
	return std::shared_ptr<DB25ChunkQueue>(atomicPtr.exchange(nullptr));
}
