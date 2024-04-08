// -------------------------------------------------------------------------------------------------
//  File IDNServer.cpp
//
//  Copyright (c) 2020-2024 DexLogic, Dirk Apitz. All Rights Reserved.
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
//  04/2019 Dirk Apitz, multithreading
//  01/2024 Dirk Apitz, modifications and integration into OpenIDN
// -------------------------------------------------------------------------------------------------

// Project headers
#include "idn.h"
#include "idn-stream.h"

// Module header
#include "IDNServer.hpp"



// -------------------------------------------------------------------------------------------------
//  Defines
// -------------------------------------------------------------------------------------------------

#define LINK_TIMEOUT_US                     1000000     // Connection/Link inactivity (microseconds)
#define SESSION_TIMEOUT_US                  1000000     // Session inactivity (microseconds)

#define SESSIONSTATE_ATTACHED               1           // Attached to a connection
#define SESSIONSTATE_ABANDONED              0           // No more in use, to be deleted
#define SESSIONSTATE_CLOSING                -1          // No more in use, waiting for output to finish
#define SESSIONSTATE_DETACHED               -2          // In use but not attached to a connection


// Copy a uint8 array field into a uint8 array field
#define STRCPY_FIELD_FROM_FIELD(dstField, srcField)                                         \
    {                                                                                       \
        unsigned cpyCount = sizeof(dstField);                                               \
        if(sizeof(srcField) < cpyCount) cpyCount = sizeof(srcField);                        \
                                                                                            \
        unsigned i = 0;                                                                     \
        uint8_t *dst = dstField;                                                            \
        uint8_t *src = srcField;                                                            \
        for(; (i < cpyCount) && (*src != 0); i++) *dst++ = *src++;                          \
        for(; i < sizeof(dstField); i++) *dst = 0;                                          \
    }

// Copy a unitID field
#define UNITID_COPY(dstField, srcField)                                                     \
    {                                                                                       \
        uint8_t *dst = dstField;                                                            \
        uint8_t *src = srcField;                                                            \
        if(!dst) return;                                                                    \
                                                                                            \
        unsigned idLen = src ? *src++ : 0;                                                  \
        if(idLen >= sizeof(srcField)) idLen = sizeof(srcField) - 1;                         \
        if(idLen >= sizeof(dstField)) idLen = sizeof(dstField) - 1;                         \
        *dst++ = idLen;                                                                     \
                                                                                            \
        if(src) memcpy(dst, src, idLen);                                                    \
                                                                                            \
        unsigned trailCount = sizeof(dstField) - (1 + idLen);                               \
        if(trailCount) memset(&dst[idLen], 0, trailCount);                                  \
    }



// -------------------------------------------------------------------------------------------------
//  Typedefs
// -------------------------------------------------------------------------------------------------

struct _RECV_COOKIE
{
    struct sockaddr_storage remoteAddr;

    int fdSocket;
    uint8_t *sendBufferPtr;
    unsigned sendBufferSize;
};



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
//  Class ODFSession
//
// -------------------------------------------------------------------------------------------------
//  scope: protected
// -------------------------------------------------------------------------------------------------

void ODFSession::openChannel(ODF_ENV *env, IDN_CHANNEL *channel)
{
    Inherited::openChannel(env, channel);

    // Initialize to 'routed'
    pipelineEvents[channel->channelID] = IDNVAL_RTACK_PEVFLG_ROUTED;
}


void ODFSession::closeChannel(ODF_ENV *env, IDN_CHANNEL *channel, bool abortFlag)
{
    Inherited::closeChannel(env, channel, abortFlag);

    // Finalize to 'closed'
    pipelineEvents[channel->channelID] = IDNVAL_RTACK_PEVFLG_CLOSED;
}


SERVICE_HANDLE ODFSession::requestService(ODF_ENV *env, uint8_t channelID, uint8_t serviceID, uint8_t serviceMode)
{
    TracePrinter tpr(env, "ODFSession~requestService");

//FIXME: Find/return service

    if((serviceID == 1) ||
       ((serviceMode == IDNVAL_SMOD_LPGRF_CONTINUOUS) || (serviceMode == IDNVAL_SMOD_LPGRF_DISCRETE)))
    {
        return (SERVICE_HANDLE)idnServer->getLaproService();

    }

    return (SERVICE_HANDLE)0;
}


void ODFSession::releaseService(ODF_ENV *env, uint8_t channelID, SERVICE_HANDLE serviceHnd)
{
    TracePrinter tpr(env, "ODFSession~releaseService");
}


CONDUIT_HANDLE ODFSession::requestConduit(ODF_ENV *env, uint8_t channelID, SERVICE_HANDLE serviceHnd, uint8_t serviceMode)
{
    TracePrinter tpr(env, "ODFSession~requestConduit");

    LaproService *laproService = (LaproService *)serviceHnd;
    if(laproService->getDACBusy()) return (CONDUIT_HANDLE)0;

    laproService->setDACBusy(true);
    return (CONDUIT_HANDLE)laproService;
}


void ODFSession::releaseConduit(ODF_ENV *env, uint8_t channelID, SERVICE_HANDLE serviceHnd, CONDUIT_HANDLE conduitHnd, bool abortFlag)
{
    TracePrinter tpr(env, "ODFSession~releaseConduit");

    LaproService *laproService = (LaproService *)serviceHnd;
    laproService->setDACBusy(false);
}


void ODFSession::input(ODF_ENV *env, SERVICE_HANDLE serviceHnd, CONDUIT_HANDLE conduitHnd, ODF_TAXI_BUFFER *taxiBuffer)
{
    LaproService *laproService = (LaproService *)serviceHnd;
    laproService->processChannelMessage((uint8_t *)(taxiBuffer->payloadPtr), taxiBuffer->payloadLen);
}


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

