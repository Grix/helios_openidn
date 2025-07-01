//create a WinUSB Gadget Device
//derived from: https://blog.soutade.fr/post/2016/07/create-your-own-usb-gadget-with-gadgetfs.html

// MIT License

// Copyright (c) 2016 Grégory Soutadé
// 2020 WinUSB feature added by beb-at-stepover.de and rundekugel @ github.com

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//--- include ---



#define CONFIG_USB_GADGET_VERBOSE 

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#include <linux/types.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadgetfs.h>
#include <time.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>


//--- defines ---
#define FETCH(_var_)                            \
    memcpy(cp, &_var_, _var_.bLength);          \
    cp += _var_.bLength;


// Config value is the number of endpoints. After that, we have paths relative to GadgetFS. 
// When mounted, there is only USB_DEV, endpoints appears after the first configuration (ep0). 
// Name of endpoints is dependent of the driver implementation.
#define CONFIG_VALUE 1

#define WR_BUF_SIZE 0x2000

//make this configurable
#define WU_VENDOR_CODE     0xcd    //choose something fitting to your purposes

#define VERSION  "1.0.1"

// Specific to controller
//#define USB_DEV "/dev/gadget/dwc2"

// this will be generated, after /dev/gadget is created
#define USB_DEV "/dev/gadget/ff400000.usb"        

#define USB_EP_INT_IN  "/dev/gadget/ep3in"   // host perspective for direction
#define USB_EP_INT_OUT "/dev/gadget/ep6out"
#define USB_EP_BULK_OUT "/dev/gadget/ep2out"

#define USB_VID  0x1209 // Imitate regular Helios USB
#define USB_PID  0xe500


/* From usbstring.[ch] */
struct usb_string {
    __u8            id;
    const char* s;
};

struct usb_gadget_strings {
    __u16            language;    /* 0x0409 for en-us */
    struct usb_string* strings;
};


enum {
    STRINGID_MANUFACTURER = 1,
    STRINGID_PRODUCT,
    STRINGID_SERIAL,
    STRINGID_CONFIG_HS,
    STRINGID_CONFIG_LS,
    STRINGID_INTERFACE,
    //STRINGID_WINUSB = 0xee,   //is done in extra function
    STRINGID_MAX
};

struct io_thread_args {
    unsigned stop;
    int fd_int_in, fd_int_out, fd_bulk_out, fd_main;  //host perspective for directions
};

//make this configurable
struct usb_string stringtab[] = {
    { STRINGID_MANUFACTURER, "Gitle Mikkelsen", },
    { STRINGID_PRODUCT,      "Helios Laser DAC", },
    { STRINGID_SERIAL,       "b00000001", },
    { STRINGID_CONFIG_HS,    "High speed configuration", },
    { STRINGID_CONFIG_LS,    "Low speed configuration", },
    { STRINGID_INTERFACE,    "Custom interface", },
    //{ STRINGID_WINUSB,    "MSFT100", },
    { STRINGID_MAX, NULL},
};

struct usb_gadget_strings strings = {
    .language = 0x0409, /* en-us */
    .strings = stringtab,
};


//WinUSB Feature Descriptor
const __u8 u8ExtendedCompatIDOSFeatDesc[] =
{
  0x28, 0x00, 0x00, 0x00, /* dwLength Length of this descriptor */ //(40 bytes)
  0x00, 0x01,             /* bcdVersion = Version 1.0 */
  0x04, 0x00,             /* wIndex = 0x0004 */
  0x01,                   /* bCount = 1 */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Reserved */
  0x00,                   /* Interface number = 0 */
  0x01,                   /* Reserved */
  0x57, 0x49, 0x4E, 0x55, 0x53, 0x42, 0x00, 0x00, //string = "WINUSB"
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* subCompatibleID */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00  /* Reserved */
};

//WinUSB os string descriptor (# 0xee)
const __u8 u8Usb_os_str_desc[] =
{
  0x12,    //bLength
  0x03,    //bDescriptorType
  'M',0,'S',0,'F',0,'T',0,'1',0,'0',0,'0',0,    //qwSignature
  WU_VENDOR_CODE,
  0x00        //flags unused
};


//globals
struct usb_endpoint_descriptor ep_descriptor_int_in;
struct usb_endpoint_descriptor ep_descriptor_int_out;
struct usb_endpoint_descriptor ep_descriptor_bulk_out;
int verbosity = 2;   //make it configurable
int txamount = 100;
int txcounter = 0;
int rxcounter = 0;
struct io_thread_args thread_args;
__u8 altSetting = 0;
pthread_t thread_main_ep0;

