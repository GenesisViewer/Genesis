/**
 * @file llcorehttppolicy.h
 * @brief Declarations for internal class enforcing policy decisions.
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

#ifndef	_LLCORE_HTTP_POLICY_H_
#define	_LLCORE_HTTP_POLICY_H_

#include "llcorehttpinternal.h"
#include "llcorehttpoprequest.h"
#include "llcorehttppolicyclass.h"
#include "llcorehttppolicyglobal.h"
#include "llcorehttprequest.h"
#include "llcorehttpretryqueue.h"
#include "llcorehttpservice.h"
#include "llerror.h"

namespace LLCore
{

// HttpReadyQueue provides a simple priority queue for HttpOpRequest objects.
//
// This uses the priority_queue adaptor class to provide the queue as well as
// the ordering scheme while allowing us access to the raw container if we
// follow a few simple rules. One of the more important of those rules is that
// any iterator becomes invalid on element erasure. So pay attention.
//
// If LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY tests true, the class implements
// a std::priority_queue interface but on std::deque behavior to eliminate
// sensitivity to priority. In the future, this will likely become the only
// behavior or it may become a run-time election.
//
// Threading: not thread-safe. Expected to be used entirely by a single thread,
// typically a worker thread of some sort.

#if LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

typedef std::deque<HttpOpRequest::ptr_t> HttpReadyQueueBase;

#else

typedef std::priority_queue<HttpOpRequest::ptr_t,
							std::deque<HttpOpRequest::ptr_t>,
							LLCore::HttpOpRequestCompare> HttpReadyQueueBase;

#endif // LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

class HttpReadyQueue final : public HttpReadyQueueBase
{
public:
	HttpReadyQueue() = default;
	~HttpReadyQueue() = default;

	HttpReadyQueue(const HttpReadyQueue&) = delete;
	void operator=(const HttpReadyQueue&) = delete;

public:
#if LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY
	// Types and methods needed to make a std::deque look more like a
	// std::priority_queue, at least for our purposes.
	typedef HttpReadyQueueBase container_type;

	LL_INLINE const_reference top() const					{ return front(); }
	LL_INLINE void pop()									{ pop_front(); }
	LL_INLINE void push(const value_type& v)				{ emplace_back(v); }

#endif // LLCORE_HTTP_READY_QUEUE_IGNORES_PRIORITY

	LL_INLINE const container_type& get_container() const	{ return *this; }
	LL_INLINE container_type& get_container()				{ return *this;	}

}; // End class HttpReadyQueue

// Implements class-based queuing policies for an HttpService instance.
//
// Threading: Single-threaded. Other than for construction/destruction, all
// methods are expected to be invoked in a single thread, typically a worker
// thread of some sort.

class HttpPolicy
{
protected:
	LOG_CLASS(HttpPolicy);

public:
	HttpPolicy(HttpService*);
	virtual ~HttpPolicy();

	HttpPolicy(const HttpPolicy&) = delete;
	void operator=(const HttpPolicy&) = delete;

	typedef std::shared_ptr<HttpOpRequest> opReqPtr_t;

	// Threading: called by init thread.
	HttpRequest::policy_t createPolicyClass();

	// Cancel all ready and retry requests sending them to their notification
	// queues. Release state resources making further request handling
	// impossible.
	//
	// Threading: called by worker thread
	void shutdown();

	// Deliver policy definitions and enable handling of requests. One-time
	// call invoked before starting the worker thread.
	//
	// Threading: called by application thread
	void start();

	// Give the policy layer some cycles to scan the ready queue promoting
	// higher-priority requests to active as permited.
	//
	// Returns an indication of how soon this method should be called again.
	//
	// Threading: called by worker thread
	int processReadyQueue();

	// Add request to a ready queue. Caller is expected to have provided us
	// with a reference count to hold the request (no additional references
	// will be added).
	//
	// OpRequest is owned by the request queue after this call and should not
	// be modified by anyone until retrieved from queue.
	//
	// Threading: called by worker thread
	void addOp(const opReqPtr_t& op);

	// Similar to addOp, used when a caller wants to retry a request that has
	// failed. It's placed on a special retry queue but ordered by retry time
	// not priority. Otherwise, handling is the same and retried operations are
	// considered before new ones but that doesn't guarantee completion order.
	//
	// Threading: called by worker thread
	void retryOp(const opReqPtr_t& op);

	// Attempt to change the priority of an earlier request.
	// Request that Shadows HttpService's method
	//
	// Threading: called by worker thread
	bool changePriority(HttpHandle handle, HttpRequest::priority_t priority);

	// Attempts to cancel a previous request.
	// Shadows HttpService's method as well.
	//
	// Threading: called by worker thread
	bool cancel(HttpHandle handle);

	// When transport is finished with an op and takes it off the active queue,
	// it is delivered here for dispatch. Policy may send it back to the ready/
	//retry queues if it needs another go or we may finalize it and send it on
	// to the reply queue.
	//
	// @return	Returns true of the request is still active or ready after
	//			staging, false if has been sent on to the reply queue.
	//
	// Threading: called by worker thread
	bool stageAfterCompletion(const opReqPtr_t& op);

	// Get a reference to global policy options. Caller is expected to do
	// context checks like no setting once running.
	//
	// Threading: called by any thread *but* the object may only be modified by
	// the worker thread once running.
	//
	LL_INLINE HttpPolicyGlobal& getGlobalOptions()
	{
		return mGlobalOptions;
	}

	// Get a reference to class policy options. Caller is expected to do
	// context checks like no setting once running. These are done, for
	// example, in @see HttpService interfaces.
	//
	// Threading: called by any thread *but* the object may only be modified by
	// the worker thread once running and read accesses by other threads are
	// exposed to races at that point.
	HttpPolicyClass& getClassOptions(HttpRequest::policy_t pclass);

	// Get ready counts for a particular policy class
	//
	// Threading: called by worker thread
	int getReadyCount(HttpRequest::policy_t policy_class) const;

	// Stall (or unstall) a policy class preventing requests from transitioning
	// to an active state. Used to allow an HTTP request policy to empty prior
	// to changing settings or state that isn't tolerant of changes when work
	// is outstanding.
	//
	// Threading:	called by worker thread
	bool stallPolicy(HttpRequest::policy_t policy_class, bool stall);

protected:
	struct ClassState;
	typedef std::vector<ClassState*> class_list_t;

	HttpPolicyGlobal					mGlobalOptions;
	// Naked pointer, not refcounted, not owner:
	HttpService*						mService;
	class_list_t						mClasses;
};

}  // End namespace LLCore

#endif // _LLCORE_HTTP_POLICY_H_
