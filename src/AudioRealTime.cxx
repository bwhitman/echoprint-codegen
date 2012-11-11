//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include <stddef.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#define POPEN_MODE "r"
#else
#include "win_unistd.h"
#include <winsock.h>
#define POPEN_MODE "rb"
#endif
#include "Codegen.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>

#include "AudioRealTime.h"
#include "Common.h"
#include "Params.h"

#include "portaudio.h"

#define PA_ENABLE_DEBUG_OUTPUT false
#define FRAMES_PER_BUFFER (512)

using std::string;

typedef struct {
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    float        *recordedSamples;
} paCallbackData;


static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    paCallbackData *data = (paCallbackData*)userData;
    const float *rptr = (const float*)inputBuffer;
    float *wptr = &data->recordedSamples[data->frameIndex];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer ) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else  {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if( inputBuffer == NULL ) {
        for( i=0; i<framesToCalc; i++) *wptr++ = 0; 
    } else {
        for( i=0; i<framesToCalc; i++) *wptr++ = *rptr++;
    }
    data->frameIndex += framesToCalc;
    return finished;
}


AudioRealTime::AudioRealTime() : _pSamples(NULL), _NumberSamples(0), _Seconds(0) {}

AudioRealTime::~AudioRealTime() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}

bool AudioRealTime::ProcessRealTime_PortAudio(int duration) {
    PaStreamParameters  inputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paCallbackData      data;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;

    data.maxFrameIndex = totalFrames = duration * 11025; 
    data.frameIndex = 0;
    numSamples = totalFrames * 1;
    numBytes = numSamples * sizeof(float);
    _pSamples = (float *) malloc(numBytes); 
    data.recordedSamples = _pSamples;

    err = Pa_Initialize();
    if( err != paNoError ) return false;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        return false;
    }

    inputParameters.channelCount = 1;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    err = Pa_OpenStream(&stream, &inputParameters, NULL, 11025, FRAMES_PER_BUFFER, paClipOff, recordCallback, &data);
    if( err != paNoError ) return false;

    err = Pa_StartStream(stream);
    if( err != paNoError ) return false;

    Codegen *pCodegen = new Codegen(duration);
    uint amount_to_compute_total = (int)(duration * 11025.0);
    uint amount_to_compute_per_section = (int)(5.0f * 11025.0);
    float * temp_buffer = (float*)malloc(sizeof(float) * amount_to_compute_per_section);
    uint counter = 0;
    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(50);
        if((uint)data.frameIndex >= amount_to_compute_per_section) {
            for(uint j=0;j<amount_to_compute_per_section;j++) {
                temp_buffer[j] = data.recordedSamples[j];
                counter++;
            }
            data.frameIndex = 0;
            pCodegen->callback(temp_buffer, amount_to_compute_per_section);
        }
        if(counter>=amount_to_compute_total) Pa_CloseStream(stream);
    }
    _NumberSamples = data.frameIndex;
    err = Pa_CloseStream( stream );
/*
    FILE * outraw = fopen("out.raw", "wb");
    fwrite(_pSamples, sizeof(float), _NumberSamples, outraw);
    fclose(outraw);
*/
    free (temp_buffer);
    delete pCodegen;
    return true;
}



