/** 
 * @file llfloateravatarpicker.cpp
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloateravatarpicker.h"

// Viewer includes
#include "llagent.h"
#include "llcallingcard.h"
#include "llfocusmgr.h"
#include "llimview.h"			// for gIMMgr
#include "lltooldraganddrop.h"	// for LLToolDragAndDrop
#include "llviewercontrol.h"
#include "llviewerregion.h"		// getCapability()
#include "llworld.h"
// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a)
#include "rlvactions.h"
#include "rlvhandler.h"
// [/RLVa:KB]

// Linden libraries
#include "llavatarnamecache.h"	// IDEVO
#include "llbutton.h"
#include "llcachename.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"
#include "lldraghandle.h"
#include "message.h"

//#include "llsdserialize.h"

//put it back as a member once the legacy path is out?
static std::map<LLUUID, LLAvatarName> sAvatarNameMap;

LLFloaterAvatarPicker* LLFloaterAvatarPicker::show(select_callback_t callback,
												   BOOL allow_multiple,
												   BOOL closeOnSelect,
												   BOOL skip_agent,
												   const std::string& name,
												   LLView * frustumOrigin)
{
	// *TODO: Use a key to allow this not to be an effective singleton
	LLFloaterAvatarPicker* floater =
		getInstance();
	floater->open();
	
	floater->mSelectionCallback = callback;
	floater->setAllowMultiple(allow_multiple);
	floater->mNearMeListComplete = FALSE;
	floater->mCloseOnSelect = closeOnSelect;
	floater->mExcludeAgentFromSearchResults = skip_agent;
	
	if (!closeOnSelect)
	{
		// Use Select/Close
		std::string select_string = floater->getString("Select");
		std::string close_string = floater->getString("Close");
		floater->getChild<LLButton>("ok_btn")->setLabel(select_string);
		floater->getChild<LLButton>("cancel_btn")->setLabel(close_string);
	}

    if(frustumOrigin)
    {
        floater->mFrustumOrigin = frustumOrigin->getHandle();
    }

	return floater;
}

// Default constructor
LLFloaterAvatarPicker::LLFloaterAvatarPicker()
  : LLFloater(),
	mNumResultsReturned(0),
	mNearMeListComplete(FALSE),
	mCloseOnSelect(FALSE),
    mContextConeOpacity	(0.f),
    mContextConeInAlpha(0.f),
    mContextConeOutAlpha(0.f),
    mContextConeFadeTime(0.f)
{
	mCommitCallbackRegistrar.add("Refresh.FriendList", boost::bind(&LLFloaterAvatarPicker::populateFriend, this));
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_avatar_picker.xml", NULL);

    mContextConeInAlpha = gSavedSettings.getF32("ContextConeInAlpha");
    mContextConeOutAlpha = gSavedSettings.getF32("ContextConeOutAlpha");
    mContextConeFadeTime = gSavedSettings.getF32("ContextConeFadeTime");
}

BOOL LLFloaterAvatarPicker::postBuild()
{
	getChild<LLLineEditor>("Edit")->setKeystrokeCallback(boost::bind(&LLFloaterAvatarPicker::editKeystroke,this,_1));
	getChild<LLLineEditor>("EditUUID")->setKeystrokeCallback(boost::bind(&LLFloaterAvatarPicker::editKeystroke, this,_1));

	childSetAction("Find", boost::bind(&LLFloaterAvatarPicker::onBtnFind, this));
	getChildView("Find")->setEnabled(FALSE);
	childSetAction("Refresh", boost::bind(&LLFloaterAvatarPicker::onBtnRefresh, this));
	getChild<LLUICtrl>("near_me_range")->setCommitCallback(boost::bind(&LLFloaterAvatarPicker::onRangeAdjust, this));

	LLScrollListCtrl* searchresults = getChild<LLScrollListCtrl>("SearchResults");
	searchresults->setDoubleClickCallback( boost::bind(&LLFloaterAvatarPicker::onBtnSelect, this));
	searchresults->setCommitCallback(boost::bind(&LLFloaterAvatarPicker::onList, this));
	getChildView("SearchResults")->setEnabled(FALSE);

	LLScrollListCtrl* nearme = getChild<LLScrollListCtrl>("NearMe");
	nearme->setDoubleClickCallback(boost::bind(&LLFloaterAvatarPicker::onBtnSelect, this));
	nearme->setCommitCallback(boost::bind(&LLFloaterAvatarPicker::onList, this));

	LLScrollListCtrl* friends = getChild<LLScrollListCtrl>("Friends");
	friends->setDoubleClickCallback(boost::bind(&LLFloaterAvatarPicker::onBtnSelect, this));
	getChild<LLUICtrl>("Friends")->setCommitCallback(boost::bind(&LLFloaterAvatarPicker::onList, this));

	childSetAction("ok_btn", boost::bind(&LLFloaterAvatarPicker::onBtnSelect, this));
	getChildView("ok_btn")->setEnabled(FALSE);
	childSetAction("cancel_btn", boost::bind(&LLFloaterAvatarPicker::onBtnClose, this));

	getChild<LLUICtrl>("Edit")->setFocus(TRUE);

	LLPanel* search_panel = getChild<LLPanel>("SearchPanel");
	if (search_panel)
	{
		// Start searching when Return is pressed in the line editor.
		search_panel->setDefaultBtn("Find");
	}

	getChild<LLScrollListCtrl>("SearchResults")->setCommentText(getString("no_results"));

	getChild<LLTabContainer>("ResidentChooserTabs")->setCommitCallback(
		boost::bind(&LLFloaterAvatarPicker::onTabChanged, this));
	
	setAllowMultiple(FALSE);
	
	center();
	
	populateFriend();

	return TRUE;
}

void LLFloaterAvatarPicker::setOkBtnEnableCb(validate_callback_t cb)
{
	mOkButtonValidateSignal.connect(cb);
}

void LLFloaterAvatarPicker::onTabChanged()
{
	getChildView("ok_btn")->setEnabled(isSelectBtnEnabled());
}

// Destroys the object
LLFloaterAvatarPicker::~LLFloaterAvatarPicker()
{
	gFocusMgr.releaseFocusIfNeeded( this );
}

void LLFloaterAvatarPicker::onBtnFind()
{
	find();
}

static void addAvatarUUID(const LLUUID av_id, uuid_vec_t& avatar_ids, std::vector<LLAvatarName>& avatar_names)
{
	if (av_id.notNull())
	{
		avatar_ids.push_back(av_id);

		std::map<LLUUID, LLAvatarName>::iterator iter = sAvatarNameMap.find(av_id);
		if (iter != sAvatarNameMap.end())
		{
			avatar_names.push_back(iter->second);
		}
		else
		{
			// the only case where it isn't in the name map is friends
			// but it should be in the name cache
			LLAvatarName av_name;
			LLAvatarNameCache::get(av_id, &av_name);
			avatar_names.push_back(av_name);
		}
	}
}

static void getSelectedAvatarData(const LLUICtrl* from, uuid_vec_t& avatar_ids, std::vector<LLAvatarName>& avatar_names)
{
	if(const LLScrollListCtrl* list = dynamic_cast<const LLScrollListCtrl*>(from))
	{
		const std::vector<LLScrollListItem*> items = list->getAllSelected();
		for (std::vector<LLScrollListItem*>::const_iterator iter = items.begin(); iter != items.end(); ++iter)
		{
			addAvatarUUID((*iter)->getUUID(), avatar_ids, avatar_names);
		}
	}
	else
		addAvatarUUID(from->getValue().asUUID(), avatar_ids, avatar_names);
}

void LLFloaterAvatarPicker::onBtnSelect()
{

	// If select btn not enabled then do not callback
	if (!isSelectBtnEnabled())
		return;

	if(mSelectionCallback)
	{
		std::string active_panel_name;
		LLUICtrl* list =  NULL;
		LLPanel* active_panel = getChild<LLTabContainer>("ResidentChooserTabs")->getCurrentPanel();
		if(active_panel)
		{
			active_panel_name = active_panel->getName();
		}
		if(active_panel_name == "SearchPanel")
		{
			list = getChild<LLScrollListCtrl>("SearchResults");
		}
		else if(active_panel_name == "NearMePanel")
		{
			list = getChild<LLScrollListCtrl>("NearMe");
		}
		else if (active_panel_name == "FriendsPanel")
		{
			list = getChild<LLScrollListCtrl>("Friends");
		}
		else if(active_panel_name == "KeyPanel")
		{
			list = getChild<LLLineEditor>("EditUUID");
		}

		if(list)
		{
			uuid_vec_t			avatar_ids;
			std::vector<LLAvatarName>	avatar_names;
			getSelectedAvatarData(list, avatar_ids, avatar_names);
			if (mCloseOnSelect) // Singu Note: Close before callback if we get here first, makes potential next dialog floater position correctly
			{
				mCloseOnSelect = FALSE;
				close();
			}
			mSelectionCallback(avatar_ids, avatar_names);
		}
	}
	getChild<LLScrollListCtrl>("SearchResults")->deselectAllItems(TRUE);
	getChild<LLScrollListCtrl>("NearMe")->deselectAllItems(TRUE);
	getChild<LLScrollListCtrl>("Friends")->deselectAllItems(TRUE);
	if (mCloseOnSelect)
	{
		mCloseOnSelect = FALSE;
		close();
	}
}

void LLFloaterAvatarPicker::onBtnRefresh()
{
	getChild<LLScrollListCtrl>("NearMe")->deleteAllItems();
	getChild<LLScrollListCtrl>("NearMe")->setCommentText(getString("searching"));
	mNearMeListComplete = FALSE;
}

void LLFloaterAvatarPicker::onBtnClose()
{
	close();
}

void LLFloaterAvatarPicker::onRangeAdjust()
{
	onBtnRefresh();
}

void LLFloaterAvatarPicker::onList()
{
	getChildView("ok_btn")->setEnabled(isSelectBtnEnabled());

// [RLVa:KB] - Checked: 2010-06-05 (RLVa-1.2.2a) | Modified: RLVa-1.2.0d
	if (rlv_handler_t::isEnabled())
	{
		LLTabContainer* pTabs = getChild<LLTabContainer>("ResidentChooserTabs");
		LLPanel* pNearMePanel = getChild<LLPanel>("NearMePanel");
		RLV_ASSERT( (pTabs) && (pNearMePanel) );
		if ( (pTabs) && (pNearMePanel) )
		{
			bool fRlvEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMETAGS);
			pTabs->enableTabButton(pTabs->getIndexForPanel(pNearMePanel), fRlvEnable);
			if ( (!fRlvEnable) && (pTabs->getCurrentPanel() == pNearMePanel) )
				pTabs->selectTabByName("SearchPanel");
		}
	}
// [/RLVa:KB]
}

void LLFloaterAvatarPicker::populateNearMe()
{
	BOOL all_loaded = TRUE;
	BOOL empty = TRUE;
	LLScrollListCtrl* near_me_scroller = getChild<LLScrollListCtrl>("NearMe");
	near_me_scroller->deleteAllItems();

	uuid_vec_t avatar_ids;
	LLWorld::getInstance()->getAvatars(&avatar_ids, NULL, gAgent.getPositionGlobal(), gSavedSettings.getF32("NearMeRange"));
	for(U32 i=0; i<avatar_ids.size(); i++)
	{
		LLUUID& av = avatar_ids[i];
		if(av == gAgent.getID()) continue;
		LLSD element;
		element["id"] = av; // value
		LLAvatarName av_name;

		if (!LLAvatarNameCache::get(av, &av_name))
		{
			element["columns"][0]["column"] = "name";
			element["columns"][0]["value"] = LLCacheName::getDefaultName();
			all_loaded = FALSE;
		}			
		else
		{
			element["columns"][0]["column"] = "name";
			element["columns"][0]["value"] = av_name.getDisplayName();
			element["columns"][1]["column"] = "username";
			element["columns"][1]["value"] = av_name.getUserName();

			sAvatarNameMap[av] = av_name;
		}
		near_me_scroller->addElement(element);
		empty = FALSE;
	}

	if (empty)
	{
		getChildView("NearMe")->setEnabled(FALSE);
		getChildView("ok_btn")->setEnabled(FALSE);
		near_me_scroller->setCommentText(getString("no_one_near"));
	}
	else 
	{
		getChildView("NearMe")->setEnabled(TRUE);
		getChildView("ok_btn")->setEnabled(TRUE);
		near_me_scroller->selectFirstItem();
		onList();
		near_me_scroller->setFocus(TRUE);
	}

	if (all_loaded)
	{
		mNearMeListComplete = TRUE;
	}
}

void LLFloaterAvatarPicker::populateFriend()
{
	LLScrollListCtrl* friends_scroller = getChild<LLScrollListCtrl>("Friends");
	friends_scroller->deleteAllItems();
	LLCollectAllBuddies collector;
	LLAvatarTracker::instance().applyFunctor(collector);
	LLCollectAllBuddies::buddy_map_t::iterator it;
	
	for(it = collector.mOnline.begin(); it!=collector.mOnline.end(); it++)
	{
		friends_scroller->addStringUUIDItem(it->first, it->second);
	}
	for(it = collector.mOffline.begin(); it!=collector.mOffline.end(); it++)
	{
		friends_scroller->addStringUUIDItem(it->first, it->second);
	}
	friends_scroller->sortByColumnIndex(0, TRUE);
}

void LLFloaterAvatarPicker::drawFrustum()
{
	if (mFrustumOrigin.get())
	{
		LLView * frustumOrigin = mFrustumOrigin.get();
		LLRect origin_rect;
		frustumOrigin->localRectToOtherView(frustumOrigin->getLocalRect(), &origin_rect, this);
		// draw context cone connecting color picker with color swatch in parent floater
		LLRect local_rect = getLocalRect();
		if (hasFocus() && frustumOrigin->isInVisibleChain() && mContextConeOpacity > 0.001f)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLGLEnable<GL_CULL_FACE> clip;
			gGL.begin(LLRender::TRIANGLE_STRIP);
			{
				gGL.color4f(0.f, 0.f, 0.f, mContextConeOutAlpha * mContextConeOpacity);
				gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeInAlpha * mContextConeOpacity);
				gGL.vertex2i(origin_rect.mLeft, origin_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeOutAlpha * mContextConeOpacity);
				gGL.vertex2i(local_rect.mRight, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeInAlpha * mContextConeOpacity);
				gGL.vertex2i(origin_rect.mRight, origin_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeOutAlpha * mContextConeOpacity);
				gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeInAlpha * mContextConeOpacity);
				gGL.vertex2i(origin_rect.mRight, origin_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeOutAlpha * mContextConeOpacity);
				gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeInAlpha * mContextConeOpacity);
				gGL.vertex2i(origin_rect.mLeft, origin_rect.mBottom);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeOutAlpha * mContextConeOpacity);
				gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
				gGL.color4f(0.f, 0.f, 0.f, mContextConeInAlpha * mContextConeOpacity);
				gGL.vertex2i(origin_rect.mLeft, origin_rect.mTop);
			}
			gGL.end();
		}

		if (gFocusMgr.childHasMouseCapture(getDragHandle()))
		{
			mContextConeOpacity = lerp(mContextConeOpacity, gSavedSettings.getF32("PickerContextOpacity"), LLSmoothInterpolation::getInterpolant(mContextConeFadeTime));
		}
		else
		{
			mContextConeOpacity = lerp(mContextConeOpacity, 0.f, LLSmoothInterpolation::getInterpolant(mContextConeFadeTime));
		}
	}
}

void LLFloaterAvatarPicker::draw()
{
	drawFrustum();

	// sometimes it is hard to determine when Select/Ok button should be disabled (see LLAvatarActions::shareWithAvatars).
	// lets check this via mOkButtonValidateSignal callback periodically.
	static LLFrameTimer timer;
	if (timer.hasExpired())
	{
		timer.setTimerExpirySec(0.33f); // three times per second should be enough.

		// simulate list changes.
		onList();
		timer.start();
	}

	LLFloater::draw();
	if (!mNearMeListComplete && getChild<LLTabContainer>("ResidentChooserTabs")->getCurrentPanel() == getChild<LLPanel>("NearMePanel"))
	{
		populateNearMe();
	}
}

BOOL LLFloaterAvatarPicker::visibleItemsSelected() const
{
	LLPanel* active_panel = getChild<LLTabContainer>("ResidentChooserTabs")->getCurrentPanel();

	if(active_panel == getChild<LLPanel>("SearchPanel"))
	{
		return getChild<LLScrollListCtrl>("SearchResults")->getFirstSelectedIndex() >= 0;
	}
	else if(active_panel == getChild<LLPanel>("NearMePanel"))
	{
		return getChild<LLScrollListCtrl>("NearMe")->getFirstSelectedIndex() >= 0;
	}
	else if(active_panel == getChild<LLPanel>("FriendsPanel"))
	{
		return getChild<LLScrollListCtrl>("Friends")->getFirstSelectedIndex() >= 0;
	}
	else if(active_panel == getChild<LLPanel>("KeyPanel"))
	{
		LLUUID specified = getChild<LLLineEditor>("EditUUID")->getValue().asUUID();
		return !specified.isNull();
	}
	return FALSE;
}

class LLAvatarPickerResponder : public LLHTTPClient::ResponderWithCompleted
{
	LOG_CLASS(LLAvatarPickerResponder);
public:
	LLUUID mQueryID;

	LLAvatarPickerResponder(const LLUUID& id) : mQueryID(id) { }

protected:
	/*virtual*/ void httpCompleted()
	{
		//std::ostringstream ss;
		//LLSDSerialize::toPrettyXML(content, ss);
		//LL_INFOS() << ss.str() << LL_ENDL;

		// in case of invalid characters, the avatar picker returns a 400
		// just set it to process so it displays 'not found'
		if (isGoodStatus(mStatus) || mStatus == 400)
		{
			if (LLFloaterAvatarPicker::instanceExists())
			{
				LLFloaterAvatarPicker::getInstance()->processResponse(mQueryID, mContent);
			}
		}
		else
		{
			LL_WARNS() << "avatar picker failed " << dumpResponse() << LL_ENDL;
		}
	}

	/*virtual*/ char const* getName(void) const { return "LLAvatarPickerResponder"; }
};

