/**
 * @file llsettingstype.h
 * @brief LLSettingsType class header file
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

#ifndef LL_SETTINGS_TYPE_H
#define LL_SETTINGS_TYPE_H

#include "llinventorytype.h"
#include "llinvtranslationbrdg.h"

// Purely static class
class LLSettingsType
{
	LLSettingsType() = delete;
	~LLSettingsType() = delete;

public:
	static void initClass(LLTranslationBridge::ptr_t& trans);
	static void cleanupClass();

	enum EType
	{
		ST_SKY = 0,
		ST_WATER = 1,
		ST_DAYCYCLE = 2,

		ST_INVALID = 255,
		ST_NONE = -1
	};

	static EType fromInventoryFlags(U32 flags);
	static LLInventoryType::EIconName getIconName(EType type);
	static std::string getDefaultName(EType type);
};

#endif // LL_SETTINGS_TYPE_H
