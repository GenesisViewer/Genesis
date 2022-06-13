/** 
 *
 * Copyright (c) 2009-2011, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llviewerprecompiledheaders.h"
#include "llappearancemgr.h"
#include "llattachmentsmgr.h"
#include "llgesturemgr.h"
#include "llnotificationsutil.h"
#include "llviewerobjectlist.h"

#include "rlvhelper.h"
#include "rlvhandler.h"
#include "rlvinventory.h"

#include <boost/algorithm/string.hpp>

// ============================================================================
// RlvCommmand
//

RlvCommand::bhvr_map_t RlvCommand::m_BhvrMap;

// Checked: 2009-12-27 (RLVa-1.1.0k) | Modified: RLVa-1.1.0k
RlvCommand::RlvCommand(const LLUUID& idObj, const std::string& strCommand)
	: m_fValid(false), m_idObj(idObj), m_eBehaviour(RLV_BHVR_UNKNOWN), m_fStrict(false), m_eParamType(RLV_TYPE_UNKNOWN), m_eRet(RLV_RET_UNKNOWN)
{
	if ((m_fValid = parseCommand(strCommand, m_strBehaviour, m_strOption, m_strParam)))
	{
		S32 nTemp = 0;
		if ( ("n" == m_strParam) || ("add" == m_strParam) )
			m_eParamType = RLV_TYPE_ADD;
		else if ( ("y" == m_strParam) || ("rem" == m_strParam) )
			m_eParamType = RLV_TYPE_REMOVE;
		else if (m_strBehaviour == "clear")						// clear is the odd one out so just make it its own type
			m_eParamType = RLV_TYPE_CLEAR;
		else if ("force" == m_strParam)
			m_eParamType = RLV_TYPE_FORCE;
		else if (LLStringUtil::convertToS32(m_strParam, nTemp))	// Assume it's a reply command if we can convert <param> to an S32
			m_eParamType = RLV_TYPE_REPLY;
		else
		{
			m_eParamType = RLV_TYPE_UNKNOWN;
			m_fValid = false;
		}
	}

	if (!m_fValid)
	{
		m_strBehaviour = m_strOption = m_strParam = "";
		return;
	}

	// HACK: all those @*overorreplace synonyms are rather tedious (and error-prone) to deal with so replace them their equivalent
	if ( (RLV_TYPE_FORCE == m_eParamType) && 
		 (m_strBehaviour.length() > 13) && (m_strBehaviour.length() - 13 == m_strBehaviour.rfind("overorreplace")) )
	{
		m_strBehaviour.erase(m_strBehaviour.length() - 13, 13);
	}
	// HACK: all those @addoutfit* synonyms are rather tedious (and error-prone) to deal with so replace them their @attach* equivalent
	if ( (RLV_TYPE_FORCE == m_eParamType) && (0 == m_strBehaviour.find("addoutfit")) )
	{
		m_strBehaviour.replace(0, 9, "attach");
	}
	m_eBehaviour = getBehaviourFromString(m_strBehaviour, &m_fStrict);
}


bool RlvCommand::parseCommand(const std::string& strCommand, std::string& strBehaviour, std::string& strOption, std::string& strParam)
{
	// (See behaviour notes for the command parsing truth table)

	// Format: <behaviour>[:<option>]=<param>
	int idxParam  = strCommand.find('=');
	int idxOption = (idxParam > 0) ? strCommand.find(':') : -1;
	if (idxOption > idxParam - 1)
		idxOption = -1;

	// If <behaviour> is missing it's always an improperly formatted command
	if ( (0 == idxOption) || (0 == idxParam) )
		return false;

	strBehaviour = strCommand.substr(0, (-1 != idxOption) ? idxOption : idxParam);
	strOption = strParam = "";

	// If <param> is missing it's an improperly formatted command
	if ( (-1 == idxParam) || (static_cast<int>(strCommand.length()) - 1 == idxParam) )
	{
		// Unless "<behaviour> == "clear" AND (idxOption == 0)" 
		// OR <behaviour> == "clear" AND (idxParam != 0) [see table above]
		if ( ("clear" == strBehaviour) && ( (!idxOption) || (idxParam) ) )
			return true;
		return false;
	}

	if ( (-1 != idxOption) && (idxOption + 1 != idxParam) )
		strOption = strCommand.substr(idxOption + 1, idxParam - idxOption - 1);
	strParam = strCommand.substr(idxParam + 1);

	return true;
}

// Checked: 2009-12-05 (RLVa-1.1.0h) | Added: RLVa-1.1.0h
ERlvBehaviour RlvCommand::getBehaviourFromString(const std::string& strBhvr, bool* pfStrict /*=NULL*/)
{
	std::string::size_type idxStrict = strBhvr.find("_sec");
	bool fStrict = (std::string::npos != idxStrict) && (idxStrict + 4 == strBhvr.length());
	if (pfStrict)
		*pfStrict = fStrict;

	RLV_ASSERT(m_BhvrMap.size() > 0);
	bhvr_map_t::const_iterator itBhvr = m_BhvrMap.find( (!fStrict) ? strBhvr : strBhvr.substr(0, idxStrict));
	if ( (itBhvr != m_BhvrMap.end()) && ((!fStrict) || (hasStrictVariant(itBhvr->second))) )
		return itBhvr->second;
	return RLV_BHVR_UNKNOWN;
}

// Checked: 2010-12-11 (RLVa-1.2.2c) | Added: RLVa-1.2.2c
bool RlvCommand::getCommands(bhvr_map_t& cmdList, const std::string &strMatch)
{
	if (strMatch.empty())
		return false;
	cmdList.clear();

	RLV_ASSERT(m_BhvrMap.size() > 0);
	for (bhvr_map_t::const_iterator itBhvr = m_BhvrMap.begin(); itBhvr != m_BhvrMap.end(); ++itBhvr)
	{
		std::string strCmd = itBhvr->first; ERlvBehaviour eBhvr = itBhvr->second;
		if (std::string::npos != strCmd.find(strMatch))
			cmdList.insert(std::pair<std::string, ERlvBehaviour>(strCmd, eBhvr));
		if ( (hasStrictVariant(eBhvr)) && (std::string::npos != strCmd.append("_sec").find(strMatch)) )
			cmdList.insert(std::pair<std::string, ERlvBehaviour>(strCmd, eBhvr));
	}
	return (0 != cmdList.size());
}