ODFSession::ODFSession(char *logIdent, IDNServer *idnServer): IDNSession(logIdent)
{
    this->idnServer = idnServer;

    // Session must have been created by a connection
    sessionState = SESSIONSTATE_ATTACHED;

    // No input yet
    inputTimeValid = false;
    inputTimeUS = 0;

    memset(pipelineEvents, 0, sizeof(pipelineEvents));
}


ODFSession::~ODFSession()
{
}


uint16_t ODFSession::clearPipelineEvents(unsigned channelID)
{
    if(channelID >= IDNVAL_CHANNEL_COUNT) return 0;

    uint16_t result = pipelineEvents[channelID];
    pipelineEvents[channelID] = 0;

    return result;
}


void ODFSession::cancelGracefully(ODF_ENV *env)
{
    // Graceful session stop: Process buffered messages, finish active output
    // ---------------------------------------------------------------------------------------------

    TracePrinter tpr(env, "ODFSession~cancelGracefully");

    do
    {

// FIXME: Code for gracefully flushing input and closing services should go here
// In case there is a backlog, the close of the session should be delayed and the
// session cleaned up with the timeout handling

    }
    while(0);

    const char *danglingNote = "";
    if(hasOpenChannels()) danglingNote = ", dangling channels";
    tpr.logDebug("%s|Session: Regular close%s", getLogIdent(), danglingNote);

    // No gracefully closing sink inputs (or error). Abandon (still) open inputs/channels.
    reset(env);

    // No more in use, to be removed/deleted
    sessionState = SESSIONSTATE_ABANDONED;
}


void ODFSession::cancelImmediately(ODF_ENV *env)
{
    // Immediate session stop: Discard buffered messages, abort output
    // ---------------------------------------------------------------------------------------------

    TracePrinter tpr(env, "ODFSession~cancelImmediately");

    tpr.logDebug("%s|Session: Immediate close", getLogIdent());

    // Abandon/Close all inputs/channels
    reset(env);

    // No more in use, to be removed/deleted
    sessionState = SESSIONSTATE_ABANDONED;
}


bool ODFSession::checkTeardown(ODF_ENV *env, uint32_t envTimeUS)
{
    TracePrinter tpr(env, "ODFSession~checkTeardown");

    // Check for already being abandoned
    if(sessionState == SESSIONSTATE_ABANDONED)
    {
        return true;
    }

    if(sessionState == SESSIONSTATE_CLOSING)
    {

// FIXME: Periodic cleanup - invoke closing processing on services for
// Check for timeout and eventually reset

    }

    // In case session had input - check for timeout
    if(inputTimeValid == true)
    {
        int32_t usDiffTime = (int32_t)((uint32_t)envTimeUS - (uint32_t)inputTimeUS);
        if(usDiffTime > SESSION_TIMEOUT_US)
        {
            // Diagnostics
            tpr.logDebug("%s|Session: Inactivity timeout", getLogIdent());

            // Abandon/Close all inputs/channels, invalidate input time
            reset(env);
        }
    }

    // Re-Check session input after timeout handling
    // Note: No further action (just reset / close channels) in case SESSIONSTATE_ATTACHED
    if(inputTimeValid == false)
    {
        // No input, not attached to a connection: Remove/Delete session
        if(sessionState == SESSIONSTATE_DETACHED) return true;
    }

    // Keep session
    return false;
}


void ODFSession::reset(ODF_ENV *env)
{
    Inherited::reset(env);

    inputTimeValid = false;
}


int ODFSession::processChannelMessage(ODF_ENV *env, ODF_TAXI_BUFFER *taxiBuffer)
{
    // Update input time with buffer reception time
    inputTimeUS = taxiBuffer->sourceRefTime;
    inputTimeValid = true;

    // Pass message to base class
    return Inherited::processChannelMessage(env, taxiBuffer);
}



// =================================================================================================
//  Class IDNHelloConnection
//
// -------------------------------------------------------------------------------------------------
//  scope: protected
// -------------------------------------------------------------------------------------------------

void IDNHelloConnection::cleanupClose(ODF_ENV *env)
{
    if(session != (ODFSession *)0)
    {
        // Graceful session stop - flush data, close channels
        session->cancelGracefully(env);
        session = (ODFSession *)0;
    }
}


void IDNHelloConnection::cleanupAbort(ODF_ENV *env)
{
    if(session != (ODFSession *)0)
    {
        // Immediate session stop - flush data, close channels
        session->cancelImmediately(env);
        session = (ODFSession *)0;
    }
}


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

IDNHelloConnection::IDNHelloConnection(struct sockaddr_storage *clientAddr, uint8_t clientGroup, char *logIdent)
{
    // Copy client address
    memcpy(&(this->clientAddr), clientAddr, sizeof(this->clientAddr));
    this->clientGroup = clientGroup;

    // Populate link identification (for log/diagnostics)
    if((logIdent != (char *)0) && (*logIdent != '\0'))
    {
        snprintf(this->logIdent, sizeof(this->logIdent), logIdent);
    }
    else
    {
        snprintf(this->logIdent, sizeof(this->logIdent), "(N/A)");
    }

    // Note: Attribute inputTimeUS initialized with the first invocation of updateInputTime()
    inputTimeUS = 0;

    session = (ODFSession *)0;

    eventFlags = IDNVAL_RTACK_IEVFLG_NEW;
    sequenceValid = 0;
    nextSequence = 0;
    errCntSequence = 0;
}


IDNHelloConnection::~IDNHelloConnection()
{
}


int IDNHelloConnection::clientMatchIDNHello(struct sockaddr_storage *remoteAddr, uint8_t clientGroup)
{
    if(clientGroup != this->clientGroup) return 1;

    return sockaddr_cmp((struct sockaddr *)remoteAddr, (struct sockaddr *)&clientAddr);
}


void IDNHelloConnection::handleLinkClose(ODF_ENV *env)
{
    // Handle a IDN-RT connection close request
    // ---------------------------------------------------------------------------------------------

    TracePrinter tpr(env, "IDNHelloConnection~handleLinkClose");

    tpr.logInfo("%s|Link: Connection close", getLogIdent());

    cleanupClose(env);
}


