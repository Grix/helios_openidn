#ifndef MYTYPES_H_
#define MYTYPES_H_

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>  /* system data type definitions */
#include <sys/socket.h> /* socket specific definitions */
#include <sys/ioctl.h>
#include <netinet/in.h>/* INET constants and stuff */
#include <arpa/inet.h>  /* IP address conversion stuff */
#include <net/if.h>
#include <netdb.h>      /* gethostbyname */
#include <pthread.h>    /* POSIX Threads */
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <vector>
#include <deque>
#include <memory>

#define DRIVER_INACTIVE 0
#define DRIVER_WAVEMODE 1
#define DRIVER_FRAMEMODE 2



/*
Laser configuration
*/
#define ISP_DB25_RED_WAVELENGTH  0x27E
#define ISP_DB25_GREEN_WAVELENGTH 0x214
#define ISP_DB25_BLUE_WAVELENGTH 0x1CC

#define ISP_DB25_USE_U1 0
#define ISP_DB25_USE_U2 0
#define ISP_DB25_USE_U3 0
#define ISP_DB25_USE_U4 0
#define ISP_DB25_U1_WAVELENGTH 0x1BD
#define ISP_DB25_U2_WAVELENGTH 0x241
#define ISP_DB25_U3_WAVELENGTH 0x1E8

using SlicePrimitive = uint8_t;
using SliceType = std::vector<SlicePrimitive>;
//using a deque because the driver might want to use it as a ring buffer

struct TimeSlice {
	SliceType dataChunk;
	unsigned durationUs;
};
using SliceBuf = std::deque<std::shared_ptr<TimeSlice>>;

struct ISPFrameMetadata {
  uint8_t scm; //Service Configuration Match
  uint32_t dur;
  size_t len;
  int once;
  int shutter;
  int isWave;
};
/*
End of Laser configuration
*/

#define IDN_DESCRIPTOR_NOP 0x00
#define IDN_DESCRIPTOR_DRAW_CONTROL_0 0x01
#define IDN_DESCRIPTOR_DRAW_CONTROL_1 0x02
#define IDN_DESCRIPTOR_X 0x03
#define IDN_DESCRIPTOR_Y 0x04
#define IDN_DESCRIPTOR_Z 0x05
#define IDN_DESCRIPTOR_COLOR 0x06
#define IDN_DESCRIPTOR_WAVELENGTH 0x07
#define IDN_DESCRIPTOR_INTENSITY 0x08
#define IDN_DESCRIPTOR_BEAM_BRUSH 0x09


#define IDN_HEADER_CC_MESSAGE 0x40
#define IDN_HEADER_CC_PINGREQUEST 0x08
#define IDN_HEADER_CC_PINGRESPONSE 0x9
#define IDN_HEADER_CC_SCANREQUEST 0x10
#define IDN_HEADER_CC_SCANRESPONSE 0x11
#define IDNCMD_SERVICEMAP_REQUEST           0x12    // Request for unit services
#define IDNCMD_SERVICEMAP_RESPONSE          0x13    // Map of supported services


#define IDN_CH_MSG_HEADER_CHUNK_TYPE_VOID 0x00
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_WAVE 0x01
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_FRAME_CHUNK 0x02
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_FRAME_FIRST_FRAG 0x03
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_OCTET_SEG 0x10
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_OCTET_STR 0x11
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_DIMMER_LVL 0x18
#define IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_FRAME_SQL_FRAG 0xc0

#define IDN_CH_MSG_HEADER_CNL_CCLF_BMASK 0x40
#define IDN_CH_MSG_HEADER_CNL_CH_BMASK 0x3F

#define IDN_CH_CONF_HEADER_CFL_SDM_BMASK 0x30
#define IDN_CH_CONF_HEADER_CFL_SDM_OFFSET 4
#define IDN_CH_CONF_HEADER_CFL_CLOSE_BMASK 0x02
#define IDN_CH_CONF_HEADER_CFL_ROUTING_BMASK 0x01

#define IDN_FRAME_SAM_HEADER_FLAGS_SCM_BMASK 0x30
#define IDN_FRAME_SAM_HEADER_FLAGS_SCM_OFFSET 4
#define IDN_FRAME_SAM_HEADER_FLAGS_ONCE_BMASK 0x01

