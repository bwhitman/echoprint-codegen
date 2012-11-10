//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include <stddef.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#define POPEN_MODE "r"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include "AudioRealTimeCoreAudio.h"
#include "Common.h"
#include "Params.h"
#include "CAStreamBasicDescription.h"
using std::string;

#define NUM_BUFFERS 3

typedef struct
{
    AudioStreamBasicDescription  dataFormat;
    AudioQueueRef                queue;
    AudioQueueBufferRef          buffers[NUM_BUFFERS];
    AudioFileID                  audioFile;
    SInt64                       currentPacket;
    bool                         recording;
} RecordState;





void AudioInputCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, const AudioTimeStamp * inStartTime,
    UInt32 inNumberPacketDescriptions, const AudioStreamPacketDescription * inPacketDescs) {
    
    RecordState * recordState = (RecordState*)inUserData;
    if (!recordState->recording) {
        printf("Not recording, returning\n");
    }

    if (inNumberPacketDescriptions == 0 && recordState->dataFormat.mBytesPerPacket != 0) {
        inNumberPacketDescriptions = inBuffer->mAudioDataByteSize / recordState->dataFormat.mBytesPerPacket;
    }

//    int sampleCount = recordState->buffers[0]->mAudioDataBytesCapacity / sizeof (AUDIO_DATA_TYPE_FORMAT);
//    AUDIO_DATA_TYPE_FORMAT *p = (AUDIO_DATA_TYPE_FORMAT*)recordState->buffers[0]->mAudioData;

    printf("Writing buffer %lld\n", recordState->currentPacket);
    // do something with mAudioData i guess
    // inBuffer->mAudioData
/*
    OSStatus status = AudioFileWritePackets(recordState->audioFile,
                                            false,
                                            inBuffer->mAudioDataByteSize,
                                            inPacketDescs,
                                            recordState->currentPacket,
                                            &inNumberPacketDescriptions,
                                            inBuffer->mAudioData);
*/
    recordState->buffers[0] = nil;
    recordState->currentPacket += inNumberPacketDescriptions;

    AudioQueueEnqueueBuffer(recordState->queue, inBuffer, 0, NULL);
}


AudioRealTimeCoreAudio::AudioRealTimeCoreAudio() : _pSamples(NULL), _NumberSamples(0), _Seconds(0) {}

AudioRealTimeCoreAudio::~AudioRealTimeCoreAudio() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}

bool AudioRealTimeCoreAudio::ProcessRealTime(int duration) {
    /* Open PCM device for recording (capture). */
    _Seconds = duration;

    RecordState recordState;

    CAStreamBasicDescription mRecordFormat;
    memset(&mRecordFormat, 0, sizeof(mRecordFormat));

    UInt32 size = sizeof(mRecordFormat.mSampleRate);
    mRecordFormat.mSampleRate = 11025; // dunno
    mRecordFormat.mChannelsPerFrame = 1;
    mRecordFormat.mFormatID = kAudioFormatLinearPCM;
    mRecordFormat.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    mRecordFormat.mBitsPerChannel = 16;
    mRecordFormat.mBytesPerPacket = mRecordFormat.mBytesPerFrame = (mRecordFormat.mBitsPerChannel / 8) * mRecordFormat.mChannelsPerFrame;
    mRecordFormat.mFramesPerPacket = 1;
    
    recordState.dataFormat = mRecordFormat;

    OSStatus status;
    status = AudioQueueNewInput(&recordState.dataFormat, AudioInputCallback, &recordState, CFRunLoopGetCurrent(),
                                kCFRunLoopCommonModes, 0, &recordState.queue);
    if (status == 0) {
        // Prime recording buffers with empty data
        for (int i = 0; i < NUM_BUFFERS; i++) {
            AudioQueueAllocateBuffer(recordState.queue, 16000, &recordState.buffers[i]);
            AudioQueueEnqueueBuffer (recordState.queue, recordState.buffers[i], 0, NULL);
        }

        recordState.recording = true;
        status = AudioQueueStart(recordState.queue, NULL);
    }
    return status;
}

