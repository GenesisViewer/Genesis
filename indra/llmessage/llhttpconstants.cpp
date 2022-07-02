/**
 * @file llhttpconstants.cpp
 * @brief Implementation of the HTTP request / response constant lookups
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

#include "linden_common.h"

#include "llhttpconstants.h"

#include "lltimer.h"

const std::string CONTEXT_HEADERS("headers");
const std::string CONTEXT_PATH("path");
const std::string CONTEXT_QUERY_STRING("query-string");
const std::string CONTEXT_REQUEST("request");
const std::string CONTEXT_RESPONSE("response");
const std::string CONTEXT_VERB("verb");
const std::string CONTEXT_WILDCARD("wildcard");

const std::string CONTEXT_REMOTE_HOST("remote-host");
const std::string CONTEXT_REMOTE_PORT("remote-port");

const std::string CONTEXT_DEST_URI_SD_LABEL("dest_uri");
const std::string CONTEXT_TRANSFERED_BYTES("transfered_bytes");

// Outgoing headers. Do *not* use these to check incoming headers.
// For incoming headers, use the lower-case headers, below.
const std::string HTTP_OUT_HEADER_ACCEPT("Accept");
const std::string HTTP_OUT_HEADER_ACCEPT_CHARSET("Accept-Charset");
const std::string HTTP_OUT_HEADER_ACCEPT_ENCODING("Accept-Encoding");
const std::string HTTP_OUT_HEADER_ACCEPT_LANGUAGE("Accept-Language");
const std::string HTTP_OUT_HEADER_ACCEPT_RANGES("Accept-Ranges");
const std::string HTTP_OUT_HEADER_AGE("Age");
const std::string HTTP_OUT_HEADER_ALLOW("Allow");
const std::string HTTP_OUT_HEADER_AUTHORIZATION("Authorization");
const std::string HTTP_OUT_HEADER_CACHE_CONTROL("Cache-Control");
const std::string HTTP_OUT_HEADER_CONNECTION("Connection");
const std::string HTTP_OUT_HEADER_CONTENT_DESCRIPTION("Content-Description");
const std::string HTTP_OUT_HEADER_CONTENT_ENCODING("Content-Encoding");
const std::string HTTP_OUT_HEADER_CONTENT_ID("Content-ID");
const std::string HTTP_OUT_HEADER_CONTENT_LANGUAGE("Content-Language");
const std::string HTTP_OUT_HEADER_CONTENT_LENGTH("Content-Length");
const std::string HTTP_OUT_HEADER_CONTENT_LOCATION("Content-Location");
const std::string HTTP_OUT_HEADER_CONTENT_MD5("Content-MD5");
const std::string HTTP_OUT_HEADER_CONTENT_RANGE("Content-Range");
const std::string HTTP_OUT_HEADER_CONTENT_TRANSFER_ENCODING("Content-Transfer-Encoding");
const std::string HTTP_OUT_HEADER_CONTENT_TYPE("Content-Type");
const std::string HTTP_OUT_HEADER_COOKIE("Cookie");
const std::string HTTP_OUT_HEADER_DATE("Date");
const std::string HTTP_OUT_HEADER_DESTINATION("Destination");
const std::string HTTP_OUT_HEADER_ETAG("ETag");
const std::string HTTP_OUT_HEADER_EXPECT("Expect");
const std::string HTTP_OUT_HEADER_EXPIRES("Expires");
const std::string HTTP_OUT_HEADER_FROM("From");
const std::string HTTP_OUT_HEADER_HOST("Host");
const std::string HTTP_OUT_HEADER_IF_MATCH("If-Match");
const std::string HTTP_OUT_HEADER_IF_MODIFIED_SINCE("If-Modified-Since");
const std::string HTTP_OUT_HEADER_IF_NONE_MATCH("If-None-Match");
const std::string HTTP_OUT_HEADER_IF_RANGE("If-Range");
const std::string HTTP_OUT_HEADER_IF_UNMODIFIED_SINCE("If-Unmodified-Since");
const std::string HTTP_OUT_HEADER_KEEP_ALIVE("Keep-Alive");
const std::string HTTP_OUT_HEADER_LAST_MODIFIED("Last-Modified");
const std::string HTTP_OUT_HEADER_LOCATION("Location");
const std::string HTTP_OUT_HEADER_MAX_FORWARDS("Max-Forwards");
const std::string HTTP_OUT_HEADER_MIME_VERSION("MIME-Version");
const std::string HTTP_OUT_HEADER_PRAGMA("Pragma");
const std::string HTTP_OUT_HEADER_PROXY_AUTHENTICATE("Proxy-Authenticate");
const std::string HTTP_OUT_HEADER_PROXY_AUTHORIZATION("Proxy-Authorization");
const std::string HTTP_OUT_HEADER_RANGE("Range");
const std::string HTTP_OUT_HEADER_REFERER("Referer");
const std::string HTTP_OUT_HEADER_RETRY_AFTER("Retry-After");
const std::string HTTP_OUT_HEADER_SERVER("Server");
const std::string HTTP_OUT_HEADER_SET_COOKIE("Set-Cookie");
const std::string HTTP_OUT_HEADER_TE("TE");
const std::string HTTP_OUT_HEADER_TRAILER("Trailer");
const std::string HTTP_OUT_HEADER_TRANSFER_ENCODING("Transfer-Encoding");
const std::string HTTP_OUT_HEADER_UPGRADE("Upgrade");
const std::string HTTP_OUT_HEADER_USER_AGENT("User-Agent");
const std::string HTTP_OUT_HEADER_VARY("Vary");
const std::string HTTP_OUT_HEADER_VIA("Via");
const std::string HTTP_OUT_HEADER_WARNING("Warning");
const std::string HTTP_OUT_HEADER_WWW_AUTHENTICATE("WWW-Authenticate");

// Incoming headers are normalized to lower-case.
const std::string HTTP_IN_HEADER_ACCEPT_LANGUAGE("accept-language");
const std::string HTTP_IN_HEADER_CACHE_CONTROL("cache-control");
const std::string HTTP_IN_HEADER_CONTENT_LENGTH("content-length");
const std::string HTTP_IN_HEADER_CONTENT_LOCATION("content-location");
const std::string HTTP_IN_HEADER_CONTENT_TYPE("content-type");
const std::string HTTP_IN_HEADER_HOST("host");
const std::string HTTP_IN_HEADER_LOCATION("location");
const std::string HTTP_IN_HEADER_RETRY_AFTER("retry-after");
const std::string HTTP_IN_HEADER_SET_COOKIE("set-cookie");
const std::string HTTP_IN_HEADER_USER_AGENT("user-agent");
const std::string HTTP_IN_HEADER_X_FORWARDED_FOR("x-forwarded-for");

const std::string HTTP_CONTENT_LLSD_XML("application/llsd+xml");
const std::string HTTP_CONTENT_OCTET_STREAM("application/octet-stream");
const std::string HTTP_CONTENT_VND_LL_MESH("application/vnd.ll.mesh");
const std::string HTTP_CONTENT_XML("application/xml");
const std::string HTTP_CONTENT_JSON("application/json");
const std::string HTTP_CONTENT_TEXT_HTML("text/html");
const std::string HTTP_CONTENT_TEXT_HTML_UTF8("text/html; charset=utf-8");
const std::string HTTP_CONTENT_TEXT_PLAIN_UTF8("text/plain; charset=utf-8");
const std::string HTTP_CONTENT_TEXT_LLSD("text/llsd");
const std::string HTTP_CONTENT_TEXT_XML("text/xml");
const std::string HTTP_CONTENT_TEXT_LSL("text/lsl");
const std::string HTTP_CONTENT_TEXT_PLAIN("text/plain");
const std::string HTTP_CONTENT_IMAGE_X_J2C("image/x-j2c");
const std::string HTTP_CONTENT_IMAGE_J2C("image/j2c");
const std::string HTTP_CONTENT_IMAGE_JPEG("image/jpeg");
const std::string HTTP_CONTENT_IMAGE_PNG("image/png");
const std::string HTTP_CONTENT_IMAGE_BMP("image/bmp");

const std::string HTTP_NO_CACHE("no-cache");
const std::string HTTP_NO_CACHE_CONTROL("no-cache, max-age=0");

const std::string HTTP_VERB_INVALID("(invalid)");
const std::string HTTP_VERB_HEAD("HEAD");
const std::string HTTP_VERB_GET("GET");
const std::string HTTP_VERB_PUT("PUT");
const std::string HTTP_VERB_POST("POST");
const std::string HTTP_VERB_DELETE("DELETE");
const std::string HTTP_VERB_MOVE("MOVE");
const std::string HTTP_VERB_OPTIONS("OPTIONS");
const std::string HTTP_VERB_PATCH("PATCH");
const std::string HTTP_VERB_COPY("COPY");

const std::string& httpMethodAsVerb(EHTTPMethod method)
{
	static const std::string VERBS[] =
	{
		HTTP_VERB_INVALID,
		HTTP_VERB_HEAD,
		HTTP_VERB_GET,
		HTTP_VERB_PUT,
		HTTP_VERB_POST,
		HTTP_VERB_DELETE,
		HTTP_VERB_MOVE,
		HTTP_VERB_OPTIONS,
		HTTP_VERB_PATCH,
		HTTP_VERB_COPY
	};

	if ((S32)method <= 0 || (S32)method >= HTTP_METHOD_COUNT)
	{
		return VERBS[0];
	}
	return VERBS[method];
}
