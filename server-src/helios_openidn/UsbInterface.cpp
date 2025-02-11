#include "UsbInterface.hpp"

extern std::shared_ptr<ManagementInterface> management;

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
            management->devices.front()->stopAndEmptyQueue();
        }
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
        if (management->devices.size() > 0)
        {
            status = management->devices.front()->getHasFrameInQueue() ? 0 : 1;
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
        strncpy((char*)response + 1, management->settingIdnHostname.append(" (USB)").c_str(), 31);
        response[0] = 0x85;
        response[31] = '\0';
        send_interrupt_msg_response(32, response);
    }

}

void UsbInterface::bulkUsbReceived(size_t numBytes, unsigned char* buffer)
{
    printf("RECEIVED BULK.\n");

    if (management->devices.size() == 0)
        return;

    if (numBytes < (5 + 7))
        return;

    uint16_t numOfPointBytes = numBytes - 5; // from length of received data
    uint16_t numOfPointBytes2 = ((buffer[numOfPointBytes + 3] << 8) | buffer[numOfPointBytes + 2]) * 7; // from control bytes

    if (numOfPointBytes != numOfPointBytes2)
        return;

    unsigned int pps = (buffer[numOfPointBytes + 1] << 8) | buffer[numOfPointBytes + 0];
    unsigned int flags = buffer[numOfPointBytes + 4];

    ISPFrameMetadata metadata;
    metadata.once = (flags & (1 << 1));
    metadata.isWave = true;
    metadata.len = numOfPointBytes / 7;
    metadata.dur = (1000000 * metadata.len) / pps;

    size_t loopLength = numBytes - 5;
    for (int i = 0; i < loopLength; i += 7)
    {
        uint8_t* currentPoint = buffer + i;

        ISPDB25Point point;
        point.x = (currentPoint[0] << 8) | (currentPoint[1] & 0xF0);
        point.y = (((currentPoint[1] & 0x0F) << 8) | currentPoint[2]) << 4;
        point.r = currentPoint[3];
        point.g = currentPoint[4];
        point.b = currentPoint[5];
        point.intensity = currentPoint[6];
        point.u1 = point.u2 = point.u3 = point.u4 = 0;
        management->devices.front()->addPointToSlice(point, metadata);
    }
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