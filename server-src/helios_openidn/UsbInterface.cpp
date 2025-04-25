#include "UsbInterface.hpp"

extern ManagementInterface* management;

UsbInterface::UsbInterface()
{
    instance = this;
    init_usb_gadget();
    set_msg_received_callbacks(static_bulkUsbReceived, static_interruptUsbReceived);
}

void UsbInterface::interruptUsbReceived(size_t numBytes, unsigned char* buffer)
{
    printf("RECEIVED INT.\n");

    if (buffer[1] == 0x01)
    {
        printf("COMMAND RECEIVED: STOP\n");
        if (management->devices.size() > 0)
        {
            //management->devices.front()->stop();
            //management->devices.front()->setDACBusy(false);
            // Todo write empty frame
            management->outputs.front()->close();
        }
        hasStarted = false;
    }
    else if (buffer[1] == 0x02)
    {
        printf("COMMAND RECEIVED: SET SHUTTER\n");
        // Todo?
    }
    else if (buffer[0] == 0x03)
    {
        printf("COMMAND RECEIVED: GET STATUS\n");

        unsigned char status = 0;
        if (management->outputs.size() > 0)
        {
            if (!management->outputs.front()->hasBufferedFrame())
                status = 1;
        }

        unsigned char response[52];
        response[0] = 0x83;
        response[1] = status;
        send_interrupt_msg_response(2, response);
    }
    else if (buffer[0] == 0x04)
    {
        printf("COMMAND RECEIVED: GET FW VERSION\n");
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
        printf("COMMAND RECEIVED: GET NAME\n");
        unsigned char response[32];
        strncpy((char*)response + 1, (management->settingIdnHostname +" (USB)").c_str(), 31);
        response[0] = 0x85;
        response[31] = '\0';
        send_interrupt_msg_response(32, response);
    }

}

void UsbInterface::bulkUsbReceived(size_t numBytes, unsigned char* buffer)
{
    isBusy = true;
    printf("RECEIVED BULK.\n");

    if (management->outputs.size() == 0)
    {
        printf("Error: Received USB frame but no devices are available\n");
        isBusy = false;
        return;
    }

    //management->devices.front()->setDACBusy(true);

    if (numBytes < (5 + 7))
    {
        printf("Error: Received USB bulk but too short length\n");
        isBusy = false;
        return;
    }

    uint16_t numOfPointBytes = numBytes - 5; // from length of received data
    uint16_t numOfPointBytes2 = ((buffer[numOfPointBytes + 3] << 8) | buffer[numOfPointBytes + 2]) * 7; // from control bytes

    if (numOfPointBytes != numOfPointBytes2)
    {
        printf("Error: USB frame: length %d, expected %d\n", numOfPointBytes, numOfPointBytes2);
        isBusy = false;
        return;
    }

    if (!hasStarted)
    {
        management->outputs.front()->close();
        management->outputs.front()->open(RTLaproGraphicOutput::OPMODE_FRAME);
        hasStarted = true;
    }

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

    RTLaproGraphicOutput::CHUNKDATA chunkData;
    memset(&chunkData, 0, sizeof(chunkData));
    chunkData.chunkFlags = IDNFLG_GRAPHIC_FRAME_ONCE; // todo
    chunkData.chunkDuration = (1000000 * numPoints) / pps; // us
    chunkData.decoder = &decoder;
    chunkData.sampleCount = numPoints;

    //pointBuffer.clear();

    uint8_t newBuffer[numPoints * 8]; // Todo reuse buffer to avoid allocation

    unsigned int bufferPos = 0;

    size_t loopLength = numOfPointBytes;
    for (int i = 0; i < loopLength; i += 7)
    {
        uint8_t* currentPoint = buffer + i;

        uint16_t x = (currentPoint[0] << 8) | (currentPoint[1] & 0xF0);
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

        bufferPos += 8;

        /*ISPDB25Point point;
        point.x = (currentPoint[0] << 8) | (currentPoint[1] & 0xF0);
        point.y = (((currentPoint[1] & 0x0F) << 8) | currentPoint[2]) << 4;
        point.r = currentPoint[3] * 0x101;
        point.g = currentPoint[4] * 0x101;
        point.b = currentPoint[5] * 0x101;
        point.intensity = currentPoint[6] * 0x101;
        point.u1 = point.u2 = point.u3 = point.u4 = 0;

        pointBuffer.push_back(point);*/

        //management->devices.front()->addPointToSlice(point, metadata);
    }

#ifndef NDEBUG
    printf("Processing, bufferpos %d\n", bufferPos);
#endif

    management->outputs.front()->process(chunkData, newBuffer, bufferPos);

    /*TimeSlice slice;
    slice.dataChunk = management->devices.front()->convertPoints(pointBuffer);
    slice.durationUs = (1000000 * numOfPointBytes) / pps;

    management->devices.front()->writeFrame(slice, (1000000 * numOfPointBytes) / pps);*/

    isBusy = false;
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