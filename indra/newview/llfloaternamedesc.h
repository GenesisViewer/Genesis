/** 
 * @file llfloaternamedesc.h
 * @brief LLFloaterNameDesc class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
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

#ifndef LL_LLFLOATERNAMEDESC_H
#define LL_LLFLOATERNAMEDESC_H

#include "llfloater.h"
#include "llresizehandle.h"
#include "llstring.h"

class LLLineEditor;
class LLButton;
class LLRadioGroup;

class LLFloaterNameDesc : public LLFloater
{
public:
	// <edit>
	LLFloaterNameDesc(const LLSD& filename, void* item = NULL);
	// </edit>
	virtual ~LLFloaterNameDesc();
	virtual BOOL postBuild();

	void		onBtnOK();
	void		onBtnCancel();
	void		doCommit();

    S32         getExpectedUploadCost() const;

protected:
	virtual void		onCommit();

protected:
	BOOL        mIsAudio;
	bool		mIsText;

	std::string		mFilenameAndPath;
	std::string		mFilename;
	// <edit>
	void* mItem;
	// </edit>
};

class LLFloaterSoundPreview : public LLFloaterNameDesc
{
public:
	LLFloaterSoundPreview(const LLSD& filename, void* item = NULL );
	virtual BOOL postBuild();
};

class LLFloaterAnimPreview : public LLFloaterNameDesc
{
public:
	LLFloaterAnimPreview(const LLSD& filename, void* item = NULL );
	virtual BOOL postBuild();
};

class LLFloaterScriptPreview : public LLFloaterNameDesc
{
public:
	LLFloaterScriptPreview(const LLSD& filename, void* item = NULL );
	virtual BOOL postBuild();
};

#endif  // LL_LLFLOATERNAMEDESC_H
