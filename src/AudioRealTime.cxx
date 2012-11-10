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
#include <sys/soundcard.h>

#include "AudioRealTime.h"
#include "Common.h"
#include "Params.h"

#include <alsa/asoundlib.h>
#include "portaudio.h"


using std::string;


/* #define SAMPLE_RATE  (17932) // Test failure to open with this value. */
#define SAMPLE_RATE  (11025)
#define FRAMES_PER_BUFFER (512)
#define NUM_CHANNELS    (1)
#define DITHER_FLAG     (0) /**/

/* Select sample format. */
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"

typedef struct
{
    int          frameIndex;  /* Index into sample array. */
    int          maxFrameIndex;
    SAMPLE      *recordedSamples;
}
paTestData;


static int recordCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
    paTestData *data = (paTestData*)userData;
    const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
    SAMPLE *wptr = &data->recordedSamples[data->frameIndex * NUM_CHANNELS];
    long framesToCalc;
    long i;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;

    (void) outputBuffer; /* Prevent unused variable warnings. */
    (void) timeInfo;
    (void) statusFlags;
    (void) userData;

    if( framesLeft < framesPerBuffer )
    {
        framesToCalc = framesLeft;
        finished = paComplete;
    }
    else
    {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if( inputBuffer == NULL )
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = SAMPLE_SILENCE;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = SAMPLE_SILENCE;  /* right */
        }
    }
    else
    {
        for( i=0; i<framesToCalc; i++ )
        {
            *wptr++ = *rptr++;  /* left */
            if( NUM_CHANNELS == 2 ) *wptr++ = *rptr++;  /* right */
        }
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
    PaStreamParameters  inputParameters,
                        outputParameters;
    PaStream*           stream;
    PaError             err = paNoError;
    paTestData          data;
    int                 i;
    int                 totalFrames;
    int                 numSamples;
    int                 numBytes;
    SAMPLE              max, val;
    double              average;


    data.maxFrameIndex = totalFrames = duration * SAMPLE_RATE; /* Record for a few seconds. */
    data.frameIndex = 0;
    numSamples = totalFrames * NUM_CHANNELS;
    numBytes = numSamples * sizeof(SAMPLE);
    data.recordedSamples = (SAMPLE *) malloc( numBytes ); /* From now on, recordedSamples is initialised. */
    if( data.recordedSamples == NULL )
    {
        printf("Could not allocate record array.\n");
        goto done;
    }
    for( i=0; i<numSamples; i++ ) data.recordedSamples[i] = 0;

    err = Pa_Initialize();
    if( err != paNoError ) goto done;

    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
    if (inputParameters.device == paNoDevice) {
        fprintf(stderr,"Error: No default input device.\n");
        goto done;
    }
    inputParameters.channelCount = 2;                    /* stereo input */
    inputParameters.sampleFormat = PA_SAMPLE_TYPE;
    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
    err = Pa_OpenStream(
              &stream,
              &inputParameters,
              NULL,                  /* &outputParameters, */
              SAMPLE_RATE,
              FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              recordCallback,
              &data );
    if( err != paNoError ) goto done;

    err = Pa_StartStream( stream );
    if( err != paNoError ) goto done;
    printf("\n=== Now recording!! Please speak into the microphone. ===\n"); fflush(stdout);

    while( ( err = Pa_IsStreamActive( stream ) ) == 1 )
    {
        Pa_Sleep(1000);
        printf("index = %d\n", data.frameIndex ); fflush(stdout);
    }
    if( err < 0 ) goto done;

    err = Pa_CloseStream( stream );
    if( err != paNoError ) goto done;

    done:
    return true;

}

bool AudioRealTime::ProcessRealTime_ALSA(int duration) {
    /* Open PCM device for recording (capture). */
    long loops;
    int rc, dir;
    unsigned int val, i;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_uframes_t frames;
    char *buffer;

    _Seconds = duration;


    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }
    printf("1\n");

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    val = (int)Params::AudioStreamInput::SamplingRate;
    snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
    frames = 32;
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
    printf("2\n");

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr,"unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }
    printf("3\n");

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    buffer = (char *) malloc(frames * 4); /* 2 bytes / sample, 2 channels */
    _pSamples = new float[(_Seconds+1) * (int)Params::AudioStreamInput::SamplingRate];
    snd_pcm_hw_params_get_period_time(params, &val, &dir);
    loops = (1000000*_Seconds) / val;

    uint sampleCounter = 0;

    float *temp_buffer = (float*)malloc(sizeof(float) * frames);
    Codegen *pCodegen = new Codegen();
    printf("4 frames is %d\n", frames);
    uint temp_buffer_counter = 0;
    uint amount_to_compute = (int)(0.5f * 11025.0);

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
        short *shortbuf = (short*)buffer;
        for(i=0;i<frames*2;i=i+2) {
            _pSamples[sampleCounter++] = ((float)shortbuf[i] + (float)shortbuf[i+1]) / 65536.0f;
            temp_buffer[temp_buffer_counter++] = ((float)shortbuf[i] + (float)shortbuf[i+1]) / 65536.0f;
        }
                    if(temp_buffer_counter == amount_to_compute) {
                printf("codegen w/ %d frames & offset %d\n", amount_to_compute, sampleCounter);
                pCodegen->callback(temp_buffer, amount_to_compute, sampleCounter);
                temp_buffer_counter = 0;
            }

    }

    _NumberSamples = sampleCounter;

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
    return true;
}

bool AudioRealTime::ProcessRealTime_OSS(int duration) {
    // Set up OSS
    _Seconds = duration;
    int rate = (int)Params::AudioStreamInput::SamplingRate;
    int dummy; // just for ioctl    
    int channels = 1; 
    int stereo = 0; 
    int format = AFMT_S16_LE; 
    int fp = open("/dev/dsp", O_RDONLY, 0);
    if (-1 == fp) { perror("open"); exit(-1); }

    dummy = format;
    if (-1 == ioctl(fp, SNDCTL_DSP_SETFMT, &dummy) || dummy != format) { perror("ioctl SNDCTL_DSP_SETFMT"); exit(-1); }
    dummy = channels;
    if (-1 == ioctl(fp, SNDCTL_DSP_CHANNELS, &dummy) || dummy != channels) { perror("ioctl SNDCTL_DSP_CHANNELS"); exit(-1); }
    dummy = stereo;
    if (-1 == ioctl(fp, SNDCTL_DSP_STEREO, &dummy) || dummy != stereo) { perror("ioctl SNDCTL_DSP_STEREO"); exit(-1); }

    // On my board the SNDCTL_DSP_SPEED setting returns the correct rate but keeps it at 8000Hz no matter what I say. So this doesn't work.
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
        _NumberSamples += samplesRead;
        vChunks.push_back(pChunk);
    } while ((samplesRead > 0) && (_NumberSamples < ((int)Params::AudioStreamInput::SamplingRate * _Seconds)));

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
    return true;
}


