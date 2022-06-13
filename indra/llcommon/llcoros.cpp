/**
 * @file   llcoros.cpp
 * @author Nat Goodspeed
 * @date   2009-06-03
 * @brief  Implementation for llcoros.
 * 
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

// Precompiled header
#include "linden_common.h"
// associated header
#include "llcoros.h"
// STL headers
// std headers
// external library headers
#include <boost/bind.hpp>
// other Linden headers
#include "llevents.h"
#include "llerror.h"
#include "stringize.h"

LLCoros::LLCoros():
    // MAINT-2724: default coroutine stack size too small on Windows.
    // Previously we used
    // boost::context::guarded_stack_allocator::default_stacksize();
    // empirically this is 64KB on Windows and Linux. Try quadrupling.
    mStackSize(256*1024)
{
    // Register our cleanup() method for "mainloop" ticks
    LLEventPumps::instance().obtain("mainloop").listen(
        "LLCoros", boost::bind(&LLCoros::cleanup, this, _1));
}

bool LLCoros::cleanup(const LLSD&)
{
    // Walk the mCoros map, checking and removing completed coroutines.
    for (CoroMap::iterator mi(mCoros.begin()), mend(mCoros.end()); mi != mend; )
    {
        // Has this coroutine exited (normal return, exception, exit() call)
        // since last tick?
        if (mi->second->exited())
        {
			   LL_INFOS("LLCoros") << "LLCoros: cleaning up coroutine " << mi->first << LL_ENDL;
            // The erase() call will invalidate its passed iterator value --
            // so increment mi FIRST -- but pass its original value to
            // erase(). This is what postincrement is all about.
            mCoros.erase(mi++);
        }
        else
        {
            // Still live, just skip this entry as if incrementing at the top
            // of the loop as usual.
            ++mi;
        }
    }
    return false;
}

std::string LLCoros::generateDistinctName(const std::string& prefix) const
{
    // Allowing empty name would make getName()'s not-found return ambiguous.
    if (prefix.empty())
    {
        LL_ERRS("LLCoros") << "LLCoros::launch(): pass non-empty name string" << LL_ENDL;
    }

    // If the specified name isn't already in the map, just use that.
    std::string name(prefix);

    // Find the lowest numeric suffix that doesn't collide with an existing
    // entry. Start with 2 just to make it more intuitive for any interested
    // parties: e.g. "joe", "joe2", "joe3"...
    for (int i = 2; ; name = STRINGIZE(prefix << i++))
    {
        if (mCoros.find(name) == mCoros.end())
        {
			   LL_INFOS("LLCoros") << "LLCoros: launching coroutine " << name << LL_ENDL;
            return name;
        }
    }
}

bool LLCoros::kill(const std::string& name)
{
    CoroMap::iterator found = mCoros.find(name);
    if (found == mCoros.end())
    {
        return false;
    }
    // Because this is a boost::ptr_map, erasing the map entry also destroys
    // the referenced heap object, in this case the boost::coroutine object,
    // which will terminate the coroutine.
    mCoros.erase(found);
    return true;
}

std::string LLCoros::getNameByID(const void* self_id) const
{
    // Walk the existing coroutines, looking for one from which the 'self_id'
    // passed to us comes.
    for (CoroMap::const_iterator mi(mCoros.begin()), mend(mCoros.end()); mi != mend; ++mi)
    {
        namespace coro_private = boost::dcoroutines::detail;
        if (static_cast<void*>(coro_private::coroutine_accessor::get_impl(const_cast<coro&>(*mi->second)).get())
            == self_id)
        {
            return mi->first;
        }
    }
    return "";
}

void LLCoros::setStackSize(S32 stacksize)
{
    LL_INFOS("LLCoros") << "Setting coroutine stack size to " << stacksize << LL_ENDL;
    mStackSize = stacksize;
}

/*****************************************************************************
*   MUST BE LAST
*****************************************************************************/
// Turn off MSVC optimizations for just LLCoros::launchImpl() -- see
// DEV-32777. But MSVC doesn't support push/pop for optimization flags as it
// does for warning suppression, and we really don't want to force
// optimization ON for other code even in Debug or RelWithDebInfo builds.

#if LL_MSVC
// work around broken optimizations
#pragma warning(disable: 4748)
#pragma optimize("", off)
#endif // LL_MSVC

std::string LLCoros::launchImpl(const std::string& prefix, coro* newCoro)
{
    std::string name(generateDistinctName(prefix));
    mCoros.insert(name, newCoro);
    /* Run the coroutine until its first wait, then return here */
    (*newCoro)(std::nothrow);
    return name;
}

#if LL_MSVC
// reenable optimizations
#pragma optimize("", on)
#endif // LL_MSVC
