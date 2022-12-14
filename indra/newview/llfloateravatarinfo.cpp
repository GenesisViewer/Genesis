/** 
 * @file llfloateravatarinfo.cpp
 * @brief LLFloaterAvatarInfo class implementation
 * Avatar information as shown in a floating window from right-click
 * Profile.  Used for editing your own avatar info.  Just a wrapper
 * for LLPanelAvatar, shared with the Find directory.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llfloateravatarinfo.h"
#include "llpanelavatar.h"
#include "lluictrlfactory.h"
LLFloaterAvatarInfo::floater_positions_t LLFloaterAvatarInfo::floater_positions;
LLUUID LLFloaterAvatarInfo::lastMoved;
void*	LLFloaterAvatarInfo::createPanelAvatar(void*	data)
{
	LLFloaterAvatarInfo* self = (LLFloaterAvatarInfo*)data;
	self->mPanelAvatarp = new LLPanelAvatar("PanelAv", LLRect(), TRUE); // allow edit self
	self->mPanelAvatarp->setAvatarID(self->mAvatarID);
	return self->mPanelAvatarp;
}

LLFloaterAvatarInfo::LLFloaterAvatarInfo(const std::string& name, const LLUUID &avatar_id)
:	LLFloater(name), LLInstanceTracker<LLFloaterAvatarInfo, LLUUID>(avatar_id),
	mAvatarID(avatar_id)
{
	setAutoFocus(true);

	LLCallbackMap::map_t factory_map;
	factory_map["Panel Avatar"] = LLCallbackMap(createPanelAvatar, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_profile.xml", &factory_map);
	setTitle(name);
}
void LLFloaterAvatarInfo::handleReshape(const LLRect& new_rect, bool by_user) {

	if (by_user && !isMinimized()) {
		LLFloaterAvatarInfo::floater_positions[this->mAvatarID]=new_rect;
		LLFloaterAvatarInfo::lastMoved=this->mAvatarID;
		gSavedSettings.setRect("FloaterProfileRect",new_rect);
	}
	LLFloater::handleReshape(new_rect, by_user);
}
void LLFloaterAvatarInfo::setOpenedPosition() {
	if (LLFloaterAvatarInfo::lastMoved.isNull()) {
		LLRect lastfloaterProfilePosition = gSavedSettings.getRect("FloaterProfileRect");
		if (lastfloaterProfilePosition.mLeft==0 && lastfloaterProfilePosition.mTop==0) {
			this->center();
		}
		else {
			this->setRect(lastfloaterProfilePosition);
		}
		LLFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
		LLFloaterAvatarInfo::lastMoved=this->mAvatarID;
	} else {
		
		//LLRect lastFloaterPosition = LLFloaterAvatarInfo::floater_positions[this->mAvatarID.asString()];
		floater_positions_t::iterator it = floater_positions.find(this->mAvatarID);
		if(it == floater_positions.end())
		{
			
			floater_positions_t::iterator lasFloaterMovedPosition = floater_positions.find(LLFloaterAvatarInfo::lastMoved);
			this->setRect(lasFloaterMovedPosition->second);
			LLFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
			LLFloaterAvatarInfo::lastMoved=this->mAvatarID;
			
		} else {
			
			this->setRect(it->second);
		}
		
	}

	BOOL overlapse=TRUE;
	while (overlapse) {
		//if I am on an existing floater
		overlapse=FALSE;
		for(floater_positions_t::iterator it = floater_positions.begin(); it != floater_positions.end();++it )
		{
			if((*it).second == getRect() && (*it).first != this->mAvatarID)
			{
				
				this->translate(20,-20);
				LLFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
				LLFloaterAvatarInfo::lastMoved=this->mAvatarID;
				
				overlapse=TRUE;
			}
			
			
			
		}
		
	}
	
}

// virtual
LLFloaterAvatarInfo::~LLFloaterAvatarInfo()
{
}

void LLFloaterAvatarInfo::resetGroupList()
{
	mPanelAvatarp->resetGroupList();
}

// virtual
BOOL LLFloaterAvatarInfo::canClose()
{
	return mPanelAvatarp && mPanelAvatarp->canClose();
}
