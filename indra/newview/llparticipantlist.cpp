/**
 * @file llparticipantlist.cpp
 * @brief LLParticipantList intended to update view(LLAvatarList) according to incoming messages
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llavataractions.h"
#include "llagent.h"
#include "llmutelist.h"
#include "llparticipantlist.h"
#include "llnamelistctrl.h"
#include "llspeakers.h"
#include "lluictrlfactory.h" // Edit: For menu duality
#include "llviewermenu.h" // Edit: For menu duality
#include "llviewerwindow.h"
#include "llvoicechannel.h" // Edit: For menu duality
#include "llworld.h" // Edit: For ghost detection
// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

LLParticipantList::LLParticipantList(LLSpeakerMgr* data_source,
									 bool show_text_chatters) :
	mSpeakerMgr(data_source),
	mAvatarList(nullptr),
	mShowTextChatters(show_text_chatters),
	mValidateSpeakerCallback(NULL)
{
	setMouseOpaque(false);

	/* Singu TODO: Avaline?
	mAvalineUpdater = new LLAvalineUpdater(boost::bind(&LLParticipantList::onAvalineCallerFound, this, _1),
										   boost::bind(&LLParticipantList::onAvalineCallerRemoved, this, _1));*/

	mSpeakerAddListener = new SpeakerAddListener(*this);
	mSpeakerRemoveListener = new SpeakerRemoveListener(*this);
	mSpeakerClearListener = new SpeakerClearListener(*this);
	//mSpeakerModeratorListener = new SpeakerModeratorUpdateListener(*this);
	mSpeakerMuteListener = new SpeakerMuteListener(*this);
	mSpeakerBatchBeginListener = new SpeakerBatchBeginListener(*this);
	mSpeakerBatchEndListener = new SpeakerBatchEndListener(*this);
	mSpeakerSortingUpdateListener = new SpeakerSortingUpdateListener(*this);
	mSpeakerMgr->addListener(mSpeakerAddListener, "add");
	mSpeakerMgr->addListener(mSpeakerRemoveListener, "remove");
	mSpeakerMgr->addListener(mSpeakerClearListener, "clear");
	mSpeakerMgr->addListener(mSpeakerBatchBeginListener, "batch_begin");
	mSpeakerMgr->addListener(mSpeakerBatchEndListener, "batch_end");
	mSpeakerMgr->addListener(mSpeakerSortingUpdateListener, "update_sorting");
	//mSpeakerMgr->addListener(mSpeakerModeratorListener, "update_moderator");
}

void LLParticipantList::setupContextMenu()
{
	if (mSpeakerMgr->getVoiceChannel() == LLVoiceChannelProximal::getInstance())
	{
		static LLMenuGL* menu = LLUICtrlFactory::getInstance()->buildMenu("menu_local_avs.xml", gMenuHolder);
		mAvatarList->setContextMenu(menu);
	}
	else mAvatarList->setContextMenu(LFIDBearer::AVATAR);
}

BOOL LLParticipantList::postBuild()
{
	mAvatarList = getChild<LLNameListCtrl>("speakers_list");

	mAvatarList->sortByColumn(gSavedSettings.getString("FloaterActiveSpeakersSortColumn"), gSavedSettings.getBOOL("FloaterActiveSpeakersSortAscending"));
	mAvatarList->setDoubleClickCallback(boost::bind(&LLParticipantList::onAvatarListDoubleClicked, this));
	mAvatarList->setCommitOnSelectionChange(true);
	mAvatarList->setCommitCallback(boost::bind(&LLParticipantList::handleSpeakerSelect, this));
	mAvatarList->setSortChangedCallback(boost::bind(&LLParticipantList::onSortChanged, this));

	if (LLUICtrl* ctrl = findChild<LLUICtrl>("mute_text_btn"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::toggleMuteText, this));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("mute_btn"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::toggleMuteVoice, this));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("speaker_volume"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::onVolumeChange, this, _2));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("profile_btn"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::onClickProfile, this));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("moderator_allow_voice"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::moderateVoiceParticipant, this, _2));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("moderator_allow_text"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::toggleAllowTextChat, this, _2));
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("moderator_mode"))
		ctrl->setCommitCallback(boost::bind(&LLParticipantList::moderateVoiceAllParticipants, this, _2));

	// update speaker UI
	handleSpeakerSelect();
	setupContextMenu();

	return true;
}

