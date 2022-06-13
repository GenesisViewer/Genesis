/**
 * @file lleventpoll.cpp
 * @brief Implementation of the LLEventPoll class.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "lleventpoll.h"
#include "llappviewer.h"
#include "llagent.h"

#include "llhttpclient.h"
#include "llhttpstatuscodes.h"
#include "llsdserialize.h"
#include "lleventtimer.h"
#include "llsdutil.h"

#include "llviewerregion.h"
#include "message.h"
#include "lltrans.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy eventPollResponder_timeout;

namespace
{
	// We will wait RETRY_SECONDS + (errorCount * RETRY_SECONDS_INC) before retrying after an error.
	// This means we attempt to recover relatively quickly but back off giving more time to recover
	// until we finally give up after MAX_EVENT_POLL_HTTP_ERRORS attempts.
	const F32 EVENT_POLL_ERROR_RETRY_SECONDS = 15.f; // ~ half of a normal timeout.
	const F32 EVENT_POLL_ERROR_RETRY_SECONDS_INC = 5.f; // ~ half of a normal timeout.
	const S32 MAX_EVENT_POLL_HTTP_ERRORS = 10; // ~5 minutes, by the above rules.

	class LLEventPollResponder : public LLHTTPClient::ResponderWithResult
	{
	public:
		
		static LLHTTPClient::ResponderPtr start(const std::string& pollURL, const LLHost& sender);
		void stop();
		
		void makeRequest();

	private:
		LLEventPollResponder(const std::string&	pollURL, const LLHost& sender);
		~LLEventPollResponder();

		
		void handleMessage(const LLSD& content);
		/*virtual*/ void httpFailure(void);
		/*virtual*/ void httpSuccess(void);
		/*virtual*/ bool is_event_poll(void) const { return true; }
		/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return eventPollResponder_timeout; }
		/*virtual*/ char const* getName(void) const { return "LLEventPollResponder"; }

	private:

		bool	mDone;

		std::string			mPollURL;
		std::string			mSender;
		
		LLSD	mAcknowledge;
		
		// these are only here for debugging so	we can see which poller	is which
		static int sCount;
		int	mCount;
		S32 mErrorCount;
	};

	class LLEventPollEventTimer : public LLEventTimer
	{
		typedef boost::intrusive_ptr<LLEventPollResponder> EventPollResponderPtr;

	public:
		LLEventPollEventTimer(F32 period, EventPollResponderPtr responder)
			: LLEventTimer(period), mResponder(responder)
		{ }

		virtual BOOL tick()
		{
			mResponder->makeRequest();
			return TRUE;	// Causes this instance to be deleted.
		}

	private:
		
		EventPollResponderPtr mResponder;
	};

	//static
	LLHTTPClient::ResponderPtr LLEventPollResponder::start(
		const std::string& pollURL, const LLHost& sender)
	{
		LLHTTPClient::ResponderPtr result = new LLEventPollResponder(pollURL, sender);
		LL_INFOS() << "LLEventPollResponder::start <" << sCount << "> "
				<< pollURL << LL_ENDL;
		return result;
	}

	void LLEventPollResponder::stop()
	{
		LL_INFOS() << "LLEventPollResponder::stop	<" << mCount <<	"> "
				<< mPollURL	<< LL_ENDL;
		// there should	be a way to	stop a LLHTTPClient	request	in progress
		mDone =	true;
	}

	int	LLEventPollResponder::sCount =	0;

	LLEventPollResponder::LLEventPollResponder(const std::string& pollURL, const LLHost& sender)
		: mDone(false),
		  mPollURL(pollURL),
		  mCount(++sCount),
		  mErrorCount(0)
	{
		//extract host and port of simulator to set as sender
		LLViewerRegion *regionp = gAgent.getRegion();
		if (!regionp)
		{
			LL_ERRS() << "LLEventPoll initialized before region is added." << LL_ENDL;
		}
		mSender = sender.getIPandPort();
		LL_INFOS() << "LLEventPoll initialized with sender " << mSender << LL_ENDL;
		makeRequest();
	}

	LLEventPollResponder::~LLEventPollResponder()
	{
		stop();
		LL_DEBUGS() <<	"LLEventPollResponder::~Impl <" <<	mCount << "> "
				 <<	mPollURL <<	LL_ENDL;
	}

	void LLEventPollResponder::makeRequest()
	{
		LLSD request;
		request["ack"] = mAcknowledge;
		request["done"]	= mDone;
		
		LL_DEBUGS() <<	"LLEventPollResponder::makeRequest	<" << mCount <<	"> ack = "
				 <<	LLSDXMLStreamer(mAcknowledge) << LL_ENDL;
		LLHTTPClient::post(mPollURL, request, this);
	}

	void LLEventPollResponder::handleMessage(const	LLSD& content)
	{
		std::string	msg_name	= content["message"];
		LLSD message;
		message["sender"] = mSender;
		message["body"] = content["body"];
		LLMessageSystem::dispatch(msg_name, message);
	}

	//virtual
	void LLEventPollResponder::httpFailure(void)
	{
		if (mDone) return;

		// Timeout
		if (is_internal_http_error_that_warrants_a_retry(mStatus))
		{ // A standard timeout response we get this when there are no events.
			mErrorCount = 0;
			makeRequest();
		}
		else if ( mStatus == HTTP_BAD_GATEWAY )
		{ // LEGACY: A HTTP_BAD_GATEWAY (502) error is our standard timeout response
		  // we get this when there are no events.
			mErrorCount = 0;
			makeRequest();
		}
		else if (mStatus == HTTP_NOT_FOUND)
		{   // Event polling for this server has been canceled.  In
			// some cases the server gets ahead of the viewer and will
			// return a 404 error (Not Found) before the cancel event
			// comes back in the queue
			LL_WARNS("LLEventPollImpl") << "Canceling coroutine" << LL_ENDL;
			stop();
		}
		else if (mCode != CURLE_OK)
		{
			/// Some LLCore or LIBCurl error was returned.  This is unlikely to be recoverable
		    LL_WARNS("LLEventPollImpl") << "Critical error from poll request returned from libraries.  Canceling coroutine." << LL_ENDL;
			stop();
		}
		else if (mErrorCount < MAX_EVENT_POLL_HTTP_ERRORS)
		{
			++mErrorCount;
			
			// The 'tick' will return TRUE causing the timer to delete this.
			new LLEventPollEventTimer(EVENT_POLL_ERROR_RETRY_SECONDS
										+ mErrorCount * EVENT_POLL_ERROR_RETRY_SECONDS_INC
									, this);

			LL_WARNS() << "Unexpected HTTP error.  status: " << mStatus << ", reason: " << mReason << LL_ENDL;
		}
		else
		{
			LL_WARNS() <<	"LLEventPollResponder::error: <" << mCount << "> got "
					<<	mStatus << ": " << mReason
					<<	(mDone ? " -- done"	: "") << LL_ENDL;
			stop();

			// At this point we have given up and the viewer will not receive HTTP messages from the simulator.
			// IMs, teleports, about land, selecing land, region crossing and more will all fail.
			// They are essentially disconnected from the region even though some things may still work.
			// Since things won't get better until they relog we force a disconnect now.

			/* Singu Note: There's no reason to disconnect, just because this failed a few too many times
			// *NOTE:Mani - The following condition check to see if this failing event poll
			// is attached to the Agent's main region. If so we disconnect the viewer.
			// Else... its a child region and we just leave the dead event poll stopped and 
			// continue running.
			if(gAgent.getRegion() && gAgent.getRegion()->getHost().getIPandPort() == mSender)
			{
				LL_WARNS() << "Forcing disconnect due to stalled main region event poll."  << LL_ENDL;
				LLAppViewer::instance()->forceDisconnect(LLTrans::getString("AgentLostConnection"));
			}
			*/
		}
	}

	//virtual
	void LLEventPollResponder::httpSuccess(void)
	{
		LL_DEBUGS() <<	"LLEventPollResponder::result <" << mCount	<< ">" 
				 <<	(mDone ? " -- done"	: "") << ll_pretty_print_sd(mContent)  << LL_ENDL; 
		
		if (mDone) return;

		mErrorCount = 0;

		if (!mContent.get("events") ||
			!mContent.get("id"))
		{
			//LL_WARNS() << "received event poll with no events or id key" << LL_ENDL;
			makeRequest();
			return;
		}
		
		mAcknowledge = mContent["id"];
		LLSD events	= mContent["events"];

		if(mAcknowledge.isUndefined())
		{
			LL_WARNS() << "LLEventPollResponder: id undefined" << LL_ENDL;
		}
		
		// was llinfos but now that CoarseRegionUpdate is TCP @ 1/second, it'd be too verbose for viewer logs. -MG
		LL_DEBUGS() << "LLEventPollResponder::completed <" <<	mCount << "> " << events.size() << "events (id "
				 <<	LLSDXMLStreamer(mAcknowledge) << ")" << LL_ENDL;
		
		LLSD::array_const_iterator i = events.beginArray();
		LLSD::array_const_iterator end = events.endArray();
		for	(; i !=	end; ++i)
		{
			if (i->has("message"))
			{
				handleMessage(*i);
			}
		}
		
		makeRequest();
	}	
}

LLEventPoll::LLEventPoll(const std::string&	poll_url, const LLHost& sender)
	: mImpl(LLEventPollResponder::start(poll_url, sender))
	{ }

LLEventPoll::~LLEventPoll()
{
	LLHTTPClient::ResponderBase* responderp = mImpl.get();
	LLEventPollResponder* event_poll_responder = dynamic_cast<LLEventPollResponder*>(responderp);
	if (event_poll_responder) event_poll_responder->stop();
}

