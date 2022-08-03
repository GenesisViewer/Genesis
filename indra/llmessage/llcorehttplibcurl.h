/**
 * @file llcorehttplibcurl.h
 * @brief Declarations for internal class providing libcurl transport.
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

#ifndef	_LLCORE_HTTP_LIBCURL_H_
#define	_LLCORE_HTTP_LIBCURL_H_

#include <vector>

#include "curl/curl.h"				// For CURL typedef
#include "curl/multi.h"				// For CURLM typedef

#include "llcorehttpinternal.h"
#include "llcorehttprequest.h"
#include "llcorehttpservice.h"
#include "llerror.h"
#include "llfastset.h"

namespace LLCore
{

class HttpPolicy;
class HttpOpRequest;
class HttpHeaders;

// Implements libcurl-based transport for an HttpService instance.
//
// Threading: Single-threaded. Other than for construction/destruction, all
// methods are expected to be invoked in a single thread, typically a worker
// thread of some sort.

class HttpLibcurl
{
protected:
	LOG_CLASS(HttpLibcurl);

public:
	HttpLibcurl(HttpService* service);
	virtual ~HttpLibcurl();

	HttpLibcurl(const HttpLibcurl&) = delete;
	void operator=(const HttpLibcurl&) = delete;

	typedef std::shared_ptr<HttpOpRequest> opReqPtr_t;

	// Give cycles to libcurl to run active requests. Completed operations
	// (successful or failed) will be retried or handed over to the reply queue
	// as final responses.
	//
	// @return	Indication of how long this method is willing to wait for next
	//			service call.
	//
	// Threading: called by worker thread.
	HttpService::ELoopSpeed processTransport();

	// Add request to the active list. Caller is expected to have provided us
	// with a reference count on the op to hold the request (no additional
	// reference will be added).
	void addOp(const opReqPtr_t& op);

	// One-time call to set the number of policy classes to be serviced and to
	// create the resources for each. Value must agree with
	// HttpPolicy::setPolicies() call.
	//
	// Threading: called by init thread.
	void start(int policy_count);

	// Synchronously stop libcurl operations. All active requests are cancelled
	// and removed from libcurl's handling. Easy handles are detached from
	// their multi handles and released. Multi handles are also released.
	// Cancelled requests are completed with cancelled status and made
	// available on their respective reply queues.
	//
	// Can be restarted with a start() call.
	//
	// Threading: called by worker thread.
	void shutdown();

	// Return global and per-class counts of active requests.
	LL_INLINE int getActiveCount() const		{ return mActiveOps.size(); }
	int getActiveCountInClass(int policy_class) const;

	// Attempt to cancel a request identified by handle.
	//
	// Interface shadows HttpService's method.
	//
	// @return	True if handle was found and operation cancelled.
	//
	// Threading: called by worker thread.
	bool cancel(HttpHandle handle);

	// Informs transport that a particular policy class has had options changed
	// and so should affect any transport state change necessary to affect
	// those changes. Used mainly for initialization and dynamic option
	// setting.
	//
	// Threading: called by worker thread.
	void policyUpdated(int policy_class);

	// Allocate a curl handle for caller. May be freed using either the
	// freeHandle() method or calling curl_easy_cleanup() directly.
	//
	// @return	Libcurl handle (CURL *) or NULL on allocation problem.
	//			Handle will be in curl_easy_reset() condition.
	//
	// Threading:	callable by worker thread.
	//
	// Deprecation:	Expect this to go away after llcorehttpoprequest is
	//				refactored bringing code into this class.
	LL_INLINE CURL* getHandle()					{ return mHandleCache.getHandle(); }

	static LL_INLINE U64 getDownloadedBytes()	{ return sDownloadedBytes; }
	static LL_INLINE U64 getUploadedBytes()		{ return sUploadedBytes; }

protected:
	// Invoked when libcurl has indicated a request has been processed to
	// completion and we need to move the request to a new state.
	bool completeRequest(CURLM* multi_handle, CURL* handle, CURLcode status);

	// Invoked to cancel an active request, mainly during shutdown and destroy.
	void cancelRequest(const opReqPtr_t& op);

	// Simple request handle cache for libcurl.
	//
	// Handle creation is somewhat slow and chunky in libcurl and there's a
	// pretty good speedup to be had from handle re-use. So, a simple vector is
	// kept of 'freed' handles to be reused as needed. When that is empty, the
	// first freed handle is kept as a template for handle duplication. This is
	// still faster than creation from nothing. And when that fails, we init
	// fresh from curl_easy_init().
	//
	// Handles allocated with getHandle() may be freed with either freeHandle()
	// or curl_easy_cleanup(). Choice may be dictated by thread constraints.
	//
	// Threading:	Single-threaded. May only be used by a single thread,
	//				typically the worker thread. If freeing requests' handles
	//				in an unknown threading context, use curl_easy_cleanup()
	//				for safety.

	class HandleCache
	{
	public:
		HandleCache();
		~HandleCache();

		HandleCache(const HandleCache&) = delete;
		void operator=(const HandleCache&) = delete;

		// Allocate a curl handle for caller. May be freed using either the
		// freeHandle() method or calling curl_easy_cleanup() directly.
		//
		// @return	Libcurl handle (CURL*) or NULL on allocation problem.
		//
		// Threading:	Single-thread (worker) only.
		CURL* getHandle();

		// Free a libcurl handle acquired by whatever means. Thread safety is
		// left to the caller.
		//
		// Threading:	Single-thread (worker) only.
		void freeHandle(CURL* handle);

	protected:
		typedef std::vector<CURL*> handle_cache_t;

	protected:
		CURL*			mHandleTemplate;	// Template for duplicating new handles
		handle_cache_t	mCache;				// Cache of old handles
	};

protected:
	HttpService*		mService;		// Simple reference, not owner
	HandleCache			mHandleCache;	// Handle allocator, owner

	typedef safe_hset<opReqPtr_t> active_set_t;
	active_set_t		mActiveOps;

	int					mPolicyCount;
	CURLM**				mMultiHandles;	// One handle per policy class
	int*				mActiveHandles;	// Active count per policy class
	bool*				mDirtyPolicy;	// Dirty policy update waiting for stall (per pc)

	static U64			sDownloadedBytes;
	static U64			sUploadedBytes;
};

}  // End namespace LLCore

#endif // _LLCORE_HTTP_LIBCURL_H_
