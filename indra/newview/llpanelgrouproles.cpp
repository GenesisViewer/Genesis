/**
 * @file llpanelgrouproles.cpp
 * @brief Panel for roles information about a particular group.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llcheckboxctrl.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llfiltereditor.h"
#include "llfloatergroupbulkban.h"
#include "llfloatergroupinvite.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelgrouproles.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llfocusmgr.h"

#include "roles_constants.h"

bool agentCanRemoveFromRole(const LLUUID& group_id,
							const LLUUID& role_id)
{
	return gAgent.hasPowerInGroup(group_id, GP_ROLE_REMOVE_MEMBER);
}

bool agentCanAddToRole(const LLUUID& group_id,
					   const LLUUID& role_id)
{
	if (gAgent.isGodlike())
		return true;
    
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
	if (!gdatap) 
	{
		LL_WARNS() << "agentCanAddToRole "
				<< "-- No group data!" << LL_ENDL;
		return false;
	}

	//make sure the agent is in the group
	LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(gAgent.getID());
	if (mi == gdatap->mMembers.end())
	{
		return false;
	}
	
	LLGroupMemberData* member_data = (*mi).second;

	// Owners can add to any role.
	if ( member_data->isInRole(gdatap->mOwnerRole) )
	{
		return true;
	}

	// 'Limited assign members' can add to roles the user is in.
	if ( gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER_LIMITED) &&
			member_data->isInRole(role_id) )
	{
		return true;
	}

	// 'assign members' can add to non-owner roles.
	if ( gAgent.hasPowerInGroup(group_id, GP_ROLE_ASSIGN_MEMBER) &&
			 role_id != gdatap->mOwnerRole )
	{
		return true;
	}

	return false;
}


// LLPanelGroupRoles /////////////////////////////////////////////////////

// static
void* LLPanelGroupRoles::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupRoles("panel group roles", *group_id);
}

LLPanelGroupRoles::LLPanelGroupRoles(const std::string& name, const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mCurrentTab(NULL),
	mRequestedTab( NULL ),
	mSubTabContainer( NULL ),
	mFirstUse( TRUE )
{
}

LLPanelGroupRoles::~LLPanelGroupRoles()
{
	for (S32 i = 0; i < mSubTabContainer->getTabCount(); ++i)
	{
		LLPanelGroupSubTab* subtabp = (LLPanelGroupSubTab*) mSubTabContainer->getPanelByIndex(i);

		subtabp->removeObserver(this);
	}
}

BOOL LLPanelGroupRoles::postBuild()
{
	LL_DEBUGS() << "LLPanelGroupRoles::postBuild()" << LL_ENDL;

	mSubTabContainer = getChild<LLTabContainer>("roles_tab_container");

	if (!mSubTabContainer) return FALSE;

	// Hook up each sub-tabs callback and widgets.
	for (S32 i = 0; i < mSubTabContainer->getTabCount(); ++i)
	{
		LLPanel* panel = mSubTabContainer->getPanelByIndex(i);
		LLPanelGroupSubTab* subtabp = dynamic_cast<LLPanelGroupSubTab*>(panel);
		if (!subtabp)
		{
			LL_WARNS() << "Invalid subtab panel: " << panel->getName() << LL_ENDL;
			return FALSE;
		}

		// Hand the subtab a pointer to this LLPanelGroupRoles, so that it can
		// look around for the widgets it is interested in.
		if (!subtabp->postBuildSubTab(this))
			return FALSE;

		subtabp->addObserver(this);
	}
	// Add click callbacks to tab switching.
	mSubTabContainer->setValidateBeforeCommit(boost::bind(&LLPanelGroupRoles::handleSubTabSwitch, this, _1));

	// Set the current tab to whatever is currently being shown.
	mCurrentTab = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!mCurrentTab)
	{
		// Need to select a tab.
		mSubTabContainer->selectFirstTab();
		mCurrentTab = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	}

	if (!mCurrentTab) return FALSE;

	// Act as though this tab was just activated.
	mCurrentTab->activate();

	// Read apply text from the xml file.
	mDefaultNeedsApplyMesg = getString("default_needs_apply_text");
	mWantApplyMesg = getString("want_apply_text");

	return LLPanelGroupTab::postBuild();
}

BOOL LLPanelGroupRoles::isVisibleByAgent(LLAgent* agentp)
{
	/* This power was removed to make group roles simpler
	return agentp->hasPowerInGroup(mGroupID, 
								   GP_ROLE_CREATE |
								   GP_ROLE_DELETE |
								   GP_ROLE_PROPERTIES |
								   GP_ROLE_VIEW |
								   GP_ROLE_ASSIGN_MEMBER |
								   GP_ROLE_REMOVE_MEMBER |
								   GP_ROLE_CHANGE_ACTIONS |
								   GP_MEMBER_INVITE |
								   GP_MEMBER_EJECT |
								   GP_MEMBER_OPTIONS );
	*/
	return agentp->isInGroup(mGroupID);
								   
}

bool LLPanelGroupRoles::handleSubTabSwitch(const LLSD& data)
{
	std::string panel_name = data.asString();
	
	if(mRequestedTab != NULL)//we already have tab change request
	{
		return false;
	}

	mRequestedTab = static_cast<LLPanelGroupTab*>(mSubTabContainer->getPanelByName(panel_name));

	std::string mesg;
	if (mCurrentTab && mCurrentTab->needsApply(mesg))
	{
		// If no message was provided, give a generic one.
		if (mesg.empty())
		{
			mesg = mDefaultNeedsApplyMesg;
		}
		// Create a notify box, telling the user about the unapplied tab.
		LLSD args;
		args["NEEDS_APPLY_MESSAGE"] = mesg;
		args["WANT_APPLY_MESSAGE"] = mWantApplyMesg;
		LLNotificationsUtil::add("PanelGroupApply", args, LLSD(),
			boost::bind(&LLPanelGroupRoles::handleNotifyCallback, this, _1, _2));
		mHasModal = TRUE;
		
		// Returning FALSE will block a close action from finishing until
		// we get a response back from the user.
		return false;
	}

	transitionToTab();
	return true;
}

void LLPanelGroupRoles::transitionToTab()
{
	// Tell the current panel that it is being deactivated.
	if (mCurrentTab)
	{
		mCurrentTab->deactivate();
	}
	
	// Tell the new panel that it is being activated.
	if (mRequestedTab)
	{
		// This is now the current tab;
		mCurrentTab = mRequestedTab;
		mCurrentTab->activate();
		mRequestedTab = 0;
	}
}

bool LLPanelGroupRoles::handleNotifyCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	mHasModal = FALSE;
	LLPanelGroupTab* transition_tab = mRequestedTab;
	switch (option)
	{
	case 0: // "Apply Changes"
	{
		// Try to apply changes, and switch to the requested tab.
		std::string apply_mesg;
		if ( !apply( apply_mesg ) )
		{
			// There was a problem doing the apply.
			if ( !apply_mesg.empty() )
			{
				mHasModal = TRUE;
				LLSD args;
				args["MESSAGE"] = apply_mesg;
				LLNotificationsUtil::add("GenericAlert", args, LLSD(), boost::bind(&LLPanelGroupRoles::onModalClose, this, _1, _2));
			}
			// Skip switching tabs.
			break;
		}
		transitionToTab();
		mSubTabContainer->selectTabPanel( transition_tab );
		
		break;
	}
	case 1: // "Ignore Changes"
		// Switch to the requested panel without applying changes
		cancel();
		transitionToTab();
		mSubTabContainer->selectTabPanel( transition_tab );
		break;
	case 2: // "Cancel"
	default:
		mRequestedTab = NULL;
		// Do nothing.  The user is canceling the action.
		break;
	}
	return false;
}

bool LLPanelGroupRoles::onModalClose(const LLSD& notification, const LLSD& response)
{
	mHasModal = FALSE;
	return false;
}

bool LLPanelGroupRoles::apply(std::string& mesg)
{
	// Pass this along to the currently visible sub tab.
	if (!mSubTabContainer) return false;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return false;
	
	// Ignore the needs apply message.
	std::string ignore_mesg;
	if ( !panelp->needsApply(ignore_mesg) )
	{
		// We don't need to apply anything.
		// We're done.
		return true;
	}

	// Try to do the actual apply.
	return panelp->apply(mesg);
}

void LLPanelGroupRoles::cancel()
{
	// Pass this along to the currently visible sub tab.
	if (!mSubTabContainer) return;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return;

	panelp->cancel();
}

// Pass all of these messages to the currently visible sub tab.
std::string LLPanelGroupRoles::getHelpText() const
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		return panelp->getHelpText();
	}
	else
	{
		return mHelpText;
	}
}

void LLPanelGroupRoles::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		panelp->update(gc);
	}
	else
	{
		LL_WARNS() << "LLPanelGroupRoles::update() -- No subtab to update!" << LL_ENDL;
	}
}

void LLPanelGroupRoles::activate()
{
	// Start requesting member and role data if needed.
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap || !gdatap->isMemberDataComplete())
	{
		LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mGroupID);
	}

	if (!gdatap || !gdatap->isRoleDataComplete() )
	{
		// Mildly hackish - clear all pending changes
		cancel();

		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
	}

	// Check role-member mapping data.
	if (!gdatap || !gdatap->isRoleMemberDataComplete() )
	{
		LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(mGroupID);
	}

	// Need this to get base group member powers
	if (!gdatap || !gdatap->isGroupPropertiesDataComplete() )
	{
		LLGroupMgr::getInstance()->sendGroupPropertiesRequest(mGroupID);
	}

	mFirstUse = FALSE;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp) panelp->activate();
}

void LLPanelGroupRoles::deactivate()
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (panelp) panelp->deactivate();
}

bool LLPanelGroupRoles::needsApply(std::string& mesg)
{
	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return false;
		
	return panelp->needsApply(mesg);
}

BOOL LLPanelGroupRoles::hasModal()
{
	if (mHasModal) return TRUE;

	LLPanelGroupTab* panelp = (LLPanelGroupTab*) mSubTabContainer->getCurrentPanel();
	if (!panelp) return FALSE;
		
	return panelp->hasModal();
}

// PanelGroupTab observer trigger
void LLPanelGroupRoles::tabChanged()
{
	notifyObservers();
}

