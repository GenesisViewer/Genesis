/**
 * @file llproxy.cpp
 * @brief UDP and HTTP proxy communications
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llproxy.h"

#include <string>

#include "llapr.h"
#include "llcurl.h"
#include "llhost.h"

// Static class variable instances

// We want this to be static to avoid excessive indirection on every
// incoming packet just to do a simple bool test. The getter for this
// member is also static
bool LLProxy::sUDPProxyEnabled = false;

// Some helpful TCP static functions.
static apr_status_t tcp_blocking_handshake(LLSocket::ptr_t handle, char * dataout, apr_size_t outlen, char * datain, apr_size_t maxinlen); // Do a TCP data handshake
static LLSocket::ptr_t tcp_open_channel(LLHost host); // Open a TCP channel to a given host
static void tcp_close_channel(LLSocket::ptr_t* handle_ptr); // Close an open TCP channel

ProxyShared::ProxyShared(void):
		mProxyType(LLPROXY_SOCKS),
		mAuthMethodSelected(METHOD_NOAUTH)
{
}

LLProxy::LLProxy():
		mHTTPProxyEnabled(false)
{
}

LLProxy::~LLProxy()
{
	stopSOCKSProxy();
	Shared_wat shared_w(mShared);
	disableHTTPProxy(shared_w);
}

/**
 * @brief Open the SOCKS 5 TCP control channel.
 *
 * Perform a SOCKS 5 authentication and UDP association with the proxy server.
 *
 * @param proxy The SOCKS 5 server to connect to.
 * @return SOCKS_OK if successful, otherwise a socks error code from llproxy.h.
 */
S32 LLProxy::proxyHandshake(LLHost proxy)
{
	S32 result;

	Unshared_rat unshared_r(mUnshared);
	Shared_rat shared_r(mShared);

	/* SOCKS 5 Auth request */
	socks_auth_request_t  socks_auth_request;
	socks_auth_response_t socks_auth_response;

	socks_auth_request.version		= SOCKS_VERSION;				// SOCKS version 5
	socks_auth_request.num_methods	= 1;							// Sending 1 method.
	socks_auth_request.methods		= getSelectedAuthMethod(shared_r);		// Send only the selected method.

	result = tcp_blocking_handshake(unshared_r->mProxyControlChannel,
									static_cast<char*>(static_cast<void*>(&socks_auth_request)),
									sizeof(socks_auth_request),
									static_cast<char*>(static_cast<void*>(&socks_auth_response)),
									sizeof(socks_auth_response));
	if (result != APR_SUCCESS)
	{
		LL_WARNS("Proxy") << "SOCKS authentication request failed, error on TCP control channel : " << result << LL_ENDL;
		stopSOCKSProxy();
		return SOCKS_CONNECT_ERROR;
	}

	if (socks_auth_response.method == AUTH_NOT_ACCEPTABLE)
	{
		LL_WARNS("Proxy") << "SOCKS 5 server refused all our authentication methods." << LL_ENDL;
		stopSOCKSProxy();
		return SOCKS_NOT_ACCEPTABLE;
	}

	/* SOCKS 5 USERNAME/PASSWORD authentication */
	if (socks_auth_response.method == METHOD_PASSWORD)
	{
		// The server has requested a username/password combination
		std::string socks_username(getSocksUser(shared_r));
		std::string socks_password(getSocksPwd(shared_r));
		U32 request_size = socks_username.size() + socks_password.size() + 3;
		char * password_auth = new char[request_size];
		password_auth[0] = 0x01;
		password_auth[1] = static_cast<char>(socks_username.size());
		memcpy(&password_auth[2], socks_username.c_str(), socks_username.size());
		password_auth[socks_username.size() + 2] = static_cast<char>(socks_password.size());
		memcpy(&password_auth[socks_username.size() + 3], socks_password.c_str(), socks_password.size());

		authmethod_password_reply_t password_reply;

		result = tcp_blocking_handshake(unshared_r->mProxyControlChannel,
										password_auth,
										request_size,
										static_cast<char*>(static_cast<void*>(&password_reply)),
										sizeof(password_reply));
		delete[] password_auth;

		if (result != APR_SUCCESS)
		{
			LL_WARNS("Proxy") << "SOCKS authentication failed, error on TCP control channel : " << result << LL_ENDL;
			stopSOCKSProxy();
			return SOCKS_CONNECT_ERROR;
		}

		if (password_reply.status != AUTH_SUCCESS)
		{
			LL_WARNS("Proxy") << "SOCKS authentication failed" << LL_ENDL;
			stopSOCKSProxy();
			return SOCKS_AUTH_FAIL;
		}
	}

	/* SOCKS5 connect request */

	socks_command_request_t  connect_request;
	socks_command_response_t connect_reply;

	connect_request.version		= SOCKS_VERSION;         // SOCKS V5
	connect_request.command		= COMMAND_UDP_ASSOCIATE; // Associate UDP
	connect_request.reserved	= FIELD_RESERVED;
	connect_request.atype		= ADDRESS_IPV4;
	connect_request.address		= htonl(0); // 0.0.0.0
	connect_request.port		= htons(0); // 0
	// "If the client is not in possession of the information at the time of the UDP ASSOCIATE,
	//  the client MUST use a port number and address of all zeros. RFC 1928"

	result = tcp_blocking_handshake(unshared_r->mProxyControlChannel,
									static_cast<char*>(static_cast<void*>(&connect_request)),
									sizeof(connect_request),
									static_cast<char*>(static_cast<void*>(&connect_reply)),
									sizeof(connect_reply));
	if (result != APR_SUCCESS)
	{
		LL_WARNS("Proxy") << "SOCKS connect request failed, error on TCP control channel : " << result << LL_ENDL;
		stopSOCKSProxy();
		return SOCKS_CONNECT_ERROR;
	}

	if (connect_reply.reply != REPLY_REQUEST_GRANTED)
	{
		LL_WARNS("Proxy") << "Connection to SOCKS 5 server failed, UDP forward request not granted" << LL_ENDL;
		stopSOCKSProxy();
		return SOCKS_UDP_FWD_NOT_GRANTED;
	}

	{
		// Write access type and read access type are really the same, so unshared_w must be simply a reference.
		Unshared_wat& unshared_w = unshared_r;
		unshared_w->mUDPProxy.setPort(ntohs(connect_reply.port)); // reply port is in network byte order
		unshared_w->mUDPProxy.setAddress(proxy.getAddress());
	}
	// The connection was successful. We now have the UDP port to send requests that need forwarding to.
	LL_INFOS("Proxy") << "SOCKS 5 UDP proxy connected on " << unshared_r->mUDPProxy << LL_ENDL;

	return SOCKS_OK;
}

