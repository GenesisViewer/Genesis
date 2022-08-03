/**
 * @file llcorebufferarray.cpp
 * @brief Implements the BufferArray scatter/gather buffer
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

#include "linden_common.h"

#include "llcorebufferarray.h"

// BufferArray is a list of chunks, each a BufferArray::Block, of contiguous
// data presented as a single array. Chunks are at least BLOCK_ALLOC_SIZE in
// length and can be larger. Any chunk may be partially filled or even empty.
//
// The BufferArray itself is sharable as a RefCounted entity. As shared reads
// do not work with the concept of a current position/seek value, none is kept
// with the object. Instead, the read and write operations all take position
// arguments. Single write/shared read is not supported directly and any such
// attempts have to be serialized outside of this implementation.

namespace LLCore
{

///////////////////////////////////////////////////////////////////////////////
// BufferArray::Block sub-class
///////////////////////////////////////////////////////////////////////////////

class BufferArray::Block
{
public:
	Block(const Block&) = delete;
	void operator=(const Block&) = delete;

	Block(size_t len)
	:	mAllocated(len),
		mUsed(0)
	{
		mDataPtr = (char*)malloc(len);
		if (!mDataPtr)
		{
			llerrs << "Failure to allocate a new memory block of "
				   << len << " bytes !" << llendl;
		}
		memset(mDataPtr, 0, len);
	}

	LL_INLINE ~Block() noexcept
	{
		free((void*)mDataPtr);
	}

public:
	char*	mDataPtr;
	size_t	mAllocated;
	size_t	mUsed;
};

///////////////////////////////////////////////////////////////////////////////
// BufferArray class
///////////////////////////////////////////////////////////////////////////////

constexpr size_t BLOCK_ALLOC_SIZE = 131072;

BufferArray::BufferArray()
:	LLCoreInt::RefCounted(true),
	mLen(0)
{
	// Allow to allocate up to 4 Mb before having to grow mBlocks
	mBlocks.reserve(32);
}

BufferArray::~BufferArray()
{
	for (container_t::iterator it = mBlocks.begin(), end = mBlocks.end();
		 it != end; ++it)
	{
		delete *it;
		*it = NULL;
	}
	mBlocks.clear();
}

size_t BufferArray::append(const void* src, size_t len)
{
	const size_t ret = len;
	const char* c_src = (const char*)src;

	// First, try to copy into the last block
	if (len && !mBlocks.empty())
	{
		Block& last = *mBlocks.back();
		if (last.mUsed < last.mAllocated)
		{
			// Some will fit...
			const size_t copy_len = llmin(len, last.mAllocated - last.mUsed);
			memcpy(last.mDataPtr + last.mUsed, c_src, copy_len);
			last.mUsed += copy_len;
			mLen += copy_len;
			c_src += copy_len;
			len -= copy_len;
		}
	}

	// Then get new blocks as needed
	while (len)
	{
		const size_t copy_len = llmin(len, BLOCK_ALLOC_SIZE);
		Block* block = new Block(BLOCK_ALLOC_SIZE);
		memcpy(block->mDataPtr, c_src, copy_len);
		block->mUsed = copy_len;
		mBlocks.push_back(block);
		mLen += copy_len;
		c_src += copy_len;
		len -= copy_len;
	}

	return ret;
}

// If someone asks for zero-length, we give them a valid pointer.
void* BufferArray::appendBufferAlloc(size_t len)
{
	Block* block = new Block(llmax(BLOCK_ALLOC_SIZE, len));
	block->mUsed = len;
	mBlocks.push_back(block);
	mLen += len;
	return block->mDataPtr;
}

size_t BufferArray::read(size_t pos, void* dst, size_t len)
{
	if (pos >= mLen)
	{
		return 0;
	}

	len = llmin(len, mLen - pos);
	if (!len)
	{
		return 0;
	}

	size_t offset = 0;
	S32 block_start = findBlock(pos, offset);
	if (block_start < 0)
	{
		return 0;
	}

	size_t result = 0;

	char* c_dst = (char*)dst;
	S32 block_limit = mBlocks.size();
	do
	{
		Block& block = *mBlocks[block_start++];
		size_t block_len = llmin(block.mUsed - offset, len);
		memcpy(c_dst, block.mDataPtr + offset, block_len);
		result += block_len;
		len -= block_len;
		c_dst += block_len;
		offset = 0;
	}
	while (len > 0 && block_start < block_limit);

	return result;
}

size_t BufferArray::write(size_t pos, const void* src, size_t len)
{
	if (pos > mLen || len == 0)
	{
		return 0;
	}

	size_t result = 0;
	const char* c_src = (const char*)src;

	size_t offset = 0;
	S32 block_start = findBlock(pos, offset);
	if (block_start >= 0)
	{
		S32 block_limit = mBlocks.size();
		// Some or all of the write will be on top of existing data.
		do
		{
			Block& block = *mBlocks[block_start++];
			size_t block_len = llmin(block.mUsed - offset, len);
			memcpy(block.mDataPtr + offset, c_src, block_len);
			result += block_len;
			c_src += block_len;
			len -= block_len;
			offset = 0;
		}
		while (len > 0 && block_start < block_limit);
	}

	// Something left, see if it will fit in the free space of the last block.
	if (len && !mBlocks.empty())
	{
		Block& last = *mBlocks.back();
		if (last.mUsed < last.mAllocated)
		{
			// Some will fit...
			const size_t copy_len = llmin(len, last.mAllocated - last.mUsed);
			memcpy(last.mDataPtr + last.mUsed, c_src, copy_len);
			last.mUsed += copy_len;
			result += copy_len;
			mLen += copy_len;
			c_src += copy_len;
			len -= copy_len;
		}
	}

	if (len)
	{
		// Some or all of the remaining write data will be an append.
		result += append(c_src, len);
	}

	return result;
}

// Note: returns a signed integer and not a size_t (which is usually unsigned),
// so that we can return -1 when no block is found.
S32 BufferArray::findBlock(size_t pos, size_t& ret_offset) const
{
	ret_offset = 0;
	if (pos >= mLen)
	{
		return -1;		// Does not exist
	}

	for (size_t i = 0, block_limit = mBlocks.size(); i < block_limit; ++i)
	{
		if (pos < mBlocks[i]->mUsed)
		{
			ret_offset = pos;
			return i;
		}
		pos -= mBlocks[i]->mUsed;
	}

	// Should not get here but...
	return -1;
}

bool BufferArray::getBlockStartEnd(S32 block, const char** start,
								   const char** end)
{
	if (block < 0 || (size_t)block >= mBlocks.size())
	{
		return false;
	}

	const Block& b = *mBlocks[block];
	*start = b.mDataPtr;
	*end = b.mDataPtr + b.mUsed;
	return true;
}

}  // End namespace LLCore
