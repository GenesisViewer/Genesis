/**
 * @file llcorehttpheaders.cpp
 * @brief Implementation of the HTTPHeaders class
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

#include "llcorehttpheaders.h"

namespace LLCore
{

void HttpHeaders::append(const std::string& name, const char* value)
{
	for (S32 i = 0, count = mHeaders.size(); i < count; ++i)
	{
		if (mHeaders[i].first == name)
		{
			// This does happen in "normal" conditions...
			if (strcmp(mHeaders[i].second.c_str(), value) == 0)
			{
				return;
			}
			// This happens with cookies.
			mHeaders[i].second = value;
			return;
		}
	}
	mHeaders.emplace_back(name, value);
}

void HttpHeaders::append(const char* name, const char* value)
{
	if (!name || !*name || !value || !*value)	// Paranoia
	{
		return;
	}
	for (S32 i = 0, count = mHeaders.size(); i < count; ++i)
	{
		if (strcmp(mHeaders[i].first.c_str(), name) == 0)
		{
			// This does happen in "normal" conditions...
			if (strcmp(mHeaders[i].second.c_str(), value) == 0)
			{
				return;
			}
			// This happens with cookies.
			mHeaders[i].second = value;
			return;
		}
	}
	mHeaders.emplace_back(name, value);
}

void HttpHeaders::appendNormal(const char* header, size_t size)
{
	std::string name;
	std::string value;

	size_t col_pos = 0;
	for ( ; col_pos < size; ++col_pos)
	{
		if (':' == header[col_pos])
		{
			break;
		}
	}

	if (col_pos < size)
	{
		// Looks like a header, split it and normalize.
		// Name is everything before the colon, may be zero-length.
		name.assign(header, col_pos);

		// Value is everything after the colon, may also be zero-length.
		const size_t val_len = size - col_pos - 1;
		if (val_len)
		{
			value.assign(header + col_pos + 1, val_len);
		}

		// Clean the strings
		LLStringUtil::toLower(name);
		LLStringUtil::trim(name);
		LLStringUtil::trimHead(value);
	}
	else
	{
		// Uncertain what this is, we'll pack it as a name without a value.
		// Won't clean as we don't know what it is...
		name.assign(header, size);
	}

	mHeaders.emplace_back(name, value);
}

// Find from end to simulate a tradition of using single-valued std::map for
// this in the past.
const std::string* HttpHeaders::find(const char* name) const
{
	for (const_reverse_iterator it = rbegin(), iend = rend(); it != iend; ++it)
	{
		if (strcmp(it->first.c_str(), name) == 0)
		{
			return &it->second;
		}
	}
	return NULL;
}

// Find from end to simulate a tradition of using single-valued std::map for
// this in the past.
const std::string* HttpHeaders::find(const std::string& name) const
{
	for (const_reverse_iterator it = rbegin(), iend = rend(); it != iend; ++it)
	{
		if (it->first == name)
		{
			return &it->second;
		}
	}
	return NULL;
}

void HttpHeaders::remove(const char* name)
{
	for (iterator it = begin(), iend = end(); it != iend; ++it)
	{
		if (strcmp(it->first.c_str(), name) == 0)
		{
			mHeaders.erase(it);
			return;
		}
	}
}

}   // End namespace LLCore
