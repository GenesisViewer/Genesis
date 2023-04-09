/** 
 * @file llfloaterpreference.cpp
 * @brief Global preferences with and without persistence.
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

/*
 * App-wide preferences.  Note that these are not per-user,
 * because we need to load many preferences before we have
 * a login name.
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterpreference.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llfocusmgr.h"
#include "llscrollbar.h"
#include "llspinctrl.h"
#include "message.h"

#include "llcommandhandler.h"
#include "llfloaterabout.h"
#include "llfloaterpreference.h"
#include "llpanelnetwork.h"
#include "llpanelaudioprefs.h"
#include "llpaneldisplay.h"
#include "llpanelgeneral.h"
#include "llpanelinput.h"
#include "llpanellogin.h"
#include "llpanelmsgs.h"
#include "llpanelweb.h"
#include "llpanelskins.h"
#include "hippopanelgrids.h"
#include "llprefschat.h"
#include "llprefsvoice.h"
#include "llprefsim.h"
#include "ascentprefschat.h"
#include "ascentprefssys.h"
#include "ascentprefsvan.h"
#include "llresizehandle.h"
#include "llresmgr.h"
#include "llassetstorage.h"
#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llkeyboard.h"
#include "llscrollcontainer.h"
#include "hippopanelgrids.h"

const S32 PREF_BORDER = 4;
const S32 PREF_PAD = 5;
const S32 PREF_BUTTON_WIDTH = 70;
const S32 PREF_CATEGORY_WIDTH = 150;

const S32 PREF_FLOATER_MIN_HEIGHT = 2 * SCROLLBAR_SIZE + 2 * LLPANEL_BORDER_WIDTH + 96;

LLFloaterPreference* LLFloaterPreference::sInstance = NULL;


class LLPreferencesHandler : public LLCommandHandler
{
public:
	// requires trusted browser
	LLPreferencesHandler() : LLCommandHandler("preferences", UNTRUSTED_BLOCK) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		LLFloaterPreference::show(NULL);
		return true;
	}
};

LLPreferencesHandler gPreferencesHandler;


// Must be done at run time, not compile time. JC
S32 pref_min_width()
{
	return  
	2 * PREF_BORDER + 
	2 * PREF_BUTTON_WIDTH + 
	PREF_PAD + RESIZE_HANDLE_WIDTH +
	PREF_CATEGORY_WIDTH +
	PREF_PAD;
}

S32 pref_min_height()
{
	return
	2 * PREF_BORDER +
	3*(BTN_HEIGHT + PREF_PAD) +
	PREF_FLOATER_MIN_HEIGHT;
}


LLPreferenceCore::LLPreferenceCore(LLTabContainer* tab_container, LLButton * default_btn) :
	mTabContainer(tab_container),
	mGeneralPanel(NULL),
	mInputPanel(NULL),
	mNetworkPanel(NULL),
	mDisplayPanel(NULL),
	mAudioPanel(NULL),
	mMsgPanel(NULL),
	mSkinsPanel(NULL),
	mGridsPanel(NULL),
	mPrefsAscentChat(NULL),
	mPrefsAscentSys(NULL),
	mPrefsAscentVan(NULL)
{
	mGeneralPanel = new LLPanelGeneral();
	mTabContainer->addTabPanel(mGeneralPanel, mGeneralPanel->getLabel());
	mGeneralPanel->setDefaultBtn(default_btn);

	mInputPanel = new LLPanelInput();
	mTabContainer->addTabPanel(mInputPanel, mInputPanel->getLabel());
	mInputPanel->setDefaultBtn(default_btn);

	mNetworkPanel = new LLPanelNetwork();
	mTabContainer->addTabPanel(mNetworkPanel, mNetworkPanel->getLabel());
	mNetworkPanel->setDefaultBtn(default_btn);

	mWebPanel = new LLPanelWeb();
	mTabContainer->addTabPanel(mWebPanel, mWebPanel->getLabel());
	mWebPanel->setDefaultBtn(default_btn);

	mDisplayPanel = new LLPanelDisplay();
	mTabContainer->addTabPanel(mDisplayPanel, mDisplayPanel->getLabel());
	mDisplayPanel->setDefaultBtn(default_btn);

	mAudioPanel = new LLPanelAudioPrefs();
	mTabContainer->addTabPanel(mAudioPanel, mAudioPanel->getLabel());
	mAudioPanel->setDefaultBtn(default_btn);

	mPrefsChat = new LLPrefsChat();
	mTabContainer->addTabPanel(mPrefsChat->getPanel(), mPrefsChat->getPanel()->getLabel());
	mPrefsChat->getPanel()->setDefaultBtn(default_btn);

	mPrefsVoice = new LLPrefsVoice();
	mTabContainer->addTabPanel(mPrefsVoice, mPrefsVoice->getLabel());
	mPrefsVoice->setDefaultBtn(default_btn);

	mPrefsIM = new LLPrefsIM();
	mTabContainer->addTabPanel(mPrefsIM->getPanel(), mPrefsIM->getPanel()->getLabel());
	mPrefsIM->getPanel()->setDefaultBtn(default_btn);

	mMsgPanel = new LLPanelMsgs();
	mTabContainer->addTabPanel(mMsgPanel, mMsgPanel->getLabel());
	mMsgPanel->setDefaultBtn(default_btn);
	
	mSkinsPanel = new LLPanelSkins();
	mTabContainer->addTabPanel(mSkinsPanel, mSkinsPanel->getLabel());
	mSkinsPanel->setDefaultBtn(default_btn);

	mGridsPanel = HippoPanelGrids::create();
	mTabContainer->addTabPanel(mGridsPanel, mGridsPanel->getLabel());
	mGridsPanel->setDefaultBtn(default_btn);

	mPrefsAscentChat = new LLPrefsAscentChat();
	mTabContainer->addTabPanel(mPrefsAscentChat, mPrefsAscentChat->getLabel());
	mPrefsAscentChat->setDefaultBtn(default_btn);

	mPrefsAscentSys = new LLPrefsAscentSys();
	mTabContainer->addTabPanel(mPrefsAscentSys, mPrefsAscentSys->getLabel());
	mPrefsAscentSys->setDefaultBtn(default_btn);

	mPrefsAscentVan = new LLPrefsAscentVan();
	mTabContainer->addTabPanel(mPrefsAscentVan, mPrefsAscentVan->getLabel());
	mPrefsAscentVan->setDefaultBtn(default_btn);

	mTabContainer->setCommitCallback(boost::bind(LLPreferenceCore::onTabChanged,_1));

	if (!mTabContainer->selectTab(gSavedSettings.getS32("LastPrefTab")))
	{
		mTabContainer->selectFirstTab();
	}
}

LLPreferenceCore::~LLPreferenceCore()
{
	if (mGeneralPanel)
	{
		delete mGeneralPanel;
		mGeneralPanel = NULL;
	}
	if (mInputPanel)
	{
		delete mInputPanel;
		mInputPanel = NULL;
	}
	if (mNetworkPanel)
	{
		delete mNetworkPanel;
		mNetworkPanel = NULL;
	}
	if (mDisplayPanel)
	{
		delete mDisplayPanel;
		mDisplayPanel = NULL;
	}

	if (mAudioPanel)
	{
		delete mAudioPanel;
		mAudioPanel = NULL;
	}
	if (mPrefsChat)
	{
		delete mPrefsChat;
		mPrefsChat = NULL;
	}
	if (mPrefsIM)
	{
		delete mPrefsIM;
		mPrefsIM = NULL;
	}
	if (mMsgPanel)
	{
		delete mMsgPanel;
		mMsgPanel = NULL;
	}
	if (mWebPanel)
	{
		delete mWebPanel;
		mWebPanel = NULL;
	}
	if (mSkinsPanel)
	{
		delete mSkinsPanel;
		mSkinsPanel = NULL;
	}
	if (mGridsPanel)
	{
		delete mGridsPanel;
		mGridsPanel = NULL;
	}
	if (mPrefsAscentChat)
	{
		delete mPrefsAscentChat;
		mPrefsAscentChat = NULL;
	}
	if (mPrefsAscentSys)
	{
		delete mPrefsAscentSys;
		mPrefsAscentSys = NULL;
	}
	if (mPrefsAscentVan)
	{
		delete mPrefsAscentVan;
		mPrefsAscentVan = NULL;
	}
}


void LLPreferenceCore::apply()
{
	mGeneralPanel->apply();
	mInputPanel->apply();
	mNetworkPanel->apply();
	mDisplayPanel->apply();
	mAudioPanel->apply();
	mPrefsChat->apply();
	mPrefsVoice->apply();
	mPrefsIM->apply();
	mMsgPanel->apply();
	mSkinsPanel->apply();
	mGridsPanel->apply();
	mPrefsAscentChat->apply();
	mPrefsAscentSys->apply();
	mPrefsAscentVan->apply();

	mWebPanel->apply();
}


void LLPreferenceCore::cancel()
{
	mGeneralPanel->cancel();
	mInputPanel->cancel();
	mNetworkPanel->cancel();
	mDisplayPanel->cancel();
	mAudioPanel->cancel();
	mPrefsChat->cancel();
	mPrefsVoice->cancel();
	mPrefsIM->cancel();
	mMsgPanel->cancel();
	mSkinsPanel->cancel();
	mGridsPanel->cancel();
	mPrefsAscentChat->cancel();
	mPrefsAscentSys->cancel();
	mPrefsAscentVan->cancel();

	mWebPanel->cancel();
}

// static
void LLPreferenceCore::onTabChanged(LLUICtrl* ctrl)
{
	LLTabContainer* self = (LLTabContainer*)ctrl;

	gSavedSettings.setS32("LastPrefTab", self->getCurrentPanelIndex());
}


void LLPreferenceCore::setPersonalInfo(const std::string& visibility, bool im_via_email, const std::string& email, bool is_verified)
{
	mPrefsIM->setPersonalInfo(visibility, im_via_email, email, is_verified);
}

//////////////////////////////////////////////
// LLFloaterPreference

void reset_to_default(const std::string& control)
{
	LLUI::getControlControlGroup(control).getControl(control)->resetToDefault(true);
}

LLFloaterPreference::LLFloaterPreference()
{
	mExitWithoutSaving = false;
	mCommitCallbackRegistrar.add("Prefs.Reset", boost::bind(reset_to_default, _2));
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_preferences.xml");
}

BOOL LLFloaterPreference::postBuild()
{
	requires<LLButton>("About...");
	requires<LLButton>("OK");
	requires<LLButton>("Cancel");
	requires<LLButton>("Apply");
	requires<LLTabContainer>("pref core");

	if (!checkRequirements())
	{
		return FALSE;
	}

	getChild<LLUICtrl>("About...")->setCommitCallback(boost::bind(LLFloaterAbout::show, (void*)0));
	getChild<LLUICtrl>("Apply")->setCommitCallback(boost::bind(&LLFloaterPreference::onBtnApply, this));
	getChild<LLUICtrl>("Cancel")->setCommitCallback(boost::bind(&LLFloaterPreference::onBtnCancel, this));
	getChild<LLUICtrl>("OK")->setCommitCallback(boost::bind(&LLFloaterPreference::onBtnOK, this));
	getChild<LLLineEditor>("search_text")->setKeystrokeCallback(boost::bind(&LLFloaterPreference::onsearch,this,_1));
	mPreferenceCore = new LLPreferenceCore(getChild<LLTabContainer>("pref core"), getChild<LLButton>("OK"));

	sInstance = this;

	return TRUE;
}


LLFloaterPreference::~LLFloaterPreference()
{
	sInstance = NULL;
	delete mPreferenceCore;
}
void LLFloaterPreference::onsearch(LLLineEditor *searchCtrl)
{
	std::string search = searchCtrl->getText();
	if (search.empty()) {
		clearSearch();
	} else {
		clearSearch();
		doSearch(search);
	}
}
void LLFloaterPreference::clearSearch()
{
	int nbTabPanels = mPreferenceCore->getTabContainer()->getTabCount();
	for (int i = nbTabPanels-1; i>=0;i--) {
		LLPanel * tabPanel = mPreferenceCore->getTabContainer()->getPanelByIndex(i);
		mPreferenceCore->getTabContainer()->enableTabButton(i,true);
		
		clearSearch(tabPanel);

	}
}
void LLFloaterPreference::clearSearch(LLPanel * tabPanel)
{
	LLView::child_list_const_iter_t	end(tabPanel->endChild());
	for (LLView::child_list_const_iter_t i = tabPanel->beginChild(); i != end; ++i)
	{
		//what is the type of this child?
		LLPanel *pPanel = dynamic_cast< LLPanel *>( (*i));
		LLTabContainer *pTabContainer = dynamic_cast< LLTabContainer *>( (*i) );
		LLUICtrl *pCtrl = dynamic_cast< LLUICtrl *>( (*i) );

		if (pTabContainer) {
			int nbTabPanels = pTabContainer->getTabCount();
			for (int i = nbTabPanels-1; i>=0;i--) {
				LLPanel * nextTabPanel = pTabContainer->getPanelByIndex(i);
				clearSearch(nextTabPanel);
				pTabContainer->enableTabButton(i,true);
			}
		}
		if (pCtrl) {
			clearSearch(pCtrl);
		}
	}
}
void LLFloaterPreference::clearSearch(LLUICtrl * ctrl) {
	ctrl->setHighlighted(false);
	LLView::child_list_const_iter_t	end(ctrl->endChild());
	for (LLView::child_list_const_iter_t i = ctrl->beginChild(); i != end; ++i)
	{
		LLUICtrl *nextCtrl = dynamic_cast< LLUICtrl *>( (*i) );
		if (nextCtrl)
			clearSearch(nextCtrl);
	}
}
void LLFloaterPreference::doSearch(std::string search)
{
	int nbTabPanels = mPreferenceCore->getTabContainer()->getTabCount();
	
	for (int i = nbTabPanels-1; i>=0;i--) {
		LLPanel * tabPanel = mPreferenceCore->getTabContainer()->getPanelByIndex(i);
		
		bool visible = doSearch(search,tabPanel);
		//tabPanel->setVisible(visible);
		mPreferenceCore->getTabContainer()->enableTabButton(i,visible);
	}
}
bool LLFloaterPreference::doSearch(std::string search,LLPanel * tabPanel)
{
	bool visible = false;
	LLView::child_list_const_iter_t	end(tabPanel->endChild());
	for (LLView::child_list_const_iter_t i = tabPanel->beginChild(); i != end; ++i)
	{
		//what is the type of this child?
		LLPanel *pPanel = dynamic_cast< LLPanel *>( (*i));
		LLTabContainer *pTabContainer = dynamic_cast< LLTabContainer *>( (*i) );
		LLUICtrl *pCtrl = dynamic_cast< LLUICtrl *>( (*i) );
		if (pPanel) {
			visible = doSearch(search,pPanel) || visible;
		}
		if (pTabContainer) {
			bool tabVisible = false;
			int nbTabPanels = pTabContainer->getTabCount();
			for (int j = nbTabPanels-1; j>=0;j--) {
				LLPanel * nextTabPanel = pTabContainer->getPanelByIndex(j);
				tabVisible =  doSearch(search,nextTabPanel);
				pTabContainer->enableTabButton(j,tabVisible);
			}
			visible = tabVisible || visible;
			
		}
		if (pCtrl) {
			visible = doSearch(search,tabPanel,pCtrl) || visible;
		}
		
	}
	return visible;
}
bool LLFloaterPreference::doSearch(std::string search,LLPanel * parent, LLUICtrl* ctrl)
{
	bool visible = false;
	if (ctrl->isHighlighted()) {
		return true;
	}
	
	LLWString value = utf8str_to_wstring(ctrl->getControlName());
	
	LLWString searchString =  utf8str_to_wstring(search);
	LLWStringUtil::toLower( value );
	LLWStringUtil::toLower( searchString );
	if (value.find(searchString) != std::string::npos) {
		visible=true;
	} 
	
	value = utf8str_to_wstring(ctrl->getToolTip());
	LLWStringUtil::toLower( value );
	if (value.find(searchString) != std::string::npos) {
		visible=true;
	} 
	LLTextBox *pText = dynamic_cast< LLTextBox *>( (ctrl) );
	if (pText) {
		value = utf8str_to_wstring(pText->getText());
		LLWStringUtil::toLower( value );
		if (value.find(searchString) != std::string::npos) {
			visible=true;
			pText->setHighlighted(true);
		} 
	}
	LLCheckBoxCtrl *pCheckbox = dynamic_cast< LLCheckBoxCtrl *>( (ctrl) );
	if (pCheckbox) {
		
		value = utf8str_to_wstring(pCheckbox->getLabel());
		LLWStringUtil::toLower( value );
		if (value.find(searchString) != std::string::npos) {
			visible=true;
			pCheckbox->setHighlighted(true);
			
		} 
	}
	LLComboBox* pCombo = dynamic_cast<LLComboBox*>(ctrl);
	if (pCombo) {
		std::vector<LLScrollListItem*> data_list = pCombo->getAllData();
		
		
		for (std::vector<LLScrollListItem*>::iterator itr = data_list.begin(); itr != data_list.end(); ++itr)
	 	{
			LLScrollListItem* item = *itr;
			value = utf8str_to_wstring(item->getColumn(0)->getValue().asString());
			LLWStringUtil::toLower( value );
			if (value.find(searchString) != std::string::npos) {
				visible=true;
				break;
			}
		}
		 
	}
	LLView::child_list_const_iter_t	end(ctrl->endChild());
	for (LLView::child_list_const_iter_t i = ctrl->beginChild(); i != end; ++i)
	{
		LLUICtrl *nextCtrl = dynamic_cast< LLUICtrl *>( (*i) );
		if (nextCtrl)
			visible = doSearch(search,parent,nextCtrl) || visible;
	}
	if (ctrl->hasHighLightControlID()) {
		
		//ok, this ctrl must be hightlighted but we have chosen to highlight another one in this case
		LLUICtrl * otherCtrl = parent->getChild<LLUICtrl>(ctrl->getHighLightControlID());
		otherCtrl->setHighlighted(otherCtrl->isHighlighted() || visible);
	} else {
		ctrl->setHighlighted(visible);
	}
	return  visible;
}
void LLFloaterPreference::apply()
{
	mPreferenceCore->apply();
}


void LLFloaterPreference::cancel()
{
	mPreferenceCore->cancel();
}


// static
void LLFloaterPreference::show(void*)
{
	if (!sInstance)
	{
		new LLFloaterPreference();
		sInstance->center();
	}

	sInstance->open();		/* Flawfinder: ignore */

	gAgent.sendAgentUserInfoRequest();

	LLPanelLogin::setAlwaysRefresh(true);
}