/**
 * @brief Initiates a SOCKS 5 proxy session.
 *
 * Performs basic checks on host to verify that it is a valid address. Opens the control channel
 * and then negotiates the proxy connection with the server. Closes any existing SOCKS
 * connection before proceeding. Also disables an HTTP proxy if it is using SOCKS as the proxy.
 *
 *
 * @param host Socks server to connect to.
 * @return SOCKS_OK if successful, otherwise a SOCKS error code defined in llproxy.h.
 */
S32 LLProxy::startSOCKSProxy(LLHost host)
{
	Unshared_wat unshared_w(mUnshared);

	if (host.isOk())
	{
		unshared_w->mTCPProxy = host;
	}
	else
	{
		return SOCKS_INVALID_HOST;
	}

	// Close any running SOCKS connection.
	stopSOCKSProxy();

	unshared_w->mProxyControlChannel = tcp_open_channel(unshared_w->mTCPProxy);
	if (!unshared_w->mProxyControlChannel)
	{
		return SOCKS_HOST_CONNECT_FAILED;
	}

	S32 status = proxyHandshake(unshared_w->mTCPProxy);

	if (status != SOCKS_OK)
	{
		// Shut down the proxy if any of the above steps failed.
		stopSOCKSProxy();
	}
	else
	{
		// Connection was successful.
		sUDPProxyEnabled = true;
	}

	return status;
}

/**
 * @brief Stop using the SOCKS 5 proxy.
 *
 * This will stop sending UDP packets through the SOCKS 5 proxy
 * and will also stop the HTTP proxy if it is configured to use SOCKS.
 * The proxy control channel will also be disconnected.
 */
void LLProxy::stopSOCKSProxy()
{
	sUDPProxyEnabled = false;

	// If the SOCKS proxy is requested to stop and we are using that for HTTP as well
	// then we must shut down any HTTP proxy operations. But it is allowable if web
	// proxy is being used to continue proxying HTTP.

	Shared_rat shared_r(mShared);
	if (LLPROXY_SOCKS == getHTTPProxyType(shared_r))
	{
		Shared_wat shared_w(shared_r);
		disableHTTPProxy(shared_w);
	}
	Unshared_wat unshared_w(mUnshared);
	if (unshared_w->mProxyControlChannel)
	{
		tcp_close_channel(&unshared_w->mProxyControlChannel);
	}
}

/**
 * @brief Set the proxy's SOCKS authentication method to none.
 */
void LLProxy::setAuthNone()
{
	Shared_wat(mShared)->mAuthMethodSelected = METHOD_NOAUTH;
}

/**
 * @brief Set the proxy's SOCKS authentication method to password.
 *
 * Check whether the lengths of the supplied username
 * and password conform to the lengths allowed by the
 * SOCKS protocol.
 *
 * @param 	username The SOCKS username to send.
 * @param 	password The SOCKS password to send.
 * @return  Return true if applying the settings was successful. No changes are made if false.
 *
 */
bool LLProxy::setAuthPassword(const std::string &username, const std::string &password)
{
	if (username.length() > SOCKSMAXUSERNAMELEN || password.length() > SOCKSMAXPASSWORDLEN ||
			username.length() < SOCKSMINUSERNAMELEN || password.length() < SOCKSMINPASSWORDLEN)
	{
		LL_WARNS("Proxy") << "Invalid SOCKS 5 password or username length." << LL_ENDL;
		return false;
	}

	Shared_wat shared_w(mShared);
	shared_w->mAuthMethodSelected = METHOD_PASSWORD;
	shared_w->mSocksUsername      = username;
	shared_w->mSocksPassword      = password;

	return true;
}

