#include <cuda_runtime.h>
#include <cuda.h>
#include "genxcudamgr.h"

GenxCudaMgr::GenxCudaMgr() 
{
	
}
GenxCudaMgr::~GenxCudaMgr()
{
	
}
void GenxCudaMgr::detectCuda(BOOL canUseIt) {
    int num_gpus = 0;
    cudaError_t err = cudaGetDeviceCount( &num_gpus );

    if (err != cudaSuccess) {
        mCudaEnabled = FALSE;
        LL_INFOS() << "CUDA has not been detected" << LL_ENDL;
    } else {
        mCudaEnabled = TRUE && canUseIt;
        LL_INFOS() << "CUDA has been detected" << LL_ENDL;
    }

   

}
