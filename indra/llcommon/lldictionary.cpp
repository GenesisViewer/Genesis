/** 
 * @file lldictionary.cpp
 * @brief Lldictionary class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 */

#include "linden_common.h"

#include "lldictionary.h"

#include "llstring.h"

// Define in .cpp file to prevent header include of llstring.h
LLDictionaryEntry::LLDictionaryEntry(const std::string &name)
:	mName(name)
{
	mNameCapitalized = mName;
	LLStringUtil::replaceChar(mNameCapitalized, '-', ' ');
	LLStringUtil::replaceChar(mNameCapitalized, '_', ' ');
	for (U32 i=0; i < mNameCapitalized.size(); i++)
	{
		if (i == 0 || mNameCapitalized[i-1] == ' ') // don't change ordering of this statement or crash
		{
			mNameCapitalized[i] = toupper(mNameCapitalized[i]);
		}
	}
}
