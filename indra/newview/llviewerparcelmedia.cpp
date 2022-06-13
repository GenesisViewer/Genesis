/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
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
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llaudioengine.h"
#include "llmimetypes.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmediaautoplay.h"
#include "llnotificationsutil.h"
#include "llviewerwindow.h"
#include "llfirstuse.h"
#include "llpluginclassmedia.h"
#include "llmediafilter.h"
#include "llnotify.h"
#include "llsdserialize.h"
#include "llaudioengine.h"
#include "lloverlaybar.h"
#include "llstreamingaudio.h"

// Static Variables

S32 LLViewerParcelMedia::sMediaParcelLocalID = 0;
LLUUID LLViewerParcelMedia::sMediaRegionID;
viewer_media_t LLViewerParcelMedia::sMediaImpl;
F32 LLViewerParcelMedia::sMediaCommandTime = 0;

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert(const LLSD& notification, const LLSD& response, LLParcel* parcel, U32 type, std::string domain);

// static
void LLViewerParcelMedia::initClass()
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", processParcelMediaCommandMessage );
	msg->setHandlerFunc("ParcelMediaUpdate", processParcelMediaUpdate );
	LLViewerParcelMediaAutoPlay::initClass();
}

//static 
void LLViewerParcelMedia::cleanupClass()
{
	// This needs to be destroyed before global destructor time.
	sMediaImpl = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				sMediaRegionID = LLUUID() ;
				stop() ;
				LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
				return ;				
			}

			// we're in a parcel
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			if (parcelid != sMediaParcelLocalID || regionid != sMediaRegionID)
			{
				LL_DEBUGS("Media") << "New parcel, parcel id = " << parcelid << ", region id = " << regionid << LL_ENDL;
				sMediaParcelLocalID = parcelid;
				sMediaRegionID = regionid;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			std::string mediaCurrentUrl = std::string( parcel->getMediaCurrentURL());

			// First use warning
			if(	! mediaUrl.empty() && gSavedSettings.getWarning("FirstStreamingVideo") )
			{
				LLNotificationsUtil::add("ParcelCanPlayMedia", LLSD(), LLSD(),
					boost::bind(&callback_play_media, _1, _2, parcel));
				return;

			}

			// if we have a current (link sharing) url, use it instead
			if (mediaCurrentUrl != "" && parcel->getMediaType() == "text/html")
			{
				mediaUrl = mediaCurrentUrl;
			}
			
			LLStringUtil::trim(mediaUrl);
			
			// If no parcel media is playing, nothing left to do
			if(sMediaImpl.isNull())

			{
				return;
			}

			// Media is playing...has something changed?
			else if (( sMediaImpl->getMediaURL() != mediaUrl )
				|| ( sMediaImpl->getMediaTextureID() != parcel->getMediaID() )
				|| ( sMediaImpl->getMimeType() != parcel->getMediaType() ))
			{
				// Only play if the media types are the same.
				if(sMediaImpl->getMimeType() == parcel->getMediaType())
				{
					if (gSavedSettings.getU32("MediaFilterEnable"))
					{
						LL_DEBUGS("MediaFilter") << "Filtering media URL: " << parcel->getMediaURL() << LL_ENDL;
						LLMediaFilter::getInstance()->filterMediaUrl(parcel);
					}
					else
					{
						play(parcel);
					}
				}

				else
				{
					stop();
				}
			}
		}
		else
		{
			stop();
		}
	}
	/*
	else
	{
		// no audio player, do a first use dialog if there is media here
		if (parcel)
		{
			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			if (!mediaUrl.empty ())
			{
				if (gSavedSettings.getWarning("QuickTimeInstalled"))
				{
					gSavedSettings.setWarning("QuickTimeInstalled", FALSE);

					LLNotificationsUtil::add("NoQuickTime" );
				};
			}
		}
	}
	*/
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel)
{
	LL_DEBUGS() << "LLViewerParcelMedia::play" << LL_ENDL;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingMedia"))
		return;

	std::string media_url = parcel->getMediaURL();
	std::string media_current_url = parcel->getMediaCurrentURL();
	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	if(sMediaImpl)
	{
		// If the url and mime type are the same, call play again
		if(sMediaImpl->getMediaURL() == media_url 
			&& sMediaImpl->getMimeType() == mime_type
			&& sMediaImpl->getMediaTextureID() == placeholder_texture_id)
		{
			LL_DEBUGS("Media") << "playing with existing url " << media_url << LL_ENDL;

			sMediaImpl->play();
		}
		// Else if the texture id's are the same, navigate and rediscover type
		// MBW -- This causes other state from the previous parcel (texture size, autoscale, and looping) to get re-used incorrectly.
		// It's also not really necessary -- just creating a new instance is fine.
//		else if(sMediaImpl->getMediaTextureID() == placeholder_texture_id)
//		{
//			sMediaImpl->navigateTo(media_url, mime_type, true);
//		}
		else
		{
			// Since the texture id is different, we need to generate a new impl

			// Delete the old one first so they don't fight over the texture.
			sMediaImpl = NULL;
			
			// A new impl will be created below.
		}
	}
	
	// Don't ever try to play if the media type is set to "none/none"
	if(stricmp(mime_type.c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if(!sMediaImpl)
		{
			LL_DEBUGS("Media") << "new media impl with mime type " << mime_type << ", url " << media_url << LL_ENDL;

			// There is no media impl, make a new one
			sMediaImpl = LLViewerMedia::newMediaImpl(
				placeholder_texture_id,
				media_width, 
				media_height, 
				media_auto_scale,
				media_loop);
			sMediaImpl->setIsParcelMedia(true);
			sMediaImpl->navigateTo(media_url, mime_type, true);
		}

		LLFirstUse::useMedia();

		LLViewerParcelMediaAutoPlay::playStarted();
	}
}

// static
void LLViewerParcelMedia::stop()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	
	// We need to remove the media HUD if it is up.
	LLViewerMediaFocus::getInstance()->clearFocus();

	// This will kill the media instance.
	sMediaImpl->stop();
	sMediaImpl = NULL;
}

