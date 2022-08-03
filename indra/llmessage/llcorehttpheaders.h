/**
 * @file llcorehttpheaders.h
 * @brief Public-facing declarations for the HttpHeaders class
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

#ifndef	_LLCORE_HTTP_HEADERS_H_
#define	_LLCORE_HTTP_HEADERS_H_

#include <string>
#include <vector>

#include "llcorehttpcommon.h"

// Convenient shortcut
#define DEFAULT_HTTP_HEADERS std::make_shared<LLCore::HttpHeaders>()

namespace LLCore
{

// Maintains an ordered list of name/value pairs representing HTTP header
// lines. This is used both to provide additional headers when making HTTP
// requests and in responses when the caller has asked that headers be
// returned (not the default option).
//
// Class is mostly a thin wrapper around a vector of pairs of strings. Methods
// provided are few and intended to reflect actual use patterns. These include:
// - Clearing the list
// - Appending a name/value pair to the vector
// - Processing a raw byte string into a normalized name/value pair and
//   appending the result.
// - Simple case-sensitive find-last-by-name search
// - Forward and reverse iterators over all pairs
//
// Container is ordered and multi-valued. Headers are written in the order in
// which they are appended and are stored in the order in which they're
// received from the wire. The same header may appear two or more times in any
// container. Searches using the simple find() interface will find only the
// last occurrence (somewhat simulates the use of std::map). Fuller searches
// require the use of an iterator. Headers received from the wire are only
// returned from the last request when redirections are involved.
//
// Threading: not intrinsically thread-safe. It *is* expected that callers will
// build these objects and then share them via reference counting with the
// worker thread. The implication is that once an HttpHeader instance is handed
// to a request, the object must be treated as read-only.
//
// Allocation: refcounted, heap only. Caller of the constructor is given a
// refcount.

class HttpHeaders
{
public:
	typedef std::pair<std::string, std::string> header_t;
	typedef std::vector<header_t> container_t;
	typedef container_t::iterator iterator;
	typedef container_t::const_iterator const_iterator;
	typedef container_t::reverse_iterator reverse_iterator;
	typedef container_t::const_reverse_iterator const_reverse_iterator;
	typedef container_t::value_type value_type;
	typedef container_t::size_type size_type;
	typedef std::shared_ptr<HttpHeaders> ptr_t;

public:
	// In addition to the instance, caller has a refcount to the instance.
	// A call to release() will destroy the instance.
	HttpHeaders() = default;

	// Non-copyable
	HttpHeaders(const HttpHeaders&) = delete;
	HttpHeaders& operator=(const HttpHeaders&) = delete;

public:
	// Empty the list of headers.
	LL_INLINE void clear()							{ mHeaders.clear(); }

	// Append a name/value pair supplied as either std::strings or
	// NUL-terminated char * to the header list. No normalization is performed
	// on the strings. No conformance test is performed (names may contain
	// spaces, colons, etc).

	void append(const char* name, const char* value);
	void append(const std::string& name, const char* value);

	LL_INLINE void append(const char* name, const std::string& value)
	{
		append(name, value.c_str());
	}

	LL_INLINE void append(const std::string& name, const std::string& value)
	{
		append(name, value.c_str());
	}

	// Extract a name/value pair from a raw byte array using the first colon
	// character as a separator. Input string does not need to be
	// NUL-terminated. Resulting name/value pair is appended to the header
	// list.
	//
	// Normalization is performed on the name/value pair as follows:
	// - name is lower-cased according to mostly ASCII rules
	// - name is left- and right-trimmed of spaces and tabs
	// - value is left-trimmed of spaces and tabs
	// - either or both of name and value may be zero-length
	//
	// By convention, headers read from the wire will be normalized in this
	// fashion prior to delivery to any HttpHandler code. Headers to be written
	// to the wire are left as appended to the list.
	void appendNormal(const char* header, size_t size);

	// Perform a simple, case-sensitive search of the header list
	// returning a pointer to the value of the last matching header
	// in the header list. If none is found, a NULL pointer is returned.
	//
	// Any pointer returned references objects in the container itself and will
	// have the same lifetime as this class. If you want the value beyond the
	// lifetime of this instance, make a copy.
	//
	// @arg		name	string giving the name of a header to search.
	//					The comparison is case-sensitive though list entries
	//					may have been normalized to lower-case.
	//
	// @return			NULL if the header wasn't found otherwise a pointer to
	//					a std::string in the container. Pointer is valid only
	//					for the lifetime of the container or until container is
	//					modifed.
	const std::string* find(const char* name) const;
	const std::string* find(const std::string& name) const;

	// Remove the header from the list if found.

	void remove(const char* name);

	LL_INLINE void remove(const std::string& name)
	{
		remove(name.c_str());
	}

	// Count of headers currently in the list.
	LL_INLINE size_type size() const				{ return mHeaders.size(); }

	// Standard std::vector-based forward iterators.

	LL_INLINE iterator begin()						{ return mHeaders.begin(); }
	LL_INLINE const_iterator begin() const			{ return mHeaders.begin(); }

	LL_INLINE iterator end()						{ return mHeaders.end(); }
	LL_INLINE const_iterator end() const			{ return mHeaders.end(); }

	// Standard std::vector-based reverse iterators.

	LL_INLINE reverse_iterator rbegin()				{ return mHeaders.rbegin(); }
	LL_INLINE const_reverse_iterator rbegin() const	{ return mHeaders.rbegin(); }

	LL_INLINE reverse_iterator rend()				{ return mHeaders.rend(); }
	LL_INLINE const_reverse_iterator rend() const	{ return mHeaders.rend(); }

protected:
	container_t mHeaders;

};

}  // End namespace LLCore

#endif	// _LLCORE_HTTP_HEADERS_H_
