/** 
 * @file llavatarnamecache.cpp
 * @brief Provides lookup of avatar SLIDs ("bobsmith123") and display names
 * ("James Cook") from avatar UUIDs.
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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
#include "linden_common.h"

#include "llavatarnamecache.h"

#include "llcachename.h"		// we wrap this system
#include "llcontrol.h"		// For LLCachedControl
#include "llframetimer.h"
#include "llhttpclient.h"
#include "llsd.h"
#include "llsdserialize.h"

#include <boost/tokenizer.hpp>

#include <map>
#include <set>

namespace LLAvatarNameCache
{
	use_display_name_signal_t mUseDisplayNamesSignal;

// [RLVa:KB] - Checked: 2010-12-08 (RLVa-1.4.0a) | Added: RLVa-1.2.2c
	// RLVa override for display names
	bool sForceDisplayNames = false;
// [/RLVa:KB]

	// Cache starts in a paused state until we can determine if the
	// current region supports display names.
	bool sRunning = false;
	
	// Use the People API (modern) for fetching name if true. Use the old legacy protocol if false.
	// For testing, there's a UsePeopleAPI setting that can be flipped (must restart viewer).
	bool sUsePeopleAPI = true;

	// Base lookup URL for name service.
	// On simulator, loaded from indra.xml
	// On viewer, usually a simulator capability (at People API team's request)
	// Includes the trailing slash, like "http://pdp60.lindenlab.com:8000/agents/"
	std::string sNameLookupURL;

	// Accumulated agent IDs for next query against service
	typedef uuid_set_t ask_queue_t;
	ask_queue_t sAskQueue;

	// Agent IDs that have been requested, but with no reply.
	// Maps agent ID to frame time request was made.
	typedef std::map<LLUUID, F64> pending_queue_t;
	pending_queue_t sPendingQueue;

	// Callbacks to fire when we received a name.
	// May have multiple callbacks for a single ID, which are
	// represented as multiple slots bound to the signal.
	// Avoid copying signals via pointers.
	typedef std::map<LLUUID, callback_signal_t*> signal_map_t;
	signal_map_t sSignalMap;

	// The cache at last, i.e. avatar names we know about.
	typedef std::map<LLUUID, LLAvatarName> cache_t;
	cache_t sCache;

	// Send bulk lookup requests a few times a second at most.
	// Only need per-frame timing resolution.
	LLFrameTimer sRequestTimer;

    // Maximum time an unrefreshed cache entry is allowed.
    const F64 MAX_UNREFRESHED_TIME = 20.0 * 60.0;

    // Time when unrefreshed cached names were checked last.
    static F64 sLastExpireCheck;

	// Time-to-live for a temp cache entry.
	const F64 TEMP_CACHE_ENTRY_LIFETIME = 60.0;

	//-----------------------------------------------------------------------
	// Internal methods
	//-----------------------------------------------------------------------

	// Handle name response off network.
	void processName(const LLUUID& agent_id,
					 const LLAvatarName& av_name);

	void requestNamesViaCapability();

	// Legacy name system callbacks
	void legacyNameCallback(const LLUUID& agent_id,
							const std::string& full_name,
							bool is_group);
	void legacyNameFetch(const LLUUID& agent_id,
						 const std::string& full_name,
						 bool is_group);

	void requestNamesViaLegacy();

	// Do a single callback to a given slot
	void fireSignal(const LLUUID& agent_id,
					const callback_slot_t& slot,
					const LLAvatarName& av_name);
	
	// Is a request in-flight over the network?
	bool isRequestPending(const LLUUID& agent_id);

	// Erase expired names from cache
	void eraseUnrefreshed();

	bool expirationFromCacheControl(AIHTTPReceivedHeaders const& headers, F64* expires);
}

/* Sample response:
<?xml version="1.0"?>
<llsd>
  <map>
    <key>agents</key>
    <array>
      <map>
        <key>display_name_next_update</key>
        <date>2010-04-16T21:34:02+00:00Z</date>
        <key>display_name_expires</key>
        <date>2010-04-16T21:32:26.142178+00:00Z</date>
        <key>display_name</key>
        <string>MickBot390 LLQABot</string>
        <key>sl_id</key>
        <string>mickbot390.llqabot</string>
        <key>id</key>
        <string>0012809d-7d2d-4c24-9609-af1230a37715</string>
        <key>is_display_name_default</key>
        <boolean>false</boolean>
      </map>
      <map>
        <key>display_name_next_update</key>
        <date>2010-04-16T21:34:02+00:00Z</date>
        <key>display_name_expires</key>
        <date>2010-04-16T21:32:26.142178+00:00Z</date>
        <key>display_name</key>
        <string>Bjork Gudmundsdottir</string>
        <key>sl_id</key>
        <string>sardonyx.linden</string>
        <key>id</key>
        <string>3941037e-78ab-45f0-b421-bd6e77c1804d</string>
        <key>is_display_name_default</key>
        <boolean>true</boolean>
      </map>
    </array>
  </map>
</llsd>
*/