void IDNHelloConnection::handleTimeout(ODF_ENV *env, int32_t usDiffTime)
{
    // Handle a connection timeout
    // ---------------------------------------------------------------------------------------------

    TracePrinter tpr(env, "IDNHelloConnection~handleTimeout");

    tpr.logInfo("%s|Link: Connection timeout, dt=%d", getLogIdent(), usDiffTime);

    cleanupAbort(env);
}


void IDNHelloConnection::handleAbort(ODF_ENV *env)
{
    // Handle abandoning the connection due to state change
    // ---------------------------------------------------------------------------------------------

    TracePrinter tpr(env, "IDNHelloConnection~handleAbort");

    tpr.logInfo("%s|Link: Connection stop", getLogIdent());

    cleanupAbort(env);
}


bool IDNHelloConnection::checkTeardown(ODF_ENV *env, uint32_t envTimeUS)
{
    // Check for reception timeout.
    int32_t usDiffTime = (int32_t)((uint32_t)envTimeUS - (uint32_t)inputTimeUS);
    if(usDiffTime > LINK_TIMEOUT_US)
    {
        // Connection timeout: Forward to session depending on connection type
        handleTimeout(env, usDiffTime);

        // Delete connection
        return true;
    }

    // Keep connection
    return false;
}



// =================================================================================================
//  Class IDNServer
//
// -------------------------------------------------------------------------------------------------
//  scope: private
// -------------------------------------------------------------------------------------------------

void IDNServer::processStreamMessage(ODF_ENV *env, IDNHelloConnection *connection, ODF_TAXI_BUFFER *taxiBuffer)
{
    TracePrinter tpr(env, "IDNServer~processStreamMessage");

    do
    {
        // Get IDN message header
        IDNHDR_MESSAGE *messageHdr = (IDNHDR_MESSAGE *)(taxiBuffer->payloadPtr);

        // Check message length
        uint16_t totalSize = ntohs(messageHdr->totalSize);
        if(totalSize != taxiBuffer->payloadLen)
        {
            tpr.logWarn("%s|IDN-Stream: Payload length (%u) / message size mismatch (%u)",
                        taxiBuffer->diagString, taxiBuffer->payloadLen, totalSize);
            break;
        }

        // Check contentID (expect channel message)
        uint16_t contentID = ntohs(messageHdr->contentID);
        if((contentID & IDNFLG_CONTENTID_CHANNELMSG) == 0)
        {
            tpr.logWarn("%s|IDN-Stream: Message type mismatch (expected channel message)",
                        taxiBuffer->diagString);
            break;
        }

        // Pass the channel message
        ODFSession *session = connection->getSession();
        if(session != (ODFSession *)0)
        {
            // Pass to IDN session for open/connect/close and sink forwarding. Discard in case of error
            int rcSession = session->processChannelMessage(env, taxiBuffer);
            if(rcSession < 0) env->freeTaxiBuffer(taxiBuffer);

            // No access to the buffer hereafter !!!
            taxiBuffer = (ODF_TAXI_BUFFER *)0;
        }
    }
    while(0);

    // In case the taxi buffer was not passed on - free memory
    if(taxiBuffer != (ODF_TAXI_BUFFER *)0) env->freeTaxiBuffer(taxiBuffer);
}


void IDNServer::processRtPacket(ODF_ENV *env, IDNHelloConnection *connection, ODF_TAXI_BUFFER *taxiBuffer)
{
    TracePrinter tpr(env, "IDNServer~processRtPacket");

    // Get packet header and sequence number
    IDNHDR_PACKET *recvPacketHdr = (IDNHDR_PACKET *)(taxiBuffer->payloadPtr);
    unsigned short recvSequence = ntohs(recvPacketHdr->sequence);

    // Check sequence number
    if(connection->sequenceValid == false)
    {
        // Sequence init
        connection->sequenceValid = true;
        connection->nextSequence = recvSequence + 1;
    }
    else
    {
        // FIXME: Find/Drop duplicates!!!
        // FIXME: Find/Report missing!!!

        // Check for strict monotonic increment
        short seqDiff = (short)((unsigned short)recvSequence - (unsigned short)connection->nextSequence);
        if(seqDiff != 0)
        {
            // Store sequence error
            connection->eventFlags |= IDNVAL_RTACK_IEVFLG_SEQERR_TYPE1;
            if(connection->errCntSequence != (unsigned)-1) connection->errCntSequence++;

            if(seqDiff > 0)
            {
                // Lost messages - possibly either in slow receive or slow file write
                tpr.logWarn("%s|IDN-RT: Out of sequence (%d lost), expected %04X, got %04X",
                            taxiBuffer->diagString, seqDiff, connection->nextSequence, recvSequence);
            }
            else
            {
                // Message sequence messed up
                tpr.logWarn("%s|IDN-RT: Out of sequence (%d early), expected %04X, got %04X",
                           taxiBuffer->diagString, -seqDiff, connection->nextSequence, recvSequence);
            }
        }

        connection->nextSequence = recvSequence + 1;
    }

    // Unwarp message, pass to session
    if(taxiBuffer->payloadLen <= sizeof(IDNHDR_PACKET))
    {
        // In case the packet doesn't contain a message - don't pass to the session
        env->freeTaxiBuffer(taxiBuffer);
    }
    else
    {
        // Adjust buffer (drop IDN-RT packet header)
        taxiBuffer->payloadPtr = (void *)&recvPacketHdr[1];
        taxiBuffer->payloadLen -= sizeof(IDNHDR_PACKET);

        // Check IDN-Stream message and pass to session input
        processStreamMessage(env, connection, taxiBuffer);
    }
}


void IDNServer::destroyConnection(IDNHelloConnection *connection)
{
    connection->LLNode<ConnectionNode>::linkout();
    delete connection;
}


void IDNServer::destroySession(ODFSession *session)
{
    session->LLNode<SessionNode>::linkout();
    delete session;
}


