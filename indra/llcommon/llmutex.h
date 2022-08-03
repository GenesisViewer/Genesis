/**
 * @file llmutex.h
 * @brief Base classes for mutex and condition handling.
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

#ifndef LL_LLMUTEXNEW_H
#define LL_LLMUTEXNEW_H

// For now, disable the fibers-aware mutexes unconditionnaly, since they cause
// crashes (weird issue with "Illegal deletion of LLDrawable") or hangs at
// startup (try_lock() waiting forever in LLError). HB
#define LL_USE_FIBER_AWARE_MUTEX 0

// Guard against "comparison between signed and unsigned integer expressions"
// warnings (sometimes seen when compiling with gcc on system with glibc v2.35:
// probably because of the recent changes and integration of pthread in glibc)
// in boost/thread/pthread/thread_data.hpp, itself being included from
// boost/thread/condition_variable.hpp
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <mutex>
#if LL_USE_FIBER_AWARE_MUTEX
# include "boost/fiber/mutex.hpp"
# include "boost/fiber/condition_variable.hpp"
# define LL_MUTEX_TYPE boost::fibers::mutex
# define LL_UNIQ_LOCK_TYPE std::unique_lock<boost::fibers::mutex>
# define LL_COND_TYPE boost::fibers::condition_variable
#else
# include <condition_variable>
# define LL_MUTEX_TYPE std::mutex
# define LL_UNIQ_LOCK_TYPE std::unique_lock<std::mutex>
# define LL_COND_TYPE std::condition_variable
#endif

#include "boost/thread.hpp"

#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

#include "llerror.h"
#include "lltimer.h"		// For ms_sleep()

class LLMutexNew
{
protected:
	LOG_CLASS(LLMutexNew);

public:
	LL_INLINE LLMutexNew()
	:	mCount(0)
	{
	}

	virtual ~LLMutexNew() = default;

	void lock();				// Blocking
	void unlock();
	bool trylock();				// Non-blocking, returns true if lock held.

	// Undefined behavior when called on mutex not being held:
	bool isLocked();

	bool isSelfLocked();		// Returns true if locked in a same thread

protected:
	LL_MUTEX_TYPE			mMutex;
	// Type of id_t must be kept the same as in LLThread !  We cannot use
	// LLThread::id_t here, because it would result in a circular header
	// dependency... HB
	typedef boost::thread::id id_t;
	mutable id_t			mLockingThread;
	mutable U32				mCount;
};

// Actually a condition/mutex pair (since each condition needs to be associated
// with a mutex).
class LLConditionNew : public LLMutexNew
{
public:
	LLConditionNew() = default;
	~LLConditionNew() = default;

	// This method blocks
	LL_INLINE void wait()
	{
		LL_UNIQ_LOCK_TYPE lock(mMutex);
		mCond.wait(lock);
	}

	LL_INLINE void signal()
	{
		mCond.notify_one();
	}

	LL_INLINE void broadcast()
	{
		mCond.notify_all();
	}

protected:
	LL_COND_TYPE mCond;
};

// Scoped locking class
class LLMutexLockNew
{
public:
	LL_INLINE LLMutexLockNew(LLMutexNew* mutex)
	:	mMutex(mutex)
	{
		if (mMutex)
		{
			mMutex->lock();
		}
	}

	LL_INLINE LLMutexLockNew(LLMutexNew& mutex)
	:	mMutex(&mutex)
	{
		mMutex->lock();
	}

	LL_INLINE ~LLMutexLockNew()
	{
		if (mMutex)
		{
			mMutex->unlock();
		}
	}

private:
	LLMutexNew* mMutex;
};

// Scoped locking class similar in function to LLMutexLock but uses the
// trylock() method to conditionally acquire lock without blocking. Caller
// resolves the resulting condition by calling the isLocked() method and either
// punts or continues as indicated.
//
// Mostly of interest to callers needing to avoid stalls and that can guarantee
// another attempt at a later time.
class LLMutexTrylock
{
public:
	LL_INLINE LLMutexTrylock(LLMutexNew* mutex)
	:	mMutex(mutex)
	{
		mLocked = mMutex && mMutex->trylock();
	}

	LL_INLINE LLMutexTrylock(LLMutexNew& mutex)
	:	mMutex(&mutex)
	{
		mLocked = mMutex->trylock();
	}

	// Tries locking 'attempts' times, with 10ms sleep delays between each try.
	LLMutexTrylock(LLMutexNew* mutex, U32 attempts);

	LL_INLINE ~LLMutexTrylock()
	{
		if (mLocked)
		{
			mMutex->unlock();
		}
	}

	LL_INLINE void unlock()
	{
		if (mLocked)
		{
			mLocked = false;
			mMutex->unlock();
		}
	}

	LL_INLINE bool isLocked() const				{ return mLocked; }

private:
	LLMutexNew*	mMutex;
	// 'true' when the mutex is actually locked by this class
	bool		mLocked;
};

#endif // LL_LLMUTEX_H