class LLAvatarNameResponder : public LLHTTPClient::ResponderWithResult
{
	LOG_CLASS(LLAvatarNameResponder);
private:
	// need to store agent ids that are part of this request in case of
	// an error, so we can flag them as unavailable
	uuid_vec_t mAgentIDs;

	// Need the headers to look up Expires: and Retry-After:
	/*virtual*/ bool needsHeaders() const { return true; }
	/*virtual*/ char const* getName() const { return "LLAvatarNameResponder"; }

public:
	LLAvatarNameResponder(const uuid_vec_t& agent_ids)
	:	mAgentIDs(agent_ids)
	{ }

protected:
	/*virtual*/ void httpSuccess()
	{
		const LLSD& content = getContent();
		if (!content.isMap())
		{
			failureResult(HTTP_INTERNAL_ERROR_OTHER, "Malformed response contents", content);
			return;
		}
		// Pull expiration out of headers if available
		F64 expires = LLAvatarNameCache::nameExpirationFromHeaders(getResponseHeaders());
		F64 now = LLFrameTimer::getTotalSeconds();

		const LLSD& agents = content["agents"];
		LLSD::array_const_iterator it = agents.beginArray();
		for ( ; it != agents.endArray(); ++it)
		{
			const LLSD& row = *it;
			LLUUID agent_id = row["id"].asUUID();

			LLAvatarName av_name;
			av_name.fromLLSD(row);

			// Use expiration time from header
			av_name.mExpires = expires;

			LL_DEBUGS("AvNameCache") << "LLAvatarNameResponder::result for " << agent_id << LL_ENDL;
			av_name.dump();
			
			// cache it and fire signals
			LLAvatarNameCache::processName(agent_id, av_name);
		}

		// Same logic as error response case
		const LLSD& unresolved_agents = content["bad_ids"];
		S32  num_unresolved = unresolved_agents.size();
		if (num_unresolved > 0)
		{
            LL_WARNS("AvNameCache") << "LLAvatarNameResponder::result " << num_unresolved << " unresolved ids; "
                                    << "expires in " << expires - now << " seconds"
                                    << LL_ENDL;
			it = unresolved_agents.beginArray();
			for ( ; it != unresolved_agents.endArray(); ++it)
			{
				const LLUUID& agent_id = *it;

				LL_WARNS("AvNameCache") << "LLAvatarNameResponder::result "
                                        << "failed id " << agent_id
                                        << LL_ENDL;

                LLAvatarNameCache::handleAgentError(agent_id);
			}
		}
        LL_DEBUGS("AvNameCache") << "LLAvatarNameResponder::result " 
                                 << LLAvatarNameCache::sCache.size() << " cached names"
                                 << LL_ENDL;
    }

