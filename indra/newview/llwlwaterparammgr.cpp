/**
 * @file llwlwaterparammgr.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llwlwaterparammgr.h"

#include "imageids.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfasttimer.h"
#include "llrender.h"
#include "llsdserialize.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llfloaterwindlight.h"
#include "llsky.h"

#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerregion.h"
#include "llwlskyparammgr.h"

LLWLWaterParamMgr gWLWaterParamMgr;

///////////////////////////////////////////////////////////////////////////////
// LLWaterParamSet class
///////////////////////////////////////////////////////////////////////////////


void LLWaterParamSet::updateHashedNames()
{
	mParamHashedNames.clear();
	// Iterate through values
	for (LLSD::map_iterator iter = mParamValues.beginMap(),
							end = mParamValues.endMap();
		 iter != end; ++iter)
	{
		mParamHashedNames.push_back(LLStaticHashedString(iter->first));
	}
}




///////////////////////////////////////////////////////////////////////////////
// LLWLWaterParamMgr class proper
///////////////////////////////////////////////////////////////////////////////

LLWLWaterParamMgr::LLWLWaterParamMgr()
:	mFogColor(22.f / 255.f, 43.f / 255.f, 54.f / 255.f, 0.f, 0.f,
			  "waterFogColor", "WaterFogColor"),
	mFogDensity(4, "waterFogDensity", 2),
	mUnderWaterFogMod(0.25, "underWaterFogMod"),
	mNormalScale(2.f, 2.f, 2.f, "normScale"),
	mFresnelScale(0.5f, "fresnelScale"),
	mFresnelOffset(0.4f, "fresnelOffset"),
	mScaleAbove(0.025f, "scaleAbove"),
	mScaleBelow(0.2f, "scaleBelow"),
	mBlurMultiplier(0.1f, "blurMultiplier"),
	mWave1Dir(.5f, .5f, "wave1Dir"),
	mWave2Dir(.5f, .5f, "wave2Dir"),
	mDensitySliderValue(1.f),
	mWaterFogKS(1.f)
{
}

void LLWLWaterParamMgr::initClass()
{
	llinfos << "Initializing." << llendl;
	loadAllPresets(LLStringUtil::null);
	getParamSet("Default", mCurParams);
}

void LLWLWaterParamMgr::loadAllPresets(const std::string& file_name)
{
	std::string name;
	std::string path_name = LLWLDayCycle::getSysDir("water");
	llinfos << "Loading Default WindLight water settings from " << path_name
			<< llendl;
	{
		LLDirIterator iter(path_name, "*.xml");
		while (iter.next(name))
		{
			name = LLURI::unescape(name.erase(name.length() - 4));
			LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
			loadPreset(name, false);
		}
	}	// Destroys LLDirIterator iter

	// And repeat for user presets, note the user presets will modify any
	// system presets already loaded

	path_name = LLWLDayCycle::getUserDir("water");
	llinfos << "Loading User WindLight water settings from " << path_name
			<< llendl;
	{
		LLDirIterator iter(path_name, "*.xml");
		while (iter.next(name))
		{
			name = LLURI::unescape(name.erase(name.length() - 4));
			LL_DEBUGS("Windlight") << "Name: " << name << LL_ENDL;
			loadPreset(name, false);
		}
	}	// Destroys LLDirIterator iter
}

bool LLWLWaterParamMgr::loadPreset(const std::string& name, bool propagate)
{
	std::string filename =  LLWLDayCycle::makeFileName(name);
	// First try as if filename contains a full path
	std::string fullname = filename;
	llifstream presets_xml(fullname.c_str());
	if (!presets_xml.is_open())
	{
		// That failed, try loading from the user settings instead.
		fullname = LLWLDayCycle::getUserDir("water") + filename;
		presets_xml.open(fullname.c_str());
	}
	if (!presets_xml.is_open())
	{
		// That failed, try loading from the viewer installation instead.
		fullname = LLWLDayCycle::getSysDir("water") + filename;
		presets_xml.open(fullname.c_str());
	}
	if (!presets_xml.is_open())
	{
		llwarns << "Cannot find preset '" << name << "'" << llendl;
		return false;
	}

	llinfos << "Loading WindLight water settings from " << fullname << llendl;

	LLSD params_data(LLSD::emptyMap());
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	parser->parse(presets_xml, params_data, LLSDSerialize::SIZE_UNLIMITED);

	if (mParamList.count(name))
	{
		setParamSet(name, params_data);
	}
	else
	{
		addParamSet(name, params_data);
	}

	presets_xml.close();

	if (propagate)
	{
		getParamSet(name, mCurParams);
		propagateParameters();
		// if (gAutomationp && name != "current parcel environment")
		// {
		// 	gAutomationp->onWindlightChange("", name, "");
		// }
	}

	return true;
}

void LLWLWaterParamMgr::savePreset(const std::string& name)
{
	// Make an empty llsd
	LLSD params_data(LLSD::emptyMap());

	// Fill it with LLSD windlight params
	params_data = mParamList[name].getAll();

	// Write to file
	std::string filename = LLWLDayCycle::getUserDir("water") +
						   LLWLDayCycle::makeFileName(name);
	llofstream presets_xml(filename.c_str());
	if (presets_xml.is_open())
	{
		LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
		formatter->format(params_data, presets_xml,
						  LLSDFormatter::OPTIONS_PRETTY);
		presets_xml.close();
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for writing."
				<< llendl;
	}

	propagateParameters();
}

//static
std::vector<std::string> LLWLWaterParamMgr::getLoadedPresetsList()
{
	std::vector<std::string> result;
	const paramset_map_t& presets = gWLWaterParamMgr.mParamList;
	for (paramset_map_t::const_iterator it = presets.begin(),
											 end = presets.end();
		 it != end; ++it)
	{
		result.emplace_back(it->first);
	}
	return result;
}

void LLWLWaterParamMgr::propagateParameters()
{
	bool err;
	F32 fog_density_slider =
		logf(mCurParams.getFloat(mFogDensity.mName, err)) /
		logf(mFogDensity.mBase);

	setDensitySliderValue(fog_density_slider);

	// Translate current Windlight water settings into their Extended Environment
	// equivalent
	LLSD msg;
	LLSettingsWater::ptr_t waterp =
		LLEnvSettingsWater::buildFromLegacyPreset(mCurParams.mName,
												  mCurParams.getAll(), msg);
	// Apply the translated settings to the local environment
	if (waterp)
	{
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, waterp);
	}
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
										LLEnvironment::TRANSITION_FAST);
}

bool LLWLWaterParamMgr::addParamSet(const std::string& name,
									LLWaterParamSet& param)
{
	// Add a new one if not one there already
	if (mParamList.count(name))
	{
		return false;
	}
	mParamList[name] = param;
	return true;
}

bool LLWLWaterParamMgr::addParamSet(const std::string& name, const LLSD& param)
{
	// Add a new one if not one there already
	if (mParamList.count(name))
	{
		return false;
	}
	mParamList[name].setAll(param);
	return true;
}

bool LLWLWaterParamMgr::getParamSet(const std::string& name,
									LLWaterParamSet& param)
{
	// Find it and set it
	paramset_map_t::iterator it = mParamList.find(name);
	if (it != mParamList.end())
	{
		param = it->second;
		param.mName = name;
		return true;
	}
	return false;
}

bool LLWLWaterParamMgr::setParamSet(const std::string& name,
									const LLSD& param)
{
	// Quick, non robust (we would not be working with files, but assets) check
	if (!param.isMap())
	{
		return false;
	}
	mParamList[name].setAll(param);
	return true;
}

bool LLWLWaterParamMgr::removeParamSet(const std::string& name,
									   bool delete_from_disk)
{
	paramset_map_t::iterator it = mParamList.find(name);
	if (it == mParamList.end())
	{
		llwarns << "No Windlight water preset named '" << name << "'"
				<< llendl;
		return false;
	}

	// Remove from param list
	mParamList.erase(it);

	if (delete_from_disk)
	{
		// LLDirIterator::deleteFilesInDir(LLWLDayCycle::getUserDir("water"),
		// 								LLWLDayCycle::makeFileName(name).c_str());
	}

	return true;
}

F32 LLWLWaterParamMgr::getFogDensity()
{
	bool err;

	F32 fog_density = mCurParams.getFloat("waterFogDensity", err);

	// Modify if we are underwater
	LLViewerRegion* regionp = gAgent.getRegion();
	const F32 water_height = regionp ? regionp->getWaterHeight() : 0.01f;
	F32 camera_height = gAgentCamera.getCameraPositionAgent().mV[2];
	
	if (camera_height <= water_height)
	{
		// Raise it to the underwater fog density modifier
		fog_density = powf(fog_density,
						   mCurParams.getFloat("underWaterFogMod", err));
	}

	return fog_density;
}