void LLPanelGroupRoles::setGroupID(const LLUUID& id)
{
	LLPanelGroupTab::setGroupID(id);

	LLPanelGroupMembersSubTab* group_members_tab = findChild<LLPanelGroupMembersSubTab>("members_sub_tab");
	LLPanelGroupRolesSubTab*  group_roles_tab = findChild<LLPanelGroupRolesSubTab>("roles_sub_tab");
	LLPanelGroupActionsSubTab* group_actions_tab = findChild<LLPanelGroupActionsSubTab>("actions_sub_tab");
	LLPanelGroupBanListSubTab* group_ban_tab = findChild<LLPanelGroupBanListSubTab>("banlist_sub_tab");

	if (group_members_tab) group_members_tab->setGroupID(id);
	if (group_roles_tab) group_roles_tab->setGroupID(id);
	if (group_actions_tab) group_actions_tab->setGroupID(id);
	if (group_ban_tab) group_ban_tab->setGroupID(id);

	LLButton* button = getChild<LLButton>("member_invite");
	if (button)
		button->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_INVITE));

	if (mSubTabContainer)
		mSubTabContainer->selectTab(1);
	group_roles_tab->mFirstOpen = TRUE;
	activate();
}


// LLPanelGroupSubTab ////////////////////////////////////////////////////
LLPanelGroupSubTab::LLPanelGroupSubTab(const std::string& name, const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mHeader(NULL),
	mFooter(NULL),
	mActivated(false),
	mHasGroupBanPower(false),
	mSearchEditor(NULL)
{
}

LLPanelGroupSubTab::~LLPanelGroupSubTab()
{
}

BOOL LLPanelGroupSubTab::postBuildSubTab(LLView* root) 
{ 
	// Get icons for later use.
	mActionIcons.clear();

	if (hasString("power_folder_icon"))
	{
		mActionIcons["folder"] = getString("power_folder_icon");
	}

	if (hasString("power_all_have_icon"))
	{
		mActionIcons["full"] = getString("power_all_have_icon");
	}

	if (hasString("power_partial_icon"))
	{
		mActionIcons["partial"] = getString("power_partial_icon");
	}
	return TRUE; 
}

BOOL LLPanelGroupSubTab::postBuild()
{
	// Hook up the search widgets.
	bool recurse = true;
	mSearchEditor = getChild<LLFilterEditor>("filter_input", recurse);

	if (!mSearchEditor) 
		return FALSE;

	mSearchEditor->setCommitCallback(boost::bind(&LLPanelGroupSubTab::setSearchFilter, this, _2));

	return LLPanelGroupTab::postBuild();
}

void LLPanelGroupSubTab::setGroupID(const LLUUID& id)
{
	LLPanelGroupTab::setGroupID(id);
	if(mSearchEditor)
	{
		mSearchEditor->clear();
		setSearchFilter("");
	}

	mActivated = false;
}

void LLPanelGroupSubTab::setSearchFilter(const std::string& filter)
{
	if(mSearchFilter == filter)
		return;
	mSearchFilter = filter;
	LLStringUtil::toLower(mSearchFilter);
	update(GC_ALL);
}

void LLPanelGroupSubTab::activate()
{
	setOthersVisible(TRUE);
}

void LLPanelGroupSubTab::deactivate()
{
	setOthersVisible(FALSE);
}

void LLPanelGroupSubTab::setOthersVisible(BOOL b)
{
	if (mHeader)
	{
		mHeader->setVisible( b );
	}

	if (mFooter)
	{
		mFooter->setVisible( b );
	}
}

bool LLPanelGroupSubTab::matchesActionSearchFilter(std::string action)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	LLStringUtil::toLower(action);
	std::string::size_type match = action.find(mSearchFilter);

	if (std::string::npos == match)
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

void LLPanelGroupSubTab::buildActionsList(LLScrollListCtrl* ctrl, 
										  U64 allowed_by_some, 
										  U64 allowed_by_all,
										  LLUICtrl::commit_callback_t commit_callback,
										  BOOL show_all,
										  BOOL filter,
										  BOOL is_owner_role)
{
	if (LLGroupMgr::getInstance()->mRoleActionSets.empty())
	{
		LL_WARNS() << "Can't build action list - no actions found." << LL_ENDL;
		return;
	}

	mHasGroupBanPower = false;

	std::vector<LLRoleActionSet*>::iterator ras_it = LLGroupMgr::getInstance()->mRoleActionSets.begin();
	std::vector<LLRoleActionSet*>::iterator ras_end = LLGroupMgr::getInstance()->mRoleActionSets.end();
	for ( ; ras_it != ras_end; ++ras_it)
	{
		buildActionCategory(ctrl,
							allowed_by_some,
							allowed_by_all,
							(*ras_it),
							commit_callback,
							show_all,
							filter,
							is_owner_role);
	}
}

void LLPanelGroupSubTab::buildActionCategory(LLScrollListCtrl* ctrl,
											 U64 allowed_by_some,
											 U64 allowed_by_all,
											 LLRoleActionSet* action_set,
											 LLUICtrl::commit_callback_t commit_callback,
											 BOOL show_all,
											 BOOL filter,
											 BOOL is_owner_role)
{
	LL_DEBUGS() << "Building role list for: " << action_set->mActionSetData->mName << LL_ENDL;
	// See if the allow mask matches anything in this category.
	if (show_all || (allowed_by_some & action_set->mActionSetData->mPowerBit))
	{
		// List all the actions in this category that at least some members have.
		LLSD row;

		row["columns"][0]["column"] = "icon";
		row["columns"][0]["type"] = "icon";
		
		icon_map_t::iterator iter = mActionIcons.find("folder");
		if (iter != mActionIcons.end())
		{
			row["columns"][0]["value"] = (*iter).second;
		}

		row["columns"][1]["column"] = "action";
		row["columns"][1]["value"] = action_set->mActionSetData->mName;
		row["columns"][1]["font-style"] = "BOLD";

		LLScrollListItem* title_row = ctrl->addElement(row, ADD_BOTTOM, action_set->mActionSetData);

		bool category_matches_filter = (filter) ? matchesActionSearchFilter(action_set->mActionSetData->mName) : true;

		std::vector<LLRoleAction*>::iterator ra_it = action_set->mActions.begin();
		std::vector<LLRoleAction*>::iterator ra_end = action_set->mActions.end();

		bool items_match_filter = false;
		BOOL can_change_actions = (!is_owner_role && gAgent.hasPowerInGroup(mGroupID, GP_ROLE_CHANGE_ACTIONS));

		for ( ; ra_it != ra_end; ++ra_it)
		{
			// See if anyone has these action.
			if (!show_all && !(allowed_by_some & (*ra_it)->mPowerBit))
			{
				continue;
			}

			// See if we are filtering out these actions
			// If we aren't using filters, category_matches_filter will be true.
			if (!category_matches_filter
				&& !matchesActionSearchFilter((*ra_it)->mDescription))
			{
				continue;										
			}

			items_match_filter = true;

			// See if everyone has these actions.
			bool show_full_strength = false;
			if ( (allowed_by_some & (*ra_it)->mPowerBit) == (allowed_by_all & (*ra_it)->mPowerBit) )
			{
				show_full_strength = true;
			}

			LLSD row;

			S32 column_index = 0;
			row["columns"][column_index]["column"] = "icon";
			++column_index;

			
			S32 check_box_index = -1;
			if (commit_callback)
			{
				row["columns"][column_index]["column"] = "checkbox";
				row["columns"][column_index]["type"] = "checkbox";
				check_box_index = column_index;
				++column_index;
			}
			else
			{
				if (show_full_strength)
				{
					icon_map_t::iterator iter = mActionIcons.find("full");
					if (iter != mActionIcons.end())
					{
						row["columns"][column_index]["column"] = "checkbox";
						row["columns"][column_index]["type"] = "icon";
						row["columns"][column_index]["value"] = (*iter).second;
						++column_index;
					}
				}
				else
				{
					icon_map_t::iterator iter = mActionIcons.find("partial");
					if (iter != mActionIcons.end())
					{
						row["columns"][column_index]["column"] = "checkbox";
						row["columns"][column_index]["type"] = "icon";
						row["columns"][column_index]["value"] = (*iter).second;
						++column_index;
					}
					row["enabled"] = false;
				}
			}

			row["columns"][column_index]["column"] = "action";
			row["columns"][column_index]["value"] = (*ra_it)->mDescription;
			row["columns"][column_index]["font"] = "SANSSERIF_SMALL";

			if (mHasGroupBanPower)
			{
				// The ban ability is being set. Prevent these abilities from being manipulated
				if ((*ra_it)->mPowerBit == GP_MEMBER_EJECT)
				{
					row["enabled"] = false;
				}
				else if ((*ra_it)->mPowerBit == GP_ROLE_REMOVE_MEMBER)
				{
					row["enabled"] = false;
				}
			}
			else
			{
				/* Singu Note: enabled should not be set on here if it was turned off above for another reason... right? Oh well, we'll find out.
				*/
				// The ban ability is not set. Allow these abilities to be manipulated
				if ((*ra_it)->mPowerBit == GP_MEMBER_EJECT)
				{
					row["enabled"] = true;
				}
				else if ((*ra_it)->mPowerBit == GP_ROLE_REMOVE_MEMBER)
				{
					row["enabled"] = true;
				}
			}

			LLScrollListItem* item = ctrl->addElement(row, ADD_BOTTOM, (*ra_it));

			if (-1 != check_box_index)
			{
				// Extract the checkbox that was created.
				LLScrollListCheck* check_cell = (LLScrollListCheck*) item->getColumn(check_box_index);
				LLCheckBoxCtrl* check = check_cell->getCheckBox();
				check->setEnabled(can_change_actions);
				check->setCommitCallback(commit_callback);
				check->setToolTip( check->getLabel() );

				if (show_all)
				{
					check->setTentative(FALSE);
					if (allowed_by_some & (*ra_it)->mPowerBit)
					{
						check->set(TRUE);
					}
					else
					{
						check->set(FALSE);
					}
				}
				else
				{
					check->set(TRUE);
					if (show_full_strength)
					{
						check->setTentative(FALSE);
					}
					else
					{
						check->setTentative(TRUE);
					}
				}

				// Regardless of whether or not this ability is allowed by all or some, we want to prevent
				// the group managers from accidentally disabling either of the two additional abilities
				// tied with GP_GROUP_BAN_ACCESS.
				if (	(allowed_by_all & GP_GROUP_BAN_ACCESS) == GP_GROUP_BAN_ACCESS ||
					(allowed_by_some & GP_GROUP_BAN_ACCESS) == GP_GROUP_BAN_ACCESS)
				{
					mHasGroupBanPower = true;
				}
			}
		}

		if (!items_match_filter)
		{
			S32 title_index = ctrl->getItemIndex(title_row);
			ctrl->deleteSingleItem(title_index);
		}
	}
}

void LLPanelGroupSubTab::setFooterEnabled(BOOL enable)
{
	if (mFooter)
	{
		mFooter->setAllChildrenEnabled(enable);
	}
}

// LLPanelGroupMembersSubTab /////////////////////////////////////////////
// static
void* LLPanelGroupMembersSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupMembersSubTab("panel group members sub tab", *group_id);
}