void IDNServer::checkTeardown(ODF_ENV *env, uint32_t envTimeUS)
{
    // First, check all connections. Teardown may leave orphan sessions
    for(LLNode<ConnectionNode> *node = firstConnection; node != (LLNode<ConnectionNode> *)0; )
    {
        IDNHelloConnection *connection = static_cast<IDNHelloConnection *>(node);
        node = node->getNextNode();

        // Check for destruction
        if(connection->checkTeardown(env, envTimeUS)) destroyConnection(connection);
    }

    // Check all sessions
    for(LLNode<SessionNode> *node = firstSession; node != (LLNode<SessionNode> *)0; )
    {
        ODFSession *session = static_cast<ODFSession *>(node);
        node = node->getNextNode();

        // Check for destruction
        if(session->checkTeardown(env, envTimeUS)) destroySession(session);
    }
}


void IDNServer::abandonClients(ODF_ENV *env)
{
    // Unconditionally abandon all connections
    while(firstConnection)
    {
        LLNode<ConnectionNode> *node = firstConnection;
        IDNHelloConnection *connection = static_cast<IDNHelloConnection *>(node);

        // Immediate connection stop, linkout and delete the connection
        connection->handleAbort(env);
        destroyConnection(connection);
    }

    // Unconditionally abandon all sessions
    while(firstSession)
    {
        LLNode<SessionNode> *node = firstSession;
        ODFSession *session = static_cast<ODFSession *>(node);

        // Immediate session stop, linkout and delete the session
/*        if(session->getSessionState() != SESSIONSTATE_ABANDONED)*/ session->cancelImmediately(env);
        destroySession(session);
    }
}


IDNHelloConnection *IDNServer::findConnection(struct sockaddr_storage *remoteAddr, uint8_t clientGroup)
{
    // Find existing connection
    for(LLNode<ConnectionNode> *node = firstConnection; node != (LLNode<ConnectionNode> *)0; node = node->getNextNode())
    {
        IDNHelloConnection *connection = static_cast<IDNHelloConnection *>(node);

        int rcCmp = connection->clientMatchIDNHello(remoteAddr, clientGroup);
        if(rcCmp == 0)
        {
            // Found / Known
            return connection;
        }
        else if(rcCmp < 0)
        {
            // Match function failed
            logError("Socket address compare failed");
        }
    }

    return (IDNHelloConnection *)0;
}


int IDNServer::processRtConnection(ODF_ENV *env, RECV_COOKIE *cookie, IDNHDR_RT_ACKNOWLEDGE *ackRspHdr, int cmd, ODF_TAXI_BUFFER *taxiBuffer)
{
    TracePrinter tpr(env, "IDNServer~processRtConnection");

    // Note: Assert packetlen >= sizeof(IDNHDR_PACKET) --- already checked, would not be here

    uint8_t clientGroup = ((IDNHDR_PACKET *)(taxiBuffer->payloadPtr))->flags & IDNMSK_PKTFLAGS_GROUP;

    // -----------------------------------------------------

    // Find existing connection
    IDNHelloConnection *connection = findConnection(&(cookie->remoteAddr), clientGroup);

    // New connection in case not found
    if(connection == (IDNHelloConnection *)0)
    {
        if((cmd == IDNCMD_RT_CNLMSG_CLOSE) && (taxiBuffer->payloadLen == sizeof(IDNHDR_PACKET)))
        {
            // Empty close command from unconnected client - discard with error
            return IDNVAL_RTACK_ERR_NOT_CONNECTED;
        }

        // Message command or close command with valid channel message payload - open connection
        connection = new IDNHelloConnection(&(cookie->remoteAddr), clientGroup, taxiBuffer->diagString);
        if(connection == (IDNHelloConnection *)0)
        {
            tpr.logError("new(IDNHelloConnection) failed");
            return IDNVAL_RTACK_ERR_GENERIC;
        }

        // Append to the list of connections. Note: Receive time initialized right before packet processing
        connection->LLNode<ConnectionNode>::linkin(&firstConnection);

        // -----------------------------------------------------------------------------------------

        // Create a session. General note: In case of limited resources, maybe use a statically allocated session
        ODFSession *session = new ODFSession(taxiBuffer->diagString, this);
        if(session != (ODFSession *)0)
        {
            // Append to the list of sessions
            session->LLNode<SessionNode>::linkin(&firstSession);
            connection->setSession(session);
        }

        // Diagnostic log
        tpr.logInfo("%s|Link: Connection open, session=%p", connection->getLogIdent(), session);
    }

    // -----------------------------------------------------

    // Assume success
    int8_t result = IDNVAL_RTACK_SUCCESS;

    // Check message payload
    if(taxiBuffer->payloadLen == sizeof(IDNHDR_PACKET))
    {
        // OK, empty packet
    }
    else if(taxiBuffer->payloadLen < (sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_CHANNEL_MESSAGE)))
    {
        // Payload must not be less than a channel message header
        result = IDNVAL_RTACK_ERR_PAYLOAD;
    }
    else
    {
        IDNHDR_PACKET *recvPacketHdr = (IDNHDR_PACKET *)(taxiBuffer->payloadPtr);
        IDNHDR_MESSAGE *messageHdr = (IDNHDR_MESSAGE *)&recvPacketHdr[1];
        if(taxiBuffer->payloadLen < (sizeof(IDNHDR_PACKET) + ntohs(messageHdr->totalSize)))
        {
            // Datagram length does not match total message length
            result = IDNVAL_RTACK_ERR_PAYLOAD;
        }
    }

    // Prefetch channel message channelID (after packet input, header may not be valid any more)
    int channelID = -1;
    if(result == IDNVAL_RTACK_SUCCESS)
    {
        IDNHDR_PACKET *recvPacketHdr = (IDNHDR_PACKET *)(taxiBuffer->payloadPtr);
        IDNHDR_CHANNEL_MESSAGE *channelMessageHdr = (IDNHDR_CHANNEL_MESSAGE *)&recvPacketHdr[1];
        uint16_t contentID = ntohs(channelMessageHdr->contentID);
        channelID = (contentID & IDNMSK_CONTENTID_CHANNELID) >> 8;
    }
    else
    {
        // Message payload error: Discard payload (but process packet for sequence checks)
        taxiBuffer->payloadLen = sizeof(IDNHDR_PACKET);
    }

    // Update connection timout, check/pass packet. Note: No access to the buffer hereafter !!!
    connection->updateInputTime(taxiBuffer->sourceRefTime);
    processRtPacket(env, connection, taxiBuffer);
    taxiBuffer = (ODF_TAXI_BUFFER *)0;

    // ---------------------------------------------------------------------------------------------

    // For acknowledgements: Get/Clear event flags since last acknowledge
    if(ackRspHdr != (IDNHDR_RT_ACKNOWLEDGE *)0)
    {
        ODFSession *session = (ODFSession *)connection->getSession();

        // -------------------

        ackRspHdr->inputEventFlags = htons(connection->eventFlags);
        connection->eventFlags = 0;

        // -------------------

        uint16_t plFlagsOut = 0;

        if(session != (ODFSession *)0)
        {
            plFlagsOut = session->clearPipelineEvents(channelID);
        }

        ackRspHdr->pipelineEventFlags = htons(plFlagsOut);

        // -------------------

        uint8_t stsFlagsOut = 0;

        if(session != (ODFSession *)0)
        {
            if(session->hasOpenChannels()) stsFlagsOut |= IDNVAL_RTACK_STSFLG_SOCNL;
        }

        ackRspHdr->statusFlags = stsFlagsOut;

        // -------------------

        // ackRspHdr->linkQuality =

        // -------------------

        // ackRspHdr->latency =
    }

    // Check for close command (disconnect and graceful session close after message processing)
    if(cmd == IDNCMD_RT_CNLMSG_CLOSE)
    {
        // Connection close: Forward to session and delete the connection.
        connection->handleLinkClose(env);
        destroyConnection(connection);
    }

    // Report successfully passed. Note: Buffer is passed and will not be freed by caller.
    return result;
}


