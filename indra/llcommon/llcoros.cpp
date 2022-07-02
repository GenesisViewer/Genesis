/**
 * @file   llcoros.cpp
 * @author Nat Goodspeed
 * @date   2009-06-03
 * @brief  Implementation for llcoros.
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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

#ifndef BOOST_DISABLE_ASSERTS
# define UNDO_BOOST_DISABLE_ASSERTS
#endif
// With Boost 1.65.1, needed for Mac with this specific header
#ifndef BOOST_DISABLE_ASSERTS
# define BOOST_DISABLE_ASSERTS
#endif
#include "boost/fiber/protected_fixedsize_stack.hpp"
#undef UNDO_BOOST_DISABLE_ASSERTS
#undef BOOST_DISABLE_ASSERTS
#include "boost/fiber/exceptions.hpp"

#include "llcoros.h"

#include "llevents.h"
#include "lltimer.h"

// Global
LLCoros gCoros;

static bool sDestroyed = false;

// Default coroutine stack size.
constexpr S32 LLCOROS_STACKSIZE = 512 * 1024;

LLCoros::LLCoros()
:	mStackSize(LLCOROS_STACKSIZE),
	// mCurrent does NOT own the current CoroData instance: it simply points to
	// it. So initialize it with a no-op deleter.
	mCurrent{ [](CoroData*){} }
{
}

LLCoros::~LLCoros()
{
	sDestroyed = true;
}

std::string LLCoros::generateDistinctName(const std::string& prefix) const
{
	static S32 unique = 0;

	// Allowing empty name would make getName()'s not-found return ambiguous.
	if (prefix.empty())
	{
		llerrs << "Empty name string !" << llendl;
	}

	// If the specified name is not already in the map, just use that.
	std::string name = prefix;

	// Until we find an unused name, append a numeric suffix for uniqueness.
	while (CoroData::getInstance(name))
	{
		name = llformat("%s%d", prefix.c_str(), unique++);
	}

	return name;
}

//static
LLCoros::CoroData& LLCoros::getCoroData()
{
	CoroData* current = NULL;
	if (!sDestroyed)
	{
		current = gCoros.mCurrent.get();
	}
	// For the main() coroutine, the one NOT explicitly launched by launch(),
	// we never explicitly set mCurrent. Use a static CoroData instance with
	// canonical values.
	if (!current)
	{
		// It is tempting to provide a distinct name for each thread's "main
		// coroutine." But as getName() has always returned the empty string to
		// mean "not in a coroutine," empty string should suffice here.
		static thread_local CoroData tMain("");
		// We need not reset() the local_ptr to this instance; we will simply
		// find it again every time we discover that current is NULL.
		current = &tMain;
	}
	return *current;
}

//static
std::string LLCoros::getName()
{
	return getCoroData().mName;
}

//static
LLCoros::coro::id LLCoros::get_self()
{
	return boost::this_fiber::get_id();
}

//static
void LLCoros::set_consuming(bool consuming)
{
	CoroData& data(getCoroData());
	// DO NOT call this on the main() coroutine.
	llassert_always(!data.mName.empty());
	data.mConsuming = consuming;
}

//static
bool LLCoros::get_consuming()
{
	return getCoroData().mConsuming;
}

void LLCoros::setStackSize(S32 stacksize)
{
	llinfos << "Setting coroutine stack size to " << stacksize << llendl;
	mStackSize = stacksize;
}

// Top-level wrapper around caller's coroutine callable.
// Normally we like to pass strings and such by const reference, but in this
// case, we WANT to copy both the name and the callable to our local stack !
void LLCoros::toplevel(std::string name, callable_t callable)
{
	// Keep the CoroData on this top-level function's stack frame
	CoroData corodata(name);
	// Set it as current
	mCurrent.reset(&corodata);

	// Run the code the caller actually wants in the coroutine
	try
	{
		// Run the code the caller actually wants in the coroutine
		callable();
	}
	catch (std::exception& e)
	{
		llwarns << "Caught exception '" << e.what() << "' during coroutine"
				<< llendl;
	}
	catch (...)
	{
		llwarns << "An unknown exception occurred during coroutine" << llendl;
	}
}

std::string LLCoros::launch(const std::string& prefix,
							const callable_t& callable)
{
	std::string name = generateDistinctName(prefix);
	LL_DEBUGS("Coros") << "Launching coroutine: " << name << LL_ENDL;

	try
	{
		// 'dispatch' means: enter the new fiber immediately, returning here
		// only when the fiber yields for whatever reason. std::allocator_arg
		// is a flag to indicate that the following argument is a
		// StackAllocator. protected_fixedsize_stack sets a guard page past the
		// end of the new stack so that stack underflow will result in an
		// access violation instead of weird, subtle, possibly undiagnosed
		// memory stomps.
		boost::fibers::fiber new_coro(boost::fibers::launch::dispatch,
									  std::allocator_arg,
									  boost::fibers::protected_fixedsize_stack(mStackSize),
									  [this, &name, &callable]()
									  {
										toplevel(name, callable);
									  });
		// You have two choices with a fiber instance: you can join() it or you
		// can detach() it. If you try to destroy the instance before doing
		// either, the program silently terminates.
		new_coro.detach();
	}
	catch (...)
	{
		llwarns << "Failed to start coroutine: " << name << llendl;
	}

	return name;
}

void LLCoros::printActiveCoroutines()
{
	if (!CoroData::instanceCount())
	{
		llinfos << "No active coroutine" << llendl;
		return;
	}
	llwarns << "------ List of active coroutines ------\n";
	F64 now = LLTimer::getTotalSeconds();
	for (auto& cd : CoroData::instance_snapshot())
	{
		F64 life_time = now - cd.mCreationTime;
		llcont << "Name: " << cd.mName << " - Key: " << cd.getKey()
			   << " - Life time: " << life_time << " seconds.\n";
	}
	llcont << "---------------------------------------" << llendl;
}

///////////////////////////////////////////////////////////////////////////////
// LLCoros::CoroData sub-class
///////////////////////////////////////////////////////////////////////////////

LLCoros::CoroData::CoroData(const std::string& name)
:	LLInstanceTracker<CoroData, std::string>(name),
	mName(name),
	// Do not consume events unless specifically directed
	mConsuming(false),
	mCreationTime(LLTimer::getTotalSeconds())
{
	if (name.empty())
	{
		mName = LLEventPump::inventName("coro");
		llinfos << "Auto-generated coro name: " << mName << llendl;
	}
	LL_DEBUGS("Coros") << "Created CoroData for coroutine: " << mName
					   << LL_ENDL;
}
