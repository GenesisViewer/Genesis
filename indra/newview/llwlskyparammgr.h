/**
 * @file llwlskyparammgr.h
 * @brief Implementation for the LLWLSkyParamMgr class.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_WLSKYPARAMMGR_H
#define LL_WLSKYPARAMMGR_H

#include <map>
#include "v4color.h"

#include "llstaticstringtable.h"
#include "v4math.h"

class LLMessageSystem;

// A class representing a set of parameter values for the WindLight sky
class LLWLParamSet
{
	friend class LLWLSkyParamMgr;

public:
	LLWLParamSet();

	// Sets the total LLSD
	void setAll(const LLSD& val);

	// Gets the total LLSD
	inline const LLSD& getAll() const			{ return mParamValues; }

	// Sets a float parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param x			The float value to set.
	void set(const std::string& param_name, F32 x);

	// Sets a float2 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param x			The x component's value to set.
	//  - param y			The y component's value to set.
	void set(const std::string& param_name, F32 x, F32 y);

	// Sets a float3 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param x			The x component's value to set.
	//  - param y			The y component's value to set.
	//  - param z			The z component's value to set.
	void set(const std::string& param_name, F32 x, F32 y, F32 z);

	// Sets a float4 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param x			The x component's value to set.
	//  - param y			The y component's value to set.
	//  - param z			The z component's value to set.
	//  - param w			The w component's value to set.
	void set(const std::string& param_name, F32 x, F32 y, F32 z, F32 w);

	// Sets a float4 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param val			An array of the 4 float values to set the parameter
	//						to.
	void set(const std::string& param_name, const F32* val);

	// Sets a float4 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param val			A struct of the 4 float values to set the parameter
	//						to.
	void set(const std::string& param_name, const LLVector4& val);

	// Sets a float4 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param val			A struct of the 4 float values to set the parameter
	//						to.
	void set(const std::string& param_name, const LLColor4& val);

	// Gets a float4 parameter.
	//  - param param_name	The name of the parameter to set.
	//  - param error		A flag to set if it's not the proper return type
	LLVector4 getVector(const std::string& param_name, bool& error);

	// Gets an integer parameter
	//  - param param_name	The name of the parameter to set.
	//  - param error		A flag to set if it's not the proper return type
	F32 getFloat(const std::string& param_name, bool& error);

	// Sets the star's brightness
	void setStarBrightness(F32 val);

	// Gets the star brightness value
	inline F32 getStarBrightness()				{ return mStartBrightness; }

	void setSunAngle(F32 val);

	inline F32 getSunAngle()						{ return mSunAngle; }

	void setEastAngle(F32 val);

	inline F32 getEastAngle()					{ return mEastAngle; }

	// Sets the cloud scroll x enable value
	void setEnableCloudScrollX(bool val);

	// Gets the scroll x enable value;
	inline bool getEnableCloudScrollX()			{ return mCloudScrollEnableX; }

	// Sets the cloud scroll y enable value
	void setEnableCloudScrollY(bool val);

	// Gets the scroll enable y value
	inline bool getEnableCloudScrollY()			{ return mCloudScrollEnableY; }

	// Sets the scroll x value
	void setCloudScrollX(F32 val);

	// Gets the scroll x enable value
	inline F32 getCloudScrollX()					{ return mCloudScrollRateX; }

	// Sets the scroll y enable value
	void setCloudScrollY(F32 val);

	// Gets the scroll enable y value
	inline F32 getCloudScrollY()					{ return mCloudScrollRateY; }

	// Interpolate two parameter sets
	//  - param src			The parameter set to start with
	//  - param dest		The parameter set to end with
	//  - param weight		The amount to interpolate
	void mix(LLWLParamSet& src, LLWLParamSet& dest, F32 weight);

	void updateCloudScrolling();

private:
	void updateHashedNames();

public:
	std::string		mName;

private:
	LLSD			mParamValues;
	typedef std::vector<LLStaticHashedString> hash_vector_t;
	hash_vector_t	mParamHashedNames;
	F32				mSunAngle;
	F32				mEastAngle;
	F32				mStartBrightness;
	F32				mCloudScrollXOffset;
	F32				mCloudScrollYOffset;
	F32				mCloudScrollRateX;
	F32				mCloudScrollRateY;
	bool			mCloudScrollEnableX;
	bool			mCloudScrollEnableY;
};

// Color control
struct WLColorControl
{
	F32 r, g, b, i;				// The values
	std::string mName;			// Name to use to dereference params
	std::string mSliderName;	// Name of the slider in menu
	bool hasSliderName;			// Only set slider name for true color types
	// Flag for if it is the Sun or ambient color controller
	bool isSunOrAmbientColor;
	// Flag for if it is the Blue Horizon or Density color controller
	bool isBlueHorizonOrDensity;

	inline WLColorControl(F32 red, F32 green, F32 blue, F32 intensity,
							 const std::string& name,
							 const std::string& sld_name = LLStringUtil::null)
	:	r(red),
		g(green),
		b(blue),
		i(intensity),
		mName(name),
		mSliderName(sld_name)
	{
		hasSliderName = !sld_name.empty();

		isSunOrAmbientColor = sld_name == "WLSunlight" ||
							  sld_name == "WLAmbient";

		isBlueHorizonOrDensity = sld_name == "WLBlueHorizon" ||
								 sld_name == "WLBlueDensity";
	}

	inline WLColorControl& operator=(const LLVector4& val)
	{
		r = val.mV[0];
		g = val.mV[1];
		b = val.mV[2];
		i = val.mV[3];
		return *this;
	}

	inline operator LLVector4() const
	{
		return LLVector4(r, g, b, i);
	}

	inline operator LLVector3() const
	{
		return LLVector3(r, g, b);
	}

	inline void update(LLWLParamSet& params) const
	{
		params.set(mName, r, g, b, i);
	}
};

// Float slider control
struct WLFloatControl
{
	F32 x;
	std::string mName;
	F32 mult;

	inline WLFloatControl(F32 val, const std::string& n, F32 m = 1.f)
	:	x(val),
		mName(n),
		mult(m)
	{
	}

	inline WLFloatControl & operator=(const LLVector4& val)
	{
		x = val.mV[0];
		return *this;
	}

	inline operator F32 () const
	{
		return x;
	}

	inline void update(LLWLParamSet & params) const
	{
		params.set(mName, x);
	}
};

// LLWLAnimator class
class LLWLAnimator
{
public:
	LLWLAnimator();

	// Update the parameters
	void update(LLWLParamSet& curParams);

	// Returns time of day which is either the region time (based on sSunPhase)
	// or the Windlight time (based on mDayOffset and mDayLenth), as a [0.0-1.0]
	// double float.
	F64 getDayTime();

	// Sets a long float 0.0 - 1.0 saying what time of day it is
	void setDayTime(F64 day_time);

	// Sets day rate and offset
	void setDayRateAndOffset(S32 day_length, S32 day_offset);

	// Sets an animation track
	inline void setTrack(const std::map<F32, std::string>& track,
							S32 day_length, F64 day_time = 0.0,
							bool run = true)
	{
		mTimeTrack = track;
		mDayLenth = day_length;
		setDayTime(day_time);
		mIsRunning = run;
	}

	// Returns the estate/region time (AKA "Linden time", based on sSunPhase),
	// as a [0.0-1.0] double float.
	static F64 getEstateTime();

public:
	F64				mDayTime;
	S32				mDayOffset;
	S32				mDayLenth;

	// Track to play
	typedef std::map<F32, std::string> time_track_t;
	time_track_t	mTimeTrack;

	typedef std::map<F32, std::string>::iterator time_track_it_t;
	time_track_it_t	mFirstIt;
	time_track_it_t	mSecondIt;

	bool			mIsRunning;

	static F32		sSunPhase;
};

// LLWLDayCycle class
class LLWLDayCycle
{
protected:
	LOG_CLASS(LLWLDayCycle);

public:
	LLWLDayCycle();

	// Finds available day cycle presets by name
	static void findPresets();

	// Removes a day cycle preset (by name or file name).
	static void removeDayCycle(const std::string& name);

	// Loads a day cycle (by name or file name). Returns true on success.
	// When 'alert' is true, pop ups alerts when a sky track is missing.
	bool loadDayCycle(const std::string& name, bool alert = true);

	// Saves a day cycle (by name or file name).
	void saveDayCycle(const std::string& name);

	// Clears keys
	void clearKeys();

	// Adds a new key frame to the day cycle. Returns true if successful. No
	// negative time allowed.
	bool addKey(F32 new_time, const std::string& param_name);

	// Adjusts a key placement in the day cycle. Returns true if successful.
	bool changeKeyTime(F32 old_time, F32 new_time);

	// Adjusts a key parameter used. Returns true if successful.
	bool changeKeyParam(F32 time, const std::string& param_name);

	// Remove a key from the day cycle. Returns true if successful.
	bool removeKey(F32 time);

	// Gets the first key time for a parameter. Returns false if not there.
	bool getKey(const std::string& name, F32& key);

	// Gets the param set at a given time. Returns true if it found one.
	bool getKeyedParam(F32 time, LLWLParamSet& param);

	// Gets the name. Returns true if it found one.
	bool getKeyedParamName(F32 time, std::string& name);

	// These methods return (respectively) the viewer installation directory
	// and the user settings path for Windlight presets files. 'subdir' may
	// either be an empty string (in which case the base Windlight presets
	// directory is returned), or contain "days", "skies" or "water".
	static std::string getSysDir(const std::string& subdir);
	static std::string getUserDir(const std::string& subdir);

	// URI-escapes 'name', with or without dash ('-') characters escaping, and
	// adds ".xml" to the name if not already there.
	static std::string makeFileName(const std::string& name,
									bool escape_dash = true);

	// Used to find a matching preset file for 'name', either in the base_path,
	// the user settings, or the viewer installation directory, in this order
	// of priority. 'name' is turned into an XML file name with URI escaping
	// and possibly with dash escaping (an attempt is first made to find a file
	// with dash escaping, then another attempt is made without it).
	// 'subdir' shall contain "days", "skies" or "water", to specify which
	// setting is sought for. 'base_path' may be empty (in which case the
	// search is done only in the user settings and viewer installation
	// directories). Returns true on success to find the preset file, with
	// 'filename' and 'path' set accordingly to the findings. On failure, false
	// is returned, with a properly escaped file name in 'filename', and 'path'
	// set to 'base_path'. 'path' contains a trailing directory separator so
	// that the full path for the file is path + filename.
	static bool findPresetFile(const std::string& name,
							   const std::string& subdir,
							   const std::string& base_path,
							   std::string& filename, std::string& path);

	static std::vector<std::string> getLoadedPresetsList();

public:
	// Lists what param sets are used and when during the day
	typedef std::map<F32, std::string> time_map_t;
	time_map_t			mTimeMap;

	// How long is my day, in seconds
	S32					mDayLenth;

	// Lists the available day cycle presets in both the user and system
	// directories.
	typedef std::set<std::string> names_list_t;
	static names_list_t	sPresetNames;
};

// Parameter manager class for the Windlight sky
class LLWLSkyParamMgr
{
	friend class LLWLAnimator;

protected:
	LOG_CLASS(LLWLSkyParamMgr);

public:
	LLWLSkyParamMgr();

	// This method is called in llappviewer.cpp only
	void initClass();

	// Loads all preset files
	void loadPresets();

	// Loads an individual preset into the sky. Returns true if successful.
	bool loadPreset(const std::string& name, bool propogate = true);

	// Saves the parameter presets to file
	void savePreset(const std::string& name);

	// Propagates the parameters to the environment
	void propagateParameters();

	// Animate the Windlight sky or not.
	void animate(bool enable = true);

	// Setups the animator to run
	void resetAnimator(F32 cur_time, bool run);

	// Gets where the light is pointing
	inline LLVector4 getLightDir() const			{ return mLightDir; }

	// Gets where the light is pointing
	inline LLVector4 getClampedLightDir() const	{ return mClampedLightDir; }

	// Adds a param to the list
	bool addParamSet(const std::string& name, LLWLParamSet& param);

	// Adds a param to the list
	bool addParamSet(const std::string& name, const LLSD& param);

	// Gets a param from the list
	bool getParamSet(const std::string& name, LLWLParamSet& param);

	// Sets the param in the list with a new param
	inline bool setParamSet(const std::string& name, LLWLParamSet& param)
	{
		mParamList.emplace(name, param);
		return true;
	}

	// Sets the param in the list with a new param
	bool setParamSet(const std::string& name, const LLSD& param);

	// Gets rid of a parameter and any references to it. Returns true if
	// successful
	bool removeParamSet(const std::string& name, bool delete_from_disk);

	// Lightshare support
	void processLightshareMessage(LLMessageSystem* msg);
	void processLightshareReset(bool force = false);

	inline void setDirty()						{ mCurrentParamsDirty = true; }

	static std::vector<std::string> getLoadedPresetsList();

	static bool findPresetFile(const std::string& name,
							   const std::string& base_path,
							   std::string& filename, std::string& path);

public:
	LLWLAnimator	mAnimator;

	// Actual direction of the sun
	LLVector4		mLightDir;

	// Clamped light norm for shaders that are adversely affected when the sun
	// goes below the horizon
	LLVector4		mClampedLightDir;

	// List of params and how they are cycled for days
	LLWLDayCycle	mDay;

	LLWLParamSet	mCurParams;

	WLFloatControl	mWLGamma;

	F32				mSceneLightStrength;

	// Atmospherics
	WLColorControl	mBlueHorizon;
	WLColorControl	mHazeDensity;
	WLColorControl	mBlueDensity;
	WLFloatControl	mDensityMult;
	WLColorControl	mHazeHorizon;
	WLFloatControl	mMaxAlt;

	// Lighting
	WLColorControl	mLightnorm;
	WLColorControl	mSunlight;
	WLColorControl	mAmbient;
	WLColorControl	mGlow;

	// Clouds
	WLColorControl	mCloudColor;
	WLColorControl	mCloudMain;
	WLFloatControl	mCloudCoverage;
	WLColorControl	mCloudDetail;
	WLFloatControl	mDistanceMult;
	WLFloatControl	mCloudScale;

	// List of all the parameter sets, indexed by name
	typedef std::map<std::string, LLWLParamSet> paramset_map_t;
	paramset_map_t	mParamList;

	bool			mHasLightshareOverride;
	bool			mCurrentParamsDirty;
};

extern LLWLSkyParamMgr gWLSkyParamMgr;

#endif