	/*virtual*/ void httpFailure()
	{
		// If there's an error, it might be caused by PeopleApi,
		// or when loading textures on startup and using a very slow 
		// network, this query may time out.
		// What we should do depends on whether or not we have a cached name
		LL_WARNS("AvNameCache") << dumpResponse() << LL_ENDL;

		// Add dummy records for any agent IDs in this request that we do not have cached already
		auto it = mAgentIDs.begin();
		for ( ; it != mAgentIDs.end(); ++it)
		{
			const LLUUID& agent_id = *it;
			LLAvatarNameCache::handleAgentError(agent_id);
		}
	}
};

// Provide some fallback for agents that return errors
void LLAvatarNameCache::handleAgentError(const LLUUID& agent_id)
{
	std::map<LLUUID,LLAvatarName>::iterator existing = sCache.find(agent_id);
	if (existing == sCache.end())
    {
        // there is no existing cache entry, so make a temporary name from legacy
        LL_WARNS("AvNameCache") << "LLAvatarNameCache get legacy for agent "
                                << agent_id << LL_ENDL;
        gCacheName->get(agent_id, false,  // legacy compatibility
                        boost::bind(&LLAvatarNameCache::legacyNameFetch, _1, _2, _3));
    }
	else
    {
        // we have a cached (but probably expired) entry - since that would have
        // been returned by the get method, there is no need to signal anyone

        // Clear this agent from the pending list
        LLAvatarNameCache::sPendingQueue.erase(agent_id);

        LLAvatarName& av_name = existing->second;
        LL_DEBUGS("AvNameCache") << "LLAvatarNameCache use cache for agent " << agent_id << LL_ENDL;
		av_name.dump();

		 // Reset expiry time so we don't constantly rerequest.
		av_name.setExpires(TEMP_CACHE_ENTRY_LIFETIME);
    }
}

void LLAvatarNameCache::processName(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	// Add to the cache
	sCache[agent_id] = av_name;

	// Suppress request from the queue
	sPendingQueue.erase(agent_id);

	// Signal everyone waiting on this name
	signal_map_t::iterator sig_it =	sSignalMap.find(agent_id);
	if (sig_it != sSignalMap.end())
	{
		callback_signal_t* signal = sig_it->second;
		(*signal)(agent_id, av_name);

		sSignalMap.erase(agent_id);

		delete signal;
		signal = NULL;
	}
}

void LLAvatarNameCache::requestNamesViaCapability()
{
	F64 now = LLFrameTimer::getTotalSeconds();

	// URL format is like:
	// http://pdp60.lindenlab.com:8000/agents/?ids=3941037e-78ab-45f0-b421-bd6e77c1804d&ids=0012809d-7d2d-4c24-9609-af1230a37715&ids=0019aaba-24af-4f0a-aa72-6457953cf7f0
	//
	// Apache can handle URLs of 4096 chars, but let's be conservative
	static const U32 NAME_URL_MAX = 4096;
	static const U32 NAME_URL_SEND_THRESHOLD = 3500;

	std::string url;
	url.reserve(NAME_URL_MAX);

	uuid_vec_t agent_ids;
	agent_ids.reserve(128);
	
	U32 ids = 0;
	ask_queue_t::const_iterator it;
	while(!sAskQueue.empty())
	{
		it = sAskQueue.begin();
		const LLUUID agent_id = *it;
		sAskQueue.erase(it);

		if (url.empty())
		{
			// ...starting new request
			url += sNameLookupURL;
			url += "?ids=";
			ids = 1;
		}
		else
		{
			// ...continuing existing request
			url += "&ids=";
			ids++;
		}
		url += agent_id.asString();
		agent_ids.push_back(agent_id);

		// mark request as pending
		sPendingQueue[agent_id] = now;

		if (url.size() > NAME_URL_SEND_THRESHOLD)
		{
			break;
		}
	}

	if (!url.empty())
	{
		LL_INFOS("AvNameCache") << "LLAvatarNameCache::requestNamesViaCapability getting " << ids << " ids" << LL_ENDL;
		LLHTTPClient::get(url, new LLAvatarNameResponder(agent_ids));
	}
}

