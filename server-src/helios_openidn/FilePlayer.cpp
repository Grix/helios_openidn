#include "FilePlayer.hpp"

//extern ManagementInterface* management;

void* outputLoopThread(void* args)
{
    FilePlayer* player = (FilePlayer*)args;

    player->outputLoop();
}

FilePlayer::FilePlayer()
{
    if (pthread_create(&outputThread, NULL, &outputLoopThread, this) != 0) {
        printf("ERROR CREATING FILEPLAYER THREAD\n");
    }
}

void FilePlayer::start()
{
	if (state == FILEPLAYER_STATE_STOP)
	{
		// Reset
		frame = 0;
	}
	state = FILEPLAYER_STATE_PLAY;
}

void FilePlayer::stop()
{
	state = FILEPLAYER_STATE_STOP;
    std::lock_guard<std::mutex> lock(threadLock);
    queue.clear();
}

void FilePlayer::pause()
{
	state = FILEPLAYER_STATE_PAUSE;
}

int FilePlayer::playFile(std::string filename)
{
	currentFile = filename;

    if (devices->empty())
    {
        logWarn("[IDTF] Attempted to play file when no devices are connected");
        return -1;
    }

    // Determine which palette to use TODO
    //unsigned palOption = options & IDTFOPT_PALETTE_MASK;
    unsigned long* currentPalette = ildaDefaultPalette;
    //if ((palOption == 0) || (palOption == IDTFOPT_PALETTE_IDTF_DEFAULT)) {}
    //else if (palOption == IDTFOPT_PALETTE_ILDA_STANDARD) { currentPalette = ildaStandardPalette; }
    //else { logError("[IDTF] Invalid palette option"); return -1; }*/

    const float xScale = 0xFFFF; //(options & IDTFOPT_MIRROR_X) ?  -xyScale : 0xFFFF;
    const float yScale = 0xFFFF; //(options & IDTFOPT_MIRROR_Y) ?  -xyScale : 0xFFFF;

    // Open the passed file
    FILE* fpIDTF = fopen(filename.c_str(), "rb");
    if (!fpIDTF)
    {
        logError("[IDTF] %s: Cannot open file (errno: %d)", filename.c_str(), errno);
        return -1;
    }

    long filePos;
    uint8_t ilda[4];

    // Sanity check - Read signature of first section
    filePos = ftell(fpIDTF);
    fread(ilda, sizeof(ilda), 1, fpIDTF);

    // Check for EOF and the signature of first section
    if (feof(fpIDTF) || !((ilda[0] == 'I') && (ilda[1] == 'L') && (ilda[2] == 'D') && (ilda[3] == 'A')))
    {
        logError("[IDTF] %s: Not an IDTF file", filename.c_str());
        fclose(fpIDTF);
        return -1;
    }

    // Rewind for loop start
    fseek(fpIDTF, filePos, SEEK_SET);


    // -------------------------------------------------------------------------
    // OK - Read the file
    // -------------------------------------------------------------------------

    int result = 0;
    while (1)
    {
        // Read section signature. Silently abort in case of (incorrect) EOF.
        filePos = ftell(fpIDTF);
        fread(ilda, sizeof(ilda), 1, fpIDTF);
        if (feof(fpIDTF)) 
            break;

        // Some systems use this signature before appending further (non-IDTF) data...
        if ((ilda[0] == 0) && (ilda[1] == 0) && (ilda[2] == 0) && (ilda[3] == 0)) 
            break;

        // Check for IDTF section signature
        if (!((ilda[0] == 'I') && (ilda[1] == 'L') && (ilda[2] == 'D') && (ilda[3] == 'A')))
        {
            logError("[IDTF] %s: Bad section signature at pos 0x%08X", filename.c_str(), filePos);
            result = -1;
            break;
        }

        // Skip reserved bytes and read format code
        fgetc(fpIDTF);
        fgetc(fpIDTF);
        fgetc(fpIDTF);
        uint8_t formatCode = (uint8_t)fgetc(fpIDTF);
        if ((result = checkEOF(fpIDTF, "Header")) != 0) 
            break;

        // Read data set name (frame name or color palette name) and company name
        uint8_t dataSetName[8], companyName[8];
        fread(dataSetName, 8, 1, fpIDTF);
        fread(companyName, 8, 1, fpIDTF);
        if ((result = checkEOF(fpIDTF, "Header")) != 0) 
            break;

        //logInfo("dataSetName: %8.8s", dataSetName);
        //logInfo("companyName: %8.8s", companyName);

        // Read record count and data set number (frame number or color palette number)
        uint16_t recordCnt = (uint16_t)readShort(fpIDTF);
        uint16_t dataSetNumber = (uint16_t)readShort(fpIDTF);
        if ((result = checkEOF(fpIDTF, "Header")) != 0) 
            break;

        // Read data set count (not used) and head number (not used)
        uint16_t dataSetCnt = (uint16_t)readShort(fpIDTF);
        uint8_t headNumber = (uint8_t)fgetc(fpIDTF);
        fgetc(fpIDTF);

        //logInfo("Format: %d, Records: %d, Set number: %d, Set count: %d, Head: %d",
        //        formatCode, recordCnt, dataSetNumber, dataSetCnt, headNumber);

        // Terminate in case of an empty section (no records - regular end)
        if (recordCnt == 0) 
            break;

        // Handle data section depending on format code
        if ((formatCode == 0) || (formatCode == 1) || (formatCode == 4) || (formatCode == 5))
        {
            //logInfo("Frame, fmt=%u, filePos 0x%08X", formatCode, filePos);

            // Terminate on insane frames
            if (recordCnt <= 1)
            {
                logError("[IDTF] %s: Frames should contain at least 2 points", filename.c_str());
                result = -1;
                break;
            }

            // Formats 0 and 4 are X, Y, Z; Formats 1 and 5 are X, Y.
            int hasZ = (formatCode == 0) || (formatCode == 4);

            // Formats 0 and 1 have color index; Formats 4 and 5 are true color RGB.
            int hasIndex = (formatCode == 0) || (formatCode == 1);

            // Open a frame (no longer needed bc done in process())
            // Todo if device is busy
            //management->devices.front()->stopAndEmptyQueue();
            //management->devices.front()->bex->setMode(DRIVER_FRAMEMODE); // todo different mode for manual playback

            /*ISPFrameMetadata metadata;
            metadata.once = true;
            metadata.isWave = false;
            metadata.len = recordCnt;
            metadata.dur = (1000000 * metadata.len) / 30000; // TODO get pps
            */


            /*auto queuedFrame = std::make_shared<QueuedFrame>();

            memset(&queuedFrame->chunkData, 0, sizeof(queuedFrame->chunkData));
            queuedFrame->chunkData.chunkFlags = IDNFLG_GRAPHIC_FRAME_ONCE;
            queuedFrame->chunkData.chunkDuration = (1000000 * recordCnt) / 30000; // us
            queuedFrame->chunkData.decoder = decoder;
            queuedFrame->chunkData.sampleCount = recordCnt;

            std::shared_ptr<uint8_t[]> buffer(new uint8_t[recordCnt * 8]);
            queuedFrame->buffer = buffer;*/


            std::shared_ptr<QueuedFrame> frame = std::make_shared<QueuedFrame>();

            unsigned int pointsPerFrame = recordCnt;
            unsigned int maxPointsPerFrame = devices->front()->maxBytesPerTransmission() / devices->front()->bytesPerPoint() - 1;
            if (pointsPerFrame > maxPointsPerFrame)
            {
                if (maxPointsPerFrame == 0)
                    return -1;

                pointsPerFrame = recordCnt / ceil((double)maxPointsPerFrame / pointsPerFrame);

                if (pointsPerFrame == 0)
                    return -1;
            }

            frame->buffer.reserve(pointsPerFrame * 8);
            
            // Loop through all points
            unsigned int currentPointInFrame = 0;
            for (int i = 0; i < recordCnt; i++)
            {
                int16_t x, y, z;
                uint8_t statusCode, r, g, b;

                // Remember file pos at the beginning of the point
                filePos = ftell(fpIDTF);

                // Read coordinates
                x = (short)((float)(short)readShort(fpIDTF) * xScale);
                y = (short)((float)(short)readShort(fpIDTF) * yScale);
                if (hasZ) 
                    z = readShort(fpIDTF); 
                else 
                    z = 0;

                // Read status code
                statusCode = (uint8_t)fgetc(fpIDTF);

                // Read color
                if (hasIndex)
                {
                    uint8_t colorIndex = (uint8_t)fgetc(fpIDTF);
                    long rgb = currentPalette[colorIndex];
                    r = (uint8_t)(rgb >> 16);
                    g = (uint8_t)(rgb >> 8);
                    b = (uint8_t)rgb;
                }
                else
                {
                    b = (uint8_t)fgetc(fpIDTF);
                    g = (uint8_t)fgetc(fpIDTF);
                    r = (uint8_t)fgetc(fpIDTF);
                }

                // Check for unexpected EOF
                if (feof(fpIDTF))
                {
                    logError("[IDTF] Unexpected end of file: Record %u of %u", i, recordCnt);
                    result = -1;
                    break;
                }

                // Output the point
                if (statusCode & 0x40) 
                    r = g = b = 0;

                frame->buffer.push_back(x & 0xFF);
                frame->buffer.push_back(x >> 8);
                frame->buffer.push_back(y & 0xFF);
                frame->buffer.push_back(y >> 8);
                frame->buffer.push_back(r);
                frame->buffer.push_back(g);
                frame->buffer.push_back(b);
                frame->buffer.push_back(0xFF);

                /*ISPDB25Point point;
                point.x = x;
                point.y = y;
                point.r = r * 0x101;
                point.g = g * 0x101;
                point.b = b * 0x101;
                point.intensity = 0xFF;
                point.u1 = point.u2 = point.u3 = point.u4 = 0;
                pointBuffer.push_back(point);

                if (currentPointInFrame++ >= pointsPerFrame && i < recordCnt-1) //  Send partial frame, if frame is split it due to being too large for DAC.
                {
                    frameSlice->durationUs = (1000000 * currentPointInFrame) / 30000; // us

                    {
                        std::lock_guard<std::mutex> lock(threadLock);
                        queue.push_back(frameSlice);
                    }

                    std::shared_ptr<TimeSlice> frameSlice = std::make_shared<TimeSlice>();

                    currentPointInFrame = 0;
                }

                //addPointToSlice(point, metadata);

                /*if (cbFunc->putSampleXYRGB(cbContext, x, y, r, g, b))
                { 
                    result = -1; 
                    break; 
                }*/

                // Check the status code (last point) against the record counter
                int lastPointFlag = ((statusCode & 0x80) != 0);
                int lastRecordFlag = ((i + 1) == recordCnt);
                if (lastPointFlag && !lastRecordFlag)
                {
                    logError("[IDTF] Last point flag set, record count mismatch: Record %u of %u, file pos 0x%08X", i, recordCnt, filePos);
                    result = -1;
                    break;
                }
                else if (!lastPointFlag && lastRecordFlag)
                {
                    logError("[IDTF] Last point flag not set on last record: File pos 0x%08X", filePos);
                }
            }
            if (result != 0) 
                break;

            // Tell the output to push the frame
            //management->devices.front()->commitChunk(true);
            //if (cbFunc->pushFrame(cbContext)) { result = -1; break; }


            //frameSlice->dataChunk = management->devices.front()->convertPoints(pointBuffer);

            if (!frame->buffer.empty())
            {
                frame->pps = 30000; // todo
                {
                    std::lock_guard<std::mutex> lock(threadLock);
                    queue.push_back(frame);
                }
            }

            //management->devices.front()->process(chunkData, )

        }
        else if (formatCode == 2)
        {
            //logInfo("Palette, filePos 0x%08X", formatCode, filePos);

            // Terminate on insane palettes
            if (recordCnt > 256)
            {
                logError("[IDTF] %s: Palettes shall not contain more than 256 colors", filename.c_str());
                result = -1;
                break;
            }

            // Initialize palette table
            memset(customPalette, 0, sizeof(customPalette));

            // Loop through all color indices
            for (int i = 0; i < recordCnt; i++)
            {
                // Read color
                uint8_t r = (uint8_t)fgetc(fpIDTF);
                uint8_t g = (uint8_t)fgetc(fpIDTF);
                uint8_t b = (uint8_t)fgetc(fpIDTF);

                // Check for unexpected EOF
                if (feof(fpIDTF))
                {
                    logError("[IDTF] Unexpected end of file: Record %u of %u", i, recordCnt);
                    result = -1;
                    break;
                }

                // Set palette entry
                customPalette[i] = ILDACOLOR(r, g, b);
            }
            if (result != 0) 
                break;

            // Set custom palette for the next sections
            currentPalette = customPalette;
        }
        else
        {
            logError("[IDTF] %s: formatCode = %d", filename.c_str(), formatCode);
            result = -1;
            break;
        }
    }

    fclose(fpIDTF);

    return result;
}

