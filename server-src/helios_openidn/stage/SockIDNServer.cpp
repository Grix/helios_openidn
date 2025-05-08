// -------------------------------------------------------------------------------------------------
//  File SockIDNServer.cpp
//
//  Copyright (c) 2020-2025 DexLogic, Dirk Apitz. All Rights Reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//
// -------------------------------------------------------------------------------------------------
//  Change History:
//
//  07/2017 Dirk Apitz, created
//  01/2024 Dirk Apitz, modifications and integration into OpenIDN
//  04/2025 Dirk Apitz, independence from network layer through derived classes (Linux/LwIP support)
// -------------------------------------------------------------------------------------------------


// Standard libraries
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

// Platform includes
#include <sys/ioctl.h>
#include <arpa/inet.h>      /* IP address conversion stuff */
#include <net/if.h>
#include <netdb.h>          /* getnameinfo */
#include <fstream>

// Project headers
#include "../shared/ODFTools.hpp"

// Module header
#include "SockIDNServer.hpp"



// -------------------------------------------------------------------------------------------------
//  Defines
// -------------------------------------------------------------------------------------------------

// Copy a uint8 array field into a uint8 array field
#define STRCPY_FIELD_BUF_FROM_FIELD(dstPtr, dstSize, srcField)                              \
    {                                                                                       \
        unsigned cpyCount = dstSize;                                                        \
        if(sizeof(srcField) < cpyCount) cpyCount = sizeof(srcField);                        \
                                                                                            \
        unsigned i = 0;                                                                     \
        uint8_t *dst = dstPtr;                                                              \
        uint8_t *src = srcField;                                                            \
        for(; (i < cpyCount) && (*src != 0); i++) *dst++ = *src++;                          \
        for(; i < dstSize; i++) *dst = 0;                                                   \
    }

// Copy a unitID field
#define UNITID_FIELD_BUF_FROM_FIELD(dstPtr, dstSize, srcField)                                                     \
    {                                                                                       \
        uint8_t *dst = dstPtr;                                                              \
        uint8_t *src = srcField;                                                            \
        if(!dst) return;                                                                    \
                                                                                            \
        unsigned idLen = src ? *src++ : 0;                                                  \
        if(idLen >= sizeof(srcField)) idLen = sizeof(srcField) - 1;                         \
        if(idLen >= dstSize) idLen = dstSize - 1;                                           \
        *dst++ = idLen;                                                                     \
                                                                                            \
        if(src) memcpy(dst, src, idLen);                                                    \
                                                                                            \
        unsigned trailCount = dstSize - (1 + idLen);                                        \
        if(trailCount) memset(&dst[idLen], 0, trailCount);                                  \
    }


// -------------------------------------------------------------------------------------------------
//  Typedefs
// -------------------------------------------------------------------------------------------------

typedef struct 
{
    RECV_COOKIE cookie;                                 // Must be first element!

    struct sockaddr_storage remoteAddr;

    int fdSocket;
    uint8_t *sendBufferPtr;
    unsigned sendBufferSize;

    // Diagnostics
    char diagString[64];                                // A diagnostic string (optional)

} SOCK_RECV_CONTEXT;



// -------------------------------------------------------------------------------------------------
//  Variables
// -------------------------------------------------------------------------------------------------

static int plt_monoValid = 0;
static struct timespec plt_monoRef = { 0 };
static uint32_t plt_monoTimeUS = 0;



// -------------------------------------------------------------------------------------------------
//  Tools
// -------------------------------------------------------------------------------------------------

static int plt_validateMonoTime()
{
    if(!plt_monoValid)
    {
        // Initialize time reference
        if(clock_gettime(CLOCK_MONOTONIC, &plt_monoRef) < 0) return -1;

        // Initialize internal time randomly
        plt_monoTimeUS = (uint32_t)((plt_monoRef.tv_sec * 1000000ul) + (plt_monoRef.tv_nsec / 1000));
        plt_monoValid = 1;
    }

    return 0;
}


