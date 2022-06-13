/**
 * @file llviewermedia.cpp
 * @brief Client interface to the media engine
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llviewermedia.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llappviewer.h"
#include "llaudioengine.h"  // for gAudiop
#include "llcallbacklist.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llevent.h"		// LLSimpleListener
#include "aifilepicker.h"
#include "llfloaterdestinations.h"
#include "llfloaterwebcontent.h"	// for handling window close requests and geometry change requests in media browser windows.
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llmarketplacefunctions.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llmimetypes.h"
#include "llmutelist.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelprofile.h"
#include "llparcel.h"
#include "llpluginclassmedia.h"
#include "llurldispatcher.h"
#include "lluuid.h"
#include "llvieweraudio.h"
#include "llviewermediafocus.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llwebprofile.h"
#include "llwindow.h"
#include "llvieweraudio.h"
#include "llhttpclient.h"

#include "llstartup.h"

std::string getProfileURL(const std::string& agent_name);

/*static*/ const char* LLViewerMedia::AUTO_PLAY_MEDIA_SETTING = "ParcelMediaAutoPlayEnable";
/*static*/ const char* LLViewerMedia::AUTO_PLAY_PRIM_MEDIA_SETTING = "PrimMediaAutoPlayEnable";
/*static*/ const char* LLViewerMedia::SHOW_MEDIA_ON_OTHERS_SETTING = "MediaShowOnOthers";
/*static*/ const char* LLViewerMedia::SHOW_MEDIA_WITHIN_PARCEL_SETTING = "MediaShowWithinParcel";
/*static*/ const char* LLViewerMedia::SHOW_MEDIA_OUTSIDE_PARCEL_SETTING = "MediaShowOutsideParcel";


// Move this to its own file.

LLViewerMediaEventEmitter::~LLViewerMediaEventEmitter()
{
	observerListType::iterator iter = mObservers.begin();

	while( iter != mObservers.end() )
	{
		LLViewerMediaObserver *self = *iter;
		iter++;
		remObserver(self);
	}
}