LLParticipantList::~LLParticipantList()
{
	/* Singu TODO?
	mAvatarListDoubleClickConnection.disconnect();
	mAvatarListRefreshConnection.disconnect();
	mAvatarListReturnConnection.disconnect();
	mAvatarListToggleIconsConnection.disconnect();
	*/
}

/*void LLParticipantList::setSpeakingIndicatorsVisible(BOOL visible)
{
	mAvatarList->setSpeakingIndicatorsVisible(visible);
}*/


void LLParticipantList::onAvatarListDoubleClicked()
{
	LLAvatarActions::startIM(mAvatarList->getValue().asUUID());
}

void LLParticipantList::handleSpeakerSelect()
{
	const LLUUID& speaker_id = mAvatarList->getValue().asUUID();
	LLPointer<LLSpeaker> selected_speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (speaker_id.isNull() || selected_speakerp.isNull() || mAvatarList->getNumSelected() != 1)
	{
		// Disable normal controls
		if (LLView* view = findChild<LLView>("mute_btn"))
			view->setEnabled(false);
		if (LLView* view = findChild<LLView>("mute_text_btn"))
			view->setEnabled(false);
		if (LLView* view = findChild<LLView>("speaker_volume"))
			view->setEnabled(false);
		// Hide moderator controls
		if (LLView* view = findChild<LLView>("moderation_mode_panel"))
			view->setVisible(false);
		if (LLView* view = findChild<LLView>("moderator_controls"))
			view->setVisible(false);
		// Clear the name
		if (LLUICtrl* ctrl = findChild<LLUICtrl>("resident_name"))
			ctrl->setValue(LLStringUtil::null);
		return;
	}

	mSpeakerMuteListener->clearDispatchers();

	bool valid_speaker = selected_speakerp->mType == LLSpeaker::SPEAKER_AGENT || selected_speakerp->mType == LLSpeaker::SPEAKER_EXTERNAL;
	bool can_mute = valid_speaker && LLAvatarActions::canBlock(speaker_id);
	bool voice_enabled = valid_speaker && LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->getVoiceEnabled(speaker_id);
	// since setting these values is delayed by a round trip to the Vivox servers
	// update them only when selecting a new speaker or
	// asynchronously when an update arrives
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("moderator_allow_voice"))
	{
		ctrl->setEnabled(voice_enabled);
		ctrl->setValue(!selected_speakerp->mModeratorMutedVoice);
	}
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("moderator_allow_text"))
	{
		ctrl->setEnabled(true);
		ctrl->setValue(!selected_speakerp->mModeratorMutedText);
	}
	if (LLView* view = findChild<LLView>("moderator_controls_label"))
	{
		view->setEnabled(true);
	}
	// update UI for selected participant
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("mute_btn"))
	{
		ctrl->setValue(LLAvatarActions::isVoiceMuted(speaker_id));
		ctrl->setEnabled(can_mute && voice_enabled);
	}
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("mute_text_btn"))
	{
		ctrl->setValue(LLAvatarActions::isBlocked(speaker_id));
		ctrl->setEnabled(can_mute);
	}
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("speaker_volume"))
	{
		// Singu Note: Allow modifying own voice volume during a voice session (Which is valued at half of what it should be)
		ctrl->setValue(gAgentID == speaker_id ? gSavedSettings.getF32("AudioLevelMic")/2 : LLVoiceClient::getInstance()->getUserVolume(speaker_id));
		ctrl->setEnabled(valid_speaker && voice_enabled);
	}
	if (LLView* view = findChild<LLView>("profile_btn"))
	{
		view->setEnabled(selected_speakerp->mType != LLSpeaker::SPEAKER_EXTERNAL);
	}
	// show selected user name in large font
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("resident_name"))
	{
		ctrl->setValue(selected_speakerp->mDisplayName);
	}
	selected_speakerp->addListener(mSpeakerMuteListener);

	//update moderator capabilities
	LLPointer<LLSpeaker> self_speakerp = mSpeakerMgr->findSpeaker(gAgentID);
	if (self_speakerp.isNull()) return;

	if (LLView* view = findChild<LLView>("moderation_mode_panel"))
	{
		view->setVisible(self_speakerp->mIsModerator && mSpeakerMgr->isVoiceActive());
	}
	if (LLView* view = findChild<LLView>("moderator_controls"))
	{
		view->setVisible(self_speakerp->mIsModerator);
	}
}

