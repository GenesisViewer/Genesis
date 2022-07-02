/**
 * @file llhttpconstants.h
 * @brief Constants for HTTP requests and responses
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2013, Linden Research, Inc.
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

#ifndef LL_HTTP_CONSTANTS_H
#define LL_HTTP_CONSTANTS_H

#include "llpreprocessor.h"

/////// HTTP STATUS CODES ///////

// Standard errors from HTTP spec:
// http://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1
constexpr S32 HTTP_CONTINUE = 100;
constexpr S32 HTTP_SWITCHING_PROTOCOLS = 101;

// Success
constexpr S32 HTTP_OK = 200;
constexpr S32 HTTP_CREATED = 201;
constexpr S32 HTTP_ACCEPTED = 202;
constexpr S32 HTTP_NON_AUTHORITATIVE_INFORMATION = 203;
constexpr S32 HTTP_NO_CONTENT = 204;
constexpr S32 HTTP_RESET_CONTENT = 205;
constexpr S32 HTTP_PARTIAL_CONTENT = 206;

// Redirection
constexpr S32 HTTP_MULTIPLE_CHOICES = 300;
constexpr S32 HTTP_MOVED_PERMANENTLY = 301;
constexpr S32 HTTP_FOUND = 302;
constexpr S32 HTTP_SEE_OTHER = 303;
constexpr S32 HTTP_NOT_MODIFIED = 304;
constexpr S32 HTTP_USE_PROXY = 305;
constexpr S32 HTTP_TEMPORARY_REDIRECT = 307;

// Client Error
constexpr S32 HTTP_BAD_REQUEST = 400;
constexpr S32 HTTP_UNAUTHORIZED = 401;
constexpr S32 HTTP_PAYMENT_REQUIRED = 402;
constexpr S32 HTTP_FORBIDDEN = 403;
constexpr S32 HTTP_NOT_FOUND = 404;
constexpr S32 HTTP_METHOD_NOT_ALLOWED = 405;
constexpr S32 HTTP_NOT_ACCEPTABLE = 406;
constexpr S32 HTTP_PROXY_AUTHENTICATION_REQUIRED = 407;
constexpr S32 HTTP_REQUEST_TIME_OUT = 408;
constexpr S32 HTTP_CONFLICT = 409;
constexpr S32 HTTP_GONE = 410;
constexpr S32 HTTP_LENGTH_REQUIRED = 411;
constexpr S32 HTTP_PRECONDITION_FAILED = 412;
constexpr S32 HTTP_REQUEST_ENTITY_TOO_LARGE = 413;
constexpr S32 HTTP_REQUEST_URI_TOO_LARGE = 414;
constexpr S32 HTTP_UNSUPPORTED_MEDIA_TYPE = 415;
constexpr S32 HTTP_REQUESTED_RANGE_NOT_SATISFIABLE = 416;
constexpr S32 HTTP_EXPECTATION_FAILED = 417;

// Server Error
constexpr S32 HTTP_INTERNAL_SERVER_ERROR = 500;
constexpr S32 HTTP_NOT_IMPLEMENTED = 501;
constexpr S32 HTTP_BAD_GATEWAY = 502;
constexpr S32 HTTP_SERVICE_UNAVAILABLE = 503;
constexpr S32 HTTP_GATEWAY_TIME_OUT = 504;
constexpr S32 HTTP_VERSION_NOT_SUPPORTED = 505;

// We combine internal process errors with status codes. These status codes
// should not be sent over the wire and indicate something went wrong
// internally. If you get these they are not normal.
constexpr S32 HTTP_INTERNAL_CURL_ERROR = 498;
constexpr S32 HTTP_INTERNAL_ERROR = 499;

////// HTTP Methods //////

extern const std::string HTTP_VERB_INVALID;
extern const std::string HTTP_VERB_HEAD;
extern const std::string HTTP_VERB_GET;
extern const std::string HTTP_VERB_PUT;
extern const std::string HTTP_VERB_POST;
extern const std::string HTTP_VERB_DELETE;
extern const std::string HTTP_VERB_MOVE;
extern const std::string HTTP_VERB_OPTIONS;
extern const std::string HTTP_VERB_PATCH;
extern const std::string HTTP_VERB_COPY;

enum EHTTPMethod
{
	HTTP_INVALID = 0,
	HTTP_HEAD,
	HTTP_GET,
	HTTP_PUT,
	HTTP_POST,
	HTTP_DELETE,
	HTTP_MOVE,			// Caller will need to set 'Destination' header
	HTTP_OPTIONS,
	HTTP_PATCH,
	HTTP_COPY,
	HTTP_METHOD_COUNT
};

inline bool isHttpInformationalStatus(S32 status)
{
	// Check for status 1xx.
	return status >= 100 && status < 200;
}

inline bool isHttpGoodStatus(S32 status)
{
	// Check for status 2xx.
	return status >= 200 && status < 300;
}

inline bool isHttpRedirectStatus(S32 status)
{
	// Check for status 3xx.
	return status >= 300 && status < 400;
}

inline bool isHttpClientErrorStatus(S32 status)
{
	// Check for status 4xx.
	// Status 499 is sometimes used for re-interpreted status 2xx errors based
	// on body content. Treat these as potentially retryable 'server' status
	// errors, since we do not have enough context to know if this will always
	// fail.
	return status != 499 && status >= 400 && status < 500;
}

inline bool isHttpServerErrorStatus(S32 status)
{
	// Check for status 5xx.
	// Status 499 is sometimes used for re-interpreted status 2xx errors.
	// Allow retry of these, since we don't have enough information in this
	// context to know if this will always fail.
	return status == 499 || (status >= 500 && status < 600);
}

const std::string& httpMethodAsVerb(EHTTPMethod method);

// Outgoing headers. Do *not* use these to check incoming headers.
// For incoming headers, use the lower-case headers, below.
extern const std::string HTTP_OUT_HEADER_ACCEPT;
extern const std::string HTTP_OUT_HEADER_ACCEPT_CHARSET;
extern const std::string HTTP_OUT_HEADER_ACCEPT_ENCODING;
extern const std::string HTTP_OUT_HEADER_ACCEPT_LANGUAGE;
extern const std::string HTTP_OUT_HEADER_ACCEPT_RANGES;
extern const std::string HTTP_OUT_HEADER_AGE;
extern const std::string HTTP_OUT_HEADER_ALLOW;
extern const std::string HTTP_OUT_HEADER_AUTHORIZATION;
extern const std::string HTTP_OUT_HEADER_CACHE_CONTROL;
extern const std::string HTTP_OUT_HEADER_CONNECTION;
extern const std::string HTTP_OUT_HEADER_CONTENT_DESCRIPTION;
extern const std::string HTTP_OUT_HEADER_CONTENT_ENCODING;
extern const std::string HTTP_OUT_HEADER_CONTENT_ID;
extern const std::string HTTP_OUT_HEADER_CONTENT_LANGUAGE;
extern const std::string HTTP_OUT_HEADER_CONTENT_LENGTH;
extern const std::string HTTP_OUT_HEADER_CONTENT_LOCATION;
extern const std::string HTTP_OUT_HEADER_CONTENT_MD5;
extern const std::string HTTP_OUT_HEADER_CONTENT_RANGE;
extern const std::string HTTP_OUT_HEADER_CONTENT_TRANSFER_ENCODING;
extern const std::string HTTP_OUT_HEADER_CONTENT_TYPE;
extern const std::string HTTP_OUT_HEADER_COOKIE;
extern const std::string HTTP_OUT_HEADER_DATE;
extern const std::string HTTP_OUT_HEADER_DESTINATION;
extern const std::string HTTP_OUT_HEADER_ETAG;
extern const std::string HTTP_OUT_HEADER_EXPECT;
extern const std::string HTTP_OUT_HEADER_EXPIRES;
extern const std::string HTTP_OUT_HEADER_FROM;
extern const std::string HTTP_OUT_HEADER_HOST;
extern const std::string HTTP_OUT_HEADER_IF_MATCH;
extern const std::string HTTP_OUT_HEADER_IF_MODIFIED_SINCE;
extern const std::string HTTP_OUT_HEADER_IF_NONE_MATCH;
extern const std::string HTTP_OUT_HEADER_IF_RANGE;
extern const std::string HTTP_OUT_HEADER_IF_UNMODIFIED_SINCE;
extern const std::string HTTP_OUT_HEADER_KEEP_ALIVE;
extern const std::string HTTP_OUT_HEADER_LAST_MODIFIED;
extern const std::string HTTP_OUT_HEADER_LOCATION;
extern const std::string HTTP_OUT_HEADER_MAX_FORWARDS;
extern const std::string HTTP_OUT_HEADER_MIME_VERSION;
extern const std::string HTTP_OUT_HEADER_PRAGMA;
extern const std::string HTTP_OUT_HEADER_PROXY_AUTHENTICATE;
extern const std::string HTTP_OUT_HEADER_PROXY_AUTHORIZATION;
extern const std::string HTTP_OUT_HEADER_RANGE;
extern const std::string HTTP_OUT_HEADER_REFERER;
extern const std::string HTTP_OUT_HEADER_RETRY_AFTER;
extern const std::string HTTP_OUT_HEADER_SERVER;
extern const std::string HTTP_OUT_HEADER_SET_COOKIE;
extern const std::string HTTP_OUT_HEADER_TE;
extern const std::string HTTP_OUT_HEADER_TRAILER;
extern const std::string HTTP_OUT_HEADER_TRANSFER_ENCODING;
extern const std::string HTTP_OUT_HEADER_UPGRADE;
extern const std::string HTTP_OUT_HEADER_USER_AGENT;
extern const std::string HTTP_OUT_HEADER_VARY;
extern const std::string HTTP_OUT_HEADER_VIA;
extern const std::string HTTP_OUT_HEADER_WARNING;
extern const std::string HTTP_OUT_HEADER_WWW_AUTHENTICATE;

// Incoming headers are normalized to lower-case.
extern const std::string HTTP_IN_HEADER_ACCEPT_LANGUAGE;
extern const std::string HTTP_IN_HEADER_CACHE_CONTROL;
extern const std::string HTTP_IN_HEADER_CONTENT_LENGTH;
extern const std::string HTTP_IN_HEADER_CONTENT_LOCATION;
extern const std::string HTTP_IN_HEADER_CONTENT_TYPE;
extern const std::string HTTP_IN_HEADER_HOST;
extern const std::string HTTP_IN_HEADER_LOCATION;
extern const std::string HTTP_IN_HEADER_RETRY_AFTER;
extern const std::string HTTP_IN_HEADER_SET_COOKIE;
extern const std::string HTTP_IN_HEADER_USER_AGENT;
extern const std::string HTTP_IN_HEADER_X_FORWARDED_FOR;

//// HTTP Content Types ////

extern const std::string HTTP_CONTENT_LLSD_XML;
extern const std::string HTTP_CONTENT_OCTET_STREAM;
extern const std::string HTTP_CONTENT_VND_LL_MESH;
extern const std::string HTTP_CONTENT_XML;
extern const std::string HTTP_CONTENT_JSON;
extern const std::string HTTP_CONTENT_TEXT_HTML;
extern const std::string HTTP_CONTENT_TEXT_HTML_UTF8;
extern const std::string HTTP_CONTENT_TEXT_PLAIN_UTF8;
extern const std::string HTTP_CONTENT_TEXT_LLSD;
extern const std::string HTTP_CONTENT_TEXT_XML;
extern const std::string HTTP_CONTENT_TEXT_LSL;
extern const std::string HTTP_CONTENT_TEXT_PLAIN;
extern const std::string HTTP_CONTENT_IMAGE_X_J2C;
extern const std::string HTTP_CONTENT_IMAGE_J2C;
extern const std::string HTTP_CONTENT_IMAGE_JPEG;
extern const std::string HTTP_CONTENT_IMAGE_PNG;
extern const std::string HTTP_CONTENT_IMAGE_BMP;

//// HTTP Cache Settings ////
extern const std::string HTTP_NO_CACHE;
extern const std::string HTTP_NO_CACHE_CONTROL;

// Common strings use for populating the context. bascally 'request',
// 'wildcard', and 'headers'.
extern const std::string CONTEXT_HEADERS;
extern const std::string CONTEXT_PATH;
extern const std::string CONTEXT_QUERY_STRING;
extern const std::string CONTEXT_REQUEST;
extern const std::string CONTEXT_RESPONSE;
extern const std::string CONTEXT_VERB;
extern const std::string CONTEXT_WILDCARD;

extern const std::string CONTEXT_REMOTE_HOST;
extern const std::string CONTEXT_REMOTE_PORT;

extern const std::string CONTEXT_DEST_URI_SD_LABEL;
extern const std::string CONTEXT_TRANSFERED_BYTES;

#endif // LL_HTTP_CONSTANTS_H
