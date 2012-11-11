//
//  echoprint-codegen
//  Copyright 2011 The Echo Nest Corporation. All rights reserved.
//



#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include "Common.h"
#include "SubbandAnalysis.h"
#include "MatrixUtility.h"
#include <vector>

#define HASH_SEED 0x9ea5fa36
#define QUANTIZE_DT_S (256.0/11025.0)
#define QUANTIZE_A_S (256.0/11025.0)
#define HASH_BITMASK 0x000fffff
#define SUBBANDS 8

struct FPCode {
    FPCode() : frame(0), code(0) {}
    FPCode(uint f, int c) : frame(f), code(c) {}
    uint frame;
    uint code;
};

unsigned int MurmurHash2 ( const void * key, int len, unsigned int seed );

class Fingerprint {
public:
    uint quantized_time_for_frame_delta(uint frame_delta);
    uint quantized_time_for_frame_absolute(uint frame);
    Fingerprint(SubbandAnalysis* pSubbandAnalysis, int offset);
    Fingerprint(int ttarg); // real time

    void adaptiveOnsetsInit(int duration);
    uint adaptiveOnsetsUpdate(SubbandAnalysis *pSubbandAnalysis, uint just_the_new_frames);
    void Compute();
    uint adaptiveOnsets(int ttarg, matrix_u&out, uint*&onset_counter_for_band) ;
    std::vector<FPCode>& getCodes(){return _Codes;}
    std::vector<FPCode>& getUpdateCodes(){return _update_codes;}
protected:
    uint * _update_onset_counter_for_band;
    uint _update_onset_counter;
    matrix_f _update_out;
    SubbandAnalysis *_pSubbandAnalysis;
    int _Offset;
    int _ttArg;
    std::vector<FPCode> _Codes, _update_codes;
    double _update_H[SUBBANDS],_update_taus[SUBBANDS], _update_N[SUBBANDS];
    int _update_contact[SUBBANDS], _update_lcontact[SUBBANDS], _update_tsince[SUBBANDS];
    double _update_Y0[SUBBANDS];
    bool _first_run;
    int _update_i; // onset counter

};

#endif