void LLParticipantList::refreshSpeakers()
{
	if (mUpdateTimer.getElapsedTimeF32() < .5f)
	{
		return;
	}
	mUpdateTimer.reset();
	// store off current selection and scroll state to preserve across list rebuilds
	const S32 scroll_pos = mAvatarList->getScrollInterface()->getScrollPos();

	// decide whether it's ok to resort the list then update the speaker manager appropriately.
	// rapid resorting by activity makes it hard to interact with speakers in the list
	// so we freeze the sorting while the user appears to be interacting with the control.
	// we assume this is the case whenever the mouse pointer is within the active speaker
	// panel and hasn't been motionless for more than a few seconds. see DEV-6655 -MG
	LLRect screen_rect;
	localRectToScreen(getLocalRect(), &screen_rect);
	bool resort_ok = !(screen_rect.pointInRect(gViewerWindow->getCurrentMouseX(), gViewerWindow->getCurrentMouseY()) && gMouseIdleTimer.getElapsedTimeF32() < 5.f);
	mSpeakerMgr->update(resort_ok);

	bool re_sort = false;
	size_t start_pos = llmax(0, scroll_pos - 20);
	size_t end_pos = scroll_pos + mAvatarList->getLinesPerPage() + 20;
	std::vector<LLScrollListItem*> items = mAvatarList->getAllData();
	if (start_pos >= items.size())
	{
		return;
	}
	size_t count = 0;
	for (std::vector<LLScrollListItem*>::iterator item_it = items.begin(); item_it != items.end(); ++item_it)
	{
		
		LLScrollListItem* itemp = (*item_it);
		LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(itemp->getUUID());
		if (speakerp.isNull()) continue;

		++count;

		// Color changes. Only perform for rows that are near or in the viewable area.
		if (count > start_pos && count <= end_pos)
		{
			if (LLScrollListCell* icon_cell = itemp->getColumn(0))
			{
				if (speakerp->mStatus == LLSpeaker::STATUS_MUTED)
				{
					icon_cell->setValue("mute_icon.tga");
					static const LLCachedControl<LLColor4> sAscentMutedColor("AscentMutedColor");
					icon_cell->setColor(speakerp->mModeratorMutedVoice ? /*LLColor4::grey*/sAscentMutedColor : LLColor4(1.f, 71.f / 255.f, 71.f / 255.f, 1.f));
				}
				else
				{
					switch (llmin(2, llfloor((speakerp->mSpeechVolume / LLVoiceClient::OVERDRIVEN_POWER_LEVEL) * 3.f)))
					{
					case 0:
						icon_cell->setValue("icn_active-speakers-dot-lvl0.tga");
						break;
					case 1:
						icon_cell->setValue("icn_active-speakers-dot-lvl1.tga");
						break;
					case 2:
						icon_cell->setValue("icn_active-speakers-dot-lvl2.tga");
						break;
					}
					// non voice speakers have hidden icons, render as transparent
					icon_cell->setColor(speakerp->mStatus > LLSpeaker::STATUS_VOICE_ACTIVE ? LLColor4::transparent : speakerp->mDotColor);
				}
			}
			// update name column
			if (LLScrollListCell* name_cell = itemp->getColumn(1))
			{
				if (speakerp->mStatus == LLSpeaker::STATUS_NOT_IN_CHANNEL)
				{
					// draw inactive speakers in different color
					static const LLCachedControl<LLColor4> sSpeakersInactive(gColors, "SpeakersInactive");
					name_cell->setColor(sSpeakersInactive);
				}
				else
				{
					// <edit>
					bool found = mShowTextChatters || speakerp->mID == gAgentID;
					if (!found)
					for (const LLViewerRegion* regionp : LLWorld::getInstance()->getRegionList())
					{
						// Are they in this sim?
						if (std::find(regionp->mMapAvatarIDs.begin(), regionp->mMapAvatarIDs.end(), speakerp->mID) != regionp->mMapAvatarIDs.end())
						{
							found = true;
							break;
						}
					}
					if (!found)
					{
						static const LLCachedControl<LLColor4> sSpeakersGhost(gColors, "SpeakersGhost");
						name_cell->setColor(sSpeakersGhost);
					}
					else
						// </edit>
					{
						static const LLCachedControl<LLColor4> sDefaultListText(gColors, "DefaultListText");
						name_cell->setColor(sDefaultListText);
					}
				}
			}
		}
		// update name column. Need to update all rows to make name sorting behave correctly.
		if (LLScrollListCell* name_cell = itemp->getColumn(1))
		{
			std::string speaker_name = speakerp->mDisplayName.empty() ? LLCacheName::getDefaultName() : speakerp->mDisplayName;
			if (speakerp->mIsModerator)
				speaker_name += ' ' + getString("moderator_label");
			if (name_cell->getValue().asString() != speaker_name)
			{
				re_sort = true;
				name_cell->setValue(speaker_name);
			}
			static_cast<LLScrollListText*>(name_cell)->setFontStyle(speakerp->mIsModerator ? LLFontGL::BOLD : LLFontGL::NORMAL);
		}
	}

	// we potentially modified the sort order by touching the list items
	if (re_sort)
	{
		mAvatarList->setNeedsSortColumn(1);
	}

	// keep scroll value stable
	mAvatarList->getScrollInterface()->setScrollPos(scroll_pos);
}