void LLAvatarNameCache::legacyNameCallback(const LLUUID& agent_id,
										   const std::string& full_name,
										   bool is_group)
{
	// Put the received data in the cache
	legacyNameFetch(agent_id, full_name, is_group);
	
	// Retrieve the name and set it to never (or almost never...) expire: when we are using the legacy
	// protocol, we do not get an expiration date for each name and there's no reason to ask the 
	// data again and again so we set the expiration time to the largest value admissible.
	std::map<LLUUID,LLAvatarName>::iterator av_record = sCache.find(agent_id);
	LLAvatarName& av_name = av_record->second;
	av_name.setExpires(MAX_UNREFRESHED_TIME);
}

void LLAvatarNameCache::legacyNameFetch(const LLUUID& agent_id,
										const std::string& full_name,
										bool is_group)
{
	LL_DEBUGS("AvNameCache") << "LLAvatarNameCache agent " << agent_id << " "
							 << "full name '" << full_name << "'"
							 << ( is_group ? " [group]" : "" )
							 << LL_ENDL;

	// Construct an av_name record from this name.
	LLAvatarName av_name;
	av_name.fromString(full_name);

	// Add to cache: we're still using the new cache even if we're using the old (legacy) protocol.
	processName(agent_id, av_name);
}

void LLAvatarNameCache::requestNamesViaLegacy()
{
	static const S32 MAX_REQUESTS = 100;
	F64 now = LLFrameTimer::getTotalSeconds();
	std::string full_name;
	ask_queue_t::const_iterator it;
	for (S32 requests = 0; !sAskQueue.empty() && requests < MAX_REQUESTS; ++requests)
	{
		it = sAskQueue.begin();
		const LLUUID& agent_id = *it;
		sAskQueue.erase(it);

		// Mark as pending first, just in case the callback is immediately
		// invoked below.  This should never happen in practice.
		sPendingQueue[agent_id] = now;

		LL_DEBUGS("AvNameCache") << "LLAvatarNameCache::requestNamesViaLegacy agent " << agent_id << LL_ENDL;

		gCacheName->get(agent_id, false,  // legacy compatibility
			boost::bind(&LLAvatarNameCache::legacyNameCallback, _1, _2, _3));
	}
}

void LLAvatarNameCache::initClass(bool running, bool usePeopleAPI)
{
	sRunning = running;
	sUsePeopleAPI = usePeopleAPI;
}

void LLAvatarNameCache::cleanupClass()
{
	sCache.clear();
}

bool LLAvatarNameCache::importFile(std::istream& istr)
{
	LLSD data;
	if (LLSDParser::PARSE_FAILURE == LLSDSerialize::fromXMLDocument(data, istr))
	{
        LL_WARNS("AvNameCache") << "avatar name cache data xml parse failed" << LL_ENDL;
		return false;
	}

	// by convention LLSD storage is a map
	// we only store one entry in the map
	LLSD agents = data["agents"];

	LLUUID agent_id;
	LLAvatarName av_name;
	LLSD::map_const_iterator it = agents.beginMap();
	for ( ; it != agents.endMap(); ++it)
	{
		agent_id.set(it->first);
		av_name.fromLLSD( it->second );
		sCache[agent_id] = av_name;
	}
    LL_INFOS("AvNameCache") << "LLAvatarNameCache loaded " << sCache.size() << LL_ENDL;
	// Some entries may have expired since the cache was stored,
    // but they will be flushed in the first call to eraseUnrefreshed
    // from LLAvatarNameResponder::idle

    return true;
}

