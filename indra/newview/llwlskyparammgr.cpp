/**
 * @file llwlskyparammgr.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llwlskyparammgr.h"

#include "lldate.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfasttimer.h"
#include "llgl.h"
#include "message.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lluri.h"

#include "llagent.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llfloaterwindlight.h"
//MK
//#include "mkrlinterface.h"
//rlv
//mk
#include "llsky.h"

#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llwaterparammanager.h"
#include "llnotifications.h"

LLWLSkyParamMgr gWLSkyParamMgr;

///////////////////////////////////////////////////////////////////////////////
// Structures used for Lightshare only
//
// Note: using the LightsharePacket structure to mirror the binary bucket data
// layout is a really dirty hack since there is no guarantee about what padding
// or byte alignment might be used by various C++ compilers between the various
// structure members. It *happens* to work, at least for now, with gcc, clang
// and Visual C++ thanks to #pragma pack which they all happen to understand...
///////////////////////////////////////////////////////////////////////////////

#pragma pack(push)
#pragma pack(1)

struct LSColor3
{
	F32	red;
	F32	green;
	F32	blue;
};

struct LSVector3
{
	F32	X;
	F32	Y;
	F32	Z;
};

struct LSVector2
{
	F32	X;
	F32	Y;
};

struct LSColor4
{
	F32	red;
	F32	green;
	F32	blue;
	F32 alpha;
};

struct LightsharePacket
{
	LSColor3		waterColor;
	F32				waterFogDensityExponent;
	F32				underwaterFogModifier;
	LSVector3		reflectionWaveletScale;
	F32				fresnelScale;
	F32				fresnelOffset;
	F32				refractScaleAbove;
	F32				refractScaleBelow;
	F32				blurMultiplier;
	LSVector2		littleWaveDirection;
	LSVector2		bigWaveDirection;
	U8				normalMapTexture[16];
	LSColor4		horizon;
	F32				hazeHorizon;
	LSColor4		blueDensity;
	F32				hazeDensity;
	F32				densityMultiplier;
	F32				distanceMultiplier;
	LSColor4		sunMoonColor;
	F32				sunMoonPosition;
	LSColor4		ambient;
	F32				eastAngle;
	F32				sunGlowFocus;
	F32				sunGlowSize;
	F32				sceneGamma;
	F32				starBrightness;
	LSColor4		cloudColor;
	LSVector3		cloudXYDensity;
	F32				cloudCoverage;
	F32				cloudScale;
	LSVector3		cloudDetailXYDensity;
	F32				cloudScrollX;
	F32				cloudScrollY;
	U16				maxAltitude;
	U8				cloudScrollXLock;
	U8				cloudScrollYLock;
	U8				drawClassicClouds;
};

#pragma pack(pop)





////////////////////////////////////////////////////////////////////////////////
// LLWLSkyParamMgr class proper
///////////////////////////////////////////////////////////////////////////////

LLWLSkyParamMgr::LLWLSkyParamMgr()
:	mHasLightshareOverride(false),
	mCurrentParamsDirty(true),
	// Sun Delta Terrain tweak variables.
	mSceneLightStrength(2.f),
	mWLGamma(1.f, "gamma"),
	mBlueHorizon(0.25f, 0.25f, 1.f, 1.f, "blue_horizon", "WLBlueHorizon"),
	mHazeDensity(1.f, 1.f, 1.f, 0.5f, "haze_density"),
	mBlueDensity(0.25f, 0.25f, 0.25f, 1.f, "blue_density", "WLBlueDensity"),
	mDensityMult(1.f, "density_multiplier", 1000),
	mHazeHorizon(1.f, 1.f, 1.f, 0.5f, "haze_horizon"),
	mMaxAlt(4000.f, "max_y"),
	// Lighting
	mLightnorm(0.f, 0.707f, -0.707f, 1.f, "lightnorm"),
	mSunlight(0.5f, 0.5f, 0.5f, 1.f, "sunlight_color", "WLSunlight"),
	mAmbient(0.5f, 0.75f, 1.f, 1.19f, "ambient", "WLAmbient"),
	mGlow(18.f, 0.f, -0.01f, 1.f, "glow"),
	// Clouds
	mCloudColor(0.5f, 0.5f, 0.5f, 1.f, "cloud_color", "WLCloudColor"),
	mCloudMain(0.5f, 0.5f, 0.125f, 1.f, "cloud_pos_density1"),
	mCloudCoverage(0.f, "cloud_shadow"),
	mCloudDetail(0.f, 0.f, 0.f, 1.f, "cloud_pos_density2"),
	mDistanceMult(1.f, "distance_multiplier"),
	mCloudScale(0.42f, "cloud_scale")
{
}

void LLWLSkyParamMgr::initClass()
{
	llinfos << "Initializing." << llendl;

	loadPresets();

	// Load the day
	mDay.loadDayCycle("Default.xml",LLEnvKey::SCOPE_LOCAL);

	// *HACK/FIXME: set cloud scrolling to what we want.
	getParamSet("Default", mCurParams);

	// Set it to noon
	resetAnimator(0.5f, true);
}

void LLWLSkyParamMgr::loadPresets()
{
	//genesis comment
	LL_INFOS() << "unimplemented method LLWLSkyParamMgr::loadPresets()" << LL_ENDL;
	//do we want local presets?
	//genesis comment
}

bool LLWLSkyParamMgr::loadPreset(const std::string& name, bool propagate)
{
	//genesis comment
	LL_INFOS() << "unimplemented method LLWLSkyParamMgr::loadPresets()" << LL_ENDL;
	//do we want local presets?
	//genesis comment
	return true;
}

void LLWLSkyParamMgr::savePreset(const std::string& name)
{
	//genesis comment
	LL_INFOS() << "unimplemented method LLWLSkyParamMgr::savePreset()" << LL_ENDL;
	//do we want local presets?
	//genesis comment
}

//static
std::vector<std::string> LLWLSkyParamMgr::getLoadedPresetsList()
{
	std::vector<std::string> result;
	const paramset_map_t& presets = gWLSkyParamMgr.mParamList;
	for (paramset_map_t::const_iterator it = presets.begin(),
											 end = presets.end();
		 it != end; ++it)
	{
		result.emplace_back(it->first);
	}
	return result;
}

void LLWLSkyParamMgr::propagateParameters()
{
	// Set the sun direction from SunAngle and EastAngle
	F32 theta = mCurParams.getEastAngle();
	F32 phi = mCurParams.getSunAngle();
	F32 sin_phi = sinf(phi);
	F32 cos_phi = cosf(phi);

	LLVector4 sun_dir;
	sun_dir.mV[0] = -sinf(theta) * cos_phi;
	sun_dir.mV[1] = sin_phi;
	sun_dir.mV[2] = cosf(theta) * cos_phi;
	sun_dir.mV[3] = 0.f;

	// Is the normal from the Sun or the Moon ?
	if (sin_phi >= 0.f)
	{
		mLightDir = sun_dir;
	}
	else if (sin_phi >= NIGHTTIME_ELEVATION_COS)
	{
		// Clamp v1 to 0 so Sun never points up and causes weirdness on some
		// machines
		LLVector3 vec(sun_dir.mV[0], 0.f, sun_dir.mV[2]);
		vec.normalize();
		mLightDir = LLVector4(vec, 0.f);
	}
	else
	{
		// *HACK: Sun and Moon are always on opposite side of SL...
		mLightDir = -sun_dir;
	}

	// Calculate the clamp lightnorm for sky (to prevent ugly banding in sky
	// when haze goes below the horizon
	mClampedLightDir = sun_dir;
	if (mClampedLightDir.mV[1] < -0.1f)
	{
		mClampedLightDir.mV[1] = -0.1f;
	}

	mCurParams.set("lightnorm", mLightDir);

	// Get the cfr version of the Sun's direction
	LLVector3 cfr_sun_dir(sun_dir.mV[2], sun_dir.mV[0], sun_dir.mV[1]);
	// Set direction, overriding Sun position
	gSky.setOverrideSun(true);
	gSky.setSunDirection(cfr_sun_dir, LLVector3::zero);

	// Translate current Windlight sky settings into their Extended Environment
	// equivalent
	LLSD msg;
	LLSettingsSky::ptr_t skyp =
		LLEnvSettingsSky::buildFromLegacyPreset(mCurParams.mName,
												mCurParams.getAll(), msg);
	// Apply the translated settings to the local environment
	if (skyp)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp);
	}
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
										LLEnvironment::TRANSITION_FAST);
}

void LLWLSkyParamMgr::animate(bool enable)
{
	
	mAnimator.activate(LLWLAnimator::TIME_LOCAL);
	if (enable)
	{
		gSky.setOverrideSun(false);
	}
	static LLCachedControl<bool> parcel_env(gSavedSettings,
											"UseParcelEnvironment");
	if (enable && parcel_env)
	{
		gSavedSettings.setBOOL("UseParcelEnvironment", false);
	}
	
}

void LLWLSkyParamMgr::resetAnimator(F32 cur_time, bool run)
{
	//genesis comment

	//mAnimator.setTrack(mDay.mTimeMap, mDay.mDayLenth, cur_time, run);
	//Genesis comment

}

bool LLWLSkyParamMgr::addParamSet(const std::string& name,
								  LLWLParamSet& param)
{
	// Add a new one if not one there already
	if (mParamList.count(name))
	{
		return false;
	}
	LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
	mParamList[name] = param;
	return true;
}

bool LLWLSkyParamMgr::addParamSet(const std::string& name, const LLSD& param)
{
	// Add a new one if not one there already
	if (mParamList.count(name))
	{
		return false;
	}
	LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
	mParamList[name].setAll(param);
	return true;
}

bool LLWLSkyParamMgr::getParamSet(const std::string& name, LLWLParamSet& param)
{
	// Find it and set it
	paramset_map_t::iterator it = mParamList.find(name);
	if (it != mParamList.end())
	{
		LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
		param = it->second;
		param.mName = name;
		return true;
	}
	return false;
}

bool LLWLSkyParamMgr::setParamSet(const std::string& name, const LLSD& param)
{
	// Quick, non robust (we would not be working with files, but assets) check
	if (param.isMap())
	{
		LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
		mParamList[name].setAll(param);
		return true;
	}
	return false;
}

bool LLWLSkyParamMgr::removeParamSet(const std::string& name,
									 bool delete_from_disk)
{
	//genesis comment
	LL_INFOS() <<"unimplemented method LLWLSkyParamMgr::removeParamSet" <<LL_ENDL;
	//genesis comment 
	return true;
}

void LLWLSkyParamMgr::processLightshareMessage(LLMessageSystem* msg)
{
	static LLCachedControl<bool> enabled(gSavedSettings, "LightshareEnabled");
	if (!enabled)
	{
		LL_DEBUGS("Windlight") << "Mesage received from sim, but Lightshare is disabled."
							   << LL_ENDL;
		return;
	}

	static const char wdefault[] =
		"\x00\x00\x80\x40\x00\x00\x18\x42\x00\x00\x80\x42\x00\x00\x80\x40\x00\x00\x80\x3e\x00\x00\x00\x40\x00\x00\x00\x40\x00\x00\x00\x40\xcd\xcc\xcc\x3e\x00\x00\x00\x3f\x8f\xc2\xf5\x3c\xcd\xcc\x4c\x3e\x0a\xd7\x23\x3d\x66\x66\x86\x3f\x3d\x0a\xd7\xbe\x7b\x14\x8e\x3f\xe1\x7a\x94\xbf\x82\x2d\xed\x49\x9a\x6c\xf6\x1c\xcb\x89\x6d\xf5\x4f\x42\xcd\xf4\x00\x00\x80\x3e\x00\x00\x80\x3e\x0a\xd7\xa3\x3e\x0a\xd7\xa3\x3e\x5c\x8f\x42\x3e\x8f\xc2\xf5\x3d\xae\x47\x61\x3e\x5c\x8f\xc2\x3e\x5c\x8f\xc2\x3e\x33\x33\x33\x3f\xec\x51\x38\x3e\xcd\xcc\x4c\x3f\x8f\xc2\x75\x3e\xb8\x1e\x85\x3e\x9a\x99\x99\x3e\x9a\x99\x99\x3e\xd3\x4d\xa2\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x33\x33\xb3\x3e\x00\x00\x00\x00\xcd\xcc\xcc\x3d\x00\x00\xe0\x3f\x00\x00\x80\x3f\x00\x00\x00\x00\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x85\xeb\xd1\x3e\x00\x00\x80\x3f\x14\xae\x07\x3f\x00\x00\x80\x3f\x71\x3d\x8a\x3e\x3d\x0a\xd7\x3e\x00\x00\x80\x3f\x14\xae\x07\x3f\x8f\xc2\xf5\x3d\xcd\xcc\x4c\x3e\x0a\xd7\x23\x3c\x45\x06\x00";
	char buf[250];
	LLWaterParamSet water;
	LLWLParamSet wl;
	std::string uuid_str;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
	for (S32 i = 0; i < count; ++i)
	{
		S32 size = msg->getSizeFast(_PREHASH_ParamList, i, _PREHASH_Parameter);
		if (size < 0)
		{
			llwarns << "Received invalid Lightshare data packet with size "
					<< size << " in param list #" << i << llendl;
			continue;
		}

		llinfos << "Applying Lightshare settings list #" << i << llendl;
		mHasLightshareOverride = true;

		msg->getBinaryDataFast(_PREHASH_ParamList, _PREHASH_Parameter, buf,
							   size, i, 249);
		if (!memcmp(wdefault, buf, sizeof(wdefault)))
		{
			LL_DEBUGS("Windlight") << "LightShare matches default" << LL_ENDL;
			processLightshareReset();
			return;
		}
		LightsharePacket* pkt = (LightsharePacket*)buf;

		// Apply water parameters

		LLWaterParamManager::instance().getParamSet("Default", water);

		water.set("waterFogColor", pkt->waterColor.red / 256.f,
				  pkt->waterColor.green / 256.f, pkt->waterColor.blue / 256.f);
		water.set("waterFogDensity", powf(2.f, pkt->waterFogDensityExponent));
		water.set("underWaterFogMod", pkt->underwaterFogModifier);
		water.set("normScale", pkt->reflectionWaveletScale.X,
				  pkt->reflectionWaveletScale.Y,
				  pkt->reflectionWaveletScale.Z);
		water.set("fresnelScale", pkt->fresnelScale);
		water.set("fresnelOffset", pkt->fresnelOffset);
		water.set("scaleAbove", pkt->refractScaleAbove);
		water.set("scaleBelow", pkt->refractScaleBelow);
		water.set("blurMultiplier", pkt->blurMultiplier);
		water.set("wave1Dir", pkt->littleWaveDirection.X,
				  pkt->littleWaveDirection.Y);
		water.set("wave2Dir", pkt->bigWaveDirection.X,
				  pkt->bigWaveDirection.Y);

		uuid_str =
			llformat("%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
					 pkt->normalMapTexture[0], pkt->normalMapTexture[1],
					 pkt->normalMapTexture[2], pkt->normalMapTexture[3],
					 pkt->normalMapTexture[4], pkt->normalMapTexture[5],
					 pkt->normalMapTexture[6], pkt->normalMapTexture[7],
					 pkt->normalMapTexture[8], pkt->normalMapTexture[9],
					 pkt->normalMapTexture[10], pkt->normalMapTexture[11],
					 pkt->normalMapTexture[12], pkt->normalMapTexture[13],
					 pkt->normalMapTexture[14], pkt->normalMapTexture[15]);

		LLWaterParamManager::instance().mCurParams = water;
		LLWaterParamManager::instance().setNormalMapID(LLUUID(uuid_str));
		LLWaterParamManager::instance().propagateParameters();

		// Apply Windlight parameters

		mAnimator.deactivate();
		getParamSet("Default", wl);

		wl.setSunAngle(F_TWO_PI * pkt->sunMoonPosition);
		wl.setEastAngle(F_TWO_PI * pkt->eastAngle);
		wl.set("sunlight_color", pkt->sunMoonColor.red * 3.f,
			   pkt->sunMoonColor.green * 3.f, pkt->sunMoonColor.blue * 3.f,
			   pkt->sunMoonColor.alpha * 3.f);
		wl.set("ambient", pkt->ambient.red * 3.f, pkt->ambient.green * 3.f,
			   pkt->ambient.blue * 3.f, pkt->ambient.alpha * 3.f);
		wl.set("blue_horizon", pkt->horizon.red * 2.f,
			   pkt->horizon.green * 2.f, pkt->horizon.blue * 2.f,
			   pkt->horizon.alpha * 2.f);
		wl.set("blue_density", pkt->blueDensity.red * 2.f,
			   pkt->blueDensity.green * 2.f, pkt->blueDensity.blue * 2.f,
			   pkt->blueDensity.alpha * 2.f);
		wl.set("haze_horizon", pkt->hazeHorizon, pkt->hazeHorizon,
			   pkt->hazeHorizon, 1.f);
		wl.set("haze_density", pkt->hazeDensity, pkt->hazeDensity,
			   pkt->hazeDensity, 1.f);
		wl.set("cloud_shadow", pkt->cloudCoverage, pkt->cloudCoverage,
			   pkt->cloudCoverage, pkt->cloudCoverage);
		wl.set("density_multiplier", pkt->densityMultiplier / 1000.f);
		wl.set("distance_multiplier", pkt->distanceMultiplier,
			   pkt->distanceMultiplier, pkt->distanceMultiplier,
			   pkt->distanceMultiplier);
		wl.set("max_y",(F32)pkt->maxAltitude);
		wl.set("cloud_color", pkt->cloudColor.red, pkt->cloudColor.green,
			   pkt->cloudColor.blue, pkt->cloudColor.alpha);
		wl.set("cloud_pos_density1", pkt->cloudXYDensity.X,
			   pkt->cloudXYDensity.Y, pkt->cloudXYDensity.Z);
		wl.set("cloud_pos_density2", pkt->cloudDetailXYDensity.X,
			   pkt->cloudDetailXYDensity.Y, pkt->cloudDetailXYDensity.Z);
		wl.set("cloud_scale", pkt->cloudScale, 0.f, 0.f, 1.f);
		wl.set("gamma", pkt->sceneGamma, pkt->sceneGamma, pkt->sceneGamma, 0.f);
		wl.set("glow", 40.f - 20.f * pkt->sunGlowSize, 0.f,
			   -5.f * pkt->sunGlowFocus);
		wl.setCloudScrollX(pkt->cloudScrollX + 10.f);
		wl.setCloudScrollY(pkt->cloudScrollY + 10.f);
		wl.setEnableCloudScrollX(!pkt->cloudScrollXLock);
		wl.setEnableCloudScrollY(!pkt->cloudScrollYLock);
		wl.setStarBrightness(pkt->starBrightness);

		mCurParams = wl;
		propagateParameters();
	}
}

void LLWLSkyParamMgr::processLightshareReset(bool force)
{
	static LLCachedControl<bool> enabled(gSavedSettings, "LightshareEnabled");
	if (!force && !enabled)
	{
		LL_DEBUGS("Windlight") << "Mesage received from sim, but Lightshare is disabled."
							   << LL_ENDL;
		return;
	}
	if (mHasLightshareOverride)
	{
		llinfos << "Resetting Lightshare." << llendl;
		mHasLightshareOverride = false;
		getParamSet("Default", mCurParams);
		animate();
	}
}
