/**
 * @file llenvsettings.cpp
 * @brief Subclasses for viewer specific settings behaviors.
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

#include "llenvsettings.h"

#include "imageids.h"
#include "llassetstorage.h"
#include "lldir.h"
#include "llnotifications.h"
#include "llpermissions.h"
#include "llsdutil.h"
#include "lltrans.h"
#include "lluri.h"

#include "llagent.h"
#include "lldrawpoolwater.h"
#include "llenvironment.h"
#include "llfloaterperms.h"
#include "llinventorymodel.h"
#include "llsky.h"
#include "llviewerassetupload.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"

constexpr F32 WATER_FOG_LIGHT_CLAMP = 0.3f;

// Helper class

class LLSettingsInventoryCB final : public LLInventoryCallback
{
public:
	LLSettingsInventoryCB(LLSettingsBase::ptr_t settings,
						  LLEnvSettingsBase::inv_result_fn cb)
	:	mSettings(settings),
		mCallback(cb)
	{
	}

	void fire(const LLUUID& inv_item_id) override
	{
		LLEnvSettingsBase::onInventoryItemCreated(inv_item_id, mSettings,
												  mCallback);
	}

private:
	LLSettingsBase::ptr_t				mSettings;
	LLEnvSettingsBase::inv_result_fn	mCallback;
};

///////////////////////////////////////////////////////////////////////////////
// LLEnvSettingsBase class
///////////////////////////////////////////////////////////////////////////////

//static
void LLEnvSettingsBase::createNewInventoryItem(LLSettingsType::EType stype,
											   const LLUUID& parent_id,
											   inv_result_fn callback)
{
	//*TODO: implement Settings default permissions ?
	U32 next_owner_perm = LLFloaterPerms::getNextOwnerPerms();
	next_owner_perm |= PERM_COPY;
	if (!LLEnvironment::isInventoryEnabled())
	{
		gNotifications.add("NoEnvironmentSettings");
		return;
	}
	LLPointer<LLInventoryCallback> cb =
		new LLSettingsInventoryCB(LLSettingsBase::ptr_t(), callback);
	create_inventory_item(parent_id, LLTransactionID::tnull,
						  LLSettingsType::getDefaultName(stype), "",
						  LLAssetType::AT_SETTINGS,
						  LLInventoryType::IT_SETTINGS, (U8)stype,
						  next_owner_perm, cb);
}

//static
void LLEnvSettingsBase::createInventoryItem(const LLSettingsBase::ptr_t& settings,
											const LLUUID& parent_id,
											const std::string& settings_name,
											inv_result_fn callback)
{
	U32 next_owner_perm = LLPermissions::DEFAULT.getMaskNextOwner();
	createInventoryItem(settings, next_owner_perm, parent_id, settings_name,
						callback);
}

//static
void LLEnvSettingsBase::createInventoryItem(const LLSettingsBase::ptr_t& settings,
											U32 next_owner_perm,
											const LLUUID& parent_id,
											std::string settings_name,
											inv_result_fn callback)
{

	if (!LLEnvironment::isInventoryEnabled())
	{
		gNotifications.add("NoEnvironmentSettings");
		return;
	}

	LLTransactionID tid;
	tid.generate();
	if (settings_name.empty())
	{
		settings_name = settings->getName();
	}
	LLPointer<LLInventoryCallback> cb = new LLSettingsInventoryCB(settings,
																  callback);
	create_inventory_item(parent_id, tid, settings_name, "",
						  LLAssetType::AT_SETTINGS,
						  LLInventoryType::IT_SETTINGS,
						  (U8)settings->getSettingsTypeValue(),
						  next_owner_perm, cb);
}

//static
void LLEnvSettingsBase::onInventoryItemCreated(const LLUUID& inv_id,
											   LLSettingsBase::ptr_t settings,
											   inv_result_fn callback)
{
	LLViewerInventoryItem* itemp = gInventory.getItem(inv_id);
	if (itemp)
	{
		LLPermissions perm = itemp->getPermissions();
		if (perm.getMaskEveryone() != PERM_COPY)
		{
			perm.setMaskEveryone(PERM_COPY);
			itemp->setPermissions(perm);
			itemp->updateServer(false);
		}
	}
	if (!settings)
	{
		// The item was created as new with no settings passed in. The
		// simulator should have given it the default for the type...
		// Check Id; no need to upload asset.
		LLUUID asset_id;
		if (itemp)
		{
			asset_id = itemp->getAssetUUID();
		}
		if (callback)
		{
			callback(asset_id, inv_id, LLUUID::null, LLSD());
		}
		return;
	}
	// We may need to update some inventory stuff here...
	updateInventoryItem(settings, inv_id, callback, false);
}

//static
void LLEnvSettingsBase::updateInventoryItem(const LLSettingsBase::ptr_t& settings,
											const LLUUID& inv_item_id,
											inv_result_fn callback,
											bool update_name)
{
	std::string cap_url =
		gAgent.getRegionCapability("UpdateSettingsAgentInventory");
	if (cap_url.empty())
	{
		llwarns << "No UpdateSettingsAgentInventory capability available. Cannot save setting."
				<< llendl;
		return;
	}

	if (!LLEnvironment::isInventoryEnabled())
	{
		gNotifications.add("NoEnvironmentSettings");
		return;
	}

	LLViewerInventoryItem* itemp = gInventory.getItem(inv_item_id);
	if (itemp)
	{
		bool need_update = false;
		LLPointer<LLViewerInventoryItem> new_itemp =
			new LLViewerInventoryItem(itemp);

		if (settings->getFlag(LLSettingsBase::FLAG_NOTRANS) &&
			new_itemp->getPermissions().allowOperationBy(PERM_TRANSFER,
														 gAgentID))
		{
			LLPermissions perm(itemp->getPermissions());
			perm.setBaseBits(LLUUID::null, false, PERM_TRANSFER);
			perm.setOwnerBits(LLUUID::null, false, PERM_TRANSFER);
			new_itemp->setPermissions(perm);
			need_update = true;
		}
		if (update_name && (settings->getName() != new_itemp->getName()))
		{
			new_itemp->rename(settings->getName());
			settings->setName(new_itemp->getName()); // Account for corrections
			need_update = true;
		}
		if (need_update)
		{
			new_itemp->updateServer(false);
			gInventory.updateItem(new_itemp);
			gInventory.notifyObservers();
		}
	}

	std::stringstream buffer;
	LLSD settingdata(settings->getSettings());
	LLSDSerialize::serialize(settingdata, buffer,
							 LLSDSerialize::LLSD_NOTATION);

	LLResourceUploadInfo::ptr_t info =
		std::make_shared<LLBufferedAssetUploadInfo>(inv_item_id,
													LLAssetType::AT_SETTINGS,
													buffer.str(),
													boost::bind(&LLEnvSettingsBase::onAgentAssetUploadComplete,
																_1, _2, _3, _4,
																settings,
																callback));
	LLViewerAssetUpload::enqueueInventoryUpload(cap_url, info);
}

//static
void LLEnvSettingsBase::updateInventoryItem(const LLSettingsBase::ptr_t& settings,
											const LLUUID& object_id,
											const LLUUID& inv_item_id,
											inv_result_fn callback)
{
	std::string cap_url =
		gAgent.getRegionCapability("UpdateSettingsAgentInventory");
	if (cap_url.empty())
	{
		llwarns << "No UpdateSettingsAgentInventory capability available. Cannot save setting."
				<< llendl;
		return;
	}

	if (!LLEnvironment::isInventoryEnabled())
	{
		gNotifications.add("NoEnvironmentSettings");
		return;
	}

	std::stringstream buffer;
	LLSD settingdata(settings->getSettings());
	LLSDSerialize::serialize(settingdata, buffer,
							 LLSDSerialize::LLSD_NOTATION);

	LLResourceUploadInfo::ptr_t info =
		std::make_shared<LLBufferedAssetUploadInfo>(object_id, inv_item_id,
													LLAssetType::AT_SETTINGS,
													buffer.str(),
													boost::bind(&LLEnvSettingsBase::onTaskAssetUploadComplete,
																_1, _2, _3, _4,
																settings,
																callback));
	LLViewerAssetUpload::enqueueInventoryUpload(cap_url, info);
}

//static
void LLEnvSettingsBase::onAgentAssetUploadComplete(LLUUID item_id,
												   LLUUID new_asset_id,
												   LLUUID new_item_id,
												   LLSD response,
												   LLSettingsBase::ptr_t psettings,
												   inv_result_fn callback)
{
	llinfos << "Item Id: " << item_id << " - New asset Id: " << new_asset_id
			<< " - New item Id: " << new_item_id << " - Response: " << response
			<< llendl;
	psettings->setAssetId(new_asset_id);
	if (callback)
	{
		callback(new_asset_id, item_id, LLUUID::null, response);
	}
}

//static
void LLEnvSettingsBase::onTaskAssetUploadComplete(LLUUID item_id,
												  LLUUID task_id,
												  LLUUID new_asset_id,
												  LLSD response,
												  LLSettingsBase::ptr_t settings,
												  inv_result_fn callback)
{
	llinfos << "Item Id: " << item_id << " - Task Id: " << task_id
			<< " - New asset Id: " << new_asset_id << " - Response: "
			<< response << " - Upload to task complete." << llendl;
	settings->setAssetId(new_asset_id);
	if (callback)
	{
		callback(new_asset_id, item_id, task_id, response);
	}
}

struct CallbackPtrStorage
{
	LLEnvSettingsBase::asset_download_fn callback;
};

//static
void LLEnvSettingsBase::getSettingsAsset(const LLUUID& asset_id,
										 asset_download_fn callback)
{
	CallbackPtrStorage* storage = new CallbackPtrStorage;
	storage->callback = callback;
	gAssetStoragep->getAssetData(asset_id, LLAssetType::AT_SETTINGS,
								 onAssetDownloadComplete, (void*)storage,
								 true);
}

//static
void LLEnvSettingsBase::onAssetDownloadComplete(const LLUUID& asset_id,
												LLAssetType::EType,
												void* user_data, S32 status,
												LLExtStat ext_status)
{
	if (!user_data) return;	// Paranoia
	CallbackPtrStorage* storage = (CallbackPtrStorage*)user_data;
	asset_download_fn callback = storage->callback;
	delete storage;

	LLSettingsBase::ptr_t settings;
	if (!status)
	{
		LLFileSystem file(asset_id);
		S32 size = file.getSize();

		std::string buffer(size + 1, '\0');
		file.read((U8*)buffer.data(), size);

		std::stringstream llsdstream(buffer);
		LLSD llsdsettings;

		if (LLSDSerialize::deserialize(llsdsettings, llsdstream, -1))
		{
			settings = createFromLLSD(llsdsettings);
		}

		if (!settings)
		{
			status = 1;
			llwarns << "Unable to create settings object for asset: "
					<< asset_id << llendl;
		}
		else
		{
			settings->setAssetId(asset_id);
		}
	}
	else
	{
		llwarns << "Error retrieving asset: " << asset_id << ". Status code="
				<< status << " (" << LLAssetStorage::getErrorString(status)
				<< ") ext_status=" << (U32)ext_status << llendl;
	}
	if (callback)
	{
		callback(asset_id, settings, status, ext_status);
	}
}

//static
bool LLEnvSettingsBase::exportFile(const LLSettingsBase::ptr_t& settings,
								   const std::string& filename,
								   LLSDSerialize::ELLSD_Serialize format)
{
	try
	{
		std::ofstream file(filename, std::ios::out | std::ios::trunc);
		file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
		if (!file)
		{
			llwarns << "Unable to open '" << filename << "' for writing."
					<< llendl;
			return false;
		}
		LLSDSerialize::serialize(settings->getSettings(), file, format);
	}
	catch (const std::ios_base::failure& e)
	{
		llwarns << "Unable to save settings to file '" << filename
				<< "': " << e.what() << llendl;
		return false;
	}

	return true;
}

//static
LLSettingsBase::ptr_t LLEnvSettingsBase::importFile(const std::string& filename)
{
	LLSD settings;

	try
	{
		std::ifstream file(filename, std::ios::in);
		file.exceptions(std::ios_base::failbit | std::ios_base::badbit);
		if (!file)
		{
			llwarns << "Unable to open '" << filename << "' for reading."
					<< llendl;
			return LLSettingsBase::ptr_t();
		}
		if (!LLSDSerialize::deserialize(settings, file, -1))
		{
			llwarns << "Unable to deserialize settings from '" << filename
					<< "'" << llendl;
			return LLSettingsBase::ptr_t();
		}
	}
	catch (const std::ios_base::failure& e)
	{
		llwarns << "Unable to save settings to file '" << filename << "': "
				<< e.what() << llendl;
		return LLSettingsBase::ptr_t();
	}

	return createFromLLSD(settings);
}

//static
LLSettingsBase::ptr_t LLEnvSettingsBase::createFromLLSD(const LLSD& settings)
{
	if (!settings.has(SETTING_TYPE))
	{
		llwarns << "No settings type in LLSD" << llendl;
		return LLSettingsBase::ptr_t();
	}

	std::string settingtype = settings[SETTING_TYPE].asString();

	if (settingtype == "daycycle")
	{
		return LLEnvSettingsDay::buildDay(settings);
	}
	if (settingtype == "sky")
	{
		return LLEnvSettingsSky::buildSky(settings);
	}
	if (settingtype == "water")
	{
		return LLEnvSettingsWater::buildWater(settings);
	}

	llwarns << "Unable to determine settings type for '" << settingtype
			<< "'." << llendl;
	return LLSettingsBase::ptr_t();
}

//static
LLSD LLEnvSettingsBase::readLegacyPresetData(std::string filename,
											 std::string path,
											 LLSD& messages)
{
	// Ensure path got a dir delimiter appended
	if (!path.empty() && path.back() != LL_DIR_DELIM_CHR)
	{
		path += LL_DIR_DELIM_STR;
	}
	// Ensure name does not have dir delimiter prepended
	if (!filename.empty() && filename[0] == LL_DIR_DELIM_CHR)
	{
		filename = filename.substr(1);
	}

	std::string full_path = path + filename;
	llifstream xml_file;
	xml_file.open(full_path.c_str());
	if (!xml_file)
	{
		messages["REASONS"] = LLTrans::getString("SettingImportFileError",
												 LLSDMap("FILE", full_path));
		llwarns << "Unable to open Windlight file: " << full_path << llendl;
		return LLSD();
	}

	LLSD params_data;
	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	if (parser->parse(xml_file, params_data,
					  LLSDSerialize::SIZE_UNLIMITED) == LLSDParser::PARSE_FAILURE)
	{
		xml_file.close();
		messages["REASONS"] = LLTrans::getString("SettingParseFileError",
												 LLSDMap("FILE", full_path));
		return LLSD();
	}
	xml_file.close();

	LL_DEBUGS("EnvSettings") << "Loaded: " << full_path << LL_ENDL;

	return params_data;
}

///////////////////////////////////////////////////////////////////////////////
// LLEnvSettingsSky class
///////////////////////////////////////////////////////////////////////////////

LLEnvSettingsSky::LLEnvSettingsSky(const LLSD& data, bool is_advanced)
:	LLSettingsSky(data),
	mIsAdvanced(is_advanced),
	mSceneLightStrength(3.f)
{
}

LLEnvSettingsSky::LLEnvSettingsSky()
:	LLSettingsSky(),
	mIsAdvanced(false),
	mSceneLightStrength(3.f)
{
}

//virtual
LLSettingsSky::ptr_t LLEnvSettingsSky::buildClone() const
{
	LLSD settings = cloneSettings();

	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Sky setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	U32 flags = getFlags();
	ptr_t skyp = std::make_shared<LLEnvSettingsSky>(settings);
	skyp->setFlags(flags);
	return skyp;
}

//virtual
void LLEnvSettingsSky::updateSettings()
{
	LLSettingsSky::updateSettings();

	// Note: calling these also causes an update of the positions in the sky
	// settings. It is therefore essential to call them here.
	LLVector3 sun_direction = getSunDirection();
	LLVector3 moon_direction = getMoonDirection();
	LL_DEBUGS("EnvSettings") << "Sun direction: " << sun_direction
							 << " - Moon direction: " << moon_direction
							 << LL_ENDL;

#if 1	// Is this still needed at all ?
	// Set the extended environment textures
	gSky.setSunTextures(getSunTextureId(), getNextSunTextureId());
	gSky.setMoonTextures(getMoonTextureId(), getNextMoonTextureId());
	gSky.setCloudNoiseTextures(getCloudNoiseTextureId(),
							   getNextCloudNoiseTextureId());
	gSky.setBloomTextures(getBloomTextureId(), getNextBloomTextureId());
#endif

	// We want the dot prod of Sun with high noon vector (0,0,1), which is just
	// the z component
	F32 dp = llmax(sun_direction[2], 0.f); // Clamped to 0 when sun is down

	// Since WL scales everything by 2, there should always be at least a 2:1
	// brightness ratio between sunlight and point lights in windlight to
	// normalize point lights.
	static LLCachedControl<F32> dyn_rng(gSavedSettings,
										"RenderSunDynamicRange");
	F32 sun_dynamic_range = llmax((F32)dyn_rng, 0.0001f);
	mSceneLightStrength = 1.5f + 2.f * sun_dynamic_range * dp;
	gSky.setSunAndMoonDirectionsCFR(sun_direction, moon_direction);

	gSky.setSunScale(getSunScale());
	gSky.setMoonScale(getMoonScale());
}

//virtual
void LLEnvSettingsSky::applySpecial(LLShaderUniforms* targetp)
{
	LLShaderUniforms* shader = &(targetp)[LLGLSLShader::SG_DEFAULT];
	if (shader)
	{
		shader->uniform4fv(LLShaderMgr::LIGHTNORM,
						   gEnvironment.getClampedLightNorm().mV);
		shader->uniform3fv(LLShaderMgr::WL_CAMPOSLOCAL,
						   gViewerCamera.getOrigin());
	}

	shader = &(targetp)[LLGLSLShader::SG_SKY];
	if (shader)
	{
		shader->uniform4fv(LLShaderMgr::LIGHTNORM,
						   gEnvironment.getClampedLightNorm().mV);

		const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();

		// Legacy SETTING_CLOUD_SCROLL_RATE("cloud_scroll_rate") ?
		const LLColor3& col_c_p_d1 = skyp->getCloudPosDensity1();
		LLVector4 vect_c_p_d1(col_c_p_d1.mV[0], col_c_p_d1.mV[1],
							  col_c_p_d1.mV[2], 1.f);
		LLVector4 cloud_scroll(gEnvironment.getCloudScrollDelta());
		// SL-13084 EEP added support for custom cloud textures: flip them
		// horizontally to match the preview of Clouds > Cloud Scroll.
		// Keep these shaders in sync:
		//  - ee/class2/windlight/cloudsV.glsl
		//  - ee/class1/deferred/cloudsV.glsl
		cloud_scroll[0] = -cloud_scroll[0];
		vect_c_p_d1 += cloud_scroll;
		shader->uniform4fv(LLShaderMgr::CLOUD_POS_DENSITY1, vect_c_p_d1.mV);

		LLColor4 sun_diffuse = skyp->getSunlightColor();
		shader->uniform4fv(LLShaderMgr::SUNLIGHT_COLOR, sun_diffuse.mV);

		LLColor4 moon_diffuse = skyp->getMoonlightColor();
		shader->uniform4fv(LLShaderMgr::MOONLIGHT_COLOR, moon_diffuse.mV);

		LLColor4 cloud_color(skyp->getCloudColor(), 1.f);
		shader->uniform4fv(LLShaderMgr::CLOUD_COLOR, cloud_color.mV);
	}

	shader = &(targetp)[LLGLSLShader::SG_ANY];
	if (!shader)
	{
		return;
	}
	shader->uniform1f(LLShaderMgr::SCENE_LIGHT_STRENGTH, mSceneLightStrength);
	LLColor4 ambient = getTotalAmbient();
	shader->uniform4fv(LLShaderMgr::AMBIENT, ambient.mV);

	shader->uniform1i(LLShaderMgr::SUN_UP_FACTOR, getIsSunUp() ? 1 : 0);
	shader->uniform1f(LLShaderMgr::SUN_MOON_GLOW_FACTOR,
					  getSunMoonGlowFactor());
	shader->uniform1f(LLShaderMgr::DENSITY_MULTIPLIER, getDensityMultiplier());
	shader->uniform1f(LLShaderMgr::DISTANCE_MULTIPLIER,
					  getDistanceMultiplier());

	shader->uniform1f(LLShaderMgr::GAMMA, getGamma());
	shader->uniform1f(LLShaderMgr::DISPLAY_GAMMA,
					  LLPipeline::RenderDeferredDisplayGamma);
}

//virtual
const LLSettingsSky::parammapping_t& LLEnvSettingsSky::getParameterMap() const
{
	static parammapping_t param_map;
	if (param_map.empty())
	{
		// LEGACY_ATMOSPHERICS

		//* TODO: default 'legacy' values duplicate the ones from functions
		// like getBlueDensity() find a better home for them. There is
		// LLSettingsSky::defaults(), but it does not contain everything since
		// it is geared towards creating new settings.
		param_map[SETTING_AMBIENT] =
			DefaultParam(LLShaderMgr::AMBIENT,
						 LLColor3(0.25f, 0.25f, 0.25f).getValue());
		param_map[SETTING_BLUE_DENSITY] =
			DefaultParam(LLShaderMgr::BLUE_DENSITY,
						 LLColor3(0.2447f, 0.4487f, 0.7599f).getValue());
		param_map[SETTING_BLUE_HORIZON] =
			DefaultParam(LLShaderMgr::BLUE_HORIZON,
						 LLColor3(0.4954f, 0.4954f, 0.6399f).getValue());
		param_map[SETTING_HAZE_DENSITY] =
			DefaultParam(LLShaderMgr::HAZE_DENSITY, LLSD(0.7f));
		param_map[SETTING_HAZE_HORIZON] =
			DefaultParam(LLShaderMgr::HAZE_HORIZON, LLSD(0.19f));
		param_map[SETTING_DENSITY_MULTIPLIER] =
			DefaultParam(LLShaderMgr::DENSITY_MULTIPLIER, LLSD(0.0001f));
		param_map[SETTING_DISTANCE_MULTIPLIER] =
			DefaultParam(LLShaderMgr::DISTANCE_MULTIPLIER, LLSD(0.8f));

		// Following values are always present, so we can just zero these ones,
		// but used values from defaults()
		LLSD sky_defaults = LLSettingsSky::defaults();

		param_map[SETTING_CLOUD_POS_DENSITY2] =
			DefaultParam(LLShaderMgr::CLOUD_POS_DENSITY2,
						 sky_defaults[SETTING_CLOUD_POS_DENSITY2]);
		param_map[SETTING_CLOUD_SCALE] =
			DefaultParam(LLShaderMgr::CLOUD_SCALE,
						 sky_defaults[SETTING_CLOUD_SCALE]);
		param_map[SETTING_CLOUD_SHADOW] =
			DefaultParam(LLShaderMgr::CLOUD_SHADOW,
						 sky_defaults[SETTING_CLOUD_SHADOW]);

		param_map[SETTING_GLOW] = DefaultParam(LLShaderMgr::GLOW,
											   sky_defaults[SETTING_GLOW]);
		param_map[SETTING_MAX_Y] = DefaultParam(LLShaderMgr::MAX_Y,
												sky_defaults[SETTING_MAX_Y]);


		param_map[SETTING_CLOUD_VARIANCE] =
			DefaultParam(LLShaderMgr::CLOUD_VARIANCE,
						 sky_defaults[SETTING_CLOUD_VARIANCE]);
		param_map[SETTING_MOON_BRIGHTNESS] =
			DefaultParam(LLShaderMgr::MOON_BRIGHTNESS,
						 sky_defaults[SETTING_MOON_BRIGHTNESS]);
		param_map[SETTING_SKY_MOISTURE_LEVEL] =
			DefaultParam(LLShaderMgr::MOISTURE_LEVEL,
						 sky_defaults[SETTING_SKY_MOISTURE_LEVEL]);
		param_map[SETTING_SKY_DROPLET_RADIUS] =
			DefaultParam(LLShaderMgr::DROPLET_RADIUS,
						 sky_defaults[SETTING_SKY_DROPLET_RADIUS]);
		param_map[SETTING_SKY_ICE_LEVEL] =
			DefaultParam(LLShaderMgr::ICE_LEVEL,
						 sky_defaults[SETTING_SKY_ICE_LEVEL]);

		// Legacy Windlight parameters used for conversion from EE to WL
		param_map[SETTING_SUNLIGHT_COLOR] =
			DefaultParam(LLShaderMgr::SUNLIGHT_COLOR,
						 sky_defaults[SETTING_SUNLIGHT_COLOR]);
		param_map[SETTING_CLOUD_COLOR] =
			DefaultParam(LLShaderMgr::CLOUD_COLOR,
						 sky_defaults[SETTING_CLOUD_COLOR]);

		// *TODO: AdvancedAtmospherics. Provide mappings for new shader params
		// here
	}
	return param_map;
}

//static
LLSettingsSky::ptr_t LLEnvSettingsSky::buildSky(LLSD settings)
{
	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Sky setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	return std::make_shared<LLEnvSettingsSky>(settings, true);
}

//static
LLSettingsSky::ptr_t LLEnvSettingsSky::buildDefaultSky()
{
	static LLSD default_settings;

	if (!default_settings.size())
	{
		default_settings = LLSettingsSky::defaults();

		default_settings[SETTING_NAME] = std::string("_default_");

		const validation_list_t& validations = validationList();
		LLSD results = settingValidation(default_settings, validations);
		if (!results["success"].asBoolean())
		{
			llwarns << "Sky setting validation failed:\n" << results << llendl;
			return ptr_t();
		}
	}

	return std::make_shared<LLEnvSettingsSky>(default_settings);
}

//static
LLSettingsSky::ptr_t LLEnvSettingsSky::buildFromLegacyPreset(const std::string& name,
															 const LLSD& old_settings,
															 LLSD& messages)
{
	LLSD new_settings = translateLegacySettings(old_settings);
	if (new_settings.isUndefined())
	{
		messages["REASONS"] = LLTrans::getString("SettingTranslateError",
												 LLSDMap("NAME", name));
		llwarns << "Sky setting translation failed:\n" << messages << llendl;
		return LLSettingsSky::ptr_t();
	}

	new_settings[SETTING_NAME] = name;

	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(new_settings, validations);
	if (!results["success"].asBoolean())
	{
		messages["REASONS"] = LLTrans::getString("SettingValidationError",
												 LLSDMap("NAME", name));
		llwarns << "Sky setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	return std::make_shared<LLEnvSettingsSky>(new_settings);
}

//static
LLSettingsSky::ptr_t LLEnvSettingsSky::buildFromLegacyPresetFile(const std::string& name,
																 const std::string& filename,
																 const std::string& path,
																 LLSD& messages)
{
	LLSD legacy_data = LLEnvSettingsBase::readLegacyPresetData(filename, path,
															   messages);
	if (!legacy_data)
	{
		// 'messages' is filled in by readLegacyPresetData
		llwarns << "Could not load legacy Windlight \"" << name << "\" from "
				<< path << llendl;
		return ptr_t();
	}

	return buildFromLegacyPreset(LLURI::unescape(name), legacy_data, messages);
}

//static
bool LLEnvSettingsSky::applyPresetByName(std::string name, bool ignore_case)
{
	if (ignore_case)
	{
		// Normally, 'name' should already be passed as a lower-cased setting
		// name when 'ignore_case' is true, but let's assume it might not be...
		LLStringUtil::toLower(name);
	}

	// Start with inventory settings, when available...

	if (LLEnvironment::isInventoryEnabled())
	{
		const LLUUID& folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS,
											   false);
		if (folder_id.notNull())
		{
			LLEnvSettingsCollector collector;
			LLInventoryModel::cat_array_t cats;
			LLInventoryModel::item_array_t items;
			gInventory.collectDescendentsIf(folder_id, cats, items, false,
											collector);
			std::string preset;
			for (LLInventoryModel::item_array_t::iterator
					iter = items.begin(), end = items.end();
				 iter != end; ++iter)
			{
				LLViewerInventoryItem* itemp = *iter;
				LLSettingsType::EType type = itemp->getSettingsType();
				if (type != LLSettingsType::ST_SKY)
				{
					continue;
				}
				preset = itemp->getName();
				if (ignore_case)
				{
					LLStringUtil::toLower(preset);
				}
				if (preset == name)
				{
					name = itemp->getName(); // Real name
					llinfos << "Using inventory settings: " << name << llendl;
					gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL,
												itemp->getAssetUUID());
					gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
														LLEnvironment::TRANSITION_INSTANT);
					if (gAutomationp)
					{
						gAutomationp->onWindlightChange(name, "", "");
					}
					return true;
				}
			}
		}
	}

	// Inventory settings failed; try Windlight settings...

	if (ignore_case)
	{
		// When ignoring the case, we must scan all usable WL settings to find
		// one which can match our lower-cased string, and then use that real
		// name to convert the corresponding WL settings.
		bool found = false;
		std::vector<std::string> presets =
			LLWLSkyParamMgr::getLoadedPresetsList();
		std::string preset;
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			preset = presets[i];
			LLStringUtil::toLower(preset);
			if (preset == name)
			{
				// Retain the real, case-sensitive name
				name = presets[i];
				found = true;
				break;
			}
		}
		if (!found)
		{
			return false;
		}
	}

	std::string filename, path;
	if (LLWLDayCycle::findPresetFile(name, "skies", "", filename, path))
	{
		LLSD msg;
		LLSettingsSky::ptr_t skyp = buildFromLegacyPresetFile(name, filename,
															  path, msg);
		if (skyp)
		{
			llinfos << "Using Windlight settings: " << name << llendl;
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp);
			gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
												LLEnvironment::TRANSITION_INSTANT);
			if (gAutomationp)
			{
				gAutomationp->onWindlightChange(name, "", "");
			}
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLEnvSettingsWater class
///////////////////////////////////////////////////////////////////////////////

LLEnvSettingsWater::LLEnvSettingsWater(const LLSD& data)
:	LLSettingsWater(data)
{
}

LLEnvSettingsWater::LLEnvSettingsWater()
:	LLSettingsWater()
{
}

//virtual
LLSettingsWater::ptr_t LLEnvSettingsWater::buildClone() const
{
	LLSD settings = cloneSettings();
	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Water setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	U32 flags = getFlags();
	ptr_t waterp = std::make_shared<LLEnvSettingsWater>(settings);
	waterp->setFlags(flags);
	return waterp;
}

//virtual
void LLEnvSettingsWater::updateSettings()
{
	LLSettingsWater::updateSettings();

	LLDrawPoolWater* poolp =
		(LLDrawPoolWater*)gPipeline.getPool(LLDrawPool::POOL_WATER);
	if (!poolp) return;

	poolp->setTransparentTextures(getTransparentTextureID(),
								  getNextTransparentTextureID());
	poolp->setNormalMaps(getNormalMapID(), getNextNormalMapID());
}

//virtual
void LLEnvSettingsWater::applySpecial(LLShaderUniforms* targetp)
{
	if (!targetp || !gAgent.getRegion())
	{
		return;
	}

	LLShaderUniforms* shader = &(targetp)[LLGLSLShader::SG_WATER];
	if (!shader)
	{
		return;
	}

	F32 water_height = gAgent.getRegion()->getWaterHeight();

	// Transform water plane to eye space
	LLVector4a norm(0.f, 0.f, 1.f);
	LLVector4a ep(0.f, 0.f, water_height + 0.1f);

	const LLMatrix4a& mat = gGLModelView;
	LLMatrix4a invtrans = mat;
	invtrans.invert();
	invtrans.transpose();

	invtrans.perspectiveTransform(norm, norm);
	norm.normalize3fast();
	mat.affineTransform(ep, ep);

	ep.setAllDot3(ep, norm);
	ep.negate();
	norm.copyComponent<3>(ep);

	shader->uniform4fv(LLShaderMgr::WATER_WATERPLANE, norm.getF32ptr());

	LLVector4 light_direction = gEnvironment.getClampedLightNorm();

	F32 fog_ks = 1.f / llmax(light_direction.mV[2], WATER_FOG_LIGHT_CLAMP);
	shader->uniform1f(LLShaderMgr::WATER_FOGKS, fog_ks);

	const LLSettingsWater::ptr_t& waterp = gEnvironment.getCurrentWater();

	bool underwater = gViewerCamera.getOrigin().mV[2] - water_height <= 0.f;
	F32 water_fog_density =
		waterp->getModifiedWaterFogDensity(underwater);
	shader->uniform1f(LLShaderMgr::WATER_FOGDENSITY, water_fog_density);

	LLColor4 fog_color(waterp->getWaterFogColor(), 0.f);
	shader->uniform4fv(LLShaderMgr::WATER_FOGCOLOR, fog_color.mV);

	F32 blend_factor = waterp->getBlendFactor();
	shader->uniform1f(LLShaderMgr::BLEND_FACTOR, blend_factor);

	// Update to normal lightnorm, water shader itself will use rotated
	// lightnorm as necessary
	shader->uniform4fv(LLShaderMgr::LIGHTNORM, light_direction.mV);
}

//virtual
const LLSettingsWater::parammapping_t& LLEnvSettingsWater::getParameterMap() const
{
	static parammapping_t param_map;
#if 0	// Disabled also in LL's viewer
	if (param_map.empty())
	{
		LLSD water_defaults = LLSettingsWater::defaults();
		param_map[SETTING_FOG_COLOR] =
			DefaultParam(LLShaderMgr::WATER_FOGCOLOR,
						 water_defaults[SETTING_FOG_COLOR]);
		// Let this get set by LLEnvSettingsWater::applySpecial so that it can
		// properly reflect the underwater modifier
		param_map[SETTING_FOG_DENSITY] =
			DefaultParam(LLShaderMgr::WATER_FOGDENSITY,
						 water_defaults[SETTING_FOG_DENSITY]);
	}
#endif
	return param_map;
}

//static
LLSettingsWater::ptr_t LLEnvSettingsWater::buildWater(LLSD settings)
{
	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Water setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	return std::make_shared<LLEnvSettingsWater>(settings);
}

//static
LLSettingsWater::ptr_t LLEnvSettingsWater::buildDefaultWater()
{
	static LLSD default_settings;
	if (!default_settings.size())
	{
		default_settings = defaults();

		default_settings[SETTING_NAME] = "_default_";

		const validation_list_t& validations = validationList();
		LLSD results = settingValidation(default_settings, validations);
		if (!results["success"].asBoolean())
		{
			llwarns << "Water setting validation failed:\n" << results
					<< llendl;
			return ptr_t();
		}
	}

	return std::make_shared<LLEnvSettingsWater>(default_settings);
}

//static
LLSettingsWater::ptr_t LLEnvSettingsWater::buildFromLegacyPreset(const std::string& name,
																 const LLSD& old_settings,
																 LLSD& messages)
{
	LLSD new_settings(translateLegacySettings(old_settings));
	if (new_settings.isUndefined())
	{
		messages["REASONS"] = LLTrans::getString("SettingTranslateError",
												 LLSDMap("NAME", name));
		llwarns << "Water setting translation failed:\n" << messages << llendl;
		return LLSettingsWater::ptr_t();
	}

	new_settings[SETTING_NAME] = name;
	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(new_settings, validations);
	if (!results["success"].asBoolean())
	{
		messages["REASONS"] = LLTrans::getString("SettingValidationError", name);
		llwarns << "Water setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	return std::make_shared<LLEnvSettingsWater>(new_settings);
}

//static
LLSettingsWater::ptr_t LLEnvSettingsWater::buildFromLegacyPresetFile(const std::string& name,
																	 const std::string& filename,
																	 const std::string& path,
																	 LLSD& messages)
{
	LLSD legacy_data = LLEnvSettingsBase::readLegacyPresetData(filename, path,
															   messages);
	if (!legacy_data)
	{
		// 'messages' is filled in by readLegacyPresetData
		llwarns << "Could not load legacy Windlight \"" << name << "\" from "
				<< path << llendl;
		return ptr_t();
	}

	return buildFromLegacyPreset(LLURI::unescape(name), legacy_data, messages);
}

//static
bool LLEnvSettingsWater::applyPresetByName(std::string name, bool ignore_case)
{
	if (ignore_case)
	{
		// Normally, 'name' should already be passed as a lower-cased setting
		// name when 'ignore_case' is true, but let's assume it might not be...
		LLStringUtil::toLower(name);
	}

	// Start with inventory settings, when available...

	if (LLEnvironment::isInventoryEnabled())
	{
		const LLUUID& folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS,
											   false);
		if (folder_id.notNull())
		{
			LLEnvSettingsCollector collector;
			LLInventoryModel::cat_array_t cats;
			LLInventoryModel::item_array_t items;
			gInventory.collectDescendentsIf(folder_id, cats, items, false,
											collector);
			std::string preset;
			for (LLInventoryModel::item_array_t::iterator
					iter = items.begin(), end = items.end();
				 iter != end; ++iter)
			{
				LLViewerInventoryItem* itemp = *iter;
				LLSettingsType::EType type = itemp->getSettingsType();
				if (type != LLSettingsType::ST_WATER)
				{
					continue;
				}
				preset = itemp->getName();
				if (ignore_case)
				{
					LLStringUtil::toLower(preset);
				}
				if (preset == name)
				{
					name = itemp->getName();	// Real name
					llinfos << "Using inventory settings: " << name << llendl;
					gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL,
												itemp->getAssetUUID());
					gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
														LLEnvironment::TRANSITION_INSTANT);
					if (gAutomationp)
					{
						gAutomationp->onWindlightChange("", name, "");
					}
					return true;
				}
			}
		}
	}

	// Inventory settings failed; try Windlight settings...

	if (ignore_case)
	{
		// When ignoring the case, we must scan all usable WL settings to find
		// one which can match our lower-cased string, and then use that real
		// name to convert the corresponding WL settings.
		bool found = false;
		std::vector<std::string> presets =
			LLWLWaterParamMgr::getLoadedPresetsList();
		std::string preset;
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			preset = presets[i];
			LLStringUtil::toLower(preset);
			if (preset == name)
			{
				// Retain the real, case-sensitive name
				name = presets[i];
				found = true;
				break;
			}
		}
		if (!found)
		{
			return false;
		}
	}

	std::string filename, path;
	if (LLWLDayCycle::findPresetFile(name, "water", "", filename, path))
	{
		LLSD msg;
		LLSettingsWater::ptr_t waterp = buildFromLegacyPresetFile(name,
																  filename,
																  path, msg);
		if (waterp)
		{
			llinfos << "Using Windlight settings: " << name << llendl;
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, waterp);
			gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
												LLEnvironment::TRANSITION_INSTANT);
			if (gAutomationp)
			{
				gAutomationp->onWindlightChange("", name, "");
			}
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLEnvSettingsDay class
///////////////////////////////////////////////////////////////////////////////

LLEnvSettingsDay::LLEnvSettingsDay(const LLSD& data)
:	LLSettingsDay(data)
{
}

LLEnvSettingsDay::LLEnvSettingsDay()
:	LLSettingsDay()
{
}

//virtual
LLSettingsDay::ptr_t LLEnvSettingsDay::buildClone() const
{
	LLSD settings = cloneSettings();

	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Day setting validation failed;\n" << results << llendl;
		return ptr_t();
	}

	ptr_t dayp = std::make_shared<LLEnvSettingsDay>(settings);

	U32 flags = getFlags();
	if (flags)
	{
		dayp->setFlags(flags);
	}
	dayp->initialize();

	return dayp;
}

//virtual
LLSettingsDay::ptr_t LLEnvSettingsDay::buildDeepCloneAndUncompress() const
{
	U32 flags = getFlags();
	// No need for SETTING_TRACKS or SETTING_FRAMES, so take base LLSD
	LLSD settings = llsd_clone(mSettings);
	ptr_t day_clone = std::make_shared<LLEnvSettingsDay>(settings);
	for (S32 i = 0; i < LLSettingsDay::TRACK_MAX; ++i)
	{
		const cycle_track_t& track = getCycleTrackConst(i);
		cycle_track_t::const_iterator iter = track.begin();
		while (iter != track.end())
		{
			// 'Unpack', usually for editing
			// - frames 'share' settings multiple times
			// - settings can reuse LLSDs they were initialized from
			// We do not want for edited frame to change multiple frames in
			// same track, so do a clone
			day_clone->setSettingsAtKeyframe(iter->second->buildDerivedClone(),
											 iter->first, i);
			++iter;
		}
	}
	day_clone->setFlags(flags);
	return day_clone;
}

//virtual
LLSettingsSky::ptr_t LLEnvSettingsDay::getDefaultSky() const
{
	return LLEnvSettingsSky::buildDefaultSky();
}

//virtual
LLSettingsWater::ptr_t LLEnvSettingsDay::getDefaultWater() const
{
	return LLEnvSettingsWater::buildDefaultWater();
}

//virtual
LLSettingsSky::ptr_t LLEnvSettingsDay::buildSky(LLSD settings) const
{
	LLSettingsSky::ptr_t skyp = std::make_shared<LLEnvSettingsSky>(settings);
	return skyp && skyp->validate() ? skyp : LLSettingsSky::ptr_t();
}

//virtual
LLSettingsWater::ptr_t LLEnvSettingsDay::buildWater(LLSD settings) const
{
	LLSettingsWater::ptr_t waterp =
		std::make_shared<LLEnvSettingsWater>(settings);
	return waterp && waterp->validate() ? waterp : LLSettingsWater::ptr_t();
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildDay(LLSD settings)
{
	const validation_list_t& validations = validationList();
	LLSD results = settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Day setting validation failed:\n" << results << llendl;
		LLSettingsDay::ptr_t();
	}

	ptr_t dayp = std::make_shared<LLEnvSettingsDay>(settings);
	if (dayp)
	{
		dayp->initialize();
	}

	return dayp;
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildDefaultDayCycle()
{
	static ptr_t default_dayp;
	if (default_dayp)
	{
		return default_dayp->buildClone();
	}
#if 1	// Default EE day cycle is borked... Use Windlight's "Default" day
		// cycle instead.
		// *TODO: fix LLSettingsDay::defaults() so that it would *actually*
		// correspond to the default Windlight day cycle...
	std::string filename, path;
	if (LLWLDayCycle::findPresetFile("Default", "days", "", filename, path))
	{
		LLSD messages;
		default_dayp = buildFromLegacyPresetFile("_default_", filename, path,
												 messages);
		if (default_dayp)
		{
			return default_dayp->buildClone();
		}
	}
	else
	{
		llwarns << "Could not find any Default.xml Windlight day cycle file."
				<< llendl;
	}
#endif
	// Fallback path...
	LLSD default_settings = LLSettingsDay::defaults();
	default_settings[SETTING_NAME] = "_default_";

	const LLSettingsDay::validation_list_t& validations =
		LLSettingsDay::validationList();
	LLSD results = LLSettingsDay::settingValidation(default_settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Day setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	default_dayp = std::make_shared<LLEnvSettingsDay>(default_settings);
	default_dayp->initialize();
	return default_dayp->buildClone();
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildFromLegacyPreset(const std::string& name,
															 const std::string& path,
															 const LLSD& old_settings,
															 LLSD& messages)
{
	LL_DEBUGS("EnvSettings") << "Loading '" << name << "' day cycle from: "
							 << path << LL_ENDL;

	LLSD skytrack = LLSD::emptyArray();
	std::set<std::string> framenames;
	for (LLSD::array_const_iterator it = old_settings.beginArray(),
									end = old_settings.endArray();
		 it != end; ++it)
	{
		std::string framename = (*it)[1].asString();
		LLSD entry = LLSDMap(SETTING_KEYKFRAME, (*it)[0].asReal())
			(SETTING_KEYNAME, "sky:" + framename);
		framenames.emplace(framename);
		skytrack.append(entry);
		LL_DEBUGS("EnvSettings") << "Added sky track: " << framename
								 << LL_ENDL;
	}

	LLSD watertrack = LLSDArray(LLSDMap(SETTING_KEYKFRAME, LLSD::Real(0.f))
							   (SETTING_KEYNAME, "water:Default"));
	LL_DEBUGS("EnvSettings") << "Added water track: Default" << LL_ENDL;
	LLSD new_settings(defaults());
	new_settings[SETTING_TRACKS] = LLSDArray(watertrack)(skytrack);

	LLSD frames(LLSD::emptyMap());

	std::string filename, actual_path;
	LLWLDayCycle::findPresetFile("Default", "water", path, filename,
								 actual_path);
	LLSettingsWater::ptr_t waterp =
		LLEnvSettingsWater::buildFromLegacyPresetFile("Default", filename,
													  actual_path, messages);
	if (!waterp)
	{
		llwarns << "Failed to load Default water." << llendl;
		// 'messages' is filled in by buildFromLegacyPresetFile
		return LLSettingsDay::ptr_t();
	}
	LL_DEBUGS("EnvSettings") << "Loaded Default water from: "
							 << actual_path << filename << LL_ENDL;
	frames["water:Default"] = waterp->getSettings();

	for (std::set<std::string>::iterator it = framenames.begin(),
										 end = framenames.end();
		 it != end; ++it)
	{
		LLWLDayCycle::findPresetFile(*it, "skies", path, filename,
									 actual_path);
		LLSettingsSky::ptr_t skyp =
			LLEnvSettingsSky::buildFromLegacyPresetFile(*it, filename,
														actual_path, messages);
		if (!skyp)
		{
			llwarns << "Failed to load sky: " << *it << llendl;
			// 'messages' is filled in by buildFromLegacyPresetFile
			return LLSettingsDay::ptr_t();
		}
		LL_DEBUGS("EnvSettings") << "Loaded '" << *it << "' sky from: "
								 << actual_path << filename << LL_ENDL;
		frames["sky:" + *it] = skyp->getSettings();
	}

	new_settings[SETTING_FRAMES] = frames;

	const LLSettingsDay::validation_list_t& validations =
		LLSettingsDay::validationList();
	LLSD results = LLSettingsDay::settingValidation(new_settings,
													validations);
	if (!results["success"].asBoolean())
	{
		messages["REASONS"] = LLTrans::getString("SettingValidationError",
												 LLSDMap("NAME", name));
		llwarns << "Day setting validation failed:\n" << results << llendl;
		return LLSettingsDay::ptr_t();
	}

	LL_DEBUGS("EnvSettings") << "Creating EE settings from LLSD: "
							 <<  ll_pretty_print_sd(new_settings) << LL_ENDL;
	LLSettingsDay::ptr_t dayp =
		std::make_shared<LLEnvSettingsDay>(new_settings);
	dayp->initialize();

	return dayp;
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildFromLegacyPresetFile(const std::string& name,
																 const std::string& filename,
																 const std::string& path,
																 LLSD& messages)
{
	LLSD legacy_data = LLEnvSettingsBase::readLegacyPresetData(filename, path,
															   messages);

	if (!legacy_data)
	{
		// 'messages' is filled in by readLegacyPresetData
		llwarns << "Could not load legacy Windlight \"" << name << "\" from "
				<< path << llendl;
		return ptr_t();
	}
	// Name for LLSettingsDay only, path to get related files from filesystem
	return buildFromLegacyPreset(LLURI::unescape(name), path, legacy_data,
								 messages);
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildFromLegacyMessage(const LLUUID& region_id,
															  LLSD daycycle,
															  LLSD skydefs,
															  LLSD waterdef)
{
	LLSD frames(LLSD::emptyMap());

	std::string newname;
	for (LLSD::map_iterator it = skydefs.beginMap(), end = skydefs.endMap();
		 it != end; ++it)
	{
		newname = "sky:" + it->first;
		LLSD new_settings = LLSettingsSky::translateLegacySettings(it->second);

		new_settings[SETTING_NAME] = newname;
		frames[newname] = new_settings;

		llwarns << "Created region sky '" << newname << "'" << llendl;
	}

	LLSD watersettings = LLSettingsWater::translateLegacySettings(waterdef);
	std::string watername = "water:"+ watersettings[SETTING_NAME].asString();
	watersettings[SETTING_NAME] = watername;
	frames[watername] = watersettings;

	LLSD watertrack =
		LLSDArray(LLSDMap(SETTING_KEYKFRAME, LLSD::Real(0.f))(SETTING_KEYNAME,
															  watername));

	LLSD skytrack(LLSD::emptyArray());
	for (LLSD::array_const_iterator it = daycycle.beginArray(),
									end = daycycle.endArray();
		 it != end; ++it)
	{
		LLSD entry =
			LLSDMap(SETTING_KEYKFRAME, (*it)[0].asReal())
				   (SETTING_KEYNAME, "sky:" + (*it)[1].asString());
		skytrack.append(entry);
	}

	LLSD new_settings =
		LLSDMap(SETTING_NAME, "Region (legacy)")
			   (SETTING_TRACKS, LLSDArray(watertrack)(skytrack))
			   (SETTING_FRAMES, frames)
			   (SETTING_TYPE, "daycycle");

	const LLSettingsSky::validation_list_t& validations =
		LLSettingsDay::validationList();
	LLSD results = LLSettingsDay::settingValidation(new_settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Day setting validation failed:\n" << results << llendl;
		return LLSettingsDay::ptr_t();
	}

	LLSettingsDay::ptr_t dayp =
		std::make_shared<LLEnvSettingsDay>(new_settings);
	if (dayp)
	{
		// true for validation; either validate here, or when cloning for
		// floater.
		dayp->initialize(true);
	}
	return dayp;
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildFromEnvironmentMessage(LLSD settings)
{
	const LLSettingsDay::validation_list_t& validations =
		LLSettingsDay::validationList();
	LLSD results = LLSettingsDay::settingValidation(settings, validations);
	if (!results["success"].asBoolean())
	{
		llwarns << "Day setting validation failed:\n" << results << llendl;
		return ptr_t();
	}

	ptr_t dayp = std::make_shared<LLEnvSettingsDay>(settings);
	dayp->initialize();

	return dayp;
}

//static
void LLEnvSettingsDay::buildFromOtherSetting(LLSettingsBase::ptr_t settings,
											 asset_built_fn cb)
{
	if (settings->getSettingsType() == "daycycle")
	{
		if (cb)
		{
			cb(std::static_pointer_cast<LLSettingsDay>(settings));
		}
		return;
	}

	LLEnvSettingsBase::getSettingsAsset(getDefaultAssetId(),
										[settings, cb](LLUUID,
													   LLSettingsBase::ptr_t dayp,
													   S32, LLExtStat)
										{
											combineIntoDayCycle(std::static_pointer_cast<LLSettingsDay>(dayp),
																settings, cb);
										});
}

//static
void LLEnvSettingsDay::combineIntoDayCycle(LLSettingsDay::ptr_t dayp,
										   LLSettingsBase::ptr_t settings,
										   asset_built_fn cb)
{
	if (settings->getSettingsType() == "sky")
	{
		dayp->setName("sky: " + settings->getName());
		dayp->clearCycleTrack(1);
		dayp->setSettingsAtKeyframe(settings, 0.0, 1);
	}
	else if (settings->getSettingsType() == "water")
	{
		dayp->setName("water: " + settings->getName());
		dayp->clearCycleTrack(0);
		dayp->setSettingsAtKeyframe(settings, 0.0, 0);
	}
	else
	{
		dayp.reset();
	}

	if (cb)
	{
		cb(dayp);
	}
}

//static
bool LLEnvSettingsDay::applyPresetByName(std::string name, bool ignore_case)
{
	if (ignore_case)
	{
		// Normally, 'name' should already be passed as a lower-cased setting
		// name when 'ignore_case' is true, but let's assume it might not be...
		LLStringUtil::toLower(name);
	}

	// Start with inventory settings, when available...

	if (LLEnvironment::isInventoryEnabled())
	{
		const LLUUID& folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS,
											   false);
		if (folder_id.notNull())
		{
			LLEnvSettingsCollector collector;
			LLInventoryModel::cat_array_t cats;
			LLInventoryModel::item_array_t items;
			gInventory.collectDescendentsIf(folder_id, cats, items, false,
											collector);
			std::string preset;
			for (LLInventoryModel::item_array_t::iterator
					iter = items.begin(), end = items.end();
				 iter != end; ++iter)
			{
				LLViewerInventoryItem* itemp = *iter;
				LLSettingsType::EType type = itemp->getSettingsType();
				if (type != LLSettingsType::ST_DAYCYCLE)
				{
					continue;
				}
				preset = itemp->getName();
				if (ignore_case)
				{
					LLStringUtil::toLower(preset);
				}
				if (preset == name)
				{
					name = itemp->getName(); // Real name
					llinfos << "Using inventory settings: " << name << llendl;
					gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL,
												itemp->getAssetUUID());
					gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
														LLEnvironment::TRANSITION_INSTANT);
					if (gAutomationp)
					{
						gAutomationp->onWindlightChange("", "", name);
					}
					return true;
				}
			}
		}
	}

	// Inventory settings failed; try Windlight settings...

	if (ignore_case)
	{
		// When ignoring the case, we must scan all usable WL settings to find
		// one which can match our lower-cased string, and then use that real
		// name to convert the corresponding WL settings.
		bool found = false;
		std::vector<std::string> presets =
			LLWLDayCycle::getLoadedPresetsList();
		std::string preset;
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			preset = presets[i];
			LLStringUtil::toLower(preset);
			if (preset == name)
			{
				// Retain the real, case-sensitive name
				name = presets[i];
				found = true;
				break;
			}
		}
		if (!found)
		{
			return false;
		}
	}

	std::string filename, path;
	if (LLWLDayCycle::findPresetFile(name, "days", "", filename, path))
	{
		LLSD msg;
		LLSettingsDay::ptr_t dayp = buildFromLegacyPresetFile(name, filename,
															  path, msg);
		if (dayp)
		{
			llinfos << "Using Windlight settings: " << name << llendl;
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, dayp);
			gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
												LLEnvironment::TRANSITION_INSTANT);
			if (gAutomationp)
			{
				gAutomationp->onWindlightChange("", "", name);
			}
			return true;
		}
	}

	return false;
}
