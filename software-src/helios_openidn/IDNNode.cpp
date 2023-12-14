#include "IDNNode.h"


#define MAXBUF 65535
#define IDN_PORT 7255



IDNNode::IDNNode(std::shared_ptr<HWBridge> hwBridge, std::shared_ptr<BEX> bex) {
  channel = (IDNChannel*)malloc(sizeof(struct IDNChannel));
  channel->chId = 0;
  channel->valid = 0;
  channel->closed = 1;
  channel->sdm = 0x00;
  channel->serviceId = 0x00;
  channel->serviceMode = 0x00;  
  channel->descriptors = NULL;

  driverPtr = hwBridge;
  this->bex = bex;
}

IDNNode::~IDNNode() {
  free(channel);
}

void printDescrTag(struct IDNDescriptorTag *tag) {
  printf("TAG: %x, %x, %x, %x\n", tag->type, tag->precision, tag->scannerId, tag->wavelength);
}

unsigned int read_uint8(char* buf, unsigned int len, unsigned int offset, uint8_t* data) {
  if (len >= offset + 1) {
    *data = buf[offset]; 
    return offset + 1;
  }
  printf("[WAR] read err uint8, offset: %i, len: %i\n", offset, len);
  return offset;
}

void print_hex_memory(void *mem, int len) {
  int i;
  unsigned char *p = (unsigned char *)mem;
  for (i=0;i<len;i++) {
    printf("0x%02x ", p[i]);
    if ((i%16==0) && i)
      printf("\n");
  }
  printf("\n");
}

unsigned int read_uint16(char* buf, unsigned int len, unsigned int offset, uint16_t* data) {
  if (len >= offset + 2) {
    *data = ((buf[offset] << 8) & 0xff00) | (buf[offset+1] & 0xff); 
    return offset + 2;
  }
  printf("[WAR] read err uint16, offset: %i, len: %i\n", offset, len);
  return offset;
}

unsigned int read_uint24(char* buf, unsigned int len, unsigned int offset, uint32_t* data) {
  if (len >= offset + 3) {
    *data = ((buf[offset] << 16) & 0x00ff0000) | ((buf[offset+1] << 8) & 0x0000ff00) | (buf[offset+2] & 0x000000ff) ; 
    return offset + 3;
  }
  printf("[WAR] read err uint24, offset: %i, len: %i\n", offset, len);
  return offset;
}

unsigned int read_uint32(char* buf, unsigned int len, unsigned int offset, uint32_t* data) {
  if (len >= offset + 4) {
    *data = ((buf[offset] << 24) & 0xff000000) | ((buf[offset+1] << 16) & 0x00ff0000) | ((buf[offset+2] << 8) & 0x0000ff00) | (buf[offset+3] & 0x000000ff) ; 
    return offset + 4;
  }
  printf("[WAR] read err uint32, offset: %i, len: %i\n", offset, len);
  return offset;
}

unsigned int read_int8(char* buf, unsigned int len, unsigned int offset, int8_t* data) {
  if (len >= offset + 1) {
    *data = buf[offset]; 
    return offset + 1;
  }
  printf("[WAR] read err int8, offset: %i, len: %i\n", offset, len);
  return offset;
}

unsigned int read_int16(char* buf, unsigned int len, unsigned int offset, int16_t* data) {
  if (len >= offset + 2) {
    *data = (buf[offset] << 8) & 0xff00 | (buf[offset+1] & 0xff); 
    return offset + 2;
  }
  printf("[WAR] read err int16, offset: %i, len: %i\n", offset, len);
  return offset;
}

