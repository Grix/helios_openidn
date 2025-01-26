#pragma once

// Class that set up and handles a Helios USB interface on the USB-C port. 
// Meaning an INPUT interface as an alternative to the IDN network interface, not output. For output USB interface see HeliosAdapter.cpp

#include <unistd.h>
#include <cstdio>

extern "C"
{
    int init_usb_gadget();
    void set_msg_received_callbacks(void (*bulk_msg_callback)(size_t, unsigned char*), void (*interrupt_msg_callback)(size_t, unsigned char*));
    int send_interrupt_msg_response(size_t numBytes, void* buffer);
}

class UsbInterface
{

public:

    UsbInterface();

    static void static_interruptUsbReceived(size_t numBytes, unsigned char* buffer);
    static void static_bulkUsbReceived(size_t numBytes, unsigned char* buffer);

private:
    void interruptUsbReceived(size_t numBytes, unsigned char* buffer);
    void bulkUsbReceived(size_t numBytes, unsigned char* buffer);

    static UsbInterface* instance;
};

