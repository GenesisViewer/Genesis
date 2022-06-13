/** 
 * @file llformat.h
 * @date   January 2007
 * @brief string formatting utility
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLFORMAT_H
#define LL_LLFORMAT_H

#if defined(LL_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"
#endif
#include <fmt/format.h>
#include <fmt/printf.h>
#if defined(LL_CLANG)
#pragma clang diagnostic pop
#endif

// Use as follows:
// LL_INFOS() << llformat("Test:%d (%.2f %.2f)", idx, x, y) << LL_ENDL;
//
// *NOTE: buffer limited to 1024, (but vsnprintf prevents overrun)
// should perhaps be replaced with boost::format.

std::string LL_COMMON_API llformat(const char *fmt, ...);

// the same version as above but ensures that returned string is in utf8 on windows
// to enable correct converting utf8_to_wstring.
std::string LL_COMMON_API llformat_to_utf8(const char *fmt, ...);
#endif // LL_LLFORMAT_H
