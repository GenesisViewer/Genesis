/**
* @file llnotificationtemplate.h
* @brief Description of notification contents
* @author Q (with assistance from Richard and Coco)
*
* $LicenseInfo:firstyear=2008&license=viewerlgpl$
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

#ifndef LL_LLNOTIFICATION_TEMPLATE_H
#define LL_LLNOTIFICATION_TEMPLATE_H

#include "llnotifications.h"


typedef boost::shared_ptr<LLNotificationForm> LLNotificationFormPtr;

// This is the class of object read from the XML file (notifications.xml, 
// from the appropriate local language directory).
struct LLNotificationTemplate
{
	LLNotificationTemplate();
    // the name of the notification -- the key used to identify it
    // Ideally, the key should follow variable naming rules 
    // (no spaces or punctuation).
    std::string mName;
    // The type of the notification
    // used to control which queue it's stored in
    std::string mType;
    // The text used to display the notification. Replaceable parameters
    // are enclosed in square brackets like this [].
    std::string mMessage;
	// The label for the notification; used for 
	// certain classes of notification (those with a window and a window title). 
	// Also used when a notification pops up underneath the current one.
	// Replaceable parameters can be used in the label.
	std::string mLabel;
	// The name of the icon image. This should include an extension.
	std::string mIcon;
    // This is the Highlander bit -- "There Can Be Only One"
    // An outstanding notification with this bit set
    // is updated by an incoming notification with the same name,
    // rather than creating a new entry in the queue.
    // (used for things like progress indications, or repeating warnings
    // like "the grid is going down in N minutes")
    bool mUnique;
    // if we want to be unique only if a certain part of the payload is constant
    // specify the field names for the payload. The notification will only be
    // combined if all of the fields named in the context are identical in the
    // new and the old notification; otherwise, the notification will be
    // duplicated. This is to support suppressing duplicate offers from the same
    // sender but still differentiating different offers. Example: Invitation to
    // conference chat.
    std::vector<std::string> mUniqueContext;
    // If this notification expires automatically, this value will be 
    // nonzero, and indicates the number of seconds for which the notification
    // will be valid (a teleport offer, for example, might be valid for 
    // 300 seconds). 
    U32 mExpireSeconds;
    // if the offer expires, one of the options is chosen automatically
    // based on its "value" parameter. This controls which one. 
    // If expireSeconds is specified, expireOption should also be specified.
    U32 mExpireOption;
    // if the notification contains a url, it's stored here (and replaced 
    // into the message where [_URL] is found)
    std::string mURL;
    // if there's a URL in the message, this controls which option visits
    // that URL. Obsolete this and eliminate the buttons for affected
    // messages when we allow clickable URLs in the UI
    U32 mURLOption;
	// does this notification persist across sessions? if so, it will be
	// serialized to disk on first receipt and read on startup
	bool mPersist;
	// This is the name of the default functor, if present, to be
	// used for the notification's callback. It is optional, and used only if 
	// the notification is constructed without an identified functor.
	std::string mDefaultFunctor;
	// The form data associated with a given notification (buttons, text boxes, etc)
    LLNotificationFormPtr mForm;
	// default priority for notifications of this type
	ENotificationPriority mPriority;
	// UUID of the audio file to be played when this notification arrives
	// this is loaded as a name, but looked up to get the UUID upon template load.
	// If null, it wasn't specified.
	LLUUID mSoundEffect;
};


#endif //LL_LLNOTIFICATION_TEMPLATE_H

