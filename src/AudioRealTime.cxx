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

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/soundcard.h>

#include "AudioRealTime.h"
#include "Common.h"
#include "Params.h"



using std::string;


AudioRealTime::AudioRealTime() : _pSamples(NULL), _NumberSamples(0), _Seconds(0) {}

AudioRealTime::~AudioRealTime() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}


bool AudioRealTime::ProcessRealTime() {
    // Set up OSS
    int rate = 11025; // sampling rate - 8000 samples per seconds
    int dummy; // just for ioctl    
    int channels = 1; // 1 channel, non-stereo (mono)
    int stereo = 0; // no stereo mode (1 channel)
    int format = AFMT_S16_LE; // 16-bit signed (little endian), this one produces garbled sound...
    int fp = open("/dev/dsp", O_RDONLY, 0);
    if (-1 == fp) { perror("open"); }

    dummy = format;
    if (-1 == ioctl(fp, SNDCTL_DSP_SETFMT, &dummy) || dummy != format) { perror("ioctl SNDCTL_DSP_SETFMT"); }
    dummy = channels;
    if (-1 == ioctl(fp, SNDCTL_DSP_CHANNELS, &dummy) || dummy != channels) { perror("ioctl SNDCTL_DSP_CHANNELS"); }
    dummy = stereo;
    if (-1 == ioctl(fp, SNDCTL_DSP_STEREO, &dummy) || dummy != stereo) { perror("ioctl SNDCTL_DSP_STEREO"); }
    dummy = rate;
    if (-1 == ioctl(fp, SNDCTL_DSP_SPEED, &dummy) || dummy != rate) { perror("ioctl SNDCTL_DSP_SPEED"); }

    bool ok;
    bool did_work = ProcessFilePointer(fp);
    bool succeeded = !close(fp);
    ok = did_work && succeeded;
    return ok;
}


bool AudioRealTime::ProcessFilePointer(int pFile) {
    uint targetSampleLength = (uint) Params::AudioStreamInput::SamplingRate * _Seconds;
    short * pShorts = new short[targetSampleLength];
    uint samplesRead = 0;
    uint i;
    do {
        samplesRead = read(pFile, pShorts + _NumberSamples, targetSampleLength);
        printf("Asked to read %d samples, got %d\n", targetSampleLength, samplesRead);
        _NumberSamples += samplesRead;
        if (samplesRead <= 0) { perror("read"); }
    } while (samplesRead > 0);


    // Convert from shorts to 16-bit floats and copy into sample buffer.
    _pSamples = new float[_NumberSamples];
    for (i = 0; i < _NumberSamples; i++) 
        _pSamples[i] = (float) pShorts[i] / 32768.0f;

    delete [] pShorts;

    return true;
}