LLPanelGroupMembersSubTab::LLPanelGroupMembersSubTab(const std::string& name, const LLUUID& group_id)
: 	LLPanelGroupSubTab(name, group_id),
	mMembersList(NULL),
	mAssignedRolesList(NULL),
	mAllowedActionsList(NULL),
	mChanged(FALSE),
	mPendingMemberUpdate(FALSE),
	mHasMatch(FALSE),
	mNumOwnerAdditions(0)
{
}

LLPanelGroupMembersSubTab::~LLPanelGroupMembersSubTab()
{
	for (avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.begin(); it != mAvatarNameCacheConnections.end(); ++it)
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
	}
	mAvatarNameCacheConnections.clear();
}

BOOL LLPanelGroupMembersSubTab::postBuildSubTab(LLView* root)
{
	LLPanelGroupSubTab::postBuildSubTab(root);
	
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("members_header", recurse);
	mFooter = parent->getChild<LLPanel>("members_footer", recurse);

	mMembersList 		= parent->getChild<LLNameListCtrl>("member_list", recurse);
	mAssignedRolesList	= parent->getChild<LLScrollListCtrl>("member_assigned_roles", recurse);
	mAllowedActionsList	= parent->getChild<LLScrollListCtrl>("member_allowed_actions", recurse);

	if (!mMembersList || !mAssignedRolesList || !mAllowedActionsList) return FALSE;

	// We want to be notified whenever a member is selected.
	mMembersList->setCommitOnSelectionChange(TRUE);
	mMembersList->setCommitCallback(boost::bind(&LLPanelGroupMembersSubTab::onMemberSelect,_1,this));

	LLButton* button = parent->getChild<LLButton>("member_invite", recurse);
	if ( button )
	{
		button->setClickedCallback(boost::bind(&LLPanelGroupMembersSubTab::onInviteMember,this));
		button->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_INVITE));
	}

	mEjectBtn = parent->getChild<LLButton>("member_eject", recurse);
	if ( mEjectBtn )
	{
		mEjectBtn->setClickedCallback(boost::bind(&LLPanelGroupMembersSubTab::onEjectMembers,this));
		mEjectBtn->setEnabled(FALSE);
	}

	mBanBtn = parent->getChild<LLButton>("member_ban", recurse);
	if (mBanBtn)
	{
		mBanBtn->setClickedCallback(boost::bind(&LLPanelGroupMembersSubTab::handleBanMember,this));
		mBanBtn->setEnabled(FALSE);
	}

	return TRUE;
}

void LLPanelGroupMembersSubTab::setGroupID(const LLUUID& id)
{
	//clear members list
	if(mMembersList) mMembersList->deleteAllItems();
	if(mAssignedRolesList) mAssignedRolesList->deleteAllItems();
	if(mAllowedActionsList) mAllowedActionsList->deleteAllItems();

	LLPanelGroupSubTab::setGroupID(id);
}



// static
void LLPanelGroupMembersSubTab::onMemberSelect(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupMembersSubTab* self = static_cast<LLPanelGroupMembersSubTab*>(user_data);
	self->handleMemberSelect();
}

void LLPanelGroupMembersSubTab::handleMemberSelect()
{
	LL_DEBUGS() << "LLPanelGroupMembersSubTab::handleMemberSelect" << LL_ENDL;

	mAssignedRolesList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::handleMemberSelect() "
				<< "-- No group data!" << LL_ENDL;
		return;
	}

	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty()) return;

	// Build a vector of all selected members, and gather allowed actions.
	uuid_vec_t selected_members;
	U64 allowed_by_all = GP_ALL_POWERS; //0xffffffffffffLL;
	U64 allowed_by_some = 0;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin();
		 itor != selection.end(); ++itor)
	{
		LLUUID member_id = (*itor)->getUUID();

		selected_members.push_back( member_id );
		// Get this member's power mask including any unsaved changes

		U64 powers = getAgentPowersBasedOnRoleChanges( member_id );

		allowed_by_all &= powers;
		allowed_by_some |= powers;
	}
	std::sort(selected_members.begin(), selected_members.end());

	//////////////////////////////////
	// Build the allowed actions list.
	//////////////////////////////////
	buildActionsList(mAllowedActionsList,
					 allowed_by_some,
					 allowed_by_all,
					 NULL,
					 FALSE,
					 FALSE,
					 FALSE);

	//////////////////////////////////
	// Build the assigned roles list.
	//////////////////////////////////
	// Add each role to the assigned roles list.
	LLGroupMgrGroupData::role_list_t::iterator iter = gdatap->mRoles.begin();
	LLGroupMgrGroupData::role_list_t::iterator end  = gdatap->mRoles.end();

	BOOL can_ban_members = gAgent.hasPowerInGroup(mGroupID,	GP_GROUP_BAN_ACCESS);
	BOOL can_eject_members = gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_EJECT);
	BOOL member_is_owner = FALSE;
	
	for( ; iter != end; ++iter)
	{
		// Count how many selected users are in this role.
		const LLUUID& role_id = iter->first;
		LLGroupRoleData* group_role_data = iter->second;

		if (group_role_data)
		{
			const BOOL needs_sort = FALSE;
			S32 count = group_role_data->getMembersInRole(
											selected_members, needs_sort);
			//check if the user has permissions to assign/remove
			//members to/from the role (but the ability to add/remove
			//should only be based on the "saved" changes to the role
			//not in the temp/meta data. -jwolk
			BOOL cb_enable = ( (count > 0) ?
							   agentCanRemoveFromRole(mGroupID, role_id) :
							   agentCanAddToRole(mGroupID, role_id) );


			// Owner role has special enabling permissions for removal.
			if (cb_enable && (count > 0) && role_id == gdatap->mOwnerRole)
			{
				// Check if any owners besides this agent are selected.
				uuid_vec_t::const_iterator member_iter;
				uuid_vec_t::const_iterator member_end =
												selected_members.end();
				for (member_iter = selected_members.begin();
					 member_iter != member_end;	
					 ++member_iter)
				{
					// Don't count the agent.
					if ((*member_iter) == gAgent.getID()) continue;
					
					// Look up the member data.
					LLGroupMgrGroupData::member_list_t::iterator mi = 
									gdatap->mMembers.find((*member_iter));
					if (mi == gdatap->mMembers.end()) continue;
					LLGroupMemberData* member_data = (*mi).second;
					// Is the member an owner?
					if ( member_data && member_data->isInRole(gdatap->mOwnerRole) )
					{
						// Can't remove other owners.
						cb_enable = FALSE;
						can_ban_members = FALSE;
						break;
					}
				}
			}

			//now see if there are any role changes for the selected
			//members and remember to include them
			uuid_vec_t::iterator sel_mem_iter = selected_members.begin();
			for (; sel_mem_iter != selected_members.end(); sel_mem_iter++)
			{
				LLRoleMemberChangeType type;
				if ( getRoleChangeType(*sel_mem_iter, role_id, type) )
				{
					if ( type == RMC_ADD ) count++;
					else if ( type == RMC_REMOVE ) count--;
				}
			}
			
			// If anyone selected is in any role besides 'Everyone' then they can't be ejected.
			if (role_id.notNull() && (count > 0))
			{
				can_eject_members = FALSE;
				if (role_id == gdatap->mOwnerRole)
				{
					member_is_owner = TRUE;
				}
			}

			LLRoleData rd;
			if (gdatap->getRoleData(role_id,rd))
			{
				std::ostringstream label;
				label << rd.mRoleName;
				// Don't bother showing a count, if there is only 0 or 1.
				if (count > 1)
				{
					label << ": " << count ;
				}
	
				LLSD row;
				row["id"] = role_id;

				row["columns"][0]["column"] = "checkbox";
				row["columns"][0]["type"] = "checkbox";

				row["columns"][1]["column"] = "role";
				row["columns"][1]["value"] = label.str();

				if (row["id"].asUUID().isNull())
				{
					// This is the everyone role, you can't take people out of the everyone role!
					row["enabled"] = false;
				}

				LLScrollListItem* item = mAssignedRolesList->addElement(row);

				// Extract the checkbox that was created.
				LLScrollListCheck* check_cell = (LLScrollListCheck*) item->getColumn(0);
				LLCheckBoxCtrl* check = check_cell->getCheckBox();
				check->setCommitCallback(onRoleCheck, this);
				check->set( count > 0 );
				check->setTentative(
					(0 != count)
					&& (selected_members.size() !=
						(uuid_vec_t::size_type)count));

				//NOTE: as of right now a user can break the group
				//by removing himself from a role if he is the
				//last owner.  We should check for this special case
				// -jwolk
				check->setEnabled(cb_enable);
				item->setEnabled(cb_enable);
			}
		}
		else
		{
			// This could happen if changes are not synced right on sub-panel change.
			LL_WARNS() << "No group role data for " << iter->second << LL_ENDL;
		}
	}
	mAssignedRolesList->setEnabled(TRUE);

	if (gAgent.isGodlike())
	{
		can_eject_members = TRUE;
		can_ban_members = TRUE;
	}

	if (!can_eject_members && !member_is_owner)
	{
		// Maybe we can eject them because we are an owner...
		LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(gAgent.getID());
		if (mi != gdatap->mMembers.end())
		{
			LLGroupMemberData* member_data = (*mi).second;

			if ( member_data && member_data->isInRole(gdatap->mOwnerRole) )
			{
				can_eject_members = TRUE;
				can_ban_members = TRUE;
			}
		}

	}

	// ... or we can eject them because we have all the requisite powers...
	if (	gAgent.hasPowerInGroup(mGroupID, GP_ROLE_REMOVE_MEMBER) &&
		!member_is_owner)
	{
		if (gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_EJECT))
		{
			can_eject_members = TRUE;
		}

		if (gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
		{
			can_ban_members = TRUE;
		}
	}


	uuid_vec_t::const_iterator member_iter = selected_members.begin();
	uuid_vec_t::const_iterator member_end = selected_members.end();
	for ( ; member_iter != member_end; ++member_iter)
	{
		// Don't count the agent.
		if ((*member_iter) == gAgent.getID())
		{
			can_eject_members = FALSE;
			can_ban_members = FALSE;
		}
	}

	mBanBtn->setEnabled(can_ban_members);
	mEjectBtn->setEnabled(can_eject_members);
}