size_t txSize = 0;
char txBuffer[32];

int doRxIgnore = 0;  //how many bytes to ignore 

//--- prototypes ---
void put_unaligned_le16(__u16 val, __u16* cp);
int utf8_to_utf16le(const char* s, __u16* cp, unsigned len);
int usb_gadget_get_string(struct usb_gadget_strings* table, int id, __u8* buf);

void* handle_ep0_thread(void* arg);
void handle_setup_request(int fd, struct usb_ctrlrequest* setup);
int init_ep(int* fd_int_in, int* fd_int_out, int* fd_bulk_out);
void* rx_int_thread(void* arg);
void* rx_bulk_thread(void* arg);
void* tx_int_thread(void* arg);
int init_usb_gadget(void);

void (*interrupt_msg_received_callback)(size_t numBytes, unsigned char* buffer);
void (*bulk_msg_received_callback)(size_t numBytes, unsigned char* buffer);
void set_msg_received_callbacks(void (*bulk_msg_callback)(size_t, unsigned char*), void (*interrupt_msg_callback)(size_t, unsigned char*));
int send_interrupt_msg_response(size_t numBytes, unsigned char* buffer);

inline void put_unaligned_le16(__u16 val, __u16* cp)
{
    __u8* p = (void*)cp;

    *p++ = (__u8)val;
    *p++ = (__u8)(val >> 8);
}

int utf8_to_utf16le(const char* s, __u16* cp, unsigned len)
{
    int    count = 0;
    __u8    c;
    __u16    uchar;

    /* this insists on correct encodings, though not minimal ones.
     * BUT it currently rejects legit 4-byte UTF-8 code points,
     * which need surrogate pairs.  (Unicode 3.1 can use them.)
     */
    while (len != 0 && (c = (__u8)*s++) != 0)
    {
        if (c & 0x80)
        {
            // 2-byte sequence:
            // 00000yyyyyxxxxxx = 110yyyyy 10xxxxxx
            if ((c & 0xe0) == 0xc0)
            {
                uchar = (c & 0x1f) << 6;

                c = (__u8)*s++;
                if ((c & 0xc0) != 0xc0)
                    goto fail;
                c &= 0x3f;
                uchar |= c;

                // 3-byte sequence (most CJKV characters):
                // zzzzyyyyyyxxxxxx = 1110zzzz 10yyyyyy 10xxxxxx
            }
            else if ((c & 0xf0) == 0xe0)
            {
                uchar = (c & 0x0f) << 12;

                c = (__u8)*s++;
                if ((c & 0xc0) != 0xc0)
                    goto fail;
                c &= 0x3f;
                uchar |= c << 6;

                c = (__u8)*s++;
                if ((c & 0xc0) != 0xc0)
                    goto fail;
                c &= 0x3f;
                uchar |= c;

                /* no bogus surrogates */
                if (0xd800 <= uchar && uchar <= 0xdfff)
                    goto fail;

                // 4-byte sequence (surrogate pairs, currently rare):
                // 11101110wwwwzzzzyy + 110111yyyyxxxxxx
                //     = 11110uuu 10uuzzzz 10yyyyyy 10xxxxxx
                // (uuuuu = wwww + 1)
                // FIXME accept the surrogate code points (only)

            }
            else
                goto fail;
        }
        else
            uchar = c;
        put_unaligned_le16(uchar, cp++);
        count++;
        len--;
    }
    return count;
fail:
    return -1;
}

int usb_gadget_get_string(struct usb_gadget_strings* table, int id, __u8* buf)
{
    struct usb_string* s;
    int            len;

    /* descriptor 0 has the language id */
    if (id == 0)
    {
        buf[0] = 4;
        buf[1] = USB_DT_STRING;
        buf[2] = (__u8)table->language;
        buf[3] = (__u8)(table->language >> 8);
        return 4;
    }
    for (s = table->strings; s && s->s; s++)
        if (s->id == id)
            break;

    /* unrecognized: stall. */
    if (!s || !s->s)
        return -EINVAL;

    /* string descriptors have length, tag, then UTF16-LE text */
    len = strlen(s->s);
    if (len > 126)
        len = 126;
    memset(buf + 2, 0, 2 * len);    /* zero all the bytes */
    len = utf8_to_utf16le(s->s, (__u16*)&buf[2], len);
    if (len < 0)
        return -EINVAL;
    buf[0] = (len + 1) * 2;
    buf[1] = USB_DT_STRING;
    return buf[0];
}

