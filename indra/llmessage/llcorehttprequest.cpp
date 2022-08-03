/**
 * @file llcorehttprequest.cpp
 * @brief Implementation of the HTTPRequest class
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

#include "llcorehttprequest.h"

#include "llcorehttpopcancel.h"
#include "llcorehttpoperation.h"
#include "llcorehttpoprequest.h"
#include "llcorehttpopsetget.h"
#include "llcorehttpopsetpriority.h"
#include "llcorehttppolicy.h"
#include "llcorehttpreplyqueue.h"
#include "llcorehttprequestqueue.h"
#include "llcorehttpservice.h"
#include "lltimer.h"

namespace LLCore
{

//static
bool HttpRequest::sInitialized = false;

static const HttpStatus sStatusNotDynamic(HttpStatus::LLCORE,
										  HE_OPT_NOT_DYNAMIC);

// ====================================
// HttpRequest Implementation
// ====================================

HttpRequest::HttpRequest()
:	mCancelled(false)
{
	mRequestQueue = HttpRequestQueue::instanceOf();
	mRequestQueue->addRef();

	mReplyQueue.reset(new HttpReplyQueue());
}

HttpRequest::~HttpRequest()
{
	if (mRequestQueue)
	{
		mRequestQueue->release();
		mRequestQueue = NULL;
	}

	mReplyQueue.reset();
}

// ====================================
// Policy Methods
// ====================================

HttpRequest::policy_t HttpRequest::createPolicyClass()
{
	if (HttpService::instanceOf()->getState() == HttpService::RUNNING)
	{
		return 0;
	}
	return HttpService::instanceOf()->createPolicyClass();
}

HttpStatus HttpRequest::setStaticPolicyOption(EPolicyOption opt,
											  policy_t pclass,
											  long value, long* ret_value)
{
	if (HttpService::instanceOf()->getState() == HttpService::RUNNING)
	{
		return sStatusNotDynamic;
	}
	return HttpService::instanceOf()->setPolicyOption(opt, pclass, value,
													  ret_value);
}

HttpStatus HttpRequest::setStaticPolicyOption(EPolicyOption opt,
											  policy_t pclass,
											  const std::string& value,
											  std::string* ret_value)
{
	if (HttpService::instanceOf()->getState() == HttpService::RUNNING)
	{
		return sStatusNotDynamic;
	}
	return HttpService::instanceOf()->setPolicyOption(opt, pclass, value,
													  ret_value);
}

HttpStatus HttpRequest::setStaticPolicyOption(EPolicyOption opt,
											  policy_t pclass,
											  policyCallback_t value,
											  policyCallback_t* ret_value)
{
	if (HttpService::instanceOf()->getState() == HttpService::RUNNING)
	{
		return sStatusNotDynamic;
	}
	return HttpService::instanceOf()->setPolicyOption(opt, pclass, value,
													  ret_value);
}

HttpHandle HttpRequest::setPolicyOption(EPolicyOption opt, policy_t pclass,
										long value, HttpHandler::ptr_t handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpSetGet::ptr_t op(new HttpOpSetGet());

	HttpStatus status = op->setupSet(opt, pclass, value);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::setPolicyOption(EPolicyOption opt, policy_t pclass,
										const std::string& value,
										HttpHandler::ptr_t handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpSetGet::ptr_t op(new HttpOpSetGet());

	HttpStatus status = op->setupSet(opt, pclass, value);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

// ====================================
// Request Methods
// ====================================

HttpHandle HttpRequest::requestGet(policy_t policy_id, priority_t priority,
								   const std::string& url,
								   const HttpOptions::ptr_t& options,
								   const HttpHeaders::ptr_t& headers,
								   const HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupGet(policy_id, priority, url, options,
									 headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestGetByteRange(policy_t policy_id,
											priority_t priority,
											const std::string& url,
											size_t offset, size_t len,
											const HttpOptions::ptr_t& options,
											const HttpHeaders::ptr_t& headers,
											HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupGetByteRange(policy_id, priority, url, offset,
											  len, options, headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestPost(policy_t policy_id, priority_t priority,
									const std::string& url, BufferArray* body,
									const HttpOptions::ptr_t& options,
									const HttpHeaders::ptr_t& headers,
									HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupPost(policy_id, priority, url, body, options,
									  headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestPut(policy_t policy_id, priority_t priority,
								   const std::string& url, BufferArray* body,
								   const HttpOptions::ptr_t& options,
								   const HttpHeaders::ptr_t& headers,
								   HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupPut(policy_id, priority, url, body, options,
									 headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestDelete(policy_t policy_id, priority_t priority,
									  const std::string& url,
									  const HttpOptions::ptr_t& options,
									  const HttpHeaders::ptr_t& headers,
									  HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupDelete(policy_id, priority, url, options,
										headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestPatch(policy_t policy_id, priority_t priority,
									 const std::string& url, BufferArray* body,
									 const HttpOptions::ptr_t& options,
									 const HttpHeaders::ptr_t& headers,
									 HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupPatch(policy_id, priority, url, body, options,
									   headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestCopy(policy_t policy_id, priority_t priority,
									const std::string& url,
									const HttpOptions::ptr_t& options,
									const HttpHeaders::ptr_t& headers,
									HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupCopy(policy_id, priority, url, options,
									  headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestMove(policy_t policy_id, priority_t priority,
									const std::string& url,
									const HttpOptions::ptr_t& options,
									const HttpHeaders::ptr_t& headers,
									HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOpRequest::ptr_t op(new HttpOpRequest());

	HttpStatus status = op->setupMove(policy_id, priority, url, options,
									  headers);
	if (!status)
	{
		mLastReqStatus = status;
		return handle;
	}

	op->setReplyPath(mReplyQueue, user_handler);

	status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestNoOp(HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOperation::ptr_t op(new HttpOpNull());
	op->setReplyPath(mReplyQueue, user_handler);

	HttpStatus status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpStatus HttpRequest::update(long usecs)
{
	if (mLastReqStatus == gStatusCancelled || !mReplyQueue)
	{
		return gStatusCancelled;
	}

	HttpOperation::ptr_t op;

	if (usecs)
	{
		const HttpTime limit(LLTimer::totalTime() + HttpTime(usecs));
		while (limit >= LLTimer::totalTime() && (op = mReplyQueue->fetchOp()))
		{
			// Process operation
			op->visitNotifier(this);

			// We are done with the operation
			op.reset();
		}
	}
	else
	{
		// Same as above, just no time limit
		HttpReplyQueue::OpContainer replies;
		mReplyQueue->fetchAll(replies);
		if (!replies.empty())
		{
			for (HttpReplyQueue::OpContainer::iterator iter = replies.begin(),
													   iend = replies.end();
				 iter != iend; ++iter)
			{
				// Swap op pointer for NULL;
				op.reset();
				op.swap(*iter);
				if (op)
				{
					// Process operation
					op->visitNotifier(this);
				}
			}
			op.reset();
		}
	}

	return HttpStatus();
}

// ====================================
// Request Management Methods
// ====================================

HttpHandle HttpRequest::requestCancel(HttpHandle request,
									  HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	mCancelled = true;

	HttpOperation::ptr_t op(new HttpOpCancel(request));
	op->setReplyPath(mReplyQueue, user_handler);

	HttpStatus status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

HttpHandle HttpRequest::requestSetPriority(HttpHandle request,
										   priority_t priority,
										   HttpHandler::ptr_t handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOperation::ptr_t op(new HttpOpSetPriority(request, priority));
	op->setReplyPath(mReplyQueue, handler);

	HttpStatus status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

// ====================================
// Utility Methods
// ====================================

HttpStatus HttpRequest::createService()
{
	HttpStatus status;

	if (!sInitialized)
	{
		sInitialized = true;
		HttpRequestQueue::init();
		HttpRequestQueue* rq = HttpRequestQueue::instanceOf();
		HttpService::init(rq);
	}

	return status;
}

HttpStatus HttpRequest::destroyService()
{
	HttpStatus status;

	if (sInitialized)
	{
		HttpService::term();
		HttpRequestQueue::term();
		sInitialized = false;
	}

	return status;
}

HttpStatus HttpRequest::startThread()
{
	HttpService::instanceOf()->startThread();

	HttpStatus status;
	return status;
}

HttpHandle HttpRequest::requestStopThread(HttpHandler::ptr_t user_handler)
{
	HttpHandle handle(LLCORE_HTTP_HANDLE_INVALID);

	HttpOperation::ptr_t op(new HttpOpStop());
	op->setReplyPath(mReplyQueue, user_handler);

	HttpStatus status = mRequestQueue->addOp(op);	// transfers refcount
	if (status)
	{
		handle = op->getHandle();
	}
	mLastReqStatus = status;

	return handle;
}

}   // End namespace LLCore
