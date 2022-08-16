#ifndef LL_LLENVSETTINGS_H
#define LL_LLENVSETTINGS_H
#include "llsettingsbase.h"
#include "llsettingsdaycycle.h"
#include "llsettingssky.h"
#include "llsettingswater.h"
#include "llassetstorage.h"
class LLEnvSettingsBase : public LLSettingsBase
{
public:
    typedef boost::function<void(LLUUID asset_id,
								 LLSettingsBase::ptr_t settings, S32 status,
								 LLExtStat extstat)> asset_download_fn;
    static LLSettingsBase::ptr_t createFromLLSD(const LLSD& settings);
    static LLSD readLegacyPresetData(std::string filename, std::string path,
									 LLSD& messages);
    static void getSettingsAsset(const LLUUID& asset_id, asset_download_fn cb);
private:
    static void onAssetDownloadComplete(LLVFS *vfs,
                                        const LLUUID& uuid,
                                        LLAssetType::EType type,
                                        void* user_data,
                                        S32 status, 
                                        LLExtStat ext_status);                                         
};
class LLEnvSettingsSky : public LLSettingsSky
{
protected:
	LOG_CLASS(LLEnvSettingsSky);

	LLEnvSettingsSky();

	void updateSettings() override;

	void applySpecial(LLShaderUniforms* targetp) override;

	parammapping_t getParameterMap() const override;
	

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

	parammapping_t getParameterMap() const override;

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
#endif LL_LLENVSETTINGS_H