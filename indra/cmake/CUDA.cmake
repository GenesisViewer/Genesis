# -*- cmake -*-
include(Prebuilt)

set(CUDA_FIND_QUIETLY ON)
set(CUDA_FIND_REQUIRED ON)
use_prebuilt_binary(cuda)
use_prebuilt_binary(nvjpeg2k)
set(CUDA_LIBRARIES
        cuda
        cudart
        )
set(CUDA_LIB_DIR ${LIBS_PREBUILT_DIR}/lib/release/)
set(CUDA_INCLUDE_DIRS ${LIBS_PREBUILT_DIR}/include)