int init_usb_gadget()
{
    int fd = -1, ret, err = -1;
    uint32_t send_size;
    struct usb_config_descriptor config;
    struct usb_config_descriptor config_hs;
    struct usb_device_descriptor device_descriptor;
    struct usb_interface_descriptor if_alt0_descriptor;
    struct usb_interface_descriptor if_alt1_descriptor;
    uint8_t init_config[2048];
    uint8_t* cp;
    int option;
    __u16 pid, vid, usbver = 2;
    char sernum[12];

    if (verbosity)
        printf("wug V"VERSION"\n");

    fd = open(USB_DEV, O_RDWR | O_SYNC);

    if (fd <= 0)
    {
        fprintf(stderr, "Unable to open %s (%m)\n", USB_DEV);
        fprintf(stderr, "Did you forget to execute this:\n"
            "mkdir /dev/gadget ; mount -t gadgetfs gadgetfs /dev/gadget\n"
            "?\n");
        return 1;
    }

    *(uint32_t*)init_config = 0;
    cp = &init_config[4];

    device_descriptor.bLength = USB_DT_DEVICE_SIZE;
    device_descriptor.bDescriptorType = USB_DT_DEVICE;
    device_descriptor.bDeviceClass = USB_CLASS_VENDOR_SPEC;
    device_descriptor.bDeviceSubClass = 0;
    device_descriptor.bDeviceProtocol = 0;
    device_descriptor.bMaxPacketSize0 = 64; //Set by driver?
    device_descriptor.idVendor = USB_VID;
    device_descriptor.idProduct = USB_PID;
    device_descriptor.bcdDevice = usbver;
    // Strings
    device_descriptor.iManufacturer = STRINGID_MANUFACTURER;
    device_descriptor.iProduct = STRINGID_PRODUCT;
    device_descriptor.iSerialNumber = STRINGID_SERIAL;
    device_descriptor.bNumConfigurations = 1; // Only one configuration

    ep_descriptor_int_in.bLength = USB_DT_ENDPOINT_SIZE;
    ep_descriptor_int_in.bDescriptorType = USB_DT_ENDPOINT;
    ep_descriptor_int_in.bEndpointAddress = USB_DIR_IN | 3;
    ep_descriptor_int_in.bmAttributes = USB_ENDPOINT_XFER_INT;
    ep_descriptor_int_in.wMaxPacketSize = 64; // HS size

    ep_descriptor_int_out.bLength = USB_DT_ENDPOINT_SIZE;
    ep_descriptor_int_out.bDescriptorType = USB_DT_ENDPOINT;
    ep_descriptor_int_out.bEndpointAddress = USB_DIR_OUT | 6;
    ep_descriptor_int_out.bmAttributes = USB_ENDPOINT_XFER_INT;
    ep_descriptor_int_out.wMaxPacketSize = 64; // HS size

    ep_descriptor_bulk_out.bLength = USB_DT_ENDPOINT_SIZE;
    ep_descriptor_bulk_out.bDescriptorType = USB_DT_ENDPOINT;
    ep_descriptor_bulk_out.bEndpointAddress = USB_DIR_OUT | 2;
    ep_descriptor_bulk_out.bmAttributes = USB_ENDPOINT_XFER_BULK;
    ep_descriptor_bulk_out.wMaxPacketSize = 512; // HS size

    if_alt0_descriptor.bLength = sizeof(if_alt0_descriptor);
    if_alt0_descriptor.bDescriptorType = USB_DT_INTERFACE;
    if_alt0_descriptor.bInterfaceNumber = 0;
    if_alt0_descriptor.bAlternateSetting = 0;
    if_alt0_descriptor.bNumEndpoints = 0;
    if_alt0_descriptor.bInterfaceClass = USB_CLASS_VENDOR_SPEC;
    if_alt0_descriptor.bInterfaceSubClass = 0;
    if_alt0_descriptor.bInterfaceProtocol = 0;
    if_alt0_descriptor.iInterface = STRINGID_INTERFACE;

    if_alt1_descriptor.bLength = sizeof(if_alt1_descriptor);
    if_alt1_descriptor.bDescriptorType = USB_DT_INTERFACE;
    if_alt1_descriptor.bInterfaceNumber = 0;
    if_alt1_descriptor.bAlternateSetting = 1;
    if_alt1_descriptor.bNumEndpoints = 3;
    if_alt1_descriptor.bInterfaceClass = USB_CLASS_VENDOR_SPEC;
    if_alt1_descriptor.bInterfaceSubClass = 0;
    if_alt1_descriptor.bInterfaceProtocol = 0;
    if_alt1_descriptor.iInterface = STRINGID_INTERFACE;

    config_hs.bLength = sizeof(config_hs);
    config_hs.bDescriptorType = USB_DT_CONFIG;
    config_hs.wTotalLength = config_hs.bLength + if_alt0_descriptor.bLength + if_alt1_descriptor.bLength + ep_descriptor_int_in.bLength + ep_descriptor_int_out.bLength + ep_descriptor_bulk_out.bLength;
    config_hs.bNumInterfaces = 1;
    config_hs.bConfigurationValue = CONFIG_VALUE;
    config_hs.iConfiguration = STRINGID_CONFIG_HS;
    config_hs.bmAttributes = USB_CONFIG_ATT_ONE;
    config_hs.bMaxPower = 250;

    config.bLength = sizeof(config);
    config.bDescriptorType = USB_DT_CONFIG;
    config.wTotalLength = config.bLength + if_alt0_descriptor.bLength + if_alt1_descriptor.bLength + ep_descriptor_int_in.bLength + ep_descriptor_int_out.bLength + ep_descriptor_bulk_out.bLength;
    config.bNumInterfaces = 1;
    config.bConfigurationValue = CONFIG_VALUE;
    config.iConfiguration = STRINGID_CONFIG_LS;
    config.bmAttributes = USB_CONFIG_ATT_ONE;
    config.bMaxPower = 250;

    FETCH(config);
    FETCH(if_alt0_descriptor);
    FETCH(if_alt1_descriptor);
    FETCH(ep_descriptor_int_in);
    FETCH(ep_descriptor_int_out);
    FETCH(ep_descriptor_bulk_out);

    // Disable HS, force FS for better reliability
    /*FETCH(config_hs);
    FETCH(if_alt0_descriptor);
    FETCH(if_alt1_descriptor);
    FETCH(ep_descriptor_int_in);
    FETCH(ep_descriptor_int_out);
    FETCH(ep_descriptor_bulk_out);*/

    FETCH(device_descriptor);

    // Configure ep0
    send_size = (uint32_t)cp - (uint32_t)init_config;
    ret = write(fd, init_config, send_size);

    if (ret != send_size)
    {
        fprintf(stderr, "Write error %d (%m)\n", ret);
        if (fd != -1)
        {
            close(fd);
        }
    }
    else
    {
        if (verbosity)
            printf("ep0 configured\n");

        thread_args.fd_main = fd;

        pthread_create(&thread_main_ep0, NULL, handle_ep0_thread, &thread_args);
    }

    return err;
}

