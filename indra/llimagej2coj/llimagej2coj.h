/** 
 * @file llimagej2coj.h
 * @brief This is an implementation of JPEG2000 encode/decode using OpenJPEG.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLIMAGEJ2COJ_H
#define LL_LLIMAGEJ2COJ_H

#include "llimagej2c.h"
#include <cuda_runtime_api.h>
#include <nvjpeg2k.h>
#include "llimagebmp.h"
#include <mutex>
struct decode_params_t
{
    std::string input_dir;
    
    int dev;
    int warmup;
    int rgb_output;

    nvjpeg2kDecodeState_t nvjpeg2k_decode_state;
    nvjpeg2kHandle_t nvjpeg2k_handle;
    cudaStream_t stream;
    nvjpeg2kStream_t jpeg2k_stream;
    bool verbose;
    bool write_decoded;
    std::string output_dir;
};
#define CHECK_CUDA(call)                                                                                          \
    {                                                                                                             \
        cudaError_t _e = (call);                                                                                  \
        if (_e != cudaSuccess)                                                                                    \
        {                                                                                                         \
            std::cout << "CUDA Runtime failure: '#" << _e << "' at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE;                                                                                     \
        }                                                                                                         \
    }

#define CHECK_NVJPEG2K(call)                                                                                \
    {                                                                                                       \
        nvjpeg2kStatus_t _e = (call);                                                                       \
        if (_e != NVJPEG2K_STATUS_SUCCESS)                                                                  \
        {                                                                                                   \
            std::cout << "NVJPEG failure: '#" << _e << "' at " << __FILE__ << ":" << __LINE__ << std::endl; \
            return EXIT_FAILURE;                                                                            \
        }                                                                                                   \
    }
	int dev_malloc(void **p, size_t s) { return (int)cudaMalloc(p, s); }

	int dev_free(void *p) { return (int)cudaFree(p); }

	int host_malloc(void **p, size_t s, unsigned int f) { return (int)cudaHostAlloc(p, s, f); }

	int host_free(void *p) { return (int)cudaFreeHost(p); }
class LLImageJ2COJ : public LLImageJ2CImpl
{	
public:
	LLImageJ2COJ();
	virtual ~LLImageJ2COJ();

protected:
	/*virtual*/ BOOL getMetadata(LLImageJ2C &base);
	/*virtual*/ BOOL decodeImpl(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count);
	/*virtual*/ BOOL encodeImpl(LLImageJ2C &base, const LLImageRaw &raw_image, const char* comment_text, F32 encode_time=0.0,
								BOOL reversible = FALSE);
		
	BOOL decodeNVJPEG2000_main(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count);	
	double decodeNVJPEG2000_process_image(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count,decode_params_t &params);	
	double decodeNVJPEG2000_decode_image(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count,decode_params_t &params);	
	std::mutex decodeNVJPEG2000_mutex;
	bool allocate_output_buffers(nvjpeg2kImage_t& output_image, nvjpeg2kImageInfo_t& image_info,
    int bytes_per_element, int rgb_output);
	int ceildivpow2(int a, int b)
	{
		// Divide a by b to the power of 2 and round upwards.
		return (a + (1 << b) - 1) >> b;
	}
private:
	bool useCudaForDecoding=false;

	
};

#endif
