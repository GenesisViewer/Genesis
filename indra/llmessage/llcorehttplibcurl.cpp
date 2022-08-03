/**
 * @file llcorehttplibcurl.cpp
 * @brief Internal definitions of the Http libcurl thread
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012-2013, Linden Research, Inc.
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

#include "llcorehttplibcurl.h"

#include "llcorebufferarray.h"
#include "llcorehttpheaders.h"
#include "llcorehttpoprequest.h"
#include "llcorehttppolicy.h"
#include "llcoremutex.h"
#include "llhttpstatuscodes.h"

namespace
{
// Error testing and reporting for libcurl status codes
void check_curl_multi_code(CURLMcode code);
void check_curl_multi_code(CURLMcode code, int curl_setopt_option);
LLCoreInt::HttpMutex sStatMutex;
}

namespace LLCore
{
U64 HttpLibcurl::sDownloadedBytes = 0;
U64 HttpLibcurl::sUploadedBytes = 0;

HttpLibcurl::HttpLibcurl(HttpService* service)
:	mService(service),
	mPolicyCount(0),
	mMultiHandles(NULL),
	mActiveHandles(NULL),
	mDirtyPolicy(NULL)
{
}

HttpLibcurl::~HttpLibcurl()
{
	shutdown();
	mService = NULL;
}

void HttpLibcurl::shutdown()
{
	while (!mActiveOps.empty())
	{
		HttpOpRequest::ptr_t op(*mActiveOps.begin());
		mActiveOps.hset_erase(mActiveOps.begin());

		cancelRequest(op);
	}

	if (mMultiHandles)
	{
		for (int policy_class = 0; policy_class < mPolicyCount; ++policy_class)
		{
			CURLM* multi_handle(mMultiHandles[policy_class]);
			if (multi_handle)
			{
				curl_multi_cleanup(multi_handle);
				mMultiHandles[policy_class] = NULL;
			}
		}

		delete[] mMultiHandles;
		mMultiHandles = NULL;

		delete[] mActiveHandles;
		mActiveHandles = NULL;

		delete[] mDirtyPolicy;
		mDirtyPolicy = NULL;
	}

	mPolicyCount = 0;
}

void HttpLibcurl::start(int policy_count)
{
	llassert_always(policy_count <= HTTP_POLICY_CLASS_LIMIT);
	llassert_always(!mMultiHandles);	// One-time call only

	mPolicyCount = policy_count;
	mMultiHandles = new CURLM*[mPolicyCount];
	mActiveHandles = new int[mPolicyCount];
	mDirtyPolicy = new bool[mPolicyCount];

	for (int policy_class = 0; policy_class < mPolicyCount; ++policy_class)
	{
		if ((mMultiHandles[policy_class] = curl_multi_init()) != NULL)
		{
			mActiveHandles[policy_class] = 0;
			mDirtyPolicy[policy_class] = false;
			policyUpdated(policy_class);
		}
		else
		{
			llwarns << "Failed to allocate multi handle in libcurl." << llendl;
			llassert(false);
		}
	}
}

HttpService::ELoopSpeed HttpLibcurl::processTransport()
{
	HttpService::ELoopSpeed	ret(HttpService::REQUEST_SLEEP);

	if (!mMultiHandles)
	{
		return ret;
	}

	// Give libcurl some cycles to do I/O & callbacks
	for (int policy_class = 0; policy_class < mPolicyCount; ++policy_class)
	{
		CURLM* multi_handle = mMultiHandles[policy_class];
		if (!multi_handle)
		{
			// *HACK: there used to be nothing but 'continue', here, but
			// Apple's clang wrongly optimized out (at either -O2 or -O3) the
			// NULL check, causing the resulting binary to crash !
			LL_DEBUGS("CoreHttp") << "NULL multi-handle found for policy class: "
								  << policy_class << LL_ENDL;
			// No handle, nothing to do.
			continue;
		}

		if (!mActiveHandles[policy_class])
		{
			// If we have gone quiet and there is a dirty update, apply it,
			// otherwise we are done.
			if (mDirtyPolicy[policy_class])
			{
				policyUpdated(policy_class);
			}
			continue;
		}

		int running;
		CURLMcode status(CURLM_CALL_MULTI_PERFORM);
		do
		{
			running = 0;
			status = curl_multi_perform(multi_handle, &running);
		}
		while (running != 0 && CURLM_CALL_MULTI_PERFORM == status);

		// Run completion on anything done
		CURLMsg* msg = NULL;
		int msgs_in_queue = 0;
		while ((msg = curl_multi_info_read(multi_handle, &msgs_in_queue)))
		{
			if (msg->msg == CURLMSG_DONE)
			{
				CURL* handle = msg->easy_handle;
				CURLcode result = msg->data.result;

				completeRequest(multi_handle, handle, result);
				handle = NULL;			// No longer valid on return
				// If anything completes, we may have a free slot.
				ret = HttpService::NORMAL;
				// Turning around quickly reduces connection gap by 7-10ms.
			}
			else if (msg->msg != CURLMSG_NONE)
			{
				LL_WARNS_ONCE() << "Unexpected message from libcurl. Msg code: "
							 << msg->msg << LL_ENDL;
			}
			msgs_in_queue = 0;
		}
	}

	if (!mActiveOps.empty())
	{
		ret = HttpService::NORMAL;
	}
	return ret;
}

// Caller has provided us with a ref count on op.
void HttpLibcurl::addOp(const HttpOpRequest::ptr_t& op)
{
	llassert_always((int)op->mReqPolicy < mPolicyCount &&
					mMultiHandles[op->mReqPolicy] != NULL);

	// Create standard handle
	if (!op->prepareRequest(mService))
	{
		// Could not issue request, fail with notification
		// *TODO: Need failure path
		return;
	}

	// Make the request live
	CURLMcode code = curl_multi_add_handle(mMultiHandles[op->mReqPolicy],
										   op->mCurlHandle);
	if (code != CURLM_OK)
	{
		// *TODO: Better cleanup and recovery but not much we can do here.
		check_curl_multi_code(code);
		return;
	}
	op->mCurlActive = true;
	mActiveOps.emplace(op);
	++mActiveHandles[op->mReqPolicy];

	if (op->mTracing > HTTP_TRACE_OFF)
	{
		HttpPolicy& policy(mService->getPolicy());
		llinfos << "TRACE, ToActiveQueue, Handle: " << op->getHandle()
				<< ". Actives: " << mActiveOps.size() << ". Ready: "
				<< policy.getReadyCount(op->mReqPolicy) << llendl;
	}
}

// Implements the transport part of any cancel operation. See if the handle is
// an active operation and if so, use the more complicated transport-based
// cancellation method to kill the request.
bool HttpLibcurl::cancel(HttpHandle handle)
{
	HttpOpRequest::ptr_t op = HttpOpRequest::fromHandle<HttpOpRequest>(handle);
	active_set_t::iterator it = mActiveOps.find(op);
	if (it == mActiveOps.end())
	{
		return false;
	}

	// Cancel request
	cancelRequest(op);

	// Drop references
	mActiveOps.hset_erase(it);
	--mActiveHandles[op->mReqPolicy];

	return true;
}

// *NOTE: cancelRequest logic parallels completeRequest logic. Keep them
// synchronized as necessary. Caller is expected to remove the op from the
// active list and release the op *after* calling this method. It must be
// called first to deliver the op to the reply queue with refcount intact.
void HttpLibcurl::cancelRequest(const HttpOpRequest::ptr_t& op)
{
	// Deactivate request
	op->mCurlActive = false;

	// Detach from multi and recycle handle
	curl_multi_remove_handle(mMultiHandles[op->mReqPolicy], op->mCurlHandle);
	mHandleCache.freeHandle(op->mCurlHandle);
	op->mCurlHandle = NULL;

	// Tracing
	if (op->mTracing > HTTP_TRACE_OFF)
	{
		llinfos << "TRACE, request cancelled, Handle: " << op->getHandle()
				<< ". Status: " << op->mStatus.toTerseString() << llendl;
	}

	// Cancel op and deliver for notification
	op->cancel();
}

// *NOTE: cancelRequest logic parallels completeRequest logic. Keep them
// synchronized as necessary.
bool HttpLibcurl::completeRequest(CURLM* multi_handle, CURL* handle,
								  CURLcode status)
{
	if (!handle)
	{
		llwarns << "Attempt to retrieve status from a NULL handle. Aborted."
				<< llendl;
		llassert(false);
		return false;
	}

	double dbytes = 0;
	double ubytes = 0;
	CURLcode ccode = curl_easy_getinfo(handle, CURLINFO_SIZE_DOWNLOAD, &dbytes);
	CURLcode ccode2 = curl_easy_getinfo(handle, CURLINFO_SIZE_UPLOAD, &ubytes);
	if (ccode == CURLE_OK || ccode2 == CURLE_OK)
	{
		LLCoreInt::HttpScopedLock lock(sStatMutex);
		sDownloadedBytes += dbytes;
		sUploadedBytes += ubytes;
	}

	HttpHandle ophandle = NULL;
	ccode = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &ophandle);
	if (ccode != CURLE_OK)
	{
		llwarns << "libcurl error: " << ccode
				<< ". Unable to retrieve operation handle from CURL handle."
				<< llendl;
		return false;
	}

	HttpOpRequest::ptr_t op(HttpOpRequest::fromHandle<HttpOpRequest>(ophandle));
	if (!op)
	{
		llwarns << "Unable to locate operation by handle. May have expired."
				<< llendl;
		return false;
	}

	if (handle != op->mCurlHandle || !op->mCurlActive)
	{
		llwarns << "libcurl handle and HttpOpRequest handle in disagreement or inactive request. Handle: "
				<< static_cast<HttpHandle>(handle) << llendl;
		return false;
	}

	active_set_t::iterator it = mActiveOps.find(op);
	if (it == mActiveOps.end())
	{
		llwarns << "libcurl completion for request not on active list. Continuing. Handle: "
				<< static_cast<HttpHandle>(handle) << llendl;
		return false;
	}

	// Deactivate request
	mActiveOps.hset_erase(it);
	--mActiveHandles[op->mReqPolicy];
	op->mCurlActive = false;

	// Set final status of request if it has not failed by other mechanisms yet
	if (op->mStatus)
	{
		op->mStatus = HttpStatus(HttpStatus::EXT_CURL_EASY, status);
	}
	if (op->mStatus)
	{
		long http_status = HTTP_OK;
		ccode = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE,
								  &http_status);
		if (ccode == CURLE_OK)
		{
			if (http_status >= 100 && http_status <= 999)
			{
				char* cont_type = NULL;
				ccode = curl_easy_getinfo(handle, CURLINFO_CONTENT_TYPE,
										  &cont_type);
				if (ccode != CURLE_OK)
				{
					llwarns << "CURL error #" << ccode
							<< " while attempting to get content type. Handle: "
							<< static_cast<HttpHandle>(handle) << llendl;
				}
				else if (cont_type)
				{
					op->mReplyConType = cont_type;
				}

				op->mStatus = HttpStatus(http_status);
			}
			else
			{
				llwarns << "Invalid HTTP response code (" << http_status
						<< ") received from server." << llendl;
				op->mStatus = HttpStatus(HttpStatus::LLCORE,
										 HE_INVALID_HTTP_STATUS);
			}
		}
		else
		{
			llwarns << "CURL error #" << ccode
					<< " while attempting to get response code. Handle: "
					<< static_cast<HttpHandle>(handle) << llendl;
			op->mStatus = HttpStatus(HttpStatus::LLCORE,
									 HE_INVALID_HTTP_STATUS);
		}
	}

	// Detach from multi and recycle handle.
	if (multi_handle)
	{
		curl_multi_remove_handle(multi_handle, handle);
	}
	else
	{
		llwarns << "Curl multi_handle is NULL on remove. Handle: "
				<< static_cast<HttpHandle>(handle) << llendl;
	}
	mHandleCache.freeHandle(op->mCurlHandle);
	op->mCurlHandle = NULL;

	// Tracing
	if (op->mTracing > HTTP_TRACE_OFF)
	{
		llinfos << "TRACE, RequestComplete, Handle: " << op->getHandle()
				<< ". Status: " << op->mStatus.toTerseString() << llendl;
	}

	// Dispatch to next stage
	HttpPolicy& policy(mService->getPolicy());

	return policy.stageAfterCompletion(op); // true if still active
}

int HttpLibcurl::getActiveCountInClass(int policy_class) const
{
	llassert(policy_class >= 0 && policy_class < mPolicyCount);
	if (mActiveHandles && policy_class >= 0 && policy_class < mPolicyCount)
	{
		return mActiveHandles[policy_class];
	}
	else
	{
		return 0;
	}
}

void HttpLibcurl::policyUpdated(int policy_class)
{
	if (policy_class < 0 || policy_class >= mPolicyCount || ! mMultiHandles)
	{
		return;
	}

	HttpPolicy& policy(mService->getPolicy());

	if (!mActiveHandles[policy_class])
	{
		// Clear to set options. As of libcurl 7.37.0, if a pipelining multi-
		// handle has active requests and you try to set the multi-handle to
		// non-pipelining, the library gets very angry and goes off the rails
		// corrupting memory. A clue that you are about to crash is that you'll
		// get a missing server response error (curl code 9). So, if options
		// are to be set, we let the multi handle run out of requests, then set
		// options, and re-enable request processing.
		//
		// All of this stall mechanism exists for this reason. If libcurl
		// becomes more resilient later, it should be possible to remove all of
		// this. The connection limit settings are fine, it's just that
		// pipelined-to-non-pipelined transition that is fatal at the moment.

		HttpPolicyClass& options(policy.getClassOptions(policy_class));
		CURLM* multi_handle(mMultiHandles[policy_class]);
		CURLMcode code;

		// Enable policy if stalled
		policy.stallPolicy(policy_class, false);
		mDirtyPolicy[policy_class] = false;

		if (options.mPipelining > 1)
		{
			// We will try to do pipelining on this multihandle
			code = curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, 1L);
			check_curl_multi_code(code, CURLMOPT_PIPELINING);
			code = curl_multi_setopt(multi_handle,
									 CURLMOPT_MAX_PIPELINE_LENGTH,
									 long(options.mPipelining));
			check_curl_multi_code(code, CURLMOPT_MAX_PIPELINE_LENGTH);
			code = curl_multi_setopt(multi_handle,
									 CURLMOPT_MAX_HOST_CONNECTIONS,
									 long(options.mPerHostConnectionLimit));
			check_curl_multi_code(code, CURLMOPT_MAX_HOST_CONNECTIONS);
			code = curl_multi_setopt(multi_handle,
									 CURLMOPT_MAX_TOTAL_CONNECTIONS,
									 long(options.mConnectionLimit));
			check_curl_multi_code(code, CURLMOPT_MAX_TOTAL_CONNECTIONS);
		}
		else
		{
			code = curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, 0L);
			check_curl_multi_code(code, CURLMOPT_PIPELINING);
			code = curl_multi_setopt(multi_handle,
									 CURLMOPT_MAX_HOST_CONNECTIONS, 0L);
			check_curl_multi_code(code, CURLMOPT_MAX_HOST_CONNECTIONS);
			code = curl_multi_setopt(multi_handle,
									 CURLMOPT_MAX_TOTAL_CONNECTIONS,
									 long(options.mConnectionLimit));
			check_curl_multi_code(code, CURLMOPT_MAX_TOTAL_CONNECTIONS);
		}
	}
	else if (!mDirtyPolicy[policy_class])
	{
		// Mark policy dirty and request a stall in the policy. When policy
		// goes idle, we will re-invoke this method and perform the change.
		// Do not allow this thread to sleep while we are waiting for
		// quiescence, we will just stop processing.
		mDirtyPolicy[policy_class] = true;
		policy.stallPolicy(policy_class, true);
	}
}

// ---------------------------------------
// HttpLibcurl::HandleCache
// ---------------------------------------

HttpLibcurl::HandleCache::HandleCache()
:	mHandleTemplate(NULL)
{
	mCache.reserve(50);
}

HttpLibcurl::HandleCache::~HandleCache()
{
	if (mHandleTemplate)
	{
		curl_easy_cleanup(mHandleTemplate);
		mHandleTemplate = NULL;
	}

	for (handle_cache_t::iterator it = mCache.begin(), end = mCache.end();
		 it != end; ++it)
	{
		curl_easy_cleanup(*it);
	}
	mCache.clear();
}

CURL* HttpLibcurl::HandleCache::getHandle()
{
	CURL* ret = NULL;

	if (!mCache.empty())
	{
		// Fastest path to handle
		ret = mCache.back();
		mCache.pop_back();
	}
	else if (mHandleTemplate)
	{
		// Still fast path
		ret = curl_easy_duphandle(mHandleTemplate);
	}
	else
	{
		// When all else fails
		ret = curl_easy_init();
	}

	return ret;
}

void HttpLibcurl::HandleCache::freeHandle(CURL* handle)
{
	if (!handle)
	{
		return;
	}

	curl_easy_reset(handle);
	if (!mHandleTemplate)
	{
		// Save the first freed handle as a template.
		mHandleTemplate = handle;
	}
	else
	{
		// Otherwise add it to the cache
		if (mCache.size() >= mCache.capacity())
		{
			mCache.reserve(mCache.capacity() + 50);
		}
		mCache.push_back(handle);
	}
}

// ---------------------------------------
// Free functions
// ---------------------------------------

struct curl_slist* append_headers_to_slist(const HttpHeaders::ptr_t& headers,
										   struct curl_slist* slist)
{
	static const char sep[] = ": ";
	std::string header;
	header.reserve(128);	// Should be more than enough

	for (HttpHeaders::const_iterator it = headers->begin(),
									 end = headers->end();
		 it != end; ++it)
	{
		header = it->first + sep + it->second;
		slist = curl_slist_append(slist, header.c_str());
	}

	return slist;
}

}  // End namespace LLCore

namespace
{

void check_curl_multi_code(CURLMcode code, int curl_setopt_option)
{
	if (code != CURLM_OK)
	{
		llwarns << "libcurl multi error detected: "
				<< curl_multi_strerror(code)
				<< " - curl_multi_setopt option: "
				<< curl_setopt_option << llendl;
	}
}

void check_curl_multi_code(CURLMcode code)
{
	if (code != CURLM_OK)
	{
		llwarns << "libcurl multi error detected: "
				<< curl_multi_strerror(code) << llendl;
	}
}

}  // end anonymous namespace