void LLParticipantList::setValidateSpeakerCallback(validate_speaker_callback_t cb)
{
	mValidateSpeakerCallback = cb;
}

bool LLParticipantList::onAddItemEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	LLUUID uu_id = event->getValue().asUUID();

	if (mValidateSpeakerCallback && !mValidateSpeakerCallback(uu_id))
	{
		return true;
	}

	addAvatarIDExceptAgent(uu_id);
	return true;
}

bool LLParticipantList::onRemoveItemEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	const S32 pos = mAvatarList->getItemIndex(event->getValue().asUUID());
	if (pos != -1)
	{
		mAvatarList->deleteSingleItem(pos);
	}
	return true;
}

bool LLParticipantList::onClearListEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	mAvatarList->clearRows();
	setupContextMenu();
	return true;
}

bool LLParticipantList::onSpeakerMuteEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	LLPointer<LLSpeaker> speakerp = (LLSpeaker*)event->getSource();
	if (speakerp.isNull()) return false;

	// update UI on confirmation of moderator mutes
	if (event->getValue().asString() == "voice")
	{
		childSetValue("moderator_allow_voice", !speakerp->mModeratorMutedVoice);
	}
	else if (event->getValue().asString() == "text")
	{
		childSetValue("moderator_allow_text", !speakerp->mModeratorMutedText);
	}
	return true;
}

void LLParticipantList::onSpeakerBatchBeginEvent()
{
	mAvatarList->setSortEnabled(false);
}

void LLParticipantList::onSpeakerBatchEndEvent()
{
	mAvatarList->setSortEnabled(true);
}

void LLParticipantList::onSpeakerSortingUpdateEvent()
{
	bool re_sort = false;
	for (auto&& item : mAvatarList->getAllData())
	{
		// update speaking order column
		if (LLScrollListCell* speaking_status_cell = item->getColumn(2))
		{
			LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(item->getUUID());
			if (speakerp)
			{
				re_sort = true;
				std::string sort_index = llformat("%010d", speakerp->mSortIndex);
				// since we are forced to sort by text, encode sort order as string
				// print speaking ordinal in a text-sorting friendly manner
				if (speaking_status_cell->getValue().asString() != sort_index)
				{
					speaking_status_cell->setValue(sort_index);
				}
			}
		}
	}
	if (re_sort)
	{
		mAvatarList->setNeedsSortColumn(2);
	}
}

