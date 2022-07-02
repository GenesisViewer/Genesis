/**
 * @file   lleventcoro.h
 * @author Nat Goodspeed
 * @date   2009-04-29
 * @brief  Utilities to interface between coroutines and events.
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

#ifndef LL_LLEVENTCORO_H
#define LL_LLEVENTCORO_H

#include <string>

#include "boost/optional.hpp"

#include "llerror.h"
#include "llevents.h"

// Like LLListenerOrPumpName, this is a class intended for parameter lists:
// accepts a const LLEventPumpOrPumpName& and you can accept either an
// LLEventPump& or its string name. For a single parameter that could be
// either, it is not hard to overload the function, but as soon as you want to
// accept two such parameters, this is cheaper than four overloads.
class LLEventPumpOrPumpName
{
public:
	// Pass an actual LLEventPump&
	LLEventPumpOrPumpName(LLEventPump& pump)
	:	mPump(pump)
	{
	}

	// Pass the string name of an LLEventPump
	LLEventPumpOrPumpName(const std::string& pumpname)
	:	mPump(gEventPumps.obtain(pumpname))
	{
	}

	// Pass string constant name of an LLEventPump. This override must be
	// explicit, since otherwise passing const char* to a function
	// accepting const LLEventPumpOrPumpName& would require two
	// different implicit conversions: const char* -> const
	// std::string& -> const LLEventPumpOrPumpName&.
	LLEventPumpOrPumpName(const char* pumpname)
	:	mPump(gEventPumps.obtain(pumpname))
	{
	}

	// Unspecified: "I choose not to identify an LLEventPump."
	LLEventPumpOrPumpName() = default;

	LL_INLINE operator LLEventPump&() const			{ return *mPump; }
	LL_INLINE LLEventPump& getPump() const			{ return *mPump; }
	LL_INLINE operator bool() const					{ return bool(mPump); }
	LL_INLINE bool operator!() const				{ return !mPump; }

private:
	boost::optional<LLEventPump&> mPump;
};

namespace llcoro
{

// Yields control from a coroutine for one "mainloop" tick. If your coroutine
// runs without suspending for nontrivial time, sprinkle in calls to this
// function to avoid stalling the rest of the viewer processing.
void suspend();

// Yields control from a coroutine for at least the specified number of seconds
void suspendUntilTimeout(F32 seconds);

// Posts specified LLSD event on the specified LLEventPump, then suspend for a
// response on specified other LLEventPump. This is more than mere convenience:
// the difference between this function and the sequence.
// Example code:
// request_pump.post(myEvent);
// LLSD reply = suspendUntilEventOn(reply_pump);
// is that the sequence above fails if the reply is posted immediately on
// reply_pump, that is, before request_pump.post() returns. In the sequence
// above, the running coroutine is not even listening on reply_pump until 
// request_pump.post() returns and suspendUntilEventOn() is entered. Therefore,
// the coroutine completely misses an immediate reply event, making it suspend
// indefinitely.
// By contrast, postAndSuspend() listens on the reply_pump before posting the
// specified LLSD event on the specified request_pump.
// 'event' is the LLSD data to be posted on request_pump.
// 'request_pump' is an LLEventPump on which to post 'event'. Pass either the
// LLEventPump& or its string name. However, if you pass a default-constructed
// LLEventPumpOrPumpName, we skip the post() call.
// 'reply_pump' is an LLEventPump on which postAndSuspend() will listen for a
// reply. Pass either the LLEventPump& or its string name. The calling
// coroutine will suspend until that reply arrives (if you are concerned about
// a reply that might not arrive, please see also LLEventTimeout).
// 'reply_pump_name_path' specifies the location within event in which to
// store reply_pump.getName(). This is a strictly optional convenience feature;
// obviously you can store the name in event "by hand" if desired.
// 'reply_pump_name_path' can be specified in any of four forms:
// * isUndefined() (default-constructed LLSD object): do nothing. This is the
//   default behavior if you omit reply_pump_name_path.
// * isInteger(): event is an array. Store reply_pump.getName() in 
//   event[reply_pump_name_path.asInteger()].
// * isString(): event is a map. Store reply_pump.getName() in
//   event[reply_pump_name_path.asString()].
// * isArray(): event has several levels of structure, e.g. map of maps, array
//   of arrays, array of maps, map of arrays, ... Store reply_pump.getName() in
//   event[reply_pump_name_path[0]][reply_pump_name_path[1]]... In other words,
//   examine each array entry in reply_pump_name_path in turn. If it is an
//   LLSD::String, the current level of event is a map; step down to that map
//   entry. If it is an LLSD::Integer, the current level of event is an array;
//   step down to that array entry. The last array entry in
//   reply_pump_name_path specifies the entry in the lowest-level structure in
//   event into which to store reply_pump.getName().
LLSD postAndSuspend(const LLSD& event,
					const LLEventPumpOrPumpName& request_pump,
					const LLEventPumpOrPumpName& reply_pump,
					const LLSD& reply_pump_name_path = "reply");

// Wait for the next event on the specified LLEventPump. Pass either the
// LLEventPump& or its string name.
LL_INLINE LLSD suspendUntilEventOn(const LLEventPumpOrPumpName& pump)
{
	// This is now a convenience wrapper for postAndSuspend().
	return postAndSuspend(LLSD(), LLEventPumpOrPumpName(), pump);
}

} // namespace llcoro

// Certain event APIs require the name of an LLEventPump on which they should
// post results. While it works to invent a distinct name and let
// LLEventPumps::obtain() instantiate the LLEventPump as a "named singleton,"
// in a certain sense it is more robust to instantiate a local LLEventPump and
// provide its name instead. This class packages the following idiom:
// 1. Instantiate a local LLCoroEventPump, with an optional name prefix.
// 2. Provide its actual name to the event API in question as the name of the
//	reply LLEventPump.
// 3. Initiate the request to the event API.
// 4. Call your LLEventTempStream's suspend() method to suspend for the reply.
// 5. Let the LLCoroEventPump go out of scope.
class LLCoroEventPump
{
protected:
	LOG_CLASS(LLCoroEventPump);

public:
	LLCoroEventPump(const std::string& name = "coro")
	:	mPump(name, true)	// allow tweaking the pump instance name
	{
	}

	// It is typical to request the LLEventPump name to direct an event API to
	// send its response to this pump.
	LL_INLINE std::string getName() const			{ return mPump.getName(); }

	// Less typically, we would request the pump itself for some reason.
	LL_INLINE LLEventPump& getPump()				{ return mPump; }

	// Wait for an event on this LLEventPump.
	LL_INLINE LLSD suspend()
	{
		return llcoro::suspendUntilEventOn(mPump);
	}

	LL_INLINE LLSD postAndSuspend(const LLSD& event,
								  const LLEventPumpOrPumpName& request_pump,
								  const LLSD& reply_pump_name_path = LLSD())
	{
		return llcoro::postAndSuspend(event, request_pump, mPump,
									  reply_pump_name_path);
	}

private:
	LLEventStream mPump;
};

namespace llcoro
{

// Instantiates a temporary local LLCoroEventPump and call its postAndSuspend()
// method, returning the result. This supports a one-liner query of some
// LLEventAPI. For multiple calls from the same function, it's still cheaper to
// instantiate LLCoroEventPump explicitly.
LL_INLINE LLSD postAndSuspendTemp(const LLSD& event,
								  const LLEventPumpOrPumpName& req_pump,
								  const LLSD& reply_pump_name = "reply")
{
	LLCoroEventPump waiter;
	return waiter.postAndSuspend(event, req_pump, reply_pump_name);
}

} // namespace llcoro


#endif 	// LL_LLEVENTCORO_H