void FilePlayer::outputLoop()
{
    while (1) // todo close on exit
    {
        struct timespec delay, dummy;
        delay.tv_sec = 0;
        delay.tv_nsec = 1000000; // 1 ms

        if (state == FILEPLAYER_STATE_STOP)
        {


            delay.tv_nsec = 3000000; // 3 ms
        }
        else if (state == FILEPLAYER_STATE_PLAY || state == FILEPLAYER_STATE_PAUSE)
        {
            if (queue.empty())
                continue;

            if (!hasStarted)
            {
                outputs->front()->close();
                outputs->front()->open(RTLaproGraphicOutput::OPMODE_FRAME);
                hasStarted = true;
            }

            // todo check timing
            if (!outputs->front()->hasBufferedFrame())
            {
                std::lock_guard<std::mutex> lock(threadLock);
                std::shared_ptr<QueuedFrame> frame = queue.front();

                unsigned int numOfPoints = frame->buffer.size() / 8;

                if (numOfPoints > 0)
                {
                    RTLaproGraphicOutput::CHUNKDATA chunkData;
                    memset(&chunkData, 0, sizeof(chunkData));
                    chunkData.chunkFlags = IDNFLG_GRAPHIC_FRAME_ONCE;
                    chunkData.chunkDuration = (1000000 * numOfPoints) / frame->pps; // us
                    chunkData.decoder = &decoder;
                    chunkData.sampleCount = numOfPoints;

                    outputs->front()->process(chunkData, frame->buffer.data(), frame->buffer.size());
                }

                if (state != FILEPLAYER_STATE_PAUSE)
                {
                    queue.pop_front();

                    if (queue.empty())
                    {
                        // todo play next files according to mode
                        state == FILEPLAYER_STATE_STOP;
                    }
                }



                //devices->front()->writeFrame(*slice, slice->durationUs);
            }
        }

        nanosleep(&delay, &dummy);
    }
}

int FilePlayer::checkEOF(FILE* fp, const char* dbgText)
{
    if (feof(fp)) { logError("[IDTF] Unexpected end of file (%s)", dbgText); return -1; }

    return 0;
}

uint16_t FilePlayer::readShort(FILE* fp)
{
    uint16_t c1 = ((uint16_t)fgetc(fp)) & 0xFF;
    uint16_t c2 = ((uint16_t)fgetc(fp)) & 0xFF;

    return (uint16_t)((c1 << 8) | c2);
}
