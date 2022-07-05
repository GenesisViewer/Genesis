/** 
 * @file llfloatergroups.cpp
 * @brief LLPanelGroups class implementation
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
 * Shown from Edit -> Groups...
 * Shows the agent's groups and allows the edit window to be invoked.
 * Also overloaded to allow picking of a single group for assigning 
 * objects and land to groups.
 */

#include "llviewerprecompiledheaders.h"
#include "roles_constants.h"
#include "llfloatercontacts.h"
#include "llagent.h"
#include "llcolorswatch.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lltaggedavatarsmgr.h"
#include "llnotificationsutil.h"
#include "lluuid.h"
using namespace LLOldEvents;

// helper functions
void init_contact_set_list(LLScrollListCtrl* ctrl);
///----------------------------------------------------------------------------
/// Class LLPanelGroups
///----------------------------------------------------------------------------

//LLEventListener
//virtual
bool LLPanelContactSets::handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
{
	
	return false	;
}

// Default constructor
LLPanelContactSets::LLPanelContactSets() :
	LLPanel()
{
	
}

LLPanelContactSets::~LLPanelContactSets()
{
	
}



// clear the group list, and get a fresh set of info.
void LLPanelContactSets::reset()
{
	
	init_contact_set_list(getChild<LLScrollListCtrl>("contact set list"));
	enableButtons();
}

BOOL LLPanelContactSets::postBuild()
{
	
	mContactSetList = getChild<LLScrollListCtrl>("contact set list");
	mGlobalSwatch = getChild<LLColorSwatchCtrl>("contact_set_color");
	//getChild<LLUICtrl>("Add")->setCommitCallback(boost::bind(&LLPanelContactSets::addContactSet));
	childSetAction("Add", addContactSet, this);
	childSetAction("Rename", renameContactSet, this);
	//mCommitCallbackRegistrar.add("ContactSet.Add",	boost::bind(&LLPanelContactSets::addContactSet, this));
	mContactSetList->setCommitOnSelectionChange(true);
	mContactSetList->setCommitCallback(boost::bind(&LLPanelContactSets::onSelectionEntry, this));

	//mGlobalSwatch->setCommitCallback(boost::bind(&LLPanelContactSets::onColorChange, this));	
	//getChild<LLColorSwatchCtrl>("contact_set_color")->setCommitCallback(boost::bind(&LLPanelEditWearable::onColorSwatchCommit, self, _1));
	//childSetCommitCallback("contact_set_color", onColorChange, this);
	
	reset();

	return TRUE;
}
void LLPanelContactSets::onColorChange () {
	
	
	LLColor4 newColor = mGlobalSwatch->get();
	
	if (mContactSetList->getFirstSelected() && newColor != NULL) {
		std::string csId = mContactSetList->getFirstSelected()->getColumn(1)->getValue().asString();
		LLTaggedAvatarsMgr::instance().updateColorContactSet(csId, newColor);
	}
	
}
void LLPanelContactSets::onSelectionEntry () {
	
//
    std::string csId = mContactSetList->getFirstSelected()->getColumn(1)->getValue().asString();
	LLColor4 color = LLTaggedAvatarsMgr::instance().getColorContactSet(csId);
	mGlobalSwatch->set(color);
	enableButtons();
}
void LLPanelContactSets::enableButtons()
{
	

	
	

}
void LLPanelContactSets::renameContactSet(void* userdata) {
	LLPanelContactSets* self = (LLPanelContactSets*)userdata;
	LLScrollListCtrl *list = self->getChild<LLScrollListCtrl>("contact set list");

	if (list->getFirstSelected()) {
		LLSD payload;
		LLSD args;
		args["ContactSetName"] = list->getFirstSelected()->getColumn(0)->getValue().asString();
		LLNotificationsUtil::add("RenameContactSet", args, payload,
							 boost::bind(&LLPanelContactSets::callbackRenameContactSet, self, _1, _2));
	}

	
    
	

}
bool LLPanelContactSets::callbackRenameContactSet(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option > 0) {
		return false;
	}
	
	std::string csId = mContactSetList->getFirstSelected()->getColumn(1)->getValue().asString();
	
	if ( response.has("contactsetname") && response["contactsetname"].isString() && !response["contactsetname"].asString().empty()) {
		
		LLTaggedAvatarsMgr::instance().updateContactSetName(csId,response["contactsetname"]);
		
		init_contact_set_list(mContactSetList);
		mContactSetList->selectByValue(csId);
	}
	return false;
}
void LLPanelContactSets::addContactSet(void* userdata) {
	LLPanelContactSets* self = (LLPanelContactSets*)userdata;
	LL_INFOS() << "Adding Contact Set" << LL_ENDL;
	std::string newId = LLUUID::generateNewID().asString();			
	std::string alias = "New Contact Set";
	std::string id = LLTaggedAvatarsMgr::instance().insertContactSet(newId,alias);
	
	

	LLScrollListCtrl *list = self->getChild<LLScrollListCtrl>("contact set list");
	init_contact_set_list(list);
	
	list->selectByValue(LLUUID(id));
			 							 
}



void init_contact_set_list(LLScrollListCtrl* ctrl)
{
	
	std::map<std::string, std::string> contact_sets = LLTaggedAvatarsMgr::instance().getContactSets();
	LLCtrlListInterface *contact_set_list = ctrl->getListInterface();
	contact_set_list->operateOnAll(LLCtrlListInterface::OP_DELETE);
	
	for(const auto& contact_set : contact_sets)
	{
		
		LLColor4 color = LLTaggedAvatarsMgr::instance().getColorContactSet(contact_set.first);
		LLScrollListItem::Params newElement;
		newElement.value = contact_set.first;
		LLScrollListCell::Params name;
		name.column = "contactsetname";
		name.type = "text";
		name.value = contact_set.second;
		name.color=color*0.5f + color*0.5f;;
		newElement.columns.add(name);
		LLScrollListCell::Params id;
		id.column = "contactsetid";
		id.type = "text";
		id.value = contact_set.first;
		id.color=color*0.5f + color*0.5f;;
		newElement.columns.add(id);
		ctrl->addRow(newElement);


		
	}

}