///////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaEventEmitter::addObserver( LLViewerMediaObserver* observer )
{
	if ( ! observer )
		return false;

	if ( std::find( mObservers.begin(), mObservers.end(), observer ) != mObservers.end() )
		return false;

	mObservers.push_back( observer );
	observer->mEmitters.push_back( this );

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaEventEmitter::remObserver( LLViewerMediaObserver* observer )
{
	if ( ! observer )
		return false;

	mObservers.remove( observer );
	observer->mEmitters.remove(this);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
void LLViewerMediaEventEmitter::emitEvent( LLPluginClassMedia* media, LLViewerMediaObserver::EMediaEvent event )
{
	// Broadcast the event to any observers.
	observerListType::iterator iter = mObservers.begin();
	while( iter != mObservers.end() )
	{
		LLViewerMediaObserver *self = *iter;
		++iter;
		self->handleMediaEvent( media, event );
	}
}

// Move this to its own file.
LLViewerMediaObserver::~LLViewerMediaObserver()
{
	std::list<LLViewerMediaEventEmitter *>::iterator iter = mEmitters.begin();

	while( iter != mEmitters.end() )
	{
		LLViewerMediaEventEmitter *self = *iter;
		iter++;
		self->remObserver( this );
	}
}


// Move this to its own file.
// helper class that tries to download a URL from a web site and calls a method
// on the Panel Land Media and to discover the MIME type
class LLMimeDiscoveryResponder : public LLHTTPClient::ResponderHeadersOnly
{
LOG_CLASS(LLMimeDiscoveryResponder);
public:
	LLMimeDiscoveryResponder( viewer_media_t media_impl)
		: mMediaImpl(media_impl),
		  mInitialized(false)
	{
		if(mMediaImpl->mMimeProbe)
		{
			LL_ERRS() << "impl already has an outstanding responder" << LL_ENDL;
		}
		
		mMediaImpl->mMimeProbe = this;
	}
	
	~LLMimeDiscoveryResponder() { disconnectOwner(); }

private:
	/* virtual */ void completedHeaders()
	{
		if (!isGoodStatus(mStatus))
		{
			LL_WARNS() << dumpResponse()
					<< " [headers:" << getResponseHeaders() << "]" << LL_ENDL;
		}
		std::string media_type;
		std::string::size_type idx1 = media_type.find_first_of(";");
		std::string mime_type = media_type.substr(0, idx1);

		LL_DEBUGS() << "status is " << getStatus() << ", media type \"" << media_type << "\"" << LL_ENDL;

		// 2xx status codes indicate success.
		// Most 4xx status codes are successful enough for our purposes.
		// 499 is the error code for host not found, timeout, etc.
		// 500 means "Internal Server error" but we decided it's okay to
		//     accept this and go past it in the MIME type probe
		// 302 means the resource can be found temporarily in a different place - added this for join.secondlife.com
		// 499 is a code specifc to join.secondlife.com apparently safe to ignore
//		if(	((status >= 200) && (status < 300))	||
//			((status >= 400) && (status < 499))	||
//			(status == 500) ||
//			(status == 302) ||
//			(status == 499)
//			)
		// We now no longer check the error code returned from the probe.
		// If we have a mime type, use it.  If not, default to the web plugin and let it handle error reporting.
		//if(1)
		{
			// The probe was successful.
			if(mime_type.empty())
			{
				// Some sites don't return any content-type header at all.
				// Treat an empty mime type as text/html.
				mime_type = "text/html";
			}
		}
		//else
		//{
		//	LL_WARNS() << "responder failed with status " << dumpResponse() << LL_ENDL;
		//
		//	if(mMediaImpl)
		//	{
		//		mMediaImpl->mMediaSourceFailed = true;
		//	}
		//	return;
		//}

		// the call to initializeMedia may disconnect the responder, which will clear mMediaImpl.
		// Make a local copy so we can call loadURI() afterwards.
		LLViewerMediaImpl *impl = mMediaImpl;
		
		if(impl && !mInitialized && ! mime_type.empty())
		{
			if(impl->initializeMedia(mime_type))
			{
				mInitialized = true;
				impl->loadURI();
				disconnectOwner();
			}
		}
	}

public:
	/*virtual*/ char const* getName(void) const { return "LLMimeDiscoveryResponder"; }
	void cancelRequest()
	{
		disconnectOwner();
	}
	
private:
	void disconnectOwner()
	{
		if(mMediaImpl)
		{
			if(mMediaImpl->mMimeProbe != this)
			{
				LL_ERRS() << "internal error: mMediaImpl->mMimeProbe != this" << LL_ENDL;
			}

			mMediaImpl->mMimeProbe = nullptr;
		}
		mMediaImpl = nullptr;
	}


	public:
		LLViewerMediaImpl *mMediaImpl;
		bool mInitialized;
};

class LLViewerMediaOpenIDResponder : public LLHTTPClient::ResponderWithCompleted
{
LOG_CLASS(LLViewerMediaOpenIDResponder);
public:
	LLViewerMediaOpenIDResponder( )
	{
	}

	~LLViewerMediaOpenIDResponder()
	{
	}

	/* virtual */ void completedRaw(
		const LLChannelDescriptors& channels,
		const LLIOPipe::buffer_ptr_t& buffer)
	{
		// We don't care about the content of the response, only the Set-Cookie header.
		LL_DEBUGS("MediaAuth") << dumpResponse()
				<< " [headers:" << getResponseHeaders() << "]" << LL_ENDL;
		std::string cookie;
		getResponseHeaders().getFirstValue("set-cookie", cookie);

		// *TODO: What about bad status codes?  Does this destroy previous cookies?
		LLViewerMedia::openIDCookieResponse(cookie);
	}

	/*virtual*/ char const* getName(void) const { return "LLViewerMediaOpenIDResponder"; }
	/*virtual*/ bool needsHeaders(void) const { return true; }
};

class LLViewerMediaWebProfileResponder : public LLHTTPClient::ResponderWithCompleted
{
LOG_CLASS(LLViewerMediaWebProfileResponder);
public:
	LLViewerMediaWebProfileResponder(std::string host)
	{
		mHost = host;
	}

	~LLViewerMediaWebProfileResponder()
	{
	}

	void completedRaw(
		const LLChannelDescriptors& channels,
		const LLIOPipe::buffer_ptr_t& buffer)
	{
		// We don't care about the content of the response, only the set-cookie header.
		LL_WARNS("MediaAuth") << dumpResponse()
				<< " [headers:" << getResponseHeaders() << "]" << LL_ENDL;

		AIHTTPReceivedHeaders stripped_content = getResponseHeaders();
		LL_WARNS("MediaAuth") << stripped_content << LL_ENDL;

		AIHTTPReceivedHeaders::range_type cookies;
		if (mReceivedHeaders.getValues("set-cookie", cookies))
		{
			for (AIHTTPReceivedHeaders::iterator_type cookie = cookies.first; cookie != cookies.second; ++cookie)
			{
				// *TODO: What about bad status codes?  Does this destroy previous cookies?
				if (cookie->second.substr(0, cookie->second.find('=')) == "_my_secondlife_session")
				{
					// Set cookie for snapshot publishing.
					std::string auth_cookie = cookie->second.substr(0, cookie->second.find(";")); // strip path
					LLWebProfile::setAuthCookie(auth_cookie);
					break;
				}
			}
		}
	}

	/*virtual*/ char const* getName() const { return "LLViewerMediaWebProfileResponder"; }
	/*virtual*/ bool needsHeaders() const { return true; }

	std::string mHost;
};


LLURL LLViewerMedia::sOpenIDURL;
std::string LLViewerMedia::sOpenIDCookie;
LLPluginClassMedia* LLViewerMedia::sSpareBrowserMediaSource = nullptr;
static LLViewerMedia::impl_list sViewerMediaImplList;
static LLViewerMedia::impl_id_map sViewerMediaTextureIDMap;
static LLTimer sMediaCreateTimer;
static const F32 LLVIEWERMEDIA_CREATE_DELAY = 1.0f;
static F32 sGlobalVolume = 1.0f;
static bool sForceUpdate = false;
static LLUUID sOnlyAudibleTextureID = LLUUID::null;
static F64 sLowestLoadableImplInterest = 0.0f;
static bool sAnyMediaShowing = false;
static boost::signals2::connection sTeleportFinishConnection;

//////////////////////////////////////////////////////////////////////////////////////////
static void add_media_impl(LLViewerMediaImpl* media)
{
	sViewerMediaImplList.push_back(media);
}

//////////////////////////////////////////////////////////////////////////////////////////
static void remove_media_impl(LLViewerMediaImpl* media)
{
	LLViewerMedia::impl_list::iterator iter = sViewerMediaImplList.begin();
	LLViewerMedia::impl_list::iterator end = sViewerMediaImplList.end();
	
	for(; iter != end; iter++)
	{
		if(media == *iter)
		{
			sViewerMediaImplList.erase(iter);
			return;
		}
	}
}

class LLViewerMediaMuteListObserver : public LLMuteListObserver
{
	/* virtual */ void onChange()  { LLViewerMedia::muteListChanged();}
};

static LLViewerMediaMuteListObserver sViewerMediaMuteListObserver;
static bool sViewerMediaMuteListObserverInitialized = false;


//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMedia

//////////////////////////////////////////////////////////////////////////////////////////
// static
viewer_media_t LLViewerMedia::newMediaImpl(
											 const LLUUID& texture_id,
											 S32 media_width, 
											 S32 media_height, 
											 U8 media_auto_scale,
											 U8 media_loop)
{
	LLViewerMediaImpl* media_impl = getMediaImplFromTextureID(texture_id);
	if(media_impl == NULL || texture_id.isNull())
	{
		// Create the media impl
		media_impl = new LLViewerMediaImpl(texture_id, media_width, media_height, media_auto_scale, media_loop);
	}
	else
	{
		media_impl->unload();
		media_impl->setTextureID(texture_id);
		media_impl->mMediaWidth = media_width;
		media_impl->mMediaHeight = media_height;
		media_impl->mMediaAutoScale = media_auto_scale;
		media_impl->mMediaLoop = media_loop;
	}
	media_impl->setPageZoomFactor(media_impl->mZoomFactor);

	return media_impl;
}

viewer_media_t LLViewerMedia::updateMediaImpl(LLMediaEntry* media_entry, const std::string& previous_url, bool update_from_self)
{	
	// Try to find media with the same media ID
	viewer_media_t media_impl = getMediaImplFromTextureID(media_entry->getMediaID());
	
	LL_DEBUGS() << "called, current URL is \"" << media_entry->getCurrentURL() 
			<< "\", previous URL is \"" << previous_url 
			<< "\", update_from_self is " << (update_from_self?"true":"false")
			<< LL_ENDL;
			
	bool was_loaded = false;
	bool needs_navigate = false;
	
	if(media_impl)
	{	
		was_loaded = media_impl->hasMedia();
		
		media_impl->setHomeURL(media_entry->getHomeURL());
		
		media_impl->mMediaAutoScale = media_entry->getAutoScale();
		media_impl->mMediaLoop = media_entry->getAutoLoop();
		media_impl->mMediaWidth = media_entry->getWidthPixels();
		media_impl->mMediaHeight = media_entry->getHeightPixels();
		media_impl->mMediaAutoPlay = media_entry->getAutoPlay();
		media_impl->mMediaEntryURL = media_entry->getCurrentURL();
		LLPluginClassMedia* plugin = media_impl->getMediaPlugin();
		if (plugin)
		{
			plugin->setAutoScale(media_impl->mMediaAutoScale);
			plugin->setLoop(media_impl->mMediaLoop);
			plugin->setSize(media_entry->getWidthPixels(), media_entry->getHeightPixels());
		}
		
		bool url_changed = (media_impl->mMediaEntryURL != previous_url);
		if(media_impl->mMediaEntryURL.empty())
		{
			if(url_changed)
			{
				// The current media URL is now empty.  Unload the media source.
				media_impl->unload();
			
				LL_DEBUGS() << "Unloading media instance (new current URL is empty)." << LL_ENDL;
			}
		}
		else
		{
			// The current media URL is not empty.
			// If (the media was already loaded OR the media was set to autoplay) AND this update didn't come from this agent,
			// do a navigate.
			bool auto_play = media_impl->isAutoPlayable();			
			if((was_loaded || auto_play) && !update_from_self)
			{
				needs_navigate = url_changed;
			}
			
			LL_DEBUGS() << "was_loaded is " << (was_loaded?"true":"false") 
					<< ", auto_play is " << (auto_play?"true":"false") 
					<< ", needs_navigate is " << (needs_navigate?"true":"false") << LL_ENDL;
		}
	}
	else
	{
		media_impl = newMediaImpl(
			media_entry->getMediaID(), 
			media_entry->getWidthPixels(),
			media_entry->getHeightPixels(), 
			media_entry->getAutoScale(), 
			media_entry->getAutoLoop());
		
		media_impl->setHomeURL(media_entry->getHomeURL());
		media_impl->mMediaAutoPlay = media_entry->getAutoPlay();
		media_impl->mMediaEntryURL = media_entry->getCurrentURL();
		if(media_impl->isAutoPlayable())
		{
			needs_navigate = true;
		}
	}
	
	if(media_impl)
	{
		if(needs_navigate)
		{
			media_impl->navigateTo(media_impl->mMediaEntryURL, "", true, true);
			LL_DEBUGS() << "navigating to URL " << media_impl->mMediaEntryURL << LL_ENDL;
		}
		else if(!media_impl->mMediaURL.empty() && (media_impl->mMediaURL != media_impl->mMediaEntryURL))
		{
			// If we already have a non-empty media URL set and we aren't doing a navigate, update the media URL to match the media entry.
			media_impl->mMediaURL = media_impl->mMediaEntryURL;

			// If this causes a navigate at some point (such as after a reload), it should be considered server-driven so it isn't broadcast.
			media_impl->mNavigateServerRequest = true;

			LL_DEBUGS() << "updating URL in the media impl to " << media_impl->mMediaEntryURL << LL_ENDL;
		}
	}
	
	return media_impl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
LLViewerMediaImpl* LLViewerMedia::getMediaImplFromTextureID(const LLUUID& texture_id)
{
	LLViewerMediaImpl* result = NULL;
	
	// Look up the texture ID in the texture id->impl map.
	impl_id_map::iterator iter = sViewerMediaTextureIDMap.find(texture_id);
	if(iter != sViewerMediaTextureIDMap.end())
	{
		result = iter->second;
	}

	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getCurrentUserAgent()
{
	// Don't include version, channel, or skin -- MC

	// Don't use user-visible string to avoid 
	// punctuation and strange characters.
	//std::string skin_name = gSavedSettings.getString("SkinCurrent");

	// Just in case we need to check browser differences in A/B test
	// builds.
	//std::string channel = gSavedSettings.getString("VersionChannelName");

	// append our magic version number string to the browser user agent id
	// See the HTTP 1.0 and 1.1 specifications for allowed formats:
	// http://www.ietf.org/rfc/rfc1945.txt section 10.15
	// http://www.ietf.org/rfc/rfc2068.txt section 3.8
	// This was also helpful:
	// http://www.mozilla.org/build/revised-user-agent-strings.html
	std::ostringstream codec;
	codec << "SecondLife/";
	codec << "C64 Basic V2";
	//codec << ViewerVersion::getImpMajorVersion() << "." << ViewerVersion::getImpMinorVersion() << "." << ViewerVersion::getImpPatchVersion() << " " << ViewerVersion::getImpTestVersion();
 	//codec << " (" << channel << "; " << skin_name << " skin)";
// 	LL_INFOS() << codec.str() << LL_ENDL;
	
	return codec.str();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateBrowserUserAgent()
{
	std::string user_agent = getCurrentUserAgent();
	
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		LLPluginClassMedia* plugin = pimpl->getMediaPlugin();
		if(plugin && plugin->pluginSupportsMediaBrowser())
		{
			plugin->setBrowserUserAgent(user_agent);
		}
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::handleSkinCurrentChanged(const LLSD& /*newvalue*/)
{
	// gSavedSettings is already updated when this function is called.
	updateBrowserUserAgent();
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::textureHasMedia(const LLUUID& texture_id)
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if(pimpl->getMediaTextureID() == texture_id)
		{
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setVolume(F32 volume)
{
	if(volume != sGlobalVolume || sForceUpdate)
	{
		sGlobalVolume = volume;
		impl_list::iterator iter = sViewerMediaImplList.begin();
		impl_list::iterator end = sViewerMediaImplList.end();

		for(; iter != end; iter++)
		{
			LLViewerMediaImpl* pimpl = *iter;
			pimpl->updateVolume();
		}

		sForceUpdate = false;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
F32 LLViewerMedia::getVolume()
{
	return sGlobalVolume;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::muteListChanged()
{
	// When the mute list changes, we need to check mute status on all impls.
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->mNeedsMuteCheck = true;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isInterestingEnough(const LLVOVolume *object, const F64 &object_interest)
{
	bool result = false;
	
	if (NULL == object)
	{
		result = false;
	}
	// Focused?  Then it is interesting!
	else if (LLViewerMediaFocus::getInstance()->getFocusedObjectID() == object->getID())
	{
		result = true;
	}
	// Selected?  Then it is interesting!
	// XXX Sadly, 'contains()' doesn't take a const :(
	else if (LLSelectMgr::getInstance()->getSelection()->contains(const_cast<LLVOVolume*>(object)))
	{
		result = true;
	}
	else 
	{
		LL_DEBUGS() << "object interest = " << object_interest << ", lowest loadable = " << sLowestLoadableImplInterest << LL_ENDL;
		if(object_interest >= sLowestLoadableImplInterest)
			result = true;
	}
	
	return result;
}

LLViewerMedia::impl_list &LLViewerMedia::getPriorityList()
{
	return sViewerMediaImplList;
}

// This is the predicate function used to sort sViewerMediaImplList by priority.
bool LLViewerMedia::priorityComparitor(const LLViewerMediaImpl* i1, const LLViewerMediaImpl* i2)
{
	if(i1->isForcedUnloaded() && !i2->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return false;
	}
	else if(i2->isForcedUnloaded() && !i1->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return true;
	}
	else if(i1->hasFocus())
	{
		// The item with user focus always comes to the front of the list, period.
		return true;
	}
	else if(i2->hasFocus())
	{
		// The item with user focus always comes to the front of the list, period.
		return false;
	}
	else if(i1->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless one has focus.
		return true;
	}
	else if(i2->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless one has focus.
		return false;
	}
	else if(i1->getUsedInUI() && !i2->getUsedInUI())
	{
		// i1 is a UI element, i2 is not.  This makes i1 "less than" i2, so it sorts earlier in our list.
		return true;
	}
	else if(i2->getUsedInUI() && !i1->getUsedInUI())
	{
		// i2 is a UI element, i1 is not.  This makes i2 "less than" i1, so it sorts earlier in our list.
		return false;
	}
	else if(i1->isPlayable() && !i2->isPlayable())
	{
		// Playable items sort above ones that wouldn't play even if they got high enough priority
		return true;
	}
	else if(!i1->isPlayable() && i2->isPlayable())
	{
		// Playable items sort above ones that wouldn't play even if they got high enough priority
		return false;
	}
	else if(i1->getInterest() == i2->getInterest())
	{
		// Generally this will mean both objects have zero interest.  In this case, sort on distance.
		return (i1->getProximityDistance() < i2->getProximityDistance());
	}
	else
	{
		// The object with the larger interest value should be earlier in the list, so we reverse the sense of the comparison here.
		return (i1->getInterest() > i2->getInterest());
	}
}

static bool proximity_comparitor(const LLViewerMediaImpl* i1, const LLViewerMediaImpl* i2)
{
	if(i1->getProximityDistance() < i2->getProximityDistance())
	{
		return true;
	}
	else if(i1->getProximityDistance() > i2->getProximityDistance())
	{
		return false;
	}
	else
	{
		// Both objects have the same distance.  This most likely means they're two faces of the same object.
		// They may also be faces on different objects with exactly the same distance (like HUD objects).
		// We don't actually care what the sort order is for this case, as long as it's stable and doesn't change when you enable/disable media.
		// Comparing the impl pointers gives a completely arbitrary ordering, but it will be stable.
		return (i1 < i2);
	}
}

static LLTrace::BlockTimerStatHandle FTM_MEDIA_UPDATE("Update Media");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_SPARE_IDLE("Spare Idle");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_UPDATE_INTEREST("Update/Interest");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_SORT("Sort");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_SORT2("Sort 2");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_MISC("Misc");


//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::updateMedia(void *dummy_arg)
{
	LL_RECORD_BLOCK_TIME(FTM_MEDIA_UPDATE);
	
	// Enable/disable the plugin read thread
	static LLCachedControl<bool> pluginUseReadThread(gSavedSettings, "PluginUseReadThread");
	LLPluginProcessParent::setUseReadThread(pluginUseReadThread);
	
	// HACK: we always try to keep a spare running webkit plugin around to improve launch times.
	createSpareBrowserMediaSource();
	
	sAnyMediaShowing = false;
	
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	{
		LL_RECORD_BLOCK_TIME(FTM_MEDIA_UPDATE_INTEREST);
		for(; iter != end;)
		{
			LLViewerMediaImpl* pimpl = *iter++;
			pimpl->update();
			pimpl->calculateInterest();
		}
	}
	
	// Let the spare media source actually launch
	if(sSpareBrowserMediaSource)
	{
		LL_RECORD_BLOCK_TIME(FTM_MEDIA_SPARE_IDLE);
		sSpareBrowserMediaSource->idle();
	}
		
	{
		LL_RECORD_BLOCK_TIME(FTM_MEDIA_SORT);
		// Sort the static instance list using our interest criteria
		sViewerMediaImplList.sort(priorityComparitor);
	}

	// Go through the list again and adjust according to priority.
	iter = sViewerMediaImplList.begin();
	end = sViewerMediaImplList.end();
	
	F64 total_cpu = 0.0f;
	int impl_count_total = 0;
	int impl_count_interest_low = 0;
	int impl_count_interest_normal = 0;
	
	std::vector<LLViewerMediaImpl*> proximity_order;
	
	static LLCachedControl<bool> inworld_media_enabled(gSavedSettings, "AudioStreamingMedia");
	static LLCachedControl<bool> inworld_audio_enabled(gSavedSettings, "AudioStreamingMusic");
	static LLCachedControl<U32> max_instances(gSavedSettings, "PluginInstancesTotal");
	static LLCachedControl<U32> max_normal(gSavedSettings, "PluginInstancesNormal");
	static LLCachedControl<U32> max_low(gSavedSettings, "PluginInstancesLow");
	static LLCachedControl<F32> max_cpu(gSavedSettings, "PluginInstancesCPULimit");
	// Setting max_cpu to 0.0 disables CPU usage checking.
	bool check_cpu_usage = (max_cpu != 0.0f);
	
	LLViewerMediaImpl* lowest_interest_loadable = NULL;
	
	// Notes on tweakable params:
	// max_instances must be set high enough to allow the various instances used in the UI (for the help browser, search, etc.) to be loaded.
	// If max_normal + max_low is less than max_instances, things will tend to get unloaded instead of being set to slideshow.
	
	{
		LL_RECORD_BLOCK_TIME(FTM_MEDIA_MISC);
		for(; iter != end; iter++)
		{
			LLViewerMediaImpl* pimpl = *iter;
		
			LLViewerMediaImpl::EPriority new_priority = LLViewerMediaImpl::PRIORITY_NORMAL;

			if(pimpl->isForcedUnloaded() || (impl_count_total >= (int)max_instances))
			{
				// Never load muted or failed impls.
				// Hard limit on the number of instances that will be loaded at one time
				new_priority = LLViewerMediaImpl::PRIORITY_UNLOADED;
			}
			else if(!pimpl->getVisible())
			{
				new_priority = LLViewerMediaImpl::PRIORITY_HIDDEN;
			}
			else if(pimpl->hasFocus())
			{
				new_priority = LLViewerMediaImpl::PRIORITY_HIGH;
				impl_count_interest_normal++;	// count this against the count of "normal" instances for priority purposes
			}
			else if(pimpl->getUsedInUI())
			{
				new_priority = LLViewerMediaImpl::PRIORITY_NORMAL;
				impl_count_interest_normal++;
			}
			else if(pimpl->isParcelMedia())
			{
				new_priority = LLViewerMediaImpl::PRIORITY_NORMAL;
				impl_count_interest_normal++;
			}
			else
			{
				// Look at interest and CPU usage for instances that aren't in any of the above states.
			
				// Heuristic -- if the media texture's approximate screen area is less than 1/4 of the native area of the texture,
				// turn it down to low instead of normal.  This may downsample for plugins that support it.
				bool media_is_small = false;
				F64 approximate_interest = pimpl->getApproximateTextureInterest();
				if(approximate_interest == 0.0f)
				{
					// this media has no current size, which probably means it's not loaded.
					media_is_small = true;
				}
				else if(pimpl->getInterest() < (approximate_interest / 4))
				{
					media_is_small = true;
				}
			
				if(pimpl->getInterest() == 0.0f)
				{
					// This media is completely invisible, due to being outside the view frustrum or out of range.
					new_priority = LLViewerMediaImpl::PRIORITY_HIDDEN;
				}
				else if(check_cpu_usage && (total_cpu > max_cpu))
				{
					// Higher priority plugins have already used up the CPU budget.  Set remaining ones to slideshow priority.
					new_priority = LLViewerMediaImpl::PRIORITY_SLIDESHOW;
				}
				else if((impl_count_interest_normal < (int)max_normal) && !media_is_small)
				{
					// Up to max_normal inworld get normal priority
					new_priority = LLViewerMediaImpl::PRIORITY_NORMAL;
					impl_count_interest_normal++;
				}
				else if (impl_count_interest_low + impl_count_interest_normal < (int)max_low + (int)max_normal)
				{
					// The next max_low inworld get turned down
					new_priority = LLViewerMediaImpl::PRIORITY_LOW;
					impl_count_interest_low++;
				
					// Set the low priority size for downsampling to approximately the size the texture is displayed at.
					{
						F32 approximate_interest_dimension = (F32) sqrt(pimpl->getInterest());
					
						pimpl->setLowPrioritySizeLimit(ll_round(approximate_interest_dimension));
					}
				}
				else
				{
					// Any additional impls (up to max_instances) get very infrequent time
					new_priority = LLViewerMediaImpl::PRIORITY_SLIDESHOW;
				}
			}
		
			if(!pimpl->getUsedInUI() && (new_priority != LLViewerMediaImpl::PRIORITY_UNLOADED))
			{
				// This is a loadable inworld impl -- the last one in the list in this class defines the lowest loadable interest.
				lowest_interest_loadable = pimpl;
			
				impl_count_total++;
			}

			// Overrides if the window is minimized or we lost focus (taking care
			// not to accidentally "raise" the priority either)
			if (!gViewerWindow->getActive() /* viewer window minimized? */ 
				&& new_priority > LLViewerMediaImpl::PRIORITY_HIDDEN)
			{
				new_priority = LLViewerMediaImpl::PRIORITY_HIDDEN;
			}
			else if (!gFocusMgr.getAppHasFocus() /* viewer window lost focus? */
					 && new_priority > LLViewerMediaImpl::PRIORITY_LOW)
			{
				new_priority = LLViewerMediaImpl::PRIORITY_LOW;
			}
		
			if(!inworld_media_enabled)
			{
				// If inworld media is locked out, force all inworld media to stay unloaded.
				if(!pimpl->getUsedInUI())
				{
					new_priority = LLViewerMediaImpl::PRIORITY_UNLOADED;
				}
			}
			// update the audio stream here as well
			if( !inworld_audio_enabled)
			{
				if(LLViewerMedia::isParcelAudioPlaying() && gAudiop && LLViewerMedia::hasParcelAudio())
				{
					gAudiop->stopInternetStream();
				}
			}
			pimpl->setPriority(new_priority);
		
			if(pimpl->getUsedInUI())
			{
				// Any impls used in the UI should not be in the proximity list.
				pimpl->mProximity = -1;
			}
			else
			{
				proximity_order.push_back(pimpl);
			}

			total_cpu += pimpl->getCPUUsage();
		
			if (!pimpl->getUsedInUI() && pimpl->hasMedia())
			{
				sAnyMediaShowing = true;
			}

		}
	}

	// Re-calculate this every time.
	sLowestLoadableImplInterest	= 0.0f;

	// Only do this calculation if we've hit the impl count limit -- up until that point we always need to load media data.
	if(lowest_interest_loadable && (impl_count_total >= (int)max_instances))
	{
		// Get the interest value of this impl's object for use by isInterestingEnough
		LLVOVolume *object = lowest_interest_loadable->getSomeObject();
		if(object)
		{
			// NOTE: Don't use getMediaInterest() here.  We want the pixel area, not the total media interest,
			// 		so that we match up with the calculation done in LLMediaDataClient.
			sLowestLoadableImplInterest = object->getPixelArea();
		}
	}
	
	static LLCachedControl<bool> mediaPerformanceManager(gSavedSettings, "MediaPerformanceManagerDebug");
	if(mediaPerformanceManager)
	{
		// Give impls the same ordering as the priority list
		// they're already in the right order for this.
	}
	else
	{
		LL_RECORD_BLOCK_TIME(FTM_MEDIA_SORT2);
		// Use a distance-based sort for proximity values.  
		std::stable_sort(proximity_order.begin(), proximity_order.end(), proximity_comparitor);
	}

	// Transfer the proximity order to the proximity fields in the objects.
	for(int i = 0; i < (int)proximity_order.size(); i++)
	{
		proximity_order[i]->mProximity = i;
	}
	
	LL_DEBUGS("PluginPriority") << "Total reported CPU usage is " << total_cpu << LL_ENDL;

}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isAnyMediaShowing()
{
	return sAnyMediaShowing;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setAllMediaEnabled(bool val)
{
	// Set "tentative" autoplay first.  We need to do this here or else
	// re-enabling won't start up the media below.
	gSavedSettings.setBOOL("MediaTentativeAutoPlay", val);
	
	// Then 
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	
	for(; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (!pimpl->getUsedInUI())
		{
			pimpl->setDisabled(!val);
		}
	}
	
	// Also do Parcel Media and Parcel Audio
	if (val)
	{
		if (!LLViewerMedia::isParcelMediaPlaying() && LLViewerMedia::hasParcelMedia())
		{	
			LLViewerParcelMedia::play(LLViewerParcelMgr::getInstance()->getAgentParcel());
		}
		
		if (gSavedSettings.getBOOL("AudioStreamingMusic") &&
			!LLViewerMedia::isParcelAudioPlaying() &&
			gAudiop && 
			LLViewerMedia::hasParcelAudio())
		{
			if (LLAudioEngine::AUDIO_PAUSED == gAudiop->isInternetStreamPlaying())
			{
				// 'false' means unpause
				gAudiop->pauseInternetStream(false);
			}
			else
			{
				LLViewerParcelMedia::playStreamingMusic(LLViewerParcelMgr::getInstance()->getAgentParcel());
			}
		}
	}
	else {
		// This actually unloads the impl, as opposed to "stop"ping the media
		LLViewerParcelMedia::stop();
		if (gAudiop)
		{
			gAudiop->stopInternetStream();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isParcelMediaPlaying()
{
	return (LLViewerMedia::hasParcelMedia() && LLViewerParcelMedia::getParcelMedia() && LLViewerParcelMedia::getParcelMedia()->hasMedia());
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::isParcelAudioPlaying()
{
	return (LLViewerMedia::hasParcelAudio() && gAudiop && LLAudioEngine::AUDIO_PLAYING == gAudiop->isInternetStreamPlaying());
}

void LLViewerMedia::onAuthSubmit(const LLSD& notification, const LLSD& response)
{
	LLViewerMediaImpl *impl = LLViewerMedia::getMediaImplFromTextureID(notification["payload"]["media_id"]);
	if(impl)
	{
		LLPluginClassMedia* media = impl->getMediaPlugin();
		if(media)
		{
			if (response["ok"])
			{
				media->sendAuthResponse(true, response["username"], response["password"]);
			}
			else
			{
				media->sendAuthResponse(false, "", "");
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::clearAllCookies()
{
	// Clear all cookies for all plugins
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	for (; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		LLPluginClassMedia* plugin = pimpl->getMediaPlugin();
		if(plugin)
		{
			plugin->clear_cookies();
		}
	}
	setOpenIDCookie();
}
	
/////////////////////////////////////////////////////////////////////////////////////////
// static 
void LLViewerMedia::clearAllCaches()
{
	// Clear all plugins' caches
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	for (; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->clearCache();
	}
}
	
/////////////////////////////////////////////////////////////////////////////////////////
// static 
void LLViewerMedia::setCookiesEnabled(bool enabled)
{
	// Set the "cookies enabled" flag for all loaded plugins
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	for (; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		LLPluginClassMedia* plugin = pimpl->getMediaPlugin();
		if(plugin)
		{
			plugin->cookies_enabled(enabled);
		}
	}
}
	
/////////////////////////////////////////////////////////////////////////////////////////
// static 
void LLViewerMedia::setProxyConfig(bool enable, const std::string &host, int port)
{
	// Set the proxy config for all loaded plugins
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	for (; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		LLPluginClassMedia* plugin = pimpl->getMediaPlugin();
		if(plugin)
		{
			//pimpl->mMediaSource->proxy_setup(enable, host, port);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// static 
/////////////////////////////////////////////////////////////////////////////////////////
//// static

AIHTTPHeaders LLViewerMedia::getHeaders()
{
	AIHTTPHeaders headers;
	headers.addHeader("Accept", "*/*");
	// *TODO: Should this be 'application/llsd+xml' ?
	// *TODO: Should this even be set at all?   This header is only not overridden in 'GET' methods.
	headers.addHeader("Content-Type", "application/xml");
	headers.addHeader("Cookie", sOpenIDCookie);
	headers.addHeader("User-Agent", getCurrentUserAgent());

	return headers;
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::parseRawCookie(const std::string raw_cookie, std::string& name, std::string& value, std::string& path, bool& httponly, bool& secure)
{
	std::size_t name_pos = raw_cookie.find_first_of('=');
	if (name_pos != std::string::npos)
	{
		name = raw_cookie.substr(0, name_pos);
		std::size_t value_pos = raw_cookie.find_first_of(';', name_pos);
		if (value_pos != std::string::npos)
		{
			value = raw_cookie.substr(name_pos + 1, value_pos - name_pos - 1);
			path = "/";	// assume root path for now

			httponly = true;	// hard coded for now
			secure = true;

			return true;
		}
	}

	return false;
}

void LLViewerMedia::setOpenIDCookie()
{
	if(!sOpenIDCookie.empty())
	{
		if (gSavedSettings.getString("WebProfileURL").empty()) return;
		std::string profileUrl = getProfileURL("");

        getOpenIDCookieCoro(profileUrl);
	}
}

/*static*/
void LLViewerMedia::getOpenIDCookieCoro(std::string url)
{
	// The LLURL can give me the 'authority', which is of the form: [username[:password]@]hostname[:port]
	// We want just the hostname for the cookie code, but LLURL doesn't seem to have a way to extract that.
	// We therefore do it here.
	std::string authority = sOpenIDURL.mAuthority;
	std::string::size_type hostStart = authority.find('@');
	if(hostStart == std::string::npos)
	{	// no username/password
		hostStart = 0;
	}
	else
	{	// Hostname starts after the @. 
		// (If the hostname part is empty, this may put host_start at the end of the string.  In that case, it will end up passing through an empty hostname, which is correct.)
		++hostStart;
	}
	std::string::size_type hostEnd = authority.rfind(':'); 
	if((hostEnd == std::string::npos) || (hostEnd < hostStart))
	{	// no port
		hostEnd = authority.size();
	}

	if (url.length())
	{
		LLMediaCtrl* media_instance = LLFloaterDestinations::getInstance()->getChild<LLMediaCtrl>("destination_guide_contents");
		if (media_instance)
		{
			std::string cookie_host = authority.substr(hostStart, hostEnd - hostStart);
			std::string cookie_name = "";
			std::string cookie_value = "";
			std::string cookie_path = "";
			bool httponly = true;
			bool secure = true;
			if (parseRawCookie(sOpenIDCookie, cookie_name, cookie_value, cookie_path, httponly, secure) &&
                media_instance->getMediaPlugin())
			{
				// MAINT-5711 - inexplicably, the CEF setCookie function will no longer set the cookie if the 
				// url and domain are not the same. This used to be my.sl.com and id.sl.com respectively and worked.
				// For now, we use the URL for the OpenID POST request since it will have the same authority
				// as the domain field.
				// (Feels like there must be a less dirty way to construct a URL from component LLURL parts)
				// MAINT-6392 - Rider: Do not change, however, the original URI requested, since it is used further
				// down.
                std::string cefUrl(std::string(sOpenIDURL.mURI) + "://" + std::string(sOpenIDURL.mAuthority));

				media_instance->getMediaPlugin()->setCookie(cefUrl, cookie_name, cookie_value, cookie_host, cookie_path, httponly, secure);
			}
		}
	}
		
    // Note: Rider: MAINT-6392 - Some viewer code requires access to the my.sl.com openid cookie for such 
    // actions as posting snapshots to the feed.  This is handled through HTTPCore rather than CEF and so 
    // we must learn to SHARE the cookies.

	// Do a web profile get so we can store the cookie
	AIHTTPHeaders headers;
	headers.addHeader("Accept", "*/*");
	headers.addHeader("Cookie", sOpenIDCookie);
	headers.addHeader("User-Agent", getCurrentUserAgent());

	LLURL raw_profile_url(url.data());

	LL_DEBUGS("MediaAuth") << "Requesting " << url << LL_ENDL;
	LL_DEBUGS("MediaAuth") << "sOpenIDCookie = [" << sOpenIDCookie << "]" << LL_ENDL;
	LLHTTPClient::get(url,
		new LLViewerMediaWebProfileResponder(raw_profile_url.getAuthority()),
		headers);
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::openIDSetup(const std::string &openidUrl, const std::string &openidToken)
{
	LL_DEBUGS("MediaAuth") << "url = \"" << openidUrl << "\", token = \"" << openidToken << "\"" << LL_ENDL;

	// post the token to the url 
	// the responder will need to extract the cookie(s).

	// Save the OpenID URL for later -- we may need the host when adding the cookie.
	sOpenIDURL.init(openidUrl.c_str());
	
	// We shouldn't ever do this twice, but just in case this code gets repurposed later, clear existing cookies.
	sOpenIDCookie.clear();

	AIHTTPHeaders headers;
	// Keep LLHTTPClient from adding an "Accept: application/llsd+xml" header
	headers.addHeader("Accept", "*/*");
	// and use the expected content-type for a post, instead of the LLHTTPClient::postRaw() default of "application/octet-stream"
	headers.addHeader("Content-Type", "application/x-www-form-urlencoded");

	// postRaw() takes ownership of the buffer and releases it later, so we need to allocate a new buffer here.
	size_t size = openidToken.size();
	U8* data = new U8[size];
	memcpy(data, openidToken.data(), size);

	LLHTTPClient::postRaw(
		openidUrl, 
		data,
		size, 
		new LLViewerMediaOpenIDResponder(),
		headers);
			
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::openIDCookieResponse(const std::string &cookie)
{
	LL_DEBUGS("MediaAuth") << "Cookie received: \"" << cookie << "\"" << LL_ENDL;
	
	sOpenIDCookie += cookie;

	setOpenIDCookie();
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::proxyWindowOpened(const std::string &target, const std::string &uuid)
{
	if(uuid.empty())
		return;
		
	for (impl_list::iterator iter = sViewerMediaImplList.begin(); iter != sViewerMediaImplList.end(); iter++)
	{
		LLPluginClassMedia* plugin = (*iter)->getMediaPlugin();
		if(plugin && plugin->pluginSupportsMediaBrowser())
		{
			plugin->proxyWindowOpened(target, uuid);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::proxyWindowClosed(const std::string &uuid)
{
	if(uuid.empty())
		return;

	for (impl_list::iterator iter = sViewerMediaImplList.begin(); iter != sViewerMediaImplList.end(); iter++)
	{
		LLPluginClassMedia* plugin = (*iter)->getMediaPlugin();
		if(plugin && plugin->pluginSupportsMediaBrowser())
		{
			plugin->proxyWindowClosed(uuid);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::createSpareBrowserMediaSource()
{
	static bool failedLoading = false;
	if (failedLoading) return;

	// If we don't have a spare browser media source, create one.
	// However, if PluginAttachDebuggerToPlugins is set then don't spawn a spare
	// SLPlugin process in order to not be confused by an unrelated gdb terminal
	// popping up at the moment we start a media plugin.
	if (!sSpareBrowserMediaSource && !gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins"))
	{
		// The null owner will keep the browser plugin from fully initializing 
		// (specifically, it keeps LLPluginClassMedia from negotiating a size change, 
		// which keeps MediaPluginWebkit::initBrowserWindow from doing anything until we have some necessary data, like the background color)
		sSpareBrowserMediaSource = LLViewerMediaImpl::newSourceFromMediaType("text/html", nullptr, 0, 0, 1.0);
		if (!sSpareBrowserMediaSource) failedLoading = true;
	}
}

/////////////////////////////////////////////////////////////////////////////////////////
// static
LLPluginClassMedia* LLViewerMedia::getSpareBrowserMediaSource() 
{
	LLPluginClassMedia* result = sSpareBrowserMediaSource;
	sSpareBrowserMediaSource = nullptr;
	return result; 
};

bool LLViewerMedia::hasInWorldMedia()
{
	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();
	// This should be quick, because there should be very few non-in-world-media impls
	for (; iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (!pimpl->getUsedInUI() && !pimpl->isParcelMedia())
		{
			// Found an in-world media impl
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::hasParcelMedia()
{
	return !LLViewerParcelMedia::getURL().empty();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
bool LLViewerMedia::hasParcelAudio()
{
	return !LLViewerMedia::getParcelAudioURL().empty();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
std::string LLViewerMedia::getParcelAudioURL()
{
	return LLViewerParcelMgr::getInstance()->getAgentParcel()->getMusicURL();
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::initClass()
{
	gIdleCallbacks.addFunction(LLViewerMedia::updateMedia, nullptr);
	sTeleportFinishConnection = LLViewerParcelMgr::getInstance()->
		setTeleportFinishedCallback(boost::bind(&LLViewerMedia::onTeleportFinished));
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::cleanupClass()
{
	gIdleCallbacks.deleteFunction(LLViewerMedia::updateMedia, nullptr);
	sTeleportFinishConnection.disconnect();
	if (sSpareBrowserMediaSource != nullptr)
	{
		delete sSpareBrowserMediaSource;
		sSpareBrowserMediaSource = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::onTeleportFinished()
{
	// On teleport, clear this setting (i.e. set it to true)
	gSavedSettings.setBOOL("MediaTentativeAutoPlay", true);

	LLViewerMediaImpl::sMimeTypesFailed.clear();
}


//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerMedia::setOnlyAudibleMediaTextureID(const LLUUID& texture_id)
{
	sOnlyAudibleTextureID = texture_id;
	sForceUpdate = true;
}

std::vector<std::string> LLViewerMediaImpl::sMimeTypesFailed;
//////////////////////////////////////////////////////////////////////////////////////////
// LLViewerMediaImpl
//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::LLViewerMediaImpl(	  const LLUUID& texture_id, 
										  S32 media_width, 
										  S32 media_height, 
										  U8 media_auto_scale, 
										  U8 media_loop)
:	
	mZoomFactor(1.0),
	mMovieImageHasMips(false),
	mMediaWidth(media_width),
	mMediaHeight(media_height),
	mMediaAutoScale(media_auto_scale),
	mMediaLoop(media_loop),
	mNeedsNewTexture(true),
	mTextureUsedWidth(0),
	mTextureUsedHeight(0),
	mSuspendUpdates(false),
	mVisible(true),
	mLastSetCursor( UI_CURSOR_ARROW ),
	mMediaNavState( MEDIANAVSTATE_NONE ),
	mInterest(0.0f),
	mUsedInUI(false),
	mHasFocus(false),
	mPriority(PRIORITY_UNLOADED),
	mNavigateRediscoverType(false),
	mNavigateServerRequest(false),
	mMediaSourceFailed(false),
	mRequestedVolume(1.0f),
	mPreviousVolume(1.0f),
	mIsMuted(false),
	mNeedsMuteCheck(false),
	mPreviousMediaState(MEDIA_NONE),
	mPreviousMediaTime(0.0f),
	mIsDisabled(false),
	mIsParcelMedia(false),
	mProximity(-1),
	mProximityDistance(0.0f),
	mMediaAutoPlay(false),
	mInNearbyMediaList(false),
	mClearCache(false),
	mBackgroundColor(LLColor4::white),
	mNavigateSuspended(false),
	mNavigateSuspendedDeferred(false),
	mTrustedBrowser(false),
    mCleanBrowser(false),
	mIsUpdated(false),
	mMimeProbe(nullptr)
{ 

	// Set up the mute list observer if it hasn't been set up already.
	if(!sViewerMediaMuteListObserverInitialized)
	{
		LLMuteList::getInstance()->addObserver(&sViewerMediaMuteListObserver);
		sViewerMediaMuteListObserverInitialized = true;
	}
	
	add_media_impl(this);

	setTextureID(texture_id);
	
	// connect this media_impl to the media texture, creating it if it doesn't exist.0
	// This is necessary because we need to be able to use getMaxVirtualSize() even if the media plugin is not loaded.
	LLViewerMediaTexture* media_tex = LLViewerTextureManager::getMediaTexture(mTextureId);
	if(media_tex)
	{
		media_tex->setMediaImpl();
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaImpl::~LLViewerMediaImpl()
{
	destroyMediaSource();
	
	LLViewerMediaTexture::removeMediaImplFromTexture(mTextureId) ;

	setTextureID();
	remove_media_impl(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::emitEvent(LLPluginClassMedia* plugin, LLViewerMediaObserver::EMediaEvent event)
{
	// Broadcast to observers using the superclass version
	LLViewerMediaEventEmitter::emitEvent(plugin, event);
	
	// If this media is on one or more LLVOVolume objects, tell them about the event as well.
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	while(iter != mObjectList.end())
	{
		LLVOVolume *self = *iter;
		++iter;
		self->mediaEvent(this, plugin, event);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializeMedia(const std::string& mime_type)
{
	bool mimeTypeChanged = (mMimeType != mime_type);
	bool pluginChanged = (LLMIMETypes::implType(mCurrentMimeType) != LLMIMETypes::implType(mime_type));

	if(!mPluginBase || pluginChanged)
	{
		// We don't have a plugin at all, or the new mime type is handled by a different plugin than the old mime type.
		(void)initializePlugin(mime_type);
	}
	else if(mimeTypeChanged)
	{
		// The same plugin should be able to handle the new media -- just update the stored mime type.
		mMimeType = mime_type;
	}	

	return (mPluginBase != nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::createMediaSource()
{
	if(mPriority == PRIORITY_UNLOADED)
	{
		// This media shouldn't be created yet.
		return;
	}
	
	if(! mMediaURL.empty())
	{
		navigateInternal();
	}
	else if(! mMimeType.empty())
	{
		if (!initializeMedia(mMimeType))
		{
			LL_WARNS("Media") << "Failed to initialize media for mime type " << mMimeType << LL_ENDL;
		}
	}

}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::destroyMediaSource()
{
	mNeedsNewTexture = true;

	// Tell the viewer media texture it's no longer active
	LLViewerMediaTexture* oldImage = LLViewerTextureManager::findMediaTexture( mTextureId );
	if (oldImage)
	{
		oldImage->setPlaying(FALSE) ;
	}
	
	cancelMimeTypeProbe();
	
	if(mPluginBase)
	{
		mPluginBase->setDeleteOK(true) ;
		destroyPlugin();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setMediaType(const std::string& media_type)
{
	mMimeType = media_type;
}

//////////////////////////////////////////////////////////////////////////////////////////
/*static*/
LLPluginClassMedia* LLViewerMediaImpl::newSourceFromMediaType(std::string media_type, LLPluginClassMediaOwner *owner /* may be NULL */, S32 default_width, S32 default_height, F64 zoom_factor, const std::string target, bool clean_browser)
{
	std::string plugin_basename = LLMIMETypes::implType(media_type);
	LLPluginClassMedia* media_source = nullptr;
	
	// HACK: we always try to keep a spare running webkit plugin around to improve launch times.
	// If a spare was already created before PluginAttachDebuggerToPlugins was set, don't use it.
    // Do not use a spare if launching with full viewer control (e.g. Facebook, Twitter and few others)
	if ((plugin_basename == "media_plugin_cef") &&
        !gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins") && !clean_browser)
	{
		media_source = LLViewerMedia::getSpareBrowserMediaSource();
		if(media_source)
		{
			media_source->setOwner(owner);
			media_source->setTarget(target);
			media_source->setSize(default_width, default_height);
			media_source->setZoomFactor(zoom_factor);
			media_source->set_page_zoom_factor(zoom_factor);
						
			return media_source;
		}
	}
	if(plugin_basename.empty())
	{
		LL_WARNS_ONCE("Media") << "Couldn't find plugin for media type " << media_type << LL_ENDL;
	}
	else
	{
		std::string launcher_name = gDirUtilp->getLLPluginLauncher();
		std::string plugin_name = gDirUtilp->getLLPluginFilename(plugin_basename);

		std::string user_data_path_cache = gDirUtilp->getCacheDir(false);
		user_data_path_cache += gDirUtilp->getDirDelimiter();

		std::string user_data_path_cookies = gDirUtilp->getOSUserAppDir();
		user_data_path_cookies += gDirUtilp->getDirDelimiter();

		std::string user_data_path_cef_log = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "cef_log.txt");

		// Fix for EXT-5960 - make browser profile specific to user (cache, cookies etc.)
		// If the linden username returned is blank, that can only mean we are
		// at the login page displaying login Web page or Web browser test via Develop menu.
		// In this case we just use whatever gDirUtilp->getOSUserAppDir() gives us (this
		// is what we always used before this change)
		std::string linden_user_dir = gDirUtilp->getLindenUserDir();
		if ( ! linden_user_dir.empty() )
		{
			user_data_path_cookies = linden_user_dir;
			user_data_path_cookies += gDirUtilp->getDirDelimiter();
		};

		// See if the plugin executable exists
		llstat s;
		if(LLFile::stat(launcher_name, &s))
		{
			LL_WARNS_ONCE("Media") << "Couldn't find launcher at " << launcher_name << LL_ENDL;
		}
		else if(LLFile::stat(plugin_name, &s))
		{
			LL_WARNS_ONCE("Media") << "Couldn't find plugin at " << plugin_name << LL_ENDL;
		}
		else
		{
			media_source = new LLPluginClassMedia(owner);
			media_source->proxy_setup(gSavedSettings.getBOOL("BrowserProxyEnabled"), gSavedSettings.getS32("BrowserProxyType"), 
				gSavedSettings.getString("BrowserProxyAddress"), gSavedSettings.getS32("BrowserProxyPort"),
				gSavedSettings.getString("BrowserProxyUsername"), gSavedSettings.getString("BrowserProxyPassword"));
			media_source->setSize(default_width, default_height);
			media_source->setUserDataPath(user_data_path_cache, user_data_path_cookies, user_data_path_cef_log);
			media_source->setLanguageCode(LLUI::getLanguage());
			media_source->setZoomFactor(zoom_factor);

			// collect 'cookies enabled' setting from prefs and send to embedded browser
			bool cookies_enabled = gSavedSettings.getBOOL( "CookiesEnabled" );
			media_source->cookies_enabled( cookies_enabled || clean_browser);

			// collect 'plugins enabled' setting from prefs and send to embedded browser
			bool plugins_enabled = gSavedSettings.getBOOL( "BrowserPluginsEnabled" );
			media_source->setPluginsEnabled( plugins_enabled  || clean_browser);

			// collect 'javascript enabled' setting from prefs and send to embedded browser
			bool javascript_enabled = gSavedSettings.getBOOL( "BrowserJavascriptEnabled" );
			media_source->setJavascriptEnabled( javascript_enabled || clean_browser);

			bool media_plugin_debugging_enabled = gSavedSettings.getBOOL("MediaPluginDebugging");
			media_source->enableMediaPluginDebugging( media_plugin_debugging_enabled  || clean_browser);

			// need to set agent string here before instance created
			media_source->setBrowserUserAgent(LLViewerMedia::getCurrentUserAgent());

			media_source->setTarget(target);
			
			const std::string plugin_dir = gDirUtilp->getLLPluginDir();
			if (media_source->init(launcher_name, plugin_dir, plugin_name, gSavedSettings.getBOOL("PluginAttachDebuggerToPlugins")))
			{
				return media_source;
			}
			else
			{
				LL_WARNS("Media") << "Failed to init plugin.  Destroying." << LL_ENDL;
				delete media_source;
			}
		}
	}
	
	LL_WARNS_ONCE("Plugin") << "plugin initialization failed for mime type: " << media_type << LL_ENDL;

	if(gAgent.isInitialized())
	{
	    if (std::find(sMimeTypesFailed.begin(), sMimeTypesFailed.end(), media_type) == sMimeTypesFailed.end())
	    {
			LLSD args;
			args["MIME_TYPE"] = media_type;
			LLNotificationsUtil::add("NoPlugin", args);
	        sMimeTypesFailed.push_back(media_type);
	    }
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::initializePlugin(const std::string& media_type)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		// Save the previous media source's last set size before destroying it.
		mMediaWidth = mMediaSource->getSetWidth();
		mMediaHeight = mMediaSource->getSetHeight();
		mZoomFactor = mMediaSource->getZoomFactor();
	}
	
	// Always delete the old media impl first.
	destroyMediaSource();
	
	// and unconditionally set the mime type
	mMimeType = media_type;

	if(mPriority == PRIORITY_UNLOADED)
	{
		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << this << "Not loading (PRIORITY_UNLOADED)" << LL_ENDL;
		
		return false;
	}

	// If we got here, we want to ignore previous init failures.
	mMediaSourceFailed = false;

	// Save the MIME type that really caused the plugin to load
	mCurrentMimeType = mMimeType;

	LLPluginClassMedia* media_source = newSourceFromMediaType(mMimeType, this, mMediaWidth, mMediaHeight, mZoomFactor, mTarget, mCleanBrowser);
	
	if (media_source)
	{
		media_source->setDisableTimeout(gSavedSettings.getBOOL("DebugPluginDisableTimeout"));
		media_source->setLoop(mMediaLoop);
		media_source->setAutoScale(mMediaAutoScale);
		media_source->setBrowserUserAgent(LLViewerMedia::getCurrentUserAgent());
		media_source->focus(mHasFocus);
		media_source->setBackgroundColor(mBackgroundColor);

		if(gSavedSettings.getBOOL("BrowserIgnoreSSLCertErrors"))
		{
			media_source->ignore_ssl_cert_errors(true);
		}

		// the correct way to deal with certs it to load ours from CA.pem and append them to the ones
		// Qt/WebKit loads from your system location.
		// Note: This needs the new CA.pem file with the Equifax Secure Certificate Authority 
		// cert at the bottom: (MIIDIDCCAomgAwIBAgIENd70zzANBg)
		std::string ca_path = gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS, "ca-bundle.crt" );
		media_source->addCertificateFilePath( ca_path );

		media_source->proxy_setup(gSavedSettings.getBOOL("BrowserProxyEnabled"), gSavedSettings.getS32("BrowserProxyType"), gSavedSettings.getString("BrowserProxyAddress"), gSavedSettings.getS32("BrowserProxyPort"),
			gSavedSettings.getString("BrowserProxyUsername"), gSavedSettings.getString("BrowserProxyPassword"));
		
		if(mClearCache)
		{
			mClearCache = false;
			media_source->clear_cache();
		}

		mPluginBase = media_source;
		mPluginBase->setDeleteOK(false) ;
		updateVolume();

		return true;
	}

	// Make sure the timer doesn't try re-initing this plugin repeatedly until something else changes.
	mMediaSourceFailed = true;

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::loadURI()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		// trim whitespace from front and back of URL - fixes EXT-5363
		LLStringUtil::trim( mMediaURL );

		// *HACK: we don't know if the URI coming in is properly escaped
		// (the contract doesn't specify whether it is escaped or not.
		// but LLQtWebKit expects it to be, so we do our best to encode
		// special characters)
		// The strings below were taken right from http://www.ietf.org/rfc/rfc1738.txt
		// Note especially that '%' and '/' are there.
		std::string uri = LLURI::escape(mMediaURL,
										"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
										"0123456789"
										"$-_.+"
										"!*'(),"
										"{}|\\^~[]`"
										"<>#%"
										";/?:@&=",
										false);
		{
			// Do not log the query parts
			LLURI u(uri);
			std::string sanitized_uri = (u.query().empty() ? uri : u.scheme() + "://" + u.authority() + u.path());
			LL_INFOS() << "Asking media source to load URI: " << sanitized_uri << LL_ENDL;
		}

		mMediaSource->loadURI( uri );
		
		// A non-zero mPreviousMediaTime means that either this media was previously unloaded by the priority code while playing/paused, 
		// or a seek happened before the media loaded.  In either case, seek to the saved time.
		if(mPreviousMediaTime != 0.0f)
		{
			seek(mPreviousMediaTime);
		}
			
		if(mPreviousMediaState == MEDIA_PLAYING)
		{
			// This media was playing before this instance was unloaded.
			start();
		}
		else if(mPreviousMediaState == MEDIA_PAUSED)
		{
			// This media was paused before this instance was unloaded.
			pause();
		}
		else
		{
			// No relevant previous media play state -- if we're loading the URL, we want to start playing.
			start();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setSize(int width, int height)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	mMediaWidth = width;
	mMediaHeight = height;
	if (mMediaSource)
	{
		mMediaSource->setSize(width, height);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::showNotification(LLNotificationPtr notify)
{
	mNotification = notify;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::hideNotification()
{
	mNotification.reset();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::play()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();

	// If the media source isn't there, try to initialize it and load an URL.
	if(mMediaSource == nullptr)
	{
	 	if(!initializeMedia(mMimeType))
		{
			// This may be the case where the plugin's priority is PRIORITY_UNLOADED
			return;
		}
		
		// Only do this if the media source was just loaded.
		loadURI();
	}
	
	// always start the media
	start();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::stop()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->stop();
		// destroyMediaSource();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::pause()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->pause();
	}
	else
	{
		mPreviousMediaState = MEDIA_PAUSED;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::start()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->start();
	}
	else
	{
		mPreviousMediaState = MEDIA_PLAYING;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::seek(F32 time)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->seek(time);
	}
	else
	{
		// Save the seek time to be set when the media is loaded.
		mPreviousMediaTime = time;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::skipBack(F32 step_scale)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		if(mMediaSource->pluginSupportsMediaTime())
		{
			F64 back_step = mMediaSource->getCurrentTime() - (mMediaSource->getDuration()*step_scale);
			if(back_step < 0.0)
			{
				back_step = 0.0;
			}
			mMediaSource->seek(back_step);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::skipForward(F32 step_scale)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		if(mMediaSource->pluginSupportsMediaTime())
		{
			F64 forward_step = mMediaSource->getCurrentTime() + (mMediaSource->getDuration()*step_scale);
			if(forward_step > mMediaSource->getDuration())
			{
				forward_step = mMediaSource->getDuration();
			}
			mMediaSource->seek(forward_step);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVolume(F32 volume)
{
	mRequestedVolume = volume;
	updateVolume();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setMute(bool mute)
{
	if (mute)
	{
		mPreviousVolume = mRequestedVolume;
		setVolume(0.0);
	}
	else
	{
		setVolume(mPreviousVolume);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateVolume()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		// always scale the volume by the global media volume 
		F32 volume = mRequestedVolume * LLViewerMedia::getVolume();

		if (mProximityCamera > 0) 
		{
			static LLCachedControl<F32> sMediaRollOffMax(gSavedSettings, "MediaRollOffMax", 30.f);
			static LLCachedControl<F32> sMediaRollOffMin(gSavedSettings, "MediaRollOffMin", 5.f);
			static LLCachedControl<F32> sMediaRollOffRate(gSavedSettings, "MediaRollOffRate", 0.125f);
			if (mProximityCamera > sMediaRollOffMax)
			{
				volume = 0;
			}
			else if (mProximityCamera > sMediaRollOffMin)
			{
				// attenuated_volume = 1 / (roll_off_rate * (d - min))^2
				// the +1 is there so that for distance 0 the volume stays the same
				F64 adjusted_distance = mProximityCamera - sMediaRollOffMin;
				F64 attenuation = 1.0 + (sMediaRollOffRate * adjusted_distance);
				attenuation = 1.0 / (attenuation * attenuation);
				// the attenuation multiplier should never be more than one since that would increase volume
				volume = volume * llmin(1.0, attenuation);
			}
		}

		if (sOnlyAudibleTextureID == LLUUID::null || sOnlyAudibleTextureID == mTextureId)
		{
			mMediaSource->setVolume(volume);
		}
		else
		{
			mMediaSource->setVolume(0.0f);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
F32 LLViewerMediaImpl::getVolume()
{
	return mRequestedVolume;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::focus(bool focus)
{
	mHasFocus = focus;

	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		// call focus just for the hell of it, even though this apopears to be a nop
		mMediaSource->focus(focus);
		if (focus)
		{
			// spoof a mouse click to *actually* pass focus
			// Don't do this anymore -- it actually clicks through now.
//			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, 1, 1, 0);
//			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 1, 1, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::hasFocus() const
{
	// FIXME: This might be able to be a bit smarter by hooking into LLViewerMediaFocus, etc.
	return mHasFocus;
}

std::string LLViewerMediaImpl::getCurrentMediaURL()
{
	if(!mCurrentMediaURL.empty())
	{
		return mCurrentMediaURL;
	}
	
	return mMediaURL;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::clearCache()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->clear_cache();
	}
	else
	{
		mClearCache = true;
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setPageZoomFactor( double factor )
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource && factor != mZoomFactor)
	{
		mZoomFactor = factor;
		mMediaSource->set_page_zoom_factor( factor );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDown(S32 x, S32 y, MASK mask, S32 button)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	LL_INFOS() << "mouse down (" << x << ", " << y << ")" << LL_ENDL;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseUp(S32 x, S32 y, MASK mask, S32 button)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	LL_INFOS() << "mouse up (" << x << ", " << y << ")" << LL_ENDL;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseMove(S32 x, S32 y, MASK mask)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
//	LL_INFOS() << "mouse move (" << x << ", " << y << ")" << LL_ENDL;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_MOVE, 0, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//static 
void LLViewerMediaImpl::scaleTextureCoords(const LLVector2& texture_coords, S32 *x, S32 *y)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	F32 texture_x = texture_coords.mV[VX];
	F32 texture_y = texture_coords.mV[VY];
	
	// Deal with repeating textures by wrapping the coordinates into the range [0, 1.0)
	texture_x = fmodf(texture_x, 1.0f);
	if(texture_x < 0.0f)
		texture_x = 1.0 + texture_x;
		
	texture_y = fmodf(texture_y, 1.0f);
	if(texture_y < 0.0f)
		texture_y = 1.0 + texture_y;

	// scale x and y to texel units.
	*x = ll_round(texture_x * mMediaSource->getTextureWidth());
	*y = ll_round((1.0f - texture_y) * mMediaSource->getTextureHeight());

	// Adjust for the difference between the actual texture height and the amount of the texture in use.
	*y -= (mMediaSource->getTextureHeight() - mMediaSource->getHeight());
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDown(const LLVector2& texture_coords, MASK mask, S32 button)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);

		mouseDown(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseUp(const LLVector2& texture_coords, MASK mask, S32 button)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{		
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);

		mouseUp(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseMove(const LLVector2& texture_coords, MASK mask)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{		
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);

		mouseMove(x, y, mask);
	}
}

void LLViewerMediaImpl::mouseDoubleClick(const LLVector2& texture_coords, MASK mask)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
    if (mMediaSource)
    {
        S32 x, y;
        scaleTextureCoords(texture_coords, &x, &y);

        mouseDoubleClick(x, y, mask);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseDoubleClick(S32 x, S32 y, MASK mask, S32 button)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOUBLE_CLICK, button, x, y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::scrollWheel(S32 x, S32 y, S32 scroll_x, S32 scroll_y, MASK mask)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->scrollEvent(x, y, scroll_x, scroll_y, mask);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::onMouseCaptureLost()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 0, mLastMouseX, mLastMouseY, 0);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
BOOL LLViewerMediaImpl::handleMouseUp(S32 x, S32 y, MASK mask) 
{ 
	// NOTE: this is called when the mouse is released when we have capture.
	// Due to the way mouse coordinates are mapped to the object, we can't use the x and y coordinates that come in with the event.
	
	if(hasMouseCapture())
	{
		// Release the mouse -- this will also send a mouseup to the media
		gFocusMgr.setMouseCapture( FALSE );
	}

	return TRUE; 
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateJavascriptObject()
{
	static LLFrameTimer timer ;

	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if ( mMediaSource )
	{
		// flag to expose this information to internal browser or not.
		static LLCachedControl<bool> enable(gSavedSettings, "BrowserEnableJSObject", false);

		if(!enable)
		{
			return ; //no need to go further.
		}

		if(timer.getElapsedTimeF32() < 1.0f)
		{
			return ; //do not update more than once per second.
		}
		timer.reset() ;

		mMediaSource->jsEnableObject( enable );

		// these values are only menaingful after login so don't set them before
		//bool logged_in = LLLoginInstance::getInstance()->authSuccess();
		bool logged_in = (LLStartUp::getStartupState() >= STATE_STARTED);
		if ( logged_in )
		{
			// current location within a region
			LLVector3 agent_pos = gAgent.getPositionAgent();
			double x = agent_pos.mV[ VX ];
			double y = agent_pos.mV[ VY ];
			double z = agent_pos.mV[ VZ ];
			mMediaSource->jsAgentLocationEvent( x, y, z );

			// current location within the grid
			LLVector3d agent_pos_global = gAgent.getLastPositionGlobal();
			double global_x = agent_pos_global.mdV[ VX ];
			double global_y = agent_pos_global.mdV[ VY ];
			double global_z = agent_pos_global.mdV[ VZ ];
			mMediaSource->jsAgentGlobalLocationEvent( global_x, global_y, global_z );

			// current agent orientation
			double rotation = atan2( gAgent.getAtAxis().mV[VX], gAgent.getAtAxis().mV[VY] );
			double angle = rotation * RAD_TO_DEG;
			if ( angle < 0.0f ) angle = 360.0f + angle;	// TODO: has to be a better way to get orientation!
			mMediaSource->jsAgentOrientationEvent( angle );

			// current region agent is in
			std::string region_name("");
			LLViewerRegion* region = gAgent.getRegion();
			if ( region )
			{
				region_name = region->getName();
			};
			mMediaSource->jsAgentRegionEvent( region_name );
		}

		// language code the viewer is set to
		mMediaSource->jsAgentLanguageEvent( LLUI::getLanguage() );

		// maturity setting the agent has selected
		if ( gAgent.prefersAdult() )
			mMediaSource->jsAgentMaturityEvent( "GMA" );	// Adult means see adult, mature and general content
		else
		if ( gAgent.prefersMature() )
			mMediaSource->jsAgentMaturityEvent( "GM" );	// Mature means see mature and general content
		else
		if ( gAgent.prefersPG() )
			mMediaSource->jsAgentMaturityEvent( "G" );	// PG means only see General content
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
const std::string& LLViewerMediaImpl::getName() const 
{ 
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		return mMediaSource->getMediaName();
	}
	
	return LLStringUtil::null; 
};

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateBack()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		mMediaSource->browse_back();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateForward()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		mMediaSource->browse_forward();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateReload()
{
	navigateTo(getCurrentMediaURL(), "", true, false);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateHome()
{
	bool rediscover_mimetype = mHomeMimeType.empty();
	navigateTo(mHomeURL, mHomeMimeType, rediscover_mimetype, false);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::unload()
{
	// Unload the media impl and clear its state.
	destroyMediaSource();
	resetPreviousMediaState();
	mMediaURL.clear();
	mMimeType.clear();
	mCurrentMediaURL.clear();
	mCurrentMimeType.clear();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateTo(const std::string& url, const std::string& mime_type,  bool rediscover_type, bool server_request, bool clean_browser)
{
	if (url.empty())
	{
		LL_WARNS() << "Calling LLViewerMediaImpl::navigateTo with empty url" << LL_ENDL;
		return;
	}

	cancelMimeTypeProbe();

	if(mMediaURL != url)
	{
		// Don't carry media play state across distinct URLs.
		resetPreviousMediaState();
	}
	
	// Always set the current URL and MIME type.
	mMediaURL = url;
	mMimeType = mime_type;
    mCleanBrowser = clean_browser;
	
	// Clear the current media URL, since it will no longer be correct.
	mCurrentMediaURL.clear();
	
	// if mime type discovery was requested, we'll need to do it when the media loads
	mNavigateRediscoverType = rediscover_type;
	
	// and if this was a server request, the navigate on load will also need to be one.
	mNavigateServerRequest = server_request;
	
	// An explicit navigate resets the "failed" flag.
	mMediaSourceFailed = false;

	if(mPriority == PRIORITY_UNLOADED)
	{
		// Helpful to have media urls in log file. Shouldn't be spammy.
		{
			// Do not log the query parts
			LLURI u(url);
			std::string sanitized_url = (u.query().empty() ? url : u.scheme() + "://" + u.authority() + u.path());
			LL_INFOS() << "NOT LOADING media id= " << mTextureId << " url=" << sanitized_url << ", mime_type=" << mime_type << LL_ENDL;
		}

		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << this << "Not loading (PRIORITY_UNLOADED)" << LL_ENDL;
		
		return;
	}

	navigateInternal();
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateInternal()
{
	// Helpful to have media urls in log file. Shouldn't be spammy.
	{
		// Do not log the query parts
		LLURI u(mMediaURL);
		std::string sanitized_url = (u.query().empty() ? mMediaURL : u.scheme() + "://" + u.authority() + u.path());
		LL_INFOS() << "media id= " << mTextureId << " url=" << sanitized_url << ", mime_type=" << mMimeType << LL_ENDL;
	}

	if (mMediaURL.empty())
	{
		LL_WARNS() << "Calling LLViewerMediaImpl::navigateInternal() with empty mMediaURL" << LL_ENDL;
		return;
	}

	if(mNavigateSuspended)
	{
		LL_WARNS() << "Deferring navigate." << LL_ENDL;
		mNavigateSuspendedDeferred = true;
		return;
	}
	
	if(mMimeProbe != nullptr)
	{
		LL_WARNS() << "MIME type probe already in progress -- bailing out." << LL_ENDL;
		return;
	}
	
	if(mNavigateServerRequest)
	{
		setNavState(MEDIANAVSTATE_SERVER_SENT);
	}
	else
	{
		setNavState(MEDIANAVSTATE_NONE);
	}
			
	// If the caller has specified a non-empty MIME type, look that up in our MIME types list.
	// If we have a plugin for that MIME type, use that instead of attempting auto-discovery.
	// This helps in supporting legacy media content where the server the media resides on returns a bogus MIME type
	// but the parcel owner has correctly set the MIME type in the parcel media settings.
	
	if(!mMimeType.empty() && (mMimeType != LLMIMETypes::getDefaultMimeType()))
	{
		std::string plugin_basename = LLMIMETypes::implType(mMimeType);
		if(!plugin_basename.empty())
		{
			// We have a plugin for this mime type
			mNavigateRediscoverType = false;
		}
	}

	if(mNavigateRediscoverType)
	{

		LLURI uri(mMediaURL);
		std::string scheme = uri.scheme();

		if(scheme.empty() || "http" == scheme || "https" == scheme)
		{
			// If we don't set an Accept header, LLHTTPClient will add one like this:
			//    Accept: application/llsd+xml
			// which is really not what we want.
			AIHTTPHeaders headers;
			headers.addHeader("Accept", "*/*");
			// Allow cookies in the response, to prevent a redirect loop when accessing join.secondlife.com
			headers.addHeader("Cookie", "");
			LLHTTPClient::getHeaderOnly( mMediaURL, new LLMimeDiscoveryResponder(this), headers);
		}
		else if("data" == scheme || "file" == scheme || "about" == scheme)
		{
			if ("blank" != uri.hostName())
			{
				// FIXME: figure out how to really discover the type for these schemes
				// We use "data" internally for a text/html url for loading the login screen
				if(initializeMedia("text/html"))
				{
					loadURI();
				}
			}
		}
		else
		{
			// This catches 'rtsp://' urls
			if(initializeMedia(scheme))
			{
				loadURI();
			}
		}
	}
	else if(initializeMedia(mMimeType))
	{
		loadURI();
	}
	else
	{
		LL_WARNS("Media") << "Couldn't navigate to: " << mMediaURL << " as there is no media type for: " << mMimeType << LL_ENDL;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::navigateStop()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->browse_stop();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleKeyHere(KEY key, MASK mask)
{
	bool result = false;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	
	if (mMediaSource)
	{
		switch (mask)
		{
		case MASK_CONTROL:
		{
			result = true; // Avoid redundant code, set false for default.
			switch(key)
			{
			// FIXME: THIS IS SO WRONG.
			// Menu keys should be handled by the menu system and not passed to UI elements, but this is how LLTextEditor and LLLineEditor do it...
			case KEY_LEFT: case KEY_RIGHT: case KEY_HOME: case KEY_END: break;

			case 'C': mMediaSource->copy(); break;
			case 'V': mMediaSource->paste(); break;
			case 'X': mMediaSource->cut(); break;
			case '=': setPageZoomFactor(mZoomFactor + .1); break;
			case '-': setPageZoomFactor(mZoomFactor - .1);  break;
			case '0': setPageZoomFactor(1.0);  break;

			default: result = false; break;
			}
			break;
		}
		case MASK_SHIFT|MASK_CONTROL:
			if (key == 'I')
			{
				mMediaSource->showWebInspector(true);
				result = true;
			}
			break;
		}

		// Singu Note: At the very least, let's allow the login menu to function
		extern LLMenuBarGL* gLoginMenuBarView;
		result = result || (gLoginMenuBarView && gLoginMenuBarView->getVisible() && gLoginMenuBarView->handleAcceleratorKey(key, mask));

		if(!result)
		{
			LLSD native_key_data = gViewerWindow->getWindow()->getNativeKeyData();
			result = mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_DOWN, key, mask, native_key_data);
		}
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleKeyUpHere(KEY key, MASK mask)
{
	bool result = false;

	LLPluginClassMedia* mMediaSource = getMediaPlugin();

	if (mMediaSource)
	{
		// FIXME: THIS IS SO WRONG.
		// Menu keys should be handled by the menu system and not passed to UI elements, but this is how LLTextEditor and LLLineEditor do it...
		if (MASK_CONTROL & mask && key != KEY_LEFT && key != KEY_RIGHT && key != KEY_HOME && key != KEY_END)
		{
			result = true;
		}

		if (!result)
		{
			LLSD native_key_data = gViewerWindow->getWindow()->getNativeKeyData();
			result = mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_UP, key, mask, native_key_data);
		}
	}

	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::handleUnicodeCharHere(llwchar uni_char)
{
	bool result = false;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	
	if (mMediaSource)
	{
		// only accept 'printable' characters, sigh...
		if (uni_char >= 32 // discard 'control' characters
			&& uni_char != 127) // SDL thinks this is 'delete' - yuck.
		{
			LLSD native_key_data = gViewerWindow->getWindow()->getNativeKeyData();
			
			mMediaSource->textInput(wstring_to_utf8str(LLWString(1, uni_char)), gKeyboard->currentMask(FALSE), native_key_data);
		}
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateForward()
{
	BOOL result = FALSE;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		result = mMediaSource->getHistoryForwardAvailable();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::canNavigateBack()
{
	BOOL result = FALSE;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
	{
		result = mMediaSource->getHistoryBackAvailable();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
static LLTrace::BlockTimerStatHandle FTM_MEDIA_DO_UPDATE("Do Update");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_GET_DATA("Get Data");
static LLTrace::BlockTimerStatHandle FTM_MEDIA_SET_SUBIMAGE("Set Subimage");


void LLViewerMediaImpl::update()
{
	LL_RECORD_BLOCK_TIME(FTM_MEDIA_DO_UPDATE);
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource == nullptr)
	{
		if(mPriority == PRIORITY_UNLOADED)
		{
			// This media source should not be loaded.
		}
		else if(mPriority <= PRIORITY_SLIDESHOW)
		{
			// Don't load new instances that are at PRIORITY_SLIDESHOW or below.  They're just kept around to preserve state.
		}
		else if(mMimeProbe != nullptr)
		{
			// this media source is doing a MIME type probe -- don't try loading it again.
		}
		else
		{
			// This media may need to be loaded.
			if(sMediaCreateTimer.hasExpired())
			{
				LL_DEBUGS("PluginPriority") << this << ": creating media based on timer expiration" << LL_ENDL;
				createMediaSource();
				sMediaCreateTimer.setTimerExpirySec(LLVIEWERMEDIA_CREATE_DELAY);
			}
			else
			{
				LL_DEBUGS("PluginPriority") << this << ": NOT creating media (waiting on timer)" << LL_ENDL;
			}
		}
	}
	else
	{
		updateVolume();

		// TODO: this is updated every frame - is this bad?
		updateJavascriptObject();
	}


	if(mMediaSource == nullptr)
	{
		return;
	}
	
	// Make sure a navigate doesn't happen during the idle -- it can cause mMediaSource to get destroyed, which can cause a crash.
	setNavigateSuspended(true);
	
	mMediaSource->idle();

	setNavigateSuspended(false);

	mMediaSource = getMediaPlugin();
	if(mMediaSource == nullptr)
	{
		return;
	}
	
	if(mMediaSource->isPluginExited())
	{
		resetPreviousMediaState();
		destroyMediaSource();
		return;
	}

	if(!mMediaSource->textureValid())
	{
		return;
	}
	
	if(mSuspendUpdates || !mVisible)
	{
		return;
	}
	
	LLViewerMediaTexture* placeholder_image = updatePlaceholderImage();
		
	if(placeholder_image)
	{
		LLRect dirty_rect;
		
		// Since we're updating this texture, we know it's playing.  Tell the texture to do its replacement magic so it gets rendered.
		placeholder_image->setPlaying(TRUE);

		if(mMediaSource->getDirty(&dirty_rect))
		{
			// Constrain the dirty rect to be inside the texture
			S32 x_pos = llmax(dirty_rect.mLeft, 0);
			S32 y_pos = llmax(dirty_rect.mBottom, 0);
			S32 width = llmin(dirty_rect.mRight, placeholder_image->getWidth()) - x_pos;
			S32 height = llmin(dirty_rect.mTop, placeholder_image->getHeight()) - y_pos;
			
			if(width > 0 && height > 0)
			{

				U8* data = nullptr;
				{
					LL_RECORD_BLOCK_TIME(FTM_MEDIA_GET_DATA);
					data = mMediaSource->getBitsData();
				}

				if(data != NULL)
				{
				// Offset the pixels pointer to match x_pos and y_pos
				data += ( x_pos * mMediaSource->getTextureDepth() * mMediaSource->getBitsWidth() );
				data += ( y_pos * mMediaSource->getTextureDepth() );
				
				{
					LL_RECORD_BLOCK_TIME(FTM_MEDIA_SET_SUBIMAGE);
					placeholder_image->setSubImage(
							data, 
							mMediaSource->getBitsWidth(),
							mMediaSource->getBitsHeight(),
							x_pos, 
							y_pos, 
							width, 
							height,
							TRUE); // <alchemy/>
					}
				}

			}
			
			mMediaSource->resetDirty();
		}
	}
}


//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::updateImagesMediaStreams()
{
}


//////////////////////////////////////////////////////////////////////////////////////////
LLViewerMediaTexture* LLViewerMediaImpl::updatePlaceholderImage()
{
	if(mTextureId.isNull())
	{
		// The code that created this instance will read from the plugin's bits.
		return nullptr;
	}
	
	LLViewerMediaTexture* placeholder_image = LLViewerTextureManager::getMediaTexture( mTextureId );
	
	LLPluginClassMedia* mMediaSource = getMediaPlugin();

	if (mNeedsNewTexture 
		|| placeholder_image->getUseMipMaps()
		|| (placeholder_image->getWidth() != mMediaSource->getTextureWidth())
		|| (placeholder_image->getHeight() != mMediaSource->getTextureHeight())
		|| (mTextureUsedWidth != mMediaSource->getWidth())
		|| (mTextureUsedHeight != mMediaSource->getHeight())
		)
	{
		LL_DEBUGS("Media") << "initializing media placeholder" << LL_ENDL;
		LL_DEBUGS("Media") << "movie image id " << mTextureId << LL_ENDL;

		int texture_width = mMediaSource->getTextureWidth();
		int texture_height = mMediaSource->getTextureHeight();
		int texture_depth = mMediaSource->getTextureDepth();
		
		// MEDIAOPT: check to see if size actually changed before doing work
		placeholder_image->destroyGLTexture();
		// MEDIAOPT: apparently just calling setUseMipMaps(FALSE) doesn't work?
		placeholder_image->reinit(FALSE);	// probably not needed

		// MEDIAOPT: seems insane that we actually have to make an imageraw then
		// immediately discard it
		LLPointer<LLImageRaw> raw = new LLImageRaw(texture_width, texture_height, texture_depth);
		// Clear the texture to the background color, ignoring alpha.
		// convert background color channels from [0.0, 1.0] to [0, 255];
		raw->clear(int(mBackgroundColor.mV[VX] * 255.0f), int(mBackgroundColor.mV[VY] * 255.0f), int(mBackgroundColor.mV[VZ] * 255.0f), 0xff);
		int discard_level = 0;

		// ask media source for correct GL image format constants
		placeholder_image->setExplicitFormat(mMediaSource->getTextureFormatInternal(),
											 mMediaSource->getTextureFormatPrimary(),
											 mMediaSource->getTextureFormatType(),
											 mMediaSource->getTextureFormatSwapBytes());

		placeholder_image->createGLTexture(discard_level, raw);

		// MEDIAOPT: set this dynamically on play/stop
		// FIXME
//		placeholder_image->mIsMediaTexture = true;
		mNeedsNewTexture = false;
				
		// If the amount of the texture being drawn by the media goes down in either width or height, 
		// recreate the texture to avoid leaving parts of the old image behind.
		mTextureUsedWidth = mMediaSource->getWidth();
		mTextureUsedHeight = mMediaSource->getHeight();
	}
	
	return placeholder_image;
}


//////////////////////////////////////////////////////////////////////////////////////////
LLUUID LLViewerMediaImpl::getMediaTextureID() const
{
	return mTextureId;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::setVisible(bool visible)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	mVisible = visible;
	
	if(mVisible)
	{
		if(mMediaSource && mMediaSource->isPluginExited())
		{
			destroyMediaSource();
			mMediaSource = NULL;
		}

		if(!mMediaSource)
		{
			createMediaSource();
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::mouseCapture()
{
	gFocusMgr.setMouseCapture(this);
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::scaleMouse(S32 *mouse_x, S32 *mouse_y)
{
#if 0
	S32 media_width, media_height;
	S32 texture_width, texture_height;
	getMediaSize( &media_width, &media_height );
	getTextureSize( &texture_width, &texture_height );
	S32 y_delta = texture_height - media_height;

	*mouse_y -= y_delta;
#endif
}



//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaTimeBased()
{
	bool result = false;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	
	if(mMediaSource)
	{
		result = mMediaSource->pluginSupportsMediaTime();
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPlaying()
{
	bool result = false;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	
	if(mMediaSource)
	{
		EMediaStatus status = mMediaSource->getStatus();
		if(status == MEDIA_PLAYING || status == MEDIA_LOADING)
			result = true;
	}
	
	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////
bool LLViewerMediaImpl::isMediaPaused()
{
	bool result = false;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();

	if(mMediaSource)
	{
		if(mMediaSource->getStatus() == MEDIA_PAUSED)
			result = true;
	}
	
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::hasMedia() const
{
	return mPluginBase != NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
void LLViewerMediaImpl::resetPreviousMediaState()
{
	mPreviousMediaState = MEDIA_NONE;
	mPreviousMediaTime = 0.0f;
}


//////////////////////////////////////////////////////////////////////////////////////////
//
void LLViewerMediaImpl::setDisabled(bool disabled, bool forcePlayOnEnable)
{
	if(mIsDisabled != disabled)
	{
		// Only do this on actual state transitions.
		mIsDisabled = disabled;
		
		if(mIsDisabled)
		{
			// We just disabled this media.  Clear all state.
			unload();
		}
		else
		{
			// We just (re)enabled this media.  Do a navigate if auto-play is in order.
			if(isAutoPlayable() || forcePlayOnEnable)
			{
				navigateTo(mMediaEntryURL, "", true, true);
			}
		}

	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isForcedUnloaded() const
{
	if(mIsMuted || mMediaSourceFailed || mIsDisabled)
	{
		return true;
	}

	// If this media's class is not supposed to be shown, unload
	if (!shouldShowBasedOnClass())
	{
		return true;
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isPlayable() const
{
	if(isForcedUnloaded())
	{
		// All of the forced-unloaded criteria also imply not playable.
		return false;
	}

	if(hasMedia())
	{
		// Anything that's already playing is, by definition, playable.
		return true;
	}

	if(!mMediaURL.empty())
	{
		// If something has navigated the instance, it's ready to be played.
		return true;
	}

	return false;
}

static void handle_pick_file_request_continued(LLPluginClassMedia* plugin, AIFilePicker* filepicker)
{
	plugin->sendPickFileResponse(filepicker->hasFilename() ? filepicker->getFilenames() : std::vector<std::string>());
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerMediaImpl::handleMediaEvent(LLPluginClassMedia* plugin, LLPluginClassMediaOwner::EMediaEvent event)
{
	bool pass_through = true;
	switch(event)
	{
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is: " << plugin->getClickURL() << LL_ENDL; 
			std::string url = plugin->getClickURL();
			std::string nav_type = plugin->getClickNavType();
			LLURLDispatcher::dispatch(url, nav_type, nullptr, mTrustedBrowser);
		}
		break;
		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << plugin->getClickTarget() << "\", uri is " << plugin->getClickURL() << LL_ENDL;
		};
		break;
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			// The plugin failed to load properly.  Make sure the timer doesn't retry.
			// TODO: maybe mark this plugin as not loadable somehow?
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();
			
			// TODO: may want a different message for this case?
			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mCurrentMimeType);
			LLNotificationsUtil::add("MediaPluginFailed", args);
		}
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			// The plugin crashed.
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();

			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mCurrentMimeType);
			// SJB: This is getting called every frame if the plugin fails to load, continuously respawining the alert!
			//LLNotificationsUtil::add("MediaPluginFailed", args);
		}
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << plugin->getCursorName() << LL_ENDL;

			std::string cursor = plugin->getCursorName();
			
			if(cursor == "arrow")
				mLastSetCursor = UI_CURSOR_ARROW;
			else if(cursor == "ibeam")
				mLastSetCursor = UI_CURSOR_IBEAM;
			else if(cursor == "splith")
				mLastSetCursor = UI_CURSOR_SIZEWE;
			else if(cursor == "splitv")
				mLastSetCursor = UI_CURSOR_SIZENS;
			else if(cursor == "hand")
				mLastSetCursor = UI_CURSOR_HAND;
			else // for anything else, default to the arrow
				mLastSetCursor = UI_CURSOR_ARROW;
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_FILE_DOWNLOAD:
		{
			//llinfos << "Media event - file download requested - filename is " << self->getFileDownloadFilename() << llendl;
			LLNotificationsUtil::add("MediaFileDownloadUnsupported");
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_BEGIN, uri is: " << plugin->getNavigateURI() << LL_ENDL;
			hideNotification();

			if(getNavState() == MEDIANAVSTATE_SERVER_SENT)
			{
				setNavState(MEDIANAVSTATE_SERVER_BEGUN);
			}
			else
			{
				setNavState(MEDIANAVSTATE_BEGUN);
			}
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_COMPLETE, uri is: " << plugin->getNavigateURI() << LL_ENDL;

			std::string url = plugin->getNavigateURI();
			if(getNavState() == MEDIANAVSTATE_BEGUN)
			{
				if(mCurrentMediaURL == url)
				{
					// This is a navigate that takes us to the same url as the previous navigate.
					setNavState(MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS);
				}
				else
				{
					mCurrentMediaURL = url;
					setNavState(MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED);
				}
			}
			else if(getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = url;
				setNavState(MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED);
			}
			else
			{
				// all other cases need to leave the state alone.
			}
		}
		break;
		
		case LLViewerMediaObserver::MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_LOCATION_CHANGED, uri is: " << plugin->getLocation() << LL_ENDL;

			std::string url = plugin->getLocation();

			if(getNavState() == MEDIANAVSTATE_BEGUN)
			{
				if(mCurrentMediaURL == url)
				{
					// This is a navigate that takes us to the same url as the previous navigate.
					setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS);
				}
				else
				{
					mCurrentMediaURL = url;
					setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED);
				}
			}
			else if(getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = url;
				setNavState(MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED);
			}
			else
			{
				// Don't track redirects.
				setNavState(MEDIANAVSTATE_NONE);
			}
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			AIFilePicker* filepicker = AIFilePicker::create();
			filepicker->open(FFLOAD_ALL, "", "openfile", true);
			filepicker->run(boost::bind(&handle_pick_file_request_continued, plugin, filepicker));
			// Display a file picker
			//std::string response;
			
			/*LLFilePicker& picker = LLFilePicker::instance();
			if (!picker.getOpenFile(LLFilePicker::FFLOAD_ALL))
			{
				// The user didn't pick a file -- the empty response string will indicate this.
			}
			
			response = picker.getFirstFile();
			
			plugin->sendPickFileResponse(response);*/
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_AUTH_REQUEST:
		{
			LLNotification::Params auth_request_params("AuthRequest");

			// pass in host name and realm for site (may be zero length but will always exist)
			LLSD args;
			LLURL raw_url( plugin->getAuthURL().c_str() );
			args["HOST_NAME"] = raw_url.getAuthority();
			args["REALM"] = plugin->getAuthRealm();
			auth_request_params.substitutions = args;

			auth_request_params.payload = LLSD().with("media_id", mTextureId);
			auth_request_params.functor(boost::bind(&LLViewerMedia::onAuthSubmit, _1, _2));
			LLNotifications::instance().add(auth_request_params);
		};
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_CLOSE_REQUEST:
		{
			std::string uuid = plugin->getClickUUID();

			LL_INFOS() << "MEDIA_EVENT_CLOSE_REQUEST for uuid " << uuid << LL_ENDL;

			if(uuid.empty())
			{
				// This close request is directed at this instance, let it fall through.
			}
			else
			{
				// This close request is directed at another instance
				pass_through = false;
				LLFloaterWebContent::closeRequest(uuid);
			}
		}
		break;

		case LLViewerMediaObserver::MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			std::string uuid = plugin->getClickUUID();

			LL_INFOS() << "MEDIA_EVENT_GEOMETRY_CHANGE for uuid " << uuid << LL_ENDL;

			if(uuid.empty())
			{
				// This geometry change request is directed at this instance, let it fall through.
			}
			else
			{
				// This request is directed at another instance
				pass_through = false;
				LLFloaterWebContent::geometryChanged(uuid, plugin->getGeometryX(), plugin->getGeometryY(), plugin->getGeometryWidth(), plugin->getGeometryHeight());
			}
		}
		break;

		case MEDIA_EVENT_DEBUG_MESSAGE:
		{
			std::string level = plugin->getDebugMessageLevel();
			if (level == "debug")
			{
				LL_DEBUGS("Media") << plugin->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "info")
			{
				LL_INFOS("Media") << plugin->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "warn")
			{
				LL_WARNS("Media") << plugin->getDebugMessageText() << LL_ENDL;
			}
			else if (level == "error")
			{
				LL_ERRS("Media") << plugin->getDebugMessageText() << LL_ENDL;
			}
			else
			{
				LL_INFOS("Media") << plugin->getDebugMessageText() << LL_ENDL;
			}
		};
		break;

		default:
		break;
	}

	if(pass_through)
	{
		// Just chain the event to observers.
		emitEvent(plugin, event);
	}
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::undo()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->undo();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canUndo() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canUndo();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::redo()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->redo();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canRedo() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canRedo();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::cut()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->cut();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCut() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canCut();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::copy() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->copy();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canCopy() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canCopy();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::paste()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->paste();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canPaste() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canPaste();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::doDelete()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->doDelete();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canDoDelete() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canDoDelete();
	else
		return FALSE;
}

////////////////////////////////////////////////////////////////////////////////
// virtual
void
LLViewerMediaImpl::selectAll()
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		mMediaSource->selectAll();
}

////////////////////////////////////////////////////////////////////////////////
// virtual
BOOL
LLViewerMediaImpl::canSelectAll() const
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if (mMediaSource)
		return mMediaSource->canSelectAll();
	else
		return FALSE;
}

void LLViewerMediaImpl::setUpdated(BOOL updated)
{
	mIsUpdated = updated ;
}

BOOL LLViewerMediaImpl::isUpdated()
{
	return mIsUpdated ;
}

static LLTrace::BlockTimerStatHandle FTM_MEDIA_CALCULATE_INTEREST("Calculate Interest");

void LLViewerMediaImpl::calculateInterest()
{
	LL_RECORD_BLOCK_TIME(FTM_MEDIA_CALCULATE_INTEREST);
	LLViewerMediaTexture* texture = LLViewerTextureManager::findMediaTexture( mTextureId );
	
	if(texture != nullptr)
	{
		mInterest = texture->getMaxVirtualSize();
	}
	else
	{
		// This will be a relatively common case now, since it will always be true for unloaded media.
		mInterest = 0.0f;
	}
	
	// Calculate distance from the avatar, for use in the proximity calculation.
	mProximityDistance = 0.0f;
	mProximityCamera = 0.0f;
	if(!mObjectList.empty())
	{
		// Just use the first object in the list.  We could go through the list and find the closest object, but this should work well enough.
		std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
		LLVOVolume* objp = *iter ;
		llassert_always(objp != NULL) ;
		
		// The distance calculation is invalid for HUD attachments -- leave both mProximityDistance and mProximityCamera at 0 for them.
		if(!objp->isHUDAttachment())
		{
			LLVector3d obj_global = objp->getPositionGlobal() ;
			LLVector3d agent_global = gAgent.getPositionGlobal() ;
			LLVector3d global_delta = agent_global - obj_global ;
			mProximityDistance = global_delta.magVecSquared();  // use distance-squared because it's cheaper and sorts the same.

			LLVector3d camera_delta = gAgentCamera.getCameraPositionGlobal() - obj_global;
			mProximityCamera = camera_delta.magVec();
		}
	}
	
	if(mNeedsMuteCheck)
	{
		// Check all objects this instance is associated with, and those objects' owners, against the mute list
		mIsMuted = false;
		
		std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
		for(; iter != mObjectList.end() ; ++iter)
		{
			LLVOVolume *obj = *iter;
			llassert(obj);
			if (!obj) continue;
			if(LLMuteList::getInstance() &&
			   LLMuteList::getInstance()->isMuted(obj->getID()))
			{
				mIsMuted = true;
			}
			else
			{
				// We won't have full permissions data for all objects.  Attempt to mute objects when we can tell their owners are muted.
				if (LLSelectMgr::getInstance())
				{
					LLPermissions* obj_perm = LLSelectMgr::getInstance()->findObjectPermissions(obj);
					if(obj_perm)
					{
						if(LLMuteList::getInstance() &&
						   LLMuteList::getInstance()->isMuted(obj_perm->getOwner()))
							mIsMuted = true;
					}
				}
			}
		}
		
		mNeedsMuteCheck = false;
	}
}

F64 LLViewerMediaImpl::getApproximateTextureInterest()
{
	F64 result = 0.0f;
	
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		result = mMediaSource->getFullWidth();
		result *= mMediaSource->getFullHeight();
	}
	else
	{
		// No media source is loaded -- all we have to go on is the texture size that has been set on the impl, if any.
		result = mMediaWidth;
		result *= mMediaHeight;
	}

	return result;
}

void LLViewerMediaImpl::setUsedInUI(bool used_in_ui)
{
	mUsedInUI = used_in_ui; 
	
	// HACK: Force elements used in UI to load right away.
	// This fixes some issues where UI code that uses the browser instance doesn't expect it to be unloaded.
	if(mUsedInUI && (mPriority == PRIORITY_UNLOADED))
	{
		if(getVisible())
		{
			setPriority(PRIORITY_NORMAL);
		}
		else
		{
			setPriority(PRIORITY_HIDDEN);
		}

		createMediaSource();
	}
};

void LLViewerMediaImpl::setBackgroundColor(LLColor4 color)
{
	mBackgroundColor = color; 

	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->setBackgroundColor(mBackgroundColor);
	}
};

F64 LLViewerMediaImpl::getCPUUsage() const
{
	F64 result = 0.0f;
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	
	if(mMediaSource)
	{
		result = mMediaSource->getCPUUsage();
	}
	
	return result;
}

char const* PRIORITYToString(LLViewerMediaImpl::EPriority priority)
{
	switch(priority)
	{
	case LLViewerMediaImpl::PRIORITY_UNLOADED:	return "unloaded";
	case LLViewerMediaImpl::PRIORITY_HIDDEN:	return "hidden";
	case LLViewerMediaImpl::PRIORITY_SLIDESHOW: return "slideshow";
	case LLViewerMediaImpl::PRIORITY_LOW:		return "low";
	case LLViewerMediaImpl::PRIORITY_NORMAL:	return "normal";
	case LLViewerMediaImpl::PRIORITY_HIGH:		return "high";
	default:									return "UNKNOWN";
	}
}

void LLViewerMediaImpl::setPriority(EPriority priority)
{
	if(mPriority != priority)
	{
		LL_DEBUGS("PluginPriority")
			<< "changing priority of media id " << mTextureId
			<< " from " << ::PRIORITYToString(mPriority)
			<< " to " << ::PRIORITYToString(priority)
			<< LL_ENDL;
	}
	
	mPriority = priority;
	
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(priority == PRIORITY_UNLOADED)
	{
		if(mMediaSource)
		{
			// Need to unload the media source
			
			// First, save off previous media state
			mPreviousMediaState = mMediaSource->getStatus();
			mPreviousMediaTime = mMediaSource->getCurrentTime();
			
			destroyMediaSource();
			mMediaSource = NULL;
		}
	}

	if(mMediaSource)
	{
		if(mPriority >= PRIORITY_LOW)
			mMediaSource->setPriority((LLPluginClassBasic::EPriority)((U32)mPriority-((U32)PRIORITY_LOW-1)));
		else
			mMediaSource->setPriority(LLPluginClassBasic::PRIORITY_SLEEP);
	}
	
	// NOTE: loading (or reloading) media sources whose priority has risen above PRIORITY_UNLOADED is done in update().
}

void LLViewerMediaImpl::setLowPrioritySizeLimit(int size)
{
	LLPluginClassMedia* mMediaSource = getMediaPlugin();
	if(mMediaSource)
	{
		mMediaSource->setLowPrioritySizeLimit(size);
	}
}

void LLViewerMediaImpl::setNavState(EMediaNavState state)
{
	mMediaNavState = state;
	
	switch (state) 
	{
		case MEDIANAVSTATE_NONE: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_NONE" << LL_ENDL; break;
		case MEDIANAVSTATE_BEGUN: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_BEGUN" << LL_ENDL; break;
		case MEDIANAVSTATE_FIRST_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_FIRST_LOCATION_CHANGED" << LL_ENDL; break;
		case MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS" << LL_ENDL; break;
		case MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED" << LL_ENDL; break;
		case MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS" << LL_ENDL; break;
		case MEDIANAVSTATE_SERVER_SENT: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_SENT" << LL_ENDL; break;
		case MEDIANAVSTATE_SERVER_BEGUN: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_BEGUN" << LL_ENDL; break;
		case MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED" << LL_ENDL; break;
		case MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED: LL_DEBUGS("Media") << "Setting nav state to MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED" << LL_ENDL; break;
	}
}

void LLViewerMediaImpl::setNavigateSuspended(bool suspend)
{
	if(mNavigateSuspended != suspend)
	{
		mNavigateSuspended = suspend;
		if(!suspend)
		{
			// We're coming out of suspend.  If someone tried to do a navigate while suspended, do one now instead.
			if(mNavigateSuspendedDeferred)
			{
				mNavigateSuspendedDeferred = false;
				navigateInternal();
			}
		}
	}
}

void LLViewerMediaImpl::cancelMimeTypeProbe()
{
	if(mMimeProbe)
	{
		// There doesn't seem to be a way to actually cancel an outstanding request.
		// Simulate it by telling the LLMimeDiscoveryResponder not to write back any results.
		mMimeProbe->cancelRequest();
		
		// The above should already have set mMimeProbe to nullptr.
		if (mMimeProbe)
		{
			LL_ERRS() << "internal error: mMimeProbe is not nullptr after cancelling request." << LL_ENDL;
		}
	}
}

void LLViewerMediaImpl::addObject(LLVOVolume* obj) 
{
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	for(; iter != mObjectList.end() ; ++iter)
	{
		if(*iter == obj)
		{
			return ; //already in the list.
		}
	}

	mObjectList.push_back(obj) ;
	mNeedsMuteCheck = true;
}
	
void LLViewerMediaImpl::removeObject(LLVOVolume* obj) 
{
	mObjectList.remove(obj) ;	
	mNeedsMuteCheck = true;
}
	
const std::list< LLVOVolume* >* LLViewerMediaImpl::getObjectList() const 
{
	return &mObjectList ;
}

LLVOVolume *LLViewerMediaImpl::getSomeObject()
{
	LLVOVolume *result = nullptr;
	
	std::list< LLVOVolume* >::iterator iter = mObjectList.begin() ;
	if(iter != mObjectList.end())
	{
		result = *iter;
	}
	
	return result;
}

void LLViewerMediaImpl::setTextureID(LLUUID id)
{
	if(id != mTextureId)
	{
		if(mTextureId.notNull())
		{
			// Remove this item's entry from the map
			sViewerMediaTextureIDMap.erase(mTextureId);
		}
		
		if(id.notNull())
		{
			sViewerMediaTextureIDMap.insert(LLViewerMedia::impl_id_map::value_type(id, this));
		}
		
		mTextureId = id;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isAutoPlayable() const
{
	static const LLCachedControl<bool> media_tentative_auto_play("MediaTentativeAutoPlay",false);
	static const LLCachedControl<bool> auto_play_parcel_media(LLViewerMedia::AUTO_PLAY_MEDIA_SETTING,false);
	static const LLCachedControl<bool> auto_play_prim_media(LLViewerMedia::AUTO_PLAY_PRIM_MEDIA_SETTING,false);
	return mMediaAutoPlay && media_tentative_auto_play &&
		(getUsedInUI()
		|| (isParcelMedia() && auto_play_parcel_media)
		|| auto_play_prim_media);
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::shouldShowBasedOnClass() const
{
	// If this is parcel media or in the UI, return true always
	if (getUsedInUI() || isParcelMedia()) return true;
	
	bool attached_to_another_avatar = isAttachedToAnotherAvatar();
	bool inside_parcel = isInAgentParcel();
	
	//	LL_INFOS() << " hasFocus = " << hasFocus() <<
	//	" others = " << (attached_to_another_avatar && gSavedSettings.getBOOL(LLViewerMedia::SHOW_MEDIA_ON_OTHERS_SETTING)) <<
	//	" within = " << (inside_parcel && gSavedSettings.getBOOL(LLViewerMedia::SHOW_MEDIA_WITHIN_PARCEL_SETTING)) <<
	//	" outside = " << (!inside_parcel && gSavedSettings.getBOOL(LLViewerMedia::SHOW_MEDIA_OUTSIDE_PARCEL_SETTING)) << LL_ENDL;
	
	// If it has focus, we should show it
	// This is incorrect, and causes EXT-6750 (disabled attachment media still plays)
//	if (hasFocus())
//		return true;
	
	// If it is attached to an avatar and the pref is off, we shouldn't show it
	if (attached_to_another_avatar)
	{
		static LLCachedControl<bool> show_media_on_others(gSavedSettings, LLViewerMedia::SHOW_MEDIA_ON_OTHERS_SETTING, false);
		return show_media_on_others;
	}
	if (inside_parcel)
	{
		static LLCachedControl<bool> show_media_within_parcel(gSavedSettings, LLViewerMedia::SHOW_MEDIA_WITHIN_PARCEL_SETTING, true);

		return show_media_within_parcel;
	}
	else 
	{
		static LLCachedControl<bool> show_media_outside_parcel(gSavedSettings, LLViewerMedia::SHOW_MEDIA_OUTSIDE_PARCEL_SETTING, true);

		return show_media_outside_parcel;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isAttachedToAnotherAvatar() const
{
	bool result = false;
	
	std::list< LLVOVolume* >::const_iterator iter = mObjectList.begin();
	std::list< LLVOVolume* >::const_iterator end = mObjectList.end();
	for ( ; iter != end; iter++)
	{
		if (isObjectAttachedToAnotherAvatar(*iter))
		{
			result = true;
			break;
		}
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
//static
bool LLViewerMediaImpl::isObjectAttachedToAnotherAvatar(LLVOVolume *obj)
{
	bool result = false;
	LLXform *xform = obj;
	// Walk up parent chain
	while (nullptr != xform)
	{
		LLViewerObject *object = dynamic_cast<LLViewerObject*> (xform);
		if (nullptr != object)
		{
			LLVOAvatar *avatar = object->asAvatar();
			if ((nullptr != avatar) && (avatar != gAgentAvatarp))
			{
				result = true;
				break;
			}
		}
		xform = xform->getParent();
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
bool LLViewerMediaImpl::isInAgentParcel() const
{
	bool result = false;
	
	std::list< LLVOVolume* >::const_iterator iter = mObjectList.begin();
	std::list< LLVOVolume* >::const_iterator end = mObjectList.end();
	for ( ; iter != end; iter++)
	{
		LLVOVolume *object = *iter;
		if (LLViewerMediaImpl::isObjectInAgentParcel(object))
		{
			result = true;
			break;
		}
	}
	return result;
}

LLNotificationPtr LLViewerMediaImpl::getCurrentNotification() const
{
	return mNotification;
}

//////////////////////////////////////////////////////////////////////////////////////////
//
// static
bool LLViewerMediaImpl::isObjectInAgentParcel(LLVOVolume *obj)
{
	return (LLViewerParcelMgr::getInstance()->inAgentParcel(obj->getPositionGlobal()));
}