void IDNServer::processCommand(ODF_ENV *env, RECV_COOKIE *cookie, ODF_TAXI_BUFFER *taxiBuffer)
{
    TracePrinter tpr(env, "IDNServer~processCommand");

    // Get packet header and payload length
    IDNHDR_PACKET *recvPacketHdr = (IDNHDR_PACKET *)(taxiBuffer->payloadPtr);
    unsigned recvPayloadLen = taxiBuffer->payloadLen - sizeof(IDNHDR_PACKET);

    // Append client group to remote address (for diagnostics)
    uint8_t clientGroup = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
    unsigned diagLen = strlen(taxiBuffer->diagString);
    char *diagPtr = &taxiBuffer->diagString[diagLen];
    snprintf(diagPtr, sizeof(taxiBuffer->diagString) - diagLen, ",cg<%u>", clientGroup);

    // Dispatch command
    int command = recvPacketHdr->command;
    switch(command)
    {
        case IDNCMD_PING_REQUEST:
        {
            unsigned sendLen = sizeof(IDNHDR_PACKET) + recvPayloadLen;
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Ping: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
                break;
            }

            // IDN-Hello packet header
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
            sendPacketHdr->command = IDNCMD_PING_RESPONSE;
            sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
            sendPacketHdr->sequence = recvPacketHdr->sequence;

            // Copy additional ping data
            memcpy(&sendPacketHdr[1], &recvPacketHdr[1], recvPayloadLen);

            // Send response back to requester
            struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
            socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
            sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
        }
        break;


        case IDNCMD_GROUP_REQUEST:
        {
            unsigned sendLen = sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_GROUP_RESPONSE);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Group: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
                break;
            }

            // IDN-Hello packet header
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
            sendPacketHdr->command = IDNCMD_GROUP_RESPONSE;
            sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
            sendPacketHdr->sequence = recvPacketHdr->sequence;

            // Group response header
            IDNHDR_GROUP_RESPONSE *groupRspHdr = (IDNHDR_GROUP_RESPONSE *)&sendPacketHdr[1];
            memset(groupRspHdr, 0, sizeof(IDNHDR_GROUP_RESPONSE));
            groupRspHdr->structSize = sizeof(IDNHDR_GROUP_RESPONSE);

            // Validate group request header
            IDNHDR_GROUP_REQUEST *groupReqHdr = (IDNHDR_GROUP_REQUEST *)&recvPacketHdr[1];
            if((recvPayloadLen < 1) || (recvPayloadLen < groupReqHdr->structSize))
            {
                groupRspHdr->opCode = IDNVAL_GROUPOP_ERR_REQUEST;
                groupReqHdr = (IDNHDR_GROUP_REQUEST *)0;
            }
            else if(groupReqHdr->structSize != sizeof(IDNHDR_GROUP_REQUEST))
            {
                groupRspHdr->opCode = IDNVAL_GROUPOP_ERR_REQUEST;
                groupReqHdr = (IDNHDR_GROUP_REQUEST *)0;
            }

            // Process operation
            if(groupReqHdr == (IDNHDR_GROUP_REQUEST *)0)
            {
                // Invalid request
            }
            else if(groupReqHdr->opCode == IDNVAL_GROUPOP_GETMASK)
            {
                groupRspHdr->opCode = IDNVAL_GROUPOP_SUCCESS;
            }
/*
            else if(groupReqHdr->opCode == IDNVAL_GROUPOP_SETMASK)
            {
                if(strcmp((char *)(groupReqHdr->authCode), "GroupAuth"))
                {
                    groupRspHdr->opCode = IDNVAL_GROUPOP_ERR_AUTH;
                }
                else
                {
                    clientGroupMask = ntohs(groupReqHdr->groupMask);
                    groupRspHdr->opCode = IDNVAL_GROUPOP_SUCCESS;

                    // Close sessions in case excluded by the client group mask
                    for(int i = 0; i < SESSION_COUNT; i++)
                    {
                        if(checkExcluded(sessions[i].getClientGroup()) &&
                           (sessions[i].isEstablished()))
                        {
                            // Immediate disconnect/reset
                            sessions[i].cancel("group excluded");
                        }
                    }
                }
            }
*/
            else
            {
                // No support for other operations
                groupRspHdr->opCode = IDNVAL_GROUPOP_ERR_OPERATION;
            }

            // Populate response group mask field (always all groups enabled)
