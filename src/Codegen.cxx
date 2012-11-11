//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include <sstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include "Codegen.h"
#include "AudioBufferInput.h"
#include "Fingerprint.h"
#include "Whitening.h"
#include "SubbandAnalysis.h"
#include "Fingerprint.h"
#include "Common.h"

#include "Base64.h"
#include <zlib.h>

using std::string;
using std::vector;


#ifdef VISUALIZE
#include <stdio.h>    
#include <fcntl.h>    
#include <termios.h> 
// space between bytes. on my box was not stable at 500us
#define SLEEP_US 1000
// Take a 64-char long array of colors and convert into a buf for the LED board & write it to fd
static void draw_frame(int fd, char*frame) {
    int j,i=0;
    char buf = 32;
    // Write the start frame
    j = write(fd, &buf, 1);
    usleep(SLEEP_US);
    // Now write the colors
    for(i=0;i<64;i=i+2) {
        buf = (frame[i] << 4) | frame[i+1];
        j = write(fd, &buf, 1);
        usleep(SLEEP_US);
    }
}
static int serialport_init(const char* serialport, speed_t brate) {
    struct termios toptions;
    int fd;
    fd = open(serialport, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1)  {
        perror("init_serialport: Unable to open port ");
        return -1;
    }
    
    if (tcgetattr(fd, &toptions) < 0) {
        perror("init_serialport: Couldn't get term attributes");
        return -1;
    }
    cfsetispeed(&toptions, brate);
    cfsetospeed(&toptions, brate);

    // 8N1
    toptions.c_cflag &= ~PARENB;
    toptions.c_cflag &= ~CSTOPB;
    toptions.c_cflag &= ~CSIZE;
    toptions.c_cflag |= CS8;
    // no flow control
    toptions.c_cflag &= ~CRTSCTS;

    toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
    toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

    toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    toptions.c_oflag &= ~OPOST; // make raw

    // see: http://unixwiz.net/techtips/termios-vmin-vtime.html
    toptions.c_cc[VMIN]  = 0;
    toptions.c_cc[VTIME] = 20;
    
    if( tcsetattr(fd, TCSANOW, &toptions) < 0) {
        perror("init_serialport: Couldn't set term attributes");
        return -1;
    }

    return fd;
}
#endif

Codegen::Codegen() {
    // real time, don't do much
    _backpack = serialport_init("/dev/ttyO1", 9600);
}

Codegen::Codegen(const float* pcm, unsigned int numSamples, int start_offset) {
    if (Params::AudioStreamInput::MaxSamples < (uint)numSamples)
        throw std::runtime_error("File was too big\n");

    Whitening *pWhitening = new Whitening(pcm, numSamples);
    pWhitening->Compute();

    AudioBufferInput *pAudio = new AudioBufferInput();
    pAudio->SetBuffer(pWhitening->getWhitenedSamples(), pWhitening->getNumSamples());

    SubbandAnalysis *pSubbandAnalysis = new SubbandAnalysis(pAudio);
    pSubbandAnalysis->Compute();

    Fingerprint *pFingerprint = new Fingerprint(pSubbandAnalysis, start_offset);
    pFingerprint->Compute();

    _CodeString = createCodeString(pFingerprint->getCodes());
    _NumCodes = pFingerprint->getCodes().size();

    delete pFingerprint;
    delete pSubbandAnalysis;
    delete pWhitening;
    delete pAudio;
}



string Codegen::callback(const float *pcm, unsigned int numSamples, unsigned int offset_samples) {
    printf("Got %d samples at %d\n", numSamples, offset_samples);
    Whitening *pWhitening = new Whitening(pcm, numSamples);
    pWhitening->Compute();
    AudioBufferInput *pAudio = new AudioBufferInput();
    pAudio->SetBuffer(pWhitening->getWhitenedSamples(), pWhitening->getNumSamples());
    SubbandAnalysis *pSubbandAnalysis = new SubbandAnalysis(pAudio);
    pSubbandAnalysis->Compute();

    #ifdef VISUALIZE
    // Draw the thing
    char * frame = (char*) malloc(sizeof(char)*64);
    for(int i=0;i<64;i++) frame[i] = 6;
    matrix_f subbands = pSubbandAnalysis->getMatrix();
    uint skip = pSubbandAnalysis->getNumFrames() / 8; // usually 673/8 = 84
    for(unsigned int j=0;j<pSubbandAnalysis->getNumBands();j++) {
        for(unsigned int i=0; i<pSubbandAnalysis->getNumFrames();i=i+skip) {
            if ((i/skip) < 8) {
                float avg = 0; 
                for(unsigned int k=0;k<skip;k++) avg = avg + subbands(j, i+k); 
                avg = avg / (float)skip;
                frame[((i/skip) * 8) + j] = abs((((int)(avg * 100000) - 15) % 15) + 1);
            }
        }
    }
    draw_frame(_backpack, frame);
    free(frame);
    #endif


    delete pSubbandAnalysis;
    delete pAudio;
    delete pWhitening;
    return "";

}

string Codegen::createCodeString(vector<FPCode> vCodes) {
    if (vCodes.size() < 3) {
        return "";
    }
    std::ostringstream codestream;
    codestream << std::setfill('0') << std::hex;
    for (uint i = 0; i < vCodes.size(); i++)
        codestream << std::setw(5) << vCodes[i].frame;

    for (uint i = 0; i < vCodes.size(); i++) {
        int hash = vCodes[i].code;
        codestream << std::setw(5) << hash;
    }
    return compress(codestream.str());
}


string Codegen::compress(const string& s) {
    long max_compressed_length = s.size()*2;
    unsigned char *compressed = new unsigned char[max_compressed_length];

    // zlib the code string
    z_stream stream;
    stream.next_in = (Bytef*)(unsigned char*)s.c_str();
    stream.avail_in = (uInt)s.size();
    stream.zalloc = (alloc_func)0;
    stream.zfree = (free_func)0;
    stream.opaque = (voidpf)0;
    deflateInit(&stream, Z_DEFAULT_COMPRESSION);
    do {
        stream.next_out = compressed;
        stream.avail_out = max_compressed_length;
        if(deflate(&stream, Z_FINISH) == Z_STREAM_END) break;
    } while (stream.avail_out == 0);
    uint compressed_length = stream.total_out;
    deflateEnd(&stream);

    // base64 the zlib'd code string
    string encoded = base64_encode(compressed, compressed_length, true);
    delete [] compressed;
    return encoded;
}
