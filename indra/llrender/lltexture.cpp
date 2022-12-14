/** 
 * @file lltexture.cpp
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2010, Linden Research, Inc.
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
#include "lltexture.h"

//virtual 
LLTexture::~LLTexture()
{
}
S8   LLTexture::getType() const { llassert(false); return 0; }
void LLTexture::setKnownDrawSize(S32 width, S32 height) { llassert(false); }
bool LLTexture::bindDefaultImage(const S32 stage) { llassert(false); return false; }
bool LLTexture::bindDebugImage(const S32 stage) { llassert(false); return false; }
void LLTexture::forceImmediateUpdate() { llassert(false); }
void LLTexture::setActive() { llassert(false);  }
S32	 LLTexture::getWidth(S32 discard_level) const { llassert(false); return 0; }
S32	 LLTexture::getHeight(S32 discard_level) const { llassert(false); return 0; }
bool LLTexture::isActiveFetching() { llassert(false); return false; }
LLImageGL* LLTexture::getGLTexture() const { llassert(false); return nullptr; }
void LLTexture::updateBindStatsForTester() { }