/**
* @file llcorehttpoperation.h
* @brief Internal declarations for HttpOperation and sub-classes
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

#ifndef	_LLCORE_HTTP_OPERATION_H_
#define	_LLCORE_HTTP_OPERATION_H_

#include "llcorehttpcommon.h"
#include "llcorehttprequest.h"
#include "llcoremutex.h"
#include "llerror.h"

namespace LLCore
{

class HttpReplyQueue;
class HttpHandler;
class HttpService;

// HttpOperation is the base class for all request/reply pairs.
//
// Operations are expected to be of two types: immediate and queued. Immediate
// requests go to the singleton request queue and when picked up by the worker
// thread are executed immediately and there results placed on the supplied
// reply queue. Queued requests (namely for HTTP operations), go to the
// request queue, are picked up and moved to a ready queue where they are
// ordered by priority and managed by the policy component, are then activated
// issuing HTTP requests and moved to an active list managed by the transport
// (libcurl) component and eventually finalized when a response is available
// and status and data return via reply queue.
//
// To manage these transitions, derived classes implement three methods:
// stageFromRequest, stageFromReady and stageFromActive. Immediate requests
// will only override stageFromRequest which will perform the operation and
// return the result by invoking addAsReply() to put the request on a reply
// queue. Queued requests will involve all three stage methods.
//
// Threading: not thread-safe. Base and derived classes provide no locking.
// Instances move across threads via queue-like interfaces that are thread
// compatible and those interfaces establish the access rules.

class HttpOperation : public std::enable_shared_from_this<HttpOperation>
{
protected:
	LOG_CLASS(HttpOperation);

public:
	typedef std::shared_ptr<HttpOperation> ptr_t;
	typedef std::weak_ptr<HttpOperation> wptr_t;
	typedef std::shared_ptr<HttpReplyQueue> HttpReplyQueuePtr_t;

	// Threading: called by a consumer thread.
	HttpOperation();

	// Threading: called by any thread.
	virtual ~HttpOperation();							// Use release()

	// Non-copyable
	HttpOperation(const HttpOperation&) = delete;
	HttpOperation& operator=(const HttpOperation&) = delete;

public:
	// Register a reply queue and a handler for completion notifications.
	//
	// Invokers of operations that want to receive notification that an
	// operation has been completed do so by binding a reply queue and a
	// handler object to the request.
	//
	// @param	reply_queue		Pointer to the reply queue where completion
	//							notifications are to be queued (typically by
	//							addAsReply()). This will typically be the reply
	//							queue referenced by the request object. This
	//							method will increment the refcount on the queue
	//							holding the queue until delivery is complete.
	//							Using a reply_queue even if the handler is NULL
	//							has some benefits for memory deallocation by
	//							keeping it in the originating thread.
	//
	// @param	handler			Possibly NULL pointer to a non-refcounted
	//							handler object to be invoked (onCompleted) when
	//							the operation is finished. Note that the
	//							handler object is never dereferenced by the
	//							worker thread. This is passible data until
	//							notification is performed.
	//
	// Threading: called by consumer thread.
	//
	void setReplyPath(HttpReplyQueuePtr_t reply_queue,
					  HttpHandler::ptr_t handler);

	// The three possible staging steps in an operation's lifecycle.
	// Asynchronous requests like HTTP operations move from the request queue
	// to the ready queue via stageFromRequest. Then from the ready queue to
	// the active queue by stageFromReady. And when complete, to the reply
	// queue via stageFromActive and the addAsReply utility.
	//
	// Immediate mode operations (everything else) move from the request queue
	// to the reply queue directly via stageFromRequest and addAsReply with no
	// existence on the ready or active queues.
	//
	// These methods will take out a reference count on the request, caller
	// only needs to dispose of its reference when done with the request.
	//
	// Threading: called by worker thread.
	//
	virtual void stageFromRequest(HttpService*);
	virtual void stageFromReady(HttpService*);
	virtual void stageFromActive(HttpService*);

	// Delivers a notification to a handler object on completion.
	//
	// Once a request is complete and it has been removed from its reply queue,
	// a handler notification may be delivered by a call to
	// HttpRequest::update(). This method does the necessary dispatching.
	//
	// Threading: called by consumer thread.
	//
	virtual void visitNotifier(HttpRequest*);

	// Cancels the operation whether queued or active. The final status of the
	// request becomes cancelled (an error) and that will be delivered to
	// caller via notification scheme.
	//
	// Threading: called by worker thread.
	//
	virtual HttpStatus cancel();

	// Retrieves a unique handle for this operation.
	HttpHandle getHandle();

	template<class OPT>
	static std::shared_ptr<OPT> fromHandle(HttpHandle handle)
	{
		ptr_t ptr = findByHandle(handle);
		if (!ptr)
		{
			return std::shared_ptr<OPT>();
		}
		return std::dynamic_pointer_cast<OPT>(ptr);
	}

protected:
	// Delivers request to reply queue on completion. After this call, worker
	// thread no longer accesses the object and it is owned by the reply queue.
	//
	// Threading: called by worker thread.
	//
	void addAsReply();

	static ptr_t findByHandle(HttpHandle handle);

private:
    HttpHandle createHandle();
    void destroyHandle();

public:
	// Request Data
	HttpRequest::policy_t		mReqPolicy;
	HttpRequest::priority_t		mReqPriority;

	// Reply Data
	HttpStatus					mStatus;

	// Tracing, debug and metrics
	HttpTime					mMetricCreated;
	long						mTracing;

protected:
	HttpReplyQueuePtr_t			mReplyQueue;
	HttpHandler::ptr_t			mUserHandler;

private:
	HttpHandle					mMyHandle;

	typedef std::map<HttpHandle, wptr_t> handleMap_t;
	static handleMap_t			sHandleMap;
    static LLCoreInt::HttpMutex	sOpMutex;
};

// HttpOpStop requests the servicing thread to shutdown operations, cease
// pulling requests from the request queue and release shared resources
// (particularly those shared via reference count). The servicing thread will
// then exit. The underlying thread object remains so that another thread can
// join on the servicing thread prior to final cleanup. The request *does*
// generate a reply on the response queue, if requested.

class HttpOpStop final : public HttpOperation
{
public:
	HttpOpStop();

	HttpOpStop(const HttpOpStop&) = delete;
	void operator=(const HttpOpStop&) = delete;

public:
	void stageFromRequest(HttpService*) override;
};

// HttpOpNull is a do-nothing operation used for testing via a basic loopback
// pattern. It is executed immediately by the servicing thread which bounces a
// reply back to the caller without any further delay.

class HttpOpNull final : public HttpOperation
{
public:
	HttpOpNull();

private:
	HttpOpNull(const HttpOpNull&) = delete;
	void operator=(const HttpOpNull&) = delete;

public:
	void stageFromRequest(HttpService*) override;
};

}   // End namespace LLCore

#endif	// _LLCORE_HTTP_OPERATION_H_
