#include "UsbInterface.hpp"

extern ManagementInterface* management;

void* usbOutputLoopThread(void* args)
{
    UsbInterface* player = (UsbInterface*)args;

    player->outputLoop();
}


UsbInterface::UsbInterface()
{
    instance = this;
    init_usb_gadget();
    set_msg_received_callbacks(static_bulkUsbReceived, static_interruptUsbReceived);

    if (pthread_create(&outputThread, NULL, &usbOutputLoopThread, this) != 0) {
        printf("ERROR CREATING USB OUTPUT THREAD\n");
    }
}
void UsbInterface::outputLoop()
{
    struct timespec delay, dummy, now, difference;
    delay.tv_sec = 0;
    delay.tv_nsec = 200000; // 200 us
    while (1) // todo close on exit
    {
        std::shared_ptr<QueuedChunk> frame;
        bool empty = false;
        {
            std::lock_guard<std::mutex> lock(threadLock);

#if __cplusplus >= 201103L
            atomic_thread_fence(std::memory_order_acquire);
#endif

            if (queue.empty())
                empty = true;
            else
            {
                frame = queue.front();
                queue.pop_front();
            }
        }

        if (empty)
        {
            clock_gettime(CLOCK_MONOTONIC, &now);
            unsigned long sdif = now.tv_sec - lastReception.tv_sec;
            unsigned long nsdif = now.tv_nsec - lastReception.tv_nsec;
            unsigned long tdif = sdif * 1000000000 + nsdif;
            if (tdif > 500000000) // 500ms timeout
            {
                delay.tv_nsec = 2000000; // 2 ms
                management->stopOutput(OUTPUT_MODE_USB);
            }

            nanosleep(&delay, &dummy);
            continue;
        }
        else
            delay.tv_nsec = 200000; // 200 us

        unsigned int numOfPoints = frame->buffer.size();

        if (numOfPoints > 0)
        {
            /*RTLaproGraphicOutput::CHUNKDATA chunkData;
            memset(&chunkData, 0, sizeof(chunkData));
            chunkData.modFlags = RTLaproGraphicOutput::MODFLAG_SCAN_ONCE;
            chunkData.chunkDuration = (1000000 * numOfPoints) / frame->pps; // us
            chunkData.decoder = &decoder;
            chunkData.sampleCount = numOfPoints;

            outputs->front()->process(chunkData, frame->buffer.data(), frame->buffer.size());*/

            TimeSlice slice;
            slice.dataChunk = management->devices.front()->convertPoints(frame->buffer);
            slice.durationUs = (double)(1000000 * numOfPoints) / frame->pps;

            management->devices.front()->writeFrame(slice, slice.durationUs);

            /*if (slice.durationUs > 1000)
            {
                nanosleep(&delay, &dummy);
            }
            else
                std::this_thread::yield();*/
        }
        else
        {
            nanosleep(&delay, &dummy);
        }
    }
}


void UsbInterface::interruptUsbReceived(size_t numBytes, unsigned char* buffer)
{
    if (buffer[1] == 0x01)
    {
        printf("CMD RECVD: STOP\n");
        if (management->devices.size() > 0)
        {
            //management->devices.front()->stop();
            //management->devices.front()->setDACBusy(false);
            // Todo write empty frame
            {
                std::lock_guard<std::mutex> lock(threadLock);
                while (!queue.empty())
                    queue.pop_front();
            }
            management->stopOutput(OUTPUT_MODE_USB);
        }
        hasStarted = false;
    }
    else if (buffer[1] == 0x02)
    {
        printf("CMD RECVD: SET SHUTTER\n");
        // Todo?
    }
    else if (buffer[0] == 0x03)
    {
        //printf("CMD RECVD: GET STATUS\n");

        unsigned char status = 0;
        if (management->devices.size() > 0)
        {
            //if (!management->devices.front()->getIsBusy())
            std::lock_guard<std::mutex> lock(threadLock);
            if (queue.size() <= 1)
            {
                //printf("status OK, empty buffer\n");

                status = 1;
            }
            else
            {
                double bufferDurationSeconds = queue.size() * queue.front()->buffer.size() / (double)queue.front()->pps;
                if (bufferDurationSeconds < bufferTargetDurationSeconds) // buffer target size. Todo better queue duration calculation
                    status = 1;

                //printf("status %d, buffer dur %f size %d\n", status, bufferDurationSeconds, queue.size());
            }
        }
        /*if (management->outputs.size() > 0)
        {
            if (!management->outputs.front()->hasBufferedFrame()) //
                status = 1;
        }*/

        unsigned char response[2];
        response[0] = 0x83;
        response[1] = status;
        send_interrupt_msg_response(2, response);
    }
    else if (buffer[0] == 0x04)
    {
        //printf("CMD RECVD: GET FW VERSION\n");
        unsigned char response[5];
        response[0] = 0x84;
        response[1] = management->softwareVersionUsb; // todo make same format as network response
        response[2] = 0;
        response[3] = 0;
        response[4] = 0;
        send_interrupt_msg_response(5, response);
    }
    else if (buffer[0] == 0x05)
    {
        printf("CMD RECVD: GET NAME\n");
        unsigned char response[32];
        strncpy((char*)response + 1, (management->settingIdnHostname +" (USB)").c_str(), 31);
        response[0] = 0x85;
        response[31] = '\0';
        send_interrupt_msg_response(32, response);
    }

}