// FIXME: current group mask
            groupRspHdr->groupMask = htons(0xFFFF);

            // Send response back to requester
            struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
            socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
            sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
        }
        break;


        case IDNCMD_SCAN_REQUEST:
        {
            unsigned sendLen = sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_SCAN_RESPONSE);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Scan: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
                break;
            }

            // IDN-Hello packet header
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
            sendPacketHdr->command = IDNCMD_SCAN_RESPONSE;
            sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
            sendPacketHdr->sequence = recvPacketHdr->sequence;

            // Scan response header
            IDNHDR_SCAN_RESPONSE *scanRspHdr = (IDNHDR_SCAN_RESPONSE *)&sendPacketHdr[1];
            memset(scanRspHdr, 0, sizeof(IDNHDR_SCAN_RESPONSE));
            scanRspHdr->structSize = sizeof(IDNHDR_SCAN_RESPONSE);
            scanRspHdr->protocolVersion = (unsigned char)((0 << 4) | 1);

            // Populate status
            scanRspHdr->status = IDNFLG_SCAN_STATUS_REALTIME;
            // FIXME: group excluded -> IDNFLG_SCAN_STATUS_EXCLUDED;
            // FIXME: unit issues -> IDNFLG_SCAN_STATUS_OFFLINE;

            // Populate unitID and host name
            UNITID_COPY(scanRspHdr->unitID, unitID);
            STRCPY_FIELD_FROM_FIELD(scanRspHdr->hostName, hostName);

            // Send response back to requester
            struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
            socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
            sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
        }
        break;


        case IDNCMD_SERVICEMAP_REQUEST:
        {
            unsigned sendLen = sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_SERVICEMAP_RESPONSE);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Map: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
                break;
            }

            // IDN-Hello packet header
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
            sendPacketHdr->command = IDNCMD_SERVICEMAP_RESPONSE;
            sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
            sendPacketHdr->sequence = recvPacketHdr->sequence;

            // Service map header setup
            IDNHDR_SERVICEMAP_RESPONSE *mapRspHdr = (IDNHDR_SERVICEMAP_RESPONSE *)&sendPacketHdr[1];
            memset(mapRspHdr, 0, sizeof(IDNHDR_SERVICEMAP_RESPONSE));
            mapRspHdr->structSize = sizeof(IDNHDR_SERVICEMAP_RESPONSE);
            mapRspHdr->entrySize = sizeof(IDNHDR_SERVICEMAP_ENTRY);

            // ---------------------------------------------

// FIXME: Rewrite using service list

            unsigned relayCount = 0;
            unsigned serviceCount = 1;
            sendLen += serviceCount * sizeof(IDNHDR_SERVICEMAP_ENTRY);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Map: Excess table size, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
                break;
            }

            IDNHDR_SERVICEMAP_ENTRY *mapEntry = (IDNHDR_SERVICEMAP_ENTRY *)&mapRspHdr[1];
            mapEntry->serviceID = 1;
            mapEntry->serviceType = IDNVAL_STYPE_LAPRO;
            mapEntry->flags = 0;
            mapEntry->relayNumber = 0;
            memset(mapEntry->name, 0, sizeof(mapEntry->name));
            laproService->getName((char *)(mapEntry->name), sizeof(mapEntry->name));
            if (mapEntry->name[0] == '\0') memcpy(mapEntry->name, "Unknown DAC", 11);

            // Populate header entry counts
            mapRspHdr->relayEntryCount = (uint8_t)relayCount;
            mapRspHdr->serviceEntryCount = (uint8_t)serviceCount;

            // ---------------------------------------------

            // Send response back to requester
            struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
            socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
            sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
        }
        break;


        case IDNCMD_RT_CNLMSG:
        {
            // FIXME: if(checkExcluded(recvPacketHdr->flags)) break;

            // Process message command. Note: If successful, the buffer is passed. No more access!
            int resultCode = processRtConnection(env, cookie, (IDNHDR_RT_ACKNOWLEDGE *)0, IDNCMD_RT_CNLMSG, taxiBuffer);
            if(resultCode == IDNVAL_RTACK_SUCCESS) taxiBuffer = (ODF_TAXI_BUFFER *)0;
        }
        break;


        case IDNCMD_RT_CNLMSG_ACKREQ:
        {
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)0;
            IDNHDR_RT_ACKNOWLEDGE *ackRspHdr = (IDNHDR_RT_ACKNOWLEDGE *)0;
            unsigned sendLen = sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_RT_ACKNOWLEDGE);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Ack: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
            }
            else
            {
                // IDN-Hello packet header
                sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
                sendPacketHdr->command = IDNCMD_RT_ACKNOWLEDGE;
                sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
                sendPacketHdr->sequence = recvPacketHdr->sequence;

                // Acknowledgement response header
                ackRspHdr = (IDNHDR_RT_ACKNOWLEDGE *)&sendPacketHdr[1];
                memset(ackRspHdr, 0, sizeof(IDNHDR_RT_ACKNOWLEDGE));
                ackRspHdr->structSize = sizeof(IDNHDR_RT_ACKNOWLEDGE);
            }

            // Process message command
            int resultCode;