void LLFloaterAvatarPicker::find()
{
	//clear our stored LLAvatarNames
	sAvatarNameMap.clear();

	std::string text = getChild<LLUICtrl>("Edit")->getValue().asString();

	mQueryID.generate();

	std::string url;
	url.reserve(128); // avoid a memory allocation or two

	LLViewerRegion* region = gAgent.getRegion();
	url = region->getCapability("AvatarPickerSearch");
	// Prefer use of capabilities to search on both SLID and display name
	if (!url.empty())
	{
		// capability urls don't end in '/', but we need one to parse
		// query parameters correctly
		if (url.size() > 0 && url[url.size()-1] != '/')
		{
			url += "/";
		}
		url += "?page_size=100&names=";
		std::replace(text.begin(), text.end(), '.', ' ');
		url += LLURI::escape(text);
		LL_INFOS() << "avatar picker " << url << LL_ENDL;
		LLHTTPClient::get(url, new LLAvatarPickerResponder(mQueryID));
	}
	else
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("AvatarPickerRequest");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->addUUID("QueryID", mQueryID);	// not used right now
		msg->nextBlock("Data");
		msg->addString("Name", text);
		gAgent.sendReliableMessage();
	}

	getChild<LLScrollListCtrl>("SearchResults")->deleteAllItems();
	getChild<LLScrollListCtrl>("SearchResults")->setCommentText(getString("searching"));
	
	getChildView("ok_btn")->setEnabled(FALSE);
	mNumResultsReturned = 0;
}