static uint32_t plt_getMonoTimeUS()
{
    extern struct timespec plt_monoRef;
    extern uint32_t plt_monoTimeUS;

    // Get current time
    struct timespec tsNow, tsDiff;
    clock_gettime(CLOCK_MONOTONIC, &tsNow);

    // Determine difference to reference time
    if(tsNow.tv_nsec < plt_monoRef.tv_nsec)
    {
        tsDiff.tv_sec = (tsNow.tv_sec - plt_monoRef.tv_sec) - 1;
        tsDiff.tv_nsec = (1000000000 + tsNow.tv_nsec) - plt_monoRef.tv_nsec;
    }
    else
    {
        tsDiff.tv_sec = tsNow.tv_sec - plt_monoRef.tv_sec;
        tsDiff.tv_nsec = tsNow.tv_nsec - plt_monoRef.tv_nsec;
    }

    // Update current time
    plt_monoTimeUS += (uint32_t)((uint64_t)tsDiff.tv_sec * (uint64_t)1000000);
    uint32_t diffMicroInt = tsDiff.tv_nsec / 1000;
    uint32_t diffMicroFrc = tsDiff.tv_nsec % 1000;
    plt_monoTimeUS += diffMicroInt;
    tsDiff.tv_nsec -= diffMicroFrc;

    // Update system time reference. Note: For both (ref and diff) tv_nsec < 1s => sum < 2000000000 (2s)
    plt_monoRef.tv_sec += tsDiff.tv_sec;
    plt_monoRef.tv_nsec += tsDiff.tv_nsec;
    if(plt_monoRef.tv_nsec >= 1000000000)
    {
        plt_monoRef.tv_sec++;
        plt_monoRef.tv_nsec -= 1000000000;
    }

    return plt_monoTimeUS;
}


static int sockaddr_cmp(struct sockaddr *a, struct sockaddr *b)
{
    if(a->sa_family != b->sa_family) return 1;

    if(a->sa_family == AF_INET)
    {
        struct sockaddr_in *aIn = (struct sockaddr_in *)a;
        struct sockaddr_in *bIn = (struct sockaddr_in *)b;
        if(aIn->sin_port != bIn->sin_port) return 1;
        if(aIn->sin_addr.s_addr != bIn->sin_addr.s_addr) return 1;
    }
    else if(a->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *aIn6 = (struct sockaddr_in6 *)a;
        struct sockaddr_in6 *bIn6 = (struct sockaddr_in6 *)b;
        if(aIn6->sin6_port != bIn6->sin6_port) return 1;
        if(memcmp(aIn6->sin6_addr.s6_addr, bIn6->sin6_addr.s6_addr, sizeof(aIn6->sin6_addr.s6_addr)) != 0) return 1;

        if(aIn6->sin6_flowinfo != bIn6->sin6_flowinfo) return 1;
        if(aIn6->sin6_scope_id != bIn6->sin6_scope_id) return 1;
    }
    else
    {
        return -1;
    }

    return 0;
}



// =================================================================================================
//  Struct RECV_COOKIE
//
// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

uint8_t *RECV_COOKIE::getSendBuffer(ODF_ENV *env, unsigned sendBufferSize)
{
    SOCK_RECV_CONTEXT *rxContext = (SOCK_RECV_CONTEXT *)this;

    if(sendBufferSize > rxContext->sendBufferSize) return (uint8_t *)0;

    return rxContext->sendBufferPtr;
}


void RECV_COOKIE::sendResponse(unsigned sendLen)
{
    SOCK_RECV_CONTEXT *rxContext = (SOCK_RECV_CONTEXT *)this;

    struct sockaddr *sendAddrPtr = (struct sockaddr *)&rxContext->remoteAddr;
    socklen_t sendAddrSize = sizeof(rxContext->remoteAddr);
    sendto(rxContext->fdSocket, (char *)rxContext->sendBufferPtr, sendLen, 0, sendAddrPtr, sendAddrSize);
}



// =================================================================================================
//  class ODFEnvironment
//
// -------------------------------------------------------------------------------------------------

class ODFEnvironment: public ODF_ENV
{
    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    virtual ~ODFEnvironment()
    {
    }

    virtual void trace(int traceOp, const char *format, va_list ap)
    {
        // Copy arg pointer. Note: This function ends apCopy, the caller ends ap !!
        va_list apCopy;
        va_copy(apCopy, ap);

        if((traceOp == ODF_TRACEOP_LOG_FATAL) || (traceOp == ODF_TRACEOP_LOG_ERROR))
        {
            printf("\x1B[1;31m");
            vprintf(format, apCopy);
            printf("\x1B[0m");
            printf("\n");
            fflush(stdout);
        }
        else if(traceOp == ODF_TRACEOP_LOG_WARN)
        {
            printf("\x1B[1;33m");
            vprintf(format, apCopy);
            printf("\x1B[0m");
            printf("\n");
            fflush(stdout);
        }
        else
        {
            vprintf(format, apCopy);
            printf("\n");
            fflush(stdout);
        }

        va_end(apCopy);
    }

    virtual uint32_t getClockUS()
    {
        return plt_getMonoTimeUS();
    }
};



// =================================================================================================
//  Class SockIDNHelloConnection
//
// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

SockIDNHelloConnection::SockIDNHelloConnection(RECV_COOKIE *cookie, uint8_t clientGroup, char *logIdent):
    IDNHelloConnection(clientGroup, logIdent)
{
    SOCK_RECV_CONTEXT *rxContext = (SOCK_RECV_CONTEXT *)cookie;

    // Copy client address
    memcpy(&(this->clientAddr), &rxContext->remoteAddr, sizeof(this->clientAddr));
}


