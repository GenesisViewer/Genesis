/** 
 * @file llmediadataclient.cpp
 * @brief class for queueing up requests for media data
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llmediadataclient.h"

#include "llhttpstatuscodes.h"
#include "llsdutil.h"
#include "llmediaentry.h"
#include "lltextureentry.h"
#include "llviewerregion.h"

//
// When making a request
// - obtain the "overall interest score" of the object.	 
//	 This would be the sum of the impls' interest scores.
// - put the request onto a queue sorted by this score 
//	 (highest score at the front of the queue)
// - On a timer, once a second, pull off the head of the queue and send 
//	 the request.
// - Any request that gets a 503 still goes through the retry logic
//

/***************************************************************************************************************
	What's up with this queueing code?

	First, a bit of background:

	Media on a prim was added into the system in the Viewer 2.0 timeframe.  In order to avoid changing the 
	network format of objects, an unused field in the object (the "MediaURL" string) was repurposed to 
	indicate that the object had media data, and also hold a sequence number and the UUID of the agent 
	who last updated the data.  The actual media data for objects is accessed via the "ObjectMedia" capability.  
	Due to concerns about sim performance, requests to this capability are rate-limited to 5 requests every 
	5 seconds per agent.

	The initial implementation of LLMediaDataClient used a single queue to manage requests to the "ObjectMedia" cap.  
	Requests to the cap were queued so that objects closer to the avatar were loaded in first, since they were most 
	likely to be the ones the media performance manager would load.

	This worked in some cases, but we found that it was possible for a scripted object that constantly updated its 
	media data to starve other objects, since the same queue contained both requests to load previously unseen media 
	data and requests to fetch media data in response to object updates.

	The solution for this we came up with was to have two queues.  The sorted queue contains requests to fetch media 
	data for objects that don't have it yet, and the round-robin queue contains requests to update media data for 
	objects that have already completed their initial load.  When both queues are non-empty, the code ping-pongs 
	between them so that updates can't completely block initial load-in.
**************************************************************************************************************/

//
// Forward decls
//
const F32 LLMediaDataClient::QUEUE_TIMER_DELAY = 1.0; // seconds(s)
const F32 LLMediaDataClient::UNAVAILABLE_RETRY_TIMER_DELAY = 5.0; // secs
const U32 LLMediaDataClient::MAX_RETRIES = 4;
const U32 LLMediaDataClient::MAX_SORTED_QUEUE_SIZE = 10000;
const U32 LLMediaDataClient::MAX_ROUND_ROBIN_QUEUE_SIZE = 10000;

// << operators
std::ostream& operator<<(std::ostream &s, const LLMediaDataClient::request_queue_t &q);
std::ostream& operator<<(std::ostream &s, const LLMediaDataClient::Request &q);

template <typename T>
typename T::iterator find_matching_request(T &c, const LLMediaDataClient::Request *request, LLMediaDataClient::Request::Type match_type)
{
	for(typename T::iterator iter = c.begin(); iter != c.end(); ++iter)
	{
		if(request->isMatch(*iter, match_type))
		{
			return iter;
		}
	}
	
	return c.end();
}

template <typename T>
typename T::iterator find_matching_request(T &c, const LLUUID &id, LLMediaDataClient::Request::Type match_type)
{
	for(typename T::iterator iter = c.begin(); iter != c.end(); ++iter)
	{
		if(((*iter)->getID() == id) && ((match_type == LLMediaDataClient::Request::ANY) || (match_type == (*iter)->getType())))
		{
			return iter;
		}
	}
	
	return c.end();
}

