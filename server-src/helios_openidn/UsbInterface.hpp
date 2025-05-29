#pragma once

// Class that sets up and handles a Helios USB interface on the USB-C port. 
// Meaning an INPUT interface, as an alternative to the IDN network server, not output. For output USB interface see HeliosAdapter.cpp

#include "ManagementInterface.hpp"
#include "shared/types.h"
#include "server/idn-stream.h"
#include "output/IdtfDecoder.hpp"
#include <unistd.h>
#include <cstdio>
#include <memory>

extern "C"
{
    int init_usb_gadget();
    void set_msg_received_callbacks(void (*bulk_msg_callback)(size_t, unsigned char*), void (*interrupt_msg_callback)(size_t, unsigned char*));
    int send_interrupt_msg_response(size_t numBytes, unsigned char* buffer);
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

    inline static UsbInterface* instance = nullptr;
    bool hasSentReadySignal = false;
    bool isBusy = false;
    IdtfDecoder decoder;
    std::vector<ISPDB25Point> pointBuffer = std::vector<ISPDB25Point>(5000);
    bool hasStarted = false;
};

