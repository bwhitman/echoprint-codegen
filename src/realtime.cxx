//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//


#include <stdio.h>
#include <string.h>
#include <memory>
#ifndef _WIN32
    #include <libgen.h>
    #include <dirent.h>
#endif
#include <stdlib.h>
#include <stdexcept>

#include "AudioStreamInput.h"
#include "AudioRealTime.h"
#include "Codegen.h"
#include <string>

using namespace std;

// The response from the codegen. Contains all the fields necessary
// to create a json string.
typedef struct {
    char *error;
    char *filename;
    int start_offset;
    int duration;
    int tag;
    double t1;
    double t2;
    int numSamples;
    Codegen* codegen;
} codegen_response_t;

// deal with quotes etc in json
std::string escape(const string& value) {
    std::string s(value);
    std::string out = "";
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if ((unsigned char)c < 31)
            continue;

        switch (c) {
            case '"' : out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b" ; break;
            case '\f': out += "\\f" ; break;
            case '\n': out += "\\n" ; break;
            case '\r': out += "\\r" ; break;
            case '\t': out += "\\t" ; break;
            // case '/' : out += "\\/" ; break; // Unnecessary?
            default:
                out += c;
                // TODO: do something with unicode?
        }
    }
    return out;
}


char *make_json_string(codegen_response_t* response) {
    
    if (response->error != NULL) {
        return response->error;
    }
    
    // preamble + codelen
    char* output = (char*) malloc(sizeof(char)*(16384 + strlen(response->codegen->getCodeString().c_str()) ));

    sprintf(output,"{\"metadata\":{\"artist\":null, \"release\":null, \"title\":null, \"genre\":null, \"bitrate\":-1, "
                    "\"sample_rate\":11025 \"duration\":%d, \"filename\":null, \"samples_decoded\":%d, \"given_duration\":%d,"
                    " \"start_offset\":%d, \"version\":%2.2f, \"codegen_time\":%2.6f, \"decode_time\":%2.6f}, \"code_count\":%d,"
                    " \"code\":\"%s\", \"tag\":%d}",
        response->duration,
        response->numSamples,
        response->duration,
        response->start_offset,
        response->codegen->getVersion(),
        response->t2,
        response->t1,
        response->codegen->getNumCodes(),
        response->codegen->getCodeString().c_str(),
        response->tag
    );
    return output;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [seconds_duration]\n", argv[0]);
        exit(-1);
    }

    try {
        int duration = atoi(argv[1]);
        double t1 = now();
        codegen_response_t *response = (codegen_response_t *)malloc(sizeof(codegen_response_t));
        response->error = NULL;
        response->codegen = NULL;
        auto_ptr<AudioRealTime> pAudio(new AudioRealTime());
        pAudio->ProcessRealTime_PortAudio(duration);
        int numSamples = pAudio->getNumSamples();
        t1 = now() - t1;
        double t2 = now();
        Codegen *pCodegen = new Codegen(pAudio->getSamples(), numSamples, -1);
        t2 = now() - t2;
        response->t1 = t1;
        response->t2 = t2;
        response->numSamples = numSamples;
        response->codegen = pCodegen;
        response->start_offset = -1;
        response->duration = duration;
        response->tag = 0;
        char *json = make_json_string(response);
        delete pCodegen;
        free (response);
        printf("%s\n", json);
        free(json);
        return 0;
    }
    catch(std::runtime_error& ex) {
        fprintf(stderr, "%s\n", ex.what());
        return 1;
    }
    catch(...) {
        fprintf(stderr, "Unknown failure occurred\n");
        return 2;
    }

}
