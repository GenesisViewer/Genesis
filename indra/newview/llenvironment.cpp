/**
 * @file llenvironment.cpp
 * @brief Extended environment manager class implementation.
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2002-2019, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include <deque>

#include "llenvironment.h"

#include "llapp.h"

#include "lldispatcher.h"
#include "lldir.h"
#include "llnotifications.h"
#include "llparcel.h"

#include "lltrans.h"

#include "llagent.h"
#include "llenvsettings.h"
#include "llexperiencelog.h"
#include "pipeline.h"
#include "llsky.h"
#include "llstartup.h"				// For getStartupState()
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "roles_constants.h"

LLEnvironment gEnvironment;

static const std::string KEY_ENVIRONMENT = "environment";
static const std::string KEY_DAYASSET = "day_asset";
static const std::string KEY_DAYCYCLE = "day_cycle";
static const std::string KEY_DAYHASH = "day_hash";
static const std::string KEY_DAYLENGTH = "day_length";
static const std::string KEY_DAYNAME = "day_name";
static const std::string KEY_DAYNAMES = "day_names";
static const std::string KEY_DAYOFFSET = "day_offset";
static const std::string KEY_ENVVERSION = "env_version";
static const std::string KEY_ISDEFAULT = "is_default";
static const std::string KEY_PARCELID = "parcel_id";
static const std::string KEY_REGIONID = "region_id";
static const std::string KEY_TRACKALTS = "track_altitudes";
static const std::string KEY_FLAGS = "flags";

static const std::string MESSAGE_PUSHENVIRONMENT = "PushExpEnvironment";

static const std::string ACTION_CLEARENVIRONMENT = "ClearEnvironment";
static const std::string ACTION_PUSHFULLENVIRONMENT = "PushFullEnvironment";
static const std::string ACTION_PUSHPARTIALENVIRONMENT = "PushPartialEnvironment";

static const std::string KEY_ASSETID = "asset_id";
static const std::string KEY_TRANSITIONTIME = "transition_time";
static const std::string KEY_ACTION = "action";
static const std::string KEY_ACTIONDATA = "action_data";
static const std::string KEY_EXPERIENCEID = "public_id";
// Some of these do not conform to the '_' format but changing them would also
// alter the Experience Log requirements.
static const std::string KEY_OBJECTNAME = "ObjectName";
static const std::string KEY_PARCELNAME = "ParcelName";
static const std::string ENV_KEY_COUNT = "Count";

static const std::string LISTENER_NAME = "LLEnvironmentSingleton";

constexpr F64 DEFAULT_UPDATE_THRESHOLD = 10.0;
constexpr F64 MINIMUM_SPANLENGTH = 0.01f;

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

inline F32 get_wrapping_distance(F32 begin, F32 end)
{
	if (begin < end)
	{
		return end - begin;
	}
	if (begin > end)
	{
		return 1.f - begin + end;
	}
	return 1.f;
}

LLSettingsDay::CycleTrack_t::iterator
	get_wrapping_atafter(LLSettingsDay::CycleTrack_t& collection, F32 key)
{
	if (collection.empty())
	{
		return collection.end();
	}

	LLSettingsDay::CycleTrack_t::iterator it = collection.upper_bound(key);
	if (it == collection.end())
	{
		// Wrap around
		it = collection.begin();
	}

	return it;
}

LLSettingsDay::CycleTrack_t::iterator
	get_wrapping_atbefore(LLSettingsDay::CycleTrack_t& collection, F32 key)
{
	if (collection.empty())
	{
		return collection.end();
	}

	LLSettingsDay::CycleTrack_t::iterator it = collection.lower_bound(key);
	if (it == collection.end())
	{
		// All keyframes are lower, take the last one.
		--it;	// We know the range is not empty
	}
	else if (it->first > key)
	{
		// The keyframe we are interested in is smaller than the found.
		if (it == collection.begin())
		{
			it = collection.end();
		}
		--it;
	}

	return it;
}

inline LLSettingsDay::TrackBound_t
	get_bounding_entries(LLSettingsDay::CycleTrack_t& track, F32 keyframe)
{
	return LLSettingsDay::TrackBound_t(get_wrapping_atbefore(track, keyframe),
										get_wrapping_atafter(track, keyframe));
}

// Find normalized track position of given time along full length of cycle
inline F32 convert_time_to_position(F64 time, F64 len)
{
	return time > 0.0 && len > 0.0 ? F32(fmod(time, len) / len) : 0.f;
}

inline F64 convert_time_to_blend_factor(F64 time, F64 len,
										   LLSettingsDay::CycleTrack_t& track)
{
	F32 position = convert_time_to_position(time, len);
	LLSettingsDay::TrackBound_t bounds = get_bounding_entries(track,
															   position);
	F32 first_pos = (bounds.first)->first;
	F32 spanlength = get_wrapping_distance(first_pos, (bounds.second)->first);
	if (position < first_pos)
	{
		position += 1.f;
	}

	F32 start = position - first_pos;

	return (F64)(start / spanlength);
}



//static
bool LLEnvironmentRequest::initiate(LLEnvironment::env_apply_fn cb)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		// No agent region is set before the STATE_WORLD_INIT step has been
		// completed, so only emit a warning when at a subsequent state.
		if (LLStartUp::getStartupState() > STATE_WORLD_INIT)
		{
			llwarns << "Agent region not set. Skipping environment settings request."
					<< llendl;
		}
		return false;
	}
	if (!regionp->capabilitiesReceived())
	{
		llinfos << "Deferring environment settings request until capabilities are received."
				<< llendl;
		regionp->setCapabilitiesReceivedCallback(boost::bind(&LLEnvironmentRequest::onRegionCapsReceived,
											   _1, cb));
		return false;
	}

	return doRequest(cb);
}

//static
void LLEnvironmentRequest::onRegionCapsReceived(const LLUUID& region_id,
												LLEnvironment::env_apply_fn cb)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || region_id != regionp->getRegionID())
	{
		llinfos << "Capabilities received for non-agent region. Ignored."
				<< llendl;
		return;
	}
	doRequest(cb);
}

//static
bool LLEnvironmentRequest::doRequest(LLEnvironment::env_apply_fn cb)
{
	const std::string& url = gAgent.getRegionCapability("EnvironmentSettings");
	if (url.empty())
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_REGION,
									LLSettingsDay::GetDefaultAssetId());
		return false;
	}

	gCoros.launch("LLEnvironmentRequest::environmentRequestCoro",
				  boost::bind(&LLEnvironmentRequest::environmentRequestCoro,
							  url, cb));
	return true;
}

//static 
void LLEnvironmentRequest::environmentRequestCoro(const std::string& url,
												  LLEnvironment::env_apply_fn cb)
{
	S32 request_id = ++sLastRequest;
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("EnvironmentRequest");
	LLSD result = adapter.getAndSuspend(url);
	llinfos << "Using legacy Windlight capability. Request Id = " << request_id
			<< llendl;
	if (request_id != sLastRequest)
	{
		llwarns << "Got superseded by another responder; discarding this reply."
				<< llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Got an error, not using region windlight... " << llendl;
		gEnvironment.setEnvironment(LLEnvironment::ENV_REGION,
									LLSettingsDay::getDefaultAssetId());
		return;
	}

	llinfos << "Received region legacy windlight settings." << llendl;
	result = result["content"];

	LLUUID region_id;
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		region_id = regionp->getRegionID();
	}
	if (region_id.notNull() && result[0]["regionID"].asUUID() != region_id)
	{
		llwarns << "Not in the region from where this data was received (wanting "
			<< region_id << " but got " << result[0]["regionID"].asUUID()
			<< "): ignoring." << llendl;
		return;
	}

	if (cb)
	{
		LLEnvironment::envinfo_ptr_t pinfo =
			LLEnvironment::EnvironmentInfo::extractLegacy(result);
		cb(INVALID_PARCEL_ID, pinfo);
	}
}

// LLTrackBlenderLoopingTime

class LLTrackBlenderLoopingTime final : public LLSettingsBlenderTimeDelta
{
public:
	LLTrackBlenderLoopingTime(const LLSettingsBase::ptr_t& target,
							  const LLSettingsDay::ptr_t& day, S32 trackno,
							  F64 cyclelength, F64 cycleoffset,
							  F64 update_threshold)
	:	LLSettingsBlenderTimeDelta(target, LLSettingsBase::ptr_t(),
								   LLSettingsBase::ptr_t(), 1.0),
		mDay(day),
		mCycleLength(cyclelength),
		mCycleOffset(cycleoffset)
	{
		// Must happen prior to getBoundingEntries call...
		mTrackNo = selectTrackNumber(trackno);

		F64 now = getAdjustedNow();
		LLSettingsDay::TrackBound_t initial = getBoundingEntries(now);

		mInitial = (initial.first)->second;
		mFinal = (initial.second)->second;
		mBlendSpan = getSpanTime(initial);

		initializeTarget(now);
		setOnFinished(boost::bind(&LLTrackBlenderLoopingTime::onFinishedSpan,
								  this));
	}

	void switchTrack(S32 trackno, F32)
	{
		S32 use_trackno = selectTrackNumber(trackno);

		if (use_trackno == mTrackNo)
		{
			// This results in no change
			return;
		}

		LLSettingsBase::ptr_t startsetting = mTarget->buildDerivedClone();
		mTrackNo = use_trackno;

		F64 now = getAdjustedNow() + LLEnvironment::TRANSITION_ALTITUDE;
		LLSettingsDay::TrackBound_t bounds = getBoundingEntries(now);

		F32 pos = (bounds.first)->first;
		LLSettingsBase::ptr_t endsetting = (bounds.first)->second->buildDerivedClone();
		F32 targetpos = convert_time_to_position(now, mCycleLength) - pos;
		F32 targetspan = get_wrapping_distance(pos, (bounds.second)->first);

		F64 blendf = calculateBlend(targetpos, targetspan);
		endsetting->blend((bounds.second)->second, blendf);

		reset(startsetting, endsetting, LLEnvironment::TRANSITION_ALTITUDE);
	}

protected:
	S32 selectTrackNumber(S32 trackno)
	{
		if (trackno == 0)
		{
			// We are dealing with the water track: there is only ever one.
			return trackno;
		}

		for (S32 test = trackno; test != 0; --test)
		{
			// Find the track below the requested one with data.
			LLSettingsDay::CycleTrack_t& track = mDay->getCycleTrack(test);
			if (!track.empty())
			{
				return test;
			}
		}

		return 1;
	}

	LLSettingsDay::TrackBound_t getBoundingEntries(F64 time)
	{
		LLSettingsDay::CycleTrack_t& wtrack = mDay->getCycleTrack(mTrackNo);
		F32 position = convert_time_to_position(time, mCycleLength);
		return get_bounding_entries(wtrack, position);
	}

	void initializeTarget(F64 time)
	{
		F64 blendf =
			convert_time_to_blend_factor(time, mCycleLength,
										 mDay->getCycleTrack(mTrackNo));
		blendf = llclamp(blendf, 0.0, 0.999);
		setTimeSpent(blendf * (F64)mBlendSpan);
		setBlendFactor(blendf);
	}

	inline F64 getAdjustedNow() const
	{
		return LLTimer::getEpochSeconds() + mCycleOffset;
	}

	F64 getSpanTime(const LLSettingsDay::TrackBound_t &bounds) const
	{
		F64 span = mCycleLength *
				   get_wrapping_distance((bounds.first)->first,
										 (bounds.second)->first);
		if (span < MINIMUM_SPANLENGTH)
		{
			 // For very short spans set a minimum length.
			span = MINIMUM_SPANLENGTH;
		}
		return span;
	}

private:
	void onFinishedSpan()
	{
		F64 adjusted_now = getAdjustedNow();
		LLSettingsDay::TrackBound_t next = getBoundingEntries(adjusted_now);
		F64 nextspan = getSpanTime(next);
		reset((next.first)->second, (next.second)->second, nextspan);

		// Recalculate (reinitialize) position. Because:
		// - 'delta' from applyTimeDelta accumulates errors (probably should be
		//	fixed/changed to absolute time).
		// - Freezes and lag can result in reset being called too late, so we
		//   need to add missed time.
		// - Occasional time corrections can happen.
		// - Some transition switches can happen outside applyTimeDelta thus
		//   causing 'desync' from 'delta' (can be fixed by getting rid of
		//   delta).
		initializeTarget(adjusted_now);
	}

private:
	LLSettingsDay::ptr_t	mDay;
	S32						mTrackNo;
	F64						mCycleLength;
	F64						mCycleOffset;
};

// LLEnvPushDispatchHandler

class LLEnvPushDispatchHandler final : public LLDispatchHandler
{
protected:
	LOG_CLASS(LLEnvPushDispatchHandler);

public:
	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override
	{
		LLSD message;
		sparam_t::const_iterator it = strings.begin();
		if (it != strings.end())
		{
			const std::string& llsd_raw = *it++;
			std::istringstream llsdData(llsd_raw);
			if (!LLSDSerialize::deserialize(message, llsdData,
											llsd_raw.length()))
			{
				llwarns << "Attempted to read parameter data into LLSD but failed: "
						<< llsd_raw << llendl;
			}
		}

		message[KEY_EXPERIENCEID] = invoice;
		// Object name
		if (it != strings.end())
		{
			message[KEY_OBJECTNAME] = *it++;
		}

		// Parcel name
		if (it != strings.end())
		{
			message[KEY_PARCELNAME] = *it++;
		}
		message[ENV_KEY_COUNT] = 1;

		gEnvironment.handleEnvironmentPush(message);
		return true;
	}
};

static LLEnvPushDispatchHandler sEnvPushDispatcher;

// LLSettingsInjected

template<class SETTINGT>
class LLSettingsInjected : public SETTINGT
{
public:
	typedef std::shared_ptr<LLSettingsInjected<SETTINGT> >  ptr_t;

	LLSettingsInjected(typename SETTINGT::ptr_t source)
	:	SETTINGT(),
		mSource(source),
		mLastSourceHash(0),
		mLastHash(0)
	{
	}

	~LLSettingsInjected() override					{}

	inline typename SETTINGT::ptr_t getSource() const
	{
		return mSource;
	}

	void setSource(const typename SETTINGT::ptr_t& source)
	{
		if (source.get() == this)
		{
			 // Do not set a source to itself.
			return;
		}
		mSource = source;
		this->setDirtyFlag(true);
		mLastSourceHash = 0;
	}

	inline bool isDirty() const override
	{
		return SETTINGT::isDirty() || mSource->isDirty();
	}

	inline bool isVeryDirty() const override
	{
		return SETTINGT::isVeryDirty() || mSource->isVeryDirty();
	}

	void injectSetting(const std::string& keyname, const LLSD& value,
					   const LLUUID& experience_id, F32 transition)
	{
		if (transition > 0.1f)
		{
			typename Injection::ptr_t injection =
				std::make_shared<Injection>(transition, keyname, value, true,
											experience_id);

			mInjections.push_back(injection);
			std::stable_sort(mInjections.begin(), mInjections.end(),
							 compare_remaining);
		}
		else
		{
			mOverrideValues[keyname] = value;
			mOverrideExps[keyname] = experience_id;
			this->setDirtyFlag(true);
		}
	}

	void removeInjection(const std::string& keyname, const LLUUID& experience,
						 F64 transition)
	{
		injections_t injections_buf;
		for (auto it = mInjections.begin(), end = mInjections.end(); it != end;
			 ++it)
		{
			if ((keyname.empty() || (*it)->mKeyName == keyname) &&
				(experience.isNull() || experience == (*it)->mExperience))
			{
				if (transition != LLEnvironment::TRANSITION_INSTANT)
				{
					typename Injection::ptr_t injection =
						std::make_shared<Injection>(transition, keyname,
													(*it)->mLastValue, false,
													LLUUID::null);
					injections_buf.push_front(injection);
				}
			}
			else
			{
				injections_buf.push_front(*it);
			}
		}

		mInjections.clear();
		mInjections = injections_buf;

		for (key_to_expid_t::iterator it = mOverrideExps.begin();
			 it != mOverrideExps.end(); )
		{
			if (experience.isNull() || it->second == experience)
			{
				if (transition != LLEnvironment::TRANSITION_INSTANT)
				{
					typename Injection::ptr_t injection =
						std::make_shared<Injection>(transition, it->first,
													mOverrideValues[it->first],
													false, LLUUID::null);
					mInjections.push_front(injection);
				}
				mOverrideValues.erase(it->first);
				mOverrideExps.erase(it++);
			}
			else
			{
				++it;
			}
		}

		std::stable_sort(mInjections.begin(), mInjections.end(),
						 compare_remaining);
	}

	void removeInjections(const LLUUID& experience_id, F64 transition)
	{
		removeInjection(LLStringUtil::null, experience_id, transition);
	}

	void injectExperienceValues(const LLSD& values,
								const LLUUID& experience_id, F64 transition)
	{
		for (LLSD::map_const_iterator it = values.beginMap(),
									  end = values.endMap();
			 it != end; ++it)
		{
			injectSetting(it->first, it->second, experience_id, transition);
		}
		this->setDirtyFlag(true);
	}

	void applyInjections(F64 delta)
	{
		this->mSettings = this->mSource->getSettings();

		for (LLSD::map_iterator it = mOverrideValues.beginMap(),
								end = mOverrideValues.endMap();
			 it != end; ++it)
		{
			this->mSettings[it->first] = it->second;
		}

		const LLSettingsBase::stringset_t& slerps = this->getSlerpKeys();
		const LLSettingsBase::stringset_t& skips =
			this->getSkipInterpolateKeys();
		const LLSettingsBase::stringset_t& specials = this->getSpecialKeys();

		typename injections_t::iterator it;
		for (it = mInjections.begin(); it != mInjections.end(); ++it)
		{
			std::string key_name = (*it)->mKeyName;

			LLSD value = this->mSettings[key_name];
			LLSD target = (*it)->mValue;

			if ((*it)->mFirstTime)
			{
				(*it)->mFirstTime = false;
			}
			else
			{
				(*it)->mTimeRemaining -= delta;
			}

			F64 mix = 1.0 - (*it)->mTimeRemaining / (*it)->mTransition;
			if (mix >= 1.0)
			{
				if ((*it)->mBlendIn)
				{
					mOverrideValues[key_name] = target;
					mOverrideExps[key_name] = (*it)->mExperience;
					this->mSettings[key_name] = target;
				}
				else
				{
					this->mSettings.erase(key_name);
				}
			}
			else if (specials.find(key_name) != specials.end())
			{
				updateSpecial(*it, mix);
			}
			else if (skips.find(key_name) == skips.end())
			{
				if (!(*it)->mBlendIn)
				{
					mix = 1.0 - mix;
				}
				(*it)->mLastValue =
					this->interpolateSDValue(key_name, value, target,
											 this->getParameterMap(), mix,
											 slerps);
				this->mSettings[key_name] = (*it)->mLastValue;
			}
		}

		size_t hash = this->getHash();
		if (hash != mLastHash)
		{
			this->setDirtyFlag(true);
			mLastHash = hash;
		}

		it = mInjections.begin();
		it = std::find_if(mInjections.begin(), mInjections.end(),
						  has_remaining);

		if (it != mInjections.begin())
		{
			mInjections.erase(mInjections.begin(), mInjections.end());
		}
	}

	inline bool hasInjections() const
	{
		return !mInjections.empty() || mOverrideValues.size() > 0;
	}

protected:
	struct Injection
	{
		typedef std::shared_ptr<Injection> ptr_t;

		Injection(F64 transition, const std::string& keyname,
				  const LLSD& value, bool blendin, const LLUUID& experience,
				  S32 index = -1)
		:	mTransition(transition),
			mTimeRemaining(transition),
			mKeyName(keyname),
			mValue(value),
			mExperience(experience),
			mIndex(index),
			mBlendIn(blendin),
			mFirstTime(true)
		{
		}

		F64			mTransition;
		F64			mTimeRemaining;
		std::string	mKeyName;
		LLSD		mValue;
		LLSD		mLastValue;
		LLUUID		mExperience;
		S32			mIndex;
		bool		mBlendIn;
		bool		mFirstTime;
	};

	void updateSettings() override
	{
		if (!mSource)
		{
			return;
		}

		// Clears the dirty flag on this object. Need to prevent triggering
		// more calls into this updateSettings.
		LLSettingsBase::updateSettings();

		resetSpecial();

		if (mSource->isDirty())
		{
			mSource->updateSettings();
		}

		SETTINGT::updateSettings();

		if (!mInjections.empty())
		{
			this->setDirtyFlag(true);
		}
	}

	const LLSettingsBase::stringset_t& getSpecialKeys() const;
	void resetSpecial();
	void updateSpecial(const typename Injection::ptr_t& injection, F64 mix);

private:
	inline static bool compare_remaining(const typename Injection::ptr_t& a,
											const typename Injection::ptr_t& b)
	{
		return a->mTimeRemaining < b->mTimeRemaining;
	}

	inline static bool has_remaining(const typename Injection::ptr_t& a)
	{
		return a->mTimeRemaining > 0.0;
	}

private:
	size_t						mLastSourceHash;
	size_t						mLastHash;

	typename SETTINGT::ptr_t	mSource;

	LLSD						mOverrideValues;

	typedef std::deque<typename Injection::ptr_t> injections_t;
	injections_t				mInjections;

	typedef std::map<std::string, LLUUID> key_to_expid_t;
	key_to_expid_t				mOverrideExps;
};

template<>
const LLSettingsBase::stringset_t& LLSettingsInjected<LLEnvSettingsSky>::getSpecialKeys() const
{
	static LLSettingsBase::stringset_t special_set;
	if (special_set.empty())
	{
		special_set.emplace(SETTING_BLOOM_TEXTUREID);
		special_set.emplace(SETTING_RAINBOW_TEXTUREID);
		special_set.emplace(SETTING_HALO_TEXTUREID);
		special_set.emplace(SETTING_CLOUD_TEXTUREID);
		special_set.emplace(SETTING_MOON_TEXTUREID);
		special_set.emplace(SETTING_SUN_TEXTUREID);
		// Due to being part of skips
		special_set.emplace(SETTING_CLOUD_SHADOW);
	}
	return special_set;
}

template<>
const LLSettingsBase::stringset_t& LLSettingsInjected<LLEnvSettingsWater>::getSpecialKeys() const
{
	static stringset_t special_set;
	if (special_set.empty())
	{
		special_set.emplace(SETTING_TRANSPARENT_TEXTURE);
		special_set.emplace(SETTING_NORMAL_MAP);
	}
	return special_set;
}

template<>
void LLSettingsInjected<LLEnvSettingsSky>::resetSpecial()
{
	mNextSunTextureId.setNull();
	mNextMoonTextureId.setNull();
	mNextCloudTextureId.setNull();
	mNextBloomTextureId.setNull();
	mNextRainbowTextureId.setNull();
	mNextHaloTextureId.setNull();
	setBlendFactor(0.f);
}

template<>
void LLSettingsInjected<LLEnvSettingsWater>::resetSpecial()
{
	mNextNormalMapID.setNull();
	mNextTransparentTextureID.setNull();
	setBlendFactor(0.f);
}

template<>
void LLSettingsInjected<LLEnvSettingsSky>::updateSpecial(const typename LLSettingsInjected<LLEnvSettingsSky>::Injection::ptr_t& injection,
														 F64 mix)
{
	if (injection->mKeyName == SETTING_SUN_TEXTUREID)
	{
		mNextSunTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_MOON_TEXTUREID)
	{
		mNextMoonTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_CLOUD_TEXTUREID)
	{
		mNextCloudTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_BLOOM_TEXTUREID)
	{
		mNextBloomTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_RAINBOW_TEXTUREID)
	{
		mNextRainbowTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_HALO_TEXTUREID)
	{
		mNextHaloTextureId = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_CLOUD_SHADOW)
	{
		// Special case due to being texture dependent and part of skips
		if (!injection->mBlendIn)
		{
			mix = 1.0 - mix;
		}
		stringset_t dummy;
		LLUUID cloud_noise_id = getCloudNoiseTextureId();
		F64 value = mSettings[injection->mKeyName].asReal();
		if (cloud_noise_id.isNull())
		{
			value = 0.0; // There was no texture so start from zero coverage
		}
		// Ideally we need to check for texture in injection, but in this case
		// user is setting value explicitly, potentially with different
		// transitions, do not ignore it.
		F64 result = lerp(value, injection->mValue.asReal(), mix);
		injection->mLastValue = LLSD::Real(result);
		mSettings[injection->mKeyName] = injection->mLastValue;
		return;	// Never set blend factor
	}

	// Unfortunately I do not have a per-texture blend factor. We will just
	// pick the one that is furthest along.
	if (getBlendFactor() < mix)
	{
		setBlendFactor(mix);
	}
}

template<>
void LLSettingsInjected<LLEnvSettingsWater>::updateSpecial(const typename LLSettingsInjected<LLEnvSettingsWater>::Injection::ptr_t& injection,
														   F64 mix)
{
	if (injection->mKeyName == SETTING_NORMAL_MAP)
	{
		mNextNormalMapID = injection->mValue.asUUID();
	}
	else if (injection->mKeyName == SETTING_TRANSPARENT_TEXTURE)
	{
		mNextTransparentTextureID = injection->mValue.asUUID();
	}

	// Unfortunately I do not have a per-texture blend factor. We will just
	// pick the one that is furthest along.
	if (getBlendFactor() < mix)
	{
		setBlendFactor(mix);
	}
}

typedef LLSettingsInjected<LLEnvSettingsSky> LLSettingsInjectedSky;
typedef LLSettingsInjected<LLEnvSettingsWater> LLSettingsInjectedWater;

// LLDayInjection

class LLDayInjection : public LLEnvironment::DayInstance
{
	friend class InjectedTransition;

public:
	typedef std::shared_ptr<LLDayInjection> ptr_t;
	typedef std::weak_ptr<LLDayInjection> wptr_t;

	LLDayInjection(LLEnvironment::EEnvSelection env);
	~LLDayInjection() override;

	bool applyTimeDelta(F64 delta) override;

	void setInjectedDay(const LLSettingsDay::ptr_t& dayp,
						const LLUUID& experience_id, F64 transition);
	void setInjectedSky(const LLSettingsSky::ptr_t& skyp,
						const LLUUID& experience_id, F64 transition);
	void setInjectedWater(const LLSettingsWater::ptr_t& waterp,
						  const LLUUID& experience_id, F64 transition);

	void injectSkySettings(const LLSD& settings, const LLUUID& experience_id,
						   F64 transition);
	void injectWaterSettings(const LLSD& settings, const LLUUID& experience_id,
							 F64 transition);

	void clearInjections(const LLUUID& experience_id, F64 transition_time);

	inline void animate() override				{}

	inline LLEnvironment::DayInstance::ptr_t getBaseDayInstance() const
	{
		return mBaseDayInstance;
	}

	void setBaseDayInstance(const LLEnvironment::DayInstance::ptr_t& baseday);

	inline S32 countExperiencesActive() const	{ return mActiveExperiences.size(); }

	inline bool isOverriddenSky() const			{ return mSkyExperience.notNull(); }
	inline bool isOverriddenWater() const		{ return mWaterExperience.notNull(); }

	inline bool hasInjections() const
	{
		return mSkyExperience.notNull() || mWaterExperience.notNull() ||
			   mDayExperience.notNull() || mBlenderSky || mBlenderWater ||
			   mInjectedSky->hasInjections() || mInjectedWater->hasInjections();
	}

	void testExperiencesOnParcel(S32 parcel_id);

private:
	void animateSkyChange(LLSettingsSky::ptr_t skyp, F64 transition);
	void animateWaterChange(LLSettingsWater::ptr_t waterp, F64 transition);

	void onEnvironmentChanged(LLEnvironment::EEnvSelection env);
	void onParcelChange();

	void checkExperience();

	static void testExperiencesOnParcelCoro(wptr_t that, S32 parcel_id);

	class InjectedTransition : public LLEnvironment::DayTransition
	{
	public:
		InjectedTransition(const LLDayInjection::ptr_t& injection,
						   const LLSettingsSky::ptr_t& skystart,
						   const LLSettingsWater::ptr_t& waterstart,
						   DayInstance::ptr_t& end, S32 time)
		:	LLEnvironment::DayTransition(skystart, waterstart, end, time),
			mInjection(injection)
		{
		}

		~InjectedTransition() override				{}

		void animate() override;

	protected:
		LLDayInjection::ptr_t mInjection;
	};

private:
	LLEnvironment::DayInstance::ptr_t	mBaseDayInstance;
	LLSettingsInjectedSky::ptr_t		mInjectedSky;
	LLSettingsInjectedWater::ptr_t		mInjectedWater;
	uuid_list_t							mActiveExperiences;
	LLUUID								mDayExperience;
	LLUUID								mSkyExperience;
	LLUUID								mWaterExperience;
	LLEnvironment::connection_t			mEnvChangeConnection;
	boost::signals2::connection			mParcelChangeConnection;
};

LLDayInjection::LLDayInjection(LLEnvironment::EEnvSelection env)
:	LLEnvironment::DayInstance(env)
{
	mSky = mInjectedSky =
		std::make_shared<LLSettingsInjectedSky>(gEnvironment.getCurrentSky());
	mWater = mInjectedWater =
		std::make_shared<LLSettingsInjectedWater>(gEnvironment.getCurrentWater());
	mBaseDayInstance = gEnvironment.getSharedEnvironmentInstance();
	mEnvChangeConnection =
		gEnvironment.setEnvironmentChanged(boost::bind(&LLDayInjection::onEnvironmentChanged,
													   this, _1));
	mParcelChangeConnection =
		gViewerParcelMgr.addAgentParcelChangedCB(boost::bind(&LLDayInjection::onParcelChange,
															 this));
}

LLDayInjection::~LLDayInjection()
{
	if (mEnvChangeConnection.connected())
	{
		mEnvChangeConnection.disconnect();
	}
	if (mParcelChangeConnection.connected())
	{
		mParcelChangeConnection.disconnect();
	}
}

bool LLDayInjection::applyTimeDelta(F64 delta)
{
	bool changed = false;

	if (mBaseDayInstance)
	{
		changed = mBaseDayInstance->applyTimeDelta(delta);
	}
	mInjectedSky->applyInjections(delta);
	mInjectedWater->applyInjections(delta);

	changed |= LLEnvironment::DayInstance::applyTimeDelta(delta);
	if (changed)
	{
		mInjectedSky->setDirtyFlag(true);
		mInjectedWater->setDirtyFlag(true);
	}

	mInjectedSky->update();
	mInjectedWater->update();

	if (!hasInjections())
	{
		// There is no injection being managed. This should really go away.
		gEnvironment.clearEnvironment(LLEnvironment::ENV_PUSH);
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	return changed;
}

void LLDayInjection::setBaseDayInstance(const LLEnvironment::DayInstance::ptr_t& baseday)
{
	mBaseDayInstance = baseday;

	if (mSkyExperience.isNull())
	{
		mInjectedSky->setSource(mBaseDayInstance->getSky());
	}
	if (mWaterExperience.isNull())
	{
		mInjectedWater->setSource(mBaseDayInstance->getWater());
	}
}

void LLDayInjection::testExperiencesOnParcel(S32 parcel_id)
{
	gCoros.launch("LLDayInjection::testExperiencesOnParcel",
				  [this, parcel_id]()
				  {
					testExperiencesOnParcelCoro(std::static_pointer_cast<LLDayInjection>(this->shared_from_this()),
												parcel_id);
				  });
}

void LLDayInjection::setInjectedDay(const LLSettingsDay::ptr_t& dayp,
									const LLUUID& experience_id, F64 transition)
{
	mSkyExperience = experience_id;
	mWaterExperience = experience_id;
	mDayExperience = experience_id;

	mBaseDayInstance = mBaseDayInstance->clone();
	mBaseDayInstance->setEnvironmentSelection(LLEnvironment::ENV_NONE);
	mBaseDayInstance->setDay(dayp, mBaseDayInstance->getDayLength(),
							 mBaseDayInstance->getDayOffset());
	animateSkyChange(mBaseDayInstance->getSky(), transition);
	animateWaterChange(mBaseDayInstance->getWater(), transition);

	mActiveExperiences.emplace(experience_id);
}

void LLDayInjection::setInjectedSky(const LLSettingsSky::ptr_t& skyp,
									const LLUUID& experience_id,
									F64 transition)
{
	mSkyExperience = experience_id;
	mActiveExperiences.emplace(experience_id);
	checkExperience();
	animateSkyChange(skyp, transition);
}

void LLDayInjection::setInjectedWater(const LLSettingsWater::ptr_t& waterp,
									  const LLUUID& experience_id,
									  F64 transition)
{
	mWaterExperience = experience_id;
	mActiveExperiences.emplace(experience_id);
	checkExperience();
	animateWaterChange(waterp, transition);
}

void LLDayInjection::injectSkySettings(const LLSD& settings,
									   const LLUUID& experience_id,
									   F64 transition)
{
	mInjectedSky->injectExperienceValues(settings, experience_id,
										 transition);
	mActiveExperiences.emplace(experience_id);
}

void LLDayInjection::injectWaterSettings(const LLSD& settings,
										 const LLUUID& experience_id,
										 F64 transition)
{
	mInjectedWater->injectExperienceValues(settings, experience_id,
										   transition);
	mActiveExperiences.emplace(experience_id);
}

void LLDayInjection::clearInjections(const LLUUID& experience_id,
									 F64 transition_time)
{
	if (experience_id == mDayExperience ||
		(experience_id.isNull() && mDayExperience.notNull()))
	{
		mDayExperience.setNull();
		if (mSkyExperience == experience_id)
		{
			mSkyExperience.setNull();
		}
		if (mWaterExperience == experience_id)
		{
			mWaterExperience.setNull();
		}

		mBaseDayInstance = gEnvironment.getSharedEnvironmentInstance();

		if (mSkyExperience.isNull())
		{
			animateSkyChange(mBaseDayInstance->getSky(), transition_time);
		}
		if (mWaterExperience.isNull())
		{
			animateWaterChange(mBaseDayInstance->getWater(), transition_time);
		}
	}

	if (experience_id == mSkyExperience ||
		(experience_id.isNull() && mSkyExperience.notNull()))
	{
		mSkyExperience.setNull();
		animateSkyChange(mBaseDayInstance->getSky(), transition_time);
	}

	if (experience_id == mWaterExperience ||
		(experience_id.isNull() && mWaterExperience.notNull()))
	{
		mWaterExperience.setNull();
		animateWaterChange(mBaseDayInstance->getWater(), transition_time);
	}

	mInjectedSky->removeInjections(experience_id, transition_time);
	mInjectedWater->removeInjections(experience_id, transition_time);

	if (experience_id.isNull())
	{
		mActiveExperiences.clear();
	}
	else
	{
		mActiveExperiences.erase(experience_id);
	}

	if (transition_time == LLEnvironment::TRANSITION_INSTANT &&
		countExperiencesActive() == 0)
	{
		// Only do this if instant and there are no other experiences injecting
		// values (it will otherwise be handled after transition)
		gEnvironment.clearEnvironment(LLEnvironment::ENV_PUSH);
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
}

void LLDayInjection::testExperiencesOnParcelCoro(wptr_t that, S32 parcel_id)
{
	std::string url = gAgent.getRegionCapability("ExperienceQuery");
	if (url.empty())
	{
		llwarns << "No experience query cap." << llendl;
		return;
	}

	{
		ptr_t thatlock(that);
		if (!thatlock)
		{
			return;
		}

		url += llformat("?parcelid=%d", parcel_id);
		bool first_param = true;
		for (uuid_list_t::iterator it = thatlock->mActiveExperiences.begin(),
								   end = thatlock->mActiveExperiences.end();
			 it != end; ++it)
		{
			if (first_param)
			{
				url += "&experiences=";
				first_param = false;
			}
			else
			{
				url += ',';
			}
			url += it->asString();
		}
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("testExperiencesOnParcelCoro");
	LLSD result = adapter.getAndSuspend(url);
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Unable to retrieve experience status for parcel Id = "
				<< parcel_id << llendl;
		return;
	}

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel || parcel_id != parcel->getLocalID())
	{
		// Agent no longer on queried parcel.
		return;
	}

	ptr_t thatlock(that);
	if (!thatlock)
	{
		return;
	}

	LLSD experiences = result["experiences"];
	for (LLSD::map_iterator it = experiences.beginMap(),
							end = experiences.endMap();
		 it != end; ++it)
	{
		if (!it->second.asBoolean())
		{
			thatlock->clearInjections(LLUUID(it->first),
									  LLEnvironment::TRANSITION_FAST);
		}
	}
}

void LLDayInjection::animateSkyChange(LLSettingsSky::ptr_t skyp,
									  F64 transition)
{
	if (mInjectedSky.get() == skyp.get())
	{
		// An attempt to animate to itself... Do not do it.
		return;
	}

	if (transition == LLEnvironment::TRANSITION_INSTANT)
	{
		mBlenderSky.reset();
		mInjectedSky->setSource(skyp);
		return;
	}

	LLSettingsSky::ptr_t start_sky = mInjectedSky->getSource()->buildClone();
	LLSettingsSky::ptr_t target_sky = start_sky->buildClone();
	mInjectedSky->setSource(target_sky);

	mBlenderSky = std::make_shared<LLSettingsBlenderTimeDelta>(target_sky,
															   start_sky,
															   skyp,	
															   transition);
	mBlenderSky->setOnFinished([this, skyp](LLSettingsBlender::ptr_t blender)
							   {
								mBlenderSky.reset();
								mInjectedSky->setSource(skyp);
								setSky(mInjectedSky);
								if (!mBlenderWater &&
									!countExperiencesActive())
								{
									gEnvironment.clearEnvironment(LLEnvironment::ENV_PUSH);
									gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
								}
							   });
}

void LLDayInjection::animateWaterChange(LLSettingsWater::ptr_t waterp,
										F64 transition)
{
	if (mInjectedWater.get() == waterp.get())
	{
		// An attempt to animate to itself. Bad idea.
		return;
	}

	if (transition == LLEnvironment::TRANSITION_INSTANT)
	{
		mBlenderWater.reset();
		mInjectedWater->setSource(waterp);
		return;
	}

	LLSettingsWater::ptr_t start_water(mInjectedWater->getSource()->buildClone());
	LLSettingsWater::ptr_t scratch_water(start_water->buildClone());
	mInjectedWater->setSource(scratch_water);

	mBlenderWater = std::make_shared<LLSettingsBlenderTimeDelta>(scratch_water,
																 start_water,
																 waterp,
																 transition);
	mBlenderWater->setOnFinished([this, waterp](LLSettingsBlender::ptr_t blender)
								 {
									mBlenderWater.reset();
									mInjectedWater->setSource(waterp);
									setWater(mInjectedWater);
									if (!mBlenderSky && (countExperiencesActive() == 0))
									{
										gEnvironment.clearEnvironment(LLEnvironment::ENV_PUSH);
										gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
									}
								 });
}

void LLDayInjection::onEnvironmentChanged(LLEnvironment::EEnvSelection env)
{
	if (env < LLEnvironment::ENV_PARCEL)
	{
		return;
	}

	LLEnvironment::EEnvSelection base_env =
			mBaseDayInstance->getEnvironmentSelection();
	LLEnvironment::DayInstance::ptr_t nextbase =
		gEnvironment.getSharedEnvironmentInstance();

	if (base_env == LLEnvironment::ENV_NONE || nextbase == mBaseDayInstance ||
		(mSkyExperience.notNull() && mWaterExperience.notNull()))
	{
		// Base instance completely overridden, or not changed no transition
		// will happen
		return;
	}

	llwarns << "Underlying environment has changed (" << env
			<< "). Base env is type " << base_env << llendl;

	LLEnvironment::DayInstance::ptr_t trans =
		std::make_shared<InjectedTransition>(std::static_pointer_cast<LLDayInjection>(shared_from_this()),
											mBaseDayInstance->getSky(),
											mBaseDayInstance->getWater(),
											nextbase,
											LLEnvironment::TRANSITION_DEFAULT);
	trans->animate();
	setBaseDayInstance(trans);
}

void LLDayInjection::onParcelChange()
{
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		testExperiencesOnParcel(parcel->getLocalID());
	}
}

void LLDayInjection::checkExperience()
{
	if (mDayExperience.notNull() && mSkyExperience != mDayExperience &&
		mWaterExperience != mDayExperience)
	{
		// There was a day experience but we have replaced it with a water and
		// a sky experience.
		mDayExperience.setNull();
		mBaseDayInstance = gEnvironment.getSharedEnvironmentInstance();
	}
}

void LLDayInjection::InjectedTransition::animate()
{
	mNextInstance->animate();

	if (!mInjection->isOverriddenSky())
	{
		mSky = mStartSky->buildClone();
		mBlenderSky =
			std::make_shared<LLSettingsBlenderTimeDelta>(mSky, mStartSky,
														 mNextInstance->getSky(),
														 mTransitionTime);
		mBlenderSky->setOnFinished([this](LLSettingsBlender::ptr_t blender)
								   {
										mBlenderSky.reset();
										if (!mBlenderSky && !mBlenderSky)
										{
											mInjection->setBaseDayInstance(mNextInstance);
										}
										else
										{
											mInjection->mInjectedSky->setSource(mNextInstance->getSky());
										}
								   });
	}
	else
	{
		mSky = mInjection->getSky();
		mBlenderSky.reset();
	}

	if (mInjection->isOverriddenWater())
	{
		mWater = mInjection->getWater();
		mBlenderWater.reset();
		return;
	}

	mWater = mStartWater->buildClone();
	mBlenderWater =
		std::make_shared<LLSettingsBlenderTimeDelta>(mWater, mStartWater,
													 mNextInstance->getWater(),
													 mTransitionTime);
	mBlenderWater->setOnFinished([this](LLSettingsBlender::ptr_t blender)
								 {
									mBlenderWater.reset();
									if (!mBlenderSky && !mBlenderWater)
									{
										mInjection->setBaseDayInstance(mNextInstance);
									}
									else
									{
										mInjection->mInjectedWater->setSource(mNextInstance->getWater());
									}
								});
}

///////////////////////////////////////////////////////////////////////////////
// LLEnvironment class proper
///////////////////////////////////////////////////////////////////////////////

#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
const LLUUID LLEnvironment::KNOWN_SKY_SUNRISE("01e41537-ff51-2f1f-8ef7-17e4df760bfb");
const LLUUID LLEnvironment::KNOWN_SKY_MIDDAY("6c83e853-e7f8-cad7-8ee6-5f31c453721c");
const LLUUID LLEnvironment::KNOWN_SKY_SUNSET("084e26cd-a900-28e8-08d0-64a9de5c15e2");
const LLUUID LLEnvironment::KNOWN_SKY_MIDNIGHT("8a01b97a-cb20-c1ea-ac63-f7ea84ad0090");
#endif

//static
S32 LLEnvironment::sBeaconsUsers = 0;

//static
void LLEnvironment::delBeaconsUser()
{
	if (--sBeaconsUsers <= 0)
	{
		sBeaconsUsers = 0;
		gSavedSettings.setBool("sunbeacon", false);
		gSavedSettings.setBool("moonbeacon", false);
	}
}

LLEnvironment::LLEnvironment()
:	mInstanceValid(false),
	mCloudScrollPaused(false),
	mSelectedEnvironment(ENV_LOCAL),
	mCurrentTrack(1),
	mLastCamYaw(0.f)
{
}

void LLEnvironment::initClass()
{
	llinfos << "Initializing." << llendl;

	LLSettingsSky::ptr_t p_default_sky = LLEnvSettingsSky::buildDefaultSky();
	LLSettingsWater::ptr_t p_default_water =
		LLEnvSettingsWater::buildDefaultWater();

	mCurrentEnvironment = std::make_shared<DayInstance>(ENV_DEFAULT);
	mCurrentEnvironment->setSky(p_default_sky);
	mCurrentEnvironment->setWater(p_default_water);

	mEnvironments[ENV_DEFAULT] = mCurrentEnvironment;

	requestParcel(INVALID_PARCEL_ID);

	mRegionChangedConnection =
		gAgent.addRegionChangedCB(boost::bind(&LLEnvironment::onRegionChange,
											  this));
	mParcelChangedConnection =
		gViewerParcelMgr.addAgentParcelChangedCB(boost::bind(&LLEnvironment::onParcelChange,
												 this));
	mPositionChangedConnection =
		gAgent.setPosChangeCallback(boost::bind(&LLEnvironment::onAgentPositionHasChanged,
												this, _1));

	// Note: a call to LLEnvironment:requestRegion() is also performed on
	// region info updates, from LLViewerRegion::processRegionInfo(). This
	// replaces the LLRegionInfoModel update callback used in LL's viewer.

	if (!gGenericDispatcher.isHandlerPresent(MESSAGE_PUSHENVIRONMENT))
	{
		gGenericDispatcher.addHandler(MESSAGE_PUSHENVIRONMENT,
									  &sEnvPushDispatcher);
	}

	LLEventPump& pump = gEventPumps.obtain(LISTENER_NAME);
	pump.listen(LISTENER_NAME,
				boost::bind(&LLEnvironment::listenExperiencePump, this, _1));

	mInstanceValid = true;
}

void LLEnvironment::cleanupClass()
{
	llinfos << "Cleaning up." << llendl;
	mInstanceValid = false;
	LLEventPump& pump = gEventPumps.obtain(PUMP_EXPERIENCE);
	pump.stopListening(LISTENER_NAME);
	mRegionChangedConnection.disconnect();
	mParcelChangedConnection.disconnect();
	mPositionChangedConnection.disconnect();
}

const LLSettingsSky::ptr_t& LLEnvironment::getCurrentSky() const
{
	const LLSettingsSky::ptr_t& skyp = mCurrentEnvironment->getSky();
	if (!skyp && mCurrentEnvironment->getEnvironmentSelection() >= ENV_EDIT)
	{
		for (U32 idx = 0; idx < (U32)ENV_END; ++idx)
		{
			if (mEnvironments[idx]->getSky())
			{
				return mEnvironments[idx]->getSky();
			}
		}
	}
	return skyp;
}

const LLSettingsWater::ptr_t& LLEnvironment::getCurrentWater() const
{
	const LLSettingsWater::ptr_t& waterp = mCurrentEnvironment->getWater();
	if (!waterp && mCurrentEnvironment->getEnvironmentSelection() >= ENV_EDIT)
	{
		for (U32 idx = 0; idx < (U32)ENV_END; ++idx)
		{
			if (mEnvironments[idx]->getWater())
			{
				return mEnvironments[idx]->getWater();
			}
		}
	}
	return waterp;
}

//static
bool LLEnvironment::canAgentUpdateParcelEnvironment(LLParcel* parcel)
{
	if (!parcel)
	{
		parcel = gViewerParcelMgr.getSelectedOrAgentParcel();
	}
	if (!parcel || !isExtendedEnvironmentEnabled())
	{
		return false;
	}

	if (gAgent.isGodlike())
	{
		return true;
	}

	return parcel->getRegionAllowEnvironmentOverride() &&
		   LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														GP_LAND_ALLOW_ENVIRONMENT);
}

//static
bool LLEnvironment::canAgentUpdateRegionEnvironment()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	return regionp && (regionp->canManageEstate() || gAgent.isGodlike());
}

//static
bool LLEnvironment::isExtendedEnvironmentEnabled()
{
	return gAgent.hasRegionCapability("ExtEnvironment");
}

//static
bool LLEnvironment::isInventoryEnabled()
{
	return gAgent.hasRegionCapability("UpdateSettingsAgentInventory") &&
		   gAgent.hasRegionCapability("UpdateSettingsTaskInventory");
}

void LLEnvironment::onRegionChange()
{
	// For now environmental experiences do not survive region crossings
	clearExperienceEnvironment(LLUUID::null, TRANSITION_DEFAULT);

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp)
	{
		return;
	}
	if (regionp->capabilitiesReceived())
	{
		requestParcel(INVALID_PARCEL_ID);
		return;
	}

	regionp->setCapsReceivedCB(boost::bind(&LLEnvironment::requestRegion,
										   env_apply_fn()));
}

void LLEnvironment::onParcelChange()
{
	S32 parcel_id = INVALID_PARCEL_ID;
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (parcel)
	{
		parcel_id = parcel->getLocalID();
	}
	requestParcel(parcel_id);
}

F32 LLEnvironment::getCamHeight() const
{
#if LL_VARIABLE_SKY_DOME_SIZE
	if (!mCurrentEnvironment || !mCurrentEnvironment->getSky())
	{
		return SKY_DOME_OFFSET * SKY_DOME_RADIUS;
	}
	return mCurrentEnvironment->getSky()->getDomeOffset() *
		   mCurrentEnvironment->getSky()->getDomeRadius();
#else
	return SKY_DOME_OFFSET * SKY_DOME_RADIUS;
#endif
}

F32 LLEnvironment::getWaterHeight() const
{
	LLViewerRegion* regionp = gAgent.getRegion();
	return regionp ? regionp->getWaterHeight() : 0.f;
}

bool LLEnvironment::getIsSunUp() const
{
	return mCurrentEnvironment && mCurrentEnvironment->getSky() &&
		   mCurrentEnvironment->getSky()->getIsSunUp();
}

bool LLEnvironment::getIsMoonUp() const
{
	return mCurrentEnvironment && mCurrentEnvironment->getSky() &&
		   mCurrentEnvironment->getSky()->getIsMoonUp();
}

// Now, the silly thing to get a time-fixed environment is that we MUST use a
// blender, set its position (=time of day) and immediately apply the resulting
// sky and water as the new (fixed) environment. The motto at LL is obviously:
// Let's make simple things complicated !  HB
void LLEnvironment::setFixedTimeOfDay(F32 position)
{
	if (position < 0.f || position > 1.f)
	{
		return;
	}
	DayInstance::ptr_t envp = getEnvironmentInstance(mSelectedEnvironment,
													 true);
	LLSettingsDay::ptr_t dayp = envp->getDayCycle();
	if (!dayp)
	{
		envp->setDay(LLEnvSettingsDay::buildDefaultDayCycle(), 14400, 0);
		dayp = envp->getDayCycle();
	}
	S32 sky_track = mCurrentTrack;
	if (mCurrentTrack == LLSettingsDay::TRACK_WATER)
	{
		// Use the ground level track for the sky track if we are currently
		// under water.
		sky_track = LLSettingsDay::TRACK_GROUND_LEVEL;
	}
	LLSettingsSky::ptr_t skyp = envp->getSky();
	LLTrackBlenderLoopingManual::ptr_t sky_blender =
		std::make_shared<LLTrackBlenderLoopingManual>(skyp, dayp, sky_track);
	LLSettingsWater::ptr_t waterp = envp->getWater();
	LLTrackBlenderLoopingManual::ptr_t water_blender =
		std::make_shared<LLTrackBlenderLoopingManual>(waterp, dayp,
													  LLSettingsDay::TRACK_WATER);
	sky_blender->setPosition(position);
	water_blender->setPosition(position);
	setEnvironment(mSelectedEnvironment, skyp, waterp);
	updateEnvironment(TRANSITION_INSTANT, true);
}

void LLEnvironment::setLocalEnvFromDefaultWindlightDay(F32 position)
{
	LLSettingsDay::ptr_t dayp = LLEnvSettingsDay::buildDefaultDayCycle();
	if (!dayp)	// Should never happen...
	{
		return;
	}

	if (mCurrentTrack > LLSettingsDay::TRACK_GROUND_LEVEL)
	{
		// There is only one sky track in Windlight...
		mCurrentTrack = LLSettingsDay::TRACK_GROUND_LEVEL;
	}
	setEnvironment(ENV_LOCAL, dayp, 14400, 0);
	updateEnvironment(TRANSITION_INSTANT);
	setSelectedEnvironment(ENV_LOCAL);

	setFixedTimeOfDay(position);
}

//static
void LLEnvironment::setSunrise()
{
#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
	if (isExtendedEnvironmentEnabled())
	{
		gEnvironment.setEnvironment(ENV_LOCAL, KNOWN_SKY_SUNRISE,
									TRANSITION_INSTANT);
		gEnvironment.setSelectedEnvironment(ENV_LOCAL, TRANSITION_INSTANT);
	}
	else
#endif
	{
		gEnvironment.setLocalEnvFromDefaultWindlightDay(0.25f);
	}
}

//static
void LLEnvironment::setMidday()
{
#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
	if (isExtendedEnvironmentEnabled())
	{
		gEnvironment.setEnvironment(ENV_LOCAL, KNOWN_SKY_MIDDAY,
									TRANSITION_INSTANT);
		gEnvironment.setSelectedEnvironment(ENV_LOCAL, TRANSITION_INSTANT);
	}
	else
#endif
	{
		gEnvironment.setLocalEnvFromDefaultWindlightDay(0.567f);
	}
}

//static
void LLEnvironment::setSunset()
{
#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
	if (isExtendedEnvironmentEnabled())
	{
		gEnvironment.setEnvironment(ENV_LOCAL, KNOWN_SKY_SUNSET,
									TRANSITION_INSTANT);
		gEnvironment.setSelectedEnvironment(ENV_LOCAL, TRANSITION_INSTANT);
	}
	else
#endif
	{
		gEnvironment.setLocalEnvFromDefaultWindlightDay(0.75f);
	}
}

//static
void LLEnvironment::setMidnight()
{
#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
	if (isExtendedEnvironmentEnabled())
	{
		gEnvironment.setEnvironment(ENV_LOCAL, KNOWN_SKY_MIDNIGHT,
									TRANSITION_INSTANT);
		gEnvironment.setSelectedEnvironment(ENV_LOCAL, TRANSITION_INSTANT);
	}
	else
#endif
	{
		gEnvironment.setLocalEnvFromDefaultWindlightDay(0.f);
	}
}

//static
void LLEnvironment::setRegion()
{
	if (!isExtendedEnvironmentEnabled())
	{
		gEnvironment.setLocalEnvFromDefaultWindlightDay();
		return;
	}
	gEnvironment.onParcelChange();
	gEnvironment.clearEnvironment(ENV_LOCAL);
	gEnvironment.setSelectedEnvironment(ENV_PARCEL);
}

void LLEnvironment::setSelectedEnvironment(EEnvSelection env, F64 transition,
										   bool forced)
{
	// This is only here so that we can transition from parcel/region to local
	// when applying local settings from inventory, RestrainedLove or Lua. The
	// logic implemented in llviewercontrol.cpp will then care for changing
	// UseParcelEnvironment accordingly and closing the floaters that need to
	// be closed.
	static LLCachedControl<bool> local(gSavedSettings, "UseLocalEnvironment");
	if (env == ENV_LOCAL && !local)
	{
		gSavedSettings.setBool("UseLocalEnvironment", true);
	}

	mSelectedEnvironment = env;
	updateEnvironment(transition, forced);
}

bool LLEnvironment::hasEnvironment(EEnvSelection env)
{
	return env >= ENV_EDIT && env < ENV_DEFAULT && mEnvironments[env];
}

LLEnvironment::DayInstance::ptr_t
	LLEnvironment::getEnvironmentInstance(LLEnvironment::EEnvSelection env,
										  bool create)
{
	DayInstance::ptr_t environment = mEnvironments[env];
	if (create)
	{
		if (environment)
		{
			environment = environment->clone();
		}
		else if (env == ENV_PUSH)
		{
			environment = std::make_shared<LLDayInjection>(env);
		}
		else
		{
			environment = std::make_shared<DayInstance>(env);
		}
		mEnvironments[env] = environment;
	}
	return environment;
}

void LLEnvironment::setCurrentEnvironmentSelection(EEnvSelection env)
{
	mCurrentEnvironment->setEnvironmentSelection(env);
}

void LLEnvironment::setEnvironment(EEnvSelection env,
								   const LLSettingsDay::ptr_t& dayp,
								   S32 daylength, S32 dayoffset,
								   S32 env_version)
{
	if (env < ENV_EDIT || env >= ENV_DEFAULT)
	{
		llwarns << "Attempted to change invalid environment selection. Aborted."
				<< llendl;
		return;
	}

	DayInstance::ptr_t environment = getEnvironmentInstance(env, true);
	environment->clear();
	environment->setDay(dayp, daylength, dayoffset);
	environment->setSkyTrack(mCurrentTrack);
	environment->animate();

	if (!mSignalEnvChanged.empty())
	{
		mSignalEnvChanged(env, env_version);
	}
}

void LLEnvironment::setEnvironment(EEnvSelection env, fixed_env_t fixed,
								   S32 env_version)
{
	if (env < ENV_EDIT || env >= ENV_DEFAULT)
	{
		llwarns << "Attempted to change invalid environment selection. Aborted."
				<< llendl;
		return;
	}

	DayInstance::ptr_t environment = getEnvironmentInstance(env, true);

	if (fixed.first)
	{
		environment->setSky(fixed.first);
		environment->setFlags(DayInstance::NO_ANIMATE_SKY);
	}
	else if (!environment->getSky())
	{
		if (mCurrentEnvironment->getEnvironmentSelection() != ENV_NONE)
		{
			// Note: this looks suspicious. Should not we assign whole day if
			// mCurrentEnvironment has whole day ?  And then add water/sky on
			// top. This looks like it will result in sky using single keyframe
			// instead of whole day if day is present when setting static water
			// without static sky.
			environment->setSky(mCurrentEnvironment->getSky());
			environment->setFlags(DayInstance::NO_ANIMATE_SKY);
		}
		else
		{
			// Environment is not properly initialized yet, but we should have
			// environment by this point.
			DayInstance::ptr_t substitute = getEnvironmentInstance(ENV_PARCEL,
																   true);
			if (!substitute || !substitute->getSky())
			{
				substitute = getEnvironmentInstance(ENV_REGION, true);
				if (!substitute || !substitute->getSky())
				{
					substitute = getEnvironmentInstance(ENV_DEFAULT, true);
				}
			}
			if (substitute && substitute->getSky())
			{
				environment->setSky(substitute->getSky());
				environment->setFlags(DayInstance::NO_ANIMATE_SKY);
			}
			else
			{
				llwarns << "Failed to assign substitute sky: environment is not properly initialized"
						<< llendl;
			}
		}
	}

	if (fixed.second)
	{
		environment->setWater(fixed.second);
		environment->setFlags(DayInstance::NO_ANIMATE_WATER);
	}
	else if (!environment->getWater())
	{
		if (mCurrentEnvironment->getEnvironmentSelection() != ENV_NONE)
		{
			// Note: this looks suspicious. Should not we assign whole day if
			// mCurrentEnvironment has whole day ?  And then add water/sky on
			// top. This looks like it will result in water using single
			// keyframe instead of whole day if day is present when setting
			// static sky without static water.
			environment->setWater(mCurrentEnvironment->getWater());
			environment->setFlags(DayInstance::NO_ANIMATE_WATER);
		}
		else
		{
			// Environment is not properly initialized yet, but we should have
			// environment by this point.
			DayInstance::ptr_t substitute = getEnvironmentInstance(ENV_PARCEL,
																   true);
			if (!substitute || !substitute->getWater())
			{
				substitute = getEnvironmentInstance(ENV_REGION, true);
				if (!substitute || !substitute->getWater())
				{
					substitute = getEnvironmentInstance(ENV_DEFAULT, true);
				}
			}
			if (substitute && substitute->getWater())
			{
				environment->setWater(substitute->getWater());
				environment->setFlags(DayInstance::NO_ANIMATE_WATER);
			}
			else
			{
				llwarns << "Failed to assign substitute water: environment is not properly initialized"
						<< llendl;
			}
		}
	}

	if (!mSignalEnvChanged.empty())
	{
		mSignalEnvChanged(env, env_version);
	}

	// *TODO: readjust environment
}

void LLEnvironment::setEnvironment(EEnvSelection env,
								   const LLSettingsBase::ptr_t& settings,
								   S32 env_version)
{
	if (env == ENV_DEFAULT)
	{
		llwarns << "Attempt to set default environment. Forbidden." << llendl;
		return;
	}
	if (!settings)
	{
		clearEnvironment(env);
		return;
	}

	DayInstance::ptr_t environment = getEnvironmentInstance(env);
	std::string settings_type = settings->getSettingsType();
	if (settings_type == "daycycle")
	{
		S32 daylength = LLSettingsDay::DEFAULT_DAYLENGTH;
		S32 dayoffset = LLSettingsDay::DEFAULT_DAYOFFSET;
		if (environment)
		{
			daylength = environment->getDayLength();
			dayoffset = environment->getDayOffset();
		}
		setEnvironment(env,
					   std::static_pointer_cast<LLSettingsDay>(settings),
					   daylength, dayoffset);
	}
	else if (settings_type == "sky")
	{
		fixed_env_t fixedenv(std::static_pointer_cast<LLSettingsSky>(settings),
							 LLSettingsWater::ptr_t());
		setEnvironment(env, fixedenv);
	}
	else if (settings_type == "water")
	{
		fixed_env_t fixedenv(LLSettingsSky::ptr_t(),
							 std::static_pointer_cast<LLSettingsWater>(settings));
		setEnvironment(env, fixedenv);
	}
}

void LLEnvironment::setEnvironment(EEnvSelection env, const LLUUID& asset_id,
								   S32 transition)
{
	if (!isExtendedEnvironmentEnabled())
	{
		return;
	}
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										boost::bind(&LLEnvironment::onSetEnvAssetLoaded,
													this, env, transition,
													_1, _2, _3));
}

void LLEnvironment::onSetEnvAssetLoaded(EEnvSelection env, S32 transition,
										const LLUUID& asset_id,
										LLSettingsBase::ptr_t settings,
										S32 status)
{
	if (!settings || status)
	{
		LLSD args;
		args["DESC"] = LLTrans::getString("asset_id") + ": " +
					   asset_id.asString();
		gNotifications.add("FailedToFindSettings", args);
		return;
	}

	setEnvironment(env, settings);
	updateEnvironment(transition);
}

void LLEnvironment::clearEnvironment(EEnvSelection env)
{
	if (env < ENV_EDIT || env >= ENV_DEFAULT)
	{
		llwarns << "Attempt to change invalid environment selection. Aborted."
				<< llendl;
		return;
	}

	mEnvironments[env].reset();

	if (!mSignalEnvChanged.empty())
	{
		mSignalEnvChanged(env, VERSION_CLEANUP);
	}

	// *TODO: readjust environment
}

LLSettingsDay::ptr_t LLEnvironment::getEnvironmentDay(EEnvSelection env)
{
	if (env >= ENV_EDIT && env < ENV_DEFAULT)
	{
		DayInstance::ptr_t environment = getEnvironmentInstance(env);
		if (environment)
		{
			return environment->getDayCycle();
		}
	}
	else
	{
		llwarns << "Attempt to retrieve invalid environment selection. Aborted."
				<< llendl;
	}
	return LLSettingsDay::ptr_t();
}

S32 LLEnvironment::getEnvironmentDayLength(EEnvSelection env)
{
	if (env >= ENV_EDIT && env < ENV_DEFAULT)
	{
		DayInstance::ptr_t environment = getEnvironmentInstance(env);
		if (environment)
		{
			return environment->getDayLength();
		}
	}
	else
	{
		llwarns << "Attempt to retrieve invalid environment selection. Aborted."
				<< llendl;
	}
	return 0;
}

S32 LLEnvironment::getEnvironmentDayOffset(EEnvSelection env)
{
	if (env >= ENV_EDIT && env < ENV_DEFAULT)
	{
		DayInstance::ptr_t environment = getEnvironmentInstance(env);
		if (environment)
		{
			return environment->getDayOffset();
		}
	}
	else
	{
		llwarns << "Attempt to retrieve invalid environment selection. Aborted."
				<< llendl;
	}
	return 0;
}

LLEnvironment::fixed_env_t LLEnvironment::getEnvironmentFixed(EEnvSelection env,
															  bool resolve)
{
	if (env == ENV_CURRENT || resolve)
	{
		fixed_env_t fixed;
		for (U32 idx = resolve ? env : mSelectedEnvironment;
			 idx < (U32)ENV_END; ++idx)
		{
			if (fixed.first && fixed.second)
			{
				break;
			}

			if (idx == ENV_EDIT)
			{
				continue;   // Skip the edit environment.
			}

			DayInstance::ptr_t environment =
				getEnvironmentInstance((EEnvSelection)idx);
			if (environment)
			{
				if (!fixed.first)
				{
					fixed.first = environment->getSky();
				}
				if (!fixed.second)
				{
					fixed.second = environment->getWater();
				}
			}
		}

		if (!fixed.first || !fixed.second)
		{
			llwarns << "Cannot construct complete fixed environment. Missing sky or water."
					<< llendl;
		}

		return fixed;
	}

	if (env >= ENV_EDIT && env <= ENV_DEFAULT)
	{
		DayInstance::ptr_t environment = getEnvironmentInstance(env);
		if (environment)
		{
			return fixed_env_t(environment->getSky(), environment->getWater());
		}
	}
	else
	{
		llwarns << "Attempt to retrieve invalid environment selection. Aborted."
				<< llendl;
	}

	return fixed_env_t();
}

LLEnvironment::DayInstance::ptr_t LLEnvironment::getSelectedEnvironmentInstance()
{
	for (U32 idx = mSelectedEnvironment; idx < (U32)ENV_DEFAULT; ++idx)
	{
		if (mEnvironments[idx])
		{
			return mEnvironments[idx];
		}
	}
	return mEnvironments[ENV_DEFAULT];
}

LLEnvironment::DayInstance::ptr_t LLEnvironment::getSharedEnvironmentInstance()
{
	for (U32 idx = ENV_PARCEL; idx < (U32)ENV_DEFAULT; ++idx)
	{
		if (mEnvironments[idx])
		{
			return mEnvironments[idx];
		}
	}
	return mEnvironments[ENV_DEFAULT];
}

void LLEnvironment::updateEnvironment(F64 transition, bool forced)
{
	DayInstance::ptr_t instancep = getSelectedEnvironmentInstance();
	if (!forced && instancep == mCurrentEnvironment)
	{
		return;
	}

	if (transition == TRANSITION_INSTANT)
	{
		mCurrentEnvironment = instancep;
		updateSettingsUniforms();
		return;
	}

	DayInstance::ptr_t trans =
		std::make_shared<DayTransition>(mCurrentEnvironment->getSky(),
										mCurrentEnvironment->getWater(),
										instancep, transition);
	trans->animate();
	mCurrentEnvironment = trans;
	updateSettingsUniforms();
}

LLVector3 LLEnvironment::getLightDirection() const
{
	const LLSettingsSky::ptr_t& skyp = mCurrentEnvironment->getSky();
	return skyp ? skyp->getLightDirection() : LLVector3::z_axis;
}

LLVector3 LLEnvironment::getSunDirection() const
{
	const LLSettingsSky::ptr_t& skyp = mCurrentEnvironment->getSky();
	return skyp ? skyp->getSunDirection() : LLVector3::z_axis;
}

LLVector3 LLEnvironment::getMoonDirection() const
{
	const LLSettingsSky::ptr_t& skyp = mCurrentEnvironment->getSky();
	return skyp ? skyp->getMoonDirection() : -LLVector3::z_axis;
}

LLVector4 LLEnvironment::getClampedLightNorm() const
{
	LLVector3 light_direction = getLightDirection();
	if (light_direction.mV[2] < -0.1f)
	{
		light_direction.mV[2] = -0.1f;
	}
	return toLightNorm(light_direction);
}

LLVector4 LLEnvironment::getClampedSunNorm() const
{
	LLVector3 light_direction = getSunDirection();
	if (light_direction.mV[2] < -0.1f)
	{
		light_direction.mV[2] = -0.1f;
	}
	return toLightNorm(light_direction);
}

LLVector4 LLEnvironment::getClampedMoonNorm() const
{
	LLVector3 light_direction = getMoonDirection();
	if (light_direction.mV[2] < -0.1f)
	{
		light_direction.mV[2] = -0.1f;
	}
	return toLightNorm(light_direction);
}

LLVector4 LLEnvironment::getRotatedLightNorm() const
{
	LLVector3 light_direction = getLightDirection();
	light_direction *= LLQuaternion(-mLastCamYaw, LLVector3::y_axis);
	return toLightNorm(light_direction);
}

void LLEnvironment::update(const LLViewerCamera* cam)
{
	LL_TRACY_TIMER(TRC_ENVIRONMENT_UPDATE);
	static LLFrameTimer timer;
	F32 delta = timer.getElapsedTimeAndResetF32();

	// Make sure the current environment does not go away until applyTimeDelta
	// is done.
	DayInstance::ptr_t keeper = mCurrentEnvironment;
	mCurrentEnvironment->applyTimeDelta(delta);

	// Update clouds, Sun and general
	updateCloudScroll();

	// Cache this for use in rotating the rotated light vec for shader param
	// updates later...
	mLastCamYaw = cam->getYaw() + SUN_DELTA_YAW;

	stop_glerror();

	updateSettingsUniforms();

	// *TODO (potential optimization): this block may only need to be executed
	// at some times. For example for water shaders only.

	bool can_use_shaders = gPipeline.canUseWindLightShaders();
	if (!can_use_shaders)
	{
		// We still need to update uniforms for these shaders...
		gWLSunProgram.mUniformsDirty = true;
		gWLMoonProgram.mUniformsDirty = true;
	}

	for (LLViewerShaderMgr::shader_iter it = gViewerShaderMgrp->beginShaders(),
										end = gViewerShaderMgrp->endShaders();
		 it != end; ++it)
	{
		if (!it->mProgramObject)
		{
			continue;
		}
#if 1
		const LLShaderFeatures& features = it->mFeatures;
		// NOTE: when hasAtmospherics is true then calculatesAtmospherics is
		// also true, so we only test for the latter... HB
		if (!features.calculatesAtmospherics && !features.hasGamma &&
			!features.hasTransport && !features.hasWaterFog)
		{
			continue;
		}
#endif
		if (can_use_shaders || it->mShaderGroup == LLGLSLShader::SG_WATER)
		{
			it->mUniformsDirty = true;
		}
	}
}

void LLEnvironment::updateCloudScroll()
{
	// This is a function of the environment rather than the sky, since it
	// should persist through sky transitions.
	static LLTimer s_cloud_timer;
	if (!mCloudScrollPaused && mCurrentEnvironment &&
		mCurrentEnvironment->getSky())
	{
		F64 delta_t = s_cloud_timer.getElapsedTimeAndResetF64() / 100.f;
		mCloudScrollDelta +=
			mCurrentEnvironment->getSky()->getCloudScrollRate() * delta_t;
	}
}

//static
void LLEnvironment::updateGLVariablesForSettings(LLShaderUniforms* uniforms,
												 const LLSettingsBase::ptr_t& settings)
{
	for (U32 i = 0; i < (U32)LLGLSLShader::SG_COUNT; ++i)
	{
		uniforms[i].clear();
	}

	LLShaderUniforms* shader = &uniforms[LLGLSLShader::SG_ANY];

	const LLSettingsBase::parammapping_t& params = settings->getParameterMap();
	LLSD::map_const_iterator settings_end = settings->mSettings.endMap();

	// Check for SETTING_LEGACY_HAZE sub-map: only sky settings may have it. HB
	LLSD::map_const_iterator haze_it, haze_end, param_it;
	bool has_legacy_haze =
		settings->getSettingsTypeValue() == LLSettingsType::ST_SKY;
	if (has_legacy_haze)
	{
		haze_it = settings->mSettings.find(LLSettingsSky::SETTING_LEGACY_HAZE);
		has_legacy_haze = haze_it != settings_end;
		if (has_legacy_haze)
		{
			haze_end = haze_it->second.endMap();
		}
	}

	for (const auto& it : params)
	{
		const std::string& key = it.first;
		LLSD value;
		// Legacy first since it contains ambient color and we prioritize value
		// from legacy; see getAmbientColor().
		if (has_legacy_haze)
		{
			param_it = haze_it->second.find(key);
			if (param_it != haze_end)
			{
				value = param_it->second;
			}
		}
		if (value.isUndefined())
		{
			param_it = settings->mSettings.find(key);
			if (param_it != settings_end)
			{
				value = param_it->second;
			}
		}
		if (value.isUndefined())
		{
			// We need to reset shaders, use defaults
			value = it.second.getDefaultValue();
		}

		LLSD::Type setting_type = value.type();
		stop_glerror();
		switch (setting_type)
		{
			case LLSD::TypeInteger:
				shader->uniform1i(it.second.getShaderKey(), value.asInteger());
				break;

			case LLSD::TypeReal:
				shader->uniform1f(it.second.getShaderKey(), value.asReal());
				break;

			case LLSD::TypeBoolean:
				shader->uniform1i(it.second.getShaderKey(),
								  value.asBoolean() ? 1 : 0);
				break;

			case LLSD::TypeArray:
				shader->uniform4fv(it.second.getShaderKey(),
								   LLVector4(value));
			default:
				break;
		}
	}

	settings->applySpecial(uniforms);
}

void LLEnvironment::updateShaderUniforms(LLGLSLShader* shader)
{
	// Apply uniforms that should be applied to all shaders
	mSkyUniforms[LLGLSLShader::SG_ANY].apply(shader);
	mWaterUniforms[LLGLSLShader::SG_ANY].apply(shader);

	// Apply uniforms specific to the given shader's shader group
	S32 group = shader->mShaderGroup;
	mSkyUniforms[group].apply(shader);
	mWaterUniforms[group].apply(shader);
}

void LLEnvironment::updateShaderSkyUniforms(LLGLSLShader* shader)
{
	// Apply uniforms that should be applied to all shaders
	mSkyUniforms[LLGLSLShader::SG_ANY].apply(shader);

	// Apply uniforms specific to the given shader's shader group
	S32 group = shader->mShaderGroup;
	mSkyUniforms[group].apply(shader);
}

void LLEnvironment::updateSettingsUniforms()
{
	if (mCurrentEnvironment->getWater())
	{
		updateGLVariablesForSettings(mWaterUniforms,
									 mCurrentEnvironment->getWater());
	}
	if (mCurrentEnvironment->getSky())
	{
		updateGLVariablesForSettings(mSkyUniforms,
									 mCurrentEnvironment->getSky());
	}
}

void LLEnvironment::recordEnvironment(S32 parcel_id, envinfo_ptr_t envinfo,
									  F64 transition)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	if (regionp->getRegionID() != envinfo->mRegionId &&
		envinfo->mRegionId.notNull())
	{
		llinfos << "Requested environment region id: " << envinfo->mRegionId
				<< " while agent is on: " << regionp->getRegionID() << llendl;
		return;
	}

	if (envinfo->mParcelId == INVALID_PARCEL_ID)
	{
		// The returned info applies to an entire region.
		if (!envinfo->mDayCycle)
		{
			clearEnvironment(ENV_PARCEL);
			setEnvironment(ENV_REGION, LLSettingsDay::getDefaultAssetId());
			updateEnvironment();
		}
		else if (envinfo->mDayCycle->isTrackEmpty(LLSettingsDay::TRACK_WATER) ||
				 envinfo->mDayCycle->isTrackEmpty(LLSettingsDay::TRACK_GROUND_LEVEL))
		{
			llwarns << "Invalid day cycle for region" << llendl;
			clearEnvironment(ENV_PARCEL);
			setEnvironment(ENV_REGION, LLSettingsDay::getDefaultAssetId());
			updateEnvironment();
		}
		else
		{
			mTrackAltitudes = envinfo->mAltitudes;
			// Update track selection based on new altitudes
			mCurrentTrack =
				calculateSkyTrackForAltitude(gAgent.getPositionAgent().mV[VZ]);
			setEnvironment(ENV_REGION, envinfo->mDayCycle, envinfo->mDayLength,
						   envinfo->mDayOffset, envinfo->mEnvVersion);
		}

		LL_DEBUGS("Environment") << "Altitudes set to {" << mTrackAltitudes[0]
								 << ", " << mTrackAltitudes[1] << ", "
								 << mTrackAltitudes[2] << ", "
								 << mTrackAltitudes[3] << LL_ENDL;
	}
	else
	{
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		LL_DEBUGS("Environment") << "Parcel environment #"
								 << envinfo->mParcelId << LL_ENDL;
		if (parcel && parcel->getLocalID() != parcel_id)
		{
			LL_DEBUGS("Environment") << "Requested parcel #" << parcel_id
									 << " while agent is on "
									 << parcel->getLocalID() << LL_ENDL;
			return;
		}

		if (!envinfo->mDayCycle)
		{
			LL_DEBUGS("Environment") << "Clearing environment on parcel #"
									 << parcel_id << LL_ENDL;
			clearEnvironment(ENV_PARCEL);
		}
		else if (envinfo->mDayCycle->isTrackEmpty(LLSettingsDay::TRACK_WATER) ||
				 envinfo->mDayCycle->isTrackEmpty(LLSettingsDay::TRACK_GROUND_LEVEL))
		{
			llwarns << "Invalid day cycle for parcel #" << parcel_id << llendl;
			clearEnvironment(ENV_PARCEL);
		}
		else
		{
			setEnvironment(ENV_PARCEL, envinfo->mDayCycle, envinfo->mDayLength,
						   envinfo->mDayOffset, envinfo->mEnvVersion);
		}
	}

	updateEnvironment(transition);
}

void LLEnvironment::adjustRegionOffset(F32 adjust)
{
	if (isExtendedEnvironmentEnabled())
	{
		llwarns << "Attempt to adjust region offset on EEP region. Legacy regions only."
				<< llendl;
	}

	if (mEnvironments[ENV_REGION])
	{
		F32 day_length = mEnvironments[ENV_REGION]->getDayLength();
		F32 day_offset = mEnvironments[ENV_REGION]->getDayOffset();
		day_offset += adjust * day_length;
		if (day_offset < 0.f)
		{
			day_offset += day_length;
		}
		mEnvironments[ENV_REGION]->setDayOffset(day_offset);
	}
}

//static
void LLEnvironment::requestRegion(env_apply_fn cb)
{
	if (gEnvironment.mInstanceValid)
	{
		gEnvironment.requestParcel(INVALID_PARCEL_ID, cb);
	}
}

void LLEnvironment::updateRegion(const LLSettingsDay::ptr_t& dayp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	updateParcel(INVALID_PARCEL_ID, dayp, day_length, day_offset, altitudes,
				 cb);
}

void LLEnvironment::updateRegion(const LLUUID& asset_id,
								 const std::string& display_name,
								 S32 track_num, S32 day_length, S32 day_offset,
								 U32 flags, altitudes_vect_t altitudes,
								 env_apply_fn cb)
{
	if (isExtendedEnvironmentEnabled())
	{
		updateParcel(INVALID_PARCEL_ID, asset_id, display_name, track_num,
					 day_length, day_offset, flags, altitudes, cb);
	}
	else
	{
		gNotifications.add("NoEnvironmentSettings");
	}
}

void LLEnvironment::updateRegion(const LLSettingsSky::ptr_t& skyp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	updateParcel(INVALID_PARCEL_ID, skyp, day_length, day_offset, altitudes,
				 cb);
}

void LLEnvironment::updateRegion(const LLSettingsWater::ptr_t& waterp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	updateParcel(INVALID_PARCEL_ID, waterp, day_length, day_offset, altitudes,
				 cb);
}

void LLEnvironment::resetRegion(env_apply_fn cb)
{
	resetParcel(INVALID_PARCEL_ID, cb);
}

void LLEnvironment::requestParcel(S32 parcel_id, env_apply_fn cb)
{
	if (!isExtendedEnvironmentEnabled())
	{
		if (parcel_id == INVALID_PARCEL_ID)
		{
			if (!cb)
			{
				F64 transition = gAgent.teleportInProgress() ||
								 gViewerParcelMgr.waitingForParcelInfo() ?
									TRANSITION_FAST : TRANSITION_DEFAULT;
				cb = [this, transition](S32 pid, envinfo_ptr_t info)
				{
					clearEnvironment(ENV_PARCEL);
					recordEnvironment(pid, info, transition);
				};
			}

			LLEnvironmentRequest::initiate(cb);
		}
		else if (cb)
		{
			cb(parcel_id, envinfo_ptr_t());
		}
		return;
	}

	if (!cb)
	{
		F64 transition = gAgent.teleportInProgress() ||
						 gViewerParcelMgr.waitingForParcelInfo() ?
							TRANSITION_FAST : TRANSITION_DEFAULT;
		cb = [this, transition](S32 pid, envinfo_ptr_t info)
		{
			recordEnvironment(pid, info, transition);
		};
	}

	gCoros.launch("LLEnvironment::coroRequestEnvironment",
				  boost::bind(&LLEnvironment::coroRequestEnvironment, this,
							  parcel_id, cb));
}

void LLEnvironment::updateParcel(S32 parcel_id, const LLUUID& asset_id,
								 const std::string& display_name,
								 S32 track_num, S32 day_length, S32 day_offset,
								 U32 flags, altitudes_vect_t altitudes,
								 env_apply_fn cb)
{
	UpdateInfo::ptr_t updates = std::make_shared<UpdateInfo>(asset_id,
															 display_name,
															 day_length,
															 day_offset,
															 altitudes,
															 flags);
	gCoros.launch("LLEnvironment::coroUpdateEnvironment",
				  boost::bind(&LLEnvironment::coroUpdateEnvironment, this,
							  parcel_id, track_num, updates, cb));
}

void LLEnvironment::onUpdateParcelAssetLoaded(const LLUUID& asset_id,
											  LLSettingsBase::ptr_t settings,
											  S32 status, S32 parcel_id,
											  S32 day_length, S32 day_offset,
											  altitudes_vect_t altitudes)
{
	if (status)
	{
		gNotifications.add("FailedToLoadSettingsApply");
		return;
	}

	LLSettingsDay::ptr_t dayp;
	if (settings->getSettingsType() == "daycycle")
	{
		dayp = std::static_pointer_cast<LLSettingsDay>(settings);
	}
	else
	{
		dayp = createDayCycleFromEnvironment(parcel_id == INVALID_PARCEL_ID ?
												ENV_REGION : ENV_PARCEL,
											 settings);
	}
	if (!dayp)
	{
		gNotifications.add("FailedToLoadSettingsApply");
		return;
	}

	updateParcel(parcel_id, dayp, day_length, day_offset, altitudes);
}

void LLEnvironment::updateParcel(S32 parcel_id,
								 const LLSettingsSky::ptr_t& skyp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	LLSettingsDay::ptr_t dayp =
		createDayCycleFromEnvironment(parcel_id == INVALID_PARCEL_ID ?
										ENV_REGION : ENV_PARCEL,
									  skyp);
	dayp->setFlag(skyp->getFlags());
	updateParcel(parcel_id, dayp, day_length, day_offset, altitudes, cb);
}

void LLEnvironment::updateParcel(S32 parcel_id,
								 const LLSettingsWater::ptr_t& waterp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	LLSettingsDay::ptr_t dayp =
		createDayCycleFromEnvironment(parcel_id == INVALID_PARCEL_ID ?
										ENV_REGION : ENV_PARCEL,
									  waterp);
	dayp->setFlag(waterp->getFlags());
	updateParcel(parcel_id, dayp, day_length, day_offset, altitudes, cb);
}

void LLEnvironment::updateParcel(S32 parcel_id,
								 const LLSettingsDay::ptr_t& dayp,
								 S32 track_num, S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	UpdateInfo::ptr_t updates = std::make_shared<UpdateInfo>(dayp, day_length,
															 day_offset,
															 altitudes);
	gCoros.launch("LLEnvironment::coroUpdateEnvironment",
				  boost::bind(&LLEnvironment::coroUpdateEnvironment, this,
							  parcel_id, track_num, updates, cb));
}

void LLEnvironment::updateParcel(S32 parcel_id,
								 const LLSettingsDay::ptr_t& dayp,
								 S32 day_length, S32 day_offset,
								 altitudes_vect_t altitudes, env_apply_fn cb)
{
	updateParcel(parcel_id, dayp, NO_TRACK, day_length, day_offset, altitudes,
				 cb);
}

void LLEnvironment::resetParcel(S32 parcel_id, env_apply_fn cb)
{
	gCoros.launch("LLEnvironment::coroResetEnvironment",
				  boost::bind(&LLEnvironment::coroResetEnvironment, this,
							  parcel_id, NO_TRACK, cb));
}

void LLEnvironment::coroRequestEnvironment(S32 parcel_id, env_apply_fn apply)
{
	std::string url = gAgent.getRegionCapability("ExtEnvironment");
	if (url.empty())
	{
		return;
	}
	LL_DEBUGS("Environment") << "Requesting for parcel_id=" << parcel_id
							 << LL_ENDL;
	if (parcel_id != INVALID_PARCEL_ID)
	{
		url += llformat("?parcelid=%d", parcel_id);
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("RequestEnvironment");
	LLSD result = adapter.getAndSuspend(url);

	if (LLApp::isExiting())
	{
		return;
	}

	// Results that come back may contain the new settings

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Could not retrieve environment settings for "
				<< (parcel_id == INVALID_PARCEL_ID ? "region" : "parcel")
				<< llendl;
		return;
	}

	LLSD environment = result[KEY_ENVIRONMENT];
	if (environment.isDefined() && apply)
	{
		envinfo_ptr_t envinfo = EnvironmentInfo::extract(environment);
		apply(parcel_id, envinfo);
	}
}

void LLEnvironment::coroUpdateEnvironment(S32 parcel_id, S32 track_no,
										  UpdateInfo::ptr_t updates,
										  env_apply_fn apply)
{

	std::string url = gAgent.getRegionCapability("ExtEnvironment");
	if (url.empty())
	{
		return;
	}

	LLSD body(LLSD::emptyMap());
	body[KEY_ENVIRONMENT] = LLSD::emptyMap();

	if (track_no == NO_TRACK)
	{
		// Day length and offset are only applicable if we are addressing the
		// entire day cycle.
		if (updates->mDayLength > 0)
		{
			body[KEY_ENVIRONMENT][KEY_DAYLENGTH] = updates->mDayLength;
		}
		if (updates->mDayOffset > 0)
		{
			body[KEY_ENVIRONMENT][KEY_DAYOFFSET] = updates->mDayOffset;
		}

		if (parcel_id == INVALID_PARCEL_ID && updates->mAltitudes.size() == 3)
		{
			// Only test for altitude changes if we are changing the region.
			body[KEY_ENVIRONMENT][KEY_TRACKALTS] = LLSD::emptyArray();
			for (S32 i = 0; i < 3; ++i)
			{
				body[KEY_ENVIRONMENT][KEY_TRACKALTS][i] =
					updates->mAltitudes[i];
			}
		}
	}

	if (updates->mDayp)
	{
		body[KEY_ENVIRONMENT][KEY_DAYCYCLE] = updates->mDayp->getSettings();
	}
	else if (!updates->mSettingsAsset.isNull())
	{
		body[KEY_ENVIRONMENT][KEY_DAYASSET] = updates->mSettingsAsset;
		if (!updates->mDayName.empty())
		{
			body[KEY_ENVIRONMENT][KEY_DAYNAME] = updates->mDayName;
		}
	}

	body[KEY_ENVIRONMENT][KEY_FLAGS] = LLSD::Integer(updates->mFlags);

	if (parcel_id != INVALID_PARCEL_ID || track_no != NO_TRACK)
	{
		url += '?';
		if (parcel_id != INVALID_PARCEL_ID)
		{
			url += llformat("parcelid=%d", parcel_id);
			if (track_no != NO_TRACK)
			{
				url += '&';
			}
		}
		if (track_no != NO_TRACK)
		{
			url += llformat("&trackno=%d", track_no);
		}
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("UpdateEnvironment");
	LLSD result = adapter.putAndSuspend(url, body);

	if (LLApp::isExiting())
	{
		return;
	}

	// Results that come back may contain the new settings

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result["success"].asBoolean())
	{
		LLSD notify = LLSD::emptyMap();
		std::string reason = result["message"].asString();
		if (reason.empty())
		{
			reason = status.toString();
		}
		notify["FAIL_REASON"] = reason;
		gNotifications.add("WLRegionApplyFail", notify);
		return;
	}

	LLSD environment = result[KEY_ENVIRONMENT];
	if (environment.isDefined() && apply)
	{
		envinfo_ptr_t envinfo = EnvironmentInfo::extract(environment);
		apply(parcel_id, envinfo);
	}
}

void LLEnvironment::coroResetEnvironment(S32 parcel_id, S32 track_no,
										 env_apply_fn apply)
{
	std::string url = gAgent.getRegionCapability("ExtEnvironment");
	if (url.empty())
	{
		return;
	}

	if (parcel_id != INVALID_PARCEL_ID || track_no != NO_TRACK)
	{
		url += '?';
		if (parcel_id != INVALID_PARCEL_ID)
		{
			url += llformat("parcelid=%d", parcel_id);
			if (track_no != NO_TRACK)
			{
				url += '&';
			}
		}
		if (track_no != NO_TRACK)
		{
			url += llformat("trackno=%d", track_no);
		}
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("ResetEnvironment");
	LLSD result = adapter.deleteAndSuspend(url);

	if (LLApp::isExiting())
	{
		return;
	}

	// Results that come back may contain the new settings

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || !result["success"].asBoolean())
	{
		llwarns << "Could not reset Windlight settings in "
				<< (parcel_id == INVALID_PARCEL_ID ? "region" : "parcel")
				<< llendl;
		LLSD notify = LLSD::emptyMap();
		std::string reason = result["message"].asString();
		if (reason.empty())
		{
			reason = status.toString();
		}
		notify["FAIL_REASON"] = reason;
		gNotifications.add("WLRegionApplyFail", notify);
		return;
	}

	LLSD environment = result[KEY_ENVIRONMENT];
	if (environment.isDefined() && apply)
	{
		envinfo_ptr_t envinfo = EnvironmentInfo::extract(environment);
		apply(parcel_id, envinfo);
	}
}

LLEnvironment::EnvironmentInfo::EnvironmentInfo()
:	mParcelId(INVALID_PARCEL_ID),
	mDayLength(0),
	mDayOffset(0),
	mDayHash(0),
	mAltitudes({ { 0.f, 0.f, 0.f, 0.f } }),
	mIsDefault(false),
	mIsLegacy(false),
	mEnvVersion(INVALID_PARCEL_ENVIRONMENT_VERSION)
{
}

LLEnvironment::envinfo_ptr_t LLEnvironment::EnvironmentInfo::extract(LLSD env)
{
	ptr_t pinfo = std::make_shared<EnvironmentInfo>();

	pinfo->mIsDefault = !env.has(KEY_ISDEFAULT) ||
						env[KEY_ISDEFAULT].asBoolean();
	pinfo->mParcelId = env.has(KEY_PARCELID) ? env[KEY_PARCELID].asInteger()
											 : INVALID_PARCEL_ID;
	pinfo->mRegionId = env.has(KEY_REGIONID) ? env[KEY_REGIONID].asUUID()
											 : LLUUID::null;
	pinfo->mIsLegacy = false;

	if (env.has(KEY_TRACKALTS))
	{
		for (S32 i = 0; i < 3; ++i)
		{
			pinfo->mAltitudes[i + 1] = env[KEY_TRACKALTS][i].asReal();
		}
		pinfo->mAltitudes[0] = 0.f;
	}

	if (env.has(KEY_DAYCYCLE))
	{
		pinfo->mDayCycle =
			LLEnvSettingsDay::buildFromEnvironmentMessage(env[KEY_DAYCYCLE]);
		pinfo->mDayLength =
			env.has(KEY_DAYLENGTH) ? env[KEY_DAYLENGTH].asInteger() : -1;
		pinfo->mDayOffset =
			env.has(KEY_DAYOFFSET) ? env[KEY_DAYOFFSET].asInteger() : -1;
		pinfo->mDayHash =
			env.has(KEY_DAYHASH) ? env[KEY_DAYHASH].asInteger() : 0;
	}
	else
	{
		pinfo->mDayLength = gEnvironment.getEnvironmentDayLength(ENV_REGION);
		pinfo->mDayOffset = gEnvironment.getEnvironmentDayOffset(ENV_REGION);
	}

	if (env.has(KEY_DAYASSET))
	{
		pinfo->mAssetId = env[KEY_DAYASSET].asUUID();
	}

	if (env.has(KEY_DAYNAMES))
	{
		LLSD daynames = env[KEY_DAYNAMES];
		if (daynames.isArray())
		{
			pinfo->mDayCycleName.clear();
			for (U32 i = 0, count = pinfo->mNameList.size(); i < count; ++i)
			{
				pinfo->mNameList[i] = daynames[i].asString();
			}
		}
		else if (daynames.isString())
		{
			for (std::string& name : pinfo->mNameList)
			{
				name.clear();
			}
			pinfo->mDayCycleName = daynames.asString();
		}
	}
	else if (pinfo->mDayCycle)
	{
		pinfo->mDayCycleName = pinfo->mDayCycle->getName();
	}
	else if (env.has(KEY_DAYNAME))
	{
		pinfo->mDayCycleName = env[KEY_DAYNAME].asString();
	}

	if (env.has(KEY_ENVVERSION))
	{
		LLSD version = env[KEY_ENVVERSION];
		pinfo->mEnvVersion = version.asInteger();
	}
	else
	{
		// Can be used for region, but versions should be same
		pinfo->mEnvVersion =
			pinfo->mIsDefault ? UNSET_PARCEL_ENVIRONMENT_VERSION
							  : INVALID_PARCEL_ENVIRONMENT_VERSION;
	}

	return pinfo;
}

LLEnvironment::envinfo_ptr_t
	LLEnvironment::EnvironmentInfo::extractLegacy(LLSD legacy)
{
	if (!legacy.isArray() || !legacy[0].has("regionID"))
	{
		llwarns << "Invalid legacy settings for environment: " << legacy
				<< llendl;
		return ptr_t();
	}

	ptr_t pinfo = std::make_shared<EnvironmentInfo>();

	pinfo->mIsDefault = false;
	pinfo->mParcelId = INVALID_PARCEL_ID;
	pinfo->mRegionId = legacy[0]["regionID"].asUUID();
	pinfo->mIsLegacy = true;

	pinfo->mDayLength = LLSettingsDay::DEFAULT_DAYLENGTH;
	pinfo->mDayOffset = LLSettingsDay::DEFAULT_DAYOFFSET;
	pinfo->mDayCycle =
		LLEnvSettingsDay::buildFromLegacyMessage(pinfo->mRegionId, legacy[1],
												 legacy[2], legacy[3]);
	if (pinfo->mDayCycle)
	{
		pinfo->mDayHash = pinfo->mDayCycle->getHash();
	}

	pinfo->mAltitudes[0] = pinfo->mAltitudes[1] = 0.f;
	pinfo->mAltitudes[2] = 10001.f;
	pinfo->mAltitudes[3] = 10002.f;
	pinfo->mAltitudes[4] = 10003.f;

	return pinfo;
}

LLSettingsWater::ptr_t
	LLEnvironment::createWaterFromLegacyPreset(const std::string& filename,
											   LLSD& messages)
{
	std::string name = gDirUtilp->getBaseFileName(filename, true);
	std::string file = gDirUtilp->getBaseFileName(filename);
	std::string path = gDirUtilp->getDirName(filename);
	LLSettingsWater::ptr_t water =
		LLEnvSettingsWater::buildFromLegacyPresetFile(name, file, path,
													  messages);
	if (!water)
	{
		messages["NAME"] = name;
		messages["FILE"] = filename;
	}
	return water;
}

LLSettingsSky::ptr_t
	LLEnvironment::createSkyFromLegacyPreset(const std::string& filename,
											 LLSD& messages)
{
	std::string name = gDirUtilp->getBaseFileName(filename, true);
	std::string file = gDirUtilp->getBaseFileName(filename);
	std::string path = gDirUtilp->getDirName(filename);
	LLSettingsSky::ptr_t sky =
		LLEnvSettingsSky::buildFromLegacyPresetFile(name, file, path,
													messages);
	if (!sky)
	{
		messages["NAME"] = name;
		messages["FILE"] = filename;
	}
	return sky;
}

LLSettingsDay::ptr_t
	LLEnvironment::createDayCycleFromLegacyPreset(const std::string& filename,
												  LLSD& messages)
{
	std::string name = gDirUtilp->getBaseFileName(filename, true);
	std::string file = gDirUtilp->getBaseFileName(filename);
	std::string path = gDirUtilp->getDirName(filename);
	LLSettingsDay::ptr_t day =
		LLEnvSettingsDay::buildFromLegacyPresetFile(name, file, path,
													messages);
	if (!day)
	{
		messages["NAME"] = name;
		messages["FILE"] = filename;
	}
	return day;
}

LLSettingsDay::ptr_t
	LLEnvironment::createDayCycleFromEnvironment(EEnvSelection env,
												 LLSettingsBase::ptr_t settings)
{
	std::string type(settings->getSettingsType());

	if (type == "daycycle")
	{
		return std::static_pointer_cast<LLSettingsDay>(settings);
	}

	if (env != ENV_PARCEL && env != ENV_REGION)
	{
		llwarns << "May only create from parcel or region environment."
				<< llendl;
		return LLSettingsDay::ptr_t();
	}

	LLSettingsDay::ptr_t day = getEnvironmentDay(env);
	if (!day && env == ENV_PARCEL)
	{
		day = getEnvironmentDay(ENV_REGION);
	}

	if (!day)
	{
		llwarns << "Could not retrieve existing day settings." << llendl;
		return LLSettingsDay::ptr_t();
	}

	day = day->buildClone();

	if (type == "sky")
	{
		for (S32 i = 1; i < LLSettingsDay::TRACK_MAX; ++i)
		{
			day->clearCycleTrack(i);
		}
		day->setSettingsAtKeyframe(settings, 0.f, 1);
	}
	else if (type == "water")
	{
		day->clearCycleTrack(LLSettingsDay::TRACK_WATER);
		day->setSettingsAtKeyframe(settings, 0.f, LLSettingsDay::TRACK_WATER);
	}

	return day;
}

void LLEnvironment::onAgentPositionHasChanged(const LLVector3& localpos)
{
	S32 trackno = calculateSkyTrackForAltitude(localpos.mV[VZ]);
	if (trackno == mCurrentTrack)
	{
		return;
	}

	mCurrentTrack = trackno;

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || !regionp->capabilitiesReceived())
	{
		// Environment not ready, environment will be updated later, do not
		// cause 'blend' yet. But keep mCurrentTrack updated in case we would
		// not get new altitudes for some reason.
		return;
	}

	for (U32 i = ENV_LOCAL; i < (U32)ENV_DEFAULT; ++i)
	{
		if (mEnvironments[i])
		{
			mEnvironments[i]->setSkyTrack(mCurrentTrack);
		}
	}
}

S32 LLEnvironment::calculateSkyTrackForAltitude(F64 altitude)
{
	altitude_list_t::iterator start = mTrackAltitudes.begin();
	altitude_list_t::iterator end = mTrackAltitudes.end();
	altitude_list_t::iterator it =
		std::find_if_not(start, end,
						 [altitude](F32 test) { return altitude > test; });
	if (it == start)
	{
		return 1;
	}
	if (it == end)
	{
		return 4;
	}
	return llmin((S32)std::distance(start, it), 4);
}

void LLEnvironment::handleEnvironmentPush(LLSD& message)
{
	// Log the experience message
	LLExperienceLog::getInstance()->handleExperienceMessage(message);

	std::string action = message[KEY_ACTION].asString();
	LLUUID experience_id = message[KEY_EXPERIENCEID].asUUID();
	LLSD action_data = message[KEY_ACTIONDATA];
	F32 transition_time = action_data[KEY_TRANSITIONTIME].asReal();

	// *TODO: check here that the viewer thinks the experience is still valid.

	if (action == ACTION_CLEARENVIRONMENT)
	{
		handleEnvironmentPushClear(experience_id, action_data,
								   transition_time);
	}
	else if (action == ACTION_PUSHFULLENVIRONMENT)
	{
		handleEnvironmentPushFull(experience_id, action_data, transition_time);
	}
	else if (action == ACTION_PUSHPARTIALENVIRONMENT)
	{
		handleEnvironmentPushPartial(experience_id, action_data,
									 transition_time);
	}
	else
	{
		llwarns << "Unknown environment push action: " << action << llendl;
	}
}

void LLEnvironment::handleEnvironmentPushClear(const LLUUID& experience_id,
											   LLSD& message, F32 transition)
{
	clearExperienceEnvironment(experience_id, transition);
}

void LLEnvironment::handleEnvironmentPushFull(const LLUUID& experience_id,
											  LLSD& message, F32 transition)
{
	setExperienceEnvironment(experience_id, message[KEY_ASSETID].asUUID(),
							 transition);
}

void LLEnvironment::handleEnvironmentPushPartial(const LLUUID& experience_id,
												 LLSD& message, F32 transition)
{
	if (message.has("settings"))
	{
		setExperienceEnvironment(experience_id, message["settings"],
								 transition);
	}
}

void LLEnvironment::clearExperienceEnvironment(const LLUUID& experience_id,
											   F64 transition_time)
{
	LLDayInjection::ptr_t injection =
		std::dynamic_pointer_cast<LLDayInjection>(getEnvironmentInstance(ENV_PUSH));
	if (injection)
	{
		injection->clearInjections(experience_id, transition_time);
	}
}

void LLEnvironment::setSharedEnvironment()
{
	clearEnvironment(ENV_LOCAL);
	setSelectedEnvironment(ENV_LOCAL);
	updateEnvironment();
}

void LLEnvironment::setExperienceEnvironment(const LLUUID& experience_id,
											 const LLUUID& asset_id,
											 F32 transition_time)
{
	if (!isExtendedEnvironmentEnabled())
	{
		return;
	}
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										[this, experience_id,
										 transition_time](LLUUID asset_id,
														  LLSettingsBase::ptr_t settings,
														  S32 status, LLExtStat)
										{
											onSetExperienceEnvAssetLoaded(experience_id,
																		  settings,
																		  transition_time,
																		   status);
										});
}

void LLEnvironment::onSetExperienceEnvAssetLoaded(const LLUUID& experience_id,
												  LLSettingsBase::ptr_t settings,
												  F32 transition_time,
												  S32 status)
{
	if (!settings || status)
	{
		LLSD args;
		args["DESC"] = LLTrans::getString("experience_id") + ": " +
					   experience_id.asString();
		gNotifications.add("FailedToFindSettings", args);
		return;
	}

	bool update = false;
	LLDayInjection::ptr_t environment =
		std::dynamic_pointer_cast<LLDayInjection>(getEnvironmentInstance(ENV_PUSH));
	if (!environment)
	{
		environment =
			std::dynamic_pointer_cast<LLDayInjection>(getEnvironmentInstance(ENV_PUSH,
																			 true));
		update = true;
	}

	std::string type = settings->getSettingsType();
	if (type == "daycycle")
	{
		environment->setInjectedDay(std::static_pointer_cast<LLSettingsDay>(settings),
									experience_id, transition_time);
	}
	else if (type == "sky")
	{
		environment->setInjectedSky(std::static_pointer_cast<LLSettingsSky>(settings),
									experience_id, transition_time);
	}
	else if (type == "water")
	{
		environment->setInjectedWater(std::static_pointer_cast<LLSettingsWater>(settings),
									  experience_id, transition_time);
	}

	if (update)
	{
		updateEnvironment(TRANSITION_INSTANT, true);
	}
}

void LLEnvironment::setExperienceEnvironment(const LLUUID& experience_id,
											 LLSD data, F32 transition_time)
{
	LLSD sky(data["sky"]);
	LLSD water(data["water"]);
	if (sky.isUndefined() && water.isUndefined())
	{
		clearExperienceEnvironment(experience_id, F64(transition_time));
		return;
	}

	bool update = false;
	LLDayInjection::ptr_t environment =
		std::dynamic_pointer_cast<LLDayInjection>(getEnvironmentInstance(ENV_PUSH));
	if (!environment)
	{
		environment =
			std::dynamic_pointer_cast<LLDayInjection>(getEnvironmentInstance(ENV_PUSH,
																			 true));
		update = true;
	}

	if (!sky.isUndefined())
	{
		environment->injectSkySettings(sky, experience_id, F64(transition_time));
	}

	if (!water.isUndefined())
	{
		environment->injectWaterSettings(sky, experience_id, F64(transition_time));
	}

	if (update)
	{
		updateEnvironment(TRANSITION_INSTANT, true);
	}
}

bool LLEnvironment::listenExperiencePump(const LLSD& message)
{
	if (!message.has("experience"))
	{
		return false;
	}

	LLUUID experience_id = message["experience"];
	LLSD data = message[experience_id.asString()];
	std::string permission(data["permission"].asString());
	if (permission == "Forget" || permission == "Block")
	{
		clearExperienceEnvironment(experience_id,
								   permission == "Block" ? TRANSITION_INSTANT
														 : TRANSITION_FAST);
	}
	return false;
}

LLEnvironment::DayInstance::DayInstance(EEnvSelection env)
:	mDayLength(LLSettingsDay::DEFAULT_DAYLENGTH),
	mDayOffset(LLSettingsDay::DEFAULT_DAYOFFSET),
	mInitialized(false),
	mType(TYPE_INVALID),
	mSkyTrack(1),
	mEnv(env),
	mAnimateFlags(0)
{
}

LLEnvironment::DayInstance::ptr_t LLEnvironment::DayInstance::clone() const
{
	ptr_t environment = std::make_shared<DayInstance>(mEnv);
	environment->mDayCycle = mDayCycle;
	environment->mSky = mSky;
	environment->mWater = mWater;
	environment->mDayLength = mDayLength;
	environment->mDayOffset = mDayOffset;
	environment->mBlenderSky = mBlenderSky;
	environment->mBlenderWater = mBlenderWater;
	environment->mInitialized = mInitialized;
	environment->mType = mType;
	environment->mSkyTrack = mSkyTrack;
	environment->mAnimateFlags = mAnimateFlags;
	return environment;
}

bool LLEnvironment::DayInstance::applyTimeDelta(F64 delta)
{
	// Makes sure that this does not go away while it is being worked on.
	ptr_t keeper(shared_from_this());

	if (!mInitialized)
	{
		initialize();
	}

	bool changed = false;
	if (mBlenderSky)
	{
		changed = mBlenderSky->applyTimeDelta(delta);
	}
	if (mBlenderWater)
	{
		changed |= mBlenderWater->applyTimeDelta(delta);
	}
	return changed;
}

void LLEnvironment::DayInstance::setDay(const LLSettingsDay::ptr_t& dayp,
										S32 daylength, S32 dayoffset)
{
	mType = TYPE_CYCLED;
	mInitialized = false;

	mAnimateFlags = 0;

	mDayCycle = dayp;
	mDayLength = daylength;
	mDayOffset = dayoffset;

	mBlenderSky.reset();
	mBlenderWater.reset();

	mSky = LLEnvSettingsSky::buildDefaultSky();
	mWater = LLEnvSettingsWater::buildDefaultWater();

	animate();
}

void LLEnvironment::DayInstance::setSky(const LLSettingsSky::ptr_t& skyp)
{
	mType = TYPE_FIXED;
	mInitialized = false;

	bool different_sky = mSky != skyp;
	mSky = skyp;
	mSky->mReplaced |= different_sky;
	mSky->update();

	mBlenderSky.reset();
}

void LLEnvironment::DayInstance::setWater(const LLSettingsWater::ptr_t& waterp)
{
	mType = TYPE_FIXED;
	mInitialized = false;

	bool different_water = mWater != waterp;
	mWater = waterp;
	mWater->mReplaced |= different_water;
	mWater->update();

	mBlenderWater.reset();
}

void LLEnvironment::DayInstance::initialize()
{
	mInitialized = true;

	if (!mWater)
	{
		mWater = LLEnvSettingsWater::buildDefaultWater();
	}

	if (!mSky)
	{
		mSky = LLEnvSettingsSky::buildDefaultSky();
	}
}

void LLEnvironment::DayInstance::clear()
{
	mType = TYPE_INVALID;
	mDayCycle.reset();
	mSky.reset();
	mWater.reset();
	mDayLength = LLSettingsDay::DEFAULT_DAYLENGTH;
	mDayOffset = LLSettingsDay::DEFAULT_DAYOFFSET;
	mBlenderSky.reset();
	mBlenderWater.reset();
	mSkyTrack = 1;
}

void LLEnvironment::DayInstance::setSkyTrack(S32 trackno)
{
	mSkyTrack = trackno;
	if (mBlenderSky)
	{
		mBlenderSky->switchTrack(trackno, 0.0);
	}
}

void LLEnvironment::DayInstance::setBlenders(const LLSettingsBlender::ptr_t& skyblend,
											 const LLSettingsBlender::ptr_t& waterblend)
{
	mBlenderSky = skyblend;
	mBlenderWater = waterblend;
}

F32 LLEnvironment::DayInstance::getProgress() const
{
	if (mDayLength <= 0 || !mDayCycle)
	{
		return -1.f;	// No actual day cycle.
	}
	F64 now = LLTimer::getEpochSeconds() + (F64)mDayOffset;
	return convert_time_to_position(now, mDayLength);
}

F32 LLEnvironment::DayInstance::secondsToKeyframe(S32 seconds)
{
	return convert_time_to_position(seconds, mDayLength);
}

void LLEnvironment::DayInstance::animate()
{
	if (!mDayCycle)
	{
		return;
	}

	if (!(mAnimateFlags & NO_ANIMATE_WATER))
	{
		LLSettingsDay::CycleTrack_t& wtrack = mDayCycle->getCycleTrack(0);
		if (wtrack.empty())
		{
			mWater.reset();
			mBlenderWater.reset();
		}
		else
		{
			mWater = LLEnvSettingsWater::buildDefaultWater();
			mBlenderWater =
				std::make_shared<LLTrackBlenderLoopingTime>(mWater, mDayCycle,
															0, mDayLength,
															mDayOffset,
															DEFAULT_UPDATE_THRESHOLD);
		}
	}

	if (!(mAnimateFlags & NO_ANIMATE_SKY))
	{
		// Initialize sky to track 1
		LLSettingsDay::CycleTrack_t &track = mDayCycle->getCycleTrack(1);
		if (track.empty())
		{
			mSky.reset();
			mBlenderSky.reset();
		}
		else
		{
			mSky = LLEnvSettingsSky::buildDefaultSky();
			mBlenderSky =
				std::make_shared<LLTrackBlenderLoopingTime>(mSky, mDayCycle,
															1, mDayLength,
															mDayOffset,
															DEFAULT_UPDATE_THRESHOLD);
			mBlenderSky->switchTrack(mSkyTrack, 0.f);
		}
	}
}

LLEnvironment::DayTransition::DayTransition(const LLSettingsSky::ptr_t& skystart,
											const LLSettingsWater::ptr_t& waterstart,
											LLEnvironment::DayInstance::ptr_t& end,
											S32 time)
:	DayInstance(ENV_NONE),
	mStartSky(skystart),
	mStartWater(waterstart),
	mNextInstance(end),
	mTransitionTime(time)
{
}

bool LLEnvironment::DayTransition::applyTimeDelta(F64 delta)
{
	bool changed = mNextInstance->applyTimeDelta(delta);
	changed |= DayInstance::applyTimeDelta(delta);
	return changed;
}

void LLEnvironment::DayTransition::animate()
{
	mNextInstance->animate();

	mWater = mStartWater->buildClone();
	mBlenderWater =
		std::make_shared<LLSettingsBlenderTimeDelta>(mWater, mStartWater,
													 mNextInstance->getWater(),
													 mTransitionTime);
	mBlenderWater->setOnFinished([this](LLSettingsBlender::ptr_t blender)
								 {
									mBlenderWater.reset();
									if (!mBlenderSky && !mBlenderWater)
									{
										gEnvironment.mCurrentEnvironment =
											mNextInstance;
									}
									else
									{
										setWater(mNextInstance->getWater());
									}
								 });

	mSky = mStartSky->buildClone();
	mBlenderSky =
		std::make_shared<LLSettingsBlenderTimeDelta>(mSky, mStartSky,
													 mNextInstance->getSky(),
													 mTransitionTime);
	mBlenderSky->setOnFinished([this](LLSettingsBlender::ptr_t blender)
							   {
									mBlenderSky.reset();
									if (!mBlenderSky && !mBlenderWater)
									{
										gEnvironment.mCurrentEnvironment =
											mNextInstance;
									}
									else
									{
										setSky(mNextInstance->getSky());
									}
							   });
}

LLTrackBlenderLoopingManual::LLTrackBlenderLoopingManual(const LLSettingsBase::ptr_t& target,
														 const LLSettingsDay::ptr_t& day,
														 S32 trackno)
:	LLSettingsBlender(target, LLSettingsBase::ptr_t(),
					  LLSettingsBase::ptr_t()),
	mDay(day),
	mTrackNo(trackno),
	mPosition(0.0)
{
	LLSettingsDay::TrackBound_t initial = getBoundingEntries(mPosition);
	if (initial.first != mEndMarker)
	{
		// No frames in track
		mInitial = (initial.first)->second;
		mFinal = (initial.second)->second;

		LLSD init = mInitial->getSettings();
		mTarget->replaceSettings(init);
	}
}

F64 LLTrackBlenderLoopingManual::setPosition(F32 position)
{
	mPosition = llclamp(position, 0.f, 1.f);

	LLSettingsDay::TrackBound_t bounds = getBoundingEntries(mPosition);

	if (bounds.first == mEndMarker)
	{
		// No frames in track.
		return 0.0;
	}

	mInitial = (bounds.first)->second;
	mFinal = (bounds.second)->second;

	F64 span_length = getSpanLength(bounds);

	F64 span_pos = mPosition;
	if ((bounds.first)->first)
	{
		mPosition += 1.0 - (bounds.first)->first;
	}
	if (span_pos > span_length)
	{
		// We are clamping position to 0-1 and span_length is 1, so do not
		// account for case of span_pos == span_length
		span_pos = fmod(span_pos, span_length);
	}

	return LLSettingsBlender::setBlendFactor(span_pos / span_length);
}

void LLTrackBlenderLoopingManual::switchTrack(S32 trackno, F32 position)
{
	mTrackNo = trackno;
	setPosition(position < 0.f ? mPosition : position);
}

LLSettingsDay::TrackBound_t LLTrackBlenderLoopingManual::getBoundingEntries(F64 position)
{
	LLSettingsDay::CycleTrack_t& wtrack = mDay->getCycleTrack(mTrackNo);
	mEndMarker = wtrack.end();
	return get_bounding_entries(wtrack, position);
}

F64 LLTrackBlenderLoopingManual::getSpanLength(const LLSettingsDay::TrackBound_t& bounds) const
{
	return get_wrapping_distance((bounds.first)->first,
								 (bounds.second)->first);
}
