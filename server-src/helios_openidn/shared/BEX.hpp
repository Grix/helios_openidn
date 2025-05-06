#ifndef BEX_H_
#define BEX_H_

#include <memory>
#include <atomic>
#include <deque>
#include <mutex>

#include "types.h"
#include "ISPDB25Point.h"


class DB25Chunk {
public:
    uint32_t duration;
	std::vector<ISPDB25Point> db25Samples;

    int isWave;
    int once;
};
using DB25ChunkQueue = std::vector<std::shared_ptr<DB25Chunk>>;


//Buffer EXchange. Here, buffers are moved from the network thread to the
//driver thread as smoothly as possible
class BEX {
public:
	//add new data to the network side of the buffer
	BEX();
    void networkAppendSlice(std::shared_ptr<DB25Chunk> db25Chunk);
	void publishReset();
	void setMode(int mode);
	unsigned getMode();
	//get the currently advertised buffer
	std::shared_ptr<DB25ChunkQueue> driverSwapRequest();
	void resetBuffers();
	bool hasBufferedFrame();

private:
	//an atomic pointer to a list of chunks
	//hotBuf: actively being changed
	DB25ChunkQueue* hotBuf;
	std::atomic<DB25ChunkQueue*> atomicPtr;
	int mode = DRIVER_INACTIVE;
	std::mutex threadLock;
};

#endif