// static
void LLViewerParcelMedia::pause()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->pause();
}

// static
void LLViewerParcelMedia::start()
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->start();

	LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	if(sMediaImpl.isNull())
	{
		return;
	}
	sMediaImpl->seek(time);
}

// static
void LLViewerParcelMedia::focus(bool focus)
{
	sMediaImpl->focus(focus);
}

// static
LLViewerMediaImpl::EMediaStatus LLViewerParcelMedia::getStatus()
{	
	LLViewerMediaImpl::EMediaStatus result = LLViewerMediaImpl::MEDIA_NONE;
	
	if(sMediaImpl.notNull() && sMediaImpl->hasMedia())
	{
		result = sMediaImpl->getMediaPlugin()->getStatus();
	}
	
	return result;
}

// static
std::string LLViewerParcelMedia::getMimeType()
{
	return sMediaImpl.notNull() ? sMediaImpl->getMimeType() : LLMIMETypes::getDefaultMimeType();
}

//static 
std::string LLViewerParcelMedia::getURL()
{
	std::string url;
	if(sMediaImpl.notNull())
		url = sMediaImpl->getMediaURL();
	
	if(stricmp(LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaType().c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaCurrentURL();
		
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaURL();
	}
	
	return url;
}

//static 
std::string LLViewerParcelMedia::getName()
{
	if(sMediaImpl.notNull())
		return sMediaImpl->getName();
	return "";
}

viewer_media_t LLViewerParcelMedia::getParcelMedia()
{
	return sMediaImpl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg, void ** )
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );

	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			if (!LLMediaFilter::getInstance()->isAlertActive())
			{
				stop();
			}
			else
			{
				LLMediaFilter::getInstance()->setQueuedMediaCommand(PARCEL_MEDIA_COMMAND_STOP);
			}
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			if (!LLMediaFilter::getInstance()->isAlertActive())
			{
				pause();
			}
			else
			{
				LLMediaFilter::getInstance()->setQueuedMediaCommand(PARCEL_MEDIA_COMMAND_PAUSE);
			}
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				if (gSavedSettings.getU32("MediaFilterEnable"))
				{
					LL_DEBUGS("MediaFilter") << "PARCEL_MEDIA_COMMAND_PLAY: Filtering media URL: " << parcel->getMediaURL() << LL_ENDL;
					LLMediaFilter::getInstance()->filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			if (!LLMediaFilter::getInstance()->isAlertActive())
			{
				stop();
			}
			else
			{
				LLMediaFilter::getInstance()->setQueuedMediaCommand(PARCEL_MEDIA_COMMAND_UNLOAD);
			}
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if(sMediaImpl.isNull())
		{
			LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			if (gSavedSettings.getU32("MediaFilterEnable"))
			{
				LL_DEBUGS("MediaFilter") << "PARCEL_MEDIA_COMMAND_TIME: Filtering media URL: " << parcel->getMediaURL() << LL_ENDL;
				LLMediaFilter::getInstance()->filterMediaUrl(parcel);
			}
			else
			{
				play(parcel);
			}
		}
	}
	if (!LLMediaFilter::getInstance()->isAlertActive())
	{
		seek(time);
	}
	else
	{
		LLMediaFilter::getInstance()->setQueuedMediaCommand(PARCEL_MEDIA_COMMAND_TIME);
		sMediaCommandTime = time;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg, void ** )
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			if (sMediaImpl.notNull())
			{
				if (gSavedSettings.getU32("MediaFilterEnable"))
				{
					LL_DEBUGS("MediaFilter") << "Parcel media changed. Filtering media URL: " << parcel->getMediaURL() << LL_ENDL;
					LLMediaFilter::getInstance()->filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
	}
}
// Static
/////////////////////////////////////////////////////////////////////////////////////////
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	std::string region_url = gAgent.getRegion()->getCapability("ParcelNavigateMedia");
	if (!region_url.empty())
	{
		// send navigate event to sim for link sharing
		LLSD body;
		body["agent-id"] = gAgent.getID();
		body["local-id"] = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
		body["url"] = url;
		LLHTTPClient::post(region_url, body, new LLHTTPClient::ResponderIgnore);
	}
	else
	{
		LL_WARNS() << "can't get ParcelNavigateMedia capability" << LL_ENDL;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
// virtual 
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_DEBUG_MESSAGE:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_DEBUG_MESSAGE " << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_ERROR_PAGE" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << self->getClickTarget() << "\", uri is " << self->getClickURL() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is " << self->getClickUUID() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_AUTH_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_AUTH_REQUEST, url " << self->getAuthURL() << ", realm " << self->getAuthRealm() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_LINK_HOVERED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LINK_HOVERED, hover text is: " << self->getHoverText() << LL_ENDL;
		};
		break;

		default:
		{
			LL_WARNS("Media") <<  "Media event:  unknown event type" << LL_ENDL;
		}
	};
}

bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		gSavedSettings.setBOOL("AudioStreamingMedia", TRUE);
		if (gSavedSettings.getU32("MediaFilterEnable"))
		{
			LLMediaFilter::getInstance()->filterMediaUrl(parcel);
		}
		else
		{
			LLViewerParcelMedia::play(parcel);
		}
	}
	else
	{
		gSavedSettings.setBOOL("AudioStreamingMedia", FALSE);
	}
	gSavedSettings.setWarning("FirstStreamingVideo", FALSE);
	return false;
}

// TODO: observer
/*
void LLViewerParcelMediaNavigationObserver::onNavigateComplete( const EventType& event_in )
{
	std::string url = event_in.getStringValue();

	if (mCurrentURL != url && ! mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}

	mCurrentURL = url;
	mFromMessage = false;

}
*/

void LLViewerParcelMedia::playStreamingMusic(LLParcel* parcel, bool filter)
{
	std::string music_url = parcel->getMusicURL();
	LLStringUtil::trim(music_url);
	if (gSavedSettings.getU32("MediaFilterEnable"))
	{
		LL_DEBUGS("MediaFilter") << "Filtering media URL: " << parcel->getMediaURL() << LL_ENDL;
		LLMediaFilter::getInstance()->filterAudioUrl(music_url);
	}
	else if (gAudiop)
	{
		LLStringUtil::trim(music_url);
		LLStreamingAudioInterface *stream = gAudiop->getStreamingAudioImpl();
		if(stream && stream->supportsAdjustableBufferSizes())
			stream->setBufferSizes(gSavedSettings.getU32("SHFMODExStreamBufferSize"),gSavedSettings.getU32("SHFMODExDecodeBufferSize"));
		gAudiop->startInternetStream(music_url);
		if (music_url.empty())
		{
			LLOverlayBar::audioFilterStop();
		}
		else
		{
			LLOverlayBar::audioFilterPlay();
		}
	}
}

void LLViewerParcelMedia::stopStreamingMusic()
{
	if (gAudiop)
	{
		gAudiop->stopInternetStream();
		LLOverlayBar::audioFilterStop();
	}
}