unsigned int IDNNode::buildDictionary(char* buf, unsigned int len, unsigned int offset, uint8_t scwc, IDNDescriptorTag** data){
  IDNDescriptorTag *result = NULL;
  IDNDescriptorTag *last = NULL, *currentDescTag = NULL;
  
  int i;
  for (i = 0; i < scwc*4; i += 2) {
    
	  uint16_t tag;
	  offset = read_uint16(buf, len, offset, &tag);

	  uint16_t category = (tag & IDN_TAG_CAT_BMASK) >> IDN_TAG_CAT_OFFSET;
	  uint16_t sub = (tag & IDN_TAG_SUB_BMASK) >> IDN_TAG_SUB_OFFSET;
	  uint16_t id = (tag & IDN_TAG_ID_BMASK) >> IDN_TAG_ID_OFFSET;
	  uint16_t prm = (tag & IDN_TAG_PRM_BMASK);
	  uint16_t wl = (tag & IDN_TAG_WL_BMASK);

	  switch (category) {
	  case 0: // 0
		  offset += prm * 2; //skip over prm 16-bit words
		  i += prm * 2;
		  break;
	  case 1: // 1
		  if (sub == 0) { //1.0
			  //BREAK TAG
		  }
		  else if (sub == 1) { //1.1
			  //COORDINATE AND COLOR SPACE MODIFIERS
		  }
		  break;
	  case 4: //4
		  if (sub == 0) { //4.0
			  if (id == 0) { //4.0.0
				  //NOP
				  currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
				  currentDescTag->type = IDN_DESCRIPTOR_NOP;
				  //because this tag is not reflected as a data field, subtract it from the count

				  //append Tag
				  if (last != NULL)
					  last->next = currentDescTag;
				  last = currentDescTag;
				  if (result == NULL) result = last;
				  //end append
			  }
			  else if (id == 1) { //4.0.1
				  //Precision Tag
				  if (last != NULL)
					  last->precision++;
			  }
		  }
		  else if (sub == 1) { //4.1
			  currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
			  if (prm == 0) {
				  currentDescTag->type = IDN_DESCRIPTOR_DRAW_CONTROL_0;
			  }
			  else if (prm == 1) {
				  currentDescTag->type = IDN_DESCRIPTOR_DRAW_CONTROL_1;
			  }

			  //apppend Tag
			  if (last != NULL)
				  last->next = currentDescTag;
			  last = currentDescTag;
			  if (result == NULL) result = last;
			  //end append
		  }
		  else if (sub == 2) { //4.2
			  currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
			  if (id == 0) { //4.2.0
				  currentDescTag->type = IDN_DESCRIPTOR_X;
			  }
			  else if (id == 1) { //4.2.1
				  currentDescTag->type = IDN_DESCRIPTOR_Y;
			  }
			  else if (id == 2) { //4.2.2
				  currentDescTag->type = IDN_DESCRIPTOR_Z;
			  }

			  currentDescTag->scannerId = prm;

			  //append Tag
			  if (last != NULL)
				  last->next = currentDescTag;
			  last = currentDescTag;
			  if (result == NULL) result = last;
			  //end append
		  }
		  break;
	  case 5: //5
		  if (sub <= 3) { //5.0 - 5.3  
			  currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
			  currentDescTag->type = IDN_DESCRIPTOR_COLOR;
			  currentDescTag->wavelength = wl;

			  //append Tag
			  if (last != NULL)
				  last->next = currentDescTag;
			  last = currentDescTag;
			  if (result == NULL) result = last;
			  //end append
		  }
		  else if (sub == 12) { //5.12
			  currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
			  if (id == 0) //5.12.0
				  currentDescTag->type = IDN_DESCRIPTOR_WAVELENGTH;
			  if (id == 1) //5.12.1
				  currentDescTag->type = IDN_DESCRIPTOR_INTENSITY;
			  if (id == 2) //5.12.2
				  currentDescTag->type = IDN_DESCRIPTOR_BEAM_BRUSH;

			  //append Tag
			  if (last != NULL)
				  last->next = currentDescTag;
			  last = currentDescTag;
			  if (result == NULL) result = last;
			  //end append
		  }
		  break;
	  }
  }
  last->next = NULL;
  *data = result;
  return offset;
}