//static
void LLPanelGroupMembersSubTab::onInviteMember(void *userdata)
{
	LLPanelGroupMembersSubTab* selfp = (LLPanelGroupMembersSubTab*) userdata;

	if ( selfp )
	{
		selfp->handleInviteMember();
	}
}

void LLPanelGroupMembersSubTab::handleInviteMember()
{
	LLFloaterGroupInvite::showForGroup(mGroupID);
}

void LLPanelGroupMembersSubTab::onEjectMembers(void *userdata)
{
	LLPanelGroupMembersSubTab* selfp = (LLPanelGroupMembersSubTab*) userdata;

	if ( selfp )
	{
		selfp->handleEjectMembers();
	}
}

void LLPanelGroupMembersSubTab::handleEjectMembers()
{
	//send down an eject message
	uuid_vec_t selected_members;

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty()) return;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		LLUUID member_id = (*itor)->getUUID();
		selected_members.push_back( member_id );
	}

	mMembersList->deleteSelectedItems();

	sendEjectNotifications(mGroupID, selected_members);

	LLGroupMgr::getInstance()->sendGroupMemberEjects(mGroupID,
									 selected_members);
}

void LLPanelGroupMembersSubTab::sendEjectNotifications(const LLUUID& group_id, const uuid_vec_t& selected_members)
{
	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);

	if (group_data)
	{
		for (uuid_vec_t::const_iterator i = selected_members.begin(); i != selected_members.end(); ++i)
		{
			LLSD args;
			std::string av_name;
			LLAvatarNameCache::getNSName(*i, av_name);
			args["AVATAR_NAME"] = av_name;
			args["GROUP_NAME"] = group_data->mName;

			LLNotifications::instance().add(LLNotification::Params("EjectAvatarFromGroup").substitutions(args));
		}
	}
}

void LLPanelGroupMembersSubTab::handleRoleCheck(const LLUUID& role_id,
												LLRoleMemberChangeType type)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) return;

	//add that the user is requesting to change the roles for selected
	//members
	U64 powers_all_have  = GP_ALL_POWERS;
	U64 powers_some_have = 0;

	BOOL   is_owner_role = ( gdatap->mOwnerRole == role_id );
	LLUUID member_id;

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty())
	{
		return;
	}
	
	for (std::vector<LLScrollListItem*>::iterator itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		member_id = (*itor)->getUUID();

		//see if we requested a change for this member before
		if ( mMemberRoleChangeData.find(member_id) == mMemberRoleChangeData.end() )
		{
			mMemberRoleChangeData[member_id] = new role_change_data_map_t;
		}
		role_change_data_map_t* role_change_datap = mMemberRoleChangeData[member_id];

		//now check to see if the selected group member
		//had changed his association with the selected role before

		role_change_data_map_t::iterator  role = role_change_datap->find(role_id);
		if ( role != role_change_datap->end() )
		{
			//see if the new change type cancels out the previous change
			if (role->second != type)
			{
				role_change_datap->erase(role_id);
				if ( is_owner_role ) mNumOwnerAdditions--;
			}
			//else do nothing

			if ( role_change_datap->empty() )
			{
				//the current member now has no role changes
				//so erase the role change and erase the member's entry
				delete role_change_datap;
                role_change_datap = NULL;

				mMemberRoleChangeData.erase(member_id);
			}
		}
		else
		{
			//a previously unchanged role is being changed
			(*role_change_datap)[role_id] = type;
			if ( is_owner_role && type == RMC_ADD ) mNumOwnerAdditions++;
		}

		//we need to calculate what powers the selected members
		//have (including the role changes we're making)
		//so that we can rebuild the action list
		U64 new_powers = getAgentPowersBasedOnRoleChanges(member_id);

		powers_all_have  &= new_powers;
		powers_some_have |= new_powers;
	}

	
	mChanged = !mMemberRoleChangeData.empty();
	notifyObservers();

	//alrighty now we need to update the actions list
	//to reflect the changes
	mAllowedActionsList->deleteAllItems();
	buildActionsList(mAllowedActionsList,
					 powers_some_have,
					 powers_all_have,
					 NULL,
					 FALSE,
					 FALSE,
					 FALSE);
}

// static 
void LLPanelGroupMembersSubTab::onRoleCheck(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupMembersSubTab* self = static_cast<LLPanelGroupMembersSubTab*>(user_data);
	LLCheckBoxCtrl* check_box = static_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check_box || !self) return;

	LLScrollListItem* first_selected =
		self->mAssignedRolesList->getFirstSelected();
	if (first_selected)
	{
		LLUUID role_id = first_selected->getUUID();
		LLRoleMemberChangeType change_type = (check_box->get() ? 
						      RMC_ADD : 
						      RMC_REMOVE);
		
		self->handleRoleCheck(role_id, change_type);
	}
}

void LLPanelGroupMembersSubTab::activate()
{
	LLPanelGroupSubTab::activate();
	if(!mActivated)
	{
		update(GC_ALL);
		mActivated = true;
	}
}

void LLPanelGroupMembersSubTab::deactivate()
{
	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupMembersSubTab::needsApply(std::string& mesg)
{
	return mChanged;
}

void LLPanelGroupMembersSubTab::cancel()
{
	if ( mChanged )
	{
		std::for_each(mMemberRoleChangeData.begin(),
					  mMemberRoleChangeData.end(),
					  DeletePairedPointer());
		mMemberRoleChangeData.clear();

		mChanged = FALSE;
		notifyObservers();
	}
}

bool LLPanelGroupMembersSubTab::apply(std::string& mesg)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS() << "Unable to get group data for group " << mGroupID << LL_ENDL;

		mesg.assign("Unable to save member data.  Try again later.");
		return false;
	}

	if (mChanged)
	{
		//figure out if we are somehow adding an owner or not and alert
		//the user...possibly make it ignorable
		if ( mNumOwnerAdditions > 0 )
		{
			LLRoleData rd;
			LLSD args;

			if ( gdatap->getRoleData(gdatap->mOwnerRole, rd) )
			{
				mHasModal = TRUE;
				args["ROLE_NAME"] = rd.mRoleName;
				LLNotificationsUtil::add("AddGroupOwnerWarning",
										args,
										LLSD(),
										boost::bind(&LLPanelGroupMembersSubTab::addOwnerCB, this, _1, _2));
			}
			else
			{
				LL_WARNS() << "Unable to get role information for the owner role in group " << mGroupID << LL_ENDL;

				mesg.assign("Unable to retried specific group information.  Try again later");
				return false;
			}
				 
		}
		else
		{
			applyMemberChanges();
		}
	}

	return true;
}

bool LLPanelGroupMembersSubTab::addOwnerCB(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	mHasModal = FALSE;

	if (0 == option)
	{
		// User clicked "Yes"
		applyMemberChanges();
	}
	return false;
}

void LLPanelGroupMembersSubTab::applyMemberChanges()
{
	//sucks to do a find again here, but it is in constant time, so, could
	//be worse
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS() << "Unable to get group data for group " << mGroupID << LL_ENDL;
		return;
	}

	//we need to add all of the changed roles data
	//for each member whose role changed
	for (member_role_changes_map_t::iterator member = mMemberRoleChangeData.begin();
		 member != mMemberRoleChangeData.end(); ++member)
	{
		for (role_change_data_map_t::iterator role = member->second->begin();
			 role != member->second->end(); ++role)
		{
			gdatap->changeRoleMember(role->first, //role_id
									 member->first, //member_id
									 role->second); //add/remove
		}

		member->second->clear();
		delete member->second;
	}
	mMemberRoleChangeData.clear();

	LLGroupMgr::getInstance()->sendGroupRoleMemberChanges(mGroupID);	
	//force a UI update
	handleMemberSelect();

	mChanged = FALSE;
	mNumOwnerAdditions = 0;
	notifyObservers();
}

bool LLPanelGroupMembersSubTab::matchesSearchFilter(const std::string& fullname)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	// Create a full name, and compare it to the search filter.
	std::string fullname_lc(fullname);
	LLStringUtil::toLower(fullname_lc);

	std::string::size_type match = fullname_lc.find(mSearchFilter);

	if (std::string::npos == match)
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

U64 LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges(const LLUUID& agent_id)
{
	//we loop over all of the changes
	//if we are adding a role, then we simply add the role's powers
	//if we are removing a role, we store that role id away
	//and then we have to build the powers up bases on the roles the agent
	//is in

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges() -- No group data!" << LL_ENDL;
		return GP_NO_POWERS;
	}

	LLGroupMgrGroupData::member_list_t::iterator iter = gdatap->mMembers.find(agent_id);
	if ( iter == gdatap->mMembers.end() )
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges() -- No member data for member with UUID " << agent_id << LL_ENDL;
		return GP_NO_POWERS;
	}

	LLGroupMemberData* member_data = (*iter).second;
	if (!member_data)
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges() -- Null member data for member with UUID " << agent_id << LL_ENDL;
		return GP_NO_POWERS;
	}

	//see if there are unsaved role changes for this agent
	role_change_data_map_t* role_change_datap = NULL;
	member_role_changes_map_t::iterator member = mMemberRoleChangeData.find(agent_id);
	if ( member != mMemberRoleChangeData.end() )
	{
		//this member has unsaved role changes
		//so grab them
		role_change_datap = (*member).second;
	}

	U64 new_powers = GP_NO_POWERS;

	if ( role_change_datap )
	{
		uuid_vec_t roles_to_be_removed;

		for (role_change_data_map_t::iterator role = role_change_datap->begin();
			 role != role_change_datap->end(); ++ role)
		{
			if ( role->second == RMC_ADD )
			{
				new_powers |= gdatap->getRolePowers(role->first);
			}
			else
			{
				roles_to_be_removed.push_back(role->first);
			}
		}

		//loop over the member's current roles, summing up
		//the powers (not including the role we are removing)
		for (LLGroupMemberData::role_list_t::iterator current_role = member_data->roleBegin();
			 current_role != member_data->roleEnd(); ++current_role)
		{
			bool role_in_remove_list =
				(std::find(roles_to_be_removed.begin(),
						   roles_to_be_removed.end(),
						   current_role->second->getID()) !=
				 roles_to_be_removed.end());

			if ( !role_in_remove_list )
			{
				new_powers |= 
					current_role->second->getRoleData().mRolePowers;
			}
		}
	}
	else
	{
		//there are no changes for this member
		//the member's powers are just the ones stored in the group
		//manager
		new_powers = member_data->getAgentPowers();
	}

	return new_powers;
}