void LLFloaterAvatarPicker::setAllowMultiple(BOOL allow_multiple)
{
	getChild<LLScrollListCtrl>("SearchResults")->setAllowMultipleSelection(allow_multiple);
	getChild<LLScrollListCtrl>("NearMe")->setAllowMultipleSelection(allow_multiple);
	getChild<LLScrollListCtrl>("Friends")->setAllowMultipleSelection(allow_multiple);
}

LLScrollListCtrl* LLFloaterAvatarPicker::getActiveList()
{
	std::string acvtive_panel_name;
	LLScrollListCtrl* list = NULL;
	LLPanel* active_panel = getChild<LLTabContainer>("ResidentChooserTabs")->getCurrentPanel();
	if(active_panel)
	{
		acvtive_panel_name = active_panel->getName();
	}
	if(acvtive_panel_name == "SearchPanel")
	{
		list = getChild<LLScrollListCtrl>("SearchResults");
	}
	else if(acvtive_panel_name == "NearMePanel")
	{
		list = getChild<LLScrollListCtrl>("NearMe");
	}
	else if (acvtive_panel_name == "FriendsPanel")
	{
		list = getChild<LLScrollListCtrl>("Friends");
	}
	return list;
}

BOOL LLFloaterAvatarPicker::handleDragAndDrop(S32 x, S32 y, MASK mask,
											  BOOL drop, EDragAndDropType cargo_type,
											  void *cargo_data, EAcceptance *accept,
											  std::string& tooltip_msg)
{
	LLScrollListCtrl* list = getActiveList();
	if(list)
	{
		LLRect rc_list;
		LLRect rc_point(x,y,x,y);
		if (localRectToOtherView(rc_point, &rc_list, list))
		{
			// Keep selected only one item
			list->deselectAllItems(TRUE);
			list->selectItemAt(rc_list.mLeft, rc_list.mBottom, mask);
			LLScrollListItem* selection = list->getFirstSelected();
			if (selection)
			{
				LLUUID session_id = LLUUID::null;
				LLUUID dest_agent_id = selection->getUUID();
				std::string avatar_name = selection->getColumn(0)->getValue().asString();
				if (dest_agent_id.notNull() && dest_agent_id != gAgentID)
				{
//					if (drop)
// [RLVa:KB] - Checked: 2011-04-11 (RLVa-1.3.0)
					if ( (drop) && (RlvActions::canStartIM(dest_agent_id)) )
// [/RLVa:KB]
					{
						// Start up IM before give the item
						session_id = gIMMgr->addSession(avatar_name, IM_NOTHING_SPECIAL, dest_agent_id);
					}
					return LLToolDragAndDrop::handleGiveDragAndDrop(dest_agent_id, session_id, drop,
																	cargo_type, cargo_data, accept);
				}
			}
		}
	}
	*accept = ACCEPT_NO;
	return TRUE;
}


