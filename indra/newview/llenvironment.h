/**
 * @file llenvironment.h
 * @brief Extended environment manager class declaration.
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

#ifndef LL_ENVIRONMENT_H
#define LL_ENVIRONMENT_H

#include <array>

#include "llglslshader.h"
#include "llsd.h"
#include "llsettingsdaycycle.h"
#include "llsettingssky.h"
#include "llsettingswater.h"
#include "llsdutil.h"

class LLAtmosModelSettings;
class LLParcel;
class LLViewerCamera;

// Defined to 0 since we do not use EE fixed sky settings (which would not
// allow to set the time in the local environment editor, since they do not
// include a day cycle at all), but instead the genuine Windlight Default day
// cycle.
#define LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME 0

class LLEnvironment
{
protected:
	LOG_CLASS(LLEnvironment);

public:
	LLEnvironment();

	// These methods are called in llappviewer.cpp only
	void initClass();
	void cleanupClass();

	class EnvironmentInfo
	{
	public:
		EnvironmentInfo();

		typedef std::shared_ptr<EnvironmentInfo> ptr_t;
		typedef std::array<std::string, 5> namelist_t;

		static ptr_t extract(LLSD);
		static ptr_t extractLegacy(LLSD);

	public:
		LLUUID					mRegionId;
		LLUUID					mAssetId;
		S32						mEnvVersion;
		S32						mParcelId;
		S64						mDayLength;
		S64						mDayOffset;
		size_t					mDayHash;
		LLSettingsDay::ptr_t	mDayCycle;
		std::array<F32, 4>		mAltitudes;
		std::string				mDayCycleName;
		namelist_t				mNameList;
		bool					mIsDefault;
		bool					mIsLegacy;
	};
	typedef EnvironmentInfo::ptr_t envinfo_ptr_t;

	enum EEnvSelection
	{
		ENV_EDIT = 0,
		ENV_LOCAL,
		ENV_PUSH,
		ENV_PARCEL,
		ENV_REGION,
		ENV_DEFAULT,
		ENV_END,
		ENV_CURRENT = -1,
		ENV_NONE = -2
	};

	// Not constexpr because then gcc 5.5 fails to see them at compile time
	// in std::make_shared constructs...
	enum EETransition
	{
		TRANSITION_INSTANT	= 0,
		TRANSITION_FAST		= 1,
		TRANSITION_DEFAULT	= 5,
		TRANSITION_SLOW		= 10,
		TRANSITION_ALTITUDE	= 5
	};

	typedef boost::signals2::connection connection_t;
	typedef boost::signals2::signal<void(EEnvSelection, S32)> env_changed_signal_t;
	typedef env_changed_signal_t::slot_type env_changed_fn;
	typedef std::pair<LLSettingsSky::ptr_t, LLSettingsWater::ptr_t> fixed_env_t;
	typedef boost::function<void(S32, envinfo_ptr_t)> env_apply_fn;
	typedef std::array<F32, 4> altitude_list_t;
	typedef std::vector<F32> altitudes_vect_t;

	inline bool canEdit() const					{ return true; }

	static bool isExtendedEnvironmentEnabled();
	static bool isInventoryEnabled();
	static bool canAgentUpdateParcelEnvironment(LLParcel* parcel = NULL);
	static bool canAgentUpdateRegionEnvironment();

	static void setSunrise();
	static void setMidday();
	static void setSunset();
	static void setMidnight();
	static void setRegion();

	void setFixedTimeOfDay(F32 position);

	void setLocalEnvFromDefaultWindlightDay(F32 position = -1.f);

	inline LLSettingsDay::ptr_t getCurrentDay() const
	{
		return mCurrentEnvironment->getDayCycle();
	}

	const LLSettingsSky::ptr_t& getCurrentSky() const;
	const LLSettingsWater::ptr_t& getCurrentWater() const;

	void update(const LLViewerCamera* cam);

	void setSelectedEnvironment(EEnvSelection env,
								F64 transition = (F64)TRANSITION_DEFAULT,
								bool forced = false);

	inline EEnvSelection getSelectedEnvironment() const
	{
		return mSelectedEnvironment;
	}

	// Apply current settings to given shader
	void updateShaderUniforms(LLGLSLShader* shader);
	// Apply current sky settings to given shader
	void updateShaderSkyUniforms(LLGLSLShader* shader);

	// Prepare settings to be applied to shaders (call whenever settings are
	// updated)
	void updateSettingsUniforms();

	static void updateGLVariablesForSettings(LLShaderUniforms* uniforms,
											 const LLSettingsBase::ptr_t& setting);

	bool hasEnvironment(EEnvSelection env);

	void setCurrentEnvironmentSelection(EEnvSelection env);

	void setEnvironment(EEnvSelection env, const LLSettingsDay::ptr_t& dayp,
						S32 daylength, S32 dayoffset,
						S32 env_version = NO_VERSION);

	void setEnvironment(EEnvSelection env, fixed_env_t fixed,
						S32 env_version = NO_VERSION);

	void setEnvironment(EEnvSelection env, const LLSettingsBase::ptr_t& fixed,
						S32 env_version = NO_VERSION);

	inline void setEnvironment(EEnvSelection env,
								  const LLSettingsSky::ptr_t& skyp,
								  S32 env_version = NO_VERSION)
	{
		setEnvironment(env, fixed_env_t(skyp, LLSettingsWater::ptr_t()),
					   env_version);
	}

	inline void setEnvironment(EEnvSelection env,
								  const LLSettingsWater::ptr_t& waterp,
								  S32 env_version = NO_VERSION)
	{
		setEnvironment(env, fixed_env_t(LLSettingsSky::ptr_t(), waterp),
					   env_version);
	}

	inline void setEnvironment(EEnvSelection env,
								  const LLSettingsSky::ptr_t& fixeds,
								  const LLSettingsWater::ptr_t& fixedw,
								  S32 env_version = NO_VERSION)
	{
		setEnvironment(env, fixed_env_t(fixeds, fixedw), env_version);
	}

	void setEnvironment(EEnvSelection env, const LLUUID& asset_id,
						S32 transition = TRANSITION_DEFAULT);

	void setSharedEnvironment();

	void clearEnvironment(EEnvSelection env);

	LLSettingsDay::ptr_t getEnvironmentDay(EEnvSelection env);
	S32 getEnvironmentDayLength(EEnvSelection env);
	S32 getEnvironmentDayOffset(EEnvSelection env);

	fixed_env_t getEnvironmentFixed(EEnvSelection env, bool resolve = false);

	inline LLSettingsSky::ptr_t getEnvironmentFixedSky(EEnvSelection env,
														  bool resolve = false)
	{
		return getEnvironmentFixed(env, resolve).first;
	}

	inline LLSettingsWater::ptr_t getEnvironmentFixedWater(EEnvSelection env,
															  bool resolve = false)
	{
		return getEnvironmentFixed(env, resolve).second;
	}

	void updateEnvironment(F64 transition = (F64)TRANSITION_DEFAULT,
						   bool forced = false);

	inline LLVector2 getCloudScrollDelta() const	{ return mCloudScrollDelta; }

	inline void pauseCloudScroll()				{ mCloudScrollPaused = true; }
	inline void resumeCloudScroll()				{ mCloudScrollPaused = false; }
	inline bool isCloudScrollPaused() const		{ return mCloudScrollPaused; }

	F32 getCamHeight() const;
	F32 getWaterHeight() const;
	bool getIsSunUp() const;
	bool getIsMoonUp() const;

	inline static void addBeaconsUser()			{ ++sBeaconsUsers; }
	static void delBeaconsUser();

	// Returns either sun or moon direction (depending on which is up and
	// stronger). Light direction in +x right, +z up, +y at internal coord sys.
	LLVector3 getLightDirection() const;
	LLVector3 getSunDirection() const;
	LLVector3 getMoonDirection() const;

	// These methods return light direction converted to CFR coord system

	inline LLVector4 getLightDirectionCFR() const
	{
		return toCFR(getLightDirection());
	}

	inline LLVector4 getSunDirectionCFR() const
	{
		return toCFR(getSunDirection());
	}

	inline LLVector4 getMoonDirectionCFR() const
	{
		return toCFR(getMoonDirection());
	}

	// Returns light direction converted to OGL coord system and clamped above
	// -0.1f in Y to avoid render artifacts in sky shaders.
	LLVector4 getClampedLightNorm() const;
	LLVector4 getClampedSunNorm() const;
	LLVector4 getClampedMoonNorm() const;

	// Returns light direction converted to OGL coord system and rotated by
	// last cam yaw needed by water rendering shaders
	LLVector4  getRotatedLightNorm() const;

	static LLSettingsWater::ptr_t createWaterFromLegacyPreset(const std::string& filename,
															  LLSD& messages);
	static LLSettingsSky::ptr_t createSkyFromLegacyPreset(const std::string& filename,
														  LLSD& messages);
	static LLSettingsDay::ptr_t createDayCycleFromLegacyPreset(const std::string& filename,
															   LLSD& messages);

	// Constructs a new day cycle based on the environment, replacing either
	// the water or the sky tracks.
	LLSettingsDay::ptr_t createDayCycleFromEnvironment(EEnvSelection env,
													   LLSettingsBase::ptr_t settings);

	inline F32 getProgress() const
	{
		return mCurrentEnvironment ? mCurrentEnvironment->getProgress() : -1.f;
	}

	inline F32 getRegionProgress() const
	{
		return mEnvironments[ENV_REGION] ?
				mEnvironments[ENV_REGION]->getProgress() : -1.f;
	}

	// Only used on legacy regions, to better sync the viewer with other
	// agents
	void adjustRegionOffset(F32 adjust);

	inline connection_t setEnvironmentChanged(env_changed_fn cb)
	{
		return mSignalEnvChanged.connect(cb);
	}

	void updateRegion(const LLUUID& asset_id, const std::string& display_name,
					  S32 track_num, S32 day_length, S32 day_offset, U32 flags,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateRegion(const LLSettingsDay::ptr_t& dayp, S32 day_length,
					  S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateRegion(const LLSettingsSky::ptr_t& skyp, S32 day_length,
					  S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateRegion(const LLSettingsWater::ptr_t& waterp, S32 day_length,
					  S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void resetRegion(env_apply_fn cb = env_apply_fn());
	void requestParcel(S32 parcel_id, env_apply_fn cb = env_apply_fn());
	void updateParcel(S32 parcel_id, const LLUUID& asset_id,
					  const std::string& display_name, S32 track_num,
					  S32 day_length, S32 day_offset, U32 flags,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateParcel(S32 parcel_id, const LLSettingsDay::ptr_t& dayp,
					  S32 track_num, S32 day_length, S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateParcel(S32 parcel_id, const LLSettingsDay::ptr_t& dayp,
					  S32 day_length, S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateParcel(S32 parcel_id, const LLSettingsSky::ptr_t& skyp,
					  S32 day_length, S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void updateParcel(S32 parcel_id, const LLSettingsWater::ptr_t& waterp,
					  S32 day_length, S32 day_offset,
					  altitudes_vect_t altitudes = altitudes_vect_t(),
					  env_apply_fn cb = env_apply_fn());
	void resetParcel(S32 parcel_id, env_apply_fn cb = env_apply_fn());

	void selectAgentEnvironment();

	static void requestRegion(env_apply_fn cb = env_apply_fn());

	S32 calculateSkyTrackForAltitude(F64 altitude);

	inline const altitude_list_t& getRegionAltitudes() const
	{
		return mTrackAltitudes;
	}

	void handleEnvironmentPush(LLSD& message);

	class DayInstance : public std::enable_shared_from_this<DayInstance>
	{
	public:
		DayInstance(EEnvSelection env);
		virtual ~DayInstance()						{}

		enum EInstanceType
		{
			TYPE_INVALID,
			TYPE_FIXED,
			TYPE_CYCLED
		};

		typedef std::shared_ptr<DayInstance> ptr_t;

		virtual ptr_t clone() const;

		virtual bool applyTimeDelta(F64 delta);

		virtual void setDay(const LLSettingsDay::ptr_t& dayp, S32 daylength,
							S32 dayoffset);
		virtual void setSky(const LLSettingsSky::ptr_t& skyp);
		virtual void setWater(const LLSettingsWater::ptr_t& waterp);

		void initialize();
		bool isInitialized();

		void clear();

		void setSkyTrack(S32 trackno);

		const inline LLSettingsDay::ptr_t& getDayCycle() const
		{
			return mDayCycle;
		}

		const inline LLSettingsSky::ptr_t& getSky() const
		{
			return mSky;
		}

		const inline LLSettingsWater::ptr_t& getWater() const
		{
			return mWater;
		}

		inline S32 getDayLength() const			{ return mDayLength; }
		inline S32 getDayOffset() const			{ return mDayOffset; }
		inline S32 getSkyTrack() const			{ return mSkyTrack; }

		inline void setDayOffset(F64 offset)
		{
			mDayOffset = offset;
			animate();
		}

		virtual void animate();

		void setBlenders(const LLSettingsBlender::ptr_t& skyblend,
						 const LLSettingsBlender::ptr_t& waterblend);

		inline EEnvSelection getEnvironmentSelection() const
		{
			return mEnv;
		}

		inline void setEnvironmentSelection(EEnvSelection env)
		{
			mEnv = env;
		}

		F32 getProgress() const;

		inline void setFlags(U32 flag)			{ mAnimateFlags |= flag; }
		inline void clearFlags(U32 flag)			{ mAnimateFlags &= ~flag; }

	protected:
		F32 secondsToKeyframe(S32 seconds);

	public:
		static constexpr U32		NO_ANIMATE_SKY = 1;
		static constexpr U32		NO_ANIMATE_WATER = 2;

	protected:
		S32							mSkyTrack;
		S32							mDayLength;
		S32							mDayOffset;
		S32							mLastTrackAltitude;
		U32							mAnimateFlags;

		LLSettingsDay::ptr_t		mDayCycle;
		LLSettingsSky::ptr_t		mSky;
		LLSettingsWater::ptr_t		mWater;
		LLSettingsBlender::ptr_t	mBlenderSky;
		LLSettingsBlender::ptr_t	mBlenderWater;

		EEnvSelection				mEnv;
		EInstanceType				mType;

		bool						mInitialized;
	};

	class DayTransition : public DayInstance
	{
	public:
		DayTransition(const LLSettingsSky::ptr_t& skystart,
					  const LLSettingsWater::ptr_t& waterstart,
					  DayInstance::ptr_t& end, S32 time);

		~DayTransition() override					{}

		bool applyTimeDelta(F64 delta) override;
		void animate() override;

	protected:
		LLSettingsSky::ptr_t	mStartSky;
		LLSettingsWater::ptr_t	mStartWater;
		DayInstance::ptr_t		mNextInstance;
		S32						mTransitionTime;
	};

	DayInstance::ptr_t getSelectedEnvironmentInstance();
	DayInstance::ptr_t getSharedEnvironmentInstance();

private:
	inline static LLVector4 toCFR(const LLVector3& vec)
	{
		return LLVector4(vec.mV[1], vec.mV[0], vec.mV[2], 0.f);
	}

	inline static LLVector4 toLightNorm(const LLVector3& vec)
	{
		return LLVector4(vec.mV[1], vec.mV[2], vec.mV[0], 0.f);
	}

	typedef std::array<DayInstance::ptr_t, ENV_END> InstanceArray_t;

	DayInstance::ptr_t getEnvironmentInstance(EEnvSelection env,
											  bool create = false);

	void updateCloudScroll();
	void onRegionChange();
	void onParcelChange();

	struct UpdateInfo
	{
		typedef std::shared_ptr<UpdateInfo> ptr_t;

		UpdateInfo(LLSettingsDay::ptr_t pday, S32 day_length, S32 day_offset,
				   altitudes_vect_t altitudes)
		:	mDayp(pday),
			mSettingsAsset(),
			mDayLength(day_length),
			mDayOffset(day_offset),
			mAltitudes(altitudes),
			mFlags(0)
		{
			if (mDayp)
			{
				mDayName = mDayp->getName();
			}
		}

		UpdateInfo(const LLUUID& settings_asset, const std::string& name,
				   S32 day_length, S32 day_offset, altitudes_vect_t altitudes,
				   U32 flags)
		:	mSettingsAsset(settings_asset),
			mDayLength(day_length),
			mDayOffset(day_offset),
			mAltitudes(altitudes),
			mDayName(name),
			mFlags(flags)
		{
		}

		LLUUID					mSettingsAsset;
		S32						mDayLength;
		S32						mDayOffset;
		U32						mFlags;
		LLSettingsDay::ptr_t	mDayp;
		altitudes_vect_t		mAltitudes;
		std::string				mDayName;
	};

	void coroRequestEnvironment(S32 parcel_id, env_apply_fn apply);
	void coroUpdateEnvironment(S32 parcel_id, S32 track_no,
							   UpdateInfo::ptr_t updates, env_apply_fn apply);
	void coroResetEnvironment(S32 parcel_id, S32 track_no, env_apply_fn apply);

	void recordEnvironment(S32 parcel_id, envinfo_ptr_t environment,
						   F64 transition);

	void onAgentPositionHasChanged(const LLVector3& localpos);

	void onSetEnvAssetLoaded(EEnvSelection env, S32 transition,
							 const LLUUID& asset_id,
							 LLSettingsBase::ptr_t settings, S32 status);
	void onUpdateParcelAssetLoaded(const LLUUID& asset_id,
								   LLSettingsBase::ptr_t settings, S32 status,
								   S32 parcel_id, S32 day_length,
								   S32 day_offset, altitudes_vect_t altitudes);

	void handleEnvironmentPushClear(const LLUUID& experience_id, LLSD& message,
									F32 transition);
	void handleEnvironmentPushFull(const LLUUID& experience_id, LLSD& message,
								   F32 transition);
	void handleEnvironmentPushPartial(const LLUUID& experience_id,
									  LLSD& message, F32 transition);

	void clearExperienceEnvironment(const LLUUID& experience_id,
									F64 transition_time);
	void setExperienceEnvironment(const LLUUID& experience_id,
								  const LLUUID& asset_id, F32 transition_time);
	void setExperienceEnvironment(const LLUUID& experience_id,
								  LLSD environment, F32 transition_time);
	void onSetExperienceEnvAssetLoaded(const LLUUID& experience_id,
									   LLSettingsBase::ptr_t setting,
									   F32 transition_time, S32 status);

	bool listenExperiencePump(const LLSD& message);

public:
#if LL_USE_FIXED_SKY_SETTINGS_FOR_DEFAULT_TIME
	static const LLUUID			KNOWN_SKY_SUNRISE;
	static const LLUUID			KNOWN_SKY_MIDDAY;
	static const LLUUID			KNOWN_SKY_SUNSET;
	static const LLUUID			KNOWN_SKY_MIDNIGHT;
#endif

	static constexpr S32		NO_TRACK = -1;
	// For viewer sided change, like ENV_LOCAL. -3 since -1 and -2 are taken by
	// parcel initial server/viewer version
	static constexpr S32		NO_VERSION = -3;
	// For cleanups
	static constexpr S32		VERSION_CLEANUP = -4;

	static constexpr F32		SUN_DELTA_YAW = F_PI;

private:
	F32							mLastCamYaw;
	S32							mCurrentTrack;

	altitude_list_t				mTrackAltitudes;

	InstanceArray_t				mEnvironments;

	EEnvSelection				mSelectedEnvironment;
	DayInstance::ptr_t			mCurrentEnvironment;

	LLSettingsSky::ptr_t		mSelectedSky;
	LLSettingsWater::ptr_t		mSelectedWater;
	LLSettingsDay::ptr_t		mSelectedDay;

	env_changed_signal_t		mSignalEnvChanged;

	connection_t				mRegionChangedConnection;
	connection_t				mParcelChangedConnection;
	connection_t				mPositionChangedConnection;

	LLSD						mSkyOverrides;
	LLSD						mWaterOverrides;

	// Cached uniform values from LLSD values
	LLShaderUniforms mWaterUniforms[LLGLSLShader::SG_COUNT];
	LLShaderUniforms mSkyUniforms[LLGLSLShader::SG_COUNT];

	// Cumulative cloud delta
	LLVector2					mCloudScrollDelta;

	bool						mCloudScrollPaused;

	bool						mInstanceValid;

	static S32					sBeaconsUsers;
};

class LLTrackBlenderLoopingManual : public LLSettingsBlender
{
public:
	typedef std::shared_ptr<LLTrackBlenderLoopingManual> ptr_t;

	LLTrackBlenderLoopingManual(const LLSettingsBase::ptr_t& target,
								const LLSettingsDay::ptr_t& day, S32 trackno);

	F64 setPosition(F32 position);

    void switchTrack(S32 trackno, const LLSettingsBase::TrackPosition& position) override;

	inline S32 getTrack() const					{ return mTrackNo; }

protected:
	LLSettingsDay::TrackBound_t getBoundingEntries(F64 position);
	F64 getSpanLength(const LLSettingsDay::TrackBound_t& bounds) const;

private:
	LLSettingsDay::ptr_t			mDay;
	LLSettingsDay::cycle_track_it_t	mEndMarker;
	F64								mPosition;
	S32								mTrackNo;
};

extern LLEnvironment gEnvironment;

#endif	// LL_ENVIRONMENT_H
