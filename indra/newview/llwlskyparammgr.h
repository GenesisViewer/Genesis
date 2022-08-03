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
#include "llvector4a.h"
#include "llwlanimator.h"
#include "llwlparamset.h"
#include "llwldaycycle.h"
class LLMessageSystem;



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