void LLFloaterAvatarPicker::openFriendsTab()
{
	LLTabContainer* tab_container = getChild<LLTabContainer>("ResidentChooserTabs");
	if (tab_container == NULL)
	{
		llassert(tab_container != NULL);
		return;
	}

	tab_container->selectTabByName("FriendsPanel");
}

// static 
void LLFloaterAvatarPicker::processAvatarPickerReply(LLMessageSystem* msg, void**)
{
	LLUUID	agent_id;
	LLUUID	query_id;
	LLUUID	avatar_id;
	std::string first_name;
	std::string last_name;

	msg->getUUID("AgentData", "AgentID", agent_id);
	msg->getUUID("AgentData", "QueryID", query_id);

	// Not for us
	if (agent_id != gAgent.getID()) return;

	LLFloaterAvatarPicker* floater = instanceExists() ? getInstance() : NULL;

	// floater is closed or these are not results from our last request
	if (NULL == floater || query_id != floater->mQueryID)
	{
		return;
	}

	LLScrollListCtrl* search_results = floater->getChild<LLScrollListCtrl>("SearchResults");

	// clear "Searching" label on first results
	if (floater->mNumResultsReturned++ == 0)
	{
		search_results->deleteAllItems();
	}

	BOOL found_one = FALSE;
	S32 num_new_rows = msg->getNumberOfBlocks("Data");
	for (S32 i = 0; i < num_new_rows; i++)
	{			
		msg->getUUIDFast(  _PREHASH_Data,_PREHASH_AvatarID,	avatar_id, i);
		msg->getStringFast(_PREHASH_Data,_PREHASH_FirstName, first_name, i);
		msg->getStringFast(_PREHASH_Data,_PREHASH_LastName,	last_name, i);

		if (avatar_id != agent_id || !floater->isExcludeAgentFromSearchResults()) // exclude agent from search results?
		{
			std::string avatar_name;
			if (avatar_id.isNull())
			{
				LLStringUtil::format_map_t map;
				map["[TEXT]"] = floater->getChild<LLUICtrl>("Edit")->getValue().asString();
				avatar_name = floater->getString("not_found", map);
				search_results->setEnabled(FALSE);
				floater->getChildView("ok_btn")->setEnabled(FALSE);
			}
			else
			{
				avatar_name = LLCacheName::buildFullName(first_name, last_name);
				search_results->setEnabled(TRUE);
				found_one = TRUE;

				LLAvatarName av_name;
				av_name.fromString(avatar_name);
				const LLUUID& agent_id = avatar_id;
				sAvatarNameMap[agent_id] = av_name;

			}
			LLSD element;
			element["id"] = avatar_id; // value
			element["columns"][0]["column"] = "name";
			element["columns"][0]["value"] = avatar_name;
			search_results->addElement(element);
		}
	}

	if (found_one)
	{
		floater->getChildView("ok_btn")->setEnabled(TRUE);
		search_results->selectFirstItem();
		floater->onList();
		search_results->setFocus(TRUE);
	}
}

