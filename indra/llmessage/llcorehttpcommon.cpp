/**
 * @file llcorehttpcommon.cpp
 * @brief
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

#include <sstream>

#include "openssl/opensslv.h"
#if OPENSSL_VERSION_NUMBER < 0x101000bfL
# define SAFE_SSL 1
# include "openssl/crypto.h"
#endif

#include "curl/curlver.h"

#include "llcorehttpcommon.h"

#include "llhttpconstants.h"
#include "llmutex.h"
#include "llstring.h"
#include "llthread.h"

// Some commonly used statuses, defined as globals, so to avoid having to
// construct and destruct them each time we need them in tests...
const LLCore::HttpStatus gStatusPartialContent(HTTP_PARTIAL_CONTENT);
const LLCore::HttpStatus gStatusBadRequest(HTTP_BAD_REQUEST);
const LLCore::HttpStatus gStatusForbidden(HTTP_FORBIDDEN);
const LLCore::HttpStatus gStatusNotFound(HTTP_NOT_FOUND);
const LLCore::HttpStatus gStatusNotSatisfiable(HTTP_REQUESTED_RANGE_NOT_SATISFIABLE);
const LLCore::HttpStatus gStatusInternalError(HTTP_INTERNAL_ERROR);
const LLCore::HttpStatus gStatusBadGateway(HTTP_BAD_GATEWAY);
const LLCore::HttpStatus gStatusUnavailable(HTTP_SERVICE_UNAVAILABLE);
const LLCore::HttpStatus gStatusCantConnect(LLCore::HttpStatus::EXT_CURL_EASY,
											CURLE_COULDNT_CONNECT);
const LLCore::HttpStatus gStatusTimeout(LLCore::HttpStatus::EXT_CURL_EASY,
										CURLE_OPERATION_TIMEDOUT);
const LLCore::HttpStatus gStatusCancelled(LLCore::HttpStatus::LLCORE,
										  LLCore::HE_OP_CANCELLED);

namespace LLCore
{

HttpStatus::type_enum_t EXT_CURL_EASY;
HttpStatus::type_enum_t EXT_CURL_MULTI;
HttpStatus::type_enum_t LLCORE;

HttpStatus::HttpStatus()
{
	constexpr type_enum_t type = LLCORE;
	mDetails = std::make_shared<Details>(type, HE_SUCCESS);
}

HttpStatus::HttpStatus(type_enum_t type, short status)
{
	mDetails = std::make_shared<Details>(type, status);
}

HttpStatus::HttpStatus(int http_status)
{
	mDetails =
		std::make_shared<Details>(http_status,
								  http_status >= 200 &&
								  http_status <= 299 ? HE_SUCCESS
													 : HE_REPLY_ERROR);
	llassert(http_status >= 100 && http_status <= 999);
}

HttpStatus::HttpStatus(int http_status, const std::string& message)
{
	mDetails =
		std::make_shared<Details>(http_status,
								  http_status >= 200 &&
								  http_status <= 299 ? HE_SUCCESS
													 : HE_REPLY_ERROR);
	llassert(http_status >= 100 && http_status <= 999);
	mDetails->mMessage = message;
}

HttpStatus::operator U32() const
{
	// Effectively, concatenate mType (high) with mStatus (low).
	static const int shift = 8 * sizeof(mDetails->mStatus);
	return U32(mDetails->mType) << shift | U32((int)mDetails->mStatus);
}

std::string HttpStatus::toHex() const
{
	std::ostringstream result;
	result.width(8);
	result.fill('0');
	result << std::hex << operator U32();
	return result.str();
}

std::string HttpStatus::toString() const
{
	static const char* llcore_errors[] = {
		"",
		"HTTP error reply status",
		"Services shutting down",
		"Operation cancelled",
		"Invalid Content-Range header encountered",
		"Request handle not found",
		"Invalid datatype for argument or option",
		"Option has not been explicitly set",
		"Option is not dynamic and must be set early",
		"Invalid HTTP status code received from server",
		"Could not allocate required resource"
	};
	constexpr S32 llcore_errors_count = LL_ARRAY_SIZE(llcore_errors);

	static const struct
	{
		type_enum_t	mCode;
		const char*	mText;
	}
	http_errors[] = {
		// Keep sorted by mCode, we binary search this list.
		{ 100, "Continue" },
		{ 101, "Switching Protocols" },
		{ 200, "OK" },
		{ 201, "Created" },
		{ 202, "Accepted" },
		{ 203, "Non-Authoritative Information" },
		{ 204, "No Content" },
		{ 205, "Reset Content" },
		{ 206, "Partial Content" },
		{ 300, "Multiple Choices" },
		{ 301, "Moved Permanently" },
		{ 302, "Found" },
		{ 303, "See Other" },
		{ 304, "Not Modified" },
		{ 305, "Use Proxy" },
		{ 307, "Temporary Redirect" },
		{ 400, "Bad Request" },
		{ 401, "Unauthorized" },
		{ 402, "Payment Required" },
		{ 403, "Forbidden" },
		{ 404, "Not Found" },
		{ 405, "Method Not Allowed" },
		{ 406, "Not Acceptable" },
		{ 407, "Proxy Authentication Required" },
		{ 408, "Request Time-out" },
		{ 409, "Conflict" },
		{ 410, "Gone" },
		{ 411, "Length Required" },
		{ 412, "Precondition Failed" },
		{ 413, "Request Entity Too Large" },
		{ 414, "Request-URI Too Large" },
		{ 415, "Unsupported Media Type" },
		{ 416, "Requested range not satisfiable" },
		{ 417, "Expectation Failed" },
		{ 499, "Malformed response contents" },
		{ 500, "Internal Server Error" },
		{ 501, "Not Implemented" },
		{ 502, "Bad Gateway" },
		{ 503, "Service Unavailable" },
		{ 504, "Gateway Time-out" },
		{ 505, "HTTP Version not supported" }
	};
	constexpr S32 http_errors_count = LL_ARRAY_SIZE(http_errors);

	if (*this)
	{
		return std::string("");
	}

	type_enum_t type = getType();
	switch (type)
	{
		case EXT_CURL_EASY:
			return std::string(curl_easy_strerror(CURLcode(getStatus())));

		case EXT_CURL_MULTI:
			return std::string(curl_multi_strerror(CURLMcode(getStatus())));

		case LLCORE:
		{
			short status = getStatus();
			if (status >= 0 && status < llcore_errors_count)
			{
				return std::string(llcore_errors[status]);
			}
			break;
		}

		default:
		{
			if (isHttpStatus())
			{
				// special handling for status 499 "Linden Catchall"
				if (type == 499 && !getMessage().empty())
				{
					return getMessage();
				}

				// Binary search for the error code and string
				S32 bottom = 0;
				S32 top = http_errors_count;
				while (true)
				{
					S32 at = (bottom + top) / 2;
					if (type == http_errors[at].mCode)
					{
						return std::string(http_errors[at].mText);
					}
					if (at == bottom)
					{
						break;
					}
					else if (type < http_errors[at].mCode)
					{
						top = at;
					}
					else
					{
						bottom = at;
					}
				}
			}
		}
	}

	return std::string("Unknown error");
}

std::string HttpStatus::toTerseString() const
{
	std::ostringstream result;
	short error_value = getStatus();

	type_enum_t type = getType();
	switch (type)
	{
		case EXT_CURL_EASY:
			result << "Easy_";
			break;

		case EXT_CURL_MULTI:
			result << "Multi_";
			break;

		case LLCORE:
			result << "Core_";
			break;

		default:
			if (isHttpStatus())
			{
				result << "Http_";
				error_value = type;
			}
			else
			{
				result << "Unknown_";
			}
			break;
	}

	result << error_value;
	return result.str();
}

// Pass true on statuses that might actually be cleared by a retry. Library
// failures, calling problems, etc aren't going to be fixed by squirting bits
// all over the Net.
bool HttpStatus::isRetryable() const
{
	static const HttpStatus bad_proxy(EXT_CURL_EASY,
									  CURLE_COULDNT_RESOLVE_PROXY);
	static const HttpStatus bad_host(EXT_CURL_EASY,
									 CURLE_COULDNT_RESOLVE_HOST);
	static const HttpStatus send_error(EXT_CURL_EASY, CURLE_SEND_ERROR);
	static const HttpStatus recv_error(EXT_CURL_EASY, CURLE_RECV_ERROR);
	static const HttpStatus upload_failed(EXT_CURL_EASY, CURLE_UPLOAD_FAILED);
	static const HttpStatus post_error(EXT_CURL_EASY, CURLE_HTTP_POST_ERROR);
	static const HttpStatus partial_file(EXT_CURL_EASY, CURLE_PARTIAL_FILE);
	static const HttpStatus bad_range(LLCORE, HE_INV_CONTENT_RANGE_HDR);

	// HE_INVALID_HTTP_STATUS is special. As of 7.37.0, there are some
	// scenarios where response processing in libcurl appear to go wrong and
	// response data is corrupted. A side-effect of this is that the HTTP
	// status is read as 0 from the library. See libcurl bug report 1420
	// (https://sourceforge.net/p/curl/bugs/1420/) for details.
	static const HttpStatus inv_status(HttpStatus::LLCORE,
									   HE_INVALID_HTTP_STATUS);

											//  vvv  Include special 499 in retryables
	return ((isHttpStatus() && getType() >= 499 && getType() <= 599) ||
			*this == gStatusCantConnect ||	// Connection reset/endpoint problems
			*this == bad_proxy ||			// DNS problems
			*this == bad_host ||			// DNS problems
			*this == send_error ||			// General socket problems
			*this == recv_error ||			// General socket problems
			*this == upload_failed ||		// Transport problem
			*this == gStatusTimeout ||		// Timer expired
			*this == post_error ||			// Transport problem
			*this == partial_file ||		// Data inconsistency in response
#if 1		// disable for "[curl:bugs] #1420" tests.
			*this == inv_status ||			// Inv status can reflect internal state problem in libcurl
#endif
			*this == bad_range);			// Short data read disagrees with content-range
}

namespace LLHttp
{
bool gEnabledHTTP2 = false;

namespace
{

static LLMutex sHandleMutex;

CURL* getCurlTemplateHandle()
{
	static CURL* template_handlep = NULL;
	if (!template_handlep)
	{
		// Late creation of the template curl handle
		template_handlep = curl_easy_init();
		if (template_handlep)
		{
			CURLcode result = curl_easy_setopt(template_handlep,
											   CURLOPT_IPRESOLVE,
											   CURL_IPRESOLVE_V4);
			check_curl_code(result, CURLOPT_IPRESOLVE);
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_NOSIGNAL, 1);
			check_curl_code(result, CURLOPT_NOSIGNAL);
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_NOPROGRESS, 1);
			check_curl_code(result, CURLOPT_NOPROGRESS);
#if LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 60
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_ENCODING, "");
			check_curl_code(result, CURLOPT_ENCODING);
#endif
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_AUTOREFERER, 1);
			check_curl_code(result, CURLOPT_AUTOREFERER);
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_FOLLOWLOCATION, 1);
			check_curl_code(result, CURLOPT_FOLLOWLOCATION);
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_SSL_VERIFYPEER, 1);
			check_curl_code(result, CURLOPT_SSL_VERIFYPEER);
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_SSL_VERIFYHOST, 0);
			check_curl_code(result, CURLOPT_SSL_VERIFYHOST);

			// The Linksys WRT54G V5 router has an issue with frequent DNS
			// lookups from LAN machines. If they happen too often, like for
			// every HTTP request, the router gets annoyed after about 700 or
			// so requests and starts issuing TCP RSTs to new connections.
			// Reuse the DNS lookups for even a few seconds and no RSTs.
			result = curl_easy_setopt(template_handlep,
									  CURLOPT_DNS_CACHE_TIMEOUT, 15);
			check_curl_code(result, CURLOPT_DNS_CACHE_TIMEOUT);
		}
		else
		{
			llwarns << "Cannot create the template curl handle !" << llendl;
		}
	}

	return template_handlep;
}

void deallocateEasyCurl(CURL* curlp)
{
	sHandleMutex.lock();
	curl_easy_cleanup(curlp);
	sHandleMutex.unlock();
}

#if SAFE_SSL
typedef std::shared_ptr<LLMutex> LLMutex_ptr;
std::vector<LLMutex_ptr> sSSLMutex;

//static
void ssl_locking_callback(int mode, int type, const char* file, int line)
{
	if ((size_t)type >= sSSLMutex.size())
	{
		llwarns << "Attempt to get unknown MUTEX in SSL Lock." << llendl;
	}

	if (mode & CRYPTO_LOCK)
	{
		sSSLMutex[type]->lock();
	}
	else
	{
		sSSLMutex[type]->unlock();
	}
}

//static
unsigned long ssl_thread_id()
{
	return LLThread::thisThreadIdHash();
}
#endif

}

void initialize()
{
	// Do not change this "unless you are familiar with and mean to control
	// internal operations of libcurl"
	// - http://curl.haxx.se/libcurl/c/curl_global_init.html
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);

	check_curl_code(code, CURL_GLOBAL_ALL);

#if SAFE_SSL
	S32 mutex_count = CRYPTO_num_locks();
	for (S32 i = 0; i < mutex_count; ++i)
	{
		sSSLMutex.push_back(LLMutex_ptr(new LLMutex()));
	}
	CRYPTO_set_id_callback(&ssl_thread_id);
	CRYPTO_set_locking_callback(&ssl_locking_callback);
#endif
}

void cleanup()
{
#if SAFE_SSL
	CRYPTO_set_id_callback(NULL);
	CRYPTO_set_locking_callback(NULL);
	sSSLMutex.clear();
#endif

	curl_global_cleanup();
}

CURL_ptr createEasyHandle()
{
	LLMutexLock lock(sHandleMutex);

	CURL* handle = curl_easy_duphandle(getCurlTemplateHandle());

	return CURL_ptr(handle, &deallocateEasyCurl);
}

const std::string& getCURLVersion()
{
	static std::string version;
	if (version.empty())
	{
		version.assign(curl_version());
		// Cleanup the version string to have it showing slashes in beetween
		// components instead of between each component name and component
		// version. HB
		LLStringUtil::replaceChar(version, ' ', ';');
		LLStringUtil::replaceChar(version, '/', ' ');
		LLStringUtil::replaceChar(version, ';', '/');
	}
	return version;
}

void check_curl_code(CURLcode code, int curl_setopt_option)
{
	if (code != CURLE_OK)
	{
		// Comment from old llcurl code which may no longer apply:
		//
		// linux appears to throw a curl error once per session for a bad
		// initialization at a pretty random time (when enabling cookies).
		llwarns << "curl error detected: " << curl_easy_strerror(code)
				<< " - curl_easy_setopt option:  " << curl_setopt_option
				<< llendl;
	}
}

}

} // End namespace LLCore
