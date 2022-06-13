/** 
 * @file media_plugin_base.cpp
 * @brief Media plugin base class for LLMedia API plugin system
 *
 * All plugins should be a subclass of MediaPluginBase. 
 *
 * @cond
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 * @endcond
 */

#include "linden_common.h"
#include "media_plugin_base.h"


// TODO: Make sure that the only symbol exported from this library is LLPluginInitEntryPoint

////////////////////////////////////////////////////////////////////////////////
/// Media plugin constructor.
///
/// @param[in] send_message_function Function for sending messages from plugin to plugin loader shell
/// @param[in] host_user_data Message data for messages from plugin to plugin loader shell
MediaPluginBase::MediaPluginBase(LLPluginInstance::sendMessageFunction send_message_function, LLPluginInstance* plugin_instance)
	: BasicPluginBase(send_message_function, plugin_instance)
{
	mPixels = 0;
	mWidth = 0;
	mHeight = 0;
	mTextureWidth = 0;
	mTextureHeight = 0;
	mDepth = 0;
	mStatus = STATUS_NONE;
}

/**
 * Converts current media status enum value into string (STATUS_LOADING into "loading", etc.)
 * 
 * @return Media status string ("loading", "playing", "paused", etc)
 *
 */
std::string MediaPluginBase::statusString()
{
	std::string result;
	
	switch(mStatus)
	{
		case STATUS_LOADING:	result = "loading";		break;
		case STATUS_LOADED:		result = "loaded";		break;
		case STATUS_ERROR:		result = "error";		break;
		case STATUS_PLAYING:	result = "playing";		break;
		case STATUS_PAUSED:		result = "paused";		break;
		case STATUS_DONE:		result = "done";		break;
		default:
			// keep the empty string
		break;
	}
	
	return result;
}
	
/**
 * Set media status.
 * 
 * @param[in] status Media status (STATUS_LOADING, STATUS_PLAYING, STATUS_PAUSED, etc)
 *
 */
void MediaPluginBase::setStatus(EStatus status)
{
	if(mStatus != status)
	{
		mStatus = status;
		sendStatus();
	}
}

/**
 * Notifies plugin loader shell that part of display area needs to be redrawn.
 * 
 * @param[in] left Left X coordinate of area to redraw (0,0 is at top left corner)
 * @param[in] top Top Y coordinate of area to redraw (0,0 is at top left corner)
 * @param[in] right Right X-coordinate of area to redraw (0,0 is at top left corner)
 * @param[in] bottom Bottom Y-coordinate of area to redraw (0,0 is at top left corner)
 *
 */
void MediaPluginBase::setDirty(int left, int top, int right, int bottom)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "updated");

	message.setValueS32("left", left);
	message.setValueS32("top", top);
	message.setValueS32("right", right);
	message.setValueS32("bottom", bottom);
	
	sendMessage(message);
}

/**
 * Sends "media_status" message to plugin loader shell ("loading", "playing", "paused", etc.)
 * 
 */
void MediaPluginBase::sendStatus()
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "media_status");

	message.setValue("status", statusString());
	
	sendMessage(message);
}


#if LL_WINDOWS
# define LLSYMEXPORT __declspec(dllexport)
#elif LL_LINUX
# define LLSYMEXPORT __attribute__ ((visibility("default")))
#else
# define LLSYMEXPORT /**/
#endif