void IDNNode::resetChunkBuffer() {
	currentSlicePoints.clear();
	currentSliceTime = this->usPerSlice;
}

void IDNNode::commitChunk() {
	if(currentSlicePoints.size() != 0) {
		std::shared_ptr<DACHWInterface> device = driverPtr->getDevice();
		//if current slice is full, convert it to bytes, put it into
		//the ringbuffer, reset it and reset the currentSliceTime
		std::shared_ptr<TimeSlice> newSlice(new TimeSlice);
		newSlice->dataChunk = device->convertPoints(currentSlicePoints);
		newSlice->durationUs = this->usPerSlice - this->currentSliceTime;
		bex->networkAppendSlice(newSlice);
		resetChunkBuffer();
	}
}

//add a point to the current slice / filter it
void IDNNode::addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata) {
	unsigned pointsPerFrame = metadata.len;
	double pointDuration = (double)metadata.dur / pointsPerFrame;
	double targetPointRate = ((1000000.0*(double)pointsPerFrame) / (double)metadata.dur);
	//TODO: move this somewhere static
	std::shared_ptr<DACHWInterface> device = driverPtr->getDevice();
	double maxDevicePointRate = device->maxPointrate();
	double rateRatio = maxDevicePointRate / targetPointRate;


	//start downsampling if the maximum device pointrate is exceeded
	if(rateRatio < 1.0) {
		//skip point, but act like it was added
		//this keeps rateRatio% of points
		if(skipCounter >= rateRatio) {
			skipCounter += rateRatio;
			skipCounter -= (int)skipCounter;
			currentSliceTime -= pointDuration;
			return;
		}
		skipCounter += rateRatio;
		skipCounter -= (int)skipCounter;
	}

	//here the current slice has enough space,
	//so append the point and decrease
	//the currentSliceTime by the point duration
	currentSlicePoints.push_back(newPoint);
	currentSliceTime -= pointDuration;

	//chunk the points
	if(metadata.isWave) {
		//wave mode chunks into chunks of x ms that don't exceed the maximum amount of
		//bytes that the adapter accepts
		if(currentSliceTime < 0 ||
				device->bytesPerPoint()*(currentSlicePoints.size()+1) > device->maxBytesPerTransmission()) {
			commitChunk();
		}
	} else if(!metadata.isWave && device->maxBytesPerTransmission() != -1) {
		//frame mode does not need to chunk based on duration, but it still needs evenly sized chunks if
		//the device has a point limit, so it tries to equalize them
		unsigned numChunksOfBlockSize = ceil(((double)metadata.len * (double)device->bytesPerPoint())/ (double)device->maxBytesPerTransmission());
		unsigned equalizedSize = ceil((double)metadata.len / (double)numChunksOfBlockSize);

		if(currentSlicePoints.size()+1 > equalizedSize) {
			commitChunk();
		}
	}
}


