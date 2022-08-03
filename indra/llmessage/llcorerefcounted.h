/**
 * @file llcorerefcounted.h
 * @brief Atomic, thread-safe ref counting and destruction mixin class
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LLCOREINT__REFCOUNTED_H_
#define LLCOREINT__REFCOUNTED_H_

#include "llatomic.h"

#if defined(GCC_VERSION) && GCC_VERSION >= 50000
// gcc v5.0+ does not like some unused variables in boost/thread.hpp...
# pragma GCC diagnostic ignored "-Wunused-variable"
#endif

#include "boost/thread.hpp"
#include "boost/intrusive_ptr.hpp"

namespace LLCoreInt
{

class RefCounted
{
private:
	RefCounted() = delete;
	void operator=(const RefCounted&) = delete;

public:
	explicit RefCounted(bool implicit)
	:	mRefCount(S32(implicit))
	{
	}

	// ref-count interface
	LL_INLINE void addRef() const
	{
		mRefCount+=1;
		S32 count = mRefCount;
		llassert_always(count >= 0);
	}

	LL_INLINE void release() const
	{
		S32 count = mRefCount;
		llassert_always(count != NOT_REF_COUNTED && count > 0);
		count = --mRefCount;

		// clean ourselves up if that was the last reference
		if (count == 0)
		{
			const_cast<RefCounted*>(this)->destroySelf();
		}
	}

	LL_INLINE bool isLastRef() const
	{
		const S32 count = mRefCount;
		llassert_always(count != NOT_REF_COUNTED && count >= 1);
		return count == 1;
	}

	LL_INLINE S32 getRefCount() const
	{
		const S32 result = mRefCount;
		return result;
	}

	LL_INLINE void noRef() const
	{
		llassert_always(mRefCount <= 1);
		mRefCount = NOT_REF_COUNTED;
	}

	static constexpr S32 NOT_REF_COUNTED = -1;

protected:
	virtual ~RefCounted() = default;

	LL_INLINE virtual void destroySelf()
	{
		delete this;
	}

private:
	mutable LLAtomicS32 mRefCount;

}; // end class RefCounted

/**
 * boost::intrusive_ptr may be used to manage RefCounted classes. Unfortunately
 * RefCounted and boost::intrusive_ptr use different conventions for the
 * initial refcount value. To avoid leaky (immortal) objects, you should really
 * construct boost::intrusive_ptr<RefCounted*>(rawptr, false). IntrusivePtr<T>
 * encapsulates that for you.
 */
template <typename T>
struct IntrusivePtr : public boost::intrusive_ptr<T>
{
	IntrusivePtr()
	:	boost::intrusive_ptr<T>()
	{
	}

	IntrusivePtr(T* p)
	:	boost::intrusive_ptr<T>(p, false)
	{
	}
};

LL_INLINE void intrusive_ptr_add_ref(RefCounted* p)
{
	p->addRef();
}

LL_INLINE void intrusive_ptr_release(RefCounted* p)
{
	p->release();
}

} // End namespace LLCoreInt

#endif	// LLCOREINT__REFCOUNTED_H_
