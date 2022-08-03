/**
 * @file llcorehttpopcancel.cpp
 * @brief Definitions for internal class HttpOpCancel
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

#include "linden_common.h"

#include "curl/curl.h"

#include "llcorehttpopcancel.h"

#include "llcorehttphandler.h"
#include "llcorehttpresponse.h"
#include "llcorehttpservice.h"

namespace LLCore
{

HttpOpCancel::HttpOpCancel(HttpHandle handle)
:	HttpOperation(),
	mHandle(handle)
{
}

// Immediately searches for the request on various queues and cancels
// operations if found. Returns the status of the search and cancel as the
// status of this request. The cancelled request will return a cancelled status
// to its handler.
void HttpOpCancel::stageFromRequest(HttpService* service)
{
	if (!service || !service->cancel(mHandle))
	{
		mStatus = HttpStatus(HttpStatus::LLCORE, HE_HANDLE_NOT_FOUND);
	}

	addAsReply();
}

}   // End namespace LLCore