int IDNNode::processIDNPacket(char* buf, unsigned int len, int sd, struct sockaddr *remote, unsigned int addr_len) {
  unsigned int offset = 0;
  //Parse IDN Header
  IDNHeader* header = (IDNHeader*)malloc(sizeof(IDNHeader));
  offset = read_uint8(buf, len, offset, &(header->cc));
  offset = read_uint8(buf, len, offset, &(header->flags));
  offset = read_uint16(buf, len, offset, &(header->seq));
 
  if (header->cc == IDN_HEADER_CC_PINGREQUEST) {

    printf("IDN_HEADER_CC_PINGREQUEST received!\n");
    //handle Pingrequest

    IDNPingResponsePacket resp = {{IDN_HEADER_CC_PINGRESPONSE,0,htons(header->seq)} };

    sendto(sd, &resp, sizeof(resp), 0, remote, addr_len);



  } else if (header->cc == IDN_HEADER_CC_SCANREQUEST) {

    printf("IDN_HEADER_CC_SCANREQUEST received!\n");
    //handle scan request

    uint8_t unitID[16];
    unitID[0] = 7;
    unitID[1] = 1;
    unitID[2] = mac_address[0];
    unitID[3] = mac_address[1];
    unitID[4] = mac_address[2];
    unitID[5] = mac_address[3];
    unitID[6] = mac_address[4];
    unitID[7] = mac_address[5];


    IDNScanResponsePacket resp = {{IDN_HEADER_CC_SCANRESPONSE,0,htons(header->seq)}, {0x28, 0x01, 0x01, 0x00,
								     {}, {}}};


    memcpy((void*) &resp.data.unitID, unitID, 16);
    memcpy((void*) &resp.data.hostName, "OpenIDN\0\0\0\0\0\0\0\0\0\0\0\0", 20);
    
    sendto(sd, &resp, sizeof(resp), 0, remote, addr_len);



  } else if (header->cc == IDNCMD_SERVICEMAP_REQUEST) {

    printf("IDNCMD_SERVICEMAP_REQUEST received!\n");
    //handle service map request

/* ***********************

typedef struct _IDNHDR_SERVICEMAP_ENTRY
{
    uint8_t serviceID;                          // Service: The ID (!=0); Relay: Must be 0
    uint8_t serviceType;                        // The type of the service; Relay: Must be 0; 0x80 = Standard laser projector
    uint8_t flags;                              // Status flags and options
    uint8_t relayNumber;                        // Service: Root(0)/Relay(>0); Relay: Number (!=0)
    uint8_t name[20];                           // Not terminated, padded with '\0'

} IDNHDR_SERVICEMAP_ENTRY;

******************** */

    IDNServicemapResponsePacket resp = {{IDNCMD_SERVICEMAP_RESPONSE,0,htons(header->seq)}, {4, 24, 0, 1}, {1, 0x80, 0, 0, ""}};
	
	char dacName[20] = "";
	driverPtr->getName(dacName);
	if (dacName[0] == '\0')
		strcpy(dacName, "Unknown DAC\0\0\0\0\0\0\0\0");
	memcpy(resp.map_entry.name, dacName, 20);

    sendto(sd, &resp, sizeof(resp), 0, remote, addr_len);



  } else if (header->cc == IDN_HEADER_CC_MESSAGE) {


    IDNChannelConfHeader* chConfHeader = NULL;
    //Parse Channel Message Header
    IDNChannelMsgHeader* chMsgHeader = (IDNChannelMsgHeader*)malloc(sizeof(IDNChannelMsgHeader));
    offset = read_uint16(buf, len, offset, &(chMsgHeader->size));
    offset = read_uint8(buf, len, offset, &(chMsgHeader->cnl));
    offset = read_uint8(buf, len, offset, &(chMsgHeader->chunkType));
    offset = read_uint32(buf, len, offset, &(chMsgHeader->timestamp));

    /*
    printf("IP:\n");
    int dd;
    for (dd = 0; dd < 14; dd++) {
      printf("locked to: %02X, rec from: %02X\n",  channel->remote.sa_data[dd], remote->sa_data[dd]);
    }
    */

    /*    if (channel->valid == 0)
      printf("unvalid\n");
    */
    //Compare Channel ID
    if (
	(
	 (channel->remote.sa_family == remote->sa_family)
	 && (strncmp(channel->remote.sa_data, remote->sa_data, 14) == 0)
	 && ((chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CH_BMASK) == channel->chId)
	 )

	|| !(channel->valid)

	) {
      channel->chId = (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CH_BMASK);
      channel->valid = 1;
      
      memcpy(channel->remote.sa_data, remote->sa_data, 14);
      channel->remote.sa_family = remote->sa_family;
      
      gettimeofday(&channel->timeout, NULL); 
      //Only support Laser Frame Chunks and Wave Chunks
      if (chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_FRAME_CHUNK || chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_WAVE) {
	
	//Check Channel header
        if (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CCLF_BMASK) {
	  chConfHeader = (IDNChannelConfHeader*)malloc(sizeof(IDNChannelConfHeader));
	  offset = read_uint8(buf, len, offset, &(chConfHeader->scwc));
	  offset = read_uint8(buf, len, offset, &(chConfHeader->cfl));
	  offset = read_uint8(buf, len, offset, &(chConfHeader->serviceId));
	  offset = read_uint8(buf, len, offset, &(chConfHeader->serviceMode));
	  //Routing flag set?
	  if (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_ROUTING_BMASK) {
	    channel->closed = 0; //open channel
	    channel->sdm = (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_SDM_BMASK) >> IDN_CH_CONF_HEADER_CFL_SDM_OFFSET;
	    channel->serviceId = chConfHeader->serviceId;
	    channel->serviceMode = chConfHeader->serviceMode;

	    //has dictionary
	    if (chConfHeader->scwc != 0x00) {
			//build dictionary (and free previous one)
			IDNDescriptorTag* previousDescriptor = channel->descriptors;
			while (previousDescriptor != NULL)
			{
				IDNDescriptorTag* nextDescriptor = previousDescriptor->next;
				free(previousDescriptor);
				previousDescriptor = nextDescriptor;
			}
       		offset = buildDictionary(buf, len, offset, chConfHeader->scwc, &(channel->descriptors));
	    }
	  }
	}

	//channel open
	if (channel->closed == 0) {
	  IDNFrameSampleHeader* samHeader = (IDNFrameSampleHeader*)malloc(sizeof(IDNFrameSampleHeader));
	  offset = read_uint8(buf, len, offset, &(samHeader->flags));
	  offset = read_uint24(buf, len, offset, &(samHeader->dur));

	  //create new frame
	  ISPFrameMetadata frame;
	  frame.scm = (samHeader->flags & IDN_FRAME_SAM_HEADER_FLAGS_SCM_BMASK) >> IDN_FRAME_SAM_HEADER_FLAGS_SCM_OFFSET;
	  frame.once = samHeader->flags & IDN_FRAME_SAM_HEADER_FLAGS_ONCE_BMASK;
	  frame.dur = samHeader->dur;

	  //set Flag for Wave/Frame mode
	  frame.isWave = 0;
	  if (chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_WAVE) 
	    frame.isWave = 1;

	  //set BEX mode
	  if(frame.isWave) {
		  bex->setMode(DRIVER_WAVEMODE);
	  } else {
		  //reset in case there was part of a wave chunk in the buffers already
		  resetChunkBuffer();
		  bex->setMode(DRIVER_FRAMEMODE);
	  }
	  	      
	  //Compare SCM & SDM
	  if (frame.scm != channel->sdm) {
	    printf("[WAR] service data missmatch\n");
	  }
	  
	  unsigned int sampleCnt = 0;

	  std::vector<ISPDB25Point> currentFramePoints;
	  
	  //read sample data
	  while (offset < len) {
	    IDNDescriptorTag* tag = channel->descriptors;
	    //create new point
	    ISPDB25Point newPoint;

	    uint8_t point_cscl = 0;
	    uint8_t point_iscl = 0;
	    

	    //parse sample
	    while (tag != NULL) {
	      if (tag->type == IDN_DESCRIPTOR_NOP) {
		offset++;
	      }
	      if (tag->type == IDN_DESCRIPTOR_INTENSITY) {
		offset++;
	      }
	      if (tag->type == IDN_DESCRIPTOR_DRAW_CONTROL_0 || tag->type == IDN_DESCRIPTOR_DRAW_CONTROL_1) {
		uint8_t hint;
		offset = read_uint8(buf, len, offset, (uint8_t*)&(hint));
		point_cscl = (hint & 0xc0) >> 6;
		point_iscl = (hint & 0x30) >> 4;
	      }
	      if (tag->precision == 1) {	   
		if (tag->type == IDN_DESCRIPTOR_X) {
		  if (tag->scannerId != 0) {
		    offset += 2;
		  } else {
		    offset = read_uint16(buf, len, offset, &(newPoint.x));
		    newPoint.x += 0x8000;
		  }
		} else if (tag->type == IDN_DESCRIPTOR_Y) {
		  if (tag->scannerId != 0) {
		    offset += 2;
		  } else {
		    offset = read_uint16(buf, len, offset, &(newPoint.y));
		    newPoint.y += 0x8000;
		  }
		} else if (tag->type == IDN_DESCRIPTOR_COLOR) {
		  if (tag->wavelength == ISP_DB25_RED_WAVELENGTH) {
		    offset = read_uint16(buf, len, offset, &(newPoint.r));
		  } else if (tag->wavelength == ISP_DB25_GREEN_WAVELENGTH) {
		    offset = read_uint16(buf, len, offset, &(newPoint.g));
		  } else if (tag->wavelength == ISP_DB25_BLUE_WAVELENGTH) {
		    offset = read_uint16(buf, len, offset, &(newPoint.b));
		  } else {
		    offset += 2;
		  }
		}
	      }// end precision 1
	      if (tag->precision == 0) {
		if (tag->type == IDN_DESCRIPTOR_X) {
		  if (tag->scannerId != 0) {
		      offset++;
		  } else {
		    offset = read_uint8(buf, len, offset, (uint8_t*)&(newPoint.x));
		    newPoint.x += 0x80;
		    newPoint.x = ((newPoint.x << 8) & 0xff00) | (newPoint.x & 0x00ff);
		  }
		} else if (tag->type == IDN_DESCRIPTOR_Y) {
		  if (tag->scannerId != 0) {
		      offset++;
		  } else {
		    offset = read_uint8(buf, len, offset, (uint8_t*)&(newPoint.y));
		    newPoint.y += 0x80;
		    newPoint.y = ((newPoint.y << 8) & 0xff00) | (newPoint.y & 0x00ff);
		  }
		} else if (tag->type == IDN_DESCRIPTOR_COLOR) {
		      
		  if (tag->wavelength == ISP_DB25_RED_WAVELENGTH) {
		    offset = read_uint8(buf, len, offset, (uint8_t*)&(newPoint.r));
		    newPoint.r = ((newPoint.r << 8) & 0xff00) | (newPoint.r & 0x00ff);
		  } else if (tag->wavelength == ISP_DB25_GREEN_WAVELENGTH) {
		    offset = read_uint8(buf, len, offset, (uint8_t*)&(newPoint.g));
		    newPoint.g = ((newPoint.g << 8) & 0xff00) | (newPoint.g & 0x00ff);
		  } else if (tag->wavelength == ISP_DB25_BLUE_WAVELENGTH) {
		    offset = read_uint8(buf, len, offset, (uint8_t*)&(newPoint.b));
		    newPoint.b = ((newPoint.b << 8) & 0xff00) | (newPoint.b & 0x00ff);
		  } else {
		    offset++;
		  }
		  
		}
	      } //end precision 0
	      tag = tag->next;
	    } // end while tag
	    //scale colors
	    if (point_cscl > 0) {
	      newPoint.r >>= 2 * point_cscl;
	      newPoint.g >>= 2 * point_cscl;
	      newPoint.b >>= 2 * point_cscl;
	    }
	    //scale intensity
	    if (point_iscl > 0) {
	      newPoint.intensity >>= 2 * point_iscl;
	    }

	    currentFramePoints.push_back(newPoint);
	  } // end while sample data

	  frame.len = currentFramePoints.size();
	  //feed the points to the driver
	  for(auto& point : currentFramePoints) {
	    addPointToSlice(point, frame);
	  }

	  //frame is over
	  //if it was a frame-mode frame, publish
	  //the buffer to the driver
	  if(!frame.isWave) {
		//push final chunk even if it's not finished yet
		commitChunk();
		bex->publishReset();
	  }
	  
	  free(samHeader);
	} // end ch open
	//if there is a channel conf header  
	if (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CCLF_BMASK) {
	  //close channel if flag was set
	  if (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_CLOSE_BMASK) {
	    channel->chId = 0;
	    channel->valid = 1;
	    channel->closed = 1;
	    //TODO make hard stop
	    bex->setMode(DRIVER_INACTIVE);
	  }
	  free(chConfHeader);
	}
      }// end frame chunk
    } // end correct ID
    free(chMsgHeader);
  } // end cc message
  free(header);
  return 0;
 } // end process packet
   
 