// The main function. We build the descriptors and send them to ep0. 
// It's needed to send both low/full speed (USB 1) and high speed (USB 2) configurations. 
// Here, they are quite the same.
// Descriptors are sent as a big char array that must starts by an
// uint32_t tag set to 0. All values are expressed in little endian.

// ep0 function :
void* handle_ep0_thread(void* arg)
{
    struct io_thread_args* thread_args = (struct io_thread_args*)arg;
    int fd = thread_args->fd_main;
    int ret, nevents, i;
    fd_set read_set;
    struct usb_gadgetfs_event events[5];

    while (!thread_args->stop)
    {
        FD_ZERO(&read_set);
        FD_SET(fd, &read_set);

        select(fd + 1, &read_set, NULL, NULL, NULL);

        ret = read(fd, &events, sizeof(events));

        if (ret < 0)
        {
            fprintf(stderr, "Read error %d (%m)\n", ret);
            goto end;
        }

        nevents = ret / sizeof(events[0]);

        if (verbosity > 1)
            printf("%d event(s): ", nevents);

        for (i = 0; i < nevents; i++)
        {
            if (verbosity > 1)
                printf("Type:%d. ", events[i].type);
            switch (events[i].type)
            {
            case GADGETFS_CONNECT:    //1
                if (verbosity)
                    printf("EP0 CONNECT\n");
                break;
            case GADGETFS_DISCONNECT:    //2
                thread_args->stop = 1;
                if (verbosity)
                    printf("EP0 DISCONNECT\n");
                break;
            case GADGETFS_SETUP:        //3
                if (verbosity > 1)
                    printf("EP0 SETUP:");
                handle_setup_request(fd, &events[i].u.setup);
                break;
            case GADGETFS_SUSPEND:    //4
                //todo: handle suspend
                //thread_args->stop = 1; // Stopping the threads will crash the system if the program is ever restarted afterwards..
                if (verbosity)
                    printf("Suspend Request!\n");
            case GADGETFS_NOP:        //0
                break;
            }
        }
        if (verbosity > 1)
        {
            printf(".\n");
        }
    }

end:
    thread_args->stop = 1;
    if (thread_args->fd_main != -1)
    {
        close(thread_args->fd_main);
        thread_args->fd_main = -1;
    }
    return;
}