void LLAvatarNameCache::exportFile(std::ostream& ostr)
{
	LLSD agents;
	F64 max_unrefreshed = LLFrameTimer::getTotalSeconds() - MAX_UNREFRESHED_TIME;
    LL_INFOS("AvNameCache") << "LLAvatarNameCache at exit cache has " << sCache.size() << LL_ENDL;
	cache_t::const_iterator it = sCache.begin();
	for ( ; it != sCache.end(); ++it)
	{
		const LLUUID& agent_id = it->first;
		const LLAvatarName& av_name = it->second;
		// Do not write temporary or expired entries to the stored cache
		if (av_name.isValidName(max_unrefreshed))
		{
			// key must be a string
			agents[agent_id.asString()] = av_name.asLLSD();
		}
	}
    LL_INFOS("AvNameCache") << "LLAvatarNameCache returning " << agents.size() << LL_ENDL;
	LLSD data;
	data["agents"] = agents;
	LLSDSerialize::toPrettyXML(data, ostr);
}

void LLAvatarNameCache::setNameLookupURL(const std::string& name_lookup_url)
{
	sNameLookupURL = name_lookup_url;
}

bool LLAvatarNameCache::hasNameLookupURL()
{
	return !sNameLookupURL.empty();
}

bool LLAvatarNameCache::usePeopleAPI()
{
	return hasNameLookupURL() && sUsePeopleAPI;
}

void LLAvatarNameCache::idle()
{
	// By convention, start running at first idle() call
	sRunning = true;

	// *TODO: Possibly re-enabled this based on People API load measurements
	// 100 ms is the threshold for "user speed" operations, so we can
	// stall for about that long to batch up requests.
	const F32 SECS_BETWEEN_REQUESTS = 0.1f;
	if (!sRequestTimer.hasExpired())
	{
		return;
	}

	if (!sAskQueue.empty())
	{
        if (usePeopleAPI())
        {
            requestNamesViaCapability();
        }
        else
        {
            LL_WARNS_ONCE("AvNameCache") << "LLAvatarNameCache still using legacy api" << LL_ENDL;
            requestNamesViaLegacy();
        }
	}

	if (sAskQueue.empty())
	{
		// cleared the list, reset the request timer.
		sRequestTimer.resetWithExpiry(SECS_BETWEEN_REQUESTS);
	}

    // erase anything that has not been refreshed for more than MAX_UNREFRESHED_TIME
    eraseUnrefreshed();
}

bool LLAvatarNameCache::isRequestPending(const LLUUID& agent_id)
{
	bool isPending = false;
	const F64 PENDING_TIMEOUT_SECS = 5.0 * 60.0;

	pending_queue_t::const_iterator it = sPendingQueue.find(agent_id);
	if (it != sPendingQueue.end())
	{
		// in the list of requests in flight, retry if too old
		F64 expire_time = LLFrameTimer::getTotalSeconds() - PENDING_TIMEOUT_SECS;
		isPending = (it->second > expire_time);
	}
	return isPending;
}

void LLAvatarNameCache::eraseUnrefreshed()
{
	F64 now = LLFrameTimer::getTotalSeconds();
	F64 max_unrefreshed = now - MAX_UNREFRESHED_TIME;

    if (!sLastExpireCheck || sLastExpireCheck < max_unrefreshed)
    {
        sLastExpireCheck = now;
        S32 expired = 0;
        for (cache_t::iterator it = sCache.begin(); it != sCache.end();)
        {
            const LLAvatarName& av_name = it->second;
            if (av_name.mExpires < max_unrefreshed)
            {
                LL_DEBUGS("AvNameCacheExpired") << "LLAvatarNameCache " << it->first 
                                         << " user '" << av_name.getAccountName() << "' "
                                         << "expired " << now - av_name.mExpires << " secs ago"
                                         << LL_ENDL;
                sCache.erase(it++);
                expired++;
            }
			else
			{
				++it;
			}
        }
        LL_INFOS("AvNameCache") << "LLAvatarNameCache expired " << expired << " cached avatar names, "
                                << sCache.size() << " remaining" << LL_ENDL;
	}
}

