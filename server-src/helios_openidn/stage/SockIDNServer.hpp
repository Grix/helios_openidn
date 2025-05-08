// -------------------------------------------------------------------------------------------------
//  File SockIDNServer.hpp
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


#ifndef SOCKIDNSERVER_HPP
#define SOCKIDNSERVER_HPP


// Project headers
#include "../server/IDNServer.hpp"



// -------------------------------------------------------------------------------------------------
//  Defines
// -------------------------------------------------------------------------------------------------

// Shortcuts for struct field sizes
#define UNITID_SIZE sizeof(((IDNHDR_SCAN_RESPONSE *)0)->unitID)
#define HOST_NAME_SIZE sizeof(((IDNHDR_SCAN_RESPONSE *)0)->hostName)


// -------------------------------------------------------------------------------------------------
//  Classes
// -------------------------------------------------------------------------------------------------

class SockIDNHelloConnection: public IDNHelloConnection
{
    typedef IDNHelloConnection Inherited;

    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    private:

    struct sockaddr_storage clientAddr;             // The network address of the client


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    SockIDNHelloConnection(RECV_COOKIE *cookie, uint8_t clientGroup, char *logIdent);
    virtual ~SockIDNHelloConnection();

    // -- Inherited Members -------------
    virtual int clientMatchIDNHello(RECV_COOKIE *cookie, uint8_t clientGroup);
};



class SockIDNServer: public IDNServer
{
    typedef IDNServer Inherited;

    // ------------------------------------------ Members ------------------------------------------

    ////////////////////////////////////////////////////////////////////////////////////////////////
    private:

    void receiveUDP(ODF_ENV *env, int sd);
    void mainNetLoop(ODF_ENV *env, int sd);


    ////////////////////////////////////////////////////////////////////////////////////////////////
    protected:

    uint8_t unitID[UNITID_SIZE];                    // The unitID to report on scan requests
    uint8_t hostName[HOST_NAME_SIZE] = { 0 };               // The host name to report on scan requests

    // -- Inherited Members -------------
    virtual IDNHelloConnection *createConnection(RECV_COOKIE *cookie, uint8_t clientGroup, char *logIdent);
    virtual void getUnitID(uint8_t *fieldPtr, unsigned fieldSize);
    virtual void getHostName(uint8_t *fieldPtr, unsigned fieldSize);


    ////////////////////////////////////////////////////////////////////////////////////////////////
    public:

    SockIDNServer(LLNode<ServiceNode> *firstService);
    virtual ~SockIDNServer();

    void networkThreadFunc();
    virtual void setHostName(char* name);
};


#endif