// This one receives events and handle them. The most important are 
// setup requests, which are requests that kernel cannot full handle by
// itself (or notice userspace).
void handle_setup_request(int fd, struct usb_ctrlrequest* setup)
{
    int status;
    uint8_t buffer[512];
    pthread_t threadr;
    pthread_t threadrbulk;
    pthread_t threadt;

    if (verbosity > 1)
    {
        printf("Setup request %d\n", setup->bRequest);
    }

    /* bmRequestType Definition
    typedef __packed union _REQUEST_TYPE {
      __packed struct _BM {
        U8 Recipient : 5;
        U8 Type      : 2;
        U8 Dir       : 1;
      } BM;
      U8 B;
    } REQUEST_TYPE;
    */
    switch (setup->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR:
        if (setup->bRequestType != USB_DIR_IN)
            goto stall;
        switch (setup->wValue >> 8)
        {
        case USB_DT_STRING:
            if (verbosity > 1)
                printf("Get string id #0x%02x (max length %d)\n", setup->wValue & 0xff,
                    setup->wLength);
            if ((setup->wValue & 0xff) == 0xee)    //winusb
            {
                if (verbosity > 1)
                    printf("asked for 0xee.\n");
                memcpy(buffer, u8Usb_os_str_desc, sizeof(u8Usb_os_str_desc));
                status = sizeof(u8Usb_os_str_desc);
                write(fd, buffer, status);
                return;
                break;
            }
            status = usb_gadget_get_string(&strings, setup->wValue & 0xff, buffer);
            // Error 
            if (status < 0)
            {
                fprintf(stderr, "String not found !!\n");
                break;
            }
            else
            {
                if (verbosity > 1)
                    printf("Found %d bytes\n", status);
            }
            write(fd, buffer, status);
            return;
        default:
            fprintf(stderr, "Cannot return descriptor %d\n", (setup->wValue >> 8));
        }
        break;
    case USB_REQ_SET_CONFIGURATION:
        if (setup->bRequestType != USB_DIR_OUT)
        {
            fprintf(stderr, "Bad dir\n");
            goto stall;
        }
        switch (setup->wValue)
        {
        case CONFIG_VALUE:
            if (verbosity > 1)
                printf("Set config value\n");
            if (!thread_args.stop)
            {
                thread_args.stop = 1;
                usleep(200000); // Wait for termination
            }
            if (thread_args.fd_int_in <= 0)
            {
                status = init_ep(&thread_args.fd_int_in, &thread_args.fd_int_out, &thread_args.fd_bulk_out);
            }
            else
                status = 0;
            if (!status)
            {
                thread_args.stop = 0;
                pthread_create(&threadr, NULL, rx_int_thread, &thread_args);
                pthread_create(&threadrbulk, NULL, rx_bulk_thread, &thread_args);
                pthread_create(&threadt, NULL, tx_int_thread, &thread_args);
            }
            break;
        case 0:
            if (verbosity)
                printf("Disable threads\n");
            thread_args.stop = 1;
            break;
        default:
            fprintf(stderr, "Unhandled configuration value %d\n", setup->wValue);
            break;
        }
        // Just ACK
        status = read(fd, &status, 0);
        return;
    case USB_REQ_GET_INTERFACE:
        if (verbosity > 1)
            printf("GET_INTERFACE\n");
        buffer[0] = altSetting;
        write(fd, buffer, 1);
        return;
    case USB_REQ_SET_INTERFACE:
        altSetting = setup->wValue;
        if (verbosity > 1)
            printf("SET_INTERFACE %d %d\n", setup->wIndex, altSetting);
        //ioctl(thread_args.fd_int_in, GADGETFS_CLEAR_HALT); // These lines break my program. What do they even do?
        //ioctl(thread_args.fd_int_out, GADGETFS_CLEAR_HALT);
        //ioctl(thread_args.fd_bulk_out, GADGETFS_CLEAR_HALT);
        // ACK
        status = read(fd, &status, 0);
        return;
    case WU_VENDOR_CODE:
    {

        //~ #define USB_DIR_OUT			0		/* to device */
        //~ #define USB_DIR_IN			0x80		/* to host */

        //~ /*
         //~ * USB types, the second of three bRequestType fields
         //~ */
        //~ #define USB_TYPE_MASK			(0x03 << 5)
        //~ #define USB_TYPE_STANDARD		(0x00 << 5)
        //~ #define USB_TYPE_CLASS			(0x01 << 5)
        //~ #define USB_TYPE_VENDOR			(0x02 << 5)
        //~ #define USB_TYPE_RESERVED		(0x03 << 5)

        //~ /*
         //~ * USB recipients, the third of three bRequestType fields
         //~ */
        //~ #define USB_RECIP_MASK			0x1f
        //~ #define USB_RECIP_DEVICE		0x00
        //~ #define USB_RECIP_INTERFACE		0x01
        //~ #define USB_RECIP_ENDPOINT		0x02
        //~ #define USB_RECIP_OTHER			0x03

            /* bmRequestType Definition */
        typedef union _req_type {
            struct _BM {
                __u8 Recipient : 5;
                __u8 Type : 2;
                __u8 Dir : 1;
            } __attribute__((packed))  BM;
            __u8 B;
        } __attribute__((packed))  SRequestType, * PRequestType;

        __u8 rt = setup->bRequestType;
        SRequestType rtt;
        rtt.B = rt;
        if (verbosity > 1)
        {
            printf("WU-code. wVal: 0x%04x. ", setup->wValue);
            printf("max length %d. \n", setup->wLength);
            printf("bRequestType: 0x%02x. \n", rt);
            printf("Req.Type.Dir=%d; Recip=%x; Type:%d\n", rtt.BM.Dir, rtt.BM.Recipient, rtt.BM.Type);
        }
        if (rtt.BM.Dir == 0)
        {//out
            if (rtt.BM.Recipient == USB_RECIP_DEVICE)
            {
                //handle vendor spec. requests
               //if(setup->bRequest != WU_VENDOR_CODE){
               // goto stall; }
            }
        }

        memcpy(buffer, u8ExtendedCompatIDOSFeatDesc, sizeof(u8ExtendedCompatIDOSFeatDesc));
        status = sizeof(u8ExtendedCompatIDOSFeatDesc);
        write(fd, buffer, status);
        return;
    }
    }
stall:
    fprintf(stderr, "Stalled\n");
    // Error
    if (setup->bRequestType & USB_DIR_IN)
        read(fd, &status, 0);
    else
        write(fd, &status, 0);
}



