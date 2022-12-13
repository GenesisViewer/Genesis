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
    void detectCuda(BOOL canUseIt);
    bool cudaEnabled() {return mCudaEnabled;}
    
private:
    bool mCudaEnabled;
    

};

#endif /* GENXCUDAMGR_H */