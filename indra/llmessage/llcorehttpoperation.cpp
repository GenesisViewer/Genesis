/**
 * @file llcorehttpoperation.cpp
 * @brief Definitions for internal classes based on HttpOperation
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

#include "llcorehttpoperation.h"

#include "llcorehttphandler.h"
#include "llcorehttpinternal.h"
#include "llcorehttpreplyqueue.h"
#include "llcorehttprequest.h"
#include "llcorehttprequestqueue.h"
#include "llcorehttpresponse.h"
#include "llcorehttpservice.h"
#include "lltimer.h"

namespace LLCore
{

// ==================================
// HttpOperation
// ==================================

// Statics
HttpOperation::handleMap_t HttpOperation::sHandleMap;
LLCoreInt::HttpMutex HttpOperation::sOpMutex;

HttpOperation::HttpOperation()
:	std::enable_shared_from_this<HttpOperation>(),
	mReqPolicy(HttpRequest::DEFAULT_POLICY_ID),
	mReqPriority(0U),
	mTracing(HTTP_TRACE_OFF),
	mMyHandle(LLCORE_HTTP_HANDLE_INVALID)
{
	mMetricCreated = LLTimer::totalTime();
}

HttpOperation::~HttpOperation()
{
	destroyHandle();
	mReplyQueue.reset();
	mUserHandler.reset();
}

void HttpOperation::setReplyPath(HttpReplyQueue::ptr_t reply_queue,
								 HttpHandler::ptr_t user_handler)
{
	mReplyQueue.swap(reply_queue);
	mUserHandler.swap(user_handler);
}

void HttpOperation::stageFromRequest(HttpService*)
{
	// Default implementation should never be called. This indicates an
	// operation making a transition that is not defined.
	llerrs << "Default stageFromRequest method may not be called." << llendl;
}

void HttpOperation::stageFromReady(HttpService*)
{
	// Default implementation should never be called. This indicates an
	// operation making a transition that is not defined.
	llerrs << "Default stageFromReady method may not be called." << llendl;
}

void HttpOperation::stageFromActive(HttpService*)
{
	// Default implementation should never be called. This indicates an
	// operation making a transition that is not defined.
	llerrs << "Default stageFromActive method may not be called." << llendl;
}

void HttpOperation::visitNotifier(HttpRequest*)
{
	if (mUserHandler)
	{
		HttpResponse* response = new HttpResponse();
		response->setStatus(mStatus);
		mUserHandler->onCompleted(getHandle(), response);
		response->release();
	}
}

HttpStatus HttpOperation::cancel()
{
	HttpStatus status;
	return status;
}

// Handle methods

HttpHandle HttpOperation::getHandle()
{
	return mMyHandle != LLCORE_HTTP_HANDLE_INVALID ? mMyHandle : createHandle();
}

HttpHandle HttpOperation::createHandle()
{
	HttpHandle handle = static_cast<HttpHandle>(this);
	{
		LLCoreInt::HttpScopedLock lock(sOpMutex);

		sHandleMap.emplace(handle, shared_from_this());
		mMyHandle = handle;
	}
	return mMyHandle;
}

void HttpOperation::destroyHandle()
{
	if (mMyHandle != LLCORE_HTTP_HANDLE_INVALID)
	{
		LLCoreInt::HttpScopedLock lock(sOpMutex);

		handleMap_t::iterator it = sHandleMap.find(mMyHandle);
		if (it != sHandleMap.end())
		{
			sHandleMap.erase(it);
		}
	}
}

//static
HttpOperation::ptr_t HttpOperation::findByHandle(HttpHandle handle)
{
	if (handle == LLCORE_HTTP_HANDLE_INVALID)
	{
		return ptr_t();
	}

	wptr_t weak;

	{
		LLCoreInt::HttpScopedLock lock(sOpMutex);

		handleMap_t::iterator it = sHandleMap.find(handle);
		if (it == sHandleMap.end())
		{
			llwarns << "Could not find operation for handle " << handle
					<< llendl;
			return ptr_t();
		}

		weak = it->second;
	}

	if (!weak.expired())
	{
		return weak.lock();
	}

	return ptr_t();
}

void HttpOperation::addAsReply()
{
	if (mTracing > HTTP_TRACE_OFF)
	{
		llinfos << "TRACE, ToReplyQueue, Handle: " << getHandle() << llendl;
	}

	if (mReplyQueue)
	{
		HttpOperation::ptr_t op = shared_from_this();
		mReplyQueue->addOp(op);
	}
}

// ==================================
// HttpOpStop
// ==================================

HttpOpStop::HttpOpStop()
:	HttpOperation()
{
}

void HttpOpStop::stageFromRequest(HttpService* service)
{
	if (service)
	{
		// Do operations
		service->stopRequested();

		// Prepare response if needed
		addAsReply();
	}
	else
	{
		llwarns << "NULL service passed" << llendl;
		llassert(false);
	}
}

// ==================================
// HttpOpNull
// ==================================

HttpOpNull::HttpOpNull()
:	HttpOperation()
{
}

void HttpOpNull::stageFromRequest(HttpService*)
{
	// Perform op/ Nothing to perform. This does not fall into the libcurl
	// ready/active queues, it just bounces over to the reply queue directly.

	// Prepare response if needed
	addAsReply();
}

}   // End namespace LLCore