void LLParticipantList::addAvatarIDExceptAgent(const LLUUID& avatar_id)
{
	//if (mExcludeAgent && gAgent.getID() == avatar_id) return;
	if (mAvatarList->getItemIndex(avatar_id) != -1) return;

	/* Singu TODO: Avaline?
	bool is_avatar = LLVoiceClient::getInstance()->isParticipantAvatar(avatar_id);

	if (is_avatar)
	{
		mAvatarList->getIDs().push_back(avatar_id);
		mAvatarList->setDirty();
	}
	else
	{
		std::string display_name = LLVoiceClient::getInstance()->getDisplayName(avatar_id);
		mAvatarList->addAvalineItem(avatar_id, mSpeakerMgr->getSessionID(), display_name.empty() ? LLTrans::getString("AvatarNameWaiting") : display_name);
		mAvalineUpdater->watchAvalineCaller(avatar_id);
	}*/
	adjustParticipant(avatar_id);
}

void LLParticipantList::adjustParticipant(const LLUUID& speaker_id)
{
	LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull()) return;

	LLNameListItem::Params name_item;
	name_item.value = speaker_id;
	name_item.columns.add(LLScrollListCell::Params().column("icon_speaking_status").type("icon").value("icn_active-speakers-dot-lvl0.tga"));
	const std::string& display_name = LLVoiceClient::getInstance()->getDisplayName(speaker_id);
	name_item.name = display_name;
	name_item.columns.add(LLScrollListCell::Params().column("speaker_name").type("text").value(display_name));
	name_item.columns.add(LLScrollListCell::Params().column("speaking_status").type("text")
			.value(llformat("%010d", speakerp->mSortIndex))); // print speaking ordinal in a text-sorting friendly manner

	mAvatarList->addNameItemRow(name_item);

	// add listener to process moderation changes
	speakerp->addListener(mSpeakerMuteListener);
}

//
// LLParticipantList::SpeakerAddListener
//
bool LLParticipantList::SpeakerAddListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	/**
	 * We need to filter speaking objects. These objects shouldn't appear in the list
	 * @see LLFloaterChat::addChat() in llviewermessage.cpp to get detailed call hierarchy
	 */
	const LLUUID& speaker_id = event->getValue().asUUID();
	LLPointer<LLSpeaker> speaker = mParent.mSpeakerMgr->findSpeaker(speaker_id);
	if(speaker.isNull() || speaker->mType == LLSpeaker::SPEAKER_OBJECT)
	{
		return false;
	}
	return mParent.onAddItemEvent(event, userdata);
}

//
// LLParticipantList::SpeakerRemoveListener
//
bool LLParticipantList::SpeakerRemoveListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	return mParent.onRemoveItemEvent(event, userdata);
}

//
// LLParticipantList::SpeakerClearListener
//
bool LLParticipantList::SpeakerClearListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	return mParent.onClearListEvent(event, userdata);
}

//
// LLParticipantList::SpeakerMuteListener
//
bool LLParticipantList::SpeakerMuteListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	return mParent.onSpeakerMuteEvent(event, userdata);
}

bool LLParticipantList::SpeakerBatchBeginListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	mParent.onSpeakerBatchBeginEvent();
	return true;
}
bool LLParticipantList::SpeakerBatchEndListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	mParent.onSpeakerBatchEndEvent();
	return true;
}

bool LLParticipantList::SpeakerSortingUpdateListener::handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata)
{
	mParent.onSpeakerSortingUpdateEvent();
	return true;
}

// Singu Note: The following functions are actually of the LLParticipantListMenu class, but we haven't married lists with menus yet.
void LLParticipantList::toggleAllowTextChat(const LLSD& userdata)
{
	if (!gAgent.getRegion()) return;
	LLIMSpeakerMgr* mgr = dynamic_cast<LLIMSpeakerMgr*>(mSpeakerMgr);
	if (mgr)
	{
		const LLUUID speaker_id = mAvatarList->getValue();
		mgr->toggleAllowTextChat(speaker_id);
	}
}