void LLFloaterAvatarPicker::processResponse(const LLUUID& query_id, const LLSD& content)
{
	// Check for out-of-date query
	if (query_id != mQueryID) return;

	LLScrollListCtrl* search_results = getChild<LLScrollListCtrl>("SearchResults");

	LLSD agents = content["agents"];
	if (agents.size() == 0)
	{
		LLStringUtil::format_map_t map;
		map["[TEXT]"] = childGetText("Edit");
		LLSD item;
		item["id"] = LLUUID::null;
		item["columns"][0]["column"] = "name";
		item["columns"][0]["value"] = getString("not_found", map);
		search_results->addElement(item);
		search_results->setEnabled(false);
		getChildView("ok_btn")->setEnabled(false);
		return;
	}

	// clear "Searching" label on first results
	search_results->deleteAllItems();

	LLSD item;
	LLSD::array_const_iterator it = agents.beginArray();
	for ( ; it != agents.endArray(); ++it)
	{
		const LLSD& row = *it;
		if (row["id"].asUUID() != gAgent.getID() || !mExcludeAgentFromSearchResults)
		{
			item["id"] = row["id"];
			LLSD& columns = item["columns"];
			columns[0]["column"] = "name";
			columns[0]["value"] = row["display_name"];
			columns[1]["column"] = "username";
			columns[1]["value"] = row["username"];
			search_results->addElement(item);

			// add the avatar name to our list
			LLAvatarName avatar_name;
			avatar_name.fromLLSD(row);
			sAvatarNameMap[row["id"].asUUID()] = avatar_name;
		}
	}

	getChildView("ok_btn")->setEnabled(true);
	search_results->setEnabled(true);
	search_results->selectFirstItem();
	onList();
	search_results->setFocus(TRUE);
}

