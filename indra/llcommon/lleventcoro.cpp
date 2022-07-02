/**
 * @file   lleventcoro.cpp
 * @author Nat Goodspeed
 * @date   2009-04-29
 * @brief  Implementation for lleventcoro.
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

#include <chrono>

#include "boost/fiber/operations.hpp"

#include "lleventcoro.h"

#include "llcoros.h"
#include "llsdserialize.h"
#include "llsdutil.h"

namespace
{

// Implements behavior described for postAndSuspend()'s reply_pump_name_path
// parameter:
// * If path.isUndefined(), do nothing.
// * If path.isString(), dest is an LLSD map, store value into
//   dest[path.asString()].
// * If path.isInteger(), dest is an LLSD array: store value into
//   dest[path.asInteger()].
// * If path.isArray(), iteratively apply the rules above to step down through
//   the structure of dest. The last array entry in path specifies the entry in
//   the lowest-level structure in dest into which to store value.
//
// Note: in the degenerate case in which path is an empty array, dest will
// become value rather than containing it.
void storeToLLSDPath(LLSD& dest, const LLSD& path, const LLSD& value)
{
	if (path.isDefined())
	{
		llsd::drill(dest, path) = value;
	}
}

} // anonymous

void llcoro::suspend()
{
	boost::this_fiber::yield();
}

void llcoro::suspendUntilTimeout(F32 seconds)
{
	boost::this_fiber::sleep_for(std::chrono::milliseconds(long(seconds * 1000)));
}

namespace
{
LLBoundListener postAndSuspendSetup(const std::string& caller_name,
									const std::string& listener_name,
									LLCoros::Promise<LLSD>& promise,
									const LLSD& event,
									const LLEventPumpOrPumpName& request_pumpp,
									const LLEventPumpOrPumpName& reply_pumpp,
									const LLSD& reply_pump_name_path)
{
	// Get the consuming attribute for THIS coroutine, the one that is about to
	// suspend. Do not call get_consuming() in the lambda body: that would
	// return the consuming attribute for some other coroutine, most likely
	// the main routine.
	bool consuming = LLCoros::get_consuming();
	// Listen on the specified LLEventPump with a lambda that will assign a
	// value to the promise, thus fulfilling its future.
	if (!reply_pumpp)
	{
		llerrs << "reply_pumpp required for " << caller_name << llendl;
	}
	LLEventPump& reply_pump(reply_pumpp.getPump());
	LLBoundListener connection(reply_pump.listen(listener_name,
							   [&promise, consuming,
								listener_name](const LLSD& result)
							   {
									try
									{
										promise.set_value(result);
										return consuming;
									}
									catch (boost::fibers::promise_already_satisfied& e)
									{
										llinfos << "Promise already satisfied in '"
												<< listener_name << ": "
												<< e.what() << llendl;
										return false;
									}
							   }));
	// Skip the "post" part if request_pumpp is default-constructed
	if (request_pumpp)
	{
		LLEventPump& request_pump(request_pumpp.getPump());
		// If reply_pump_name_path is non-empty, store the reply_pump name in
		// the request event.
		LLSD modevent(event);
		storeToLLSDPath(modevent, reply_pump_name_path, reply_pump.getName());
		LL_DEBUGS("EventCoro") << caller_name << ": coroutine "
							   << listener_name << " posting to "
							   << request_pump.getName() << LL_ENDL;
		request_pump.post(modevent);
	}
	LL_DEBUGS("EventCoro") << caller_name << ": coroutine " << listener_name
						   << " about to wait on LLEventPump "
						   << reply_pump.getName() << LL_ENDL;
	return connection;
}

} // anonymous

LLSD llcoro::postAndSuspend(const LLSD& event,
							const LLEventPumpOrPumpName& request_pump,
							const LLEventPumpOrPumpName& reply_pump,
							const LLSD& reply_pump_name_path)
{
	LLCoros::Promise<LLSD> promise;
	std::string listener_name = LLCoros::getName();

	// Store connection into an LLTempBoundListener so we implicitly disconnect
	// on return from this function.
	LLTempBoundListener connection = postAndSuspendSetup("postAndSuspend()",
														 listener_name,
														 promise, event,
														 request_pump,
														 reply_pump,
														 reply_pump_name_path);

	// Declare the future
	LLCoros::Future<LLSD> future = LLCoros::getFuture(promise);

	// Calling get() on the future makes us wait for it
	LLSD value(future.get());
	LL_DEBUGS("EventCoro") << "Coroutine '" << listener_name
						   << "' resuming with: " << value << LL_ENDL;

	// Returning should disconnect the connection
	return value;
}