// Checked: 2010-02-27 (RLVa-1.2.0a) | Modified: RLVa-1.1.0h
void RlvCommand::initLookupTable()
{
	static bool fInitialized = false;
	if (!fInitialized)
	{
		// NOTE: keep this matched with the enumeration at all times
		std::string arBehaviours[RLV_BHVR_COUNT] =
			{
				"detach", "attach", "addattach", "remattach", "addoutfit", "remoutfit", "sharedwear", "sharedunwear", 
				"unsharedwear", "unsharedunwear", "emote", "sendchat", "recvchat", "recvchatfrom", "recvemote", "recvemotefrom",
				"redirchat", "rediremote", "chatwhisper", "chatnormal", "chatshout", "sendchannel", "sendim", "sendimto",
				"recvim", "recvimfrom", "startim", "startimto", "permissive", "notify", "showinv", "showminimap", "showworldmap", "showloc",
				"shownames", "shownametags", "showhovertext", "showhovertexthud", "showhovertextworld", "showhovertextall", "tplm", "tploc", "tplure", "tprequest",
				"viewnote", "viewscript", "viewtexture", "acceptpermission", "accepttp", "accepttprequest", "allowidle", "edit", "editobj", "rez",
				"fartouch", "interact", "touchthis", "touchattach", "touchattachself", "touchattachother", "touchhud", "touchworld", "touchall",
				"touchme", "fly", "setgroup", "unsit", "sit", "sittp", "standtp", "setdebug", "setenv", "alwaysrun", "temprun", "detachme", 
				"attachover", "attachthis", "attachthisover", "attachthis_except", "detachthis", "detachthis_except", "attachall", 
				"attachallover", "detachall", "attachallthis", "attachallthis_except", "attachallthisover", "detachallthis", 
				"detachallthis_except", "adjustheight", "camzoommax", "camzoommin", "camdistmax", "camdistmin",
				// Singu TODO: Implement the RLV 2.9 graphics options and remove TODO from their strings so they work (and report as working)
				// If there is work to be done immediately after these is toggled, add to the onToggleCamXXX section in rlvui.cpp
				"TODOcamdrawmax", "TODOcamdrawmin", "TODOcamdrawalphamax", "TODOcamdrawalphamin", "TODOcamdrawcolor", "camunlock", "camavdist", "TODOcamtextures",
				"tpto", "version", "versionnew", "versionnum", "getattach", "getattachnames",
				"getaddattachnames", "getremattachnames", "getoutfit", "getoutfitnames", "getaddoutfitnames", "getremoutfitnames", 
				"findfolder", "findfolders", "getpath", "getpathnew", "getinv", "getinvworn", "getgroup", "getsitid", "getcommand", 
				"getstatus", "getstatusall"
			};

		for (int idxBvhr = 0; idxBvhr < RLV_BHVR_COUNT; idxBvhr++)
			m_BhvrMap.insert(std::pair<std::string, ERlvBehaviour>(arBehaviours[idxBvhr], (ERlvBehaviour)idxBvhr));

		fInitialized = true;
	}
}

// ============================================================================
// RlvCommandOption structures
//

// Checked: 2010-09-28 (RLVa-1.2.1c) | Added: RLVa-1.2.1c
RlvCommandOptionGeneric::RlvCommandOptionGeneric(const std::string& strOption): m_fEmpty(false)
{
	LLWearableType::EType wtType(LLWearableType::WT_INVALID); LLUUID idOption; ERlvAttachGroupType eAttachGroup(RLV_ATTACHGROUP_INVALID);
	LLViewerJointAttachment* pAttachPt = NULL; LLViewerInventoryCategory* pFolder = NULL;

	if (!(m_fEmpty = strOption.empty()))														// <option> could be an empty string
	{
		if ( ((wtType = LLWearableType::typeNameToType(strOption)) != LLWearableType::WT_INVALID) && (wtType != LLWearableType::WT_NONE) )
			m_varOption = wtType;																// ... or specify a (valid) clothing layer
		else if ((pAttachPt = RlvAttachPtLookup::getAttachPoint(strOption)) != NULL)
			m_varOption = pAttachPt;															// ... or specify an attachment point
		else if ( ((UUID_STR_LENGTH - 1) == strOption.length()) && (idOption.set(strOption)) )
			m_varOption = idOption;																// ... or specify an UUID
		else if ((pFolder = RlvInventory::instance().getSharedFolder(strOption)) != NULL)
			m_varOption = pFolder;																// ... or specify a shared folder path
		else if ((eAttachGroup = rlvAttachGroupFromString(strOption)) != RLV_ATTACHGROUP_INVALID)
			m_varOption = eAttachGroup;															// ... or specify an attachment point group
		else
			m_varOption = strOption;															// ... or it might just be a string
	}
	m_fValid = true;
}

// ============================================================================
// RlvCommandOption structures
//

// Checked: 2012-07-28 (RLVa-1.4.7)
class RlvCommandOptionGetPathCallback
{
public:
	RlvCommandOptionGetPathCallback(const LLUUID& idAttachObj, RlvCommandOptionGetPath::getpath_callback_t cb)
		: mObjectId(idAttachObj), mCallback(cb)
	{
		if (isAgentAvatarValid())
			mAttachmentConnection = gAgentAvatarp->setAttachmentCallback(boost::bind(&RlvCommandOptionGetPathCallback::onAttachment, this, _1, _3));
		gIdleCallbacks.addFunction(&onIdle, this);
	}

	~RlvCommandOptionGetPathCallback()
	{
		if (mAttachmentConnection.connected())
			mAttachmentConnection.disconnect();
		gIdleCallbacks.deleteFunction(&onIdle, this);
	}

	void onAttachment(LLViewerObject* pAttachObj, LLVOAvatarSelf::EAttachAction eAction) const
	{
		if ( (LLVOAvatarSelf::ACTION_ATTACH == eAction) && (pAttachObj->getID() == mObjectId) )
		{
			uuid_vec_t idItems(1, pAttachObj->getAttachmentItemID());
			mCallback(idItems);
			delete this;
		}
	}

	static void onIdle(void* pData)
	{
		RlvCommandOptionGetPathCallback* pInstance = reinterpret_cast<RlvCommandOptionGetPathCallback*>(pData);
		if (pInstance->mExpirationTimer.getElapsedTimeF32() > 30.0f)
			delete pInstance;
	}

protected:
	LLUUID                      mObjectId;
	RlvCommandOptionGetPath::getpath_callback_t mCallback;
	boost::signals2::connection mAttachmentConnection;
	LLFrameTimer                mExpirationTimer;
};