void LLFloaterAvatarPicker::editKeystroke(LLLineEditor* caller)
{
	if (caller->getName() == "Edit")
		getChildView("Find")->setEnabled(caller->getText().size() >= 3);
	else
		getChildView("Select")->setEnabled(caller->getValue().asUUID().notNull());
}

// virtual
BOOL LLFloaterAvatarPicker::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		if (getChild<LLUICtrl>("Edit")->hasFocus())
		{
			onBtnFind();
		}
		else
		{
			onBtnSelect();
		}
		return TRUE;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		close();
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

bool LLFloaterAvatarPicker::isSelectBtnEnabled()
{
	bool ret_val = visibleItemsSelected();

	if ( ret_val && mOkButtonValidateSignal.num_slots() )
	{
		std::string active_panel_name;
		LLUICtrl* list =  NULL;
		LLPanel* active_panel = getChild<LLTabContainer>("ResidentChooserTabs")->getCurrentPanel();

		if(active_panel)
		{
			active_panel_name = active_panel->getName();
		}
		if(active_panel_name == "SearchPanel")
		{
			list = getChild<LLScrollListCtrl>("SearchResults");
		}
		else if(active_panel_name == "NearMePanel")
		{
			list = getChild<LLScrollListCtrl>("NearMe");
		}
		else if (active_panel_name == "FriendsPanel")
		{
			list = getChild<LLScrollListCtrl>("Friends");
		}
		else if(active_panel_name == "KeyPanel")
		{
			list = getChild<LLLineEditor>("EditUUID");
		}

		if(list)
		{
			uuid_vec_t avatar_ids;
			std::vector<LLAvatarName> avatar_names;
			getSelectedAvatarData(list, avatar_ids, avatar_names);
			return mOkButtonValidateSignal(avatar_ids);
		}
	}

	return ret_val;
}
