

#include "idn.h"
#include "idn-stream.h"

#include "Service.hpp"


// -------------------------------------------------------------------------------------------------
//  Tools
// -------------------------------------------------------------------------------------------------

static void printDescrTag(struct IDNDescriptorTag *tag) {
  printf("TAG: %x, %x, %x, %x\n", tag->type, tag->precision, tag->scannerId, tag->wavelength);
}


static unsigned int read_uint8(uint8_t *buf, unsigned int len, unsigned int offset, uint8_t* data) {
  if (len >= offset + 1) {
    *data = buf[offset];
    return offset + 1;
  }
  printf("[WAR] read err uint8, offset: %i, len: %i\n", offset, len);
  return offset;
}


static void print_hex_memory(void *mem, int len) {
  int i;
  unsigned char *p = (unsigned char *)mem;
  for (i=0;i<len;i++) {
    printf("0x%02x ", p[i]);
    if ((i%16==0) && i)
      printf("\n");
  }
  printf("\n");
}


static unsigned int read_uint16(uint8_t *buf, unsigned int len, unsigned int offset, uint16_t* data) {
  if (len >= offset + 2) {
    *data = ((buf[offset] << 8) & 0xff00) | (buf[offset+1] & 0xff);
    return offset + 2;
  }
  printf("[WAR] read err uint16, offset: %i, len: %i\n", offset, len);
  return offset;
}


static unsigned int read_uint24(uint8_t *buf, unsigned int len, unsigned int offset, uint32_t* data) {
  if (len >= offset + 3) {
    *data = ((buf[offset] << 16) & 0x00ff0000) | ((buf[offset+1] << 8) & 0x0000ff00) | (buf[offset+2] & 0x000000ff);
    return offset + 3;
  }
  printf("[WAR] read err uint24, offset: %i, len: %i\n", offset, len);
  return offset;
}


static unsigned int read_uint32(uint8_t *buf, unsigned int len, unsigned int offset, uint32_t* data) {
  if (len >= offset + 4) {
    *data = ((buf[offset] << 24) & 0xff000000) | ((buf[offset+1] << 16) & 0x00ff0000) | ((buf[offset+2] << 8) & 0x0000ff00) | (buf[offset+3] & 0x000000ff);
    return offset + 4;
  }
  printf("[WAR] read err uint32, offset: %i, len: %i\n", offset, len);
  return offset;
}


static unsigned int read_int8(uint8_t *buf, unsigned int len, unsigned int offset, int8_t* data) {
  if (len >= offset + 1) {
    *data = buf[offset];
    return offset + 1;
  }
  printf("[WAR] read err int8, offset: %i, len: %i\n", offset, len);
  return offset;
}


static unsigned int read_int16(uint8_t *buf, unsigned int len, unsigned int offset, int16_t* data) {
  if (len >= offset + 2) {
    *data = (buf[offset] << 8) & 0xff00 | (buf[offset+1] & 0xff);
    return offset + 2;
  }
  printf("[WAR] read err int16, offset: %i, len: %i\n", offset, len);
  return offset;
}



// =================================================================================================
//  Class LaproService
//
// -------------------------------------------------------------------------------------------------
//  scope: private
// -------------------------------------------------------------------------------------------------

