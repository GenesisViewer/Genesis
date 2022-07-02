/**
 * @file llframetimer.cpp
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
 * COMPLETENESS OR PERFORMANCE.a
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llcommonmath.h"

#include "llframetimer.h"

// Static members
U64 LLFrameTimer::sStartTotalTime = LLTimer::totalTime();
F64 LLFrameTimer::sFrameTime = 0.0;
U64 LLFrameTimer::sTotalTime = 0;
F64 LLFrameTimer::sTotalSeconds = 0.0;
S32 LLFrameTimer::sFrameCount = 0;
U64 LLFrameTimer::sFrameDeltaTime = 0;

constexpr F64 USEC_TO_SEC_F64 = 0.000001;

//static
void LLFrameTimer::updateFrameTime()
{
	U64 total_time = LLTimer::totalTime();
	if (total_time < sTotalTime)
	{
		llwarns << "Clock went backwards. Adjusting start time accordingly."
				<< llendl;
		sFrameDeltaTime = 0;
		// Let's approximate the adjusted start time +/- last frame time
		sStartTotalTime += total_time;
		sStartTotalTime -= sTotalTime;
	}
	else
	{
		sFrameDeltaTime = total_time - sTotalTime;
	}
	sTotalTime = total_time;
	sTotalSeconds = U64_to_F64(sTotalTime) * USEC_TO_SEC_F64;
	sFrameTime = U64_to_F64(sTotalTime - sStartTotalTime) * USEC_TO_SEC_F64;
}

void LLFrameTimer::setExpiryAt(F64 seconds_since_epoch)
{
	mStartTime = sFrameTime;
	mExpiry = seconds_since_epoch - (USEC_TO_SEC_F64 * sStartTotalTime);
}

F64 LLFrameTimer::expiresAt() const
{
	F64 expires_at = U64_to_F64(sStartTotalTime) * USEC_TO_SEC_F64;
	expires_at += mExpiry;
	return expires_at;
}

bool LLFrameTimer::checkExpirationAndReset(F32 expiration)
{
	if (hasExpired())
	{
		reset();
		setTimerExpirySec(expiration);
		return true;
	}
	return false;
}

//static
F32 LLFrameTimer::getFrameDeltaTimeF32()
{
	return (F32)(U64_to_F64(sFrameDeltaTime) * USEC_TO_SEC_F64);
}

// Return seconds since the current frame started
//static
F32 LLFrameTimer::getCurrentFrameTime()
{
	U64 frame_time = LLTimer::totalTime() - sTotalTime;
	return (F32)(U64_to_F64(frame_time) * USEC_TO_SEC_F64);
}

// Glue code to avoid full class .h file #includes
F32 getCurrentFrameTime()
{
	return (F32)(LLFrameTimer::getCurrentFrameTime());
}