//If there is no change, returns false be sure to verify
//that there is a role change before attempting to get it or else
//the data will make no sense.  Stores the role change type
bool LLPanelGroupMembersSubTab::getRoleChangeType(const LLUUID& member_id,
												  const LLUUID& role_id,
												  LLRoleMemberChangeType& type)
{
	member_role_changes_map_t::iterator member_changes_iter = mMemberRoleChangeData.find(member_id);
	if ( member_changes_iter != mMemberRoleChangeData.end() )
	{
		role_change_data_map_t::iterator role_changes_iter = member_changes_iter->second->find(role_id);
		if ( role_changes_iter != member_changes_iter->second->end() )
		{
			type = role_changes_iter->second;
			return true;
		}
	}

	return false;
}

void LLPanelGroupMembersSubTab::draw()
{
	LLPanelGroupSubTab::draw();

	if (mPendingMemberUpdate)
	{
		updateMembers();
	}
}

void LLPanelGroupMembersSubTab::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	if ( GC_TITLES == gc || GC_PROPERTIES == gc )
	{
		// Don't care about title or general group properties updates.
		return;
	}

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::update() -- No group data!" << LL_ENDL;
		return;
	}

	// Wait for both all data to be retrieved before displaying anything.
	if (   gdatap->isMemberDataComplete() 
		&& gdatap->isRoleDataComplete()
		&& gdatap->isRoleMemberDataComplete())
	{
		mMemberProgress = gdatap->mMembers.begin();
		mPendingMemberUpdate = TRUE;
		mHasMatch = FALSE;
	}
	else
	{
		// Build a string with info on retrieval progress.
		std::ostringstream retrieved;
		if ( !gdatap->isMemberDataComplete() )
		{
			// Still busy retreiving member list.
			retrieved << "Retrieving member list (" << gdatap->mMembers.size()
					  << " / " << gdatap->mMemberCount << ")...";
		}
		else if( !gdatap->isRoleDataComplete() )
		{
			// Still busy retreiving role list.
			retrieved << "Retrieving role list (" << gdatap->mRoles.size()
					  << " / " << gdatap->mRoleCount << ")...";
		}
		else // (!gdatap->isRoleMemberDataComplete())
		{
			// Still busy retreiving role/member mappings.
			retrieved << "Retrieving role member mappings...";
		}
		mMembersList->setEnabled(FALSE);
		mMembersList->setCommentText(retrieved.str());
	}
}

bool is_online_status_string(const std::string& status)
{
	const std::string& localized_online();
	const std::string& localized_unknown();
	return status == localized_online() || status == localized_unknown();
}

void LLPanelGroupMembersSubTab::addMemberToList(LLGroupMemberData* data)
{
	if (!data) return;
	LLUIString donated = getString("donation_area");
	donated.setArg("[AREA]", llformat("%d", data->getContribution()));
	
	LLNameListCtrl::NameItem item_params;
	item_params.value = data->getID();

	item_params.columns.add().column("name").font/*.name*/("SANSSERIF_SMALL").font_style("NORMAL");
	
	item_params.columns.add().column("donated").value(donated.getString())
			.font/*.name*/("SANSSERIF_SMALL").font_style("NORMAL");

	static const LLCachedControl<std::string> format(gSavedSettings, "ShortDateFormat");
	item_params.columns.add().column("online").value(data->getOnlineStatus())
			.format(format).type(is_online_status_string(data->getOnlineStatus()) ? "text" : "date")
			.font/*.name*/("SANSSERIF_SMALL").font_style("NORMAL");
	mMembersList->addNameItemRow(item_params);

	mHasMatch = TRUE;
}

const S32& group_member_name_system()
{
	static const LLCachedControl<S32> name_system("GroupMembersNameSystem", 0);
	return name_system;
}

void LLPanelGroupMembersSubTab::onNameCache(const LLUUID& update_id, LLGroupMemberData* member, const LLAvatarName& av_name, const LLUUID& av_id)
{
	avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(av_id);
	if (it != mAvatarNameCacheConnections.end())
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
		mAvatarNameCacheConnections.erase(it);
	}

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap
		|| gdatap->getMemberVersion() != update_id
		|| !member)
	{
		return;
	}

	// trying to avoid unnecessary hash lookups
	// Singu Note: Diverge from LL Viewer and filter by name displayed
	if (matchesSearchFilter(av_name.getNSName(group_member_name_system())))
	{
		addMemberToList(member);
		if(!mMembersList->getEnabled())
		{
			mMembersList->setEnabled(TRUE);
		}
	}
}

void LLPanelGroupMembersSubTab::updateMembers()
{
	mPendingMemberUpdate = FALSE;

	// Rebuild the members list.

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupMembersSubTab::updateMembers() -- No group data!" << LL_ENDL;
		return;
	}

	// Make sure all data is still complete.  Incomplete data
	// may occur if we refresh.
	if (   !gdatap->isMemberDataComplete() 
		|| !gdatap->isRoleDataComplete()
		|| !gdatap->isRoleMemberDataComplete())
	{
		return;
	}

	//cleanup list only for first iteration
	if(mMemberProgress == gdatap->mMembers.begin())
	{
		mMembersList->deleteAllItems();
	}

	LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();

	LLTimer update_time;
	update_time.setTimerExpirySec(UPDATE_MEMBERS_SECONDS_PER_FRAME);

	for( ; mMemberProgress != end && !update_time.hasExpired(); ++mMemberProgress)
	{
		if (!mMemberProgress->second)
			continue;

		// Do filtering on name if it is already in the cache.
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(mMemberProgress->first, &av_name))
		{
			// Singu Note: Diverge from LL Viewer and filter by name displayed
			if (matchesSearchFilter(av_name.getNSName(group_member_name_system())))
			{
				addMemberToList(mMemberProgress->second);
			}
		}
		else
		{
			// If name is not cached, onNameCache() should be called when it is cached and add this member to list.
			avatar_name_cache_connection_map_t::iterator it = mAvatarNameCacheConnections.find(mMemberProgress->first);
			if (it != mAvatarNameCacheConnections.end())
			{
				if (it->second.connected())
				{
					it->second.disconnect();
				}
				mAvatarNameCacheConnections.erase(it);
			}
			mAvatarNameCacheConnections[mMemberProgress->first] = LLAvatarNameCache::get(mMemberProgress->first, boost::bind(&LLPanelGroupMembersSubTab::onNameCache, this, gdatap->getMemberVersion(), mMemberProgress->second, _2, _1));
		}
	}

	if (mMemberProgress == end)
	{
		if (mHasMatch)
		{
			mMembersList->setEnabled(TRUE);
		}
		else
		{
			mMembersList->setEnabled(FALSE);
			mMembersList->setCommentText(std::string("No match."));
		}
	}
	else
	{
		mPendingMemberUpdate = TRUE;
	}

	// This should clear the other two lists, since nothing is selected.
	handleMemberSelect();
}

void LLPanelGroupMembersSubTab::handleBanMember()
{
	LLGroupMgrGroupData* gdatap	= LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS("Groups") << "Unable to get group data for group " << mGroupID << LL_ENDL;
		return;
	}

	// Singu Note: We have this function, so there's less code here.
	uuid_vec_t ban_ids = mMembersList->getSelectedIDs();
	if (ban_ids.empty())
	{
		return;
	}

	uuid_vec_t::iterator itor;
	for(itor = ban_ids.begin(); itor != ban_ids.end(); ++itor)
	{
		LLGroupBanData ban_data;
		gdatap->createBanEntry(*itor, ban_data);
	}

	LLGroupMgr::getInstance()->sendGroupBanRequest(LLGroupMgr::REQUEST_POST, mGroupID, LLGroupMgr::BAN_CREATE, ban_ids);
	handleEjectMembers();
}


// LLPanelGroupRolesSubTab ///////////////////////////////////////////////
// static
void* LLPanelGroupRolesSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupRolesSubTab("panel group roles sub tab", *group_id);
}

LLPanelGroupRolesSubTab::LLPanelGroupRolesSubTab(const std::string& name, const LLUUID& group_id)
: LLPanelGroupSubTab(name, group_id),
	mRolesList(NULL),
	mAssignedMembersList(NULL),
	mAllowedActionsList(NULL),
	mRoleName(NULL),
	mRoleTitle(NULL),
	mRoleDescription(NULL),
	mMemberVisibleCheck(NULL),
	mDeleteRoleButton(NULL),
	mCreateRoleButton(NULL),
	mFirstOpen(TRUE),
	mHasRoleChange(FALSE)
{
}

LLPanelGroupRolesSubTab::~LLPanelGroupRolesSubTab()
{
}

BOOL LLPanelGroupRolesSubTab::postBuildSubTab(LLView* root)
{
	LLPanelGroupSubTab::postBuildSubTab(root);

	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("roles_header", recurse);
	mFooter = parent->getChild<LLPanel>("roles_footer", recurse);


	mRolesList 		= parent->getChild<LLScrollListCtrl>("role_list", recurse);
	mAssignedMembersList	= parent->getChild<LLNameListCtrl>("role_assigned_members", recurse);
	mAllowedActionsList	= parent->getChild<LLScrollListCtrl>("role_allowed_actions", recurse);

	mRoleName = parent->getChild<LLLineEditor>("role_name", recurse);
	mRoleTitle = parent->getChild<LLLineEditor>("role_title", recurse);
	mRoleDescription = parent->getChild<LLTextEditor>("role_description", recurse);

	mMemberVisibleCheck = parent->getChild<LLCheckBoxCtrl>("role_visible_in_list", recurse);

	if (!mRolesList || !mAssignedMembersList || !mAllowedActionsList
		|| !mRoleName || !mRoleTitle || !mRoleDescription || !mMemberVisibleCheck)
	{
		LL_WARNS() << "ARG! element not found." << LL_ENDL;
		return FALSE;
	}

	mRemoveEveryoneTxt = getString("cant_delete_role");

	mCreateRoleButton = 
		parent->getChild<LLButton>("role_create", recurse);
	if ( mCreateRoleButton )
	{
		mCreateRoleButton->setClickedCallback(boost::bind(&LLPanelGroupRolesSubTab::onCreateRole,this));
		mCreateRoleButton->setEnabled(FALSE);
	}
	
	mDeleteRoleButton =  
		parent->getChild<LLButton>("role_delete", recurse);
	if ( mDeleteRoleButton )
	{
		mDeleteRoleButton->setClickedCallback(boost::bind(&LLPanelGroupRolesSubTab::onDeleteRole,this));
		mDeleteRoleButton->setEnabled(FALSE);
	}

	mRolesList->setCommitOnSelectionChange(TRUE);
	mRolesList->setCommitCallback(boost::bind(&LLPanelGroupRolesSubTab::onRoleSelect,_1,this));

	mMemberVisibleCheck->setCommitCallback(boost::bind(&LLPanelGroupRolesSubTab::onMemberVisibilityChange,_1,this));

	mAllowedActionsList->setCommitOnSelectionChange(TRUE);

	mRoleName->setCommitOnFocusLost(TRUE);
	mRoleName->setKeystrokeCallback(boost::bind(&LLPanelGroupRolesSubTab::onPropertiesKey,_1,this));

	mRoleTitle->setCommitOnFocusLost(TRUE);
	mRoleTitle->setKeystrokeCallback(boost::bind(&LLPanelGroupRolesSubTab::onPropertiesKey,_1,this));

	mRoleDescription->setCommitOnFocusLost(TRUE);
	mRoleDescription->setKeystrokeCallback(boost::bind(&LLPanelGroupRolesSubTab::onDescriptionKeyStroke, this, _1));

	setFooterEnabled(FALSE);

	return TRUE;
}

void LLPanelGroupRolesSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	mRolesList->deselectAllItems();
	mAssignedMembersList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	mRoleName->clear();
	mRoleDescription->clear();
	mRoleTitle->clear();

	setFooterEnabled(FALSE);

	mHasRoleChange = FALSE;
	update(GC_ALL);
}

void LLPanelGroupRolesSubTab::deactivate()
{
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::deactivate()" << LL_ENDL;

	LLPanelGroupSubTab::deactivate();
	mFirstOpen = FALSE;
}

bool LLPanelGroupRolesSubTab::needsApply(std::string& mesg)
{
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::needsApply()" << LL_ENDL;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS() << "Unable to get group data for group " << mGroupID << LL_ENDL;
		return false;
	}


	return (mHasRoleChange								// Text changed in current role
			|| (gdatap && gdatap->pendingRoleChanges()));	// Pending role changes in the group
}

bool LLPanelGroupRolesSubTab::apply(std::string& mesg)
{
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::apply()" << LL_ENDL;

	saveRoleChanges(true);
	mFirstOpen = FALSE;
	LLGroupMgr::getInstance()->sendGroupRoleChanges(mGroupID);

	notifyObservers();

	return true;
}

void LLPanelGroupRolesSubTab::cancel()
{
	mHasRoleChange = FALSE;
	LLGroupMgr::getInstance()->cancelGroupRoleChanges(mGroupID);

	notifyObservers();
}

LLSD LLPanelGroupRolesSubTab::createRoleItem(const LLUUID& role_id, 
								 std::string name, 
								 std::string title, 
								 S32 members)
{
	LLSD row;
	row["id"] = role_id;

	row["columns"][0]["column"] = "name";
	row["columns"][0]["value"] = name;

	row["columns"][1]["column"] = "title";
	row["columns"][1]["value"] = title;

	row["columns"][2]["column"] = "members";
	row["columns"][2]["value"] = members;
	
	return row;
}

bool LLPanelGroupRolesSubTab::matchesSearchFilter(std::string rolename, std::string roletitle)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	LLStringUtil::toLower(rolename);
	LLStringUtil::toLower(roletitle);
	std::string::size_type match_name = rolename.find(mSearchFilter);
	std::string::size_type match_title = roletitle.find(mSearchFilter);

	if ( (std::string::npos == match_name)
		&& (std::string::npos == match_title))
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

void LLPanelGroupRolesSubTab::update(LLGroupChange gc)
{
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::update()" << LL_ENDL;

	if (mGroupID.isNull()) return;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap || !gdatap->isRoleDataComplete())
	{
		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
	}
	else
	{
		bool had_selection = false;
		LLUUID last_selected;
		if (mRolesList->getFirstSelected())
		{
			last_selected = mRolesList->getFirstSelected()->getUUID();
			had_selection = true;
		}
		mRolesList->deleteAllItems();

		LLScrollListItem* item = NULL;

		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.begin();
		LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();

		for ( ; rit != end; ++rit)
		{
			LLRoleData rd;
			if (gdatap->getRoleData((*rit).first,rd))
			{
				if (matchesSearchFilter(rd.mRoleName, rd.mRoleTitle))
				{
					// If this is the everyone role, then EVERYONE is in it.
					S32 members_in_role = (*rit).first.isNull() ? gdatap->mMembers.size() : (*rit).second->getTotalMembersInRole();
					LLSD row = createRoleItem((*rit).first,rd.mRoleName, rd.mRoleTitle, members_in_role);
					item = mRolesList->addElement(row, ((*rit).first.isNull()) ? ADD_TOP : ADD_BOTTOM, this);
					if (had_selection && ((*rit).first == last_selected))
					{
						item->setSelected(TRUE);
					}
				}
			}
			else
			{
				LL_WARNS() << "LLPanelGroupRolesSubTab::update() No role data for role " << (*rit).first << LL_ENDL;
			}
		}

		mRolesList->sortByColumn(std::string("name"), TRUE);

		if ( (gdatap->mRoles.size() < (U32)MAX_ROLES)
			&& gAgent.hasPowerInGroup(mGroupID, GP_ROLE_CREATE) )
		{
			mCreateRoleButton->setEnabled(TRUE);
		}
		else
		{
			mCreateRoleButton->setEnabled(FALSE);
		}

		if (had_selection)
		{
			handleRoleSelect();
		}
		else
		{
			mAssignedMembersList->deleteAllItems();
			mAllowedActionsList->deleteAllItems();
			mRoleName->clear();
			mRoleDescription->clear();
			mRoleTitle->clear();
			setFooterEnabled(FALSE);
			mDeleteRoleButton->setEnabled(FALSE);
		}
	}

	if (!mFirstOpen)
	{
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mGroupID);
		}

		if (!gdatap || !gdatap->isRoleMemberDataComplete())
		{
			LLGroupMgr::getInstance()->sendGroupRoleMembersRequest(mGroupID);
		}
	}

	if ((GC_ROLE_MEMBER_DATA == gc || GC_MEMBER_DATA == gc)
	    && gdatap
	    && gdatap->isMemberDataComplete()
	    && gdatap->isRoleMemberDataComplete())
	{
		buildMembersList();
	}
}

// static
void LLPanelGroupRolesSubTab::onRoleSelect(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self)
		return;

	self->handleRoleSelect();
}

void LLPanelGroupRolesSubTab::handleRoleSelect()
{
	BOOL can_delete = TRUE;
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::handleRoleSelect()" << LL_ENDL;

	mAssignedMembersList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();
	
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << LL_ENDL;
		return;
	}

	saveRoleChanges(false);

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item)
	{
		setFooterEnabled(FALSE);
		return;
	}

	setFooterEnabled(TRUE);

	LLRoleData rd;
	if (gdatap->getRoleData(item->getUUID(),rd))
	{
		BOOL is_owner_role = ( gdatap->mOwnerRole == item->getUUID() );
		mRoleName->setText(rd.mRoleName);
		mRoleTitle->setText(rd.mRoleTitle);
		mRoleDescription->setText(rd.mRoleDescription);

		mAllowedActionsList->setEnabled(gAgent.hasPowerInGroup(mGroupID,
									   GP_ROLE_CHANGE_ACTIONS));
		buildActionsList(mAllowedActionsList,
						 rd.mRolePowers,
						 0LL,
						 boost::bind(&LLPanelGroupRolesSubTab::handleActionCheck, this, _1, false),
						 TRUE,
						 FALSE,
						 is_owner_role);


		mMemberVisibleCheck->set((rd.mRolePowers & GP_MEMBER_VISIBLE_IN_DIR) == GP_MEMBER_VISIBLE_IN_DIR);
		mRoleName->setEnabled(!is_owner_role &&
				gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		mRoleTitle->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		mRoleDescription->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
		
		if ( is_owner_role ) 
			{
				// you can't delete the owner role
				can_delete = FALSE;
				// ... or hide members with this role
				mMemberVisibleCheck->setEnabled(FALSE);
			}
		else
			{
				mMemberVisibleCheck->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_ROLE_PROPERTIES));
			}
		
		if (item->getUUID().isNull())
		{
			// Everyone role, can't edit description or name or delete
			mRoleDescription->setEnabled(FALSE);
			mRoleName->setEnabled(FALSE);
			can_delete = FALSE;
		}
	}
	else
	{
		mRolesList->deselectAllItems();
		mAssignedMembersList->deleteAllItems();
		mAllowedActionsList->deleteAllItems();
		mRoleName->clear();
		mRoleDescription->clear();
		mRoleTitle->clear();
		setFooterEnabled(FALSE);

		can_delete = FALSE;
	}
	mSelectedRole = item->getUUID();
	buildMembersList();

	can_delete = can_delete && gAgent.hasPowerInGroup(mGroupID,
													  GP_ROLE_DELETE);
	mDeleteRoleButton->setEnabled(can_delete);
}

void LLPanelGroupRolesSubTab::buildMembersList()
{
	mAssignedMembersList->deleteAllItems();

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << LL_ENDL;
		return;
	}

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item) return;

	if (item->getUUID().isNull())
	{
		// Special cased 'Everyone' role
		LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.begin();
		LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
		for ( ; mit != end; ++mit)
		{
			mAssignedMembersList->addNameItem((*mit).first);
		}
	}
	else
	{
		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.find(item->getUUID());
		if (rit != gdatap->mRoles.end())
		{
			LLGroupRoleData* rdatap = (*rit).second;
			if (rdatap)
			{
				uuid_vec_t::const_iterator mit = rdatap->getMembersBegin();
				uuid_vec_t::const_iterator end = rdatap->getMembersEnd();
				for ( ; mit != end; ++mit)
				{
					mAssignedMembersList->addNameItem((*mit));
				}
			}
		}
	}
}

struct ActionCBData
{
	LLPanelGroupRolesSubTab*	mSelf;
	LLCheckBoxCtrl* 			mCheck;
};