/*
            if(checkExcluded(recvPacketHdr->flags))
            {
                resultCode = IDNVAL_RTACK_ERR_EXCLUDED;
            }
            else
*/
            {
                // Note: If successful, the buffer is passed. No more access!
                resultCode = processRtConnection(env, cookie, ackRspHdr, IDNCMD_RT_CNLMSG, taxiBuffer);
                if(resultCode == IDNVAL_RTACK_SUCCESS) taxiBuffer = (ODF_TAXI_BUFFER *)0;
            }


            if(ackRspHdr != (IDNHDR_RT_ACKNOWLEDGE *)0)
            {
                // Populate acknowledgement result code
                ackRspHdr->resultCode = resultCode;

                // Send acknowledgement to requester
                struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
                socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
                sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
            }
        }
        break;


        case IDNCMD_RT_CNLMSG_CLOSE:
        {
            // FIXME: if(checkExcluded(recvPacketHdr->flags)) break;

            // Process message command. Note: If successful, the buffer is passed. No more access!
            int resultCode = processRtConnection(env, cookie, (IDNHDR_RT_ACKNOWLEDGE *)0, IDNCMD_RT_CNLMSG_CLOSE, taxiBuffer);
            if(resultCode == IDNVAL_RTACK_SUCCESS) taxiBuffer = (ODF_TAXI_BUFFER *)0;
        }
        break;


        case IDNCMD_RT_CNLMSG_CLOSE_ACKREQ:
        {
            IDNHDR_PACKET *sendPacketHdr = (IDNHDR_PACKET *)0;
            IDNHDR_RT_ACKNOWLEDGE *ackRspHdr = (IDNHDR_RT_ACKNOWLEDGE *)0;
            unsigned sendLen = sizeof(IDNHDR_PACKET) + sizeof(IDNHDR_RT_ACKNOWLEDGE);
            if(sendLen > cookie->sendBufferSize)
            {
                tpr.logError("%s|Ack: Insufficient send buffer, %u > %u", taxiBuffer->diagString,
                             sendLen, cookie->sendBufferSize);
            }
            else
            {
                // IDN-Hello packet header
                sendPacketHdr = (IDNHDR_PACKET *)(cookie->sendBufferPtr);
                sendPacketHdr->command = IDNCMD_RT_ACKNOWLEDGE;
                sendPacketHdr->flags = recvPacketHdr->flags & IDNMSK_PKTFLAGS_GROUP;
                sendPacketHdr->sequence = recvPacketHdr->sequence;

                // Acknowledgement response header
                ackRspHdr = (IDNHDR_RT_ACKNOWLEDGE *)&sendPacketHdr[1];
                memset(ackRspHdr, 0, sizeof(IDNHDR_RT_ACKNOWLEDGE));
                ackRspHdr->structSize = sizeof(IDNHDR_RT_ACKNOWLEDGE);
            }

            // Process message / close command
            int resultCode;
/*
            if(checkExcluded(recvPacketHdr->flags))
            {
                resultCode = IDNVAL_RTACK_ERR_EXCLUDED;
            }
            else
*/
            {
                // Note: If successful, the buffer is passed. No more access!
                resultCode = processRtConnection(env, cookie, ackRspHdr, IDNCMD_RT_CNLMSG_CLOSE, taxiBuffer);
                if(resultCode == IDNVAL_RTACK_SUCCESS) taxiBuffer = (ODF_TAXI_BUFFER *)0;
            }

            if(ackRspHdr != (IDNHDR_RT_ACKNOWLEDGE *)0)
            {
                // Populate acknowledgement result code
                ackRspHdr->resultCode = resultCode;

                // Send acknowledgement to requester
                struct sockaddr *sendAddrPtr = (struct sockaddr *)&(cookie->remoteAddr);
                socklen_t sendAddrSize = sizeof(cookie->remoteAddr);
                sendto(cookie->fdSocket, (char *)sendPacketHdr, sendLen, 0, sendAddrPtr, sendAddrSize);
            }
        }
        break;


        case IDNCMD_RT_ABORT:
        {
            // Find connection
            IDNHelloConnection *connection = findConnection(&(cookie->remoteAddr), clientGroup);

            // In case there is a connection - abort
            if(connection != (IDNHelloConnection *)0)
            {
                connection->handleAbort(env);
                destroyConnection(connection);
            }
        }
        break;


        default:
        {
            tpr.logWarn("%s|IDN-Hello: Unknown command %02X", taxiBuffer->diagString, command);
        }
        break;
    }

    // In case the taxi buffer was not passed on - free memory
    if(taxiBuffer != (ODF_TAXI_BUFFER *)0) env->freeTaxiBuffer(taxiBuffer);
}