// Checked: 2010-11-30 (RLVa-1.3.0b) | Modified: RLVa-1.3.0b
RlvCommandOptionGetPath::RlvCommandOptionGetPath(const RlvCommand& rlvCmd, getpath_callback_t cb)
	: m_fCallback(false)
{
	m_fValid = true;	// Assume the option will be a valid one until we find out otherwise

	// @getpath[:<option>]=<channel> => <option> is transformed to a list of inventory item UUIDs to get the path of
	RlvCommandOptionGeneric rlvCmdOption(rlvCmd.getOption());
	if (rlvCmdOption.isWearableType())			// <option> can be a clothing layer
	{
		getItemIDs(rlvCmdOption.getWearableType(), m_idItems);
	}
	else if (rlvCmdOption.isAttachmentPoint())	// ... or it can specify an attachment point
	{
		getItemIDs(rlvCmdOption.getAttachmentPoint(), m_idItems);
	}
	else if (rlvCmdOption.isUUID())				// ... or it can specify a specific attachment
	{
		const LLViewerObject* pAttachObj = gObjectList.findObject(rlvCmdOption.getUUID());
		if ( (pAttachObj) && (pAttachObj->isAttachment()) && (pAttachObj->permYouOwner()) )
			m_idItems.push_back(pAttachObj->getAttachmentItemID());
		else
			m_fValid = false;
	}
	else if (rlvCmdOption.isEmpty())			// ... or it can be empty (in which case we act on the object that issued the command)
	{
		const LLViewerObject* pObj = gObjectList.findObject(rlvCmd.getObjectID());
		if (pObj)
		{
			if (pObj->isAttachment())
				m_idItems.push_back(pObj->getAttachmentItemID());
		}
		else if (!cb.empty())
		{
			new RlvCommandOptionGetPathCallback(rlvCmd.getObjectID(), cb);
			m_fCallback = true;
			return;
		}
	}
	else										// ... but anything else isn't a valid option
	{
		m_fValid = false;
		return;
	}

	if (!cb.empty())
	{
		cb(getItemIDs());
	}
}

// Checked: 2013-10-12 (RLVa-1.4.9)
bool RlvCommandOptionGetPath::getItemIDs(const LLViewerJointAttachment* pAttachPt, uuid_vec_t& idItems)
{
	uuid_vec_t::size_type cntItemsPrev = idItems.size();

	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvFindAttachmentsOnPoint f(pAttachPt);
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), folders, items, false, f);
	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		if (pItem)
			idItems.push_back(pItem->getLinkedUUID());
	}

	return (cntItemsPrev != idItems.size());
}

// Checked: 2013-10-12 (RLVa-1.4.9)
bool RlvCommandOptionGetPath::getItemIDs(LLWearableType::EType wtType, uuid_vec_t& idItems)
{
	uuid_vec_t::size_type cntItemsPrev = idItems.size();

	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	LLFindWearablesOfType f(wtType);
	gInventory.collectDescendentsIf(LLAppearanceMgr::instance().getCOF(), folders, items, false, f);
	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		if (pItem)
			idItems.push_back(pItem->getLinkedUUID());
	}

	return (cntItemsPrev != idItems.size());
}

// Checked: 2015-03-30 (RLVa-1.5.0)
RlvCommandOptionAdjustHeight::RlvCommandOptionAdjustHeight(const RlvCommand& rlvCmd)
	: m_nPelvisToFoot(0.0f), m_nPelvisToFootDeltaMult(0.0f), m_nPelvisToFootOffset(0.0f)
{
	std::vector<std::string> cmdTokens;
	boost::split(cmdTokens, rlvCmd.getOption(), boost::is_any_of(std::string(";")));
	if (1 == cmdTokens.size())
	{
		m_fValid = (LLStringUtil::convertToF32(cmdTokens[0], m_nPelvisToFootOffset));
		m_nPelvisToFootOffset /= 100;
	}
	else if ( (2 <= cmdTokens.size()) && (cmdTokens.size() <= 3) )
	{
		m_fValid = (LLStringUtil::convertToF32(cmdTokens[0], m_nPelvisToFoot)) &&
			 (LLStringUtil::convertToF32(cmdTokens[1], m_nPelvisToFootDeltaMult)) && 
			 ( (2 == cmdTokens.size()) || (LLStringUtil::convertToF32(cmdTokens[2], m_nPelvisToFootOffset)) );
	}
}

// Checked: 2011-03-28 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
RlvCommandOptionTpTo::RlvCommandOptionTpTo(const RlvCommand &rlvCmd)
{
	std::vector<std::string> cmdTokens;
	boost::split(cmdTokens, rlvCmd.getOption(), boost::is_any_of(std::string("/")));

	m_fValid = (3 == cmdTokens.size());
	for (int idxAxis = 0; (idxAxis < 3) && (m_fValid); idxAxis++)
		m_fValid &= static_cast<bool>(LLStringUtil::convertToF64(cmdTokens[idxAxis], m_posGlobal[idxAxis]));
}

// =========================================================================
// RlvObject
//

RlvObject::RlvObject(const LLUUID& idObj) : m_idObj(idObj), m_nLookupMisses(0)
{
	LLViewerObject* pObj = gObjectList.findObject(idObj);
	m_fLookup = (NULL != pObj);
	m_idxAttachPt = (pObj) ? ATTACHMENT_ID_FROM_STATE(pObj->getAttachmentState()) : 0;
	m_idRoot = (pObj) ? pObj->getRootEdit()->getID() : LLUUID::null;
}

bool RlvObject::addCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_ADD == rlvCmd.getParamType());

	// Don't add duplicate commands for this object (ie @detach=n followed by another @detach=n later on)
	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			return false;
		}
	}

	// Now that we know it's not a duplicate, add it to the end of the list
	m_Commands.push_back(rlvCmd);

	return true;
}

bool RlvObject::removeCommand(const RlvCommand& rlvCmd)
{
	RLV_ASSERT(RLV_TYPE_REMOVE == rlvCmd.getParamType());

	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		//if (*itCmd == rlvCmd) <- commands will never be equal since one is an add and the other is a remove *rolls eyes*
		if ( (itCmd->getBehaviour() == rlvCmd.getBehaviour()) && (itCmd->getOption() == rlvCmd.getOption()) && 
			 (itCmd->isStrict() == rlvCmd.isStrict() ) )
		{
			m_Commands.erase(itCmd);
			return true;
		}
	}
	return false;	// Command was never added so nothing to remove now
}