unsigned int LaproService::buildDictionary(uint8_t *buf, unsigned int len, unsigned int offset, uint8_t scwc, IDNDescriptorTag** data)
{
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

    switch(category) {
    case 0: // 0
          offset += prm*2; //skip over prm 16-bit words
      i += prm*2;
        break;
    case 1: // 1
      if (sub == 0) { //1.0
    //BREAK TAG
      } else if (sub = 1) { //1.1
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
     } else if (id == 1) { //4.0.1
      //Precision Tag
      if (last != NULL)
        last->precision++;
    }
       } else if (sub == 1) { //4.1
    currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
    if (prm == 0) {
      currentDescTag->type = IDN_DESCRIPTOR_DRAW_CONTROL_0;
        } else if (prm == 1) {
      currentDescTag->type = IDN_DESCRIPTOR_DRAW_CONTROL_1;
        }

    //apppend Tag
    if (last != NULL)
      last->next = currentDescTag;
    last = currentDescTag;
    if (result == NULL) result = last;
    //end append
       } else if (sub == 2) { //4.2
    currentDescTag = (IDNDescriptorTag*)calloc(1, sizeof(IDNDescriptorTag));
    if (id == 0) { //4.2.0
      currentDescTag->type = IDN_DESCRIPTOR_X;
        } else if (id == 1) { //4.2.1
      currentDescTag->type = IDN_DESCRIPTOR_Y;
        } else if (id == 2) { //4.2.2
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
      } else if (sub == 12) { //5.12
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


void LaproService::resetChunkBuffer()
{
    currentSlicePoints.clear();
    currentSliceTime = this->usPerSlice;
}


void LaproService::commitChunk()
{
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
void LaproService::addPointToSlice(ISPDB25Point newPoint, ISPFrameMetadata metadata)
{
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


// -------------------------------------------------------------------------------------------------
//  scope: public
// -------------------------------------------------------------------------------------------------

LaproService::LaproService(std::shared_ptr<HWBridge> hwBridge, std::shared_ptr<BEX> bex)
{
    driverPtr = hwBridge;
    this->bex = bex;

    dacBusyFlag = false;

    channel = (IDNChannel*)malloc(sizeof(struct IDNChannel));
    channel->chId = 0;
    channel->valid = 0;
    channel->closed = 1;
    channel->sdm = 0x00;
    channel->serviceId = 0x00;
    channel->serviceMode = 0x00;
    channel->descriptors = NULL;
}


LaproService::~LaproService()
{
    free(channel);
}


void LaproService::getName(char *nameBufferPtr, unsigned nameBufferSize)
{
    driverPtr->getDevice()->getName(nameBufferPtr, nameBufferSize);
}


void LaproService::checkTimeout()
{
    struct timeval now;
    gettimeofday(&now, NULL);
    uint64_t sdif = now.tv_sec - channel->timeout.tv_sec;
    uint64_t usdif = now.tv_usec - channel->timeout.tv_usec;
    uint64_t tdif = sdif*1000000 + usdif;

    // FIXME: timeout
    if (tdif > 1000000)
    {
        //      printf("timeout %d %d", sdif, usdif);
        //TODO make soft (timeout stop)
        bex->setMode(DRIVER_INACTIVE);
        channel->chId = 0;
        channel->valid = 0;
        channel->closed = 1;
    }
}


void LaproService::processChannelMessage(uint8_t *recvBuffer, unsigned recvLen)
{
    unsigned int offset = 0;

    //Parse Channel Message Header
    IDNChannelMsgHeader* chMsgHeader = (IDNChannelMsgHeader*)malloc(sizeof(IDNChannelMsgHeader));
    offset = read_uint16(recvBuffer, recvLen, offset, &(chMsgHeader->size));
    offset = read_uint8(recvBuffer, recvLen, offset, &(chMsgHeader->cnl));
    offset = read_uint8(recvBuffer, recvLen, offset, &(chMsgHeader->chunkType));
    offset = read_uint32(recvBuffer, recvLen, offset, &(chMsgHeader->timestamp));

    /*
    printf("IP:\n");
    int dd;
    for (dd = 0; dd < 14; dd++) {
        printf("locked to: %02X, rec from: %02X\n",  channel->remote.sa_data[dd], remote->sa_data[dd]);
    }
    */

    /*
        if (channel->valid == 0)
        printf("unvalid\n");
    */

    // Compare Channel ID
    if (!(channel->valid) || ((chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CH_BMASK) == channel->chId))
    {
        channel->chId = (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CH_BMASK);
        channel->valid = 1;

        gettimeofday(&channel->timeout, NULL);

        //Only support Laser Frame Chunks and Wave Chunks
        if (chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_FRAME_CHUNK || chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_WAVE) 
        {
            //Check Channel header
            IDNChannelConfHeader* chConfHeader = NULL;
            if (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CCLF_BMASK)
            {
                chConfHeader = (IDNChannelConfHeader*)malloc(sizeof(IDNChannelConfHeader));
                offset = read_uint8(recvBuffer, recvLen, offset, &(chConfHeader->scwc));
                offset = read_uint8(recvBuffer, recvLen, offset, &(chConfHeader->cfl));
                offset = read_uint8(recvBuffer, recvLen, offset, &(chConfHeader->serviceId));
                offset = read_uint8(recvBuffer, recvLen, offset, &(chConfHeader->serviceMode));

                //Routing flag set?
                if (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_ROUTING_BMASK)
                {
                    channel->closed = 0; //open channel
                    channel->sdm = (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_SDM_BMASK) >> IDN_CH_CONF_HEADER_CFL_SDM_OFFSET;
                    channel->serviceId = chConfHeader->serviceId;
                    channel->serviceMode = chConfHeader->serviceMode;

                    // Has dictionary
                    if (chConfHeader->scwc != 0x00)
                    {
                        //build dictionary (and free previous one)
                        IDNDescriptorTag* previousDescriptor = channel->descriptors;
                        while (previousDescriptor != NULL)
                        {
                            IDNDescriptorTag* nextDescriptor = previousDescriptor->next;
                            free(previousDescriptor);
                            previousDescriptor = nextDescriptor;
                        }

                        offset = buildDictionary(recvBuffer, recvLen, offset, chConfHeader->scwc, &(channel->descriptors));
                    }
                }
            }

            //channel open
            if (channel->closed == 0)
            {
                IDNFrameSampleHeader* samHeader = (IDNFrameSampleHeader*)malloc(sizeof(IDNFrameSampleHeader));
                offset = read_uint8(recvBuffer, recvLen, offset, &(samHeader->flags));
                offset = read_uint24(recvBuffer, recvLen, offset, &(samHeader->dur));

                // Create new frame
                ISPFrameMetadata frame;
                frame.scm = (samHeader->flags & IDN_FRAME_SAM_HEADER_FLAGS_SCM_BMASK) >> IDN_FRAME_SAM_HEADER_FLAGS_SCM_OFFSET;
                frame.once = samHeader->flags & IDN_FRAME_SAM_HEADER_FLAGS_ONCE_BMASK;
                frame.dur = samHeader->dur;

                // Set Flag for Wave/Frame mode
                frame.isWave = 0;
                if (chMsgHeader->chunkType == IDN_CH_MSG_HEADER_CHUNK_TYPE_LASER_WAVE) frame.isWave = 1;

                //set BEX mode
                if(frame.isWave)
                {
                    bex->setMode(DRIVER_WAVEMODE);
                }
                else
                {
                    //reset in case there was part of a wave chunk in the buffers already
                    resetChunkBuffer();
                    bex->setMode(DRIVER_FRAMEMODE);
                }

                //Compare SCM & SDM
                if (frame.scm != channel->sdm)
                {
                    printf("[WAR] service data missmatch\n");
                }

                unsigned int sampleCnt = 0;
                std::vector<ISPDB25Point> currentFramePoints;

                //read sample data
                while (offset < recvLen)
                {
                    IDNDescriptorTag* tag = channel->descriptors;
                    //create new point
                    ISPDB25Point newPoint;

                    uint8_t point_cscl = 0;
                    uint8_t point_iscl = 0;

                    //parse sample
                    while (tag != NULL)
                    {
                        if (tag->type == IDN_DESCRIPTOR_NOP)
                        {
                            offset++;
                        }
                        if (tag->type == IDN_DESCRIPTOR_INTENSITY)
                        {
                            offset++;
                        }
                        if (tag->type == IDN_DESCRIPTOR_DRAW_CONTROL_0 || tag->type == IDN_DESCRIPTOR_DRAW_CONTROL_1)
                        {
                            uint8_t hint;
                            offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(hint));
                            point_cscl = (hint & 0xc0) >> 6;
                            point_iscl = (hint & 0x30) >> 4;
                        }
                        if (tag->precision == 1)
                        {
                            if (tag->type == IDN_DESCRIPTOR_X)
                            {
                                if (tag->scannerId != 0)
                                {
                                    offset += 2;
                                }
                                else
                                {
                                    offset = read_uint16(recvBuffer, recvLen, offset, &(newPoint.x));
                                    newPoint.x += 0x8000;
                                }
                            }
                            else if (tag->type == IDN_DESCRIPTOR_Y)
                            {
                                if (tag->scannerId != 0)
                                {
                                    offset += 2;
                                }
                                else
                                {
                                    offset = read_uint16(recvBuffer, recvLen, offset, &(newPoint.y));
                                    newPoint.y += 0x8000;
                                }
                            }
                            else if (tag->type == IDN_DESCRIPTOR_COLOR)
                            {
                                if (tag->wavelength == ISP_DB25_RED_WAVELENGTH)
                                {
                                    offset = read_uint16(recvBuffer, recvLen, offset, &(newPoint.r));
                                }
                                else if (tag->wavelength == ISP_DB25_GREEN_WAVELENGTH)
                                {
                                    offset = read_uint16(recvBuffer, recvLen, offset, &(newPoint.g));
                                }
                                else if (tag->wavelength == ISP_DB25_BLUE_WAVELENGTH)
                                {
                                    offset = read_uint16(recvBuffer, recvLen, offset, &(newPoint.b));
                                }
                                else
                                {
                                    offset += 2;
                                }
                            }
                        }// end precision 1

                        if (tag->precision == 0)
                        {
                            if (tag->type == IDN_DESCRIPTOR_X)
                            {
                                if (tag->scannerId != 0)
                                {
                                    offset++;
                                }
                                else
                                {
                                    offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(newPoint.x));
                                    newPoint.x += 0x80;
                                    newPoint.x = ((newPoint.x << 8) & 0xff00) | (newPoint.x & 0x00ff);
                                }
                            }
                            else if (tag->type == IDN_DESCRIPTOR_Y)
                            {
                                if (tag->scannerId != 0)
                                {
                                    offset++;
                                }
                                else
                                {
                                    offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(newPoint.y));
                                    newPoint.y += 0x80;
                                    newPoint.y = ((newPoint.y << 8) & 0xff00) | (newPoint.y & 0x00ff);
                                }
                            }
                            else if (tag->type == IDN_DESCRIPTOR_COLOR)
                            {
                                if (tag->wavelength == ISP_DB25_RED_WAVELENGTH)
                                {
                                    offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(newPoint.r));
                                    newPoint.r = ((newPoint.r << 8) & 0xff00) | (newPoint.r & 0x00ff);
                                }
                                else if (tag->wavelength == ISP_DB25_GREEN_WAVELENGTH)
                                {
                                    offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(newPoint.g));
                                    newPoint.g = ((newPoint.g << 8) & 0xff00) | (newPoint.g & 0x00ff);
                                }
                                else if (tag->wavelength == ISP_DB25_BLUE_WAVELENGTH)
                                {
                                    offset = read_uint8(recvBuffer, recvLen, offset, (uint8_t*)&(newPoint.b));
                                    newPoint.b = ((newPoint.b << 8) & 0xff00) | (newPoint.b & 0x00ff);
                                }
                                else
                                {
                                    offset++;
                                }
                            }
                        } //end precision 0

                        tag = tag->next;

                    } // end while tag

                    //scale colors
                    if (point_cscl > 0)
                    {
                        newPoint.r >>= 2 * point_cscl;
                        newPoint.g >>= 2 * point_cscl;
                        newPoint.b >>= 2 * point_cscl;
                    }

                    //scale intensity
                    if (point_iscl > 0)
                    {
                        newPoint.intensity >>= 2 * point_iscl;
                    }

                    currentFramePoints.push_back(newPoint);
                } // end while sample data

                frame.len = currentFramePoints.size();

                //feed the points to the driver
                for(auto& point : currentFramePoints)
                {
                    addPointToSlice(point, frame);
                }

                //frame is over
                //if it was a frame-mode frame, publish
                //the buffer to the driver
                if(!frame.isWave)
                {
                    //push final chunk even if it's not finished yet
                    commitChunk();
                    bex->publishReset();
                }

                free(samHeader);
            } // end ch open

            //if there is a channel conf header
            if (chMsgHeader->cnl & IDN_CH_MSG_HEADER_CNL_CCLF_BMASK)
            {
                //close channel if flag was set
                if (chConfHeader->cfl & IDN_CH_CONF_HEADER_CFL_CLOSE_BMASK)
                {
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
}