void LLFloaterPreference::onBtnOK()
{
	// commit any outstanding text entry
	if (hasFocus())
	{
		LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
		if (cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}

	if (canClose())
	{
		apply();
		close(false);

		gSavedSettings.saveToFile( gSavedSettings.getString("ClientSettingsFile"), TRUE );
		if (gAgentID.notNull())
			gSavedPerAccountSettings.saveToFile( gSavedSettings.getString("PerAccountSettingsFile"), TRUE );
	}
	else
	{
		// Show beep, pop up dialog, etc.
		LL_INFOS() << "Can't close preferences!" << LL_ENDL;
	}

	LLPanelLogin::updateLocationSelectorsVisibility();
}

void LLFloaterPreference::onBtnApply()
{
	if (hasFocus())
	{
		LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}
	apply();

	LLPanelLogin::updateLocationSelectorsVisibility();
}


void LLFloaterPreference::onClose(bool app_quitting)
{
	LLPanelLogin::setAlwaysRefresh(false);
	if (!mExitWithoutSaving)
	{
		cancel(); // will be a no-op if OK or apply was performed just prior.
	}
	LLFloater::onClose(app_quitting);
}


// static 
void LLFloaterPreference::onBtnCancel()
{
	if (hasFocus())
	{
		LLUICtrl* cur_focus = dynamic_cast<LLUICtrl*>(gFocusMgr.getKeyboardFocus());
		if (cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}
	close(); // side effect will also cancel any unsaved changes.
}


// static
void LLFloaterPreference::updateUserInfo(const std::string& visibility, bool im_via_email, const std::string& email, bool is_verified)
{
	if (sInstance && sInstance->mPreferenceCore)
	{
		sInstance->mPreferenceCore->setPersonalInfo(visibility, im_via_email, email, is_verified);
	}
}

//static
void LLFloaterPreference::switchTab(S32 i)
{
	sInstance->mPreferenceCore->getTabContainer()->selectTab(i);
}

// static
void LLFloaterPreference::closeWithoutSaving()
{
	sInstance->mExitWithoutSaving = true;
	sInstance->close();
}