// fills in av_name if it has it in the cache, even if expired (can check expiry time)
// returns bool specifying  if av_name was filled, false otherwise
bool LLAvatarNameCache::get(const LLUUID& agent_id, LLAvatarName *av_name)
{
	if (sRunning)
	{
		// ...only do immediate lookups when cache is running
		std::map<LLUUID,LLAvatarName>::iterator it = sCache.find(agent_id);
		if (it != sCache.end())
		{
			*av_name = it->second;

			// re-request name if entry is expired
			if (av_name->mExpires < LLFrameTimer::getTotalSeconds())
			{
				if (!isRequestPending(agent_id))
				{
					LL_DEBUGS("AvNameCache") << "LLAvatarNameCache refresh agent " << agent_id
											 << LL_ENDL;
					sAskQueue.insert(agent_id);
				}
			}
			
			return true;
		}
		else if (!usePeopleAPI())
		{
			std::string full_name;
			if (gCacheName->getFullName(agent_id, full_name))
			{
				av_name->fromString(full_name);
				sCache[agent_id] = *av_name;
				return true;
			}
		}
	}

	if (!isRequestPending(agent_id))
	{
		LL_DEBUGS("AvNameCache") << "LLAvatarNameCache queue request for agent " << agent_id << LL_ENDL;
		sAskQueue.insert(agent_id);
	}

	return false;
}

const S32& main_name_system()
{
	static const LLCachedControl<S32> name_system("PhoenixNameSystem", 0);
	return name_system;
}

// Return true when name has been set to Name System Name, if not return false.
bool LLAvatarNameCache::getNSName(const LLUUID& agent_id, std::string& name, const S32& name_system)
{
	LLAvatarName avatar_name;
	if (get(agent_id, &avatar_name))
		name = avatar_name.getNSName(name_system);
	else return false;
	return true;
}

void LLAvatarNameCache::fireSignal(const LLUUID& agent_id,
								   const callback_slot_t& slot,
								   const LLAvatarName& av_name)
{
	callback_signal_t signal;
	signal.connect(slot);
	signal(agent_id, av_name);
}

LLAvatarNameCache::callback_connection_t LLAvatarNameCache::get(const LLUUID& agent_id, callback_slot_t slot)
{
	callback_connection_t connection;

	if (sRunning)
	{
		// ...only do immediate lookups when cache is running
		std::map<LLUUID,LLAvatarName>::iterator it = sCache.find(agent_id);
		if (it != sCache.end())
		{
			const LLAvatarName& av_name = it->second;
			
			if (av_name.mExpires > LLFrameTimer::getTotalSeconds())
			{
				// ...name already exists in cache, fire callback now
				fireSignal(agent_id, slot, av_name);
				return connection;
			}
		}
	}

	// schedule a request
	if (!isRequestPending(agent_id))
	{
		sAskQueue.insert(agent_id);
	}

	// always store additional callback, even if request is pending
	signal_map_t::iterator sig_it = sSignalMap.find(agent_id);
	if (sig_it == sSignalMap.end())
	{
		// ...new callback for this id
		callback_signal_t* signal = new callback_signal_t();
		connection = signal->connect(slot);
		sSignalMap[agent_id] = signal;
	}
	else
	{
		// ...existing callback, bind additional slot
		callback_signal_t* signal = sig_it->second;
		connection = signal->connect(slot);
	}

	return connection;
}

// [RLVa:KB] - Checked: 2010-12-08 (RLVa-1.4.0a) | Added: RLVa-1.2.2c
bool LLAvatarNameCache::getForceDisplayNames()
{
	return sForceDisplayNames;
}

void LLAvatarNameCache::setForceDisplayNames(bool force)
{
	sForceDisplayNames = force;
	if ( (!LLAvatarName::useDisplayNames()) && (force) )
	{
		setUseDisplayNames(true);
	}
}
// [/RLVa:KB]

void LLAvatarNameCache::setUseDisplayNames(bool use)
{
// [RLVa:KB] - Checked: 2010-12-08 (RLVa-1.4.0a) | Added: RLVa-1.2.2c
	// We need to force the use of the "display names" cache when @shownames=n restricted (and disallow toggling it)
	use |= getForceDisplayNames();
// [/RLVa:KB]
	if (use != LLAvatarName::useDisplayNames())
	{
		LLAvatarName::setUseDisplayNames(use);
		mUseDisplayNamesSignal();
	}
}

