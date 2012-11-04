//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#ifndef AUDIOREALTIME_H
#define AUDIOREALTIME_H
#include "Common.h"
#include "Params.h"
#include <iostream>
#include <string>
#include <math.h>
#include "File.h"
#if defined(_WIN32) && !defined(__MINGW32__)
#define snprintf _snprintf
#define DEVNULL "nul"
#else
#define DEVNULL "/dev/null"
#endif

class AudioRealTime {
public:
    AudioRealTime();
    virtual ~AudioRealTime();
    virtual bool ProcessRealTime(void);
    virtual std::string GetName() = 0;
    bool ProcessFilePointer(int pFile);
    int getNumSamples() const {return _NumberSamples;}
    const float* getSamples() {return _pSamples;}
    double getDuration() { return (double)getNumSamples() / Params::AudioStreamInput::SamplingRate; }
    int GetSeconds() const { return _Seconds;}

protected:
    float* _pSamples;
    uint _NumberSamples;
    int _Seconds;

};


#endif


