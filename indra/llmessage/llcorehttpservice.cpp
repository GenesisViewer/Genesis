/**
 * @file llcorehttpservice.cpp
 * @brief Internal definitions of the Http service thread
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

#include "llcorehttpservice.h"

#include "llcorehttpinternal.h"
#include "llcorehttplibcurl.h"
#include "llcorehttpoperation.h"
#include "llcorehttppolicy.h"
#include "llcorehttprequestqueue.h"
#include "llcorethread.h"
#include "llthread.h"
#include "lltimer.h"

namespace LLCore
{

const HttpService::OptionDescriptor HttpService::sOptionDesc[] =
{	// isLong, isDynamic, isGlobal, isClass, isCallback
	{ true,		true,	true,	true,	false	},	// PO_CONNECTION_LIMIT
	{ true,		true,	false,	true,	false	},	// PO_PER_HOST_CONNECTION_LIMIT
	{ false,	false,	true,	false,	false	},	// PO_CA_PATH
	{ false,	false,	true,	false,	false	},	// PO_CA_FILE
	{ false,	true,	true,	false,	false	},	// PO_HTTP_PROXY
	{ true,		true,	true,	false,	false	},	// PO_LLPROXY
	{ true,		true,	true,	true,	false	},	// PO_TRACE
	{ true,		true,	false,	true,	false	},	// PO_ENABLE_PIPELINING
	{ true,		true,	false,	true,	false	},	// PO_THROTTLE_RATE
	{ false,	false,	true,	false,	true	}	// PO_SSL_VERIFY_CALLBACK
};

HttpService* HttpService::sInstance = NULL;
volatile HttpService::EState HttpService::sState = NOT_INITIALIZED;

HttpService::HttpService()
:	mRequestQueue(NULL),
	mExitRequested(0U),
	mThread(NULL),
	mPolicy(NULL),
	mTransport(NULL),
	mLastPolicy(0)
{
}

HttpService::~HttpService()
{
	mExitRequested = 1U;
	if (sState == RUNNING)
	{
		// Trying to kill the service object with a running thread is a bit
		// tricky.
		if (mRequestQueue)
		{
			if (mRequestQueue->stopQueue())
			{
				ms_sleep(10);	// Give mRequestQueue a chance to finish
			}
		}

		if (mThread && !mThread->timedJoin(250))
		{
			// Failed to join, expect problems ahead so do a hard termination.
			llwarns << "Destroying HttpService with running thread. Expect problems. Last policy: "
					<< (U32)mLastPolicy << llendl;
			mThread->cancel();
		}
	}

	if (mRequestQueue)
	{
		mRequestQueue->release();
		mRequestQueue = NULL;
	}

	delete mTransport;
	mTransport = NULL;

	delete mPolicy;
	mPolicy = NULL;

	if (mThread)
	{
		mThread->release();
		mThread = NULL;
	}
}

void HttpService::init(HttpRequestQueue* queue)
{
	llassert_always(!sInstance && sState == NOT_INITIALIZED);

	sInstance = new HttpService();

	queue->addRef();
	sInstance->mRequestQueue = queue;
	sInstance->mPolicy = new HttpPolicy(sInstance);
	sInstance->mTransport = new HttpLibcurl(sInstance);
	sState = INITIALIZED;
}

void HttpService::term()
{
	if (sInstance)
	{
		if (sState == RUNNING && sInstance->mThread)
		{
			// Unclean termination. Thread appears to be running. We'll try to
			// give the worker thread a chance to cancel using the exit flag...
			sInstance->mExitRequested = 1U;
			sInstance->mRequestQueue->stopQueue();

			// And a little sleep
			for (S32 i = 0; i < 10 && sState == RUNNING; ++i)
			{
				ms_sleep(100);
			}
		}

		delete sInstance;
		sInstance = NULL;
	}
	sState = NOT_INITIALIZED;
}

HttpRequest::policy_t HttpService::createPolicyClass()
{
	mLastPolicy = mPolicy->createPolicyClass();
	return mLastPolicy;
}

// Threading: callable by consumer thread *once*.
void HttpService::startThread()
{
	llassert_always(!mThread || sState == STOPPED || sState == INITIALIZED);

	if (mThread)
	{
		mThread->release();
	}

	// Push current policy definitions, enable policy & transport components
	mPolicy->start();
	mTransport->start(mLastPolicy + 1);

	mThread = new LLCoreInt::HttpThread(boost::bind(&HttpService::threadRun,
													this, _1));
	sState = RUNNING;
}

// Threading: callable by worker thread.
bool HttpService::changePriority(HttpHandle handle,
								 HttpRequest::priority_t priority)
{
	// Skip the request queue as we currently don't leave earlier requests
	// sitting there. Start with the ready queue...
	bool found = mPolicy->changePriority(handle, priority);

	// If not there, we could try the transport/active queue but priority does
	// not really have much effect there so we don't waste cycles.

	return found;
}

// Tries to find the given request handle on any of the request queues and
// cancels the operation. Returns true if the request was cancelled.
// Threading: callable by the worker thread.
bool HttpService::cancel(HttpHandle handle)
{
	// Request cannot be on request queue so skip that and check the policy
	// component's queues first
	bool cancelled = mPolicy->cancel(handle);

	if (!cancelled)
	{
		// If that did not work, check transport's.
		cancelled = mTransport->cancel(handle);
	}

	return cancelled;
}

// Threading: callable by worker thread.
void HttpService::shutdown()
{
	// Disallow future enqueue of requests
	mRequestQueue->stopQueue();

	// Cancel requests already on the request queue
	HttpRequestQueue::OpContainer ops;
	mRequestQueue->fetchAll(false, ops);

	for (HttpRequestQueue::OpContainer::iterator it = ops.begin(),
												 end = ops.end();
		 it != end; ++it)
	{
		(*it)->cancel();
	}
	ops.clear();

	// Shutdown transport cancelling requests, freeing resources
	mTransport->shutdown();

	// And now policy
	mPolicy->shutdown();
}

// Working thread loop-forever method. Gives time to each of the request queue,
// policy layer and transport layer pieces and then either sleeps for a small
// time or waits for a request to come in. Repeats until requested to stop.
void HttpService::threadRun(LLCoreInt::HttpThread* thread)
{
	boost::this_thread::disable_interruption di;

	int loop = REQUEST_SLEEP;
	while (!mExitRequested)
	{
		loop = (int)processRequestQueue((ELoopSpeed)loop);

		// Process ready queue issuing new requests as needed
		int new_loop = (int)mPolicy->processReadyQueue();
		loop = (std::min)(loop, new_loop);

		// Give libcurl some cycles
		new_loop = mTransport->processTransport();
		loop = (std::min)(loop, new_loop);

		// Determine whether to spin, sleep briefly or sleep for next request
		if (loop != (int)REQUEST_SLEEP)
		{
			ms_sleep(HTTP_SERVICE_LOOP_SLEEP_NORMAL_MS);
		}
	}

	shutdown();
	sState = STOPPED;
}

int HttpService::processRequestQueue(ELoopSpeed loop)
{
	HttpRequestQueue::OpContainer ops;
	const bool wait_for_req = loop == REQUEST_SLEEP;
	mRequestQueue->fetchAll(wait_for_req, ops);

	while (!ops.empty())
	{
		HttpOperation::ptr_t op(ops.front());
		ops.erase(ops.begin());

		// Process operation
		if (!mExitRequested)
		{
			// Setup for subsequent tracing
			long tracing = HTTP_TRACE_OFF;
			mPolicy->getGlobalOptions().get(HttpRequest::PO_TRACE, &tracing);
			op->mTracing = llmax(op->mTracing, tracing);

			if (op->mTracing > HTTP_TRACE_OFF)
			{
				llinfos << "TRACE, FromRequestQueue, Handle: "
						<< op->getHandle() << llendl;
			}

			// Stage
			op->stageFromRequest(this);
		}

		// Done with operation
		op.reset();
	}

	// Queue emptied, allow polling loop to sleep
	return (int)REQUEST_SLEEP;
}

HttpStatus HttpService::getPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										long* ret_value)
{
	if (opt < HttpRequest::PO_CONNECTION_LIMIT	||	// option must be in range
		opt >= HttpRequest::PO_LAST	||				// ditto
		!sOptionDesc[opt].mIsLong ||				// datatype is long
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass))
		// can always get, no dynamic check
	{
		return HttpStatus(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);
	}

	HttpStatus status;
	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.get(opt, ret_value);
	}
	else
	{
		HttpPolicyClass& opts = mPolicy->getClassOptions(pclass);
		status = opts.get(opt, ret_value);
	}

	return status;
}

HttpStatus HttpService::getPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										std::string* ret_value)
{
	HttpStatus status(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);

	if (opt < HttpRequest::PO_CONNECTION_LIMIT ||	// option must be in range
		opt >= HttpRequest::PO_LAST ||				// ditto
		sOptionDesc[opt].mIsLong ||					// datatype is string
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass))
		// can always get, no dynamic check
	{
		return status;
	}

	// Only global has string values
	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.get(opt, ret_value);
	}

	return status;
}

HttpStatus HttpService::getPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										HttpRequest::policyCallback_t* ret_value)
{
	HttpStatus status(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);

	if (opt < HttpRequest::PO_CONNECTION_LIMIT ||	// option must be in range
		opt >= HttpRequest::PO_LAST	||				// ditto
		sOptionDesc[opt].mIsLong ||					// datatype is string
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass))
		// can always get, no dynamic check
	{
		return status;
	}

	// Only global has callback values
	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.get(opt, ret_value);
	}

	return status;
}

HttpStatus HttpService::setPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										long value, long* ret_value)
{
	HttpStatus status(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);

	if (opt < HttpRequest::PO_CONNECTION_LIMIT ||	// option must be in range
		opt >= HttpRequest::PO_LAST ||				// ditto
		!sOptionDesc[opt].mIsLong ||				// datatype is long
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass) ||
		// dynamic setting permitted
		(RUNNING == sState && ! sOptionDesc[opt].mIsDynamic))
	{
		return status;
	}

	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.set(opt, value);
		if (status && ret_value)
		{
			status = opts.get(opt, ret_value);
		}
	}
	else
	{
		HttpPolicyClass& opts = mPolicy->getClassOptions(pclass);
		status = opts.set(opt, value);
		if (status)
		{
			mTransport->policyUpdated(pclass);
			if (ret_value)
			{
				status = opts.get(opt, ret_value);
			}
		}
	}

	return status;
}

HttpStatus HttpService::setPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										const std::string& value,
										std::string* ret_value)
{
	HttpStatus status(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);

	if (opt < HttpRequest::PO_CONNECTION_LIMIT ||	// option must be in range
		opt >= HttpRequest::PO_LAST ||				// ditto
		sOptionDesc[opt].mIsLong ||					// datatype is string
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass) ||
		// dynamic setting permitted
		(sState == RUNNING && !sOptionDesc[opt].mIsDynamic))
	{
		return status;
	}

	// String values are always global (at this time).
	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.set(opt, value);
		if (status && ret_value)
		{
			status = opts.get(opt, ret_value);
		}
	}

	return status;
}

HttpStatus HttpService::setPolicyOption(HttpRequest::EPolicyOption opt,
										HttpRequest::policy_t pclass,
										HttpRequest::policyCallback_t value,
										HttpRequest::policyCallback_t* ret_value)
{
	HttpStatus status(HttpStatus::LLCORE, LLCore::HE_INVALID_ARG);

	if (opt < HttpRequest::PO_CONNECTION_LIMIT ||	// option must be in range
		opt >= HttpRequest::PO_LAST ||				// ditto
		sOptionDesc[opt].mIsLong ||					// datatype is string
		// pclass in valid range
		(pclass != HttpRequest::GLOBAL_POLICY_ID && pclass > mLastPolicy) ||
		// global setting permitted
		(pclass == HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsGlobal) ||
		// class setting permitted
		(pclass != HttpRequest::GLOBAL_POLICY_ID &&
		 !sOptionDesc[opt].mIsClass) ||
		// dynamic setting permitted
		(sState == RUNNING && !sOptionDesc[opt].mIsDynamic))
	{
		return status;
	}

	// Callbacks values are always global (at this time).
	if (pclass == HttpRequest::GLOBAL_POLICY_ID)
	{
		HttpPolicyGlobal& opts = mPolicy->getGlobalOptions();
		status = opts.set(opt, value);
		if (status && ret_value)
		{
			status = opts.get(opt, ret_value);
		}
	}

	return status;
}

}  // End namespace LLCore