void LLAvatarNameCache::setUseUsernames(bool use)
{
	if (use != LLAvatarName::useUsernames())
	{
		LLAvatarName::setUseUsernames(use);
		mUseDisplayNamesSignal();
	}
}

void LLAvatarNameCache::erase(const LLUUID& agent_id)
{
	sCache.erase(agent_id);
}

void LLAvatarNameCache::insert(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	// *TODO: update timestamp if zero?
	sCache[agent_id] = av_name;
}

F64 LLAvatarNameCache::nameExpirationFromHeaders(AIHTTPReceivedHeaders const& headers)
{
	F64 expires = 0.0;
	if (expirationFromCacheControl(headers, &expires))
	{
		return expires;
	}
	else
	{
		// With no expiration info, default to an hour
		const F64 DEFAULT_EXPIRES = 60.0 * 60.0;
		F64 now = LLFrameTimer::getTotalSeconds();
		return now + DEFAULT_EXPIRES;
	}
}

bool LLAvatarNameCache::expirationFromCacheControl(AIHTTPReceivedHeaders const& headers, F64* expires)
{
	bool fromCacheControl = false;
	F64 now = LLFrameTimer::getTotalSeconds();

	// Allow the header to override the default
	std::string cache_control;
	if (headers.getFirstValue("cache-control", cache_control))
	{
		S32 max_age = 0;
		if (max_age_from_cache_control(cache_control, &max_age))
		{
			*expires = now + (F64)max_age;
			fromCacheControl = true;
		}
	}
	LL_DEBUGS("AvNameCache") << "LLAvatarNameCache "
		<< ( fromCacheControl ? "expires based on cache control " : "default expiration " )
		<< "in " << *expires - now << " seconds"
		<< LL_ENDL;

	return fromCacheControl;
}


void LLAvatarNameCache::addUseDisplayNamesCallback(const use_display_name_signal_t::slot_type& cb) 
{ 
	mUseDisplayNamesSignal.connect(cb); 
}


static const std::string MAX_AGE("max-age");
static const boost::char_separator<char> EQUALS_SEPARATOR("=");
static const boost::char_separator<char> COMMA_SEPARATOR(",");

bool max_age_from_cache_control(const std::string& cache_control, S32 *max_age)
{
	// Split the string on "," to get a list of directives
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer directives(cache_control, COMMA_SEPARATOR);

	tokenizer::iterator token_it = directives.begin();
	for ( ; token_it != directives.end(); ++token_it)
	{
		// Tokens may have leading or trailing whitespace
		std::string token = *token_it;
		LLStringUtil::trim(token);

		if (token.compare(0, MAX_AGE.size(), MAX_AGE) == 0)
		{
			// ...this token starts with max-age, so let's chop it up by "="
			tokenizer subtokens(token, EQUALS_SEPARATOR);
			tokenizer::iterator subtoken_it = subtokens.begin();

			// Must have a token
			if (subtoken_it == subtokens.end()) return false;
			std::string subtoken = *subtoken_it;

			// Must exactly equal "max-age"
			LLStringUtil::trim(subtoken);
			if (subtoken != MAX_AGE) return false;

			// Must have another token
			++subtoken_it;
			if (subtoken_it == subtokens.end()) return false;
			subtoken = *subtoken_it;

			// Must be a valid integer
			// *NOTE: atoi() returns 0 for invalid values, so we have to
			// check the string first.
			// *TODO: Do servers ever send "0000" for zero?  We don't handle it
			LLStringUtil::trim(subtoken);
			if (subtoken == "0")
			{
				*max_age = 0;
				return true;
			}
			S32 val = atoi( subtoken.c_str() );
			if (val > 0 && val < S32_MAX)
			{
				*max_age = val;
				return true;
			}
			return false;
		}
	}
	return false;
}

