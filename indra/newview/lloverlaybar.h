/** 
 * @file lloverlaybar.h
 * @brief LLOverlayBar class definition
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

#ifndef LL_LLOVERLAYBAR_H
#define LL_LLOVERLAYBAR_H

#include "llpanel.h"
#include "lllayoutstack.h"

// "Constants" loaded from settings.xml at start time
extern S32 STATUS_BAR_HEIGHT;

class LLButton;
class LLLineEditor;
class LLMediaRemoteCtrl;
class LLMessageSystem;
class LLTextBox;
class LLTextEditor;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLStatGraph;
class LLSlider;
class LLVoiceRemoteCtrl;
class wlfPanel_AdvSettings;
class AORemoteCtrl;
class LLChatBar;

class LLOverlayBar
:	public LLLayoutPanel
{
public:
	LLOverlayBar();
	~LLOverlayBar();

	/*virtual*/ void refresh();
	/*virtual*/ void reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	/*virtual*/ BOOL postBuild();

	LLButton* updateButtonVisiblity(LLButton* button, bool visible);

	void layoutButtons();

	// helpers for returning desired state
	BOOL musicPlaying() { return mMusicState == PLAYING; }

	LLView* getChatbarAndButtons() const {return mChatbarAndButtons;}
	
	static void onClickIMReceived(void* data);
	static void onClickSetNotBusy(void* data);
	static void onClickMouselook(void* data);
	static void onClickStandUp(void* data);
	static void onClickCancelTP(void* data);
	static void onClickCancelImport(void* data);
	static void onClickResetView(void* data);
 	static void onClickFlycam(void* data);

	static void audioFilterPlay();
	static void audioFilterStop();

	//static media helper functions
	static void toggleMediaPlay(void*);
	static void toggleMusicPlay(void*);
	static void musicPause(void*);
	static void musicStop(void*);
	static void mediaStop(void*);
	static void stopAnimations(void*);
	static void toggleAudioVolumeFloater(void*);

	void setCancelTPButtonVisible(BOOL b, const std::string& label);

protected:	
	static void* createMediaRemote(void* userdata);
	static void* createVoiceRemote(void* userdata);
	static void* createAdvSettings(void* userdata);
	static void* createAORemote(void* userdata);
	static void* createChatBar(void* userdata);

	void enableMediaButtons();

protected:
	friend class LLFloaterNearbyMedia; //Crappy workaround to access mMediaRemote
	LLMediaRemoteCtrl*	mMediaRemote;
	LLVoiceRemoteCtrl*	mVoiceRemote;
	LLButton*	mCancelBtn;
	AORemoteCtrl*			mAORemote;
	bool mBuilt;	// dialog constructed yet?
	enum { STOPPED=0, PLAYING=1, PAUSED=2 };
	S32 mMusicState;
	std::string			mOriginalIMLabel;
	std::string			mUnreadCountStringPlural;
	
	CachedUICtrl<LLView> mChatbarAndButtons;
	CachedUICtrl<LLButton> mNewIM;
	CachedUICtrl<LLButton> mNotBusy;
	CachedUICtrl<LLButton> mMouseLook;
	CachedUICtrl<LLButton> mStandUp;
	CachedUICtrl<LLButton> mFlyCam;
	CachedUICtrl<LLChatBar> mChatBar;
	CachedUICtrl<LLPanel> mVoiceRemoteContainer;
	CachedUICtrl<LLPanel> mStateManagementContainer;
	CachedUICtrl<LLPanel> mAORemoteContainer;
	CachedUICtrl<LLPanel> mAdvSettingsContainer;
	CachedUICtrl<LLPanel> mMediaRemoteContainer;
private:
	bool	updateChatVisible(const LLSD &data);
	bool	updateAORemoteVisible(const LLSD &data);
	bool	updateAdvSettingsPopup(const LLSD &data);
};

extern LLOverlayBar* gOverlayBar;

#endif
