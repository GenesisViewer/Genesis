/**
 * @file llmutex.cpp
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llmutex.h"

#include "llthread.h"

void LLMutex::lock()
{
	if (isSelfLocked())
	{
		// Redundant lock
		++mCount;
		return;
	}

	mMutex.lock();
	mLockingThread = LLThread::currentID();
}

bool LLMutex::trylock()
{
	if (isSelfLocked())
	{
		// Redundant lock
		++mCount;
		return true;
	}

	if (!mMutex.try_lock())
	{
		return false;
	}

	mLockingThread = LLThread::currentID();
	return true;
}

void LLMutex::unlock()
{
	if (mCount > 0)
	{
		// Not the root unlock
		--mCount;
		return;
	}

	mLockingThread = LLThread::id_t();
	mMutex.unlock();
}

bool LLMutex::isLocked()
{
	if (!mMutex.try_lock())
	{
		return true;
	}

	mMutex.unlock();
	return false;
}

bool LLMutex::isSelfLocked()
{
	return mLockingThread == LLThread::currentID();
}

///////////////////////////////////////////////////////////////////////////////
// LLMutexTrylock class
///////////////////////////////////////////////////////////////////////////////

LLMutexTrylock::LLMutexTrylock(LLMutex* mutex, U32 attempts)
:	mMutex(mutex),
	mLocked(false)
{
	if (mMutex && attempts > 0)
	{
		while (!(mLocked = mMutex->trylock()) && --attempts > 0)
		{
			ms_sleep(10);
		}
	}
}
