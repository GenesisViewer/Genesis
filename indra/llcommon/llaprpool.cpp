/**
 * @file llaprpool.cpp
 *
 * Copyright (c) 2010, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   04/04/2010
 *   - Initial version, written by Aleric Inglewood @ SL
 *
 *   10/11/2010
 *   - Changed filename, class names and license to a more
 *     company-neutral format.
 *   - Added APR_HAS_THREADS #if's to allow creation and destruction
 *     of subpools by threads other than the parent pool owner.
 */

#include "linden_common.h"

#include "llerror.h"
#include "llaprpool.h"
#include "llatomic.h"
#include "llthread.h"

// Create a subpool from parent.
void LLAPRPool::create(LLAPRPool& parent)
{
	llassert(!mPool);			// Must be non-initialized.
	mParent = &parent;
	if (!mParent)				// Using the default parameter?
	{
		// By default use the root pool of the current thread.
		mParent = &LLThreadLocalData::tldata().mRootPool;
	}
	llassert(mParent->mPool);	// Parent must be initialized.
#if APR_HAS_THREADS
	// As per the documentation of APR (ie http://apr.apache.org/docs/apr/1.4/apr__pools_8h.html):
	//
	// Note that most operations on pools are not thread-safe: a single pool should only be
	// accessed by a single thread at any given time. The one exception to this rule is creating
	// a subpool of a given pool: one or more threads can safely create subpools at the same
	// time that another thread accesses the parent pool.
	//
	// In other words, it's safe for any thread to create a (sub)pool, independent of who
	// owns the parent pool.
	mOwner.reset_inline();
#else
	mOwner = mParent->mOwner;
	llassert(mOwner.equals_current_thread_inline());
#endif
	apr_status_t const apr_pool_create_status = apr_pool_create(&mPool, mParent->mPool);
	llassert_always(apr_pool_create_status == APR_SUCCESS);
	llassert(mPool);			// Initialized.
	apr_pool_cleanup_register(mPool, this, &s_plain_cleanup, &apr_pool_cleanup_null);
}

// Destroy the (sub)pool, if any.
void LLAPRPool::destroy(void)
{
	// Only do anything if we are not already (being) destroyed.
	if (mPool)
	{
#if !APR_HAS_THREADS
		// If we are a root pool, then every thread may destruct us: in that case
		// we have to assume that no other thread will use this pool concurrently,
		// of course. Otherwise, if we are a subpool, only the thread that owns
		// the parent may destruct us, since that is the pool that is still alive,
		// possibly being used by others and being altered here.
		llassert(!mParent || mParent->mOwner.equals_current_thread_inline());
#endif
		apr_pool_t* pool = mPool;
		mPool = NULL;				// Mark that we are BEING destructed.
		apr_pool_cleanup_kill(pool, this, &s_plain_cleanup);
		apr_pool_destroy(pool);
	}
}

bool LLAPRPool::parent_is_being_destructed(void)
{
	return mParent && (!mParent->mPool || mParent->parent_is_being_destructed());
}

LLAPRInitialization::LLAPRInitialization(void)
{
	static bool apr_initialized = false;

	if (!apr_initialized)
	{
		apr_initialize();
	}

	apr_initialized = true;
}

bool LLAPRRootPool::sCountInitialized = false;
LLAtomicS32 LLAPRRootPool::sCount;

LLAPRRootPool::LLAPRRootPool(void) : LLAPRInitialization(), LLAPRPool(0)
{
	// sCountInitialized don't need locking because when we get here there is still only a single thread.
	if (!sCountInitialized)
	{
#ifdef NEEDS_APR_ATOMICS
		apr_status_t status = apr_atomic_init(mPool);
		llassert_always(status == APR_SUCCESS);
#endif
		sCount = 1;	// Set to 1 to account for the global root pool.
		sCountInitialized = true;

		// Initialize thread-local APR pool support.
		// Because this recursively calls LLAPRRootPool::LLAPRRootPool(void)
		// it must be done last, so that sCount is already initialized.
		LLThreadLocalData::init();
	}
	sCount++;
}

LLAPRRootPool::~LLAPRRootPool()
{
	if (!--sCount)
	{
		// The last pool was destructed. Cleanup remainder of APR.
		LL_INFOS("APR") << "Cleaning up APR" << LL_ENDL;

		// Must destroy ALL, and therefore this last LLAPRRootPool, before terminating APR.
		static_cast<LLAPRRootPool*>(this)->destroy();

		apr_terminate();
	}
}

//static
LLAPRRootPool& LLAPRRootPool::get(void)
{
  static LLAPRRootPool global_APRpool(0);		// This is what used to be gAPRPoolp.
  return global_APRpool;
}

void LLVolatileAPRPool::clearVolatileAPRPool()
{
	llassert_always(mNumActiveRef > 0);
	if (--mNumActiveRef == 0)
	{
		if (isOld())
		{
			destroy();
			mNumTotalRef = 0 ;
		}
		else
		{
			// This does not actually free the memory,
			// it just allows the pool to re-use this memory for the next allocation.
			clear();
		}
	}

	// Paranoia check if the pool is jammed.
	llassert(mNumTotalRef < (FULL_VOLATILE_APR_POOL << 2)) ;
}
