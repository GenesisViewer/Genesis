/**
 * @file   llcoros.h
 * @author Nat Goodspeed
 * @date   2009-06-02
 * @brief  Manage running boost::fiber instances
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

#ifndef LL_LLCOROS_H
#define LL_LLCOROS_H

#include <string>

#include "boost/fiber/fiber.hpp"
#include "boost/fiber/fss.hpp"
#include "boost/fiber/future/promise.hpp"
#include "boost/fiber/future/future.hpp"
#include "boost/function.hpp"

#include "llerror.h"
#include "llinstancetracker.h"

// Registry of named Boost.Coroutine instances
//
// The viewer's use of the term "coroutine" became deeply embedded before the
// industry term "fiber" emerged to distinguish userland threads from simpler,
// more transient kinds of coroutines. Semantically they have always been
// fibers. But at this point in history, we are pretty much stuck with the term
// "coroutine."
//
// The Boost.Coroutine library supports the general case of a coroutine
// accepting arbitrary parameters and yielding multiple (sets of) results. For
// such use cases, it's natural for the invoking code to retain the coroutine
// instance: the consumer repeatedly calls into the coroutine, perhaps passing
// new parameter values, prompting it to yield its next result.
//
// Our typical coroutine usage is different, though. For us, coroutines
// provide an alternative to the Responder pattern. Our typical coroutine
// has void return, invoked in fire-and-forget mode: the handler for some
// user gesture launches the coroutine and promptly returns to the main loop.
// The coroutine initiates some action that will take multiple frames (e.g. a
// capability request), waits for its result, processes it and silently steals
// away.
//
// This usage poses two (related) problems:
//
// # Who should own the coroutine instance? If it's simply local to the
//   handler code that launches it, return from the handler will destroy the
//   coroutine object, terminating the coroutine.
// # Once the coroutine terminates, in whatever way, who's responsible for
//   cleaning up the coroutine object?
//
// LLCoros is a Singleton collection of currently-active coroutine instances.
// Each has a name. You ask LLCoros to launch a new coroutine with a suggested
// name prefix; from your prefix it generates a distinct name, registers the
// new coroutine and returns the actual name.
//
// The name can be used to kill off the coroutine prematurely, if needed. It
// can also provide diagnostic info: we can look up the name of the
// currently-running coroutine.
//
// Finally, the next frame ("mainloop" event) after the coroutine terminates,
// LLCoros will notice its demise and destroy it.
class LLCoros final
{
protected:
	LOG_CLASS(LLCoros);

public:
	LLCoros();
	~LLCoros();

	// Canonical signature we use
	typedef boost::fibers::fiber coro;

	// Canonical callable type
	typedef boost::function<void()> callable_t;

	// Creates and starts running a new coroutine with specified name. The name
	// string you pass is a suggestion; it will be tweaked for uniqueness. The
	// actual name is returned to you.
	//
	// Usage looks like this, for (e.g.) two coroutine parameters:
	// class MyClass
	// {
	// public:
	//	 ...
	//	 // Do NOT NOT NOT accept reference params !
	//	 // Pass by value only !
	//	 void myCoroutineMethod(std::string, LLSD);
	//	 ...
	// };
	// ...
	// std::string name =
	//		gCoros.launch("mycoro",
	//					 boost::bind(&MyClass::myCoroutineMethod, this,
	//								 "somestring", LLSD(17)));
	//
	// Your function/method can accept any parameters you want -- but ONLY BY
	// VALUE ! Reference parameters are a BAD IDEA !  You have been warned. See
	// DEV-32777 comments for an explanation.
	//
	// Pass a nullary callable. It works to directly pass a nullary free
	// function (or static method); for all other cases use boost::bind(). Of
	// course, for a non-static class method, the first parameter must be the
	// class instance. Any other parameters should be passed via the bind()
	// expression.
	//
	// launch() tweaks the suggested name so it won't collide with any
	// existing coroutine instance, creates the coroutine instance, registers
	// it with the tweaked name and runs it until its first wait. At that
	// point it returns the tweaked name.
	std::string launch(const std::string& prefix, const callable_t& callable);

	// From within a coroutine, looks up the (tweaked) name string by which
	// this coroutine is registered. Returns an auto-generated name if not
	// found (e.g. if the coroutine was launched by hand rather than using
	// LLCoros::launch()).
	static std::string getName();

	// For delayed initialization
	void setStackSize(S32 stacksize);

	LL_INLINE bool hasActiveCoroutines() const		{ return CoroData::instanceCount() > 0; }

	void printActiveCoroutines();

	// Gets the current coro::id for those who really really care
	static coro::id get_self();

	// Most coroutines, most of the time, do not "consume" the events for which
	// they are suspending. This way, an arbitrary number of listeners (whether
	// coroutines or simple callbacks) can be registered on a particular
	// LLEventPump, every listener responding to each of the events on that
	// LLEventPump. But a particular coroutine can assert that it will consume
	// each event for which it suspends. (See also llcoro::postAndSuspend(),
	// llcoro::VoidListener)
	static void set_consuming(bool consuming);
	static bool get_consuming();

	// Aliases for promise and future.
	// An older underlying future implementation required us to wrap future;
	// this is no longer needed. However, if it is important to restore kill()
	// functionality, we might need to provide a proxy, so continue using the
	// aliases.
	template <typename T>
	using Promise = boost::fibers::promise<T>;

	template <typename T>
	using Future = boost::fibers::future<T>;

	template <typename T>
	static Future<T> getFuture(Promise<T>& promise)
	{
		return promise.get_future();
	}

	// For data local to each running coroutine
	template <typename T>
	using local_ptr = boost::fibers::fiber_specific_ptr<T>;

private:
	std::string generateDistinctName(const std::string& prefix) const;

	void toplevel(std::string name, callable_t callable);

	class CoroData;
	static CoroData& getCoroData();

private:
	S32					mStackSize;

	// Coroutine-local storage, as it were: one per coro we track
	class CoroData : public LLInstanceTracker<CoroData, std::string>
	{
	protected:
		LOG_CLASS(LLCoros::CoroData);

	public:
		CoroData(const std::string& name);

	public:
		// Tweaked name of the current coroutine
		F64			mCreationTime;
		std::string	mName;
		bool		mConsuming;			// set_consuming() state
	};

	// Identify the current fiber CoroData
	local_ptr<CoroData>	mCurrent;
};

extern LLCoros gCoros;

#endif	// LL_LLCOROS_H
