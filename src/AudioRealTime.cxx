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
#include <string.h>

#include "AudioRealTime.h"
#include "Common.h"
#include "Params.h"

using std::string;


bool AudioRealTime::IsSupported(const char *path) {
    return true; // Take a crack at anything, by default. The worst thing that will happen is that we fail.
}

AudioRealTime::AudioRealTime() : _pSamples(NULL), _NumberSamples(0), _Offset_s(0), _Seconds(0) {}

AudioRealTime::~AudioRealTime() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}


bool AudioRealTime::ProcessRealTime() {
    // Set up OSS
    

    bool did_work = ProcessFilePointer(fp);
    bool succeeded = !pclose(fp);
    ok = did_work && succeeded;
    return ok;
}


bool AudioRealTime::ProcessFilePointer(FILE* pFile) {
    std::vector<short*> vChunks;
    uint nSamplesPerChunk = (uint) Params::AudioStreamInput::SamplingRate * Params::AudioStreamInput::SecondsPerChunk;
    uint samplesRead = 0;
    do {
        short* pChunk = new short[nSamplesPerChunk];
        samplesRead = fread(pChunk, sizeof (short), nSamplesPerChunk, pFile);
        _NumberSamples += samplesRead;
        vChunks.push_back(pChunk);
    } while (samplesRead > 0);

    // Convert from shorts to 16-bit floats and copy into sample buffer.
    uint sampleCounter = 0;
    _pSamples = new float[_NumberSamples];
    uint samplesLeft = _NumberSamples;
    for (uint i = 0; i < vChunks.size(); i++)
    {
        short* pChunk = vChunks[i];
        uint numSamples = samplesLeft < nSamplesPerChunk ? samplesLeft : nSamplesPerChunk;

        for (uint j = 0; j < numSamples; j++)
            _pSamples[sampleCounter++] = (float) pChunk[j] / 32768.0f;

        samplesLeft -= numSamples;
        delete [] pChunk, vChunks[i] = NULL;
    }
    assert(samplesLeft == 0);

    int error = ferror(pFile);
    bool success = error == 0;

    return success;
}


