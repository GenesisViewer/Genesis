/** 
 * @file streamingaudio_fmodstudio.h
 * @author Tofu Linden
 * @brief Definition of LLStreamingAudio_FMODSTUDIO implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#ifndef LL_STREAMINGAUDIO_FMODSTUDIO_H
#define LL_STREAMINGAUDIO_FMODSTUDIO_H

#include "stdtypes.h" // from llcommon

#include "llstreamingaudio.h"
#include "lltimer.h"

//Stubs
class LLAudioStreamManagerFMODSTUDIO;
namespace FMOD
{
	class System;
	class Channel;
	class ChannelGroup;
	class ChannelGroup;
	class DSP;
}

//Interfaces
class LLStreamingAudio_FMODSTUDIO : public LLStreamingAudioInterface
{
 public:
	LLStreamingAudio_FMODSTUDIO(FMOD::System *system);
	/*virtual*/ ~LLStreamingAudio_FMODSTUDIO();

	/*virtual*/ void start(const std::string& url);
	/*virtual*/ void stop();
	/*virtual*/ void pause(int pause);
	/*virtual*/ void update();
	/*virtual*/ int isPlaying();
	/*virtual*/ void setGain(F32 vol);
	/*virtual*/ F32 getGain();
	/*virtual*/ std::string getURL();

	/*virtual*/ bool supportsMetaData(){return true;}
	/*virtual*/ const LLSD *getMetaData(){return mMetaData;}	//return NULL if not playing.
	/*virtual*/ bool supportsWaveData(){return true;}
	/*virtual*/ bool getWaveData(float* arr, S32 count, S32 stride = 1);
	/*virtual*/ bool supportsAdjustableBufferSizes(){return true;}
	/*virtual*/ void setBufferSizes(U32 streambuffertime, U32 decodebuffertime);

private:
	bool releaseDeadStreams();
	void cleanupWaveData();

	FMOD::System *mSystem;

	LLAudioStreamManagerFMODSTUDIO *mCurrentInternetStreamp;
	FMOD::Channel *mFMODInternetStreamChannelp;
	std::list<LLAudioStreamManagerFMODSTUDIO *> mDeadStreams;

	std::string mURL;
	std::string mPendingURL;
	F32 mGain;

	LLSD *mMetaData;

	FMOD::ChannelGroup* mStreamGroup;
	FMOD::DSP* mStreamDSP;
};


#endif // LL_STREAMINGAUDIO_FMOD_H