// A bad response within this function can stall the endpoint. Two principle 
//functions are to send back strings (not managed by driver) and starts/stop io_thread().

// The init_ep() function is pretty simple. It justs sends endpoint descriptors
// (in low/full and high speed configuration). Like ep0, it must starts with an uint32_t tag of value 1 :

int init_ep(int* fd_int_in, int* fd_int_out, int* fd_bulk_out)
{
    uint8_t init_config[2048];
    uint8_t* cp;
    int ret = -1;
    uint32_t send_size;

    // Configure ep1 (low/full speed + high speed)
    *fd_int_in = open(USB_EP_INT_IN, O_RDWR);

    if (*fd_int_in <= 0)
    {
        fprintf(stderr, "Unable to open %s (%m)\n", USB_EP_INT_IN);
        goto end;
    }

    *(uint32_t*)init_config = 1;
    cp = &init_config[4];

    FETCH(ep_descriptor_int_in);
    FETCH(ep_descriptor_int_in);

    send_size = (uint32_t)cp - (uint32_t)init_config;
    ret = write(*fd_int_in, init_config, send_size);

    if (ret != send_size)
    {
        fprintf(stderr, "Write error %d (%m)\n", ret);
        goto end;
    }
    if (verbosity)
        printf("int in EP configured\n");


    // Configure ep2 (low/full speed + high speed)
    *fd_int_out = open(USB_EP_INT_OUT, O_RDWR);

    if (*fd_int_out <= 0)
    {
        fprintf(stderr, "Unable to open %s (%m)\n", USB_EP_INT_OUT);
        goto end;
    }

    *(uint32_t*)init_config = 1;
    cp = &init_config[4];

    FETCH(ep_descriptor_int_out);
    FETCH(ep_descriptor_int_out);

    send_size = (uint32_t)cp - (uint32_t)init_config;
    ret = write(*fd_int_out, init_config, send_size);

    if (ret != send_size)
    {
        fprintf(stderr, "Write error %d (%m)\n", ret);
        goto end;
    }
    if (verbosity)
        printf("int out EP configured\n");


    // Configure ep3 (low/full speed + high speed)
    *fd_bulk_out = open(USB_EP_BULK_OUT, O_RDWR);

    if (*fd_bulk_out <= 0)
    {
        fprintf(stderr, "Unable to open %s (%m)\n", USB_EP_BULK_OUT);
        goto end;
    }

    *(uint32_t*)init_config = 1;
    cp = &init_config[4];

    FETCH(ep_descriptor_bulk_out);
    FETCH(ep_descriptor_bulk_out);

    send_size = (uint32_t)cp - (uint32_t)init_config;
    ret = write(*fd_bulk_out, init_config, send_size);

    if (ret != send_size)
    {
        fprintf(stderr, "Write error %d (%m)\n", ret);
        goto end;
    }
    if (verbosity)
        printf("bulk out EP configured\n");

    ret = 0;

end:
    return ret;
}