void IDNNode::mainNetLoop(int sd) {
  unsigned int len;
  int n;
  char bufin[MAXBUF];
  struct sockaddr_in remote;
  struct timeval now;
  uint64_t sdif, usdif, tdif; 

  len = sizeof(remote);

  while (1) {
    /* read a datagram from the socket (put result in bufin) */

    n = recvfrom(sd,bufin,MAXBUF,0,(struct sockaddr *)&remote,&len);

    //Did we recieve Data?!
    if (n>=0) {
      processIDNPacket(bufin, n, sd, (struct sockaddr *)&remote, len);
    }

    gettimeofday(&now, NULL);
    sdif = now.tv_sec - channel->timeout.tv_sec;
    usdif = now.tv_usec - channel->timeout.tv_usec;
    tdif = sdif*1000000 + usdif;

    if (tdif > 1000000) { //timeout
      //      printf("timeout %d %d", sdif, usdif);
	//TODO make soft (timeout stop)
      bex->setMode(DRIVER_INACTIVE);
      channel->chId = 0;
      channel->valid = 0;
      channel->closed = 1;
    }
  }
}


void* IDNNode::networkThreadStart() {
  printf("Starting Network Thread\n");
  
  sleep(1);
  
  //Setup socket
  int ld;
  struct sockaddr_in sockaddr;
  unsigned int length;
  struct ifreq ifr;

   
  if ((ld = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
    printf("Problem creating socket\n");
    exit(1);
  }

  sockaddr.sin_family = AF_INET;
  sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  sockaddr.sin_port = htons(IDN_PORT);

  if (bind(ld, (struct sockaddr *) &sockaddr, sizeof(sockaddr))<0) {
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
    if (ioctl(ld, SIOCGIFCONF, &ifc) == -1) { 
	printf("Problem ioctl SIOCGIFCONF\n");
	exit(1);
    }

    struct ifreq* it = ifc.ifc_req;
    const struct ifreq* const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    printf("Checking network interfaces ... \n");
    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
	printf("%s: ", it->ifr_name);
        if (ioctl(ld, SIOCGIFFLAGS, &ifr) == 0) {

	   int not_loopback = ! (ifr.ifr_flags & IFF_LOOPBACK);

	  //get IP
	  ioctl(ld, SIOCGIFADDR, &ifr);
	  ip = ntohl(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr);
	  printf("inet %s ", inet_ntoa( ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr ));

	  //get netmask
	  ioctl(ld, SIOCGIFNETMASK, &ifr);
	  netmask = ntohl(((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr.s_addr);
	  printf("netmask %s\n", inet_ntoa( ((struct sockaddr_in *)&ifr.ifr_netmask)->sin_addr ));
  
            if ( not_loopback ) { // don't count loopback
  		//get MAC (EUI-48)
                if (ioctl(ld, SIOCGIFHWADDR, &ifr) == 0) {
                    success = 1;
                    break;
                }
            }
        }
        else {
	  printf("Problem enumerate ioctl SIOCGIFHWADDR\n");
	  exit(1);
    	}
    }

    if (success) {
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
       printf("setsockopt failed\n");

  if (setsockopt (ld, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
       printf("setsockopt failed\n");


  //Start Main Loop 
  mainNetLoop(ld);

  //Close Network Socket
  close(ld);


  return NULL;
}


void IDNNode::setChunkLengthUs(double us) {
	this->usPerSlice = us;
}
