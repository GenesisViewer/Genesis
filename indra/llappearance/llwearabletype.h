/** 
 * @file llwearabletype.h
 * @brief LLWearableType class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLWEARABLETYPE_H
#define LL_LLWEARABLETYPE_H

#include "llassettype.h"
#include "lldictionary.h"
#include "llinventorytype.h"
#include "llsingleton.h"
#include "llinvtranslationbrdg.h"

class LLWearableType
{
public: 
	enum EType
	{
		WT_SHAPE	  = 0,
		WT_SKIN		  = 1,
		WT_HAIR		  = 2,
		WT_EYES		  = 3,
		WT_SHIRT	  = 4,
		WT_PANTS	  = 5,
		WT_SHOES	  = 6,
		WT_SOCKS	  = 7,
		WT_JACKET	  = 8,
		WT_GLOVES	  = 9,
		WT_UNDERSHIRT = 10,
		WT_UNDERPANTS = 11,
		WT_SKIRT	  = 12,
		WT_ALPHA	  = 13,
		WT_TATTOO	  = 14,
		WT_PHYSICS	  = 15,
		WT_UNIVERSAL  = 16,
		WT_UNKNOWN	  = 17,	// Singu note: used for corrupt wearables that do not have their type set in the inventory database.
							// While all the above values are serialized and stored in the database, this value is local only:
							// When a new item with value 17 is added by upstream, just increase this value to 18 (and WT_COUNT to 19).
							// Keep WT_UNKNOWN and WT_COUNT in sync with llinventory.cpp
		WT_COUNT	  = 18,

		WT_INVALID	  = 255,
		WT_NONE		  = -1,
	};

	static void			initClass(LLTranslationBridge::ptr_t &trans); // initializes static members
	static void			cleanupClass(); // initializes static members

	static const std::string& 			getTypeName(EType type);
	static const std::string& 			getTypeDefaultNewName(EType type);
	static const std::string& 			getTypeLabel(EType type);
	static LLAssetType::EType 			getAssetType(EType type);
	static EType 						typeNameToType(const std::string& type_name);
	static LLInventoryType::EIconName 	getIconName(EType type);
	static BOOL 						getDisableCameraSwitch(EType type);
	static BOOL 						getAllowMultiwear(EType type);
    static EType						inventoryFlagsToWearableType(U32 flags);

protected:
	LLWearableType() {}
	~LLWearableType() {}
};

#endif  // LL_LLWEARABLETYPE_H
