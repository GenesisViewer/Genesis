/** 
 * @file llpanelaudioprefs.cpp
 * @brief Audio preference implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

// file include
#include "llpanelaudioprefs.h"

// linden library includes
#include "llerror.h"
#include "llrect.h"
#include "llstring.h"
#include "llfontgl.h"

// project includes
#include "llaudioengine.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfirstuse.h"
#include "llnotify.h"
#include "llpanelaudiovolume.h"
#include "llparcel.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "llslider.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llui.h"
#include "llviewerparcelmgr.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"

#include "hippogridmanager.h"

//
// Static functions
//

//static
void* LLPanelAudioPrefs::createVolumePanel(void* data)
{
	LLPanelAudioVolume* panel = new LLPanelAudioVolume();
	return panel;
}

LLPanelAudioPrefs::LLPanelAudioPrefs()
{
	mFactoryMap["Volume Panel"]	= LLCallbackMap(createVolumePanel, NULL);
	
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_audio.xml", &getFactoryMap());
}

LLPanelAudioPrefs::~LLPanelAudioPrefs()
{
	// Children all cleaned up by default view destructor.
}

BOOL LLPanelAudioPrefs::postBuild()
{
	getChildView("filter_group")->setValue(S32(gSavedSettings.getU32("MediaFilterEnable")));
	refreshValues(); // initialize member data from saved settings
	childSetLabelArg("currency_change_threshold", "[CURRENCY]", gHippoGridManager->getConnectedGrid()->getCurrencySymbol());

	return TRUE;
}

void LLPanelAudioPrefs::refreshValues()
{
	mPreviousVolume = gSavedSettings.getF32("AudioLevelMaster");
	mPreviousSFX = gSavedSettings.getF32("AudioLevelSFX");
	mPreviousUI = gSavedSettings.getF32("AudioLevelUI");
	mPreviousEnvironment = gSavedSettings.getF32("AudioLevelAmbient");
	mPreviousMusicVolume = gSavedSettings.getF32("AudioLevelMusic");
	mPreviousMediaVolume = gSavedSettings.getF32("AudioLevelMedia");
	mPreviousDoppler = gSavedSettings.getF32("AudioLevelDoppler");
	mPreviousRolloff = gSavedSettings.getF32("AudioLevelRolloff");
	mPreviousUnderwaterRolloff = gSavedSettings.getF32("AudioLevelUnderwaterRolloff");

	mPreviousMoneyThreshold = gSavedSettings.getF32("UISndMoneyChangeThreshold");
	mPreviousHealthThreshold = gSavedSettings.getF32("UISndHealthReductionThreshold");

	mPreviousStreamingMusic = gSavedSettings.getBOOL("AudioStreamingMusic");
	mPreviousStreamingVideo = gSavedSettings.getBOOL("AudioStreamingMedia");

	mPreviousMuteAudio = gSavedSettings.getBOOL("MuteAudio");
	mPreviousMuteWhenMinimized = gSavedSettings.getBOOL("MuteWhenMinimized");
	gSavedSettings.setU32("MediaFilterEnable", getChildView("filter_group")->getValue().asInteger());
}

void LLPanelAudioPrefs::cancel()
{
	gSavedSettings.setF32("AudioLevelMaster", mPreviousVolume );
	gSavedSettings.setF32("AudioLevelUI", mPreviousUI );
	gSavedSettings.setF32("AudioLevelSFX", mPreviousSFX );
	gSavedSettings.setF32("AudioLevelAmbient", mPreviousEnvironment );
	gSavedSettings.setF32("AudioLevelMusic", mPreviousMusicVolume);
	gSavedSettings.setF32("AudioLevelMedia", mPreviousMediaVolume);
	gSavedSettings.setF32("AudioLevelDoppler", mPreviousDoppler );
	gSavedSettings.setF32("AudioLevelRolloff", mPreviousRolloff );
	gSavedSettings.setF32("AudioLevelUnderwaterRolloff", mPreviousUnderwaterRolloff );

	gSavedSettings.setF32("UISndMoneyChangeThreshold", mPreviousMoneyThreshold );
	gSavedSettings.setF32("UISndHealthReductionThreshold", mPreviousHealthThreshold );

	gSavedSettings.setBOOL("AudioStreamingMusic", mPreviousStreamingMusic );
	gSavedSettings.setBOOL("AudioStreamingMedia", mPreviousStreamingVideo );

	
	gSavedSettings.setBOOL("MuteAudio", mPreviousMuteAudio );
	gSavedSettings.setBOOL("MuteWhenMinimized", mPreviousMuteWhenMinimized );
}