void LLParticipantList::toggleMute(const LLSD& userdata, U32 flags)
{
	const LLUUID speaker_id = userdata.asUUID(); //mUUIDs.front();
	BOOL is_muted = LLMuteList::getInstance()->isMuted(speaker_id, flags);
	std::string name;

	//fill in name using voice client's copy of name cache
	LLPointer<LLSpeaker> speakerp = mSpeakerMgr->findSpeaker(speaker_id);
	if (speakerp.isNull())
	{
		LL_WARNS("Speakers") << "Speaker " << speaker_id << " not found" << LL_ENDL;
		return;
	}

	name = speakerp->mDisplayName;

	LLMute::EType mute_type;
	switch (speakerp->mType)
	{
		case LLSpeaker::SPEAKER_AGENT:
			mute_type = LLMute::AGENT;
			break;
		case LLSpeaker::SPEAKER_OBJECT:
			mute_type = LLMute::OBJECT;
			break;
		case LLSpeaker::SPEAKER_EXTERNAL:
		default:
			mute_type = LLMute::EXTERNAL;
			break;
	}
	LLMute mute(speaker_id, name, mute_type);

	if (!is_muted)
	{
		LLMuteList::getInstance()->add(mute, flags);
	}
	else
	{
		LLMuteList::getInstance()->remove(mute, flags);
	}
}

void LLParticipantList::toggleMuteText()
{
	toggleMute(mAvatarList->getValue(), LLMute::flagTextChat);
}

void LLParticipantList::toggleMuteVoice()
{
	toggleMute(mAvatarList->getValue(), LLMute::flagVoiceChat);
}

void LLParticipantList::moderateVoiceParticipant(const LLSD& param)
{
	if (!gAgent.getRegion()) return;
	LLIMSpeakerMgr* mgr = dynamic_cast<LLIMSpeakerMgr*>(mSpeakerMgr);
	if (mgr)
	{
		mgr->moderateVoiceParticipant(mAvatarList->getValue(), param);
	}
}

void LLParticipantList::moderateVoiceAllParticipants(const LLSD& param)
{
	if (!gAgent.getRegion()) return;
	// Singu Note: moderateVoiceAllParticipants ends up flipping the boolean passed to it before the actual post
	if (LLIMSpeakerMgr* speaker_manager = dynamic_cast<LLIMSpeakerMgr*>(mSpeakerMgr))
	{
		if (param.asString() == "unmoderated")
		{
			speaker_manager->moderateVoiceAllParticipants(true);
		}
		else if (param.asString() == "moderated")
		{
			speaker_manager->moderateVoiceAllParticipants(false);
		}
	}
}

// Singu Note: The following callbacks are not found upstream
void LLParticipantList::onVolumeChange(const LLSD& param)
{
	// Singu Note: Allow modifying own voice volume during a voice session (Which is valued at half of what it should be)
	const LLUUID& speaker_id = mAvatarList->getValue().asUUID();
	if (gAgentID == speaker_id)
	{
		gSavedSettings.setF32("AudioLevelMic", param.asFloat()*2);
	}
	else
	{
		LLVoiceClient::getInstance()->setUserVolume(speaker_id, param.asFloat());
	}
}

void LLParticipantList::onClickProfile()
{
// [RLVa:KB] - Checked: 2009-07-10 (RLVa-1.0.0g) | Added: RLVa-1.0.0g
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) return;
// [/RLVa:KB]
	LLAvatarActions::showProfile(mAvatarList->getValue().asUUID());
}

void LLParticipantList::onSortChanged()
{
	gSavedSettings.setString("FloaterActiveSpeakersSortColumn", mAvatarList->getSortColumnName());
	gSavedSettings.setBOOL("FloaterActiveSpeakersSortAscending", mAvatarList->getSortAscending());
}

void LLParticipantList::setVoiceModerationCtrlMode(const bool& moderated_voice)
{
	if (LLUICtrl* voice_moderation_ctrl = findChild<LLUICtrl>("moderation_mode"))
	{
		voice_moderation_ctrl->setValue(moderated_voice ? "moderated" : "unmoderated");
	}
}