SockIDNHelloConnection::~SockIDNHelloConnection()
{
}


int SockIDNHelloConnection::clientMatchIDNHello(RECV_COOKIE *cookie, uint8_t clientGroup)
{
    SOCK_RECV_CONTEXT *rxContext = (SOCK_RECV_CONTEXT *)cookie;

    int rcCmp = sockaddr_cmp((struct sockaddr *)&rxContext->remoteAddr, (struct sockaddr *)&clientAddr);
    if(rcCmp != 0) return rcCmp;

    return Inherited::clientMatchIDNHello(cookie, clientGroup);
}




// =================================================================================================
//  Class OpenIDNServer
//
// -------------------------------------------------------------------------------------------------
//  scope: private
// -------------------------------------------------------------------------------------------------

void SockIDNServer::receiveUDP(ODF_ENV *env, int sd)
{
    TracePrinter tpr(env, "IDNServer~receiveUDP");

    // Receive the next packet via UDP
    uint32_t usRecvTime;
    do
    {
        uint8_t recvBuffer[0x10000];
        uint8_t sendBuffer[0x10000];

// FIXME: Better use select() / recvfrom() on nonblocking socket

        // Create/Populate the receive context/cookie
        SOCK_RECV_CONTEXT rxContext;
        RECV_COOKIE *cookie = &rxContext.cookie;
        memset(&rxContext, 0, sizeof(rxContext));
        rxContext.fdSocket = sd;
        rxContext.sendBufferPtr = sendBuffer;
        rxContext.sendBufferSize = sizeof(sendBuffer);

        // Read a datagram from the socket, remember the reception time
        socklen_t remoteAddrLen = sizeof(rxContext.remoteAddr);
        struct sockaddr *remoteAddrPtr = (struct sockaddr *)&(rxContext.remoteAddr);
        int recvLen = recvfrom(sd, (char *)recvBuffer, sizeof(recvBuffer), 0, remoteAddrPtr, &remoteAddrLen);
        usRecvTime = plt_getMonoTimeUS();

        // No packet processing in case of errors (or timeout)
        if (recvLen < 0) break;

        // Populate a taxi buffer
        ODF_TAXI_BUFFER taxiBuffer;
        memset(&taxiBuffer, 0, sizeof(taxiBuffer));
        taxiBuffer.payloadLen = recvLen;
        taxiBuffer.payloadPtr = recvBuffer;
        taxiBuffer.sourceRefTime = usRecvTime;

        // Build readable client name (for diagnostics)
        int rcNameInfo = getnameinfo(remoteAddrPtr, remoteAddrLen,
                                     cookie->diagString, sizeof(cookie->diagString),
                                     NULL, 0, NI_NUMERICHOST);
        if(rcNameInfo != 0)
        {
            tpr.logError("prep: getnameinfo() failed, rc=%d", rcNameInfo);
            break;
        }

        // Append client port to name (for diagnostics)
        unsigned short addrFamily = remoteAddrPtr->sa_family;
        if(addrFamily == AF_INET)
        {
            unsigned nameLen = strlen(cookie->diagString);
            char *bufferPtr = &cookie->diagString[nameLen];
            unsigned bufferSize = sizeof(cookie->diagString) - nameLen;
            struct sockaddr_in *sockAddrIn = (struct sockaddr_in *)remoteAddrPtr;
            snprintf(bufferPtr, bufferSize, ":%u", ntohs(sockAddrIn->sin_port));
        }
        else if(addrFamily == AF_INET6)
        {
            unsigned nameLen = strlen(cookie->diagString);
            char *bufferPtr = &cookie->diagString[nameLen];
            unsigned bufferSize = sizeof(cookie->diagString) - nameLen;
            struct sockaddr_in6 *sockAddrIn6 = (struct sockaddr_in6 *)remoteAddrPtr;
            snprintf(bufferPtr, bufferSize, ":%u", ntohs(sockAddrIn6->sin6_port));
        }
        else
        {
            tpr.logWarn("%s|prep: Unsupported address family", cookie->diagString);
            break;
        }

        // -----------------------------------------------------------------------------------------

        // Process the received packet.
        processCommand(env, cookie, &taxiBuffer);
    }
    while (0);

    // Check connections and sessions for timeouts or cleanup (after graceful close)
    checkTeardown(env, usRecvTime);
}


void SockIDNServer::mainNetLoop(ODF_ENV *env, int sd)
{
    while (1)
    {
        receiveUDP(env, sd);
    }
}


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

IDNHelloConnection *SockIDNServer::createConnection(RECV_COOKIE *cookie, uint8_t clientGroup, char *logIdent)
{
    return new SockIDNHelloConnection(cookie, clientGroup, logIdent); 
}


