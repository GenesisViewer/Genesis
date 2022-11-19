/** 
 * @file llfloateractivespeakers.cpp
 * @brief Management interface for muting and controlling volume of residents currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2005-2009, Linden Research, Inc.
 * 
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

#include "llfloateractivespeakers.h"

#include "llparticipantlist.h"
#include "llpanelvoiceeffect.h"
#include "llspeakers.h"
#include "lluictrlfactory.h"

namespace
{
	void* createEffectPanel(void*)
	{
		return new LLPanelVoiceEffect;
	}
}

LLFloaterActiveSpeakers::LLFloaterActiveSpeakers(const LLSD& seed) : mPanel(NULL)
{
	mFactoryMap["active_speakers_panel"] = LLCallbackMap(createSpeakersPanel, NULL);
	mFactoryMap["panel_voice_effect"] = LLCallbackMap(createEffectPanel, NULL);
	// do not automatically open singleton floaters (as result of getInstance())
	BOOL no_open = FALSE;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_active_speakers.xml", &getFactoryMap(), no_open);	
	//RN: for now, we poll voice client every frame to get voice amplitude feedback
	//LLVoiceClient::getInstance()->addObserver(this);
	
	
	mPanel->refreshSpeakers();
	mTitle = this->getTitle();
	//buttons to pause/unpause the refresh of active speakers
	LLButton*  buttonPause = this->getChild<LLButton>("active_speakers_pause");
	buttonPause->setCommitCallback(boost::bind(&LLFloaterActiveSpeakers::toggleActiveSpeakersRefresh, this));
	LLButton*  buttonUnPause = this->getChild<LLButton>("active_speakers_unpause");
	buttonUnPause->setCommitCallback(boost::bind(&LLFloaterActiveSpeakers::toggleActiveSpeakersRefresh, this));
	LLButton*  buttonManualRefresh = this->getChild<LLButton>("active_speakers_refresh");
	buttonManualRefresh->setCommitCallback(boost::bind(&LLFloaterActiveSpeakers::manualActiveSpeakersRefresh, this));
}

LLFloaterActiveSpeakers::~LLFloaterActiveSpeakers()
{
}

void LLFloaterActiveSpeakers::onOpen()
{
	
	gSavedSettings.setBOOL("ShowActiveSpeakers", TRUE);
}

void LLFloaterActiveSpeakers::onClose(bool app_quitting)
{
	
	if (!app_quitting)
	{
		this->setTitle(mTitle);
		gSavedSettings.setBOOL("ShowActiveSpeakers", FALSE);
	}
	setVisible(FALSE);
}

void LLFloaterActiveSpeakers::draw()
{
	// update state every frame to get live amplitude feedback
	if (!mPaused) {
		mPanel->refreshSpeakers();
		this->setTitle(mTitle);
	} else {
		this->setTitle(mTitle + " (Paused)");
	}
	LLFloater::draw();
}

BOOL LLFloaterActiveSpeakers::postBuild()
{
	mPanel = getChild<LLParticipantList>("active_speakers_panel");
	return TRUE;
}

void LLFloaterActiveSpeakers::onParticipantsChanged()
{
	//refresh();
}
void LLFloaterActiveSpeakers::manualActiveSpeakersRefresh()
{
	if (mPaused) {	
		mPanel->readWhilePausedEvents();
	}
}
void LLFloaterActiveSpeakers::toggleActiveSpeakersRefresh()
{
	mPaused = !mPaused;
	
	this->getChild<LLButton>("active_speakers_unpause")->setEnabled(mPaused);
	this->getChild<LLButton>("active_speakers_unpause")->setVisible(mPaused);
	this->getChild<LLButton>("active_speakers_pause")->setEnabled(!mPaused);
	this->getChild<LLButton>("active_speakers_pause")->setVisible(!mPaused);
	mPanel->toggleRefreshActiveSpeakers();
}

//static
void* LLFloaterActiveSpeakers::createSpeakersPanel(void* data)
{
	return new LLParticipantList(LLActiveSpeakerMgr::getInstance(), /*show_text_chatters=*/ false);
}

