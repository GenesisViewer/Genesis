/**
 * @file hippopanelgrids.cpp
 * @author Mana Janus
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2008, Linden Research, Inc.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "hippopanelgrids.h"

#include "hippogridmanager.h"
#include "hippolimits.h"

#include "llcombobox.h"
#include "llpanellogin.h"
#include "llstartup.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "llnotificationsutil.h"
#include "llhttpstatuscodes.h"

// ********************************************************************


class HippoPanelGridsImpl : public HippoPanelGrids
{
	public:
		HippoPanelGridsImpl();
		virtual ~HippoPanelGridsImpl();

		BOOL postBuild();
		void apply();
		void cancel();

	private:
		enum State { NORMAL, ADD_NEW, ADD_COPY };
		State mState;
		std::string mCurGrid;
		bool mIsEditable;

		void loadCurGrid();
		bool saveCurGrid();

		void refresh();
		void reset();
		void retrieveGridInfo();

		static void onSelectGrid(LLUICtrl *ctrl, void *data);
		static void onSelectPlatform(LLUICtrl *ctrl, void *data);
		static void onClickDelete(void *data);
		static void onClickAdd(void *data);
		static void onClickCopy(void *data);
		static void onClickDefault(void *data);
		static void onClickGridInfo(void *data);
		static void onClickHelpRenderCompat(void *data);
		static void onClickAdvanced(void *data);

		void enableEditing(bool);
};


// ********************************************************************


HippoPanelGrids::HippoPanelGrids()
{
}

HippoPanelGrids::~HippoPanelGrids()
{
}

// static
HippoPanelGrids *HippoPanelGrids::create()
{
	return new HippoPanelGridsImpl();
}


// ********************************************************************


HippoPanelGridsImpl::HippoPanelGridsImpl() :
	mState(NORMAL), mIsEditable(true)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_grids.xml");
}

HippoPanelGridsImpl::~HippoPanelGridsImpl()
{
}


// ********************************************************************
// panel interface

BOOL HippoPanelGridsImpl::postBuild()
{
	requires<LLComboBox>("grid_selector");
	requires<LLComboBox>("platform");
	requires<LLLineEditor>("gridname");
	requires<LLLineEditor>("loginuri");
	requires<LLLineEditor>("loginpage");
	requires<LLLineEditor>("helperuri");
	requires<LLLineEditor>("website");
	requires<LLLineEditor>("support");
	requires<LLLineEditor>("register");
	requires<LLLineEditor>("partner");
	requires<LLLineEditor>("password");
	requires<LLLineEditor>("search");
	requires<LLButton>("btn_delete");
	requires<LLButton>("btn_add");
	requires<LLButton>("btn_copy");
	requires<LLButton>("btn_default");
	requires<LLButton>("btn_gridinfo");
	requires<LLButton>("btn_help_render_compat");
	if (!checkRequirements()) return false;
	
	LLComboBox *platform = getChild<LLComboBox>("platform");
	platform->removeall();
	for (int p=HippoGridInfo::PLATFORM_OTHER; p<HippoGridInfo::PLATFORM_LAST; p++)
		platform->add(HippoGridInfo::getPlatformString(static_cast<HippoGridInfo::Platform>(p)));
	
	childSetAction("btn_delete", onClickDelete, this);
	childSetAction("btn_add", onClickAdd, this);
	childSetAction("btn_copy", onClickCopy, this);
	childSetAction("btn_default", onClickDefault, this);
	childSetAction("btn_gridinfo", onClickGridInfo, this);
	childSetAction("btn_help_render_compat", onClickHelpRenderCompat, this);
	childSetAction("btn_advanced", onClickAdvanced, this);
	
	childSetCommitCallback("grid_selector", onSelectGrid, this);
	childSetCommitCallback("platform", onSelectPlatform, this);

	// !!!### 	server_choice_combo->setFocusLostCallback(onServerComboLostFocus);
	
	reset();
	return true;
}


// called from setFocus()
// called internally too
void HippoPanelGridsImpl::refresh()
{
	const std::string &defaultGrid = gHippoGridManager->getDefaultGridName();

	LLComboBox *grids = getChild<LLComboBox>("grid_selector");
	S32 selectIndex = -1, i = 0;
	grids->removeall();
	if (!defaultGrid.empty()) {
		grids->add(defaultGrid);
		selectIndex = i++;
	}
	HippoGridManager::GridIterator it, end = gHippoGridManager->endGrid();
	for (it = gHippoGridManager->beginGrid(); it != end; ++it) {
		const std::string &grid = it->second->getGridName();
		if (grid != defaultGrid) {
			grids->add(grid);
			if (grid == mCurGrid) selectIndex = i;
			i++;
		}
	}
	if ((mState == ADD_NEW) || (mState == ADD_COPY)) {
		grids->add("<new>");
		selectIndex = i++;
	}
	if (selectIndex >= 0) {
		grids->setCurrentByIndex(selectIndex);
	} else {
		grids->setLabel(LLStringUtil::null);  // LLComboBox::removeall() does not clear the label
	}

	childSetTextArg("default_grid", "[DEFAULT]", !defaultGrid.empty() ? defaultGrid : " ");

	childSetEnabled("btn_delete", (selectIndex >= 0) && mIsEditable );
	childSetEnabled("btn_copy", (mState == NORMAL) && (selectIndex >= 0));
	childSetEnabled("btn_default", (mState == NORMAL) && (selectIndex > 0));
	childSetEnabled("gridname", (mState == ADD_NEW) || (mState == ADD_COPY));	
}


// ********************************************************************
// called from preferences floater

void HippoPanelGridsImpl::apply()
{
	if (saveCurGrid()) {
		// adding new grid did not fail
		gHippoGridManager->setCurrentGrid(mCurGrid);
	}
	gHippoGridManager->saveFile();
	refresh();
	// update render compatibility
	if (mCurGrid == gHippoGridManager->getConnectedGrid()->getGridName())
		gHippoLimits->setLimits();
}

// called on close too
void HippoPanelGridsImpl::cancel()
{
	reset();
}


// ********************************************************************
// load/save current grid

void HippoPanelGridsImpl::loadCurGrid()
{
	HippoGridInfo *gridInfo = gHippoGridManager->getGrid(mCurGrid);
	if (gridInfo && (mState != ADD_NEW)) {
		LLComboBox *platform = getChild<LLComboBox>("platform");
		if (platform) platform->setCurrentByIndex(gridInfo->getPlatform());
		childSetText("gridname", gridInfo->getGridName());
		childSetText("loginuri", gridInfo->getLoginUri());
		childSetText("loginpage", gridInfo->getLoginPage());
		childSetText("helperuri", gridInfo->getHelperUri());
		childSetText("website", gridInfo->getWebSite());
		childSetText("support", gridInfo->getSupportUrl());
		childSetText("search", gridInfo->getSearchUrl());
		childSetText("register", gridInfo->getRegisterUrl());
		childSetText("partner", gridInfo->getPartnerUrl());
		childSetText("password", gridInfo->getPasswordUrl());
		childSetValue("render_compat", gridInfo->isRenderCompat());
		enableEditing(!gridInfo->getLocked());
	} else {
		const std::string& empty = LLStringUtil::null;
		LLComboBox *platform = getChild<LLComboBox>("platform");
		if (platform) platform->setCurrentByIndex(HippoGridInfo::PLATFORM_OTHER);
		childSetText("gridname", empty);
		childSetText("loginuri", empty);
		childSetText("loginpage", empty);
		childSetText("helperuri", empty);
		childSetText("website", empty);
		childSetText("support", empty);
		childSetText("search", empty);
		childSetText("register", empty);
		childSetText("partner", empty);
		childSetText("password", empty);
		childSetValue("render_compat", true);
		enableEditing(true);
	}

	if (mState == ADD_NEW) {
		std::string required = "<required>";
		childSetText("gridname", required);
		childSetText("loginuri", required);
	} else if (mState == ADD_COPY) {
		childSetText("gridname", std::string("<required>"));
		enableEditing(true);
	} else if (mState != NORMAL) {
		LL_WARNS() << "Illegal state " << mState << '.' << LL_ENDL;
	}
	
	refresh();
}

// returns false, if adding new grid failed
bool HippoPanelGridsImpl::saveCurGrid()
{
	HippoGridInfo *gridInfo = 0;
	
	gridInfo = gHippoGridManager->getGrid(mCurGrid);
	//gridInfo->getGridInfo();
	refresh();
	
	std::string gridname = childGetValue("gridname");
	if (gridname == "<required>") gridname.clear();
	std::string loginuri = childGetValue("loginuri");
	if (loginuri == "<required>") loginuri.clear();
	
	if (gridname.empty() && !loginuri.empty())
		this->retrieveGridInfo();
	
	if ((mState == ADD_NEW) || (mState == ADD_COPY)) {
		
		// check nickname
		std::string gridname = childGetValue("gridname");
		childSetValue("gridname", !gridname.empty() ? gridname : "<required>");
		if (gridname.empty()) {
			LLNotificationsUtil::add("GridsNoNick");
			return false;
		}
		if (gHippoGridManager->getGrid(gridname)) {
			LLSD args;
			args["NAME"] = gridname;
			LLNotificationsUtil::add("GridExists", args);
			return false;
		}
		
		// check login URI
		if (loginuri.empty()) {
			LLSD args;
			args["NAME"] = gridname;
			LLNotificationsUtil::add("GridsNoLoginUri", args);
			return false;
		}
		
		mState = NORMAL;
		mCurGrid = gridname;
		gridInfo = new HippoGridInfo(gridname);
		gridInfo->setLoginUri(loginuri);
		gHippoGridManager->addGrid(gridInfo);
		try
		{
			gridInfo->getGridInfo();
		}
		catch (AIAlert::ErrorCode const& error)
		{
			if (error.getCode() == HTTP_NOT_FOUND || error.getCode() == HTTP_METHOD_NOT_ALLOWED)
			{
				// Ignore this error; it might be a user entered entry for a grid that has no get_grid_info support.
				LL_WARNS() << AIAlert::text(error) << LL_ENDL;
			}
			else if (error.getCode() == HTTP_OK)
			{
				// XML parse error.
				AIAlert::add("GridInfoError", error);
			}
			else
			{
				// Append GridInfoErrorInstruction to error message.
				AIAlert::add("GridInfoError", AIAlert::Error(AIAlert::Prefix(), AIAlert::not_modal, error, "GridInfoErrorInstruction"));
			}
		}
	}

	if (!gridInfo) {
		LL_WARNS() << "Grid not found, ignoring changes." << LL_ENDL;
		return true;
	}

	gridInfo->setPlatform(childGetValue("platform"));
	gridInfo->setGridName(childGetValue("gridname"));
	gridInfo->setLoginUri(childGetValue("loginuri"));
	gridInfo->setLoginPage(childGetValue("loginpage"));
	gridInfo->setHelperUri(childGetValue("helperuri"));
	gridInfo->setWebSite(childGetValue("website"));
	gridInfo->setSupportUrl(childGetValue("support"));
	gridInfo->setRegisterUrl(childGetValue("register"));
	gridInfo->setPartnerUrl(childGetValue("partner"));
	gridInfo->setPasswordUrl(childGetValue("password"));
	gridInfo->setSearchUrl(childGetValue("search"));
	gridInfo->setRenderCompat(childGetValue("render_compat"));
	
	refresh();
	return true;
}


// ********************************************************************
// local helper functions

void HippoPanelGridsImpl::reset()
{
	mState = NORMAL;
	mCurGrid = gHippoGridManager->getCurrentGridName();
	loadCurGrid();
}


void HippoPanelGridsImpl::retrieveGridInfo()
{
	std::string loginuri = childGetValue("loginuri");
	if ((loginuri.empty()) || (loginuri == "<required>")) {
		LLNotificationsUtil::add("GridInfoNoLoginUri");
		return;
	}
	
	HippoGridInfo *grid = 0;
	bool cleanupGrid = false;
	if (mState == NORMAL) {
		grid = gHippoGridManager->getGrid(mCurGrid);
	} else if ((mState == ADD_NEW) || (mState == ADD_COPY)) {
		grid = new HippoGridInfo(LLStringUtil::null);
		cleanupGrid = true;
	} else {
		LL_ERRS() << "Illegal state " << mState << '.' << LL_ENDL;
		return;
	}
	if (!grid) {
		LL_ERRS() << "Internal error retrieving grid info." << LL_ENDL;
		return;
	}
	
	grid->setLoginUri(loginuri);
	try
	{
		grid->getGridInfo();

		if (grid->getPlatform() != HippoGridInfo::PLATFORM_OTHER)
			getChild<LLComboBox>("platform")->setCurrentByIndex(grid->getPlatform());
		if (!grid->getGridName().empty()) childSetText("gridname", grid->getGridName());
		if (!grid->getLoginUri().empty()) childSetText("loginuri", grid->getLoginUri());
		if (!grid->getLoginPage().empty()) childSetText("loginpage", grid->getLoginPage());
		if (!grid->getHelperUri().empty()) childSetText("helperuri", grid->getHelperUri());
		if (!grid->getWebSite().empty()) childSetText("website", grid->getWebSite());
		if (!grid->getSupportUrl().empty()) childSetText("support", grid->getSupportUrl());
		if (!grid->getRegisterUrl().empty()) childSetText("register", grid->getRegisterUrl());
		if (!grid->getPartnerUrl().empty()) childSetText("partner", grid->getPartnerUrl());
		if (!grid->getPasswordUrl().empty()) childSetText("password", grid->getPasswordUrl());
		if (!grid->getSearchUrl().empty()) childSetText("search", grid->getSearchUrl());
		if (!grid->getGridMessage().empty()) childSetText("gridmessage", grid->getGridMessage());
	}
	catch(AIAlert::ErrorCode const& error)
	{
		if (error.getCode() == HTTP_METHOD_NOT_ALLOWED || error.getCode() == HTTP_OK)
		{
			AIAlert::add("GridInfoError", error);
		}
		else
		{
			// Append GridInfoErrorInstruction to error message.
			AIAlert::add("GridInfoError", AIAlert::Error(AIAlert::Prefix(), AIAlert::not_modal, error, "GridInfoErrorInstruction"));
		}
	}

	if (cleanupGrid) delete grid;
}


// ********************************************************************
// UI callbacks

// static
void HippoPanelGridsImpl::onSelectGrid(LLUICtrl* ctrl, void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	std::string newGrid = ctrl->getValue().asString();
	if (!self->saveCurGrid()) {
		// creating new grid failed
		LLComboBox *grids = self->getChild<LLComboBox>("grid_selector");
		grids->setCurrentByIndex(grids->getItemCount() - 1);
		return;
	}
	self->mCurGrid = newGrid;
	self->loadCurGrid();
}

// static
void HippoPanelGridsImpl::onSelectPlatform(LLUICtrl *ctrl, void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	self->refresh();
}

// static
void HippoPanelGridsImpl::onClickDelete(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	if (self->mState == NORMAL)
		gHippoGridManager->deleteGrid(self->mCurGrid);
	self->reset();
}

// static
void HippoPanelGridsImpl::onClickAdd(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	self->mState = ADD_NEW;
	self->loadCurGrid();
}

// static
void HippoPanelGridsImpl::onClickCopy(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	if (self->mState == NORMAL) {
		self->mState = ADD_COPY;
		self->loadCurGrid();
	}
}

// static
void HippoPanelGridsImpl::onClickDefault(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	if (self->mState == NORMAL) {
		if (self->saveCurGrid())
		{
			gHippoGridManager->setDefaultGrid(self->mCurGrid);
		}
		self->refresh();
	}
}

// static
void HippoPanelGridsImpl::onClickGridInfo(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	self->retrieveGridInfo();
}

// static
void HippoPanelGridsImpl::onClickAdvanced(void *data)
{
	HippoPanelGridsImpl *self = (HippoPanelGridsImpl*)data;
	if(self->mState != NORMAL)
	{
		self->retrieveGridInfo();
	}
	if(self->childIsVisible("loginpage_label"))
	{
		self->childSetVisible("loginpage_label", false);
		self->childSetVisible("loginpage", false);
		self->childSetVisible("helperuri_label", false);
		self->childSetVisible("helperuri", false);
		self->childSetVisible("website_label", false);
		self->childSetVisible("website", false);
		self->childSetVisible("support_label", false);
		self->childSetVisible("support", false);
		self->childSetVisible("register_label", false);
		self->childSetVisible("register", false);
		self->childSetVisible("partner_label", false);
		self->childSetVisible("partner", false);
		self->childSetVisible("password_label", false);
		self->childSetVisible("password", false);
		self->childSetVisible("search_label", false);
		self->childSetVisible("search", false);
		self->childSetVisible("render_compat", false);
		self->childSetVisible("btn_help_render_compat", false);
	}
	else
	{
		self->childSetVisible("loginpage_label", true);
		self->childSetVisible("loginpage", true);
		self->childSetVisible("helperuri_label", true);
		self->childSetVisible("helperuri", true);
		self->childSetVisible("website_label", true);
		self->childSetVisible("website", true);
		self->childSetVisible("support_label", true);
		self->childSetVisible("support", true);
		self->childSetVisible("register_label", true);
		self->childSetVisible("register", true);
		self->childSetVisible("partner_label", true);
		self->childSetVisible("partner", true);
		self->childSetVisible("password_label", true);
		self->childSetVisible("password", true);
		self->childSetVisible("search_label", true);
		self->childSetVisible("search", true);
		self->childSetVisible("render_compat", true);
		self->childSetVisible("btn_help_render_compat", true);
	}
}

// static
void HippoPanelGridsImpl::onClickHelpRenderCompat(void *data)
{
	LLNotificationsUtil::add("HelpRenderCompat");
}


void HippoPanelGridsImpl::enableEditing(bool b)
{
	static const char * elements [] = {
		"platform",
		"gridname",
		"loginuri",
		"loginpage",
		"helperuri",
		"website",
		"support",
		"register",
		"partner",
		"password",
		"search",
		"btn_delete",
		"btn_gridinfo",
		"render_compat",
		"gridmessage",
		0
	};

	for(int i = 0; elements[i]; ++i ) {
		this->childSetEnabled(elements[i], b);
	}

	mIsEditable = b;
}
