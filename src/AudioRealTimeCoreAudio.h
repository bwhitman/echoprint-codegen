//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#ifndef AUDIOREALTIMECOREAUDIO_H
#define AUDIOREALTIMECOREAUDIO_H
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

#import <AudioToolbox/AudioQueue.h>  //(don't know to use that) 
#import <AudioToolbox/AudioFile.h>   //(don't know to use that)
#import <AudioUnit/AudioUnit.h>
#import <AudioToolbox/AudioToolbox.h>

class AudioRealTimeCoreAudio {
public:
    AudioRealTimeCoreAudio();
    virtual ~AudioRealTimeCoreAudio();
    virtual bool ProcessRealTime(int duration);
    std::string GetName() {return "real time in";}
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


