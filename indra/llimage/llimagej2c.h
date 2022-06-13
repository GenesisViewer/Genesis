/** 
 * @file llimagej2c.h
 * @brief Image implmenation for jpeg2000.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLIMAGEJ2C_H
#define LL_LLIMAGEJ2C_H

#include "llimage.h"
#include "llassettype.h"

class LLImageJ2CImpl;
class LLImageJ2C : public LLImageFormatted
{
protected:
	virtual ~LLImageJ2C();

public:
	LLImageJ2C();

	// Base class overrides
	/*virtual*/ std::string getExtension() { return std::string("j2c"); }
	/*virtual*/ BOOL updateData();
	/*virtual*/ BOOL decode(LLImageRaw *raw_imagep, F32 decode_time);
	/*virtual*/ BOOL decodeChannels(LLImageRaw *raw_imagep, F32 decode_time, S32 first_channel, S32 max_channel_count);
	/*virtual*/ BOOL encode(const LLImageRaw *raw_imagep, F32 encode_time);
	/*virtual*/ S32 calcHeaderSize();
	/*virtual*/ S32 calcDataSize(S32 discard_level = 0);
	/*virtual*/ S32 calcDiscardLevelBytes(S32 bytes);
	/*virtual*/ S8  getRawDiscardLevel();
	// Override these so that we don't try to set a global variable from a DLL
	/*virtual*/ void resetLastError();
	/*virtual*/ void setLastError(const std::string& message, const std::string& filename = std::string());
	
	
	// Encode with comment text 
	BOOL encode(const LLImageRaw *raw_imagep, const char* comment_text, F32 encode_time=0.0);

	BOOL validate(U8 *data, U32 file_size);
	BOOL loadAndValidate(const std::string &filename);

	// Encode accessors
	void setReversible(const BOOL reversible); // Use non-lossy?
	void setRate(F32 rate);
	void setMaxBytes(S32 max_bytes);
	S32 getMaxBytes() const { return mMaxBytes; }

	static S32 calcHeaderSizeJ2C();
	static S32 calcDataSizeJ2C(S32 w, S32 h, S32 comp, S32 discard_level, F32 rate = 0.f);

	static void openDSO();
	static void closeDSO();
	static std::string getEngineInfo();
	
protected:
	friend class LLImageJ2CImpl;
	friend class LLImageJ2COJ;
	friend class LLImageJ2CKDU;
	void decodeFailed();
	void updateRawDiscardLevel();

	S32 mMaxBytes; // Maximum number of bytes of data to use...
	S32 mDataSizes[MAX_DISCARD_LEVEL+1];		// Size of data required to reach a given level
	U32 mAreaUsedForDataSizeCalcs;				// Height * width used to calculate mDataSizes
	S8  mRawDiscardLevel;
	F32 mRate;
	BOOL mReversible;
	LLImageJ2CImpl *mImpl;
	std::string mLastError;
};

// Derive from this class to implement JPEG2000 decoding
class LLImageJ2CImpl
{
public:
	virtual ~LLImageJ2CImpl();
protected:
	// Find out the image size and number of channels.
	// Return value:
	// true: image size and number of channels was determined
	// false: error on decode
	virtual BOOL getMetadata(LLImageJ2C &base) = 0;
	// Decode the raw image optionally aborting (to continue later) after
	// decode_time seconds.  Decode at most max_channel_count and start
	// decoding channel first_channel.
	// Return value:
	// true: decoding complete (even if it failed)
	// false: time expired while decoding
	virtual BOOL decodeImpl(LLImageJ2C &base, LLImageRaw &raw_image, F32 decode_time, S32 first_channel, S32 max_channel_count) = 0;
	virtual BOOL encodeImpl(LLImageJ2C &base, const LLImageRaw &raw_image, const char* comment_text, F32 encode_time=0.0,
							BOOL reversible=FALSE) = 0;

	friend class LLImageJ2C;
};

#define LINDEN_J2C_COMMENT_PREFIX "LL_"

#endif
