/**
 * @file llcorehttpopcancel.h
 * @brief Internal declarations for the HttpOpCancel subclass
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

#ifndef	_LLCORE_HTTP_OPCANCEL_H_
#define	_LLCORE_HTTP_OPCANCEL_H_

#include "llcorehttpcommon.h"
#include "llcorehttpoperation.h"

namespace LLCore
{

// HttpOpCancel requests that a previously issued request be cancelled, if
// possible. This includes active requests that may be in the middle of an
// HTTP transaction. Any completed request will not be cancelled and will
// return its final status unchanged and *this* request will complete with an
// HE_HANDLE_NOT_FOUND error status.

class HttpOpCancel final : public HttpOperation
{
public:
	// @param	handle	Handle of previously-issued request to be cancelled.
	HttpOpCancel(HttpHandle handle);

public:
	void stageFromRequest(HttpService*) override;

public:
	HttpHandle mHandle;
};

}   // End namespace LLCore

#endif	// _LLCORE_HTTP_OPCANCEL_H_
