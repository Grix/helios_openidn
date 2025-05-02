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
#include <string>
#include <stdexcept>

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
  uint32_t dur;
  size_t len;
  int once;
  int shutter;
  int isWave;
};
/*
End of Laser configuration
*/



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


 
#endif
