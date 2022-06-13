/** 
 * @file llpanelgroupinvite.cpp
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * 
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llpanelgroupinvite.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llfloateravatarpicker.h"
#include "llbutton.h"
#include "llcallingcard.h"
#include "llcombobox.h"
#include "llgroupactions.h"
#include "llgroupmgr.h"
#include "llnamelistctrl.h"
#include "llnotificationsutil.h"
#include "llscrolllistitem.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"

#include <boost/foreach.hpp>

class LLPanelGroupInvite::impl : public boost::signals2::trackable
{
public:
	impl(const LLUUID& group_id);
	~impl();

	void addUsers(const std::vector<std::string>& names,
				  const uuid_vec_t& agent_ids);
	void submitInvitations();
	void addRoleNames(LLGroupMgrGroupData* gdatap);
	void handleRemove();
	void handleSelection();

	static void callbackClickCancel(void* userdata);
	static void callbackClickOK(void* userdata);
	static void callbackClickAdd(void* userdata);
	static void callbackClickRemove(void* userdata);
	static void callbackSelect(LLUICtrl* ctrl, void* userdata);
	void callbackAddUsers(const uuid_vec_t& agent_ids);
	
	void onAvatarNameCache(const LLUUID& agent_id,
											 const LLAvatarName& av_name);

	bool inviteOwnerCallback(const LLSD& notification, const LLSD& response);

public:
	LLUUID mGroupID;

	std::string		mLoadingText;
	LLNameListCtrl	*mInvitees;
	LLComboBox      *mRoleNames;
	LLButton		*mOKButton;
 	LLButton		*mRemoveButton;
	LLTextBox		*mGroupName;
	std::string		mOwnerWarning;
	std::string		mAlreadyInGroup;
	std::string		mTooManySelected;
	bool		mConfirmedOwnerInvite;

	void (*mCloseCallback)(void* data);

	void* mCloseCallbackUserData;

	std::map<LLUUID, boost::signals2::connection> mAvatarNameCacheConnection;
};


LLPanelGroupInvite::impl::impl(const LLUUID& group_id):
	mGroupID( group_id ),
	mLoadingText (),
	mInvitees ( NULL ),
	mRoleNames( NULL ),
	mOKButton ( NULL ),
	mRemoveButton( NULL ),
	mGroupName( NULL ),
	mConfirmedOwnerInvite( false ),
	mCloseCallback( NULL ),
	mCloseCallbackUserData( NULL ),
	mAvatarNameCacheConnection()
{
}

LLPanelGroupInvite::impl::~impl()
{
	for (std::map<LLUUID, boost::signals2::connection>::const_iterator it = mAvatarNameCacheConnection.begin(); it != mAvatarNameCacheConnection.end(); ++it)
		if ((*it).second.connected())
			(*it).second.disconnect();
}

const S32 MAX_GROUP_INVITES = 100; // Max invites per request. 100 to match server cap.

void LLPanelGroupInvite::impl::addUsers(const std::vector<std::string>& names,
										const uuid_vec_t& agent_ids)
{
	std::string name;
	LLUUID id;

	for (S32 i = 0; i < (S32)names.size(); i++)
	{
		name = names[i];
		id = agent_ids[i];

		// Make sure this agent isn't already in the list.
		bool already_in_list = false;
		std::vector<LLScrollListItem*> items = mInvitees->getAllData();
		for (std::vector<LLScrollListItem*>::iterator iter = items.begin();
			 iter != items.end(); ++iter)
		{
			LLScrollListItem* item = *iter;
			if (item->getUUID() == id)
			{
				already_in_list = true;
				break;
			}
		}
		if (already_in_list)
		{
			continue;
		}

		//add the name to the names list
		LLSD row;
		row["id"] = id;
		row["columns"][0]["column"] = "name";
		row["columns"][0]["value"] = name;

		mInvitees->addElement(row);
	}
}

void LLPanelGroupInvite::impl::submitInvitations()
{
	std::map<LLUUID, LLUUID> role_member_pairs;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	// Default to everyone role.
	LLUUID role_id = LLUUID::null;

	if (mRoleNames)
	{
		role_id = mRoleNames->getCurrentID();
		
		// owner role: display confirmation and wait for callback
		if ((role_id == gdatap->mOwnerRole) && (!mConfirmedOwnerInvite))
		{
			LLSD args;
			args["MESSAGE"] = mOwnerWarning;
			LLNotificationsUtil::add("GenericAlertYesCancel", args, LLSD(), boost::bind(&LLPanelGroupInvite::impl::inviteOwnerCallback, this, _1, _2));
			return; // we'll be called again if user confirms
		}
	}

	bool already_in_group = false;
	//loop over the users
	std::vector<LLScrollListItem*> items = mInvitees->getAllData();
	for (std::vector<LLScrollListItem*>::iterator iter = items.begin();
		 iter != items.end(); ++iter)
	{
		LLScrollListItem* item = *iter;
		if(LLGroupActions::isAvatarMemberOfGroup(mGroupID, item->getUUID()))
		{
			already_in_group = true;
			continue;
		}
		role_member_pairs[item->getUUID()] = role_id;
	}
	
	if (role_member_pairs.size() > MAX_GROUP_INVITES)
	{
		// Fail!
		LLSD msg;
		msg["MESSAGE"] = mTooManySelected;
		LLNotificationsUtil::add("GenericAlert", msg);
		(*mCloseCallback)(mCloseCallbackUserData);
		return;
	}

	LLGroupMgr::getInstance()->sendGroupMemberInvites(mGroupID, role_member_pairs);
	
	if(already_in_group)
	{
		LLSD msg;
		msg["MESSAGE"] = mAlreadyInGroup;
		LLNotificationsUtil::add("GenericAlert", msg);
	}

	//then close
	(*mCloseCallback)(mCloseCallbackUserData);
}

bool LLPanelGroupInvite::impl::inviteOwnerCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	switch(option)
	{
	case 0:
		// user confirmed that they really want a new group owner
		mConfirmedOwnerInvite = true;
		submitInvitations();
		break;
	case 1:
		// fall through
	default:
		break;
	}
	return false;
}



void LLPanelGroupInvite::impl::addRoleNames(LLGroupMgrGroupData* gdatap)
{
	LLGroupMgrGroupData::member_list_t::iterator agent_iter =
		gdatap->mMembers.find(gAgent.getID());

	//get the member data for the agent if it exists
	if ( agent_iter != gdatap->mMembers.end() )
	{
		LLGroupMemberData* member_data = (*agent_iter).second;

		//loop over the agent's roles in the group
		//then add those roles to the list of roles that the agent
		//can invite people to be
		if ( member_data && mRoleNames)
		{
			//if the user is the owner then we add
			//all of the roles in the group
			//else if they have the add to roles power
			//we add every role but owner,
			//else if they have the limited add to roles power
			//we add every role the user is in
			//else we just add to everyone
			bool is_owner   = member_data->isOwner();
			bool can_assign_any = gAgent.hasPowerInGroup(mGroupID,
												 GP_ROLE_ASSIGN_MEMBER);
			bool can_assign_limited = gAgent.hasPowerInGroup(mGroupID,
												 GP_ROLE_ASSIGN_MEMBER_LIMITED);

			LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.begin();
			LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();

			//populate the role list
			for ( ; rit != end; ++rit)
			{
				LLUUID role_id = (*rit).first;
				LLRoleData rd;
				if ( gdatap->getRoleData(role_id,rd) )
				{
					// Owners can add any role.
					if ( is_owner 
						// Even 'can_assign_any' can't add owner role.
						 || (can_assign_any && role_id != gdatap->mOwnerRole)
						// Add all roles user is in
						 || (can_assign_limited && member_data->isInRole(role_id))
						// Everyone role.
						 || role_id == LLUUID::null )
					{
							mRoleNames->add(rd.mRoleName,
											role_id,
											ADD_BOTTOM);
					}
				}
			}
		}//end if member data is not null
	}//end if agent is in the group
}

//static
void LLPanelGroupInvite::impl::callbackClickAdd(void* userdata)
{
	LLPanelGroupInvite* panelp = (LLPanelGroupInvite*) userdata;

	if ( panelp )
	{
		//Right now this is hard coded with some knowledge that it is part
		//of a floater since the avatar picker needs to be added as a dependent
		//floater to the parent floater.
		//Soon the avatar picker will be embedded into this panel
		//instead of being it's own separate floater.  But that is next week.
		//This will do for now. -jwolk May 10, 2006
		LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(
			boost::bind(&impl::callbackAddUsers, panelp->mImplementation, _1), TRUE);
		if (picker)
		{
			gFloaterView->getParentFloater(panelp)->addDependentFloater(picker);
		}
	}
}

//static
void LLPanelGroupInvite::impl::callbackClickRemove(void* userdata)
{
	impl* selfp = (impl*) userdata;

	if ( selfp ) selfp->handleRemove();
}

void LLPanelGroupInvite::impl::handleRemove()
{
	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = 
			mInvitees->getAllSelected();
	if (selection.empty()) return;

	// Remove all selected invitees.
	mInvitees->deleteSelectedItems();
	mRemoveButton->setEnabled(FALSE);
}

// static
void LLPanelGroupInvite::impl::callbackSelect(
									LLUICtrl* ctrl, void* userdata)
{
	impl* selfp = (impl*) userdata;
	if ( selfp ) selfp->handleSelection();
}

void LLPanelGroupInvite::impl::handleSelection()
{
	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = 
			mInvitees->getAllSelected();
	if (selection.empty())
	{
		mRemoveButton->setEnabled(FALSE);
	}
	else
	{
		mRemoveButton->setEnabled(TRUE);
	}
}

void LLPanelGroupInvite::impl::callbackClickCancel(void* userdata)
{
	impl* selfp = (impl*) userdata;

	if ( selfp ) 
	{
		(*(selfp->mCloseCallback))(selfp->mCloseCallbackUserData);
	}
}

void LLPanelGroupInvite::impl::callbackClickOK(void* userdata)
{
	impl* selfp = (impl*) userdata;

	if ( selfp ) selfp->submitInvitations();
}


void LLPanelGroupInvite::impl::callbackAddUsers(const uuid_vec_t& agent_ids)
{	
	std::vector<std::string> names;
	for (S32 i = 0; i < (S32)agent_ids.size(); i++)
	{
		const LLUUID& id(agent_ids[i]);
		if (mAvatarNameCacheConnection[id].connected())
		{
			mAvatarNameCacheConnection[id].disconnect();
		}
		mAvatarNameCacheConnection[id] = LLAvatarNameCache::get(id,
			boost::bind(&LLPanelGroupInvite::impl::onAvatarNameCache, this, _1, _2));
	}	
}

void LLPanelGroupInvite::impl::onAvatarNameCache(const LLUUID& agent_id,
											 const LLAvatarName& av_name)
{
	if (mAvatarNameCacheConnection[agent_id].connected())
	{
		mAvatarNameCacheConnection[agent_id].disconnect();
	}
	std::vector<std::string> names;
	uuid_vec_t agent_ids;
	agent_ids.push_back(agent_id);
	names.push_back(av_name.getCompleteName());
		
	addUsers(names, agent_ids);
}

LLPanelGroupInvite::LLPanelGroupInvite(const std::string& name,
									   const LLUUID& group_id)
	: LLPanel(name),
	  mImplementation(new impl(group_id)),
	  mPendingUpdate(FALSE)
{
	// Pass on construction of this panel to the control factory.
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_group_invite.xml", &getFactoryMap());
}

LLPanelGroupInvite::~LLPanelGroupInvite()
{
	delete mImplementation;
}

void LLPanelGroupInvite::setCloseCallback(void (*close_callback)(void*),
										  void* data)
{
	mImplementation->mCloseCallback         = close_callback;
	mImplementation->mCloseCallbackUserData = data;
}

void LLPanelGroupInvite::clear()
{
	mStoreSelected = LLUUID::null;
	mImplementation->mInvitees->deleteAllItems();
	mImplementation->mRoleNames->clear();
	mImplementation->mRoleNames->removeall();
	mImplementation->mOKButton->setEnabled(FALSE);
}

void LLPanelGroupInvite::addUsers(uuid_vec_t& agent_ids)
{
	std::vector<std::string> names;
	for (S32 i = 0; i < (S32)agent_ids.size(); i++)
	{
		std::string fullname;
		LLUUID agent_id = agent_ids[i];
		LLViewerObject* dest = gObjectList.findObject(agent_id);
		if(dest && dest->isAvatar())
		{
			LLNameValue* nvfirst = dest->getNVPair("FirstName");
			LLNameValue* nvlast = dest->getNVPair("LastName");
			if(nvfirst && nvlast)
			{
				fullname = LLCacheName::buildFullName(
					nvfirst->getString(), nvlast->getString());

			}
			if (!fullname.empty())
			{
				names.push_back(fullname);
			} 
			else 
			{
				LL_WARNS() << "llPanelGroupInvite: Selected avatar has no name: " << dest->getID() << LL_ENDL;
				names.push_back("(Unknown)");
			}
		}
		else
		{
			//looks like user try to invite offline friend
			//for offline avatar_id gObjectList.findObject() will return null
			//so we need to do this additional search in avatar tracker, see EXT-4732
			//if (LLAvatarTracker::instance().isBuddy(agent_id)) // Singu Note: We may be using this from another avatar list like group profile, disregard friendship status.
			{
				LLAvatarName av_name;
				if (!LLAvatarNameCache::get(agent_id, &av_name))
				{
					// actually it should happen, just in case
					LLAvatarNameCache::get(LLUUID(agent_id), boost::bind(
							&LLPanelGroupInvite::addUserCallback, this, _1, _2));
					// for this special case!
					//when there is no cached name we should remove resident from agent_ids list to avoid breaking of sequence
					// removed id will be added in callback
					agent_ids.erase(agent_ids.begin() + i);
				}
				else
				{
					names.push_back(av_name.getNSName());
				}
			}
		}
	}
	mImplementation->addUsers(names, agent_ids);
}

void LLPanelGroupInvite::addUserCallback(const LLUUID& id, const LLAvatarName& av_name)
{
	std::vector<std::string> names;
	uuid_vec_t agent_ids;
	agent_ids.push_back(id);
	names.push_back(av_name.getNSName());

	mImplementation->addUsers(names, agent_ids);
}

void LLPanelGroupInvite::draw()
{
	LLPanel::draw();
	if (mPendingUpdate)
	{
		updateLists();
	}
}
 
void LLPanelGroupInvite::update()
{
	mPendingUpdate = FALSE;
	if (mImplementation->mGroupName) 
	{
		mImplementation->mGroupName->setText(mImplementation->mLoadingText);
	}
	if ( mImplementation->mRoleNames ) 
	{
		mStoreSelected = mImplementation->mRoleNames->getCurrentID();
		mImplementation->mRoleNames->clear();
		mImplementation->mRoleNames->removeall();
		mImplementation->mRoleNames->add(mImplementation->mLoadingText, LLUUID::null, ADD_BOTTOM);
		mImplementation->mRoleNames->setCurrentByID(LLUUID::null);
	}

	updateLists();
}

void LLPanelGroupInvite::updateLists()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mImplementation->mGroupID);
	bool waiting = false;

	if (gdatap) 
	{
		if (gdatap->isGroupPropertiesDataComplete()) 
		{
			if (mImplementation->mGroupName) 
			{
				mImplementation->mGroupName->setText(gdatap->mName);
			}
		} 
		else 
		{
			waiting = true;
		}
		if (gdatap->isRoleDataComplete() && gdatap->isMemberDataComplete() && gdatap->isRoleMemberDataComplete()) 
		{
			if ( mImplementation->mRoleNames )
			{
				mImplementation->mRoleNames->clear();
				mImplementation->mRoleNames->removeall();

				//add the role names and select the everybody role by default
				mImplementation->addRoleNames(gdatap);
				mImplementation->mRoleNames->setCurrentByID(mStoreSelected);
			}
		} 
		else 
		{
			waiting = true;
		}
	} 
	else 
	{
		waiting = true;
	}

	if (waiting) 
	{
		if (!mPendingUpdate) 
		{
			LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mImplementation->mGroupID);
			LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mImplementation->mGroupID);
			LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(mImplementation->mGroupID);
			LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mImplementation->mGroupID);
		}
		mPendingUpdate = TRUE;
	} 
	else
	{
		mPendingUpdate = FALSE;
		if (mImplementation->mOKButton && mImplementation->mRoleNames->getItemCount()) 
		{
			mImplementation->mOKButton->setEnabled(TRUE);
		}
	}
}

BOOL LLPanelGroupInvite::postBuild()
{
	BOOL recurse = TRUE;

	mImplementation->mLoadingText = getString("loading");
	mImplementation->mRoleNames = getChild<LLComboBox>("role_name",
															   recurse);
	mImplementation->mGroupName = getChild<LLTextBox>("group_name_text", recurse);
	mImplementation->mInvitees = 
		getChild<LLNameListCtrl>("invitee_list", recurse);
	if ( mImplementation->mInvitees )
	{
		mImplementation->mInvitees->setCommitOnSelectionChange(TRUE);
		mImplementation->mInvitees->setCommitCallback(impl::callbackSelect, mImplementation);
	}

	LLButton* button = getChild<LLButton>("add_button", recurse);
	if ( button )
	{
		// default to opening avatarpicker automatically
		// (*impl::callbackClickAdd)((void*)this);
		button->setClickedCallback(impl::callbackClickAdd, this);
	}

	mImplementation->mRemoveButton = 
			getChild<LLButton>("remove_button", recurse);
	if ( mImplementation->mRemoveButton )
	{
		mImplementation->mRemoveButton->setClickedCallback(impl::callbackClickRemove, mImplementation);
		mImplementation->mRemoveButton->setEnabled(FALSE);
	}

	mImplementation->mOKButton = 
		getChild<LLButton>("invite_button", recurse);
	if ( mImplementation->mOKButton )
 	{
		mImplementation->mOKButton->setClickedCallback(impl::callbackClickOK, mImplementation);
		mImplementation->mOKButton->setEnabled(FALSE);
 	}

	button = getChild<LLButton>("cancel_button", recurse);
	if ( button )
	{
		button->setClickedCallback(impl::callbackClickCancel, mImplementation);
	}

	mImplementation->mOwnerWarning = getString("confirm_invite_owner_str");
	mImplementation->mAlreadyInGroup = getString("already_in_group");
	mImplementation->mTooManySelected = getString("invite_selection_too_large");

	update();
	
	return (mImplementation->mRoleNames &&
			mImplementation->mInvitees &&
			mImplementation->mRemoveButton);
}
