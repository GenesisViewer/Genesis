/**
 * @file llpanellandaudio.cpp
 * @brief Allows configuration of "media" for a land parcel,
 *   for example movies, web pages, and audio.
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

#include "llpanellandaudio.h"

// viewer includes
#include "llmimetypes.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "lluictrlfactory.h"

// library includes
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterurlentry.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "lltextbox.h"
#include "llradiogroup.h"
#include "llspinctrl.h"
#include "llsdutil.h"
#include "lltexturectrl.h"
#include "roles_constants.h"
#include "llscrolllistctrl.h"

// Values for the parcel voice settings radio group
enum
{
	kRadioVoiceChatEstate = 0,
	kRadioVoiceChatPrivate = 1,
	kRadioVoiceChatDisable = 2
};

//---------------------------------------------------------------------------
// LLPanelLandAudio
//---------------------------------------------------------------------------

LLPanelLandAudio::LLPanelLandAudio(LLParcelSelectionHandle& parcel)
:	LLPanel(),
	mParcel(parcel),
	mCheckSoundLocal(NULL),
	mSoundHelpButton(NULL),
	mRadioVoiceChat(NULL),
	mMusicURLEdit(NULL),
	mCheckAVSoundAny(NULL),
	mCheckAVSoundGroup(NULL)
{
}


// virtual
LLPanelLandAudio::~LLPanelLandAudio()
{
}


BOOL LLPanelLandAudio::postBuild()
{
	mCheckSoundLocal = getChild<LLCheckBoxCtrl>("check_sound_local");
	childSetCommitCallback("check_sound_local", onCommitAny, this);

	mSoundHelpButton = getChild<LLButton>("?");
	mSoundHelpButton->setClickedCallback(onClickSoundHelp, this);
	
	mRadioVoiceChat = getChild<LLRadioGroup>("parcel_voice_channel");
	childSetCommitCallback("parcel_voice_channel", onCommitAny, this);

	mMusicURLEdit = getChild<LLLineEditor>("music_url");
	childSetCommitCallback("music_url", onCommitAny, this);

	mCheckAVSoundAny = getChild<LLCheckBoxCtrl>("all av sound check");
	childSetCommitCallback("all av sound check", onCommitAny, this);

	mCheckAVSoundGroup = getChild<LLCheckBoxCtrl>("group av sound check");
	childSetCommitCallback("group av sound check", onCommitAny, this);

	return TRUE;
}


// public
void LLPanelLandAudio::refresh()
{
	LLParcel *parcel = mParcel->getParcel();

	if (!parcel)
	{
		clearCtrls();
	}
	else
	{
		// something selected, hooray!

		// Display options
		BOOL can_change_media = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_CHANGE_MEDIA);

		mMusicURLEdit->setText(parcel->getMusicURL());
		mMusicURLEdit->setEnabled( can_change_media );

		mCheckSoundLocal->set( parcel->getSoundLocal() );
		mCheckSoundLocal->setEnabled( can_change_media );

		if(parcel->getParcelFlagAllowVoice())
		{
			if(parcel->getParcelFlagUseEstateVoiceChannel())
				mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatEstate);
			else
				mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatPrivate);
		}
		else
		{
			mRadioVoiceChat->setSelectedIndex(kRadioVoiceChatDisable);
		}

		LLViewerRegion* region = LLViewerParcelMgr::getInstance()->getSelectionRegion();
		mRadioVoiceChat->setEnabled( region && region->isVoiceEnabled() && can_change_media );
	
		BOOL can_change_av_sounds = LLViewerParcelMgr::isParcelModifiableByAgent(parcel, GP_LAND_OPTIONS) && parcel->getHaveNewParcelLimitData();
		mCheckAVSoundAny->set(parcel->getAllowAnyAVSounds());
		mCheckAVSoundAny->setEnabled(can_change_av_sounds);

		mCheckAVSoundGroup->set(parcel->getAllowGroupAVSounds() || parcel->getAllowAnyAVSounds());	// On if "Everyone" is on
		mCheckAVSoundGroup->setEnabled(can_change_av_sounds && !parcel->getAllowAnyAVSounds());		// Enabled if "Everyone" is off
	}
}
// static
void LLPanelLandAudio::onCommitAny(LLUICtrl*, void *userdata)
{
	LLPanelLandAudio *self = (LLPanelLandAudio *)userdata;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel)
	{
		return;
	}

	// Extract data from UI
	BOOL sound_local		= self->mCheckSoundLocal->get();
	int voice_setting		= self->mRadioVoiceChat->getSelectedIndex();
	std::string music_url	= self->mMusicURLEdit->getText();

	BOOL voice_enabled;
	BOOL voice_estate_chan;

	switch(voice_setting)
	{
		default:
		case kRadioVoiceChatEstate:
			voice_enabled = TRUE;
			voice_estate_chan = TRUE;
		break;
		case kRadioVoiceChatPrivate:
			voice_enabled = TRUE;
			voice_estate_chan = FALSE;
		break;
		case kRadioVoiceChatDisable:
			voice_enabled = FALSE;
			voice_estate_chan = FALSE;
		break;
	}

	BOOL any_av_sound		= self->mCheckAVSoundAny->get();
	BOOL group_av_sound		= TRUE;		// If set to "Everyone" then group is checked as well
	if (!any_av_sound)
	{	// If "Everyone" is off, use the value from the checkbox
		group_av_sound = self->mCheckAVSoundGroup->get();
	}

	// Remove leading/trailing whitespace (common when copying/pasting)
	LLStringUtil::trim(music_url);

	// Push data into current parcel
	parcel->setParcelFlag(PF_ALLOW_VOICE_CHAT, voice_enabled);
	parcel->setParcelFlag(PF_USE_ESTATE_VOICE_CHAN, voice_estate_chan);
	parcel->setParcelFlag(PF_SOUND_LOCAL, sound_local);
	parcel->setMusicURL(music_url);
	parcel->setAllowAnyAVSounds(any_av_sound);
	parcel->setAllowGroupAVSounds(group_av_sound);

	// Send current parcel data upstream to server
	LLViewerParcelMgr::getInstance()->sendParcelPropertiesUpdate( parcel );

	// Might have changed properties, so let's redraw!
	self->refresh();
}


// static 
void LLPanelLandAudio::onClickSoundHelp(void*)
{ 
	LLNotificationsUtil::add("ClickSoundHelpLand");
} 
