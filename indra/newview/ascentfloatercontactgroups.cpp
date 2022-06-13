/** 
 * @file ascentfloatercontactgroups.cpp
 * @Author Charley Levenque
 * Allows the user to assign friends to contact groups for advanced sorting.
 *
 * Created Sept 6th 2010
 * 
 * ALL SOURCE CODE IS PROVIDED "AS IS." THE CREATOR MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.

*/

#include "llviewerprecompiledheaders.h"

#include "ascentfloatercontactgroups.h"

//UI Elements
#include "llbutton.h" //Buttons
#include "llcombobox.h" //Combo dropdowns
#include "llscrolllistctrl.h" //List box for filenames
#include "lluictrlfactory.h" //Loads the XUI
#include "llresmgr.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llsdserialize.h"
#include "llfile.h"
#include "llchat.h"
#include "llfloaterchat.h"

ASFloaterContactGroups* ASFloaterContactGroups::sInstance = NULL;
uuid_vec_t ASFloaterContactGroups::mSelectedUUIDs;
LLSD ASFloaterContactGroups::mContactGroupData;

ASFloaterContactGroups::ASFloaterContactGroups()
:	LLFloater(std::string("floater_contact_groups"), std::string("FloaterContactRect"), LLStringUtil::null)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_contact_groups.xml");
}

// static
void ASFloaterContactGroups::show(const uuid_vec_t& ids)
{
    if (!sInstance)
	sInstance = new ASFloaterContactGroups();
	mSelectedUUIDs = ids;

    sInstance->open();
	sInstance->populateGroupList();
	sInstance->populateFriendList();

	sInstance->childSetAction("combo_group_add", onBtnAdd, sInstance);
	sInstance->childSetAction("combo_group_remove", onBtnRemove, sInstance);
	//sInstance->childSetAction("New", onBtnCreate, sInstance);
	//sInstance->childSetAction("Delete", onBtnDelete, sInstance);
}

void ASFloaterContactGroups::onBtnDelete(void* userdata)
{
	ASFloaterContactGroups* self = (ASFloaterContactGroups*)userdata;

	if(self)
	{
		LLScrollListCtrl* scroller = self->getChild<LLScrollListCtrl>("group_scroll_list");
		if(scroller != NULL) 
		{
			self->deleteContactGroup(scroller->getValue().asString());
			self->populateGroupList();
		}
	}
}

void ASFloaterContactGroups::onBtnAdd(void* userdata)
{
	ASFloaterContactGroups* self = (ASFloaterContactGroups*)userdata;
	LL_INFOS() << "Button Add Begin" << LL_ENDL;
	if(self)
	{
		LLComboBox* combo = self->getChild<LLComboBox>("buddy_group_combobox");
		if (combo->getCurrentIndex() == -1) //Entered text is a new group name, create a group first
		{
			std::string name = combo->getSimple();
			self->createContactGroup(name);
			combo->selectByValue(name);
		}
		uuid_vec_t::reverse_iterator it = self->mSelectedUUIDs.rbegin();
		for (;it != self->mSelectedUUIDs.rend();++it)
		{
			//self->addContactMember(combo->getSimple(), self->mSelectedUUIDs.get(i));
		}
	}
}


void ASFloaterContactGroups::onBtnRemove(void* userdata)
{
	ASFloaterContactGroups* self = (ASFloaterContactGroups*)userdata;

	if(self)
	{
		if (!self->mSelectedUUIDs.empty())
		{
			LLScrollListCtrl* scroller = self->getChild<LLScrollListCtrl>("group_scroll_list");
			if(scroller != NULL) 
			{
				uuid_vec_t::size_type i = self->mSelectedUUIDs.size();
				uuid_vec_t::reverse_iterator it = self->mSelectedUUIDs.rbegin();
				for (;it != self->mSelectedUUIDs.rend();++it)
				{
					std::string i_str;
					LLResMgr::getInstance()->getIntegerString(i_str, --i);
					LLChat msg("Adding index " + i_str + ": " + it->asString());
					LLFloaterChat::addChat(msg);

					self->addContactMember(scroller->getValue().asString(), *it);
				}
			}
		}
	}
}

