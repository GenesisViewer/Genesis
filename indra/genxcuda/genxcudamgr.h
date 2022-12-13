#include <cuda_runtime.h>
#include <cuda.h>
#include <stdio.h>
#include "llsingleton.h"
#include <nvjpeg2k.h>


#ifndef GENXCUDAMGR_H
#define GENXCUDAMGR_H

class GenxCudaMgr : public LLSingleton<GenxCudaMgr>
{
public:
	GenxCudaMgr();
	~GenxCudaMgr();
    void detectCuda();
    bool cudaEnabled() {return mCudaEnabled;}
    nvjpeg2kHandle_t getNvHandle(){return nvjpeg2k_handle;}
    nvjpeg2kStream_t getNvStream() {return nvjpeg2k_stream;}
    nvjpeg2kDecodeState_t getNvDecodeState() {return decode_state;}
private:
    bool mCudaEnabled;
    nvjpeg2kHandle_t nvjpeg2k_handle;
    nvjpeg2kStream_t nvjpeg2k_stream;
    nvjpeg2kDecodeState_t decode_state;

};

#endif /* GENXCUDAMGR_H */