// Checked: 2011-05-23 (RLVa-1.3.1c) | Added: RLVa-1.3.1c
void RlvObject::setCommandRet(const RlvCommand& rlvCmd, ERlvCmdRet eRet)
{
	for (rlv_command_list_t::iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		if (*itCmd == rlvCmd)
		{
			itCmd->m_eRet = eRet;
			break;
		}
	}
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, bool fStrictOnly) const
{
	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
		if ( (itCmd->getBehaviourType() == eBehaviour) && (itCmd->getOption().empty()) && ((!fStrictOnly) || (itCmd->isStrict())) )
			return true;
	return false;
}

bool RlvObject::hasBehaviour(ERlvBehaviour eBehaviour, const std::string& strOption, bool fStrictOnly) const
{
	for (const RlvCommand& rlvCmd : m_Commands)
	{
		// The specified behaviour is contained within the current object if:
		//   - the (parsed) behaviour matches
		//   - the option matches (or we're checking for an empty option and the command was reference counted)
		//   - we're not matching on strict (or it is a strict command)
		if ( (rlvCmd.getBehaviourType() == eBehaviour) &&
			 ( (rlvCmd.getOption() == strOption) /*|| ((strOption.empty()) && (rlvCmd.isRefCounted()))*/ ) &&
			 ( (!fStrictOnly) ||(rlvCmd.isStrict()) ) )
		{
			return true;
		}
	}
	return false;
}

// Checked: 2009-11-27 (RLVa-1.1.0f) | Modified: RLVa-1.1.0f
std::string RlvObject::getStatusString(const std::string& strFilter, const std::string& strSeparator) const
{
	std::string strStatus, strCmd;

	for (rlv_command_list_t::const_iterator itCmd = m_Commands.begin(); itCmd != m_Commands.end(); ++itCmd)
	{
		strCmd = itCmd->asString();
		if ( (strFilter.empty()) || (std::string::npos != strCmd.find(strFilter)) )
			strStatus.append(strSeparator).append(strCmd);
	}

	return strStatus;
}

// ============================================================================
// RlvForceWear
//

// Checked: 2010-04-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
bool RlvForceWear::isWearingItem(const LLInventoryItem* pItem)
{
	if (pItem)
	{
		switch (pItem->getActualType())
		{
			case LLAssetType::AT_BODYPART:
			case LLAssetType::AT_CLOTHING:
				return gAgentWearables.isWearingItem(pItem->getUUID());
			case LLAssetType::AT_OBJECT:
				return (isAgentAvatarValid()) && (gAgentAvatarp->isWearingAttachment(pItem->getUUID()));
			case LLAssetType::AT_GESTURE:
				return LLGestureMgr::instance().isGestureActive(pItem->getUUID());
			case LLAssetType::AT_LINK:
				return isWearingItem(gInventory.getItem(pItem->getLinkedUUID()));
			default:
				break;
		}
	}
	return false;
}

