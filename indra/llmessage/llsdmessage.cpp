/**
 * @file   llsdmessage.cpp
 * @author Nat Goodspeed
 * @date   2008-10-31
 * @brief  Implementation for llsdmessage.
 * 
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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

#if LL_WINDOWS
#pragma warning (disable : 4675) // "resolved by ADL" -- just as I want!
#endif

// Precompiled header
#include "linden_common.h"
// associated header
#include "llsdmessage.h"
// STL headers
// std headers
// external library headers
// other Linden headers
#include "llevents.h"
#include "llsdserialize.h"
#include "llhttpclient.h"
#include "llmessageconfig.h"
#include "llhost.h"
#include "message.h"
#include "llsdutil.h"
#include "aihttptimeoutpolicy.h"

// Declare a static LLSDMessage instance to ensure that we have a listener as
// soon as someone tries to post on our canonical LLEventPump name.
static LLSDMessage httpListener;

LLSDMessage::LLSDMessage():
    // Instantiating our own local LLEventPump with a string name the
    // constructor is NOT allowed to tweak is a way of ensuring Singleton
    // semantics: attempting to instantiate a second LLSDMessage object would
    // throw LLEventPump::DupPumpName.
    mEventPump("LLHTTPClient")
{
    mEventPump.listen("self", boost::bind(&LLSDMessage::httpListener, this, _1));
}

bool LLSDMessage::httpListener(const LLSD& request)
{
    llassert(false);	// This function is never called. --Aleric

    // Extract what we want from the request object. We do it all up front
    // partly to document what we expect.
    LLSD::String url(request["url"]);
    LLSD payload(request["payload"]);
    LLSD::String reply(request["reply"]);
    LLSD::String error(request["error"]);
    LLSD::String timeoutpolicy(request["timeoutpolicy"]);
    // If the LLSD doesn't even have a "url" key, we doubt it was intended for
    // this listener.
    if (url.empty())
    {
        std::ostringstream out;
        out << "request event without 'url' key to '" << mEventPump.getName() << "'";
        throw ArgError(out.str());
    }
	LLSDMessage::EventResponder* responder =
		new LLSDMessage::EventResponder(LLEventPumps::instance(), request, url, "POST", reply, error);
	responder->setTimeoutPolicy(timeoutpolicy);
    LLHTTPClient::post(url, payload, responder);
    return false;
}

LLSDMessage::EventResponder::EventResponder(LLEventPumps& pumps, LLSD const& request, std::string const& target,
	                           std::string const& message, std::string const& replyPump, std::string const& errorPump) :
	mPumps(pumps), mReqID(request), mTarget(target), mMessage(message), mReplyPump(replyPump), mErrorPump(errorPump),
	mHTTPTimeoutPolicy(AIHTTPTimeoutPolicy::getTimeoutPolicyByName(std::string()))
{
}

void LLSDMessage::EventResponder::setTimeoutPolicy(std::string const& name)
{
	mHTTPTimeoutPolicy = AIHTTPTimeoutPolicy::getTimeoutPolicyByName(name);
}

void LLSDMessage::EventResponder::httpSuccess(void)
{
    // If our caller passed an empty replyPump name, they're not
    // listening: this is a fire-and-forget message. Don't bother posting
    // to the pump whose name is "".
    if (! mReplyPump.empty())
    {
        LLSD response(mContent);
        mReqID.stamp(response);
        mPumps.obtain(mReplyPump).post(response);
    }
    else                            // default success handling
    {
        LL_INFOS("LLSDMessage::EventResponder")
            << "'" << mMessage << "' to '" << mTarget << "' succeeded"
            << LL_ENDL;
    }
}

void LLSDMessage::EventResponder::httpFailure(void)
{
    // If our caller passed an empty errorPump name, they're not
    // listening: "default error handling is acceptable." Only post to an
    // explicit pump name.
    if (! mErrorPump.empty())
    {
        LLSD info(mReqID.makeResponse());
        info["target"]  = mTarget;
        info["message"] = mMessage;
        info["code"] = mCode;
        info["status"]  = LLSD::Integer(mStatus);
        info["reason"]  = mReason;
        info["content"] = mContent;
        mPumps.obtain(mErrorPump).post(info);
    }
    else                        // default error handling
    {
        // convention seems to be to use llinfos, but that seems a bit casual?
        LL_WARNS("LLSDMessage::EventResponder")
            << "'" << mMessage << "' to '" << mTarget
            << "' failed with code " << mStatus << ": " << mReason << '\n'
            << ll_pretty_print_sd(mContent)
            << LL_ENDL;
    }
}

LLSDMessage::ResponderAdapter::ResponderAdapter(LLHTTPClient::ResponderWithResult* responder,
                                                const std::string& name):
    mResponder(responder),
    mReplyPump(name + ".reply", true), // tweak name for uniqueness
    mErrorPump(name + ".error", true)
{
    mReplyPump.listen("self", boost::bind(&ResponderAdapter::listener, this, _1, true));
    mErrorPump.listen("self", boost::bind(&ResponderAdapter::listener, this, _1, false));
}

std::string LLSDMessage::ResponderAdapter::getTimeoutPolicyName(void) const
{
  return mResponder->getHTTPTimeoutPolicy().name();
}

bool LLSDMessage::ResponderAdapter::listener(const LLSD& payload, bool success)
{
    LLHTTPClient::ResponderWithResult* responder = dynamic_cast<LLHTTPClient::ResponderWithResult*>(mResponder.get());
    // If this assertion fails then ResponderAdapter has been used for a ResponderWithCompleted derived class,
    // which is not allowed because ResponderAdapter can only work for classes derived from Responder that
    // implement httpSuccess() and httpFailure().
    llassert_always(responder);
    if (success)
    {
        responder->successResult(payload);
    }
    else
    {
        responder->failureResult(payload["status"].asInteger(), payload["reason"], payload["content"], (CURLcode)payload["code"].asInteger());
    }

    /*---------------- MUST BE LAST STATEMENT BEFORE RETURN ----------------*/
    delete this;
    // Destruction of mResponder will usually implicitly free its referent as well
    /*------------------------- NOTHING AFTER THIS -------------------------*/
    return false;
}

void LLSDMessage::link()
{
}