// NOTE: remove_matching_requests will not work correctly for containers where deleting an element may invalidate iterators
// to other elements in the container (such as std::vector).
// If the implementation is changed to use a container with this property, this will need to be revisited.
template <typename T>
void remove_matching_requests(T &c, const LLUUID &id, LLMediaDataClient::Request::Type match_type)
{
	for(typename T::iterator iter = c.begin(); iter != c.end();)
	{
		typename T::value_type i = *iter;
		typename T::iterator next = iter;
		next++;
		if((i->getID() == id) && ((match_type == LLMediaDataClient::Request::ANY) || (match_type == i->getType())))
		{
			i->markDead();
			c.erase(iter);
		}
		iter = next;
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//
// LLMediaDataClient
//
//////////////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::LLMediaDataClient(F32 queue_timer_delay,
									 F32 retry_timer_delay,
									 U32 max_retries,
									 U32 max_sorted_queue_size,
									 U32 max_round_robin_queue_size)
	: mQueueTimerDelay(queue_timer_delay),
	  mRetryTimerDelay(retry_timer_delay),
	  mMaxNumRetries(max_retries),
	  mMaxSortedQueueSize(max_sorted_queue_size),
	  mMaxRoundRobinQueueSize(max_round_robin_queue_size),
	  mQueueTimerIsRunning(false)
{
}

LLMediaDataClient::~LLMediaDataClient()
{
	stopQueueTimer();
}

bool LLMediaDataClient::isEmpty() const
{
	return mQueue.empty();
}

bool LLMediaDataClient::isInQueue(const LLMediaDataClientObject::ptr_t &object)
{
	if(find_matching_request(mQueue, object->getID(), LLMediaDataClient::Request::ANY) != mQueue.end())
		return true;
	
	if(find_matching_request(mUnQueuedRequests, object->getID(), LLMediaDataClient::Request::ANY) != mUnQueuedRequests.end())
		return true;
	
	return false;
}

void LLMediaDataClient::removeFromQueue(const LLMediaDataClientObject::ptr_t &object)
{
	LL_DEBUGS("LLMediaDataClient") << "removing requests matching ID " << object->getID() << LL_ENDL;
	remove_matching_requests(mQueue, object->getID(), LLMediaDataClient::Request::ANY);
	remove_matching_requests(mUnQueuedRequests, object->getID(), LLMediaDataClient::Request::ANY);
}

void LLMediaDataClient::startQueueTimer() 
{
	if (! mQueueTimerIsRunning)
	{
		LL_DEBUGS("LLMediaDataClient") << "starting queue timer (delay=" << mQueueTimerDelay << " seconds)" << LL_ENDL;
		// LLEventTimer automagically takes care of the lifetime of this object
		new QueueTimer(mQueueTimerDelay, this);
	}
	else { 
		LL_DEBUGS("LLMediaDataClient") << "queue timer is already running" << LL_ENDL;
	}
}

void LLMediaDataClient::stopQueueTimer()
{
	mQueueTimerIsRunning = false;
}

bool LLMediaDataClient::processQueueTimer()
{
	if(isEmpty())
		return true;

	LL_DEBUGS("LLMediaDataClient") << "QueueTimer::tick() started, queue size is:	  " << mQueue.size() << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "QueueTimer::tick() started, SORTED queue is:	  " << mQueue << LL_ENDL;
			
	serviceQueue();
	
	LL_DEBUGS("LLMediaDataClient") << "QueueTimer::tick() finished, queue size is:	  " << mQueue.size() << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "QueueTimer::tick() finished, SORTED queue is:	  " << mQueue << LL_ENDL;
	
	return isEmpty();
}

LLMediaDataClient::request_ptr_t LLMediaDataClient::dequeue()
{
	request_ptr_t request;
	request_queue_t *queue_p = getQueue();
	
	if (queue_p->empty())
	{
		LL_DEBUGS("LLMediaDataClient") << "queue empty: " << (*queue_p) << LL_ENDL;
	}
	else
	{
		request = queue_p->front();
		
		if(canServiceRequest(request))
		{
			// We will be returning this request, so remove it from the queue.
			queue_p->pop_front();
		}
		else
		{
			// Don't return this request -- it's not ready to be serviced.
			request = NULL;
		}
	}

	return request;
}

void LLMediaDataClient::pushBack(request_ptr_t request)
{
	request_queue_t *queue_p = getQueue();
	queue_p->push_front(request);
}

void LLMediaDataClient::trackRequest(request_ptr_t request)
{
	request_set_t::iterator iter = mUnQueuedRequests.find(request);
	
	if(iter != mUnQueuedRequests.end())
	{
		LL_WARNS("LLMediaDataClient") << "Tracking already tracked request: " << *request << LL_ENDL;
	}
	else
	{
		mUnQueuedRequests.insert(request);
	}
}

void LLMediaDataClient::stopTrackingRequest(request_ptr_t request)
{
	request_set_t::iterator iter = mUnQueuedRequests.find(request);
	
	if (iter != mUnQueuedRequests.end())
	{
		mUnQueuedRequests.erase(iter);
	}
	else
	{
		LL_WARNS("LLMediaDataClient") << "Removing an untracked request: " << *request << LL_ENDL;
	}
}

void LLMediaDataClient::serviceQueue()
{	
	// Peel one off of the items from the queue and execute it
	request_ptr_t request;
	
	do
	{
		request = dequeue();

		if(request.isNull())
		{
			// Queue is empty.
			return;
		}

		if(request->isDead())
		{
			LL_INFOS("LLMediaDataClient") << "Skipping dead request " << *request << LL_ENDL;
			continue;
		}

	} while(false);
		
	// try to send the HTTP message to the cap url
	std::string url = request->getCapability();
	if (!url.empty())
	{
		const LLSD &sd_payload = request->getPayload();
		LL_INFOS("LLMediaDataClient") << "Sending request for " << *request << LL_ENDL;
		
		// Add this request to the non-queued tracking list
		trackRequest(request);
		
		// and make the post
		LLHTTPClient::post(url, sd_payload, request->createResponder());
	}
	else 
	{
		// Cap url doesn't exist.
		
		if(request->getRetryCount() < mMaxNumRetries)
		{
			LL_WARNS("LLMediaDataClient") << "Could not send request " << *request << " (empty cap url), will retry." << LL_ENDL; 
			// Put this request back at the head of its queue, and retry next time the queue timer fires.
			request->incRetryCount();
			pushBack(request);
		}
		else
		{
			// This request has exceeded its maxumim retry count.  It will be dropped.
			LL_WARNS("LLMediaDataClient") << "Could not send request " << *request << " for " << mMaxNumRetries << " tries, dropping request." << LL_ENDL; 
		}

	}
}


// dump the queue
std::ostream& operator<<(std::ostream &s, const LLMediaDataClient::request_queue_t &q)
{
	int i = 0;
	LLMediaDataClient::request_queue_t::const_iterator iter = q.begin();
	LLMediaDataClient::request_queue_t::const_iterator end = q.end();
	while (iter != end)
	{
		s << "\t" << i << "]: " << (*iter)->getID().asString() << "(" << (*iter)->getObject()->getMediaInterest() << ")";
		iter++;
		i++;
	}
	return s;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// LLMediaDataClient::QueueTimer
// Queue of LLMediaDataClientObject smart pointers to request media for.
//
//////////////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::QueueTimer::QueueTimer(F32 time, LLMediaDataClient *mdc)
: LLEventTimer(time), mMDC(mdc)
{
	mMDC->setIsRunning(true);
}

// virtual
BOOL LLMediaDataClient::QueueTimer::tick()
{
	BOOL result = TRUE;
	
	if (!mMDC.isNull())
	{
		result = mMDC->processQueueTimer();
	
		if(result)
		{
			// This timer won't fire again.  
			mMDC->setIsRunning(false);
			mMDC = NULL;
		}
	}

	return result;
}


//////////////////////////////////////////////////////////////////////////////////////
//
// LLMediaDataClient::Responder::RetryTimer
//
//////////////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::RetryTimer::RetryTimer(F32 time, request_ptr_t request)
: LLEventTimer(time), mRequest(request)
{
	mRequest->startTracking();
}

// virtual
BOOL LLMediaDataClient::RetryTimer::tick()
{
	mRequest->stopTracking();

	if(mRequest->isDead())
	{
		LL_INFOS("LLMediaDataClient") << "RetryTimer fired for dead request: " << *mRequest << ", aborting." << LL_ENDL;
	}
	else
	{
		LL_INFOS("LLMediaDataClient") << "RetryTimer fired for: " << *mRequest << ", retrying." << LL_ENDL;
		mRequest->reEnqueue();
	}
	
	// Release the ref to the request.
	mRequest = NULL;

	// Don't fire again
	return TRUE;
}


//////////////////////////////////////////////////////////////////////////////////////
//
// LLMediaDataClient::Request
//
//////////////////////////////////////////////////////////////////////////////////////
/*static*/U32 LLMediaDataClient::Request::sNum = 0;

LLMediaDataClient::Request::Request(Type in_type,
									LLMediaDataClientObject *obj, 
									LLMediaDataClient *mdc,
									S32 face)
: mType(in_type),
  mObject(obj),
  mNum(++sNum), 
  mRetryCount(0),
  mMDC(mdc),
  mScore((F64)0.0),
  mFace(face)
{
	mObjectID = mObject->getID();
}

const char *LLMediaDataClient::Request::getCapName() const
{
	if(mMDC)
		return mMDC->getCapabilityName();
	
	return "";
}

std::string LLMediaDataClient::Request::getCapability() const
{
	if(mMDC)
	{
		return getObject()->getCapabilityUrl(getCapName());
	}
	
	return "";
}

const char *LLMediaDataClient::Request::getTypeAsString() const
{
	Type t = getType();
	switch (t)
	{
		case GET:
			return "GET";
			break;
		case UPDATE:
			return "UPDATE";
			break;
		case NAVIGATE:
			return "NAVIGATE";
			break;
		case ANY:
			return "ANY";
			break;
	}
	return "";
}


void LLMediaDataClient::Request::reEnqueue()
{
	if(mMDC)
	{
		mMDC->enqueue(this);
	}
}

F32 LLMediaDataClient::Request::getRetryTimerDelay() const
{
	if(mMDC)
		return mMDC->mRetryTimerDelay; 
		
	return 0.0f;
}

U32 LLMediaDataClient::Request::getMaxNumRetries() const
{
	if(mMDC)
		return mMDC->mMaxNumRetries;
	
	return 0;
}

void LLMediaDataClient::Request::updateScore()
{				
	F64 tmp = mObject->getMediaInterest();
	if (tmp != mScore)
	{
		LL_DEBUGS("LLMediaDataClient") << "Score for " << mObject->getID() << " changed from " << mScore << " to " << tmp << LL_ENDL; 
		mScore = tmp;
	}
}
		   
void LLMediaDataClient::Request::markDead() 
{ 
	mMDC = NULL;
}

bool LLMediaDataClient::Request::isDead() 
{ 
	return ((mMDC == NULL) || mObject->isDead()); 
}

void LLMediaDataClient::Request::startTracking() 
{ 
	if(mMDC) 
		mMDC->trackRequest(this); 
}

void LLMediaDataClient::Request::stopTracking() 
{ 
	if(mMDC) 
		mMDC->stopTrackingRequest(this); 
}

std::ostream& operator<<(std::ostream &s, const LLMediaDataClient::Request &r)
{
	s << "request: num=" << r.getNum() 
	<< " type=" << r.getTypeAsString() 
	<< " ID=" << r.getID() 
	<< " face=" << r.getFace() 
	<< " #retries=" << r.getRetryCount();
	return s;
}
			
//////////////////////////////////////////////////////////////////////////////////////
//
// LLMediaDataClient::Responder
//
//////////////////////////////////////////////////////////////////////////////////////

LLMediaDataClient::Responder::Responder(const request_ptr_t &request)
: mRequest(request)
{
}

/*virtual*/
void LLMediaDataClient::Responder::httpFailure(void)
{
	mRequest->stopTracking();

	if(mRequest->isDead())
	{
		LL_WARNS("LLMediaDataClient") << "dead request " << *mRequest << LL_ENDL;
		return;
	}
	
	if (mStatus == HTTP_SERVICE_UNAVAILABLE)
	{
		F32 retry_timeout = mRequest->getRetryTimerDelay();
		
		mRequest->incRetryCount();
		
		if (mRequest->getRetryCount() < mRequest->getMaxNumRetries()) 
		{
			LL_INFOS("LLMediaDataClient") << *mRequest << " got SERVICE_UNAVAILABLE...retrying in " << retry_timeout << " seconds" << LL_ENDL;
			
			// Start timer (instances are automagically tracked by
			// InstanceTracker<> and LLEventTimer)
			new RetryTimer(F32(retry_timeout/*secs*/), mRequest);
		}
		else 
		{
			LL_INFOS("LLMediaDataClient") << *mRequest << " got SERVICE_UNAVAILABLE...retry count " 
				<< mRequest->getRetryCount() << " exceeds " << mRequest->getMaxNumRetries() << ", not retrying" << LL_ENDL;
		}
	}
	else 
	{
		LL_WARNS("LLMediaDataClient") << *mRequest << " http error: " << dumpResponse() << LL_ENDL;
	}
}

/*virtual*/
void LLMediaDataClient::Responder::httpSuccess(void)
{
	mRequest->stopTracking();

	if(mRequest->isDead())
	{
		LL_WARNS("LLMediaDataClient") << "dead request " << *mRequest << LL_ENDL;
		return;
	}

	LL_DEBUGS("LLMediaDataClientResponse") << *mRequest << " result : " << ll_print_sd(mContent) << LL_ENDL;
}

//////////////////////////////////////////////////////////////////////////////////////
//
// LLObjectMediaDataClient
// Subclass of LLMediaDataClient for the ObjectMedia cap
//
//////////////////////////////////////////////////////////////////////////////////////

void LLObjectMediaDataClient::fetchMedia(LLMediaDataClientObject *object)
{
	// Create a get request and put it in the queue.
	enqueue(new RequestGet(object, this));
}

const char *LLObjectMediaDataClient::getCapabilityName() const 
{
	return "ObjectMedia";
}

LLObjectMediaDataClient::request_queue_t *LLObjectMediaDataClient::getQueue()
{
	return (mCurrentQueueIsTheSortedQueue) ? &mQueue : &mRoundRobinQueue;
}

void LLObjectMediaDataClient::sortQueue()
{
	if(!mQueue.empty())
	{
		// score all elements in the sorted queue.
		for(request_queue_t::iterator iter = mQueue.begin(); iter != mQueue.end(); iter++)
		{
			(*iter)->updateScore();
		}
		
		// Re-sort the list...
		mQueue.sort(compareRequestScores);
		
		// ...then cull items over the max
		U32 size = mQueue.size();
		if (size > mMaxSortedQueueSize) 
		{
			U32 num_to_cull = (size - mMaxSortedQueueSize);
			LL_INFOS_ONCE("LLMediaDataClient") << "sorted queue MAXED OUT!  Culling " 
				<< num_to_cull << " items" << LL_ENDL;
			while (num_to_cull-- > 0)
			{
				mQueue.back()->markDead();
				mQueue.pop_back();
			}
		}
	}
	
}

// static
bool LLObjectMediaDataClient::compareRequestScores(const request_ptr_t &o1, const request_ptr_t &o2)
{
	if (o2.isNull()) return true;
	if (o1.isNull()) return false;
	return ( o1->getScore() > o2->getScore() );
}

void LLObjectMediaDataClient::enqueue(Request *request)
{
	if(request->isDead())
	{
		LL_DEBUGS("LLMediaDataClient") << "not queueing dead request " << *request << LL_ENDL;
		return;
	}

	// Invariants:
	// new requests always go into the sorted queue.
	//  
	
	bool is_new = request->isNew();
	
	if(!is_new && (request->getType() == Request::GET))
	{
		// For GET requests that are not new, if a matching request is already in the round robin queue, 
		// in flight, or being retried, leave it at its current position.
		request_queue_t::iterator iter = find_matching_request(mRoundRobinQueue, request->getID(), Request::GET);
		request_set_t::iterator iter2 = find_matching_request(mUnQueuedRequests, request->getID(), Request::GET);
		
		if( (iter != mRoundRobinQueue.end()) || (iter2 != mUnQueuedRequests.end()) )
		{
			LL_DEBUGS("LLMediaDataClient") << "ALREADY THERE: NOT Queuing request for " << *request << LL_ENDL;

			return;
		}
	}
	
	// TODO: should an UPDATE cause pending GET requests for the same object to be removed from the queue?
	// IF the update will cause an object update message to be sent out at some point in the future, it probably should.
	
	// Remove any existing requests of this type for this object
	remove_matching_requests(mQueue, request->getID(), request->getType());
	remove_matching_requests(mRoundRobinQueue, request->getID(), request->getType());
	remove_matching_requests(mUnQueuedRequests, request->getID(), request->getType());

	if (is_new)
	{
		LL_DEBUGS("LLMediaDataClient") << "Queuing SORTED request for " << *request << LL_ENDL;
		
		mQueue.push_back(request);
		
		LL_DEBUGS("LLMediaDataClientQueue") << "SORTED queue:" << mQueue << LL_ENDL;
	}
	else
	{
		if (mRoundRobinQueue.size() > mMaxRoundRobinQueueSize) 
		{
			LL_INFOS_ONCE("LLMediaDataClient") << "RR QUEUE MAXED OUT!!!" << LL_ENDL;
			LL_DEBUGS("LLMediaDataClient") << "Not queuing " << *request << LL_ENDL;
			return;
		}
				
		LL_DEBUGS("LLMediaDataClient") << "Queuing RR request for " << *request << LL_ENDL;
		// Push the request on the pending queue
		mRoundRobinQueue.push_back(request);
		
		LL_DEBUGS("LLMediaDataClientQueue") << "RR queue:" << mRoundRobinQueue << LL_ENDL;			
	}	
	// Start the timer if not already running
	startQueueTimer();
}

bool LLObjectMediaDataClient::canServiceRequest(request_ptr_t request) 
{
	if(mCurrentQueueIsTheSortedQueue)
	{
		if(!request->getObject()->isInterestingEnough())
		{
			LL_DEBUGS("LLMediaDataClient") << "Not fetching " << *request << ": not interesting enough" << LL_ENDL;
			return false;
		}
	}
	
	return true; 
};

void LLObjectMediaDataClient::swapCurrentQueue()
{
	// Swap
	mCurrentQueueIsTheSortedQueue = !mCurrentQueueIsTheSortedQueue;
	// If its empty, swap back
	if (getQueue()->empty()) 
	{
		mCurrentQueueIsTheSortedQueue = !mCurrentQueueIsTheSortedQueue;
	}
}

bool LLObjectMediaDataClient::isEmpty() const
{
	return mQueue.empty() && mRoundRobinQueue.empty();
}

bool LLObjectMediaDataClient::isInQueue(const LLMediaDataClientObject::ptr_t &object)
{
	// First, call parent impl.
	if(LLMediaDataClient::isInQueue(object))
		return true;

	if(find_matching_request(mRoundRobinQueue, object->getID(), LLMediaDataClient::Request::ANY) != mRoundRobinQueue.end())
		return true;
	
	return false;
}

void LLObjectMediaDataClient::removeFromQueue(const LLMediaDataClientObject::ptr_t &object)
{
	// First, call parent impl.
	LLMediaDataClient::removeFromQueue(object);
	
	remove_matching_requests(mRoundRobinQueue, object->getID(), LLMediaDataClient::Request::ANY);
}

bool LLObjectMediaDataClient::processQueueTimer()
{
	if(isEmpty())
		return true;
		
	LL_DEBUGS("LLMediaDataClient") << "started, SORTED queue size is:	  " << mQueue.size() 
		<< ", RR queue size is:	  " << mRoundRobinQueue.size() << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "    SORTED queue is:	  " << mQueue << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "    RR queue is:	  " << mRoundRobinQueue << LL_ENDL;

//	purgeDeadRequests();

	sortQueue();

	LL_DEBUGS("LLMediaDataClientQueue") << "after sort, SORTED queue is:	  " << mQueue << LL_ENDL;
	
	serviceQueue();

	swapCurrentQueue();
	
	LL_DEBUGS("LLMediaDataClient") << "finished, SORTED queue size is:	  " << mQueue.size() 
		<< ", RR queue size is:	  " << mRoundRobinQueue.size() << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "    SORTED queue is:	  " << mQueue << LL_ENDL;
	LL_DEBUGS("LLMediaDataClientQueue") << "    RR queue is:	  " << mRoundRobinQueue << LL_ENDL;
	
	return isEmpty();
}

LLObjectMediaDataClient::RequestGet::RequestGet(LLMediaDataClientObject *obj, LLMediaDataClient *mdc):
	LLMediaDataClient::Request(LLMediaDataClient::Request::GET, obj, mdc)
{
}

LLSD LLObjectMediaDataClient::RequestGet::getPayload() const
{
	LLSD result;
	result["verb"] = "GET";
	result[LLTextureEntry::OBJECT_ID_KEY] = mObject->getID();
	
	return result;
}

LLMediaDataClient::Responder *LLObjectMediaDataClient::RequestGet::createResponder()
{
	return new LLObjectMediaDataClient::Responder(this);
}


void LLObjectMediaDataClient::updateMedia(LLMediaDataClientObject *object)
{
	// Create an update request and put it in the queue.
	enqueue(new RequestUpdate(object, this));
}

LLObjectMediaDataClient::RequestUpdate::RequestUpdate(LLMediaDataClientObject *obj, LLMediaDataClient *mdc):
	LLMediaDataClient::Request(LLMediaDataClient::Request::UPDATE, obj, mdc)
{
}

LLSD LLObjectMediaDataClient::RequestUpdate::getPayload() const
{
	LLSD result;
	result["verb"] = "UPDATE";
	result[LLTextureEntry::OBJECT_ID_KEY] = mObject->getID();

	LLSD object_media_data;
	int i = 0;
	int end = mObject->getMediaDataCount();
	for ( ; i < end ; ++i) 
	{
		object_media_data.append(mObject->getMediaDataLLSD(i));
	}
	
	result[LLTextureEntry::OBJECT_MEDIA_DATA_KEY] = object_media_data;
	
	return result;
}

LLMediaDataClient::Responder *LLObjectMediaDataClient::RequestUpdate::createResponder()
{
	// This just uses the base class's responder.
	return new LLMediaDataClient::Responder(this);
}


/*virtual*/
void LLObjectMediaDataClient::Responder::httpSuccess(void)
{
	getRequest()->stopTracking();

	if(getRequest()->isDead())
	{
		LL_WARNS("LLMediaDataClient") << "dead request " << *(getRequest()) << LL_ENDL;
		return;
	}

	// This responder is only used for GET requests, not UPDATE.

	LL_DEBUGS("LLMediaDataClientResponse") << *(getRequest()) << " GET returned: " << ll_print_sd(mContent) << LL_ENDL;
	
	// Look for an error
	if (mContent.has("error"))
	{
		const LLSD &error = mContent["error"];
		LL_WARNS("LLMediaDataClient") << *(getRequest()) << " Error getting media data for object: code=" << 
			error["code"].asString() << ": " << error["message"].asString() << LL_ENDL;
		
		// XXX Warn user?
	}
	else 
	{
		// Check the data
		const LLUUID &object_id = mContent[LLTextureEntry::OBJECT_ID_KEY];
		if (object_id != getRequest()->getObject()->getID()) 
		{
			// NOT good, wrong object id!!
			LL_WARNS("LLMediaDataClient") << *(getRequest()) << " DROPPING response with wrong object id (" << object_id << ")" << LL_ENDL;
			return;
		}
		
		// Otherwise, update with object media data
		getRequest()->getObject()->updateObjectMediaData(mContent[LLTextureEntry::OBJECT_MEDIA_DATA_KEY],
														 mContent[LLTextureEntry::MEDIA_VERSION_KEY]);
	}
}

//////////////////////////////////////////////////////////////////////////////////////
//
// LLObjectMediaNavigateClient
// Subclass of LLMediaDataClient for the ObjectMediaNavigate cap
//
//////////////////////////////////////////////////////////////////////////////////////

const char *LLObjectMediaNavigateClient::getCapabilityName() const 
{
	return "ObjectMediaNavigate";
}

void LLObjectMediaNavigateClient::enqueue(Request *request)
{
	if(request->isDead())
	{
		LL_DEBUGS("LLMediaDataClient") << "not queueing dead request " << *request << LL_ENDL;
		return;
	}
	
	// If there's already a matching request in the queue, remove it.
	request_queue_t::iterator iter = find_matching_request(mQueue, request, LLMediaDataClient::Request::ANY);
	if(iter != mQueue.end())
	{
		LL_DEBUGS("LLMediaDataClient") << "removing matching queued request " << (**iter) << LL_ENDL;
		mQueue.erase(iter);
	}
	else
	{
		request_set_t::iterator set_iter = find_matching_request(mUnQueuedRequests, request, LLMediaDataClient::Request::ANY);
		if(set_iter != mUnQueuedRequests.end())
		{
			LL_DEBUGS("LLMediaDataClient") << "removing matching unqueued request " << (**set_iter) << LL_ENDL;
			mUnQueuedRequests.erase(set_iter);
		}
	}

#if 0
	// Sadly, this doesn't work.  It ends up creating a race condition when the user navigates and then hits the "back" button
	// where the navigate-back appears to be spurious and doesn't get broadcast.	
	if(request->getObject()->isCurrentMediaUrl(request->getFace(), request->getURL()))
	{
		// This navigate request is trying to send the face to the current URL.  Drop it.
		LL_DEBUGS("LLMediaDataClient") << "dropping spurious request " << (*request) << LL_ENDL;
	}
	else
#endif
	{
		LL_DEBUGS("LLMediaDataClient") << "queueing new request " << (*request) << LL_ENDL;
		mQueue.push_back(request);
		
		// Start the timer if not already running
		startQueueTimer();
	}
}

void LLObjectMediaNavigateClient::navigate(LLMediaDataClientObject *object, U8 texture_index, const std::string &url)
{

//	LL_INFOS("LLMediaDataClient") << "navigate() initiated: " << ll_print_sd(sd_payload) << LL_ENDL;
	
	// Create a get request and put it in the queue.
	enqueue(new RequestNavigate(object, this, texture_index, url));
}

LLObjectMediaNavigateClient::RequestNavigate::RequestNavigate(LLMediaDataClientObject *obj, LLMediaDataClient *mdc, U8 texture_index, const std::string &url):
	LLMediaDataClient::Request(LLMediaDataClient::Request::NAVIGATE, obj, mdc, (S32)texture_index),
	mURL(url)
{
}

LLSD LLObjectMediaNavigateClient::RequestNavigate::getPayload() const
{
	LLSD result;
	result[LLTextureEntry::OBJECT_ID_KEY] = getID();
	result[LLMediaEntry::CURRENT_URL_KEY] = mURL;
	result[LLTextureEntry::TEXTURE_INDEX_KEY] = (LLSD::Integer)getFace();
	
	return result;
}

LLMediaDataClient::Responder *LLObjectMediaNavigateClient::RequestNavigate::createResponder()
{
	return new LLObjectMediaNavigateClient::Responder(this);
}

/*virtual*/
void LLObjectMediaNavigateClient::Responder::httpFailure(void)
{
	getRequest()->stopTracking();

	if(getRequest()->isDead())
	{
		LL_WARNS("LLMediaDataClient") << "dead request " << *(getRequest()) << LL_ENDL;
		return;
	}

	// Bounce back (unless HTTP_SERVICE_UNAVAILABLE, in which case call base
	// class
	if (mStatus == HTTP_SERVICE_UNAVAILABLE)
	{
		LLMediaDataClient::Responder::httpFailure();
	}
	else
	{
		// bounce the face back
		LL_WARNS("LLMediaDataClient") << *(getRequest()) << " Error navigating: http code=" << mStatus << LL_ENDL;
		const LLSD &payload = getRequest()->getPayload();
		// bounce the face back
		getRequest()->getObject()->mediaNavigateBounceBack((LLSD::Integer)payload[LLTextureEntry::TEXTURE_INDEX_KEY]);
	}
}

/*virtual*/
void LLObjectMediaNavigateClient::Responder::httpSuccess(void)
{
	getRequest()->stopTracking();

	if(getRequest()->isDead())
	{
		LL_WARNS("LLMediaDataClient") << "dead request " << *(getRequest()) << LL_ENDL;
		return;
	}

	LL_INFOS("LLMediaDataClient") << *(getRequest()) << " NAVIGATE returned " << ll_print_sd(mContent) << LL_ENDL;
	
	if (mContent.has("error"))
	{
		const LLSD &error = mContent["error"];
		int error_code = error["code"];
		
		if (ERROR_PERMISSION_DENIED_CODE == error_code)
		{
			LL_WARNS("LLMediaDataClient") << *(getRequest()) << " Navigation denied: bounce back" << LL_ENDL;
			const LLSD &payload = getRequest()->getPayload();
			// bounce the face back
			getRequest()->getObject()->mediaNavigateBounceBack((LLSD::Integer)payload[LLTextureEntry::TEXTURE_INDEX_KEY]);
		}
		else
		{
			LL_WARNS("LLMediaDataClient") << *(getRequest()) << " Error navigating: code=" << 
				error["code"].asString() << ": " << error["message"].asString() << LL_ENDL;
		}			 

		// XXX Warn user?
	}
	else 
	{
		// No action required.
		LL_DEBUGS("LLMediaDataClientResponse") << *(getRequest()) << " result : " << ll_print_sd(mContent) << LL_ENDL;
	}
}
