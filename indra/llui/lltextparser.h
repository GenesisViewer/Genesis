/** 
 * @file llTextParser.h
 * @brief GUI for user-defined highlights
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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
 *
 */

#ifndef LL_LLTEXTPARSER_H
#define LL_LLTEXTPARSER_H

#include "llsd.h"
#include "llsingleton.h"

class LLUUID;
class LLVector3d;
class LLColor4;

class LLTextParser : public LLSingleton<LLTextParser>
{
public:
	typedef enum e_condition_type { CONTAINS, MATCHES, STARTS_WITH, ENDS_WITH } EConditionType;
	typedef enum e_highlight_type { PART, ALL } EHighlightType;
	typedef enum e_highlight_position { WHOLE, START, MIDDLE, END } EHighlightPosition;
	typedef enum e_dialog_action { ACTION_NONE, ACTION_CLOSE, ACTION_ADD, ACTION_COPY, ACTION_UPDATE } EDialogAction;

	LLTextParser();

	LLSD parsePartialLineHighlights(const std::string &text,const LLColor4 &color, EHighlightPosition part=WHOLE, S32 index=0);
	bool parseFullLineHighlights(const std::string &text, LLColor4 *color);

	S32  findPattern(const std::string &text, LLSD highlight);
private:
	std::string getFileName();
	void loadKeywords();
	bool saveToDisk(LLSD highlights);

public:
	LLSD	mHighlights;
	bool	mLoaded;
};

#endif