void IDNServer::receiveUDP(ODF_ENV *env, int sd)
{
    TracePrinter tpr(env, "IDNServer~receiveUDP");

    // Receive the next packet via UDP
    uint32_t usRecvTime;
    do
    {
        uint8_t recvBuffer[0x10000];
        uint8_t sendBuffer[0x10000];

// FIXME: Better use select() / recvfrom() on nonblocking socket

        // Read a datagram from the socket, remember the reception time
        RECV_COOKIE cookie = { 0 };
        socklen_t remoteAddrLen = sizeof(cookie.remoteAddr);
        int recvLen = recvfrom(sd, (char *)recvBuffer, sizeof(recvBuffer), 0, (struct sockaddr *)&(cookie.remoteAddr), &remoteAddrLen);
        usRecvTime = plt_getMonoTimeUS();

        // No packet processing in case of errors (or timeout)
        if (recvLen < 0) break;

        // Populate the receive cookie
        cookie.fdSocket = sd;
        cookie.sendBufferPtr = sendBuffer;
        cookie.sendBufferSize = sizeof(sendBuffer);

        // Populate a taxi buffer
        ODF_TAXI_BUFFER taxiBuffer = { 0 };
        taxiBuffer.payloadLen = recvLen;
        taxiBuffer.payloadPtr = recvBuffer;
        taxiBuffer.sourceRefTime = usRecvTime;

        // Build readable client name (for diagnostics)
        int rcNameInfo = getnameinfo((struct sockaddr *)&(cookie.remoteAddr), remoteAddrLen,
                                     taxiBuffer.diagString, sizeof(taxiBuffer.diagString),
                                     NULL, 0, NI_NUMERICHOST);
        if(rcNameInfo != 0)
        {
            tpr.logError("prep: getnameinfo() failed, rc=%d", rcNameInfo);
            break;
        }

        // Append client port to name (for diagnostics)
        unsigned short addrFamily = ((struct sockaddr *)&(cookie.remoteAddr))->sa_family;
        if(addrFamily == AF_INET)
        {
            unsigned nameLen = strlen(taxiBuffer.diagString);
            char *bufferPtr = &taxiBuffer.diagString[nameLen];
            unsigned bufferSize = sizeof(taxiBuffer.diagString) - nameLen;
            struct sockaddr_in *sockAddrIn = (struct sockaddr_in *)&(cookie.remoteAddr);
            snprintf(bufferPtr, bufferSize, ":%u", ntohs(sockAddrIn->sin_port));
        }
        else if(addrFamily == AF_INET6)
        {
            unsigned nameLen = strlen(taxiBuffer.diagString);
            char *bufferPtr = &taxiBuffer.diagString[nameLen];
            unsigned bufferSize = sizeof(taxiBuffer.diagString) - nameLen;
            struct sockaddr_in6 *sockAddrIn6 = (struct sockaddr_in6 *)&(cookie.remoteAddr);
            snprintf(bufferPtr, bufferSize, ":%u", ntohs(sockAddrIn6->sin6_port));
        }
        else
        {
            tpr.logWarn("%s|prep: Unsupported address family", taxiBuffer.diagString);
            break;
        }

        // -----------------------------------------------------------------------------------------

        // Check minimum packet size
        if(taxiBuffer.payloadLen < sizeof(IDNHDR_PACKET))
        {
            tpr.logWarn("%s|prep: Invalid packet size %u", taxiBuffer.diagString, taxiBuffer.payloadLen);
            break;
        }

        // Process the received packet.
        processCommand(env, &cookie, &taxiBuffer);
    }
    while (0);

    // Check connections and sessions for timeouts or cleanup (after graceful close)
    checkTeardown(env, usRecvTime);
}


void IDNServer::mainNetLoop(ODF_ENV *env, int sd)
{
    while (1)
    {
        receiveUDP(env, sd);

        laproService->checkTimeout();

        struct timespec delay, dummy; // Prevents hogging 100% CPU use
        delay.tv_sec = 0;
        delay.tv_nsec = 500;
        nanosleep(&delay, &dummy);
    }
}


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

IDNServer::IDNServer(std::shared_ptr<LaproService> laproService)
{
    this->laproService = laproService;

    firstConnection = (LLNode<ConnectionNode> *)0;
    firstSession = (LLNode<SessionNode> *)0;

    memset(hostName, 0, HOST_NAME_SIZE);
}


IDNServer::~IDNServer()
{
    // Sanity check
    if(firstConnection != (LLNode<ConnectionNode> *)0)
    {
        logError("IDNServer|dtor: Unfreed connection list %p", firstConnection);
    }
    if(firstSession != (LLNode<SessionNode> *)0)
    {
        logError("IDNServer|dtor: Unfreed session list %p", firstSession);
    }
}


void IDNServer::networkThreadFunc()
{
    printf("Starting Network Thread\n");

    ODF_ENV odfEnv;
    ODF_ENV *env = &odfEnv;


    if(plt_validateMonoTime() < 0)
    {
        printf("Cannot initialize monotonic time\n");
        exit(1);
    }


    // Setup socket
    int ld;
    struct sockaddr_in sockaddr;
    unsigned int length;
    struct ifreq ifr;

    if ((ld = socket(PF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("Problem creating socket\n");
        exit(1);
    }

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockaddr.sin_port = htons(IDNVAL_HELLO_UDP_PORT);

    if (bind(ld, (struct sockaddr *) &sockaddr, sizeof(sockaddr))<0)
    {
        printf("Problem binding\n");
        exit(0);
    }

    struct ifconf ifc;
    char buf[1024];
    int success = 0;

    uint32_t ip;
    uint32_t netmask;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(ld, SIOCGIFCONF, &ifc) == -1)
    {
        printf("Problem ioctl SIOCGIFCONF\n");
        exit(1);
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    printf("Checking network interfaces ... \n");
    for (; it != end; ++it)
    {
        strcpy(ifr.ifr_name, it->ifr_name);
        printf("%s: ", it->ifr_name);

        if (ioctl(ld, SIOCGIFFLAGS, &ifr) == 0)
        {
            int not_loopback = ! (ifr.ifr_flags & IFF_LOOPBACK);

            //get IP
            ioctl(ld, SIOCGIFADDR, &ifr);
            ip = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
            printf("inet %s ", inet_ntoa( ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr ));

            //get netmask
            ioctl(ld, SIOCGIFNETMASK, &ifr);
            netmask = ntohl(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr);
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

    if (success)
    {
        memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
    }

    printf("MAC address / ether ");
    printf("%02x:", mac_address[0]);
    printf("%02x:", mac_address[1]);
    printf("%02x:", mac_address[2]);
    printf("%02x:", mac_address[3]);
    printf("%02x:", mac_address[4]);
    printf("%02x ", mac_address[5]);

    printf("\n\n");


    //Set Socket Timeout
    //This allows to react on cancel requests by control thread every 10us
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10;

    if (setsockopt (ld, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed\n");
    }

    if (setsockopt (ld, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        printf("setsockopt failed\n");
    }


    unitID[0] = 7;
    unitID[1] = 1;
    unitID[2] = mac_address[0];
    unitID[3] = mac_address[1];
    unitID[4] = mac_address[2];
    unitID[5] = mac_address[3];
    unitID[6] = mac_address[4];
    unitID[7] = mac_address[5];

    //gethostname((char *)&hostName, 20);


    // Run main loop
    mainNetLoop(env, ld);

    // Abandon remaining clients
    abandonClients(env);


    // Close network Socket
    close(ld);
}