// Finally, the io_thread() that responds to host requests. Here, I use 
// select, but it seems not to be handled by driver, I/Os are
// just blocking, but it could be necessary if we want to stop thread.

// Respond to host requests
void* rx_int_thread(void* arg)
{
    // Real time driver thread
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR) + (sched_get_priority_max(SCHED_RR) - sched_get_priority_min(SCHED_RR)) / 4;
    sched_setscheduler(0, SCHED_RR, &sp);

    struct io_thread_args* thread_args = (struct io_thread_args*)arg;
    fd_set read_set;
    struct timeval timeout;
    int ret, max_read_fd;
    __u8 buffer[32];

    max_read_fd = 0;

    //if (thread_args->fd_in > max_write_fd) max_write_fd = thread_args->fd_in;
    if (thread_args->fd_int_out > max_read_fd) // was fd_out
        max_read_fd = thread_args->fd_int_out;

    if (verbosity > 1)
        fprintf(stderr, "Starting rx_int_thread\n");

    while (!thread_args->stop)
    {
        FD_ZERO(&read_set);
        FD_SET(thread_args->fd_int_out, &read_set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 30000; // 30ms

        if (verbosity > 1)
            fprintf(stderr, "Starting select\n");

        memset(buffer, 0, sizeof(buffer));
        ret = select(max_read_fd + 1, &read_set, NULL, NULL, NULL);//&timeout);

        if (verbosity > 1)
            fprintf(stderr, "Select returned: %d(%m)\n", ret);

        // Timeout
        if (ret == 0)
            continue;

        // Error
        if (ret < 0)
        {
            fprintf(stderr, "Select ERROR");
            break;
        }

        if (verbosity > 1)
            fprintf(stderr, "Starting read\n");

        ret = read(thread_args->fd_int_out, buffer, sizeof(buffer));

        if (ret >= 0)
        {
            if (verbosity > 1)
                printf("Read %d bytes\n", ret);
            rxcounter++;
            if (ret > 0)
            {
                interrupt_msg_received_callback(ret, buffer);
            }
        }
        else
        {
            fprintf(stderr, "Read ERROR %d(%m)\n", ret);
            //todo: put a sleep here
        }
    }
    close(thread_args->fd_int_out);

    thread_args->fd_int_out = -1;

    return NULL;
}

void* rx_bulk_thread(void* arg)
{

    // Real time driver thread
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR) + (sched_get_priority_max(SCHED_RR) - sched_get_priority_min(SCHED_RR)) / 4;
    sched_setscheduler(0, SCHED_RR, &sp);

    struct io_thread_args* thread_args = (struct io_thread_args*)arg;
    fd_set read_set;
    struct timeval timeout;
    int ret, max_read_fd;
    __u8 buffer[0xFFFF];

    max_read_fd = 0;

    if (thread_args->fd_bulk_out > max_read_fd) // was fd_out
        max_read_fd = thread_args->fd_bulk_out;

    if (verbosity > 1)
        fprintf(stderr, "Starting rx_bulk_thread\n");

    while (!thread_args->stop)
    {
        FD_ZERO(&read_set);
        FD_SET(thread_args->fd_bulk_out, &read_set);
        timeout.tv_sec = 0;
        timeout.tv_usec = 20000; // 20ms

        //if (verbosity > 1)
        //    fprintf(stderr, "Starting bulk select\n");

        memset(buffer, 0, sizeof(buffer));
        ret = select(max_read_fd + 1, &read_set, NULL, NULL, &timeout);

        //if (verbosity > 1)
        //    fprintf(stderr, "Select returned: %d(%m)\n", ret);

        // Timeout
        if (ret == 0)
            continue;

        // Error
        if (ret < 0)
        {
            fprintf(stderr, "Select ERROR");
            break;
        }

        if (verbosity > 1)
            fprintf(stderr, "Starting bulk read\n");

        struct timespec now, then;
        unsigned long sdif, nsdif, tdif;
        clock_gettime(1, &then);

        ret = read(thread_args->fd_bulk_out, buffer, sizeof(buffer));

        clock_gettime(1, &now);
        sdif = now.tv_sec - then.tv_sec;
        nsdif = now.tv_nsec - then.tv_nsec;
        tdif = sdif * 1000000000 + nsdif;

        if (ret >= 0)
        {
            if (verbosity > 1)
                printf("Bulk read %d bytes, time %d\n", ret, tdif);
            rxcounter++;
            if (ret > 0)
            {
                bulk_msg_received_callback(ret, buffer);
            }
        }
        else
        {
            fprintf(stderr, "Read ERROR %d(%m)\n", ret);
            //todo: put a sleep here
        }
    }
    close(thread_args->fd_bulk_out);

    thread_args->fd_bulk_out = -1;

    return NULL;
}