// Checked: 2010-03-21 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceFolder(const LLViewerInventoryCategory* pFolder, EWearAction eAction, EWearFlags eFlags)
{
	// [See LLWearableBridge::wearOnAvatar(): don't wear anything until initial wearables are loaded, can destroy clothing items]
	if (!gAgentWearables.areWearablesLoaded())
	{
		LLNotificationsUtil::add("CanNotChangeAppearanceUntilLoaded");
		return;
	}
	if (!isAgentAvatarValid())
		return;

	// Grab a list of all the items we'll be wearing/attaching
	LLInventoryModel::cat_array_t folders; LLInventoryModel::item_array_t items;
	RlvWearableItemCollector f(pFolder, eAction, eFlags);
	gInventory.collectDescendentsIf(pFolder->getUUID(), folders, items, FALSE, f, true);

	// TRUE if we've already encountered this LLWearableType::EType (used only on wear actions and only for AT_CLOTHING)
	bool fSeenWType[LLWearableType::WT_COUNT] = { false };

	EWearAction eCurAction = eAction;
	for (S32 idxItem = 0, cntItem = items.size(); idxItem < cntItem; idxItem++)
	{
		LLViewerInventoryItem* pRlvItem = items.at(idxItem);
		LLViewerInventoryItem* pItem = (LLAssetType::AT_LINK == pRlvItem->getActualType()) ? pRlvItem->getLinkedItem() : pRlvItem;

		// If it's wearable it should be worn on detach
//		if ( (ACTION_DETACH == eAction) && (isWearableItem(pItem)) && (!isWearingItem(pItem)) )
//			continue;

		// Each folder can specify its own EWearAction override
		if (isWearAction(eAction))
			eCurAction = f.getWearAction(pRlvItem->getParentUUID());
		else
			eCurAction = eAction;

		//  NOTES: * if there are composite items then RlvWearableItemCollector made sure they can be worn (or taken off depending)
		//         * some scripts issue @remattach=force,attach:worn-items=force so we need to attach items even if they're currently worn
		switch (pItem->getType())
		{
			case LLAssetType::AT_BODYPART:
				RLV_ASSERT(isWearAction(eAction));	// RlvWearableItemCollector shouldn't be supplying us with body parts on detach
			case LLAssetType::AT_CLOTHING:
				if (isWearAction(eAction))
				{
					// The first time we encounter any given clothing type we use 'eCurAction' (replace or add)
					// The second time we encounter a given clothing type we'll always add (rather than replace the previous iteration)
					eCurAction = (!fSeenWType[pItem->getWearableType()]) ? eCurAction : ACTION_WEAR_ADD;

					ERlvWearMask eWearMask = gRlvWearableLocks.canWear(pRlvItem);
					if ( ((ACTION_WEAR_REPLACE == eCurAction) && (eWearMask & RLV_WEAR_REPLACE)) ||
						 ((ACTION_WEAR_ADD == eCurAction) && (eWearMask & RLV_WEAR_ADD)) )
					{
						// The check for whether we're replacing a currently worn composite item happens in onWearableArrived()
						if (!isAddWearable(pItem))
							addWearable(pRlvItem, eCurAction);
						fSeenWType[pItem->getWearableType()] = true;
					}
				}
				else
				{
					const LLViewerWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getUUID());
					if ( (pWearable) && (isForceRemovable(pWearable, false)) )
						remWearable(pWearable);
				}
				break;

			case LLAssetType::AT_OBJECT:
				if (isWearAction(eAction))
				{
					ERlvWearMask eWearMask = gRlvAttachmentLocks.canAttach(pRlvItem);
					if ( ((ACTION_WEAR_REPLACE == eCurAction) && (eWearMask & RLV_WEAR_REPLACE)) ||
						 ((ACTION_WEAR_ADD == eCurAction) && (eWearMask & RLV_WEAR_ADD)) )
					{
						if (!isAddAttachment(pRlvItem))
						{
							#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							// We still need to check whether we're about to replace a currently worn composite item
							// (which we're not if we're just reattaching an attachment we're already wearing)
							LLViewerInventoryCategory* pCompositeFolder = NULL;
							if ( (pAttachPt->getObject()) && (RlvSettings::getEnableComposites()) && 
								 (pAttachPt->getItemID() != pItem->getUUID()) &&
								 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pCompositeFolder)) )
							{
								// If we can't take off the composite folder this item would replace then don't allow it to get attached
								if (gRlvHandler.canTakeOffComposite(pCompositeFolder))
								{
									forceFolder(pCompositeFolder, ACTION_DETACH, FLAG_DEFAULT);
									addAttachment(pRlvItem);
								}
							}
							else
							#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
							{
								addAttachment(pRlvItem, eCurAction);
							}
						}
					}
				}
				else
				{
					const LLViewerObject* pAttachObj = gAgentAvatarp->getWornAttachment(pItem->getUUID());
					if ( (pAttachObj) && (isForceDetachable(pAttachObj, false)) )
						remAttachment(pAttachObj);
				}
				break;

			#ifdef RLV_EXTENSION_FORCEWEAR_GESTURES
			case LLAssetType::AT_GESTURE:
				if (isWearAction(eAction))
				{
					if (std::find_if(m_addGestures.begin(), m_addGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_addGestures.end())
						m_addGestures.push_back(pRlvItem);
				}
				else
				{
					if (std::find_if(m_remGestures.begin(), m_remGestures.end(), RlvPredIsEqualOrLinkedItem(pRlvItem)) == m_remGestures.end())
						m_remGestures.push_back(pRlvItem);
				}
				break;
			#endif // RLV_EXTENSION_FORCEWEAR_GESTURES

			default:
				break;
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerObject* pAttachObj, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment can be detached by an RLV command if:
	//   - it's not "remove locked" by anything (or anything except the object specified by pExceptObj)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
	    (pAttachObj) && (pAttachObj->isAttachment())
		&& ( (idExcept.isNull()) ? (!gRlvAttachmentLocks.isLockedAttachment(pAttachObj))
								 : (!gRlvAttachmentLocks.isLockedAttachmentExcept(pAttachObj, idExcept)) )
		&& (isStrippable(pAttachObj->getAttachmentItemID()))
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
		     (!gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceDetachable(const LLViewerJointAttachment* pAttachPt, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Attachment point can be detached by an RLV command if there's at least one attachment that can be removed
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		if (isForceDetachable(*itAttachObj, fCheckComposite, idExcept))
			return true;
	}
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.1.0i
void RlvForceWear::forceDetach(const LLViewerObject* pAttachObj)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pAttachObj) || (isRemAttachment(pAttachObj)) )
		return;

	if (isForceDetachable(pAttachObj))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(pAttachPt->getItemID(), NULL, &pFolder)) )
		{
			// Attachment belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remAttachment(pAttachObj);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceDetach(const LLViewerJointAttachment* pAttachPt)
{
	for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator itAttachObj = pAttachPt->mAttachedObjects.begin();
			itAttachObj != pAttachPt->mAttachedObjects.end(); ++itAttachObj)
	{
		forceDetach(*itAttachObj);
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(const LLViewerWearable* pWearable, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable can be removed by an RLV command if:
	//   - its asset type is AT_CLOTHING
	//   - it's not "remove locked" by anything (or anything except the object specified by idExcept)
	//   - it's strippable
	//   - composite folders are disabled *or* it isn't part of a composite folder that has at least one item locked
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	LLViewerInventoryCategory* pFolder = NULL;
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	return 
	  (
		(pWearable) && (LLAssetType::AT_CLOTHING == pWearable->getAssetType()) 
		&& ( (idExcept.isNull()) ? !gRlvWearableLocks.isLockedWearable(pWearable)
		                         : !gRlvWearableLocks.isLockedWearableExcept(pWearable, idExcept) )
		&& (isStrippable(pWearable->getItemID()))
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		&& ( (!fCheckComposite) || (!RlvSettings::getEnableComposites()) || 
		     (!gRlvHandler.getCompositeInfo(pWearable->getItemID(), NULL, &pFolder)) || (gRlvHandler.canTakeOffComposite(pFolder)) )
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	  );
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvForceWear::isForceRemovable(LLWearableType::EType wtType, bool fCheckComposite /*=true*/, const LLUUID& idExcept /*=LLUUID::null*/)
{
	// Wearable type can be removed by an RLV command if there's at least one currently worn wearable that can be removed
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		if (isForceRemovable(gAgentWearables.getViewerWearable(wtType, idxWearable), fCheckComposite, idExcept))
			return true;
	return false;
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvForceWear::forceRemove(const LLViewerWearable* pWearable)
{
	// Sanity check - no need to process duplicate removes
	if ( (!pWearable) || (isRemWearable(pWearable)) )
		return;

	if (isForceRemovable(pWearable))
	{
		#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		LLViewerInventoryCategory* pFolder = NULL;
		if ( (RlvSettings::getEnableComposites()) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(wtType), NULL, &pFolder)) )
		{
			// Wearable belongs to a composite folder so detach the entire folder (if we can take it off)
			if (gRlvHandler.canTakeOffComposite(pFolder))
				forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
		}
		else
		#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS
		{
			remWearable(pWearable);
		}
	}
}

// Checked: 2010-03-19 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvForceWear::forceRemove(LLWearableType::EType wtType)
{
	for (U32 idxWearable = 0, cntWearable = gAgentWearables.getWearableCount(wtType); idxWearable < cntWearable; idxWearable++)
		forceRemove(gAgentWearables.getViewerWearable(wtType, idxWearable));
}

// Checked: 2010-03-19 (RLVa-1.2.0c) | Modified: RLVa-1.2.0a
bool RlvForceWear::isStrippable(const LLInventoryItem* pItem)
{
	// An item is exempt from @detach or @remoutfit if:
	//   - its name contains "nostrip" (anywhere in the name)
	//   - its parent folder contains "nostrip" (anywhere in the name)
	if (pItem)
	{
		// If the item is an inventory link then we first examine its target before examining the link itself (and never its name)
		if (LLAssetType::AT_LINK == pItem->getActualType())
		{
			if (!isStrippable(pItem->getLinkedUUID()))
				return false;
		}
		else
		{
			if (std::string::npos != pItem->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
		}

		LLViewerInventoryCategory* pFolder = gInventory.getCategory(pItem->getParentUUID());
		while (pFolder)
		{
			if (std::string::npos != pFolder->getName().find(RLV_FOLDER_FLAG_NOSTRIP))
				return false;
			// If the item's parent is a folded folder then we need to check its parent as well
			if ( (gInventory.getRootFolderID() != pFolder->getParentUUID()) && (RlvInventory::isFoldedFolder(pFolder, true)) )
				pFolder = gInventory.getCategory(pFolder->getParentUUID());
			else
				pFolder = NULL;
		}
	}
	return true;
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::addAttachment(const LLViewerInventoryItem* pItem, EWearAction eAction)
{
	// Remove it from 'm_remAttachments' if it's queued for detaching
	const LLViewerObject* pAttachObj = (isAgentAvatarValid()) ? gAgentAvatarp->getWornAttachment(pItem->getLinkedUUID()) : NULL;
	if ( (pAttachObj) && (isRemAttachment(pAttachObj)) )
		m_remAttachments.erase(std::remove(m_remAttachments.begin(), m_remAttachments.end(), pAttachObj), m_remAttachments.end());

	S32 idxAttachPt = RlvAttachPtLookup::getAttachPointIndex(pItem, true);
	if (ACTION_WEAR_ADD == eAction)
	{
		// Insert it at the back if it's not already there
		idxAttachPt |= ATTACHMENT_ADD;
		if (!isAddAttachment(pItem))
		{
			addattachments_map_t::iterator itAddAttachments = m_addAttachments.find(idxAttachPt);
			if (itAddAttachments == m_addAttachments.end())
			{
				m_addAttachments.insert(addattachment_pair_t(idxAttachPt, LLInventoryModel::item_array_t()));
				itAddAttachments = m_addAttachments.find(idxAttachPt);
			}
			itAddAttachments->second.push_back(const_cast<LLViewerInventoryItem*>(pItem));
		}
	}
	else if (ACTION_WEAR_REPLACE == eAction)
	{
		// Replace all pending attachments on this attachment point with the specified item (don't clear if it's the default attach point)
		addattachments_map_t::iterator itAddAttachments = m_addAttachments.find(idxAttachPt | ATTACHMENT_ADD);
		if ( (0 != idxAttachPt) && (itAddAttachments != m_addAttachments.end()) )
			itAddAttachments->second.clear();

		itAddAttachments = m_addAttachments.find(idxAttachPt);
		if (itAddAttachments == m_addAttachments.end())
		{
			m_addAttachments.insert(addattachment_pair_t(idxAttachPt, LLInventoryModel::item_array_t()));
			itAddAttachments = m_addAttachments.find(idxAttachPt);
		}

		if (0 != idxAttachPt)
			itAddAttachments->second.clear();
		itAddAttachments->second.push_back(const_cast<LLViewerInventoryItem*>(pItem));
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::remAttachment(const LLViewerObject* pAttachObj)
{
	// Remove it from 'm_addAttachments' if it's queued for attaching
	const LLViewerInventoryItem* pItem = (pAttachObj->isAttachment()) ? gInventory.getItem(pAttachObj->getAttachmentItemID()) : NULL;
	if (pItem)
	{
		addattachments_map_t::iterator itAddAttachments = m_addAttachments.begin();
		while (itAddAttachments != m_addAttachments.end())
		{
			LLInventoryModel::item_array_t& wearItems = itAddAttachments->second;
			if (std::find_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)) != wearItems.end())
				wearItems.erase(std::remove_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)), wearItems.end());

			if (wearItems.empty())
				m_addAttachments.erase(itAddAttachments++);
			else
				++itAddAttachments;
		}
	}

	// Add it to 'm_remAttachments' if it's not already there
	if (!isRemAttachment(pAttachObj))
	{
		m_remAttachments.push_back(const_cast<LLViewerObject*>(pAttachObj));
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::addWearable(const LLViewerInventoryItem* pItem, EWearAction eAction)
{
	const LLViewerWearable* pWearable = gAgentWearables.getWearableFromItemID(pItem->getLinkedUUID());
	// When replacing remove all currently worn wearables of this type *unless* the item is currently worn
	if ( (ACTION_WEAR_REPLACE == eAction) && (!pWearable) )
		forceRemove(pItem->getWearableType());
	// Remove it from 'm_remWearables' if it's pending removal
	if ( (pWearable) && (isRemWearable(pWearable)) )
		m_remWearables.erase(std::remove(m_remWearables.begin(), m_remWearables.end(), pWearable), m_remWearables.end());

	addwearables_map_t::iterator itAddWearables = m_addWearables.find(pItem->getWearableType());
	if (itAddWearables == m_addWearables.end())
	{
		m_addWearables.insert(addwearable_pair_t(pItem->getWearableType(), LLInventoryModel::item_array_t()));
		itAddWearables = m_addWearables.find(pItem->getWearableType());
	}

	if (ACTION_WEAR_ADD == eAction)				// Add it at the back if it's not already there
	{
		if (!isAddWearable(pItem))
			itAddWearables->second.push_back(const_cast<LLViewerInventoryItem*>(pItem));
	}
	else if (ACTION_WEAR_REPLACE == eAction)	// Replace all pending wearables of this type with the specified item
	{
		itAddWearables->second.clear();
		itAddWearables->second.push_back(const_cast<LLViewerInventoryItem*>(pItem));
	}
}

// Checked: 2010-08-30 (RLVa-1.2.1c) | Modified: RLVa-1.2.1c
void RlvForceWear::remWearable(const LLViewerWearable* pWearable)
{
	// Remove it from 'm_addWearables' if it's queued for wearing
	const LLViewerInventoryItem* pItem = gInventory.getItem(pWearable->getItemID());
	if ( (pItem) && (isAddWearable(pItem)) )
	{
		addwearables_map_t::iterator itAddWearables = m_addWearables.find(pItem->getWearableType());

		LLInventoryModel::item_array_t& wearItems = itAddWearables->second;
		wearItems.erase(std::remove_if(wearItems.begin(), wearItems.end(), RlvPredIsEqualOrLinkedItem(pItem)), wearItems.end());
		if (wearItems.empty())
			m_addWearables.erase(itAddWearables);
	}

	// Add it to 'm_remWearables' if it's not already there
	if (!isRemWearable(pWearable))
		m_remWearables.push_back(pWearable);
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::updatePendingAttachments()
{
	if (RlvForceWear::instanceExists())
	{
		RlvForceWear* pThis = RlvForceWear::getInstance();
		for (const auto& itAttach : pThis->m_pendingAttachments)
			LLAttachmentsMgr::instance().addAttachmentRequest(itAttach.first, itAttach.second & ~ATTACHMENT_ADD, itAttach.second & ATTACHMENT_ADD);
		pThis->m_pendingAttachments.clear();
	}
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::addPendingAttachment(const LLUUID& idItem, U8 idxPoint)
{
	auto itAttach = m_pendingAttachments.find(idItem);
	if (m_pendingAttachments.end() == itAttach)
		m_pendingAttachments.insert(std::make_pair(idItem, idxPoint));
	else
		itAttach->second = idxPoint;
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::remPendingAttachment(const LLUUID& idItem)
{
	m_pendingAttachments.erase(idItem);
}

// Checked: 2015-05-05 (RLVa-1.4.12)
void RlvForceWear::done()
{
	// Sanity check - don't go through all the motions below only to find out there's nothing to actually do
	if ( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) &&
		 (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) )
	{
		return;
	}

	//
	// Process removals
	//

	uuid_vec_t remItems;

	// Wearables
	if (m_remWearables.size())
	{
		for (const LLViewerWearable* pWearable : m_remWearables)
			remItems.push_back(pWearable->getItemID());
		m_remWearables.clear();
	}

	// Gestures
	if (m_remGestures.size())
	{
		// NOTE: LLGestureMgr::deactivateGesture() will call LLAppearanceMgr::removeCOFItemLinks() for us and supply its own callback
		for (const LLViewerInventoryItem* pItem : m_remGestures)
			LLGestureMgr::instance().deactivateGesture(pItem->getUUID());
		m_remGestures.clear();
	}

	// Attachments
	if (m_remAttachments.size())
	{
		LLAgentWearables::userRemoveMultipleAttachments(m_remAttachments);
		for (const LLViewerObject* pAttachObj : m_remAttachments)
			remItems.push_back(pAttachObj->getAttachmentItemID());
		m_remAttachments.clear();
	}

	//
	// Process additions
	//

	// Wearables need to be split into AT_BODYPART and AT_CLOTHING for COF
	LLInventoryModel::item_array_t addBodyParts, addClothing;
	for (addwearables_map_t::const_iterator itAddWearables = m_addWearables.cbegin(); itAddWearables != m_addWearables.cend(); ++itAddWearables)
	{
		// NOTE: LLAppearanceMgr will filter our duplicates so no need for us to check here
		for (LLViewerInventoryItem* pItem : itAddWearables->second)
		{
			if (LLAssetType::AT_BODYPART == pItem->getType())
				addBodyParts.push_back(pItem);
			else
				addClothing.push_back(pItem);
		}
	}
	m_addWearables.clear();

	// Until LL provides a way for updateCOF to selectively attach add/replace we have to deal with attachments ourselves
	for (addattachments_map_t::const_iterator itAddAttachments = m_addAttachments.cbegin(); itAddAttachments != m_addAttachments.cend(); ++itAddAttachments)
	{
		for (const LLViewerInventoryItem* pItem : itAddAttachments->second)
			addPendingAttachment(pItem->getLinkedUUID(), itAddAttachments->first);
	}
	m_addAttachments.clear();

	//
	// Tie it all together
	//

	//          |    Wearables    |   Attachments    |   Gestures      |
	//          |======================================================|
	// Add    : | LLAppearanceMgr | <custom>         | LLAppearanceMgr |
	// Remove : | LLAppearanceMgr | LLAppearanceMgr  | LLGestureMgr    |
	LLPointer<LLInventoryCallback> cb = new LLUpdateAppearanceOnDestroy(false, false, boost::bind(RlvForceWear::updatePendingAttachments));

	if (!remItems.empty())
	{
		LLAppearanceMgr::instance().removeItemsFromAvatar(remItems, cb, true);
	}

	if ( (addBodyParts.empty()) && (!addClothing.empty()) && (m_addGestures.empty()) )
	{
		// Clothing items only
		uuid_vec_t idClothing;
		for (const LLViewerInventoryItem* pItem : addClothing)
			idClothing.push_back(pItem->getUUID());
		LLAppearanceMgr::instance().wearItemsOnAvatar(idClothing, false, false, cb);
	}
	else if ( (!addBodyParts.empty()) || (!addClothing.empty()) || (!m_addGestures.empty()) )
	{
		// Mixture of body parts, clothing and/or gestures
		LLInventoryModel::item_array_t addAttachments;
		LLAppearanceMgr::instance().updateCOF(addBodyParts, addClothing, addAttachments, m_addGestures, true, LLUUID::null, cb);

		m_addGestures.clear();
	}

	// Make sure there are no leftovers for the next cycle
	RLV_ASSERT( (m_remWearables.empty()) && (m_remAttachments.empty()) && (m_remGestures.empty()) );
	RLV_ASSERT( (m_addWearables.empty()) && (m_addAttachments.empty()) && (m_addGestures.empty()) );
}

// Checked: 2010-02-17 (RLVa-1.1.0o) | Modified: RLVa-1.1.0o
/*
void RlvForceWear::onWearableArrived(LLWearable* pWearable, void* pParam)
{
	#ifdef RLV_EXPERIMENTAL_COMPOSITEFOLDERS
	// If this wearable will end up replacing a currently worn one that belongs to a composite folder then we need to detach the composite
	LLViewerInventoryCategory* pFolder = NULL;
	if ( (RlvSettings::getEnableComposites()) && (pWearable) && (gAgent.getWearable(pWearable->getType())) )
	{
		// If we're just rewearing the same item we're already wearing then we're not replacing a composite folder
		LLWearableHoldingPattern* pWearData = (LLWearableHoldingPattern*)pParam; LLUUID idItem;
		for (LLWearableHoldingPattern::found_list_t::const_iterator itWearable = pWearData->mFoundList.begin();
				itWearable != pWearData->mFoundList.end(); ++itWearable)
		{
			LLFoundData* pFound = *itWearable;
			if (pWearable->getID() == pFound->mAssetID)
			{
				idItem = pFound->mItemID;
				break;
			}
		}
		if ( (idItem.notNull()) && (idItem != gAgent.getWearableItem(pWearable->getType())) && 
			 (gRlvHandler.getCompositeInfo(gAgent.getWearableItem(pWearable->getType()), NULL, &pFolder)) )
		{
			RlvForceWear rlvWear;
			rlvWear.forceFolder(pFolder, ACTION_DETACH, FLAG_DEFAULT);
			rlvWear.done();
		}
	}
	#endif // RLV_EXPERIMENTAL_COMPOSITEFOLDERS

	wear_inventory_category_on_avatar_loop(pWearable, pParam);
}
*/

// ============================================================================
// RlvBehaviourNotifyObserver
//

// Checked: 2010-03-03 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
RlvBehaviourNotifyHandler::RlvBehaviourNotifyHandler()
{
	// NOTE: the reason we use rlv_command_signal_t instead of the better-suited rlv_behaviour_signal_t is because
	//       RLV will notify scripts about "invalid" commands so we need to as well
	m_ConnCommand = gRlvHandler.setCommandCallback(boost::bind(&RlvBehaviourNotifyHandler::onCommand, this, _1, _2, _3));
}

// ReSharper disable once CppMemberFunctionMayBeStatic

// Checked: 2010-03-03 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
void RlvBehaviourNotifyHandler::onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet, bool fInternal) const
{
	if (fInternal)
		return;

	switch (rlvCmd.getParamType())
	{
		case RLV_TYPE_ADD:
		case RLV_TYPE_REMOVE:
			sendNotification(rlvCmd.asString(), "=" + rlvCmd.getParam());
			break;
		case RLV_TYPE_CLEAR:
			sendNotification(rlvCmd.asString());
			break;
		default:
			break;
	}
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Modify: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::sendNotification(const std::string& strText, const std::string& strSuffix)
{
	if (instanceExists())
	{
		RlvBehaviourNotifyHandler* pThis = getInstance();

		// NOTE: notifications have two parts (which are concatenated without token) where only the first part is subject to the filter
		for (std::multimap<LLUUID, notifyData>::const_iterator itNotify = pThis->m_Notifications.begin(); 
				itNotify != pThis->m_Notifications.end(); ++itNotify)
		{
			if ( (itNotify->second.strFilter.empty()) || (boost::icontains(strText, itNotify->second.strFilter)) )
				RlvUtil::sendChatReply(itNotify->second.nChannel, "/" + strText + strSuffix);
		}
	}
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onWear(LLWearableType::EType eType, bool fAllowed)
{
	sendNotification(llformat("worn %s %s", (fAllowed) ? "legally" : "illegally", LLWearableType::getTypeName(eType).c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onTakeOff(LLWearableType::EType eType, bool fAllowed)
{
	sendNotification(llformat("unworn %s %s", (fAllowed) ? "legally" : "illegally", LLWearableType::getTypeName(eType).c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onAttach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("attached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onDetach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("detached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// Checked: 2011-03-31 (RLVa-1.3.0f) | Added: RLVa-1.3.0f
void RlvBehaviourNotifyHandler::onReattach(const LLViewerJointAttachment* pAttachPt, bool fAllowed)
{
	sendNotification(llformat("reattached %s %s", (fAllowed) ? "legally" : "illegally", pAttachPt->getName().c_str()));
}

// =========================================================================
// Various helper classes/timers/functors
//

// Checked: 2010-03-13 (RLVa-1.2.0a) | Modified: RLVa-1.2.0a
BOOL RlvGCTimer::tick()
{
	bool fContinue = gRlvHandler.onGC();
	if (!fContinue)
		gRlvHandler.m_pGCTimer = NULL;
	return !fContinue;
}

// ============================================================================
// Various helper functions
//

// ============================================================================
// Attachment group helper functions
//

// Has to match the order of ERlvAttachGroupType
const std::string cstrAttachGroups[RLV_ATTACHGROUP_COUNT] = { "head", "torso", "arms", "legs", "hud" };

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromIndex(S32 idxGroup)
{
	switch (idxGroup)
	{
		case 0: // Right Hand
		case 1: // Right Arm
		case 3: // Left Arm
		case 4: // Left Hand
			return RLV_ATTACHGROUP_ARMS;
		case 2: // Head
			return RLV_ATTACHGROUP_HEAD;
		case 5: // Left Leg
		case 7: // Right Leg
			return RLV_ATTACHGROUP_LEGS;
		case 6: // Torso
			return RLV_ATTACHGROUP_TORSO;
		case 8: // HUD
			return RLV_ATTACHGROUP_HUD;
		default:
			return RLV_ATTACHGROUP_INVALID;
	}
}

// Checked: 2009-10-19 (RLVa-1.1.0e) | Added: RLVa-1.1.0e
ERlvAttachGroupType rlvAttachGroupFromString(const std::string& strGroup)
{
	for (int idx = 0; idx < RLV_ATTACHGROUP_COUNT; idx++)
		if (cstrAttachGroups[idx] == strGroup)
			return static_cast<ERlvAttachGroupType>(idx);
	return RLV_ATTACHGROUP_INVALID;
}

// =========================================================================
// String helper functions
//

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetFirstParenthesisedText(const std::string& strText, std::string::size_type* pidxMatch /*=NULL*/)
{
	if (pidxMatch)
		*pidxMatch = std::string::npos;	// Assume we won't find anything

	std::string::size_type idxIt, idxStart; int cntLevel = 1;
	if ((idxStart = strText.find_first_of('(')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxStart;
	while ( (cntLevel > 0) && (++idxIt < strText.length()) )
	{
		if ('(' == pstrText[idxIt])
			cntLevel++;
		else if (')' == pstrText[idxIt])
			cntLevel--;
	}

	if (idxIt < strText.length())
	{
		if (pidxMatch)
			*pidxMatch = idxStart;	// Return the character index of the starting '('
		return strText.substr(idxStart + 1, idxIt - idxStart - 1);
	}
	return std::string();
}

// Checked: 2009-07-29 (RLVa-1.0.1b) | Added: RLVa-1.0.1b
std::string rlvGetLastParenthesisedText(const std::string& strText, std::string::size_type* pidxStart /*=NULL*/)
{
	if (pidxStart)
		*pidxStart = std::string::npos;	// Assume we won't find anything

	// Extracts the last - matched - parenthesised text from the input string
	std::string::size_type idxIt, idxEnd; int cntLevel = 1;
	if ((idxEnd = strText.find_last_of(')')) == std::string::npos)
		return std::string();

	const char* pstrText = strText.c_str(); idxIt = idxEnd;
	while ( (cntLevel > 0) && (--idxIt >= 0) && (idxIt < strText.length()) )
	{
		if (')' == pstrText[idxIt])
			cntLevel++;
		else if ('(' == pstrText[idxIt])
			cntLevel--;
	}

	if ( (idxIt >= 0) && (idxIt < strText.length()) )	// NOTE: allow for std::string::size_type to be signed or unsigned
	{
		if (pidxStart)
			*pidxStart = idxIt;		// Return the character index of the starting '('
		return strText.substr(idxIt + 1, idxEnd - idxIt - 1);
	}
	return std::string();
}

// =========================================================================