void LLPanelGroupRolesSubTab::handleActionCheck(LLUICtrl* ctrl, bool force)
{
	LLCheckBoxCtrl* check = dynamic_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check)
		return;
	
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::handleActionSelect()" << LL_ENDL;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << LL_ENDL;
		return;
	}

	LLScrollListItem* action_item = mAllowedActionsList->getFirstSelected();
	if (!action_item)
	{
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}
	LLUUID role_id = role_item->getUUID();

	LLRoleAction* rap = (LLRoleAction*)action_item->getUserdata();
	U64 power = rap->mPowerBit;

	bool isEnablingAbility = check->get();
	LLRoleData rd;
	LLSD args;

	if (isEnablingAbility &&
		!force &&
		((GP_ROLE_ASSIGN_MEMBER == power) || (GP_ROLE_CHANGE_ACTIONS == power) ))
	{
		// Uncheck the item, for now.  It will be
		// checked if they click 'Yes', below.
		check->set(FALSE);

		LLRoleData rd;
		LLSD args;

		if ( gdatap->getRoleData(role_id, rd) )
		{
			args["ACTION_NAME"] = rap->mDescription;
			args["ROLE_NAME"] = rd.mRoleName;
			mHasModal = TRUE;
			std::string warning = "AssignDangerousActionWarning";
			if (GP_ROLE_CHANGE_ACTIONS == power)
			{
				warning = "AssignDangerousAbilityWarning";
			}
			LLNotificationsUtil::add(warning, args, LLSD(), boost::bind(&LLPanelGroupRolesSubTab::addActionCB, this, _1, _2, check));
		}
		else
		{
			LL_WARNS() << "Unable to look up role information for role id: "
				<< role_id << LL_ENDL;
		}
	}

	if (GP_GROUP_BAN_ACCESS == power)
	{
		std::string warning = isEnablingAbility ? "AssignBanAbilityWarning" : "RemoveBanAbilityWarning";

		//////////////////////////////////////////////////////////////////////////
		// Get role data for both GP_ROLE_REMOVE_MEMBER and GP_MEMBER_EJECT
		// Add description and role name to LLSD
		// Pop up dialog saying "Yo, you also granted these other abilities when you did this!"
		if (gdatap->getRoleData(role_id, rd))
		{
			args["ACTION_NAME"] = rap->mDescription;
			args["ROLE_NAME"] = rd.mRoleName;
			mHasModal = TRUE;

			std::vector<LLScrollListItem*> all_data = mAllowedActionsList->getAllData();
			std::vector<LLScrollListItem*>::iterator ad_it = all_data.begin();
			std::vector<LLScrollListItem*>::iterator ad_end = all_data.end();
			LLRoleAction* adp;
			for( ; ad_it != ad_end; ++ad_it)
			{
				adp = (LLRoleAction*)(*ad_it)->getUserdata();
				if (adp->mPowerBit == GP_MEMBER_EJECT)
				{
					args["ACTION_NAME_2"] = adp->mDescription;
				}
				else if (adp->mPowerBit == GP_ROLE_REMOVE_MEMBER)
				{
					args["ACTION_NAME_3"] = adp->mDescription;
				}
			}

			LLNotificationsUtil::add(warning, args);
		}
		else
		{
			LL_WARNS() << "Unable to look up role information for role id: "
				<< role_id << LL_ENDL;
		}

		//////////////////////////////////////////////////////////////////////////

		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.find(role_id);
		U64 current_role_powers = GP_NO_POWERS;
		if (rit != gdatap->mRoles.end())
		{
			current_role_powers = ((*rit).second->getRoleData().mRolePowers);
		}

		if (isEnablingAbility)
		{
			power |= (GP_ROLE_REMOVE_MEMBER | GP_MEMBER_EJECT);
			current_role_powers |= power;
		}
		else
		{
			current_role_powers &= ~GP_GROUP_BAN_ACCESS;
		}

		mAllowedActionsList->deleteAllItems();
		buildActionsList(	mAllowedActionsList,
			current_role_powers,
			current_role_powers,
			boost::bind(&LLPanelGroupRolesSubTab::handleActionCheck, this, _1, false),
			TRUE,
			FALSE,
			FALSE);

	}

	//////////////////////////////////////////////////////////////////////////
	// Adding non-specific ability to role
	//////////////////////////////////////////////////////////////////////////
	if (isEnablingAbility)
	{
		gdatap->addRolePower(role_id, power);
	}
	else
	{
		gdatap->removeRolePower(role_id,power);
	}

	mHasRoleChange = TRUE;
	notifyObservers();

}

bool LLPanelGroupRolesSubTab::addActionCB(const LLSD& notification, const LLSD& response, LLCheckBoxCtrl* check)
{
	if (!check) return false;

	mHasModal = FALSE;

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		// User clicked "Yes"
		check->set(TRUE);
		const bool force_add = true;
		handleActionCheck(check, force_add);
	}
	return false;
}

// static
void LLPanelGroupRolesSubTab::onPropertiesKey(LLLineEditor* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->mHasRoleChange = TRUE;
	self->notifyObservers();
}

void LLPanelGroupRolesSubTab::onDescriptionKeyStroke(LLTextEditor* caller)
{
	mHasRoleChange = TRUE;
	notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onDescriptionCommit(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->mHasRoleChange = TRUE;
	self->notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onMemberVisibilityChange(LLUICtrl* ctrl, void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	LLCheckBoxCtrl* check = static_cast<LLCheckBoxCtrl*>(ctrl);
	if (!check || !self) return;

	self->handleMemberVisibilityChange(check->get());
}

void LLPanelGroupRolesSubTab::handleMemberVisibilityChange(bool value)
{
	LL_DEBUGS() << "LLPanelGroupRolesSubTab::handleMemberVisibilityChange()" << LL_ENDL;

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap) 
	{
		LL_WARNS() << "LLPanelGroupRolesSubTab::handleRoleSelect() "
				<< "-- No group data!" << LL_ENDL;
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (value)
	{
		gdatap->addRolePower(role_item->getUUID(),GP_MEMBER_VISIBLE_IN_DIR);
	}
	else
	{
		gdatap->removeRolePower(role_item->getUUID(),GP_MEMBER_VISIBLE_IN_DIR);
	}
}

// static 
void LLPanelGroupRolesSubTab::onCreateRole(void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->handleCreateRole();
}

void LLPanelGroupRolesSubTab::handleCreateRole()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLUUID new_role_id;
	new_role_id.generate();

	LLRoleData rd;
	rd.mRoleName = "New Role";
	gdatap->createRole(new_role_id,rd);

	mRolesList->deselectAllItems(TRUE);
	LLSD row;
	row["id"] = new_role_id;
	row["columns"][0]["column"] = "name";
	row["columns"][0]["value"] = rd.mRoleName;
	mRolesList->addElement(row, ADD_BOTTOM, this);
	mRolesList->selectByID(new_role_id);

	// put focus on name field and select its contents
	if(mRoleName)
	{
		mRoleName->setFocus(TRUE);
		mRoleName->onTabInto();
		gFocusMgr.triggerFocusFlash();
	}

	notifyObservers();
}

// static 
void LLPanelGroupRolesSubTab::onDeleteRole(void* user_data)
{
	LLPanelGroupRolesSubTab* self = static_cast<LLPanelGroupRolesSubTab*>(user_data);
	if (!self) return;

	self->handleDeleteRole();
}

void LLPanelGroupRolesSubTab::handleDeleteRole()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (role_item->getUUID().isNull() || role_item->getUUID() == gdatap->mOwnerRole)
	{
		LLSD args;
		args["MESSAGE"] = mRemoveEveryoneTxt;
		LLNotificationsUtil::add("GenericAlert", args);
		return;
	}

	gdatap->deleteRole(role_item->getUUID());
	mRolesList->deleteSingleItem(mRolesList->getFirstSelectedIndex());
	mRolesList->selectFirstItem();

	notifyObservers();
}

void LLPanelGroupRolesSubTab::saveRoleChanges(bool select_saved_role)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	if (mHasRoleChange)
	{
		LLRoleData rd;
		if (!gdatap->getRoleData(mSelectedRole,rd)) return;

		rd.mRoleName = mRoleName->getText();
		rd.mRoleDescription = mRoleDescription->getText();
		rd.mRoleTitle = mRoleTitle->getText();

		S32 role_members_count = 0;
		if (mSelectedRole.isNull())
		{
			role_members_count = gdatap->mMemberCount;
		}
		else if(LLGroupRoleData* grd = get_ptr_in_map(gdatap->mRoles, mSelectedRole))
		{
			role_members_count = grd->getTotalMembersInRole();
		}

		gdatap->setRoleData(mSelectedRole,rd);

		mRolesList->deleteSingleItem(mRolesList->getItemIndex(mSelectedRole));
		
		LLSD row = createRoleItem(mSelectedRole,rd.mRoleName,rd.mRoleTitle,role_members_count);
		LLScrollListItem* item = mRolesList->addElement(row, ADD_BOTTOM, this);
		item->setSelected(select_saved_role);

		mHasRoleChange = FALSE;
	}
}

void LLPanelGroupRolesSubTab::setGroupID(const LLUUID& id)
{
	if (mRolesList) mRolesList->deleteAllItems();
	if (mAssignedMembersList) mAssignedMembersList->deleteAllItems();
	if (mAllowedActionsList) mAllowedActionsList->deleteAllItems();

	if (mRoleName) mRoleName->clear();
	if (mRoleDescription) mRoleDescription->clear();
	if (mRoleTitle) mRoleTitle->clear();

	mHasRoleChange = FALSE;

	setFooterEnabled(FALSE);

	LLPanelGroupSubTab::setGroupID(id);
}


// LLPanelGroupActionsSubTab /////////////////////////////////////////////
// static
void* LLPanelGroupActionsSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupActionsSubTab("panel group actions sub tab", *group_id);
}

LLPanelGroupActionsSubTab::LLPanelGroupActionsSubTab(const std::string& name, const LLUUID& group_id)
: LLPanelGroupSubTab(name, group_id)
{
}

LLPanelGroupActionsSubTab::~LLPanelGroupActionsSubTab()
{
}

BOOL LLPanelGroupActionsSubTab::postBuildSubTab(LLView* root)
{
	LLPanelGroupSubTab::postBuildSubTab(root);

	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;
	mHeader = parent->getChild<LLPanel>("actions_header", recurse);
	mFooter = parent->getChild<LLPanel>("actions_footer", recurse);

	mActionDescription = parent->getChild<LLTextEditor>("action_description", recurse);

	mActionList = parent->getChild<LLScrollListCtrl>("action_list",recurse);
	mActionRoles = parent->getChild<LLScrollListCtrl>("action_roles",recurse);
	mActionMembers  = parent->getChild<LLNameListCtrl>("action_members",recurse);

	if (!mActionList || !mActionDescription || !mActionRoles || !mActionMembers) return FALSE;

	mActionList->setCommitOnSelectionChange(TRUE);
	mActionList->setCommitCallback(boost::bind(&LLPanelGroupActionsSubTab::handleActionSelect, this));

	update(GC_ALL);

	return TRUE;
}

void LLPanelGroupActionsSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	update(GC_ALL);
}

