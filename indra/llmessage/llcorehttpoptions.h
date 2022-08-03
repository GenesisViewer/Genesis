/**
 * @file llcorehttpoptions.h
 * @brief Public-facing declarations for the HTTPOptions class
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

#ifndef	_LLCORE_HTTP_OPTIONS_H_
#define	_LLCORE_HTTP_OPTIONS_H_

#include "llcorehttpcommon.h"

// Convenient shortcut
#define DEFAULT_HTTP_OPTIONS std::make_shared<LLCore::HttpOptions>()

namespace LLCore
{

// Really a struct in spirit, it provides options that modify HTTP requests.
//
// Sharing instances across requests. It is intended that these be shared
// across requests: caller can create one of these, set it up as needed and
// then reference it repeatedly in HTTP operations. But see the Threading note
// about references.
//
// Threading: While this class does nothing to ensure thread safety, it *is*
// intended to be shared between the application thread and the worker thread.
// This means that once an instance is delivered to the library in request
// operations, the option data must not be written until all such requests
// complete and relinquish their references.
//
// Allocation: Refcounted, heap only. Caller of the constructor is given a
// refcount.

class HttpOptions
{
public:
	typedef std::shared_ptr<HttpOptions> ptr_t;

	HttpOptions();

	// Non-copyable
	HttpOptions(const HttpOptions&) = delete;
	HttpOptions& operator=(const HttpOptions&) = delete;

public:
	// Default: false
	LL_INLINE void setWantHeaders(bool wanted)			{ mWantHeaders = wanted; }
	LL_INLINE bool getWantHeaders() const				{ return mWantHeaders; }

	// Default: 0
	LL_INLINE void setTrace(S32 level)					{ mTracing = level; }
	LL_INLINE S32 getTrace() const						{ return mTracing; }

	// Default: 30
	LL_INLINE void setTimeout(U32 timeout)				{ mTimeout = timeout; }
	LL_INLINE U32 getTimeout() const					{ return mTimeout; }

	// Default: 0
	LL_INLINE void setTransferTimeout(U32 t)			{ mTransferTimeout = t; }
	LL_INLINE U32 getTransferTimeout() const			{ return mTransferTimeout; }

	// Sets the number of retries on an LLCore::HTTPRequest before the request
	// fails. Default: 8
	LL_INLINE void setRetries(U32 retries)				{ mRetries = retries; }
	LL_INLINE U32 getRetries() const					{ return mRetries; }

	// Default: true
	LL_INLINE void setUseRetryAfter(bool use_retry)		{ mUseRetryAfter = use_retry; }
	LL_INLINE bool getUseRetryAfter() const				{ return mUseRetryAfter; }

	// Instructs the LLCore::HTTPRequest to follow redirects. Default: false
	LL_INLINE void setFollowRedirects(bool follow)		{ mFollowRedirects = follow; }
	LL_INLINE bool getFollowRedirects() const			{ return mFollowRedirects; }

	// Instructs the LLCore::HTTPRequest to verify that the exchanged security
	// certificate is authentic. Default: false
	LL_INLINE void setSSLVerifyPeer(bool verify)		{ mVerifyPeer = verify; }
	LL_INLINE bool getSSLVerifyPeer() const				{ return mVerifyPeer; }

	// Instructs the LLCore::HTTPRequest to verify that the name in the
	// security certificate matches the name of the host contacted.
	// Default: false
	LL_INLINE void setSSLVerifyHost(bool verify)		{ mVerifyHost = verify; }
	LL_INLINE bool getSSLVerifyHost() const				{ return mVerifyHost; }

	// Sets the time for DNS name caching in seconds. Setting this value to 0
	// will disable name caching. Setting this value to -1 causes the name
	// cache to never time out. Default: -1
	void setDNSCacheTimeout(S32 timeout)				{ mDNSCacheTimeout = timeout; }
	LL_INLINE S32 getDNSCacheTimeout() const			{ return mDNSCacheTimeout; }

	// Retrieve only the headers and status from the request. Setting this to
	// true implies setWantHeaders(true) as well. Default: false
	LL_INLINE void setHeadersOnly(bool nobody)
	{
		mNoBody = nobody;
		if (mNoBody)
		{
			mWantHeaders = true;
		}
	}

	LL_INLINE bool getHeadersOnly() const				{ return mNoBody; }

protected:
	S32				mTracing;
	S32				mDNSCacheTimeout;
	U32				mTimeout;
	U32				mTransferTimeout;
	U32				mRetries;
	bool			mWantHeaders;
	bool			mUseRetryAfter;
	bool			mFollowRedirects;
	bool			mVerifyPeer;
	bool			mVerifyHost;
	bool			mNoBody;
};

}	// End namespace LLCore

#endif	// _LLCORE_HTTP_OPTIONS_H_
