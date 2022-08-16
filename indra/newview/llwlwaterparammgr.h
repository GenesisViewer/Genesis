/**
 * @file llwlwaterparammgr.h
 * @brief Implementation for the LLWLWaterParamMgr class.
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

#ifndef LL_WLWATERPARAMMGR_H
#define LL_WLWATERPARAMMGR_H

#include <map>

#include "v4color.h"
#include "llstaticstringtable.h"
#include "v4math.h"

class LLViewerCamera;

// A class representing a set of parameter values for the water
class LLWaterParamSet
{
	friend class LLWLWaterParamMgr;

public:
	LLWaterParamSet();

	// Sets the total LLSD
	void setAll(const LLSD& val);

	// Gets the total LLSD
	inline const LLSD& getAll() const		{ return mParamValues; }

	// Set a float parameter.
	// - param param_name	The name of the parameter to set.
	// - param x			The float value to set.
	void set(const std::string& param_name, F32 x);

	// Set a float2 parameter.
	// - param param_name	The name of the parameter to set.
	// - param x			The x component's value to set.
	// - param y			The y component's value to set.
	void set(const std::string& param_name, F32 x, F32 y);

	// Set a float3 parameter.
	// - param param_name	The name of the parameter to set.
	// - param x			The x component's value to set.
	// - param y			The y component's value to set.
	// - param z			The z component's value to set.
	void set(const std::string& param_name, F32 x, F32 y, F32 z);

	// Set a float4 parameter.
	// - param param_name	The name of the parameter to set.
	// - param x			The x component's value to set.
	// - param y			The y component's value to set.
	// - param z			The z component's value to set.
	// - param w			The w component's value to set.
	void set(const std::string& param_name, F32 x, F32 y, F32 z, F32 w);

	// Set a float4 parameter.
	// - param param_name	The name of the parameter to set.
	// - param val			Array of 4 floats to set the parameter to.
	void set(const std::string& param_name, const F32* val);

	// Set a float4 parameter.
	// - param param_name	The name of the parameter to set.
	// - param val			Struct 4 floats to set the parameter to.
	void set(const std::string& param_name, const LLVector4& val);

	// Set a float4 parameter.
	// - param param_name	The name of the parameter to set.
	// - param val			Struct 4 floats to set the parameter to.
	void set(const std::string& param_name, const LLColor4& val);

	// Get a float4 parameter.
	// - param param_name	The name of the parameter to set.
	// - param error		A flag to set if it's not the proper return type
	LLVector4 getVector4(const std::string& param_name, bool& error);

	// Get a float3 parameter.
	// - param param_name	The name of the parameter to set.
	// - param error		A flag to set if it's not the proper return type
	LLVector3 getVector3(const std::string& param_name, bool& error);

	// Get a float2 parameter.
	// - param param_name	The name of the parameter to set.
	// - param error		A flag to set if it's not the proper return type
	LLVector2 getVector2(const std::string& param_name, bool& error);

	// Get an integer parameter
	// - param param_name	The name of the parameter to set.
	// - param error		A flag to set if it's not the proper return type
	F32 getFloat(const std::string& param_name, bool& error);

	// interpolate two parameter sets
	// - param src			The parameter set to start with
	// - param dest			The parameter set to end with
	// - param weight		The amount to interpolate
	void mix(LLWaterParamSet& src, LLWaterParamSet& dest, F32 weight);

private:
	void updateHashedNames();

public:
	std::string mName;

private:
	LLSD			mParamValues;
	typedef std::vector<LLStaticHashedString> hash_vector_t;
	hash_vector_t	mParamHashedNames;
};

// Color control structure
struct WaterColorControl
{
	F32 mR, mG, mB, mA, mI;			// the values
	std::string mName;				// name to use to dereference params
	std::string mSliderName;		// name of the slider in menu
	bool mHasSliderName;			// only set slider name for true color types

	inline WaterColorControl(F32 red, F32 green, F32 blue, F32 alpha,
								F32 intensity, const std::string& n,
								const std::string& sliderName = LLStringUtil::null)
	:	mR(red),
		mG(green),
		mB(blue),
		mA(alpha),
		mI(intensity),
		mName(n),
		mSliderName(sliderName)
	{
		// if there's a slider name, say we have one
		mHasSliderName = false;
		if (mSliderName != "")
		{
			mHasSliderName = true;
		}
	}

	inline WaterColorControl& operator=(const LLColor4& val)
	{
		mR = val.mV[0];
		mG = val.mV[1];
		mB = val.mV[2];
		mA = val.mV[3];
		return *this;
	}

	inline operator LLColor4(void) const
	{
		return LLColor4(mR, mG, mB, mA);
	}

	inline WaterColorControl& operator=(const LLVector4& val)
	{
		mR = val.mV[0];
		mG = val.mV[1];
		mB = val.mV[2];
		mA = val.mV[3];
		return *this;
	}

	inline operator LLVector4(void) const
	{
		return LLVector4(mR, mG, mB, mA);
	}

	inline operator LLVector3(void) const
	{
		return LLVector3(mR, mG, mB);
	}

	inline void update(LLWaterParamSet& params) const
	{
		params.set(mName, mR, mG, mB, mA);
	}
};

struct WaterVector3Control
{
	F32 mX;
	F32 mY;
	F32 mZ;

	std::string mName;

	// Basic constructor
	inline WaterVector3Control(F32 valX, F32 valY, F32 valZ,
								  const std::string& n)
	:	mX(valX),
		mY(valY),
		mZ(valZ),
		mName(n)
	{
	}

	inline WaterVector3Control& operator=(const LLVector3& val)
	{
		mX = val.mV[0];
		mY = val.mV[1];
		mZ = val.mV[2];
		return *this;
	}

	inline void update(LLWaterParamSet& params) const
	{
		params.set(mName, mX, mY, mZ);
	}

};

struct WaterVector2Control
{
	F32 mX;
	F32 mY;
	std::string mName;

	// basic constructor
	inline WaterVector2Control(F32 valX, F32 valY, const std::string& n)
	:	mX(valX),
		mY(valY),
		mName(n)
	{
	}

	inline WaterVector2Control& operator=(const LLVector2& val)
	{
		mX = val.mV[0];
		mY = val.mV[1];
		return *this;
	}

	inline void update(LLWaterParamSet& params) const
	{
		params.set(mName, mX, mY);
	}
};

// float slider control
struct WaterFloatControl
{
	F32 mX;
	F32 mMult;
	std::string mName;

	inline WaterFloatControl(F32 val, const std::string& n, F32 m = 1.f)
	:	mX(val),
		mName(n),
		mMult(m)
	{
	}

	inline WaterFloatControl& operator = (const LLVector4& val)
	{
		mX = val.mV[0];
		return *this;
	}

	inline operator F32(void) const
	{
		return mX;
	}

	inline void update(LLWaterParamSet& params) const
	{
		params.set(mName, mX);
	}
};

// float slider control
struct WaterExpFloatControl
{
	F32 mExp;
	F32 mBase;
	std::string mName;

	inline WaterExpFloatControl(F32 val, const std::string& n, F32 b)
	:	mExp(val),
		mName(n),
		mBase(b)
	{
	}

	inline WaterExpFloatControl& operator=(F32 val)
	{
		mExp = logf(val) / logf(mBase);
		return *this;
	}

	inline operator F32(void) const
	{
		return powf(mBase, mExp);
	}

	inline void update(LLWaterParamSet& params) const
	{
		params.set(mName, powf(mBase, mExp));
	}
};

// Parameter manager class for the Windlight water
class LLWLWaterParamMgr
{
protected:
	LOG_CLASS(LLWLWaterParamMgr);

private:
public:
	LLWLWaterParamMgr();

	// This method is called in llappviewer.cpp only
	void initClass();

	// Loads a preset file
	void loadAllPresets(const std::string& filename);

	// Loads an individual preset for the water. Returns true if successful.
	bool loadPreset(const std::string& name, bool propagate = true);

	// Saves the parameter presets to file
	void savePreset(const std::string& name);

	// Propagates the parameters to the environment
	void propagateParameters();

	// Cleans up global data that is only inited once per class.
	static void cleanupClass();

	// Adds a param to the list
	bool addParamSet(const std::string& name, LLWaterParamSet& param);

	// Adds a param to the list
	bool addParamSet(const std::string& name, const LLSD& param);

	// Geta a param from the list
	bool getParamSet(const std::string& name, LLWaterParamSet& param);

	// Sets the param in the list with a new param
	inline bool setParamSet(const std::string& name, LLWaterParamSet& param)
	{
		mParamList.emplace(name, param);
		return true;
	}

	// Sets the param in the list with a new param
	bool setParamSet(const std::string& name, const LLSD& param);

	// Gets rid of a parameter and any references to it
	// returns true if successful
	bool removeParamSet(const std::string& name, bool delete_from_disk);

	// Sets the normap map we want for water
	inline bool setNormalMapID(const LLUUID& id)
	{
		mCurParams.mParamValues["normalMap"] = id;
		return true;
	}

	inline void setDensitySliderValue(F32 val)
	{
		val = 1.f - 0.1f * val;
		mDensitySliderValue = val * val * val;
	}

	inline LLUUID getNormalMapID()
	{
		return mCurParams.mParamValues["normalMap"].asUUID();
	}

	inline LLVector2 getWave1Dir()
	{
		bool err;
		return mCurParams.getVector2("wave1Dir", err);
	}

	inline LLVector2 getWave2Dir()
	{
		bool err;
		return mCurParams.getVector2("wave2Dir", err);
	}

	inline F32 getScaleAbove()
	{
		bool err;
		return mCurParams.getFloat("scaleAbove", err);
	}

	inline F32 getScaleBelow()
	{
		bool err;
		return mCurParams.getFloat("scaleBelow", err);
	}

	inline LLVector3 getNormalScale()
	{
		bool err;
		return mCurParams.getVector3("normScale", err);
	}

	inline F32 getFresnelScale()
	{
		bool err;
		return mCurParams.getFloat("fresnelScale", err);
	}

	inline F32 getFresnelOffset()
	{
		bool err;
		return mCurParams.getFloat("fresnelOffset", err);
	}

	inline F32 getBlurMultiplier()
	{
		bool err;
		return mCurParams.getFloat("blurMultiplier", err);
	}

	inline LLColor4 getFogColor()
	{
		bool err;
		return LLColor4(mCurParams.getVector4("waterFogColor", err));
	}

	F32 getFogDensity();

	static std::vector<std::string> getLoadedPresetsList();

public:
	LLWaterParamSet			mCurParams;

	// Atmospherics
	WaterColorControl		mFogColor;
	WaterExpFloatControl	mFogDensity;
	WaterFloatControl		mUnderWaterFogMod;

	// Wavelet scales and directions
	WaterVector3Control		mNormalScale;
	WaterVector2Control		mWave1Dir;
	WaterVector2Control		mWave2Dir;

	// Control how water is reflected and refracted
	WaterFloatControl		mFresnelScale;
	WaterFloatControl		mFresnelOffset;
	WaterFloatControl		mScaleAbove;
	WaterFloatControl		mScaleBelow;
	WaterFloatControl		mBlurMultiplier;

	// List of all the parameters, listed by name
	typedef std::map<std::string, LLWaterParamSet> paramset_map_t;
	paramset_map_t			mParamList;

	F32						mDensitySliderValue;

private:
	LLVector4				mWaterPlane;
	F32						mWaterFogKS;
};

extern LLWLWaterParamMgr gWLWaterParamMgr;

#endif
