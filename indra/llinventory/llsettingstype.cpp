/**
 * @file llsettingstype.cpp
 * @brief LLSettingsType class implementation
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2001-2019, Linden Research, Inc.
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

#include "linden_common.h"

#include "llsettingstype.h"

#include "lldictionary.h"
#include "llinventory.h"


static LLTranslationBridge::ptr_t sTrans;

void LLSettingsType::initClass(LLTranslationBridge::ptr_t& trans)
{
	sTrans = trans;
}

void LLSettingsType::cleanupClass()
{
	sTrans.reset();
}

struct SettingsEntry : public LLDictionaryEntry
{
	SettingsEntry(const std::string& name, const std::string& default_new_name,
				  LLInventoryType::EIconName icon_name)
	:	LLDictionaryEntry(name),
		mDefaultNewName(default_new_name),
		mIconName(icon_name)
	{
		if (sTrans)
		{
			mLabel = sTrans->getString(name);
		}
		else
		{
			llwarns << "No translation bridge: SettingsEntry '" << name
					<< "' will be left untranslated." << llendl;
		}
		if (mLabel.empty())
		{
			mLabel = name;
		}
	}

	LLInventoryType::EIconName	mIconName;
	std::string					mLabel;
	std::string					mDefaultNewName;
};

class LLSettingsDictionary : public LLDictionary<LLSettingsType::EType, SettingsEntry>
{
public:
	LLSettingsDictionary();
};

// Since it is a small structure, let's initialize it unconditionally (i.e.
// even if we do not log in) at global scope. This saves having to bother with
// a costly LLSingleton (slow, lot's of CPU cycles and cache lines wasted) or
// to find the right place where to construct the class on login... HB
LLSettingsDictionary gSettingsDictionary;

LLSettingsDictionary::LLSettingsDictionary()
{
	addEntry(LLSettingsType::ST_SKY,      new SettingsEntry("sky",     "New Sky",      LLInventoryType::ICONNAME_SETTINGS_SKY));
	addEntry(LLSettingsType::ST_WATER,    new SettingsEntry("water",   "New Water",    LLInventoryType::ICONNAME_SETTINGS_WATER));
	addEntry(LLSettingsType::ST_DAYCYCLE, new SettingsEntry("day",     "New Daycycle", LLInventoryType::ICONNAME_SETTINGS_DAY));
	addEntry(LLSettingsType::ST_NONE,     new SettingsEntry("none",    "New Settings", LLInventoryType::ICONNAME_SETTINGS));
	addEntry(LLSettingsType::ST_INVALID,  new SettingsEntry("invalid", "New Settings", LLInventoryType::ICONNAME_SETTINGS));
}

LLSettingsType::EType LLSettingsType::fromInventoryFlags(U32 flags)
{
	//return (LLSettingsType::EType)(flags & LLInventoryItem::II_FLAGS_SUBTYPE_MASK);
	return (LLSettingsType::EType)(flags);
}

LLInventoryType::EIconName LLSettingsType::getIconName(LLSettingsType::EType type)
{
	const SettingsEntry* entry = gSettingsDictionary.lookup(type);
	return entry ? entry->mIconName : getIconName(ST_INVALID);
}

std::string LLSettingsType::getDefaultName(LLSettingsType::EType type)
{
	const SettingsEntry* entry = gSettingsDictionary.lookup(type);
	return entry ? entry->mDefaultNewName : getDefaultName(ST_INVALID);
}
