#include "llviewerprecompiledheaders.h"
#include "llagent.h"
#include "llsdutil.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llviewerinventory.h"
#include "stdtypes.h"
#include "llsky.h"
#include "llglslshader.h"
#include "llshadermgr.h"
#include "llviewercamera.h"
#include "pipeline.h"
#include "lltrans.h"
#include "llinventorymodel.h"
#include "llinventoryfunctions.h"
#include "llwaterparammanager.h"
#include "llwlskyparammgr.h"
#include "lldrawpoolwater.h"
#include "llviewerregion.h"


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
	
    gAssetStorage->getAssetData(asset_id,
									LLAssetType::AT_SETTINGS,
									onAssetDownloadComplete,
									(void*)storage);  
                                                                      
                           
}
//static
void LLEnvSettingsBase::onAssetDownloadComplete(
    LLVFS *vfs,
	const LLUUID& uuid,
	LLAssetType::EType type,
	void* user_data,
	S32 status, 
	LLExtStat ext_status )
{
	if (!user_data) return;	// Paranoia
	CallbackPtrStorage* storage = (CallbackPtrStorage*)user_data;
	asset_download_fn callback = storage->callback;
	delete storage;

	LLSettingsBase::ptr_t settings;
	if (!status)
	{
		
		
		LLVFile file(vfs, uuid, type, LLVFile::READ);
		S32 file_length = file.getSize();

		char* buffer = new char[file_length+1];
		file.read((U8*)buffer, file_length);		/*Flawfinder: ignore*/

		// put a EOS at the end
		buffer[file_length] = 0;
		std::string s = LLStringExplicit(&buffer[0]);
        LL_INFOS()<< "LLEnvironment::processGetAssetReply "<< s <<LL_ENDL;;

		std::stringstream llsdstream(&buffer[0]);
		LLSD llsdsettings;

		if (LLSDSerialize::deserialize(llsdsettings, llsdstream, -1))
		{
			
			settings = createFromLLSD(llsdsettings);
		}

		if (!settings)
		{
			status = 1;
			llwarns << "Unable to create settings object for asset: "
					<< uuid << llendl;
		}
		else
		{
			settings->setAssetId(uuid);
		}
	}
	else
	{
		llwarns << "Error retrieving asset: " << uuid << ". Status code="
				<< status << " (" << LLAssetStorage::getErrorString(status)
				<< ") ext_status=" << (U32)ext_status << llendl;
	}
	if (callback)
	{
		callback(uuid, settings, status, ext_status);
	}
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

	validation_list_t& validations = validationList();
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
						   LLViewerCamera::getInstance()->getOrigin());
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

LLSettingsSky::parammapping_t LLEnvSettingsSky::getParameterMap() const
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
	validation_list_t& validations = validationList();
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

		validation_list_t& validations = validationList();
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

	validation_list_t& validations = validationList();
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
				LLSettingsType::type_e type = itemp->getSettingsType();
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


                    //genesis comment                                                        
					//if (gAutomationp)
					//{
					//	gAutomationp->onWindlightChange(name, "", "");
					//}
                    //genesis comment
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
    //genesis comment
    //local preset?
	/**if (LLWLDayCycle::findPresetFile(name, "skies", "", filename, path))
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
			//genesis comment
            //if (gAutomationp)
			//{
			//	gAutomationp->onWindlightChange(name, "", "");
			//}
			return true;
		}
	}**/
     //genesis comment
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
	validation_list_t& validations = validationList();
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

	bool underwater = LLViewerCamera::getInstance()->getOrigin().mV[2] - water_height <= 0.f;
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
LLSettingsWater::parammapping_t LLEnvSettingsWater::getParameterMap() const
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
	validation_list_t& validations = validationList();
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

		validation_list_t& validations = validationList();
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
	validation_list_t& validations = validationList();
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
				LLSettingsType::type_e type = itemp->getSettingsType();
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
                    //genesis comment                                    
					//if (gAutomationp)
					//{
					//	gAutomationp->onWindlightChange("", name, "");
					//}
                    //genesis comment
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
			LLWaterParamManager::getLoadedPresetsList();
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
    //genesis comment
    //local environment?
	/**if (LLWLDayCycle::findPresetFile(name, "water", "", filename, path))
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

            //genesis comment                                                
			//if (gAutomationp)
			//{
			//	gAutomationp->onWindlightChange("", name, "");
			//}
            //genesis comment
			return true;
		}
	}**/

    //genesis comment
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

	validation_list_t& validations = validationList();
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
LLSettingsSky::ptr_t LLEnvSettingsDay::buildSky(const LLSD& settings) const
{
	LLSettingsSky::ptr_t skyp = std::make_shared<LLEnvSettingsSky>(settings);
	return skyp && skyp->validate() ? skyp : LLSettingsSky::ptr_t();
}

//virtual
LLSettingsWater::ptr_t LLEnvSettingsDay::buildWater(const LLSD& settings) const
{
	LLSettingsWater::ptr_t waterp =
		std::make_shared<LLEnvSettingsWater>(settings);
	return waterp && waterp->validate() ? waterp : LLSettingsWater::ptr_t();
}

//static
LLSettingsDay::ptr_t LLEnvSettingsDay::buildDay(LLSD settings)
{
	validation_list_t& validations = validationList();
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
    //Genesis comment
    //local preset
	/**if (LLWLDayCycle::findPresetFile("Default", "days", "", filename, path))
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
    **/
    //genesis comment
#endif
	// Fallback path...
	LLSD default_settings = LLSettingsDay::defaults();
	default_settings[SETTING_NAME] = "_default_";

	LLSettingsDay::validation_list_t& validations =
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
	//genesis comment 
    LL_INFOS() << "unimplemented method LLEnvSettingsDay::buildFromLegacyPreset" << LL_ENDL;
	LLSettingsDay::ptr_t dayp ;
	

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

	LLSettingsSky::validation_list_t& validations =
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
	LLSettingsDay::validation_list_t& validations =
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

	LLEnvSettingsBase::getSettingsAsset(GetDefaultAssetId(),
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
				LLSettingsType::type_e type = itemp->getSettingsType();
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
                    //genesis comment                                    
					//if (gAutomationp)
					//{
					//	gAutomationp->onWindlightChange("", "", name);
					//}
                    //genesis comment
					return true;
				}
			}
		}
	}

	// Inventory settings failed; try Windlight settings...
    //genesis comment
    //do we want that?
    //genesis comment
	
	return false;
}