void UsbInterface::bulkUsbReceived(size_t numBytes, unsigned char* buffer)
{
    //return; // TEST

    if (!management->requestOutput(OUTPUT_MODE_USB))
    {
        printf("Warning: Requested USB output, but was busy\n");
        return;
    }

#ifndef NDEBUG
    printf("RECEIVED BULK.\n");
#endif

    if (management->devices.size() == 0)
    {
        printf("Error: Received USB frame but no devices are available\n");
        return;
    }

    //management->devices.front()->setDACBusy(true);

    if (numBytes < (5 + 7))
    {
        printf("Error: Received USB bulk but too short length\n");
        return;
    }

    uint16_t numOfPointBytes = numBytes - 5; // from length of received data
    uint16_t numOfPointBytes2 = ((buffer[numOfPointBytes + 3] << 8) | buffer[numOfPointBytes + 2]) * 7; // from control bytes

    if (numOfPointBytes != numOfPointBytes2)
    {
        printf("Error: USB frame: length %d, expected %d\n", numOfPointBytes, numOfPointBytes2);
        return;
    }

    clock_gettime(CLOCK_MONOTONIC, &lastReception);

    /*if (!hasStarted)
    {
        management->outputs.front()->close();
        management->outputs.front()->open(RTLaproGraphicOutput::OPMODE_FRAME);
        hasStarted = true;
    }*/

    //management->devices.front()->resetChunkBuffer();
    //management->devices.front()->bex->setMode(DRIVER_FRAMEMODE);

    unsigned int pps = (buffer[numOfPointBytes + 1] << 8) | buffer[numOfPointBytes + 0];
    unsigned int flags = buffer[numOfPointBytes + 4];
    unsigned int numPoints = numOfPointBytes / 7;

#ifndef NDEBUG
    printf("Recvd USB: points %d, pps %d\n", numPoints, pps);
#endif

    /*ISPFrameMetadata metadata;
    metadata.once = (flags & (1 << 1));
    metadata.isWave = false;
    metadata.len = numOfPointBytes / 7;
    metadata.dur = (1000000 * metadata.len) / pps;*/

    /*RTLaproGraphicOutput::CHUNKDATA chunkData;
    memset(&chunkData, 0, sizeof(chunkData));
    chunkData.modFlags = RTLaproGraphicOutput::MODFLAG_SCAN_ONCE; // todo
    chunkData.chunkDuration = (1000000 * numPoints) / pps; // us
    chunkData.decoder = &decoder;
    chunkData.sampleCount = numPoints;*/

    // Todo reuse buffer to avoid allocation

    int pointsPerFrame = numPoints;
    int maxPointsPerFrame = management->devices.front()->maxBytesPerTransmission() / management->devices.front()->bytesPerPoint();
    if (numPoints > maxPointsPerFrame)
    {
        if (maxPointsPerFrame == 0)
            return;

        pointsPerFrame = (int)ceil(numPoints / ceil((double)numPoints / maxPointsPerFrame));
        if (pointsPerFrame < maxPointsPerFrame)
            pointsPerFrame += 1;

        if (pointsPerFrame <= 0)
            return;
    }

    std::shared_ptr<QueuedChunk> frame = std::make_shared<QueuedChunk>();
    frame->buffer.reserve(pointsPerFrame);

    unsigned int bufferPos = 0;
    unsigned int currentPointInFrame = 0;

    size_t loopLength = numOfPointBytes;
    for (int i = 0; i < loopLength; i += 7)
    {
        uint8_t* currentPoint = buffer + i;

        /*uint16_t x = (currentPoint[0] << 8) | (currentPoint[1] & 0xF0);
        *((uint16_t*)&newBuffer[bufferPos + 0]) = x;
        //newBuffer[bufferPos + 0] = x & 0xFF;
        //newBuffer[bufferPos + 1] = x >> 8;
        uint16_t y = (((currentPoint[1] & 0x0F) << 8) | currentPoint[2]) << 4;
        *((uint16_t*)&newBuffer[bufferPos + 2]) = y;
        //newBuffer[bufferPos + 2] = y & 0xFF;
        //newBuffer[bufferPos + 3] = y >> 8;
        newBuffer[bufferPos + 4] = currentPoint[3];
        newBuffer[bufferPos + 5] = currentPoint[4];
        newBuffer[bufferPos + 6] = currentPoint[5];
        newBuffer[bufferPos + 7] = currentPoint[6];

        bufferPos += 8;*/

        ISPDB25Point point;
        point.x = (currentPoint[0] << 8) | (currentPoint[1] & 0xF0);
        point.y = (((currentPoint[1] & 0x0F) << 8) | currentPoint[2]) << 4;
        point.r = currentPoint[3] * 0x101;
        point.g = currentPoint[4] * 0x101;
        point.b = currentPoint[5] * 0x101;
        point.intensity = currentPoint[6] * 0x101;
        point.u1 = point.u2 = point.u3 = 0;
        frame->buffer.push_back(point);

        currentPointInFrame++;

        if (currentPointInFrame >= pointsPerFrame && i < loopLength - 7) //  Send partial frame, if frame is split it due to being too large for DAC.
        {
            //printf("Partial frame, currentPointInFrame %d, bufsize %d\n", currentPointInFrame, frame->buffer.size());

            frame->pps = pps; 

            {
                std::lock_guard<std::mutex> lock(threadLock);
                queue.push_back(frame);
            }
            frame = std::make_shared<QueuedChunk>();
            frame->buffer.reserve(pointsPerFrame);

            //std::shared_ptr<TimeSlice> frameSlice = std::make_shared<TimeSlice>();

            currentPointInFrame = 0;
        }

        //pointBuffer.push_back(point);

        //management->devices.front()->addPointToSlice(point, metadata);
    }

    //printf("Finished frame, currentPointInFrame %d\n", currentPointInFrame);

#ifndef NDEBUG
    printf("Processing, bufferpos %d\n", bufferPos);
#endif

    //management->outputs.front()->process(chunkData, newBuffer, bufferPos);

    if (!frame->buffer.empty())
    {
        frame->pps = pps; // todo
        {
            std::lock_guard<std::mutex> lock(threadLock);
            queue.push_back(frame);
        }
    }

    /*TimeSlice slice;
    slice.dataChunk = management->devices.front()->convertPoints(pointBuffer);
    slice.durationUs = (1000000 * numOfPointBytes) / pps;

    isReceiveBusy = false;

    while (isSendBusy)
    { 
        if (slice.durationUs > 1000)
        {
            struct timespec delay, dummy;
            delay.tv_sec = 0;
            delay.tv_nsec = 200000;
            nanosleep(&delay, &dummy);
        }
    }

    isSendBusy = true;

    management->devices.front()->writeFrame(slice, (1000000 * numOfPointBytes) / pps);

    isSendBusy = false;*/

}


void UsbInterface::static_interruptUsbReceived(size_t numBytes, unsigned char* buffer)
{
    if (instance)
        instance->interruptUsbReceived(numBytes, buffer);
}

void UsbInterface::static_bulkUsbReceived(size_t numBytes, unsigned char* buffer)
{
    if (instance)
        instance->bulkUsbReceived(numBytes, buffer);
}
