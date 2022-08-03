/**
 * @file llcorehttpresponse.h
 * @brief Public-facing declarations for the HttpResponse class
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

#ifndef	_LLCORE_HTTP_RESPONSE_H_
#define	_LLCORE_HTTP_RESPONSE_H_

#include <string>

#include "llcorehttpcommon.h"
#include "llcorehttpheaders.h"
#include "llcorerefcounted.h"

namespace LLCore
{

class BufferArray;

// HttpResponse is instantiated by the library and handed to the caller during
// callbacks to the handler. It supplies all the status, header and HTTP body
// data the caller is interested in. Methods provide simple getters to return
// individual pieces of the response.
//
// Typical usage will have the caller interrogate the object and return from
// the handler callback and then simply returning. But instances are refcounted
// and and callers can add a reference and hold onto the object after the
// callback.
//
// Threading: Not intrinsically thread-safe.
//
// Allocation: Refcounted, heap only. Caller of the constructor is given a
// refcount.

class HttpResponse final : public LLCoreInt::RefCounted
{
protected:
	virtual ~HttpResponse();				// Use release()

public:
	HttpResponse();

	HttpResponse(const HttpResponse&) = delete;
	void operator=(const HttpResponse&) = delete;

	// Returns the final status of the requested operation.
	LL_INLINE HttpStatus getStatus() const				{ return mStatus; }
	LL_INLINE void setStatus(const HttpStatus& s)		{ mStatus = s; }

	// Simple getter for the response body returned as a scatter/gather buffer.
	// If the operation doesn't produce data (such as the Null or StopThread
	// operations), this may be NULL.
	//
	// Caller can hold onto the response by incrementing the reference count of
	// the returned object.
	LL_INLINE BufferArray* getBody() const				{ return mBufferArray; }

	// Safely get the size of the body buffer. If the body buffer is missing
	// return 0 as the size.
	size_t getBodySize() const;

	// Set the response data in the instance. Will drop the reference count to
	// any existing data and increment the count of that passed in. It is legal
	// to set the data to NULL.
	void setBody(BufferArray* ba);

	// And a getter for the headers. And as with getResponse(), if headers are
	// not available because the operation doesn't produce any or delivery of
	// headers wasn't requested in the options, this will be NULL.
	//
	// Caller can hold onto the headers by incrementing the reference count of
	// the returned object.
	LL_INLINE HttpHeaders::ptr_t getHeaders() const		{ return mHeaders; }

	// Behaves like @see setResponse() but for header data.
	LL_INLINE void setHeaders(HttpHeaders::ptr_t& hdr)	{ mHeaders = hdr; }

	// If a 'Range:' header was used, these methods are involved in setting and
	// returning data about the actual response. If both @offset and @length
	// are returned as 0, we probably didn't get a Content-Range header in the
	// response. This occurs with various Capabilities-based services and the
	// caller is going to have to make assumptions on receipt of a 206 status.
	// The @full value may also be zero in cases of parsing problems or a wild-
	// carded length response.
	//
	// These values will not necessarily agree with the data in the body itself
	// (if present). The BufferArray object is authoritative for actual data
	// length.
	LL_INLINE void getRange(unsigned int* offset, unsigned int* length,
							unsigned int* full) const
	{
		*offset = mReplyOffset;
		*length = mReplyLength;
		*full = mReplyFullLength;
	}

	LL_INLINE void setRange(unsigned int offset, unsigned int length,
							unsigned int full_length)
	{
		mReplyOffset = offset;
		mReplyLength = length;
		mReplyFullLength = full_length;
	}

	LL_INLINE const std::string& getContentType() const	{ return mContentType; }
	LL_INLINE void setContentType(const std::string& t)	{ mContentType = t; }

	// Get and set retry attempt information on the request.
	LL_INLINE void getRetries(unsigned int* retries,
							  unsigned int* retries_503) const
	{
		if (retries)
		{
			*retries = mRetries;
		}
		if (retries_503)
		{
			*retries_503 = m503Retries;
		}
	}

	LL_INLINE void setRetries(unsigned int retries, unsigned int retries_503)
	{
		mRetries = retries;
		m503Retries = retries_503;
	}

	LL_INLINE void setRequestURL(const std::string& u)	{ mRequestUrl = u; }
	LL_INLINE const std::string& getRequestURL() const	{ return mRequestUrl; }

	// Statistics for the HTTP 
	struct TransferStats
	{
		typedef std::shared_ptr<TransferStats> ptr_t;

		TransferStats()
		:	mSizeDownload(0.0),
			mTotalTime(0.0),
			mSpeedDownload(0.0)
		{
		}

		F64 mSizeDownload;
		F64 mTotalTime;
		F64 mSpeedDownload;
	};

	LL_INLINE void setTransferStats(TransferStats::ptr_t& stats)
	{
		mStats = stats;
	}

	LL_INLINE TransferStats::ptr_t getTransferStats()	{ return mStats; }

protected:
	HttpStatus				mStatus;
	unsigned int			mReplyOffset;
	unsigned int			mReplyLength;
	unsigned int			mReplyFullLength;
	unsigned int			mRetries;
	unsigned int			m503Retries;
	BufferArray*			mBufferArray;
	HttpHeaders::ptr_t		mHeaders;
	std::string				mContentType;
	std::string				mRequestUrl;

	TransferStats::ptr_t	mStats;
};

}   // End namespace LLCore

#endif	// _LLCORE_HTTP_RESPONSE_H_
