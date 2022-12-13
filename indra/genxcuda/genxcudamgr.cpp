#include <cuda_runtime.h>
#include <cuda.h>
#include "genxcudamgr.h"

GenxCudaMgr::GenxCudaMgr() 
{
	
}
GenxCudaMgr::~GenxCudaMgr()
{
	nvjpeg2kStreamDestroy(nvjpeg2k_stream);
    nvjpeg2kDecodeStateDestroy(decode_state);
    nvjpeg2kDestroy(nvjpeg2k_handle);
}
void GenxCudaMgr::detectCuda() {
    int num_gpus = 0;
    cudaError_t err = cudaGetDeviceCount( &num_gpus );

    if (err != cudaSuccess) {
        mCudaEnabled = FALSE;
        LL_INFOS() << "CUDA has not been detected" << LL_ENDL;
    } else {
        mCudaEnabled = TRUE;
        LL_INFOS() << "CUDA has been detected" << LL_ENDL;
        
        nvjpeg2kCreateSimple(&nvjpeg2k_handle);
        nvjpeg2kDecodeStateCreate(nvjpeg2k_handle,&decode_state);
        nvjpeg2kStreamCreate(&nvjpeg2k_stream);

    }

   

}