void* tx_int_thread(void* arg)
{
    // Real time driver thread
    struct sched_param sp;
    memset(&sp, 0, sizeof(sp));
    sp.sched_priority = sched_get_priority_min(SCHED_RR) + (sched_get_priority_max(SCHED_RR) - sched_get_priority_min(SCHED_RR)) / 3;
    sched_setscheduler(0, SCHED_RR, &sp);

    struct io_thread_args* thread_args = (struct io_thread_args*)arg;

    //fd_set write_set;
    //int max_write_fd = 0;
    //if (thread_args->fd_int_in > max_write_fd)
    //    max_write_fd = thread_args->fd_int_in;

    while (!thread_args->stop)
    {
        if (txSize == 0)
        {
            struct timespec delay, dummy; // Prevents hogging 100% CPU use
            delay.tv_sec = 0;
            delay.tv_nsec = 200000;
            nanosleep(&delay, &dummy);

            continue;
        }

        fd_set write_set;
        int max_write_fd = 0;
        if (thread_args->fd_int_in > max_write_fd)
            max_write_fd = thread_args->fd_int_in;
        //in loop:
        FD_ZERO(&write_set);
        FD_SET(thread_args->fd_int_in, &write_set);

        int ret = select(max_write_fd + 1, NULL, &write_set, NULL, NULL);

        struct timespec now, then;
        unsigned long sdif, nsdif, tdif;
        clock_gettime(1, &then);

        // Error
        if (ret < 0)
        {
            if (verbosity)
                fprintf(stderr, "select ERROR in int msg response %d!\n", ret);
        }
        else
        {
            ret = write(thread_args->fd_int_in, txBuffer, txSize);

            clock_gettime(1, &now);
            sdif = now.tv_sec - then.tv_sec;
            nsdif = now.tv_nsec - then.tv_nsec;
            tdif = sdif * 1000000000 + nsdif;


            if (verbosity > 1)
                printf("Write status %d (%m), time %d\n", ret, tdif);
            if (ret > 0)
            {
                txcounter++;
                txSize = 0;
            }
            else if (ret < 0)
            {
                //write error
                if (verbosity > 1)
                    printf("Write ERROR");
                //usleep(1);
            }
        }
        txSize = 0; // todo retry once if not successful

        //usleep(100000);

        // txs moved to synchronous send_interrupt_msg_response()

    }

    close(thread_args->fd_int_in);
    thread_args->fd_int_in = -1;

    return NULL;
}

void set_msg_received_callbacks(void (*bulk_msg_callback)(size_t, unsigned char*), void (*interrupt_msg_callback)(size_t, unsigned char*))
{
    interrupt_msg_received_callback = interrupt_msg_callback;
    bulk_msg_received_callback = bulk_msg_callback;
}

int send_interrupt_msg_response(size_t numBytes, unsigned char* buffer)
{
    if (numBytes == 0)
        return -1;

    int busyRetries = 1;
    while (txSize != 0)
    {
        if (busyRetries-- <= 0) 
            return -2;

        struct timespec delay, dummy; // Prevents hogging 100% CPU use
        delay.tv_sec = 0;
        delay.tv_nsec = 100000;
        nanosleep(&delay, &dummy);
    }

    memcpy(txBuffer, buffer, numBytes);
    txSize = numBytes;

    // was previously in tx_int_thread: 
    
}

//eof