#define IDN_TAG_CAT_BMASK 0xF000
#define IDN_TAG_CAT_OFFSET 12
#define IDN_TAG_SUB_BMASK 0x0F00
#define IDN_TAG_SUB_OFFSET 8
#define IDN_TAG_ID_BMASK 0x00F0
#define IDN_TAG_ID_OFFSET 4
#define IDN_TAG_PRM_BMASK 0x000F
#define IDN_TAG_WL_BMASK 0x03FF


struct IDNDescriptorTag {
  uint8_t type; 
  uint8_t precision; 
  uint8_t scannerId; 
  uint16_t wavelength; 
  struct IDNDescriptorTag *next;
};

struct IDNChannel {
  uint8_t chId;
  int valid;
  struct sockaddr remote;
  int closed;
  uint8_t sdm;
  uint8_t serviceId;
  uint8_t serviceMode;
  struct timeval timeout;
  IDNDescriptorTag *descriptors;
};

struct ISPDB25Point {
  uint16_t x;
  uint16_t y;
  uint16_t r;
  uint16_t g;
  uint16_t b;
  uint16_t intensity;
  uint16_t shutter;
  uint16_t u1;
  uint16_t u2;
  uint16_t u3;
  uint16_t u4;

  bool operator==(const ISPDB25Point& rhs) {
	  if(this->x != rhs.x)
		  return false;

	  if(this->y != rhs.y)
		  return false;

	  if(this->r != rhs.r)
		  return false;

	  if(this->g != rhs.g)
		  return false;

	  if(this->b != rhs.b)
		  return false;

	  if(this->intensity != rhs.intensity)
		  return false;

	  if(this->shutter != rhs.shutter)
		  return false;

	  if(this->u1 != rhs.u1)
		  return false;

	  if(this->u2 != rhs.u2)
		  return false;

	  if(this->u3 != rhs.u3)
		  return false;

	  if(this->u4 != rhs.u4)
		  return false;

	  return true;
  }
};


struct IDNHeader {
  uint8_t cc;
  uint8_t flags;
  uint16_t seq;
};

struct IDNChannelMsgHeader {
  uint16_t size;
  uint8_t cnl;
  uint8_t chunkType;
  uint32_t timestamp;
};

struct IDNChannelConfHeader {
  uint8_t scwc;
  uint8_t cfl;
  uint8_t serviceId;
  uint8_t serviceMode;
};

struct IDNFrameSampleHeader {
  uint8_t flags;
  uint32_t dur;
};


struct IDNPingResponsePacket {
  IDNHeader head;
};

struct IDNScanResponse
{
    uint8_t structSize;                         // Size of this struct.
    uint8_t protocolVersion;                    // Upper 4 bits: Major; Lower 4 bits: Minor
    uint8_t status;                             // Unit and link status flags
    uint8_t reserved;
    uint8_t unitID[16];                         // [0]: Len, [1]: Cat, [2..Len]: ID, padded with '\0'
    uint8_t hostName[20];                       // Not terminated, padded with '\0'

};

struct IDNScanResponsePacket {
  IDNHeader head;
  IDNScanResponse data;
};
  

struct IDNHDR_SERVICEMAP_RESPONSE
{
    uint8_t structSize;                         // Size of this struct.
    uint8_t entrySize;                          // Size of an entry - sizeof(IDNHDR_SERVICEMAP_ENTRY)
    uint8_t relayEntryCount;                    // Number of relay entries
    uint8_t serviceEntryCount;                  // Number of service entries

    // Followed by the relay table (of relayEntryCount entries)
    // Followed by the service table (of serviceEntryCount entries)

};


struct IDNHDR_SERVICEMAP_ENTRY
{
    uint8_t serviceID;                          // Service: The ID (!=0); Relay: Must be 0
    uint8_t serviceType;                        // The type of the service; Relay: Must be 0
    uint8_t flags;                              // Status flags and options
    uint8_t relayNumber;                        // Service: Root(0)/Relay(>0); Relay: Number (!=0)
    uint8_t name[20];                           // Not terminated, padded with '\0'

};


struct IDNServicemapResponsePacket {
  IDNHeader head;
  IDNHDR_SERVICEMAP_RESPONSE map_response;
  IDNHDR_SERVICEMAP_ENTRY map_entry;
};
 
#endif
