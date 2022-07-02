/**
 * @file llenvsettings.h
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

#ifndef LL_ENVSETTINGS_H
#define LL_ENVSETTINGS_H

#include "llglslshader.h"
#include "llextendedstatus.h"
#include "llsdserialize.h"
#include "llsettingsdaycycle.h"
#include "llsettingssky.h"
#include "llsettingswater.h"

class LLInventoryItem;

class LLEnvSettingsBase : public LLSettingsBase
{
	friend class LLSettingsInventoryCB;

protected:
	LOG_CLASS(LLEnvSettingsBase);

public:
	typedef boost::function<void(LLUUID asset_id,
								 LLSettingsBase::ptr_t settings, S32 status,
								 LLExtStat extstat)> asset_download_fn;
	typedef boost::function<void(LLUUID asset_id, LLUUID inventory_id,
								 LLUUID object_id,
								 LLSD results)> inv_result_fn;

	static void createNewInventoryItem(LLSettingsType::type_e stype,
									   const LLUUID& parent_id,
									   inv_result_fn cb = inv_result_fn());
	static void createInventoryItem(const LLSettingsBase::ptr_t& settings,
									const LLUUID& parent_id,
									const std::string& settings_name,
									inv_result_fn cb = inv_result_fn());
	static void createInventoryItem(const LLSettingsBase::ptr_t& settings,
									U32 next_owner_perm,
									const LLUUID& parent_id,
									std::string settings_name,
									inv_result_fn cb = inv_result_fn());

	static void updateInventoryItem(const LLSettingsBase::ptr_t& settings,
									const LLUUID& inv_item_id,
									inv_result_fn cb = inv_result_fn(),
									bool update_name = true);
	static void updateInventoryItem(const LLSettingsBase::ptr_t& settings,
									const LLUUID& object_id,
									const LLUUID& inv_item_id,
									inv_result_fn callback = inv_result_fn());

	static void getSettingsAsset(const LLUUID& asset_id, asset_download_fn cb);

	static bool exportFile(const LLSettingsBase::ptr_t& settings,
						   const std::string& filename,
						   LLSDSerialize::ELLSD_Serialize format =
							LLSDSerialize::LLSD_NOTATION);
	static LLSettingsBase::ptr_t importFile(const std::string& filename);
	static LLSettingsBase::ptr_t createFromLLSD(const LLSD& settings);

	static LLSD readLegacyPresetData(std::string filename, std::string path,
									 LLSD& messages);

private:
	LLEnvSettingsBase()							{}

	struct SettingsSaveData
	{
		typedef std::shared_ptr<SettingsSaveData> ptr_t;
		LLSettingsBase::ptr_t	mSettings;
		LLTransactionID			mTransId;
		std::string				mType;
		std::string				mTempFile;
	};

	static void onInventoryItemCreated(const LLUUID& inv_id,
									   LLSettingsBase::ptr_t settings,
									   inv_result_fn cb);

	static void onAgentAssetUploadComplete(LLUUID item_id, LLUUID new_asset_id,
										   LLUUID new_item_id, LLSD response,
										   LLSettingsBase::ptr_t psettings,
										   inv_result_fn cb);
	static void onTaskAssetUploadComplete(LLUUID item_id, LLUUID task_id,
										  LLUUID new_asset_id, LLSD response,
										  LLSettingsBase::ptr_t psettings,
										  inv_result_fn cb);

	static void onAssetDownloadComplete(const LLUUID& asset_id,
										LLAssetType::EType, void* user_data,
										S32 status, LLExtStat ext_status);
};

class LLEnvSettingsSky : public LLSettingsSky
{
protected:
	LOG_CLASS(LLEnvSettingsSky);

	LLEnvSettingsSky();

	void updateSettings() override;

	void applySpecial(LLShaderUniforms* targetp) override;

	virtual parammapping_t getParameterMap() const const override;

public:
	LLEnvSettingsSky(const LLSD& data, bool advanced = false);

	ptr_t buildClone() const override;

	inline bool isAdvanced() const			{ return  mIsAdvanced; }

	static ptr_t buildSky(LLSD settings);

	static ptr_t buildFromLegacyPreset(const std::string& name,
									   const LLSD& old_settings,
									   LLSD& messages);
	static ptr_t buildDefaultSky();

	static ptr_t buildFromLegacyPresetFile(const std::string& name,
										   const std::string& filename,
										   const std::string& path,
										   LLSD& messages);

	static bool applyPresetByName(std::string name, bool ignore_case = false);

protected:
	F32		mSceneLightStrength;
	bool	mIsAdvanced;
};

class LLEnvSettingsWater : public LLSettingsWater
{
protected:
	LOG_CLASS(LLEnvSettingsWater);

	LLEnvSettingsWater();

	void updateSettings() override;
	void applySpecial(LLShaderUniforms* targetp) override;

	virtual parammapping_t getParameterMap() const override;

public:
	LLEnvSettingsWater(const LLSD& data);

	ptr_t buildClone() const override;

	static ptr_t buildWater(LLSD settings);
	static ptr_t buildFromLegacyPreset(const std::string& name,
									   const LLSD& old_settings,
									   LLSD& messages);
	static ptr_t buildDefaultWater();
	static ptr_t buildFromLegacyPresetFile(const std::string& name,
										   const std::string& filename,
										   const std::string& path,
										   LLSD& messages);
	static bool applyPresetByName(std::string name, bool ignore_case = false);
};

class LLEnvSettingsDay : public LLSettingsDay
{
protected:
	LOG_CLASS(LLEnvSettingsDay);

	LLEnvSettingsDay();

public:
	typedef boost::function<void(LLSettingsDay::ptr_t day)> asset_built_fn;

	LLEnvSettingsDay(const LLSD& data);

	ptr_t buildClone() const override;
	ptr_t buildDeepCloneAndUncompress() const override;

	LLSettingsSky::ptr_t getDefaultSky() const override;
	LLSettingsWater::ptr_t getDefaultWater() const override;
	LLSettingsSky::ptr_t buildSky(LLSD settings) const override;
	LLSettingsWater::ptr_t buildWater(LLSD settings) const override;

	static ptr_t buildDay(LLSD settings);
	static ptr_t buildFromLegacyPreset(const std::string& name,
									   const std::string& path,
									   const LLSD& old_settings,
									   LLSD& messages);
	static ptr_t buildFromLegacyPresetFile(const std::string& name,
										   const std::string& filename,
										   const std::string& path,
										   LLSD& messages);
	static ptr_t buildFromLegacyMessage(const LLUUID& region_id,
										LLSD daycycle, LLSD skys, LLSD water);
	static ptr_t buildDefaultDayCycle();
	static ptr_t buildFromEnvironmentMessage(LLSD settings);
	static void buildFromOtherSetting(LLSettingsBase::ptr_t settings,
									  asset_built_fn cb);
	static bool applyPresetByName(std::string name, bool ignore_case = false);

private:
	static void combineIntoDayCycle(LLSettingsDay::ptr_t day,
									LLSettingsBase::ptr_t settings,
									asset_built_fn cb);
};

#endif	// LL_ENVSETTINGS_H
