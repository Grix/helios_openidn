#include "UsbInterface.hpp"

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
    }
    else if (buffer[1] == 0x02)
    {
        printf("COMMAND RECEIVED: SET SHUTTER\n");
    }
    else if (buffer[0] == 0x03)
    {
        printf("COMMAND RECEIVED: GET STATUS\n");
        unsigned char response[52];
        response[0] = 0x83;
        response[1] = 1;
        send_interrupt_msg_response(2, response);
    }
    else if (buffer[0] == 0x04)
    {
        printf("COMMAND RECEIVED: GET FW VERSION\n");
        unsigned char response[5];
        response[0] = 0x84;
        response[1] = 50;
        response[2] = 0;
        response[3] = 0;
        response[4] = 0;
        send_interrupt_msg_response(5, response);
    }
    else if (buffer[0] == 0x05)
    {
        printf("COMMAND RECEIVED: GET NAME\n");
        unsigned char response[32] = " TESTNAME";
        response[0] = 0x85;
        send_interrupt_msg_response(32, response);
    }

}

void UsbInterface::bulkUsbReceived(size_t numBytes, unsigned char* buffer)
{
    printf("RECEIVED BULK.\n");


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