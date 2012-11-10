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
    float*                       pSamplePtr;
    UInt32                       lastSampleIndex;
    UInt32                       stopRecordingIndex;
} RecordState;





void AudioInputCallback(void * inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer, const AudioTimeStamp * inStartTime,
    UInt32 inNumberPacketDescriptions, const AudioStreamPacketDescription * inPacketDescs) {
    
    int status;

    RecordState * recordState = (RecordState*)inUserData;

    if (inNumberPacketDescriptions == 0 && recordState->dataFormat.mBytesPerPacket != 0) {
        inNumberPacketDescriptions = inBuffer->mAudioDataByteSize / recordState->dataFormat.mBytesPerPacket;
    }
    float* audioData = (float*)inBuffer->mAudioData;
    for(UInt32 i=0;i<inNumberPacketDescriptions;i++) {
        if (recordState->lastSampleIndex == recordState->stopRecordingIndex) {
            recordState->recording=false;
        } else {
            recordState->pSamplePtr[recordState->lastSampleIndex++] = audioData[i];
        }
    }
    
    if (recordState->recording) {
        status = AudioQueueEnqueueBuffer(recordState->queue, inBuffer, 0, NULL);
    }
}


AudioRealTimeCoreAudio::AudioRealTimeCoreAudio() : _pSamples(NULL), _NumberSamples(0), _Seconds(0) {}

AudioRealTimeCoreAudio::~AudioRealTimeCoreAudio() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}

bool AudioRealTimeCoreAudio::ProcessRealTime(int duration) {
    /* Open PCM device for recording (capture). */
    _Seconds = duration;
    _pSamples = new float[(_Seconds+1) * (int)Params::AudioStreamInput::SamplingRate];
    int i;
    RecordState recordState;

    CAStreamBasicDescription mRecordFormat;
    memset(&mRecordFormat, 0, sizeof(mRecordFormat));

    mRecordFormat.mSampleRate = 11025; // dunno
    mRecordFormat.mChannelsPerFrame = 1;
    mRecordFormat.mFormatID = kAudioFormatLinearPCM;
    mRecordFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    mRecordFormat.mBitsPerChannel = 32;
    mRecordFormat.mBytesPerPacket = 4; //mRecordFormat.mBytesPerFrame = (mRecordFormat.mBitsPerChannel / 8) * mRecordFormat.mChannelsPerFrame;
    mRecordFormat.mFramesPerPacket = 1;
    mRecordFormat.mBytesPerFrame = 4;

    recordState.dataFormat = mRecordFormat;
    recordState.lastSampleIndex = 0;
    recordState.pSamplePtr = _pSamples;
    recordState.stopRecordingIndex = (_Seconds+1) * (int)Params::AudioStreamInput::SamplingRate;
    OSStatus status;
    status = AudioQueueNewInput(&recordState.dataFormat, AudioInputCallback, &recordState, NULL,
                                NULL, 0, &recordState.queue);

    if (status == 0) {
        // Prime recording buffers with empty data

        // allocate and enqueue buffers

        int packets, frames, bytes = 0;
        frames = (int)ceil(0.5f * mRecordFormat.mSampleRate);
        
        if (mRecordFormat.mBytesPerFrame > 0)
            bytes = frames * mRecordFormat.mBytesPerFrame;
        else {
            UInt32 maxPacketSize;
            if (mRecordFormat.mBytesPerPacket > 0)
                maxPacketSize = mRecordFormat.mBytesPerPacket;    // constant packet size
            else {
                UInt32 propertySize = sizeof(maxPacketSize);
                AudioQueueGetProperty(recordState.queue, kAudioQueueProperty_MaximumOutputPacketSize, &maxPacketSize, &propertySize);
            }
            if (mRecordFormat.mFramesPerPacket > 0)
                packets = frames / mRecordFormat.mFramesPerPacket;
            else
                packets = frames;   // worst-case scenario: 1 frame in a packet
            if (packets == 0)       // sanity check
                packets = 1;
            bytes = packets * maxPacketSize;
        }


        for (i = 0; i < NUM_BUFFERS; ++i) {
            AudioQueueAllocateBuffer(recordState.queue, bytes, &recordState.buffers[i]);
            AudioQueueEnqueueBuffer(recordState.queue, recordState.buffers[i], 0, NULL);
        }

        recordState.recording = true;
        status = AudioQueueStart(recordState.queue, NULL);
    }

    while(recordState.recording) {
        sleep(0.1);
    }
    _NumberSamples = recordState.lastSampleIndex;
    status = AudioQueueStop(recordState.queue, true);
    status = AudioQueueDispose(recordState.queue, true);

    return 1;

}