void LLPanelGroupActionsSubTab::deactivate()
{
	LL_DEBUGS() << "LLPanelGroupActionsSubTab::deactivate()" << LL_ENDL;

	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupActionsSubTab::needsApply(std::string& mesg)
{
	LL_DEBUGS() << "LLPanelGroupActionsSubTab::needsApply()" << LL_ENDL;

	return false;
}

bool LLPanelGroupActionsSubTab::apply(std::string& mesg)
{
	LL_DEBUGS() << "LLPanelGroupActionsSubTab::apply()" << LL_ENDL;
	return true;
}

void LLPanelGroupActionsSubTab::update(LLGroupChange gc)
{
	LL_DEBUGS() << "LLPanelGroupActionsSubTab::update()" << LL_ENDL;

	if (mGroupID.isNull()) return;

	mActionList->deselectAllItems();
	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();
	mActionDescription->clear();

	mActionList->deleteAllItems();
	buildActionsList(mActionList,
					 GP_ALL_POWERS,
					 GP_ALL_POWERS,
					 NULL,
					 FALSE,
					 TRUE,
					 FALSE);
}

void LLPanelGroupActionsSubTab::handleActionSelect()
{
	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();

	U64 power_mask = GP_NO_POWERS;
	std::vector<LLScrollListItem*> selection = 
							mActionList->getAllSelected();
	if (selection.empty()) return;

	LLRoleAction* rap;

	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = selection.begin() ; 
		 itor != selection.end(); ++itor)
	{
		rap = (LLRoleAction*)( (*itor)->getUserdata() );
		power_mask |= rap->mPowerBit;
	}

	if (selection.size() == 1)
	{
		LLScrollListItem* item = selection[0];
		rap = (LLRoleAction*)(item->getUserdata());

		if (rap->mLongDescription.empty())
		{
			mActionDescription->setText(rap->mDescription);
		}
		else
		{
			mActionDescription->setText(rap->mLongDescription);
		}
	}
	else
	{
		mActionDescription->clear();
	}

	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);

	if (!gdatap) return;

	if (gdatap->isMemberDataComplete())
	{
		LLGroupMgrGroupData::member_list_t::iterator it = gdatap->mMembers.begin();
		LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
		LLGroupMemberData* gmd;

		for ( ; it != end; ++it)
		{
			gmd = (*it).second;
			if (!gmd) continue;
			if ((gmd->getAgentPowers() & power_mask) == power_mask)
			{
				mActionMembers->addNameItem(gmd->getID());
			}
		}
	}
	else
	{
		LLGroupMgr::getInstance()->sendCapGroupMembersRequest(mGroupID);
	}

	if (gdatap->isRoleDataComplete())
	{
		LLGroupMgrGroupData::role_list_t::iterator it = gdatap->mRoles.begin();
		LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();
		LLGroupRoleData* rmd;

		for ( ; it != end; ++it)
		{
			rmd = (*it).second;
			if (!rmd) continue;
			if ((rmd->getRoleData().mRolePowers & power_mask) == power_mask)
			{
				mActionRoles->addSimpleElement(rmd->getRoleData().mRoleName);
			}
		}
	}
	else
	{
		LLGroupMgr::getInstance()->sendGroupRoleDataRequest(mGroupID);
	}
}

void LLPanelGroupActionsSubTab::setGroupID(const LLUUID& id)
{
	if (mActionList) mActionList->deleteAllItems();
	if (mActionRoles) mActionRoles->deleteAllItems();
	if (mActionMembers) mActionMembers->deleteAllItems();

	if (mActionDescription) mActionDescription->clear();

	LLPanelGroupSubTab::setGroupID(id);
}

// LLPanelGroupBanListSubTab /////////////////////////////////////////////
// static
void* LLPanelGroupBanListSubTab::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupBanListSubTab("panel group ban list sub tab", *group_id);
}

LLPanelGroupBanListSubTab::LLPanelGroupBanListSubTab(const std::string& name, const LLUUID& group_id)
	: LLPanelGroupSubTab(name, group_id),
	mBanList(NULL),
	mCreateBanButton(NULL),
	mDeleteBanButton(NULL)
{}

BOOL LLPanelGroupBanListSubTab::postBuildSubTab(LLView* root)
{
	LLPanelGroupSubTab::postBuildSubTab(root);

	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*)root;

	// Look recursively from the parent to find all our widgets.
	bool recurse = true;

	mHeader	= parent->getChild<LLPanel>("banlist_header", recurse);
	mFooter	= parent->getChild<LLPanel>("banlist_footer", recurse);

	mBanList = parent->getChild<LLNameListCtrl>("ban_list", recurse);

	mCreateBanButton		= parent->getChild<LLButton>("ban_create", recurse);
	mDeleteBanButton		= parent->getChild<LLButton>("ban_delete", recurse);
	mRefreshBanListButton	= parent->getChild<LLButton>("ban_refresh", recurse);

	if (!mBanList || !mCreateBanButton || !mDeleteBanButton || !mRefreshBanListButton)
		return FALSE;

	mBanList->setCommitOnSelectionChange(TRUE);
	mBanList->setCommitCallback(boost::bind(&LLPanelGroupBanListSubTab::handleBanEntrySelect, this));

	mCreateBanButton->setCommitCallback(boost::bind(&LLPanelGroupBanListSubTab::handleCreateBanEntry, this));
	mCreateBanButton->setEnabled(FALSE);

	mDeleteBanButton->setCommitCallback(boost::bind(&LLPanelGroupBanListSubTab::handleDeleteBanEntry, this));
	mDeleteBanButton->setEnabled(FALSE);

	mRefreshBanListButton->setCommitCallback(boost::bind(&LLPanelGroupBanListSubTab::handleRefreshBanList, this));
	mRefreshBanListButton->setEnabled(FALSE);

	mBanList->setOnNameListCompleteCallback(boost::bind(&LLPanelGroupBanListSubTab::onBanListCompleted, this, _1));

	populateBanList();

	//setFooterEnabled(FALSE); // Singu Note: This probably serves no purpose upstream, but for us, we need the footer enabled because we use it to make use of this entire panel.
	return TRUE;
}

void LLPanelGroupBanListSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	mBanList->deselectAllItems();
	mDeleteBanButton->setEnabled(FALSE);

	mCreateBanButton->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS));

	// BAKER: Should I really request everytime activate() is called?
	//		  Perhaps I should only do it on a force refresh, or if an action on the list happens...
	//		  Because it's not going to live-update the list anyway... You'd have to refresh if you
	//		  wanted to see someone else's additions anyway...
	//
	LLGroupMgr::getInstance()->sendGroupBanRequest(LLGroupMgr::REQUEST_GET, mGroupID);

	//setFooterEnabled(FALSE); // Singu Note: This probably serves no purpose upstream, but for us, we need the footer enabled because we use it to make use of this entire panel.
	update(GC_ALL);
}

void LLPanelGroupBanListSubTab::update(LLGroupChange gc)
{
	populateBanList();
}

void LLPanelGroupBanListSubTab::draw()
{
	LLPanelGroupSubTab::draw();

	// BAKER: Might be good to put it here instead of update, maybe.. See how often draw gets hit.
	//if(
	//	populateBanList();
}

void LLPanelGroupBanListSubTab::handleBanEntrySelect()
{
	if (gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
	{
		mDeleteBanButton->setEnabled(!!mBanList->getFirstSelected()); // Singu Note: Avoid empty selection.
	}
}

void LLPanelGroupBanListSubTab::handleCreateBanEntry()
{
	LLFloaterGroupBulkBan::showForGroup(mGroupID);
	//populateBanList();
}

void LLPanelGroupBanListSubTab::handleDeleteBanEntry()
{
	LLGroupMgrGroupData* gdatap	= LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS("Groups") << "Unable to get group data for group " << mGroupID << LL_ENDL;
		return;
	}

	// Singu Note: We have this function, so there's less code here.
	uuid_vec_t ban_ids = mBanList->getSelectedIDs();
	if (ban_ids.empty())
	{
		return;
	}

	bool can_ban_members = false;
	if (gAgent.isGodlike() ||
		gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
	{
		can_ban_members = true;
	}

	// Owners can ban anyone in the group.
	LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(gAgent.getID());
	if (mi != gdatap->mMembers.end())
	{
		LLGroupMemberData* member_data = (*mi).second;
		if (member_data && member_data->isInRole(gdatap->mOwnerRole))
		{
			can_ban_members = true;
		}
	}

	if (!can_ban_members)
		return;

	uuid_vec_t::iterator itor;
	for(itor = ban_ids.begin(); itor != ban_ids.end(); ++itor)
	{
		LLUUID ban_id = (*itor);

		gdatap->removeBanEntry(ban_id);
		mBanList->removeNameItem(ban_id);

	}
	// Removing an item removes the selection, we shouldn't be able to click the button anymore until we reselect another entry.
	mDeleteBanButton->setEnabled(FALSE);

	LLGroupMgr::getInstance()->sendGroupBanRequest(LLGroupMgr::REQUEST_POST, mGroupID, LLGroupMgr::BAN_DELETE, ban_ids);
}

void LLPanelGroupBanListSubTab::handleRefreshBanList()
{
	mRefreshBanListButton->setEnabled(FALSE);
	LLGroupMgr::getInstance()->sendGroupBanRequest(LLGroupMgr::REQUEST_GET, mGroupID);
}

void LLPanelGroupBanListSubTab::onBanListCompleted(bool isComplete)
{
	if (isComplete)
	{
		mRefreshBanListButton->setEnabled(TRUE);
		populateBanList();
	}
}

void LLPanelGroupBanListSubTab::populateBanList()
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupID);
	if (!gdatap)
	{
		LL_WARNS("Groups") << "Unable to get group data for group " << mGroupID << LL_ENDL;
		return;
	}

	mBanList->deleteAllItems();
	std::map<LLUUID,LLGroupBanData>::const_iterator entry = gdatap->mBanList.begin();
	for(; entry != gdatap->mBanList.end(); entry++)
	{
		LLNameListCtrl::NameItem ban_entry;
		ban_entry.value = entry->first;
		LLGroupBanData bd = entry->second;

		ban_entry.columns.add().column("name").font/*.name*/("SANSSERIF_SMALL").font_style("NORMAL");

		// Singu Note: We have special date columns, so our code is unique here
		ban_entry.columns.add().column("ban_date").value(bd.mBanDate).type("date").format("%Y/%m/%d").font/*.name*/("SANSSERIF_SMALL").font_style("NORMAL");

		mBanList->addNameItemRow(ban_entry);
	}

	mRefreshBanListButton->setEnabled(TRUE);
}

void LLPanelGroupBanListSubTab::setGroupID(const LLUUID& id)
{
	if (mBanList)
		mBanList->deleteAllItems();

	//setFooterEnabled(FALSE);
	LLPanelGroupSubTab::setGroupID(id);
}
