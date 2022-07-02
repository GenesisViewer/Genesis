/**
 * @file llframetimer.h
 * @brief A lightweight timer that measures seconds and is only
 * updated once per frame.
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
 */

#ifndef LL_LLFRAMETIMER_H
#define LL_LLFRAMETIMER_H

#include "lltimer.h"

class LLFrameTimer
{
	LOG_CLASS(LLFrameTimer);

public:
	LLFrameTimer()
	:	mStartTime(sFrameTime),
		mExpiry(0),
		mStarted(true)
	{
	}

	// Returns the number of seconds since the start of this application
	// instance.
	LL_INLINE static F64 getElapsedSeconds()
	{
		// Loses msec precision after ~4.5 hours...
		return sFrameTime;
	}

	// Returns a low precision usec since epoch
	LL_INLINE static U64 getTotalTime()
	{
		return sTotalTime ? sTotalTime : LLTimer::totalTime();
	}

	// Returns a low precision seconds since epoch
	LL_INLINE static F64 getTotalSeconds()
	{
		return sTotalSeconds;
	}

	// Call this method once per frame to update the current frame time. This
	// is actually called at some other times as well
	static void updateFrameTime();

	// Call this method once, and only once, per frame to update the current
	// frame count.
	LL_INLINE static void updateFrameCount()	{ ++sFrameCount; }

	LL_INLINE static U32 getFrameCount()		{ return sFrameCount; }

	static F32 getFrameDeltaTimeF32();

	// Return seconds since the current frame started
	static F32 getCurrentFrameTime();

	LL_INLINE void start()
	{
		reset();
		mStarted = true;
	}

	LL_INLINE void stop()						{ mStarted = false; }

	LL_INLINE void reset()
	{
		mStartTime = sFrameTime;
		mExpiry = sFrameTime;
	}

	// Do not combine pause/unpause with start/stop. Usage:
	//  LLFrameTime foo;	(starts automatically)
	//  foo.unpause();		(noop but safe)
	//  foo.pause();		(pauses timer)
	//  foo.unpause();		(unpauses)
	//  F32 elapsed = foo.getElapsedTimeF32();	(does not include time between
	//											 pause() and unpause())
	//  Note: elapsed would also be valid with no unpause() call (= time run
	//        until pause() called)

	LL_INLINE void pause()
	{
		if (mStarted)
		{
			mStartTime = sFrameTime - mStartTime;
			mStarted = false;
		}
	}

	LL_INLINE void unpause()
	{
		if (!mStarted)
		{
			mStartTime = sFrameTime - mStartTime;
			mStarted = true;
		}
	}

	LL_INLINE void setTimerExpirySec(F32 delay)	{ mExpiry = delay + mStartTime; }

	LL_INLINE void resetWithExpiry(F32 delay)
	{
		reset();
		mExpiry = delay + mStartTime;
	}

	void setExpiryAt(F64 seconds_since_epoch);
	bool checkExpirationAndReset(F32 expiration);

	LL_INLINE F32 getElapsedTimeAndResetF32()
 	{
		F32 t = F32(sFrameTime - mStartTime);
		reset();
		return t;
	}

	LL_INLINE void setAge(F64 age)				{ mStartTime = sFrameTime - age; }

	LL_INLINE bool hasExpired() const			{ return sFrameTime >= mExpiry; }
	LL_INLINE F32 getTimeToExpireF32() const	{ return (F32)(mExpiry - sFrameTime); }
	LL_INLINE F32 getElapsedTimeF32() const		{ return mStarted ? (F32)(sFrameTime - mStartTime) : (F32)mStartTime; }
	LL_INLINE bool getStarted() const			{ return mStarted; }

	// Returns the seconds since epoch when this timer will expire.
	F64 expiresAt() const;

protected:
	// Number of seconds after application start when this timer was started.
	// Set equal to sFrameTime when reset.
	F64 mStartTime;

	// Timer expires this many seconds after application start time.
	F64 mExpiry;

	// Useful bit of state usually associated with timers, but does not affect
	// actual functionality
	bool mStarted;

	// Time at which the viewer session started in micro-seconds since epoch
	static U64 sStartTotalTime;

	//
	// Data updated at each frame
	//

	// Seconds since application start
	static F64 sFrameTime;

	// Time that has elapsed since last call to updateFrameTime()
	static U64 sFrameDeltaTime;

	// Total microseconds since epoch.
	static U64 sTotalTime;

	// Seconds since epoch.
	static F64 sTotalSeconds;

	// Total number of frames elapsed in application
	static S32 sFrameCount;
};

#endif  // LL_LLFRAMETIMER_H
