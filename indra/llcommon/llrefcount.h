/**
 * @file llrefcount.h
 * @brief Base class for reference counted objects for use with LLPointer
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LLREFCOUNT_H
#define LLREFCOUNT_H

#include "llatomic.h"
#include "llerror.h"

//-----------------------------------------------------------------------------
// RefCount objects should generally only be accessed by way of LLPointer<>'s.
// See llpointer.h for LLPointer<> definition
//-----------------------------------------------------------------------------

class LLRefCount
{
protected:
	inline LLRefCount(const LLRefCount&) noexcept
	:	mRef(0)
	{
	}

	inline LLRefCount& operator=(const LLRefCount&) noexcept
	{
		// Do nothing, since ref count is specific to *this* reference
		return *this;
	}

	virtual ~LLRefCount();	// Use unref()

public:
	inline LLRefCount() noexcept
	:	mRef(0)
	{
	}

	inline void ref() const noexcept
	{
		++mRef;
	}

	inline void unref() const
	{
		llassert(mRef >= 1);
		if (--mRef == 0)
		{
			// If we hit zero, the caller should be the only smart pointer
			// owning the object and we can delete it.
			delete this;
		}
	}

	// NOTE: when passing around a const LLRefCount object, this can return
	// different results at different types, since mRef is mutable
	inline S32 getNumRefs() const
	{
		return mRef;
	}

private:
	mutable S32	mRef;
};

//-----------------------------------------------------------------------------
// LLThreadSafeRefCount class
//-----------------------------------------------------------------------------

class LLThreadSafeRefCount
{
protected:
	virtual ~LLThreadSafeRefCount();	// Use unref()

public:
	inline LLThreadSafeRefCount() noexcept
	:	mRef(0)
	{
	}

	// Non-copyable because LLAtomicS32 (std::atomic<S32>) is non-copyable. HB
	LLThreadSafeRefCount(const LLThreadSafeRefCount&) noexcept = delete;
	LLThreadSafeRefCount& operator=(const LLThreadSafeRefCount&) noexcept = delete;

	inline void ref() noexcept
	{
		++mRef;
	}

	inline void unref()
	{
		llassert(mRef >= 1);
		if (--mRef == 0)
		{
			// If we hit zero, the caller should be the only smart pointer
			// owning the object and we can delete it. It is technically
			// possible for a vanilla pointer to mess this up, or another
			// thread to jump in, find this object, create another smart
			// pointer and end up dangling, but if the code is that bad and not
			// thread-safe, it is trouble already.
			delete this;
		}
	}

	inline S32 getNumRefs() const
	{
		const S32 current_val = mRef.CurrentValue();
		return current_val;
	}

private:
	LLAtomicS32 mRef;
};

#endif
