/**
 * @file llcorebufferarray.h
 * @brief Public-facing declaration for the BufferArray scatter/gather class
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 *
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef	_LLCORE_BUFFER_ARRAY_H_
#define	_LLCORE_BUFFER_ARRAY_H_

#include <cstdlib>
#include <vector>

#include "boost/intrusive_ptr.hpp"

#include "llcorerefcounted.h"

namespace LLCore
{

class BufferArrayStreamBuf;

// A very simple scatter/gather type map for bulk data. The motivation for
// this class is the writedata callback used by libcurl. Response bodies are
// delivered to the caller in a sequence of sequential write operations and
// this class captures them without having to reallocate and move data.
//
// The interface looks a little like a unix file descriptor but only just.
// There is a notion of a current position, starting from 0, which is used as
// the position in the data when performing read and write operations. The
// position also moves after various operations:
// - seek(...)
// - read(...)
// - write(...)
// - append(...)
// - appendBufferAlloc(...)
// The object also keeps a total length value which is updated after write and
// append operations and beyond which the current position cannot be set.
//
// Threading: not thread-safe
//
// Allocation: Refcounted, heap only. Caller of the constructor is given a
// single refcount.

class BufferArray : public LLCoreInt::RefCounted
{
public:
	// BufferArrayStreamBuf has intimate knowledge of this implementation to
	// implement a buffer-free adapter. Changes here will likely need to be
	// reflected there.
	friend class BufferArrayStreamBuf;

	BufferArray();
	BufferArray(const BufferArray&) = delete;

	void operator=(const BufferArray&) = delete;

	typedef LLCoreInt::IntrusivePtr<BufferArray> ptr_t;

protected:
	virtual ~BufferArray();				// Use release()

public:
	// Appends the indicated data to the BufferArray modifying current position
	// and total size. New position is one beyond the final byte of the buffer.
	// Returns the count of bytes copied to BufferArray
	size_t append(const void* src, size_t len);

	// Similar to append(), this method guarantees a contiguous block of memory
	// of requested size placed at the current end of the BufferArray.
	// On return, the data in the memory is considered valid whether the caller
	// writes to it or not. Returns a pointer to contiguous region at end of
	// BufferArray of 'len' size.
	void *appendBufferAlloc(size_t len);

	// Current count of bytes in BufferArray instance.
	LL_INLINE size_t size() const				{ return mLen; }

	// Copies data from the given position in the instance to the caller's
	// buffer. Will return a short count of bytes copied if the 'len' extends
	// beyond the data.
	size_t read(size_t pos, void* dst, size_t len);

	// Copies data from the caller's buffer to the instance at the current
	// position. May overwrite existing data, append data when current position
	// is equal to the size of the instance or do a mix of both.
	size_t write(size_t pos, const void* src, size_t len);

protected:
	// Returns -1 when there is no corresponding block.
	S32 findBlock(size_t pos, size_t& ret_offset) const;

	bool getBlockStartEnd(S32 block, const char** start, const char** end);

protected:
	class Block;
	typedef std::vector<Block*> container_t;
	container_t	mBlocks;

	size_t		mLen;
};  // End class BufferArray

}  // End namespace LLCore

#endif	// _LLCORE_BUFFER_ARRAY_H_
