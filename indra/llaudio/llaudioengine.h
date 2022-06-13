/** 
 * @file audioengine.h
 * @brief Definition of LLAudioEngine base class abstracting the audio support
 *
 * $LicenseInfo:firstyear=2000&license=viewerlgpl$
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


#ifndef LL_AUDIOENGINE_H
#define LL_AUDIOENGINE_H

#include <list>
#include <map>

#include "v3math.h"
#include "v3dmath.h"
#include "lltimer.h"
#include "lluuid.h"
#include "llframetimer.h"
#include "llassettype.h"
#include "llextendedstatus.h"

#include "lllistener.h"

const F32 LL_WIND_UPDATE_INTERVAL = 0.1f;
const F32 LL_WIND_UNDERWATER_CENTER_FREQ = 20.f;

const F32 ATTACHED_OBJECT_TIMEOUT = 5.0f;
const F32 DEFAULT_MIN_DISTANCE = 2.0f;

#define MAX_CHANNELS 30
#define MAX_BUFFERS 40	// Some extra for preloading, maybe?

// This define is intended to allow us to switch from os based wav
// file loading to vfs based wav file loading. The problem is that I
// am unconvinced that the LLWaveFile works for loading sounds from
// memory. So, until that is fixed up, changed, whatever, this remains
// undefined.
//#define USE_WAV_VFILE

class LLVFS;

class LLAudioSource;
class LLAudioData;
class LLAudioChannel;
class LLAudioChannelOpenAL;
class LLAudioBuffer;
class LLStreamingAudioInterface;


//
//  LLAudioEngine definition
//

class LLAudioEngine 
{
	friend class LLAudioChannelOpenAL; // bleh. channel needs some listener methods.
	
public:
	enum LLAudioType
	{
		AUDIO_TYPE_NONE    = 0,
		AUDIO_TYPE_SFX     = 1,
		AUDIO_TYPE_UI      = 2,
		AUDIO_TYPE_AMBIENT = 3,
		AUDIO_TYPE_COUNT   = 4 // last
	};
	
	enum LLAudioPlayState
	{
		// isInternetStreamPlaying() returns an *int*, with
		// 0 = stopped, 1 = playing, 2 = paused.
		AUDIO_STOPPED = 0,
		AUDIO_PLAYING = 1,
		AUDIO_PAUSED = 2
	};
	
	LLAudioEngine();
	virtual ~LLAudioEngine();

	// initialization/startup/shutdown
	virtual bool init(const S32 num_channels, void *userdata);
	virtual std::string getDriverName(bool verbose) = 0;
	virtual void shutdown();

	// Used by the mechanics of the engine
	//virtual void processQueue(const LLUUID &sound_guid);
	virtual void setListener(LLVector3 pos,LLVector3 vel,LLVector3 up,LLVector3 at);
	virtual void updateWind(LLVector3 direction, F32 camera_height_above_water) = 0;
	virtual void idle(F32 max_decode_time = 0.f);
	virtual void updateChannels();

	//
	// "End user" functionality
	//
	virtual bool isWindEnabled();
	virtual void enableWind(bool state_b);

	// Use these for temporarily muting the audio system.
	// Does not change buffers, initialization, etc. but
	// stops playing new sounds.
	void setMuted(bool muted);
	bool getMuted() const { return mMuted; }
#ifdef USE_PLUGIN_MEDIA
	LLPluginClassMedia* initializeMedia(const std::string& media_type);
#endif
	F32 getMasterGain();
	void setMasterGain(F32 gain);

	F32 getSecondaryGain(S32 type);
	void setSecondaryGain(S32 type, F32 gain);

	F32 getInternetStreamGain();

	virtual void setDopplerFactor(F32 factor);
	virtual F32 getDopplerFactor();
	virtual void setRolloffFactor(F32 factor);
	virtual F32 getRolloffFactor();
	virtual void setMaxWindGain(F32 gain);


	// Methods actually related to setting up and removing sounds
	// Owner ID is the owner of the object making the request
	void triggerSound(const LLUUID &sound_id, const LLUUID& owner_id, const F32 gain,
					  const S32 type = LLAudioEngine::AUDIO_TYPE_NONE,
					  // <edit>
					  //const LLVector3d &pos_global = LLVector3d::zero);
					  const LLVector3d &pos_global = LLVector3d::zero,
					  const LLUUID source_object = LLUUID::null);
					  // </edit>
	bool preloadSound(const LLUUID &id);	//Preloads sounds without requiring a source.

	void addAudioSource(LLAudioSource *asp);
	void cleanupAudioSource(LLAudioSource *asp);

	LLAudioSource *findAudioSource(const LLUUID &source_id);
	LLAudioData *getAudioData(const LLUUID &audio_uuid);
	void removeAudioData(LLUUID &audio_uuid);

	// Internet stream implementation manipulation
	LLStreamingAudioInterface *getStreamingAudioImpl();
	void setStreamingAudioImpl(LLStreamingAudioInterface *impl);
	// Internet stream methods - these will call down into the *mStreamingAudioImpl if it exists
	void startInternetStream(const std::string& url);
	void stopInternetStream();
	void pauseInternetStream(int pause);
	void updateInternetStream(); // expected to be called often
	LLAudioPlayState isInternetStreamPlaying();
	// use a value from 0.0 to 1.0, inclusive
	void setInternetStreamGain(F32 vol);
	std::string getInternetStreamURL();
	
	// For debugging usage
	virtual LLVector3 getListenerPos();

	LLAudioBuffer *getFreeBuffer(); // Get a free buffer, or flush an existing one if you have to.
	LLAudioChannel *getFreeChannel(const F32 priority); // Get a free channel or flush an existing one if your priority is higher
	void cleanupBuffer(LLAudioBuffer *bufferp);

	bool hasDecodedFile(const LLUUID &uuid);

	void setAllowLargeSounds(bool allow) { mAllowLargeSounds = allow ;}
	bool getAllowLargeSounds() const {return mAllowLargeSounds;}

	// Asset callback when we're retrieved a sound from the asset server.
	void startNextTransfer();
	static void assetCallback(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type, void *user_data, S32 result_code, LLExtStat ext_status);

	friend class LLPipeline; // For debugging

	void checkStates();
public:
	F32 mMaxWindGain; // Hack.  Public to set before fade in?

protected:
	virtual LLAudioBuffer *createBuffer() = 0;
	virtual LLAudioChannel *createChannel() = 0;

	virtual bool initWind() = 0;
	virtual void cleanupWind() = 0;
	virtual void setInternalGain(F32 gain) = 0;

	void commitDeferredChanges();

	virtual void allocateListener() = 0;


	// listener methods
	virtual void setListenerPos(LLVector3 vec);
	virtual void setListenerVelocity(LLVector3 vec);
	virtual void orientListener(LLVector3 up, LLVector3 at);
	virtual void translateListener(LLVector3 vec);


	F64 mapWindVecToGain(LLVector3 wind_vec);
	F64 mapWindVecToPitch(LLVector3 wind_vec);
	F64 mapWindVecToPan(LLVector3 wind_vec);

protected:
	LLListener *mListenerp;

	bool mMuted;
	void* mUserData;

	S32 mLastStatus;
	
	S32 mNumChannels;
	bool mEnableWind;

	LLAudioData* mCurrentTransfer; // Audio file currently being transferred by the system
	LLFrameTimer mCurrentTransferTimer;

	// A list of all audio sources that are known to the viewer at this time.
	// This is most likely a superset of the ones that we actually have audio
	// data for, or are playing back.
public://Jay: IDGAF
	typedef std::map<LLUUID, LLAudioSource *> source_map;
protected:
	typedef std::map<LLUUID, LLAudioData *> data_map;
	
public://Jay: IDGAF
	source_map mAllSources;
protected:
	data_map mAllData;
	std::list<LLUUID> mPreloadSystemList;
	LLAudioChannel *mChannels[MAX_CHANNELS];

	// Buffers needs to change into a different data structure, as the number of buffers
	// that we have active should be limited by RAM usage, not count.
	LLAudioBuffer *mBuffers[MAX_BUFFERS];
	
	F32 mMasterGain;
	F32 mInternalGain;			// Actual gain set; either mMasterGain or 0 when mMuted is true.
	F32 mSecondaryGain[AUDIO_TYPE_COUNT];

	F32 mNextWindUpdate;

	LLFrameTimer mWindUpdateTimer;

private:
	void setDefaults();
	LLStreamingAudioInterface *mStreamingAudioImpl;

	bool mAllowLargeSounds;
};




//
// Standard audio source.  Can be derived from for special sources, such as those attached to objects.
//


class LLAudioSource
{
public:
	// owner_id is the id of the agent responsible for making this sound
	// play, for example, the owner of the object currently playing it
	// <edit>
	//LLAudioSource(const LLUUID &id, const LLUUID& owner_id, const F32 gain, const S32 type = LLAudioEngine::AUDIO_TYPE_NONE);
	LLAudioSource(const LLUUID &id, const LLUUID& owner_id, const F32 gain, const S32 type = LLAudioEngine::AUDIO_TYPE_NONE, const LLUUID source_id = LLUUID::null, const bool isTrigger = true);
	// </edit>
	virtual ~LLAudioSource();

	virtual void update();						// Update this audio source
	void updatePriority();

	void preload(const LLUUID &audio_id);

	void setAmbient(const bool ambient)						{ mAmbient = ambient; }
	bool isAmbient() const									{ return mAmbient; }

	void setLoop(const bool loop)							{ mLoop = loop; }
	bool isLoop() const										{ return mLoop; }

	void setSyncMaster(const bool master)					{ mSyncMaster = master; }
	bool isSyncMaster() const								{ return mSyncMaster; }

	void setSyncSlave(const bool slave)						{ mSyncSlave = slave; }
	bool isSyncSlave() const								{ return mSyncSlave; }

	void setQueueSounds(const bool queue)					{ mQueueSounds = queue; }
	bool isQueueSounds() const								{ return mQueueSounds; }

	void setPlayedOnce(const bool played_once)				{ mPlayedOnce = played_once; }

	void setType(S32 type)                                  { mType = type; }
	S32 getType(void) const                                 { return mType; }

	LLUUID const& getOwnerID(void) const					{ return mOwnerID; }
	LLUUID const& getSourceID(void) const					{ return mSourceID; }
	bool getIsTrigger(void) const							{ return mIsTrigger; }

	void setPositionGlobal(const LLVector3d &position_global)		{ mPositionGlobal = position_global; }
	LLVector3d getPositionGlobal() const							{ return mPositionGlobal; }
	LLVector3 getVelocity()	const									{ return mVelocity; }				
	F32 getPriority() const											{ return mPriority; }
	void setPriority(F32 priority)									{ mPriority = priority; }

	// Gain should always be clamped between 0 and 1.
	F32 getGain() const												{ return mGain; }
	virtual void setGain(const F32 gain)							{ mGain = llclamp(gain, 0.f, 1.f); }

	const LLUUID &getID() const		{ return mID; }
	// <edit>
	const LLUUID &getLogID() const { return mLogID; }
	// </edit>
	bool isDone() const;
	bool isMuted() const { return mSourceMuted; }

	LLAudioData *getCurrentData();
	LLAudioData *getQueuedData();
	LLAudioBuffer *getCurrentBuffer();

	bool setupChannel();
	bool play(const LLUUID &audio_id);	// Start the audio source playing

	bool hasPendingPreloads() const;	// Has preloads that haven't been done yet

	friend class LLAudioEngine;
	friend class LLAudioChannel;
protected:
	void setChannel(LLAudioChannel *channelp);
public:
	LLAudioChannel *getChannel() const						{ return mChannelp; }

protected:
	LLUUID			mID; // The ID of the source is that of the object if it's attached to an object.
	LLUUID			mOwnerID;	// owner of the object playing the sound
public:
	const LLUUID &getOwnerID()		{ return mOwnerID; }
protected:
	F32				mPriority;
	F32				mGain;
	bool			mSourceMuted;
	bool			mAmbient;
	bool			mLoop;
	bool			mSyncMaster;
	bool			mSyncSlave;
	bool			mQueueSounds;
	bool			mPlayedOnce;
	bool            mCorrupted;
	S32             mType;
	LLVector3d		mPositionGlobal;
	LLVector3		mVelocity;
	// <edit>
	LLUUID			mLogID;
	LLUUID			mSourceID;
	bool			mIsTrigger;
	// </edit>

	//LLAudioSource	*mSyncMasterp;	// If we're a slave, the source that we're synced to.
	LLAudioChannel	*mChannelp;		// If we're currently playing back, this is the channel that we're assigned to.
	LLAudioData		*mCurrentDatap;
	LLAudioData		*mQueuedDatap;

	typedef std::map<LLUUID, LLAudioData *> data_map;
	data_map mPreloadMap;

	LLFrameTimer mAgeTimer;
};




//
// Generic metadata about a particular piece of audio data.
// The actual data is handled by the derived LLAudioBuffer classes which are
// derived for each audio engine.
//


class LLAudioData
{
public:
	LLAudioData(const LLUUID &uuid);
	bool load();

	LLUUID getID() const				{ return mID; }
	LLAudioBuffer *getBuffer() const	{ return mBufferp; }

	enum ELoadState
	{
		STATE_LOAD_ERROR,
		STATE_LOAD_REQ_FETCH,
		STATE_LOAD_FETCHING,
		STATE_LOAD_REQ_DECODE,
		STATE_LOAD_DECODING,
		STATE_LOAD_READY
	};
	ELoadState	getLoadState() const			{ return mLoadState; }
	ELoadState	setLoadState(ELoadState state)	{ return mLoadState = state; }
	bool		isInPreload() const				{ return mLoadState > STATE_LOAD_ERROR && mLoadState < STATE_LOAD_READY; }

	void updateLoadState();

	friend class LLAudioEngine; // Severe laziness, bad.

protected:
	LLUUID mID;
	LLAudioBuffer *mBufferp;	// If this data is being used by the audio system, a pointer to the buffer will be set here.
	ELoadState mLoadState;
};


//
// Base class for an audio channel, i.e. a channel which is capable of playing back a sound.
// Management of channels is done generically, methods for actually manipulating the channel
// are derived for each audio engine.
//


class LLAudioChannel
{
public:
	LLAudioChannel();
	virtual ~LLAudioChannel();

	virtual void setSource(LLAudioSource *sourcep);
	LLAudioSource *getSource() const			{ return mCurrentSourcep; }

	void setSecondaryGain(F32 gain)             { mSecondaryGain = gain; }
	F32 getSecondaryGain()                      { return mSecondaryGain; }

	friend class LLAudioEngine;
	friend class LLAudioSource;

protected:
	virtual void play() = 0;
	virtual void playSynced(LLAudioChannel *channelp) = 0;
	virtual void cleanup();

public:
	virtual bool isPlaying() = 0;
	bool isFree() const							{ return mCurrentSourcep==NULL; }

protected:
	virtual bool updateBuffer(); // Check to see if the buffer associated with the source changed, and update if necessary.
	virtual void update3DPosition() = 0;
	virtual void updateLoop() = 0; // Update your loop/completion status, for use by queueing/syncing.

protected:
	LLAudioSource	*mCurrentSourcep;
	LLAudioBuffer	*mCurrentBufferp;
	bool			mLoopedThisFrame;
	F32             mSecondaryGain;
};




// Basically an interface class to the engine-specific implementation
// of audio data that's ready for playback.
// Will likely get more complex as we decide to do stuff like real streaming audio.


class LLAudioBuffer
{
public:
	LLAudioBuffer() : mInUse(true), mAudioDatap(NULL) { mLastUseTimer.reset(); }
	virtual ~LLAudioBuffer() {};
	virtual bool loadWAV(const std::string& filename) = 0;
	virtual U32 getLength() = 0;

	friend class LLAudioEngine;
	friend class LLAudioChannel;
	friend class LLAudioData;
protected:
	bool mInUse;
	LLAudioData *mAudioDatap;
	LLFrameTimer mLastUseTimer;
};



extern LLAudioEngine* gAudiop;

// <edit>
struct LLSoundHistoryItem
{
	LLUUID mID;
	LLVector3d mPosition;
	S32 mType;
	bool mPlaying;
	LLUUID mAssetID;
	LLUUID mOwnerID;
	LLUUID mSourceID;
	bool mIsTrigger;
	bool mIsLooped;
	F64 mTimeStarted;
	F64 mTimeStopped;
	bool mReviewed;
	bool mReviewedCollision;
	LLAudioSource* mAudioSource;
	
	LLSoundHistoryItem() : mType(0), mPlaying(false), mIsTrigger(false),
			mIsLooped(false), mReviewed(false), mReviewedCollision(false),
			mTimeStarted(0), mTimeStopped(0), mAudioSource(0) {}

	bool isPlaying(void) const
	{
		return mPlaying && mAudioSource && mAudioSource->getChannel()
		  // It's only REALLY playing if the following also returns true,
		  // but it's too hard detect when that part is starting/stopping,
		  // so in the sound explorer we call "playing" (or "looping")
		  // those audio source that have a channel assigned and call
		  // it a day.
			 /*&& mAudioSource->getChannel()->isPlaying()*/ ;
	}
};

extern std::map<LLUUID, LLSoundHistoryItem> gSoundHistory;

extern void logSoundPlay(LLAudioSource* audio_source, LLUUID const& assetid);
extern void logSoundStop(LLUUID const& id, bool destructed);
extern void pruneSoundLog();
extern int gSoundHistoryPruneCounter;

// </edit>

#endif
