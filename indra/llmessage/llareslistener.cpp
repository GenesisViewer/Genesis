/**
 * @file   llareslistener.cpp
 * @author Nat Goodspeed
 * @date   2009-03-18
 * @brief  Implementation for llareslistener.
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
#include "llareslistener.h"
// STL headers
// std headers
// external library headers
// other Linden headers
#include "llares.h"
#include "llerror.h"
#include "llevents.h"
#include "llsdutil.h"

LLAresListener::LLAresListener(LLAres* llares):
    LLEventAPI("LLAres",
               "LLAres listener to request DNS operations"),
    mAres(llares)
{
    // add() every method we want to be able to invoke via this event API.
    // Optional last parameter validates expected LLSD request structure.
    add("rewriteURI",
        "Given [\"uri\"], return on [\"reply\"] an array of alternative URIs.\n"
        "On failure, returns an array containing only the original URI, so\n"
        "failure case can be processed like success case.",
        &LLAresListener::rewriteURI,
        LLSD().with("uri", LLSD()).with("reply", LLSD()));
}

/// This UriRewriteResponder subclass packages returned URIs as an LLSD
/// array to send back to the requester.
class UriRewriteResponder: public LLAres::UriRewriteResponder
{
public:
    /**
     * Specify the request, containing the event pump name on which to send
     * the reply.
     */
    UriRewriteResponder(const LLSD& request):
        mReqID(request),
        mPumpName(request["reply"])
    {}

    /// Called by base class with results. This is called in both the
    /// success and error cases. On error, the calling logic passes the
    /// original URI.
    virtual void rewriteResult(const std::vector<std::string>& uris)
    {
        LLSD result;
        for (std::vector<std::string>::const_iterator ui(uris.begin()), uend(uris.end());
             ui != uend; ++ui)
        {
            result.append(*ui);
        }
        // This call knows enough to avoid trying to insert a map key into an
        // LLSD array. It's there so that if, for any reason, we ever decide
        // to change the response from array to map, it will Just Start Working.
        mReqID.stamp(result);
        LLEventPumps::instance().obtain(mPumpName).post(result);
    }

private:
    LLReqID mReqID;
    const std::string mPumpName;
};

void LLAresListener::rewriteURI(const LLSD& data)
{
	if (mAres)
	{
		mAres->rewriteURI(data["uri"], new UriRewriteResponder(data));
	}
	else
	{
		LL_INFOS() << "LLAresListener::rewriteURI requested without Ares present. Ignoring: " << data << LL_ENDL;
	}
}