void SockIDNServer::getUnitID(uint8_t *fieldPtr, unsigned fieldSize)
{
    UNITID_FIELD_BUF_FROM_FIELD(fieldPtr, fieldSize, unitID);
}


void SockIDNServer::getHostName(uint8_t *fieldPtr, unsigned fieldSize)
{
    STRCPY_FIELD_BUF_FROM_FIELD(fieldPtr, fieldSize, hostName);
}

void SockIDNServer::setHostName(char* name)
{
    for (int i = 0; i < HOST_NAME_SIZE; i++)
        hostName[i] = 0;
    strncpy((char*)hostName, name, HOST_NAME_SIZE);
}

// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

SockIDNServer::SockIDNServer(LLNode<ServiceNode> *firstService):
    IDNServer(firstService)
{
}


SockIDNServer::~SockIDNServer()
{
}


void SockIDNServer::networkThreadFunc()
{
    printf("Starting Network Thread\n");

    ODFEnvironment odfEnv;
    ODF_ENV *env = &odfEnv;


    if(plt_validateMonoTime() < 0)
    {
        printf("Cannot initialize monotonic time\n");
        exit(1);
    }


    // Setup socket
    int ld;
    if ((ld = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("Problem creating socket\n");
        exit(1);
    }

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr.sin_port = htons(IDNVAL_HELLO_UDP_PORT);
    if (bind(ld, (struct sockaddr *) &sockaddr, sizeof(sockaddr))<0)
    {
        printf("Problem binding\n");
        exit(1);
    }

    // Set socket timeout. This allows to react on cancel requests by control thread
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;

    if (setsockopt (ld, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed\n");
        exit(1);
    }

    if (setsockopt (ld, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed\n");
        exit(1);
    }


    struct ifconf ifc;
    char buf[1024];

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(ld, SIOCGIFCONF, &ifc) == -1)
    {
        printf("Problem ioctl SIOCGIFCONF\n");
        exit(1);
    }

    printf("Checking network interfaces ... \n");

    int success = 0;
    struct ifreq ifr;
    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));
    for (; it != end; ++it)
    {
        strcpy(ifr.ifr_name, it->ifr_name);
        printf("%s: ", it->ifr_name);

        if (ioctl(ld, SIOCGIFFLAGS, &ifr) == 0)
        {
            int not_loopback = ! (ifr.ifr_flags & IFF_LOOPBACK);

            //get IP
            ioctl(ld, SIOCGIFADDR, &ifr);
            uint32_t ip = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
            printf("inet %s ", inet_ntoa( ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr ));

            //get netmask
            ioctl(ld, SIOCGIFNETMASK, &ifr);
            uint32_t netmask = ntohl(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr);
            printf("netmask %s\n", inet_ntoa( ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr ));

            if ( not_loopback ) // don't count loopback
            {
                //get MAC (EUI-48)
                if (ioctl(ld, SIOCGIFHWADDR, &ifr) == 0)
                {
                    success = 1;
                    break;
                }
            }
        }
        else
        {
            printf("Problem enumerate ioctl SIOCGIFHWADDR\n");
            exit(1);
        }
    }

    unsigned char mac_address[6] = { 0 };
    if (success)
    {
        memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
    }
    else
    {
        // Backup method of getting MAC address
        std::ifstream file;
        file.open("/sys/class/net/end0/address");
        if (file.is_open())
        {
            char buffer[19] = { 0 };
            file.read(buffer, 18);

            if (file) {
                sscanf(buffer, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx", &mac_address[0], &mac_address[1], &mac_address[2], &mac_address[3], &mac_address[4], &mac_address[5]);
            }
            else {
                printf("Error reading file for MAC address or file has less than 8 bytes\n");
            }

            file.close();
        }
        else
            printf("Error reading file for MAC address\n");
    }

    printf("MAC address / ether ");
    printf("%02x:", mac_address[0]);
    printf("%02x:", mac_address[1]);
    printf("%02x:", mac_address[2]);
    printf("%02x:", mac_address[3]);
    printf("%02x:", mac_address[4]);
    printf("%02x ", mac_address[5]);
    printf("\n\n");


    unitID[0] = 7;
    unitID[1] = 1;
    unitID[2] = mac_address[0];
    unitID[3] = mac_address[1];
    unitID[4] = mac_address[2];
    unitID[5] = mac_address[3];
    unitID[6] = mac_address[4];
    unitID[7] = mac_address[5];


    if (!hostName)
        gethostname((char *)&hostName, 20);

    // Run main loop
    mainNetLoop(env, ld);

    // Abandon remaining clients
    abandonClients(env);


    // Close network Socket
    close(ld);
}

