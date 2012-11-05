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

#include <alsa/asoundlib.h>


using std::string;


AudioRealTime::AudioRealTime() : _pSamples(NULL), _NumberSamples(0), _Seconds(0) {}

AudioRealTime::~AudioRealTime() {
    if (_pSamples != NULL)
        delete [] _pSamples, _pSamples = NULL;
}

bool AudioRealTime::ProcessRealTime_ALSA(int duration) {
    /* Open PCM device for recording (capture). */
    long loops;
    int rc;
    int size;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;
    uint i;
    int er;

    _Seconds = duration;

    snd_pcm_uframes_t frames;
    char *buffer;

    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }



    snd_pcm_hw_params_alloca(&params);
    //fprintf(stderr, "alloc %d\n", er);

    /* Fill it in with default values. */
    er = snd_pcm_hw_params_any(handle, params);
    fprintf(stderr, "params %d\n", er);

    /* interleaved mode */
    er = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    fprintf(stderr, "access %d\n", er);

    /* Signed 16-bit little-endian format */
    er = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    fprintf(stderr, "form %d\n", er);

    er = snd_pcm_hw_params_set_channels(handle, params, 2);
    fprintf(stderr, "chan %d %s\n", er, snd_strerror(er));

    val = (int)Params::AudioStreamInput::SamplingRate;
    er = snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    fprintf(stderr, "rate %d\n", er);

    /* Set period size to 32 frames. */
    frames = 32;
    er = snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    fprintf(stderr, "frames %d\n", er);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr,"unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * 4; /* 2 bytes/sample, 2 channel */
    buffer = (char *) malloc(size);
    fprintf(stderr, "buffer size is %d from %d frames\n", size, frames);

    _pSamples = new float[_Seconds * (int)Params::AudioStreamInput::SamplingRate];

    /* We want to loop for 5 seconds */
    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    loops = (1000000*_Seconds) / val;
    fprintf(stderr, "loops is %d from %d Seconds and val %d\n", loops, _Seconds, val);
    uint sampleCounter = 0;
    while (loops > 0) {
        loops--;
        rc = snd_pcm_readi(handle, (void*)buffer, frames);
        if (rc == -EPIPE) {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (rc < 0) {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        } else if (rc != (int)frames) {
            fprintf(stderr, "short read, read %d frames\n", rc);
        }
        fprintf(stderr, "Converting from shortbuf to psamples\n");
        short *shortbuf = (short*)buffer;
        for(i=0;i<frames;i=i+2)
            _pSamples[sampleCounter++] = (float) shortbuf[i] / 32768.0f;
        fprintf(stderr, "Done converting this round. sampleCounter is %d\n", sampleCounter);
    }

    _NumberSamples = sampleCounter;

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return true;
}

bool AudioRealTime::ProcessRealTime_OSS(int duration) {
    // Set up OSS
    printf("asking to read %d seconds\n", duration);
    _Seconds = duration;
    int rate = (int)Params::AudioStreamInput::SamplingRate;
    int dummy; // just for ioctl    
    int channels = 1; 
    int stereo = 0; 
    int format = AFMT_S16_LE; // 16-bit signed (little endian), this one produces garbled sound...
    int fp = open("/dev/dsp", O_RDONLY, 0);
    if (-1 == fp) { perror("open"); exit(-1); }

    dummy = format;
    if (-1 == ioctl(fp, SNDCTL_DSP_SETFMT, &dummy) || dummy != format) { perror("ioctl SNDCTL_DSP_SETFMT"); exit(-1); }
    dummy = channels;
    if (-1 == ioctl(fp, SNDCTL_DSP_CHANNELS, &dummy) || dummy != channels) { perror("ioctl SNDCTL_DSP_CHANNELS"); exit(-1); }
    dummy = stereo;
    if (-1 == ioctl(fp, SNDCTL_DSP_STEREO, &dummy) || dummy != stereo) { perror("ioctl SNDCTL_DSP_STEREO"); exit(-1); }
    dummy = rate;
    if (-1 == ioctl(fp, SNDCTL_DSP_SPEED, &dummy) || dummy != rate) { perror("ioctl SNDCTL_DSP_SPEED"); exit(-1); }

    bool did_work = ProcessFilePointer_OSS(fp);
    close(fp);
    return did_work;
}


bool AudioRealTime::ProcessFilePointer_OSS(int pFile) {
    std::vector<short*> vChunks;
    uint nSamplesPerChunk = (uint) Params::AudioStreamInput::SamplingRate * Params::AudioStreamInput::SecondsPerRealTimeChunk;
    uint samplesRead = 0;
    uint bytesRead = 0;
    do {
        short* pChunk = new short[nSamplesPerChunk];
        bytesRead = read(pFile, pChunk, sizeof(short) * nSamplesPerChunk);
        samplesRead = bytesRead / sizeof(short);
        //printf("Asked to read %d samples, got %d bytes / %d samples \n", nSamplesPerChunk, bytesRead, samplesRead);
        _NumberSamples += samplesRead;
        vChunks.push_back(pChunk);
    } while ((samplesRead > 0) && (_NumberSamples < ((int)Params::AudioStreamInput::SamplingRate * _Seconds)));
    printf("Done listening. samplesRead %d, _NumberSamples %d, target samples %d", samplesRead, _NumberSamples, ((int)Params::AudioStreamInput::SamplingRate * _Seconds));

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
    FILE* output = fopen("output.raw", "wb");
    fwrite(_pSamples, sizeof(float), _NumberSamples, output);
    fclose(output);
    assert(samplesLeft == 0);

    return true;
}