void ASFloaterContactGroups::onBtnCreate(void* userdata)
{
	ASFloaterContactGroups* self = (ASFloaterContactGroups*)userdata;
	if(self)
	{
		LLLineEditor* editor = self->getChild<LLLineEditor>("add_group_lineedit");
		if (editor)
		{
			/*LLScrollListCtrl* scroller = self->getChild<LLScrollListCtrl>("friend_scroll_list");
			if(scroller != NULL) 
			{
				self->createContactGroup(editor->getValue().asString());
				self->populateGroupList();
			}*/
		}
		else
		{
			LLChat msg("Null editor");
			LLFloaterChat::addChat(msg);
		}
	}
	else
	{
		LLChat msg("Null floater");
		LLFloaterChat::addChat(msg);
	}
}

ASFloaterContactGroups::~ASFloaterContactGroups()
{
    sInstance=NULL;
}

void ASFloaterContactGroups::populateFriendList()
{
	/*LLScrollListCtrl* scroller = getChild<LLScrollListCtrl>("friend_scroll_list");
	if(scroller != NULL) 
	{
		
	}*/
}

void ASFloaterContactGroups::addContactMember(std::string contact_grp, LLUUID to_add)
{
	BOOL is_new = true;
	S32 entrycount = ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][contact_grp].size();
	for(S32 i = 0; i < entrycount; i++)
	{
		if (ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][contact_grp][i].asString() == to_add.asString())
		{
			is_new = false;
			break;
		}
	}
	if (is_new)
	{
		ASFloaterContactGroups::mContactGroupData[contact_grp].append(to_add.asString());
		gSavedPerAccountSettings.setLLSD("AscentContactGroups", ASFloaterContactGroups::mContactGroupData);
	}
	populateActiveGroupList(to_add);
}

void ASFloaterContactGroups::populateActiveGroupList(LLUUID user_key)
{
	LLScrollListCtrl* scroller = getChild<LLScrollListCtrl>("group_scroll_list");
	if(scroller != NULL) 
	{
		LL_INFOS() << "Cleaning and rebuilding group list" << LL_ENDL;
		scroller->deleteAllItems();

		S32 count = ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"].size();
		for (S32 index = 0; index < count; index++)
		{
			LL_INFOS() << "Entries for " << ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index].asString() << LL_ENDL;
			S32 entrycount = ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index].size();
			for(S32 i = 0; i < entrycount; i++)
			{
				LL_INFOS() << "Subentries for " << ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index][i].asString() << LL_ENDL;
				if (ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index][i].asString() == user_key.asString())
				{

					scroller->addSimpleElement(ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index].asString(), ADD_BOTTOM);
					break;
				}
			}
		}
	} 
}

void ASFloaterContactGroups::deleteContactGroup(std::string contact_grp)
{
	
}

void ASFloaterContactGroups::createContactGroup(std::string contact_grp)
{
	ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"].append(contact_grp);
	gSavedPerAccountSettings.setLLSD("AscentContactGroups", ASFloaterContactGroups::mContactGroupData);
	populateGroupList();
}

void ASFloaterContactGroups::populateGroupList()
{
	ASFloaterContactGroups::mContactGroupData = gSavedPerAccountSettings.getLLSD("AscentContactGroups");
	LLComboBox* combo = getChild<LLComboBox>("buddy_group_combobox");
	if(combo != NULL) 
	{
		combo->removeall();

		S32 count = ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"].size();
		S32 index;
		for (index = 0; index < count; index++)
		{
			std::string group = ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index].asString();
			if (group != "")
			{
				LL_INFOS() << "Adding " << group << LL_ENDL;
				combo->add(ASFloaterContactGroups::mContactGroupData["ASC_MASTER_GROUP_LIST"][index].asString(), ADD_BOTTOM);
			}
		}
	} 
}