/**
 * @brief Enable the HTTP proxy for either SOCKS or HTTP.
 *
 * Check the supplied host to see if it is a valid IP and port.
 *
 * @param httpHost Proxy server to connect to.
 * @param type Is the host a SOCKS or HTTP proxy.
 * @return Return true if applying the setting was successful. No changes are made if false.
 */
bool LLProxy::enableHTTPProxy(LLHost httpHost, LLHttpProxyType type)
{
	if (!httpHost.isOk())
	{
		LL_WARNS("Proxy") << "Invalid SOCKS 5 Server" << LL_ENDL;
		return false;
	}

	Shared_wat shared_w(mShared);
	mHTTPProxyEnabled = true;
	shared_w->mHTTPProxy        = httpHost;
	shared_w->mProxyType        = type;

	return true;
}

/**
 * @brief Enable the HTTP proxy without changing the proxy settings.
 *
 * This should not be called unless the proxy has already been set up.
 *
 * @return Return true only if the current settings are valid and the proxy was enabled.
 */
bool LLProxy::enableHTTPProxy()
{
	bool ok;

	Shared_rat shared_r(mShared);
	ok = (shared_r->mHTTPProxy.isOk());
	if (ok)
	{
		mHTTPProxyEnabled = true;
	}

	return ok;
}

/**
 * @brief Stop the LLProxy and make certain that any APR pools and classes are deleted before terminating APR.
 *
 * Deletes the LLProxy singleton, destroying the APR pool used by the control channel as well as .
 */
//static
void LLProxy::cleanupClass()
{
	getInstance()->stopSOCKSProxy();
	deleteSingleton();
}

/**
 * @brief Send one TCP packet and receive one in return.
 *
 * This operation is done synchronously with a 1000ms timeout. Therefore, it should not be used when a blocking
 * operation would impact the operation of the viewer.
 *
 * @param handle_ptr 	Pointer to a connected LLSocket of type STREAM_TCP.
 * @param dataout		Data to send.
 * @param outlen		Length of dataout.
 * @param datain		Buffer for received data. Undefined if return value is not APR_SUCCESS.
 * @param maxinlen		Maximum possible length of received data.  Short reads are allowed.
 * @return 				Indicates APR status code of exchange. APR_SUCCESS if exchange was successful, -1 if invalid data length was received.
 */
static apr_status_t tcp_blocking_handshake(LLSocket::ptr_t handle, char * dataout, apr_size_t outlen, char * datain, apr_size_t maxinlen)
{
	apr_socket_t* apr_socket = handle->getSocket();
	apr_status_t rv = APR_SUCCESS;

	apr_size_t expected_len = outlen;

	handle->setBlocking(1000);

  	rv = apr_socket_send(apr_socket, dataout, &outlen);
	if (APR_SUCCESS != rv)
	{
		LL_WARNS("Proxy") << "Error sending data to proxy control channel, status: " << rv << LL_ENDL;
		ll_apr_warn_status(rv);
	}
	else if (expected_len != outlen)
	{
		LL_WARNS("Proxy") << "Incorrect data length sent. Expected: " << expected_len <<
				" Sent: " << outlen << LL_ENDL;
		rv = -1;
	}

	if (APR_SUCCESS == rv)
	{
		expected_len = maxinlen;
		rv = apr_socket_recv(apr_socket, datain, &maxinlen);
		if (rv != APR_SUCCESS)
		{
			LL_WARNS("Proxy") << "Error receiving data from proxy control channel, status: " << rv << LL_ENDL;
			ll_apr_warn_status(rv);
		}
		else if (expected_len < maxinlen)
		{
			LL_WARNS("Proxy") << "Incorrect data length received. Expected: " << expected_len <<
					" Received: " << maxinlen << LL_ENDL;
			rv = -1;
		}
	}

	handle->setNonBlocking();

	return rv;
}

/**
 * @brief Open a LLSocket and do a blocking connect to the chosen host.
 *
 * Checks for a successful connection, and makes sure the connection is closed if it fails.
 *
 * @param host		The host to open the connection to.
 * @return			The created socket.  Will evaluate as NULL if the connection is unsuccessful.
 */
static LLSocket::ptr_t tcp_open_channel(LLHost host)
{
	LLSocket::ptr_t socket = LLSocket::create(LLSocket::STREAM_TCP);
	bool connected = socket->blockingConnect(host);
	if (!connected)
	{
		tcp_close_channel(&socket);
	}

	return socket;
}

/**
 * @brief Close the socket.
 *
 * @param handle_ptr The handle of the socket being closed. A pointer-to-pointer to avoid increasing the use count.
 */
static void tcp_close_channel(LLSocket::ptr_t* handle_ptr)
{
	LL_DEBUGS("Proxy") << "Resetting proxy LLSocket handle, use_count == " << handle_ptr->use_count() << LL_ENDL;
	handle_ptr->reset();
}
