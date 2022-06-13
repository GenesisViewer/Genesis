/**
 * @file llsidepanelinventory.cpp
 * @brief LLPanelObjectInventory class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

//*****************************************************************************
//
// Implementation of the panel inventory - used to view and control a
// task's inventory.
//
// *****************************************************************************

#include "llviewerprecompiledheaders.h"

#include "llpanelobjectinventory.h"

#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llcallbacklist.h"
#include "llfloaterbuycontents.h"
#include "llfloaterbuycurrency.h"
#include "llfolderview.h"
#include "llinventorybridge.h"
#include "llinventorydefines.h"
#include "llinventoryfilter.h"
#include "llinventoryfunctions.h"
#include "llinventoryicon.h"
#include "llpreviewanim.h"
#include "llpreviewgesture.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewsound.h"
#include "llpreviewtexture.h"
#include "llscrollcontainer.h"
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llviewerassettype.h"
#include "llviewerinventory.h"
#include "llviewerregion.h"
#include "llviewerobjectlist.h"
#include "llviewermessage.h"
// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.2.0b)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

#include "hippogridmanager.h"


///----------------------------------------------------------------------------
/// Class LLTaskInvFVBridge
///----------------------------------------------------------------------------

class LLTaskInvFVBridge : public LLFolderViewEventListener
{
protected:
	LLUUID mUUID;
	std::string mName;
	mutable std::string mDisplayName;
	LLPanelObjectInventory* mPanel;
	U32 mFlags;
	LLAssetType::EType mAssetType;	
	LLInventoryType::EType mInventoryType;

	LLInventoryObject* findInvObject() const;
	LLInventoryItem* findItem() const;

public:
	LLTaskInvFVBridge(LLPanelObjectInventory* panel,
					  const LLUUID& uuid,
					  const std::string& name,
					  U32 flags=0);
	virtual ~LLTaskInvFVBridge() {}

	virtual LLFontGL::StyleFlags getLabelStyle() const { return LLFontGL::NORMAL; }
	virtual std::string getLabelSuffix() const { return LLStringUtil::null; }

	static LLTaskInvFVBridge* createObjectBridge(LLPanelObjectInventory* panel,
												 LLInventoryObject* object);
	void showProperties();
	void buyItem();
	S32 getPrice();
	static bool commitBuyItem(const LLSD& notification, const LLSD& response);

	// LLFolderViewEventListener functionality
	virtual const std::string& getName() const;
	virtual const std::string& getDisplayName() const;
	virtual PermissionMask getPermissionMask() const { return PERM_NONE; }
	/*virtual*/ LLFolderType::EType getPreferredType() const { return LLFolderType::FT_NONE; }
	virtual const LLUUID& getUUID() const { return mUUID; }
	virtual time_t getCreationDate() const;
	virtual LLUIImagePtr getIcon() const;
	virtual void openItem();
	virtual BOOL canOpenItem() const { return FALSE; }
	virtual void closeItem() {}
	virtual void previewItem();
	virtual void selectItem() {}
	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL isItemMovable() const;
	virtual BOOL isItemRemovable() const;
	virtual BOOL removeItem();
	virtual void removeBatch(std::vector<LLFolderViewEventListener*>& batch);
	virtual void move(LLFolderViewEventListener* parent_listener);
	virtual BOOL isItemCopyable() const;
	virtual BOOL copyToClipboard() const;
	virtual BOOL cutToClipboard();
	virtual BOOL isClipboardPasteable() const;
	virtual void pasteFromClipboard(bool only_copies = false);
	virtual void pasteLinkFromClipboard();
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual void performAction(LLInventoryModel* model, std::string action);
	virtual BOOL isUpToDate() const { return TRUE; }
	virtual bool hasChildren() const { return FALSE; }
	virtual LLInventoryType::EType getInventoryType() const { return LLInventoryType::EType::IT_NONE; }
	virtual LLWearableType::EType getWearableType() const { return LLWearableType::EType::WT_NONE; }

	// LLDragAndDropBridge functionality
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
//	virtual void dropped();

};

LLTaskInvFVBridge::LLTaskInvFVBridge(
	LLPanelObjectInventory* panel,
	const LLUUID& uuid,
	const std::string& name,
	U32 flags):
	mUUID(uuid),
	mName(name),
	mPanel(panel),
	mFlags(flags),
	mAssetType(LLAssetType::AT_NONE),
	mInventoryType(LLInventoryType::EType::IT_NONE)
{
	const LLInventoryItem *item = findItem();
	if (item)
	{
		mAssetType = item->getType();
		mInventoryType = item->getInventoryType();
	}
}

LLInventoryObject* LLTaskInvFVBridge::findInvObject() const
{
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if (object)
	{
		return object->getInventoryObject(mUUID);
	}
	return NULL;
}


LLInventoryItem* LLTaskInvFVBridge::findItem() const
{
	return dynamic_cast<LLInventoryItem*>(findInvObject());
}

void LLTaskInvFVBridge::showProperties()
{
	if(!LLFloaterProperties::show(mUUID, mPanel->getTaskUUID()))
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PropertiesRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );
		LLFloaterProperties* floater = new LLFloaterProperties("obj item properties",
													   rect,
													   "Object Inventory Item Properties",
													   mUUID,
													   mPanel->getTaskUUID());
		floater->open();		/*Flawfinder: ignore*/
	}
}

void LLTaskInvFVBridge::buyItem()
{
	LL_INFOS() << "LLTaskInvFVBridge::buyItem()" << LL_ENDL;
	LLInventoryItem* item = findItem();
	if(!item || !item->getSaleInfo().isForSale()) return;

	const LLSaleInfo& sale_info = item->getSaleInfo();
	const LLPermissions& perm = item->getPermissions();
	const std::string owner_name; // no owner name currently... FIXME?

	LLViewerObject* obj;
	if( ( obj = gObjectList.findObject( mPanel->getTaskUUID() ) ) && obj->isAttachment() )
	{
		LLNotificationsUtil::add("Cannot_Purchase_an_Attachment");
		LL_INFOS() << "Attempt to purchase an attachment" << LL_ENDL;
	}
	else
	{
        LLSD args;
        args["PRICE"] = llformat("%d",sale_info.getSalePrice());
        args["OWNER"] = owner_name;
        if (sale_info.getSaleType() != LLSaleInfo::FS_CONTENTS)
        {
        	U32 next_owner_mask = perm.getMaskNextOwner();
        	args["MODIFYPERM"] = LLTrans::getString((next_owner_mask & PERM_MODIFY) ? "PermYes" : "PermNo");
        	args["COPYPERM"] = LLTrans::getString((next_owner_mask & PERM_COPY) ? "PermYes" : "PermNo");
        	args["RESELLPERM"] = LLTrans::getString((next_owner_mask & PERM_TRANSFER) ? "PermYes" : "PermNo");
        }

		std::string alertdesc;
       	switch(sale_info.getSaleType())
       	{
       	  case LLSaleInfo::FS_ORIGINAL:
       		alertdesc = owner_name.empty() ? "BuyOriginalNoOwner" : "BuyOriginal";
       		break;
       	  case LLSaleInfo::FS_CONTENTS:
       		alertdesc = owner_name.empty() ? "BuyContentsNoOwner" : "BuyContents";
       		break;
		  case LLSaleInfo::FS_COPY:
       	  default:
       		alertdesc = owner_name.empty() ? "BuyCopyNoOwner" : "BuyCopy";
       		break;
       	}

		LLSD payload;
		payload["task_id"] = mPanel->getTaskUUID();
		payload["item_id"] = mUUID;
		payload["type"] = item->getType();
		LLNotificationsUtil::add(alertdesc, args, payload, LLTaskInvFVBridge::commitBuyItem);
	}
}

S32 LLTaskInvFVBridge::getPrice()
{
	LLInventoryItem* item = findItem();
	if(item)
	{
		return item->getSaleInfo().getSalePrice();
	}
	else
	{
		return -1;
	}
}

// static
bool LLTaskInvFVBridge::commitBuyItem(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(0 == option)
	{
		LLViewerObject* object = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if(!object || !object->getRegion()) return false;


		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_BuyObjectInventory);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_Data);
		msg->addUUIDFast(_PREHASH_ObjectID, notification["payload"]["task_id"].asUUID());
		msg->addUUIDFast(_PREHASH_ItemID, notification["payload"]["item_id"].asUUID());
		msg->addUUIDFast(_PREHASH_FolderID,
		//"type" should be LLAssetType::AssetType, not LLFolderType::EType
			gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType((LLAssetType::EType)notification["payload"]["type"].asInteger())));
		msg->sendReliable(object->getRegion()->getHost());
	}
	return false;
}

const std::string& LLTaskInvFVBridge::getName() const
{
	return mName;
}

const std::string& LLTaskInvFVBridge::getDisplayName() const
{
	LLInventoryItem* item = findItem();

	if(item)
	{
		mDisplayName.assign(item->getName());

		// Localize "New Script", "New Script 1", "New Script 2", etc.
		if (item->getType() == LLAssetType::AT_LSL_TEXT &&
			LLStringUtil::startsWith(item->getName(), "New Script"))
		{
			LLStringUtil::replaceString(mDisplayName, "New Script", LLTrans::getString("PanelContentsNewScript"));
		}

		const LLPermissions& perm(item->getPermissions());
		BOOL copy = gAgent.allowOperation(PERM_COPY, perm, GP_OBJECT_MANIPULATE);
		BOOL mod  = gAgent.allowOperation(PERM_MODIFY, perm, GP_OBJECT_MANIPULATE);
		BOOL xfer = gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE);

		if(!copy)
		{
			mDisplayName.append(LLTrans::getString("no_copy"));
		}
		if(!mod)
		{
			mDisplayName.append(LLTrans::getString("no_modify"));
		}
		if(!xfer)
		{
			mDisplayName.append(LLTrans::getString("no_transfer"));
		}
	}

	return mDisplayName;
}

// BUG: No creation dates for task inventory
time_t LLTaskInvFVBridge::getCreationDate() const
{
	return 0;
}

LLUIImagePtr LLTaskInvFVBridge::getIcon() const
{
	const BOOL item_is_multi = (mFlags & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS);

	return LLInventoryIcon::getIcon(mAssetType, mInventoryType, 0, item_is_multi );
}

void LLTaskInvFVBridge::openItem()
{
	// no-op.
	LL_DEBUGS() << "LLTaskInvFVBridge::openItem()" << LL_ENDL;
}

void LLTaskInvFVBridge::previewItem()
{
	openItem();
}

BOOL LLTaskInvFVBridge::isItemRenameable() const
{
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		return FALSE;
	}
// [/RLVa:KB]

	if(gAgent.isGodlike()) return TRUE;
//	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		LLInventoryItem* item = (LLInventoryItem*)(object->getInventoryObject(mUUID));
		if(item && gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
										 GP_OBJECT_MANIPULATE, GOD_LIKE))
		{
			return TRUE;
		}
	}
	return FALSE;
}

BOOL LLTaskInvFVBridge::renameItem(const std::string& new_name)
{
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		return FALSE;
	}
// [/RLVa:KB]

//	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		LLViewerInventoryItem* item = NULL;
		item = (LLViewerInventoryItem*)object->getInventoryObject(mUUID);
		if(item && (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
										GP_OBJECT_MANIPULATE, GOD_LIKE)))
		{
			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
			new_item->rename(new_name);
			object->updateInventory(
				new_item,
				TASK_INVENTORY_ITEM_KEY,
				false);
		}
	}
	return TRUE;
}

BOOL LLTaskInvFVBridge::isItemMovable() const
{
	//LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	//if(object && (object->permModify() || gAgent.isGodlike()))
	//{
	//	return TRUE;
	//}
	//return FALSE;
// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
	if (rlv_handler_t::isEnabled())
	{
		const LLViewerObject* pObj = gObjectList.findObject(mPanel->getTaskUUID());
		if (pObj)
		{
			if (gRlvAttachmentLocks.isLockedAttachment(pObj->getRootEdit()))
			{
				return FALSE;
			}
			else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
			{
				LLVOAvatar* pAvatar = gAgentAvatarp;
				if ( (pAvatar) && (pAvatar->isSitting()) && (pAvatar->getRoot() == pObj->getRootEdit()) )
					return FALSE;
			}
		}
	}
// [/RLVa:KB]
	return TRUE;
}

BOOL LLTaskInvFVBridge::isItemRemovable() const
{
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
	if ( (object) && (rlv_handler_t::isEnabled()) )
	{
		const LLViewerObject* pObjRoot = object->getRootEdit();
		if (gRlvAttachmentLocks.isLockedAttachment(pObjRoot))
		{
			return FALSE;
		}
		else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
		{
			LLVOAvatar* pAvatar = gAgentAvatarp;
			if ( (pAvatar) && (pAvatar->isSitting()) && (pAvatar->getRoot() == pObjRoot) )
				return FALSE;
		}
	}
// [/RLVa:KB]

	if(object
	   && (object->permModify() || object->permYouOwner()))
	{
		return TRUE;
	}
	return FALSE;
}

bool remove_task_inventory_callback(const LLSD& notification, const LLSD& response, LLPanelObjectInventory* panel)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLViewerObject* object = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
	if(option == 0 && object)
	{
		// yes
		for (auto const& entry : notification["payload"]["inventory_ids"].array())
		{
			object->removeInventory(entry.asUUID());
		}

		// refresh the UI.
		panel->refresh();
	}
	return false;
}

// helper for remove
// ! REFACTOR ! two_uuids_list_t is also defined in llinventorybridge.h, but differently.
typedef std::pair<LLUUID, std::list<LLUUID> > panel_two_uuids_list_t;
typedef std::pair<LLPanelObjectInventory*, panel_two_uuids_list_t> remove_data_t;
BOOL LLTaskInvFVBridge::removeItem()
{
	if(isItemRemovable() && mPanel)
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			if(object->permModify())
			{
				// just do it.
				object->removeInventory(mUUID);
				return TRUE;
			}
			else
			{
				LLSD payload;
				payload["task_id"] = mPanel->getTaskUUID();
				payload["inventory_ids"].append(mUUID);
				LLNotificationsUtil::add("RemoveItemWarn", LLSD(), payload, boost::bind(&remove_task_inventory_callback, _1, _2, mPanel));
				return FALSE;
			}
		}
	}
	return FALSE;
}

void LLTaskInvFVBridge::removeBatch(std::vector<LLFolderViewEventListener*>& batch)
{
	if (!mPanel)
	{
		return;
	}

	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if (!object)
	{
		return;
	}

	if (!object->permModify())
	{
		LLSD payload;
		payload["task_id"] = mPanel->getTaskUUID();
		for (S32 i = 0; i < (S32)batch.size(); i++)
		{
			LLTaskInvFVBridge* itemp = (LLTaskInvFVBridge*)batch[i];
			payload["inventory_ids"].append(itemp->getUUID());
		}
		LLNotificationsUtil::add("RemoveItemWarn", LLSD(), payload, boost::bind(&remove_task_inventory_callback, _1, _2, mPanel));
		
	}
	else
	{
		for (S32 i = 0; i < (S32)batch.size(); i++)
		{
			LLTaskInvFVBridge* itemp = (LLTaskInvFVBridge*)batch[i];

			if(itemp->isItemRemovable())
			{
				// just do it.
				object->removeInventory(itemp->getUUID());
			}
		}
	}
}

void LLTaskInvFVBridge::move(LLFolderViewEventListener* parent_listener)
{
}

BOOL LLTaskInvFVBridge::isItemCopyable() const
{
	LLInventoryItem* item = findItem();
	if(!item) return FALSE;
	return gAgent.allowOperation(PERM_COPY, item->getPermissions(),
								GP_OBJECT_MANIPULATE);
}

BOOL LLTaskInvFVBridge::copyToClipboard() const
{
	return FALSE;
}

BOOL LLTaskInvFVBridge::cutToClipboard()
{
	return FALSE;
}

BOOL LLTaskInvFVBridge::isClipboardPasteable() const
{
	return FALSE;
}

void LLTaskInvFVBridge::pasteFromClipboard(bool)
{
}

void LLTaskInvFVBridge::pasteLinkFromClipboard()
{
}

BOOL LLTaskInvFVBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	//LL_INFOS() << "LLTaskInvFVBridge::startDrag()" << LL_ENDL;
	if(mPanel)
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			LLInventoryItem* inv = NULL;
			if((inv = (LLInventoryItem*)object->getInventoryObject(mUUID)))
			{
				const LLPermissions& perm = inv->getPermissions();
				bool can_copy = gAgent.allowOperation(PERM_COPY, perm,
														GP_OBJECT_MANIPULATE);
// [RLVa:KB] - Checked: 2009-10-10 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
				// Kind of redundant due to the note below, but in case that ever gets fixed
				if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
				{
					return FALSE;
				}
// [/RLVa:KB]
				if (object->isAttachment() && !can_copy)
				{
                    //RN: no copy contents of attachments cannot be dragged out
                    // due to a race condition and possible exploit where
                    // attached objects do not update their inventory items
                    // when their contents are manipulated
                    return FALSE;
				}
				if((can_copy && perm.allowTransferTo(gAgent.getID()))
				   || object->permYouOwner())
//				   || gAgent.isGodlike())

				{
					*type = LLViewerAssetType::lookupDragAndDropType(inv->getType());

					*id = inv->getUUID();
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

BOOL LLTaskInvFVBridge::dragOrDrop(MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data)
{
	//LL_INFOS() << "LLTaskInvFVBridge::dragOrDrop()" << LL_ENDL;
	return FALSE;
}

void pack_script_message(LLMessageSystem*, const LLInventoryItem*, const LLViewerObject*);

void reset_script(const LLInventoryItem* item, const LLViewerObject* obj)
{
	if (!item || !obj) return;
	gMessageSystem->newMessageFast(_PREHASH_ScriptReset);
	pack_script_message(gMessageSystem, item, obj);
	gMessageSystem->sendReliable(obj->getRegion()->getHost());
}

void set_script_running(bool running, const LLInventoryItem* item, const LLViewerObject* obj)
{
	if (!item || !obj) return;
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_SetScriptRunning);
	pack_script_message(msg, item, obj);
	msg->addBOOLFast(_PREHASH_Running, running);
	msg->sendReliable(obj->getRegion()->getHost());
}

void pack_script_message(LLMessageSystem* msg, const LLInventoryItem* item, const LLViewerObject* obj)
{
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Script);
	msg->addUUIDFast(_PREHASH_ObjectID, obj->getID());
	msg->addUUIDFast(_PREHASH_ItemID, item->getUUID());
}

// virtual
void LLTaskInvFVBridge::performAction(LLInventoryModel* model, std::string action)
{
	if (action == "task_buy")
	{
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			LL_WARNS() << "label_buy_task_bridged_item: Invalid price" << LL_ENDL;
		}
		else
		{
			if (price > 0 && price > gStatusBar->getBalance())
			{
				LLStringUtil::format_map_t args;
				args["CURRENCY"] = gHippoGridManager->getConnectedGrid()->getCurrencySymbol();
				LLFloaterBuyCurrency::buyCurrency( LLTrans::getString("this_costs", args), price );
			}
			else
			{
				buyItem();
			}
		}
	}
	else if (action == "task_open")
	{
		openItem();
	}
	else if (action == "task_properties")
	{
		showProperties();
	}
	else if (action == "reset_script")
	{
		reset_script(findItem(), gObjectList.findObject(mPanel->getTaskUUID()));
	}
	else if (action == "start_script")
	{
		set_script_running(true, findItem(), gObjectList.findObject(mPanel->getTaskUUID()));
	}
	else if (action == "stop_script")
	{
		set_script_running(false, findItem(), gObjectList.findObject(mPanel->getTaskUUID()));
	}
}

void LLTaskInvFVBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryItem* item = findItem();
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	
	if (!item)
	{
		hide_context_entries(menu, items, disabled_items);
		return;
	}

	if(!gAgent.allowOperation(PERM_OWNER, item->getPermissions(),
							 GP_OBJECT_MANIPULATE)
	   && item->getSaleInfo().isForSale())
	{
		items.push_back(std::string("Task Buy"));

		std::string label= LLTrans::getString("Buy");
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			LL_WARNS() << "label_buy_task_bridged_item: Invalid price" << LL_ENDL;
		}
		else
		{
			std::ostringstream info;
			info << LLTrans::getString("Buyfor") << gHippoGridManager->getConnectedGrid()->getCurrencySymbol() << price;
			label.assign(info.str());
		}

		const LLView::child_list_t *list = menu.getChildList();
		LLView::child_list_t::const_iterator itor;
		for (itor = list->begin(); itor != list->end(); ++itor)
		{
			std::string name = (*itor)->getName();
			LLMenuItemCallGL* menu_itemp = dynamic_cast<LLMenuItemCallGL*>(*itor);
			if (name == "Task Buy" && menu_itemp)
			{
				menu_itemp->setLabel(label);
			}
		}
	}
	else if (canOpenItem())
	{
		if (LLAssetType::AT_LSL_TEXT == item->getType())
		{
			items.push_back(std::string("Task Reset"));
			items.push_back(std::string("Task Set Running"));
			items.push_back(std::string("Task Set Not Running"));
			const LLViewerObject* obj = gObjectList.findObject(mPanel->getTaskUUID());
			if (!obj || !(obj->permModify() || obj->permYouOwner()))
			{
				disabled_items.push_back(std::string("Task Reset"));
				disabled_items.push_back(std::string("Task Set Running"));
				disabled_items.push_back(std::string("Task Set Not Running"));
			}
		}

		items.push_back(std::string("Task Open"));
		if (!isItemCopyable())
		{
			disabled_items.push_back(std::string("Task Open"));
		}
// [RLVa:KB] - Checked: 2010-03-01 (RLVa-1.2.0b) | Modified: RLVa-1.1.0a
		else if (rlv_handler_t::isEnabled())
		{
			LLViewerObject* pAttachObj = gObjectList.findObject(mPanel->getTaskUUID());
			bool fLocked = (pAttachObj) ? gRlvAttachmentLocks.isLockedAttachment(pAttachObj->getRootEdit()) : false;
			if ( ((LLAssetType::AT_NOTECARD == item->getType()) && ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) || (fLocked))) || 
				 ((LLAssetType::AT_LSL_TEXT == item->getType()) && ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) || (fLocked))) ||
				 ((LLAssetType::AT_TEXTURE == item->getType()) && (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))) )
			{
				disabled_items.push_back(std::string("Task Open"));
			}
		}
// [/RLVa:KB]
	}
	items.push_back(std::string("Task Properties"));
//	if(isItemRenameable())
//	{
//		items.push_back(std::string("Task Rename"));
//	}
//	if(isItemRemovable())
//	{
//		items.push_back(std::string("Task Remove"));
//	}
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Added: RLVa-1.2.1f
	items.push_back(std::string("Task Rename"));
	items.push_back(std::string("Task Remove"));
	if (!isItemRenameable())
	{
		disabled_items.push_back(std::string("Task Rename"));
	}
	if (!isItemRemovable())
	{
		disabled_items.push_back(std::string("Task Remove"));
	}
// [/RLVa:KB]

	hide_context_entries(menu, items, disabled_items);
}


///----------------------------------------------------------------------------
/// Class LLTaskFolderBridge
///----------------------------------------------------------------------------

class LLTaskCategoryBridge : public LLTaskInvFVBridge
{
public:
	LLTaskCategoryBridge(
		LLPanelObjectInventory* panel,
		const LLUUID& uuid,
		const std::string& name);

	virtual LLUIImagePtr getIcon() const;
	virtual const std::string& getDisplayName() const;
	virtual BOOL isItemRenameable() const;
	// virtual BOOL isItemCopyable() const { return FALSE; }
	virtual BOOL renameItem(const std::string& new_name);
	virtual BOOL isItemRemovable() const;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	virtual bool hasChildren() const;
	virtual BOOL startDrag(EDragAndDropType* type, LLUUID* id) const;
	virtual BOOL dragOrDrop(MASK mask, BOOL drop,
							EDragAndDropType cargo_type,
							void* cargo_data);
	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
};

LLTaskCategoryBridge::LLTaskCategoryBridge(
	LLPanelObjectInventory* panel,
	const LLUUID& uuid,
	const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name)
{
}

LLUIImagePtr LLTaskCategoryBridge::getIcon() const
{
	return LLUI::getUIImage("inv_folder_plain_closed.tga");
}

// virtual
const std::string& LLTaskCategoryBridge::getDisplayName() const
{
	return getName();
}

BOOL LLTaskCategoryBridge::isItemRenameable() const
{
	return FALSE;
}

BOOL LLTaskCategoryBridge::renameItem(const std::string& new_name)
{
	return FALSE;
}

BOOL LLTaskCategoryBridge::isItemRemovable() const
{
	return FALSE;
}

void LLTaskCategoryBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;
	hide_context_entries(menu, items, disabled_items);
}

bool LLTaskCategoryBridge::hasChildren() const
{
	// return TRUE if we have or do know know if we have children.
	// *FIX: For now, return FALSE - we will know for sure soon enough.
	return FALSE;
}

void LLTaskCategoryBridge::openItem()
{
}

BOOL LLTaskCategoryBridge::startDrag(EDragAndDropType* type, LLUUID* id) const
{
	//LL_INFOS() << "LLTaskInvFVBridge::startDrag()" << LL_ENDL;
	if(mPanel && mUUID.notNull())
	{
		LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
		if(object)
		{
			const LLInventoryObject* cat = object->getInventoryObject(mUUID);
			if ( (cat) && (move_inv_category_world_to_agent(mUUID, LLUUID::null, FALSE)) )
			{
				*type = LLViewerAssetType::lookupDragAndDropType(cat->getType());
				*id = mUUID;
				return TRUE;
			}
		}
	}
	return FALSE;
}

BOOL LLTaskCategoryBridge::dragOrDrop(MASK mask, BOOL drop,
									  EDragAndDropType cargo_type,
									  void* cargo_data)
{
	//LL_INFOS() << "LLTaskCategoryBridge::dragOrDrop()" << LL_ENDL;
	BOOL accept = FALSE;
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(object)
	{
		switch(cargo_type)
		{
		case DAD_CATEGORY:
			accept = LLToolDragAndDrop::getInstance()->dadUpdateInventoryCategory(object,drop);
			break;
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
		case DAD_CALLINGCARD:
		case DAD_MESH:
			accept = LLToolDragAndDrop::isInventoryDropAcceptable(object, (LLViewerInventoryItem*)cargo_data);
			if(accept && drop)
			{
				LLToolDragAndDrop::dropInventory(object,
												 (LLViewerInventoryItem*)cargo_data,
												 LLToolDragAndDrop::getInstance()->getSource(),
												 LLToolDragAndDrop::getInstance()->getSourceID());
			}
			break;
		case DAD_SCRIPT:
			// *HACK: In order to resolve SL-22177, we need to block
			// drags from notecards and objects onto other
			// objects. uncomment the simpler version when we have
			// that right.
			//accept = LLToolDragAndDrop::isInventoryDropAcceptable(object, (LLViewerInventoryItem*)cargo_data);
			if(LLToolDragAndDrop::isInventoryDropAcceptable(
				   object, (LLViewerInventoryItem*)cargo_data)
			   && (LLToolDragAndDrop::SOURCE_WORLD != LLToolDragAndDrop::getInstance()->getSource())
			   && (LLToolDragAndDrop::SOURCE_NOTECARD != LLToolDragAndDrop::getInstance()->getSource()))
			{
				accept = TRUE;
			}
			if(accept && drop)
			{
				LLViewerInventoryItem* item = (LLViewerInventoryItem*)cargo_data;
				// rez in the script active by default, rez in
				// inactive if the control key is being held down.
				BOOL active = ((mask & MASK_CONTROL) == 0);
				LLToolDragAndDrop::dropScript(object, item, active,
											  LLToolDragAndDrop::getInstance()->getSource(),
											  LLToolDragAndDrop::getInstance()->getSourceID());
			}
			break;
		default:
			break;
		}
	}
	return accept;
}

///----------------------------------------------------------------------------
/// Class LLTaskTextureBridge
///----------------------------------------------------------------------------

class LLTaskTextureBridge : public LLTaskInvFVBridge
{
public:
	LLTaskTextureBridge(LLPanelObjectInventory* panel,
						const LLUUID& uuid,
						const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
};

void LLTaskTextureBridge::openItem()
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_TEXTURE);
		return;
	}
// [/RLVa:KB]

	LL_INFOS() << "LLTaskTextureBridge::openItem()" << LL_ENDL;
	if(!LLPreview::show(mUUID))
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
		rect.translate( left - rect.mLeft, top - rect.mTop );
		LLPreviewTexture* preview = new LLPreviewTexture("preview task texture",
												 rect,
												 getName(),
												 mUUID,
												 mPanel->getTaskUUID());
		preview->setFocus(TRUE);
	}
}


///----------------------------------------------------------------------------
/// Class LLTaskSoundBridge
///----------------------------------------------------------------------------

class LLTaskSoundBridge : public LLTaskInvFVBridge
{
public:
	LLTaskSoundBridge(LLPanelObjectInventory* panel,
					  const LLUUID& uuid,
					  const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
	virtual void performAction(LLInventoryModel* model, std::string action);
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags);
	static void openSoundPreview(void* data);
};

void LLTaskSoundBridge::openItem()
{
	openSoundPreview((void*)this);
}

void LLTaskSoundBridge::openSoundPreview(void* data)
{
	LLTaskSoundBridge* self = (LLTaskSoundBridge*)data;
	if(!self)
		return;
	if(!LLPreview::show(self->mUUID))
	{
		// There isn't one, so make a new preview
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewSoundRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewSound* floaterp = 	new LLPreviewSound("preview task sound",
							   rect,
							   self->getName(),
							   self->mUUID,
							   self->mPanel->getTaskUUID());
		floaterp->open();	/*Flawfinder: ignore*/
	}
}

// virtual
void LLTaskSoundBridge::performAction(LLInventoryModel* model, std::string action)
{
	if (action == "task_play")
	{
		LLInventoryItem* item = findItem();
		if(item)
		{
			send_sound_trigger(item->getAssetUUID(), 1.0);
		}
	}
	LLTaskInvFVBridge::performAction(model, action);
}

void LLTaskSoundBridge::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	std::vector<std::string> items;
	std::vector<std::string> disabled_items;

	if(item->getPermissions().getOwner() != gAgent.getID()
	   && item->getSaleInfo().isForSale())
	{
		items.push_back(std::string("Task Buy"));

		std::string label= LLTrans::getString("Buy");
		// Check the price of the item.
		S32 price = getPrice();
		if (-1 == price)
		{
			LL_WARNS() << "label_buy_task_bridged_item: Invalid price" << LL_ENDL;
		}
		else
		{
			std::ostringstream info;
			info <<  LLTrans::getString("Buyfor") << gHippoGridManager->getConnectedGrid()->getCurrencySymbol() << price;
			label.assign(info.str());
		}

		const LLView::child_list_t *list = menu.getChildList();
		LLView::child_list_t::const_iterator itor;
		for (itor = list->begin(); itor != list->end(); ++itor)
		{
			std::string name = (*itor)->getName();
			LLMenuItemCallGL* menu_itemp = dynamic_cast<LLMenuItemCallGL*>(*itor);
			if (name == "Task Buy" && menu_itemp)
			{
				menu_itemp->setLabel(label);
			}
		}
	}
	else if (canOpenItem())
	{
		if (!isItemCopyable())
		{
			disabled_items.push_back(std::string("Task Open"));
		}
	}
	items.push_back(std::string("Task Properties"));
	if(isItemRenameable())
	{
		items.push_back(std::string("Task Rename"));
	}
	if(isItemRemovable())
	{
		items.push_back(std::string("Task Remove"));
	}

	items.push_back(std::string("Task Play"));

	/*menu.addSeparator();
	menu.append(new LLMenuItemCallGL("Play",
									 &LLTaskSoundBridge::playSound,
									 NULL,
									 (void*)this));*/

	hide_context_entries(menu, items, disabled_items);
}

///----------------------------------------------------------------------------
/// Class LLTaskLandmarkBridge
///----------------------------------------------------------------------------

class LLTaskLandmarkBridge : public LLTaskInvFVBridge
{
public:
	LLTaskLandmarkBridge(LLPanelObjectInventory* panel,
						 const LLUUID& uuid,
						 const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}
};

///----------------------------------------------------------------------------
/// Class LLTaskCallingCardBridge
///----------------------------------------------------------------------------

class LLTaskCallingCardBridge : public LLTaskInvFVBridge
{
public:
	LLTaskCallingCardBridge(LLPanelObjectInventory* panel,
							const LLUUID& uuid,
							const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL isItemRenameable() const;
	virtual BOOL renameItem(const std::string& new_name);
};

BOOL LLTaskCallingCardBridge::isItemRenameable() const
{
	return FALSE;
}

BOOL LLTaskCallingCardBridge::renameItem(const std::string& new_name)
{
	return FALSE;
}


///----------------------------------------------------------------------------
/// Class LLTaskScriptBridge
///----------------------------------------------------------------------------

class LLTaskScriptBridge : public LLTaskInvFVBridge
{
public:
	LLTaskScriptBridge(LLPanelObjectInventory* panel,
					   const LLUUID& uuid,
					   const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	//static BOOL enableIfCopyable( void* userdata );
};

class LLTaskLSLBridge : public LLTaskScriptBridge
{
public:
	LLTaskLSLBridge(LLPanelObjectInventory* panel,
					const LLUUID& uuid,
					const std::string& name) :
		LLTaskScriptBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
	virtual BOOL removeItem();
	//virtual void buildContextMenu(LLMenuGL& menu);

	//static void copyToInventory(void* userdata);
};

void LLTaskLSLBridge::openItem()
{
	LL_INFOS() << "LLTaskLSLBridge::openItem() " << mUUID << LL_ENDL;
	if(LLLiveLSLEditor::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

// [RLVa:KB] - Checked: 2010-03-27 (RLVa-1.1.3b) | Modified: RLVa-1.1.0a
	if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_SCRIPT);
		return;
	}
// [/RLVa:KB]

	if(object->permModify() || gAgent.isGodlike())
	{
		std::string title("Script: ");
		LLInventoryItem* item = findItem();
		if (item)
		{
			title.append(item->getName());
		}

		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLLiveLSLEditor* editor;
		editor = new LLLiveLSLEditor("lsl ed",
									   rect,
									   title,
									   mPanel->getTaskUUID(),
									   mUUID);
		LLMultiFloater* previous_host = LLFloater::getFloaterHost();
		LLFloater::setFloaterHost(NULL);
		editor->open();	/*Flawfinder: ignore*/
		LLFloater::setFloaterHost(previous_host);

		// keep onscreen
		gFloaterView->adjustToFitScreen(editor, FALSE);
	}
}

BOOL LLTaskLSLBridge::removeItem()
{
	LLLiveLSLEditor::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskObjectBridge
///----------------------------------------------------------------------------

class LLTaskObjectBridge : public LLTaskInvFVBridge
{
public:
	LLTaskObjectBridge(LLPanelObjectInventory* panel,
					   const LLUUID& uuid,
					   const std::string& name,
					   U32 flags = 0) :
		LLTaskInvFVBridge(panel, uuid, name, flags) {}
};

///----------------------------------------------------------------------------
/// Class LLTaskNotecardBridge
///----------------------------------------------------------------------------

class LLTaskNotecardBridge : public LLTaskInvFVBridge
{
public:
	LLTaskNotecardBridge(LLPanelObjectInventory* panel,
						 const LLUUID& uuid,
						 const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
	virtual BOOL removeItem();
};

void LLTaskNotecardBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.3b) | Modified: RLVa-1.1.0a
	if ( (rlv_handler_t::isEnabled()) && 
		 ((gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) || (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit()))) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_NOTECARD);
		return;
	}
// [/RLVa:KB]
	if(object->permModify() || gAgent.isGodlike())
	{
		S32 left, top;
		gFloaterView->getNewFloaterPosition(&left, &top);
		LLRect rect = gSavedSettings.getRect("PreviewScriptRect");
		rect.translate(left - rect.mLeft, top - rect.mTop);
		LLPreviewNotecard* preview;
		preview = new LLPreviewNotecard("live notecard editor",
										 rect,
										 getName(),
										 mUUID,
										 mPanel->getTaskUUID());
		preview->setFocus(TRUE);  // if you're opening a notecard from an object's inventory, it takes focus

		// keep onscreen
		gFloaterView->adjustToFitScreen(preview, FALSE);
	}
}

BOOL LLTaskNotecardBridge::removeItem()
{
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskGestureBridge
///----------------------------------------------------------------------------

class LLTaskGestureBridge : public LLTaskInvFVBridge
{
public:
	LLTaskGestureBridge(LLPanelObjectInventory* panel,
						const LLUUID& uuid,
						const std::string& name) :
	LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
	virtual BOOL removeItem();
};

void LLTaskGestureBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

	// TODO: save rectangle
	std::string title = getName();
	LLUUID item_id = mUUID;
	LLUUID object_id = mPanel->getTaskUUID();
	LLPreviewGesture* preview = LLPreviewGesture::show(title, item_id, object_id);

	// keep onscreen
	gFloaterView->adjustToFitScreen(preview, FALSE);
}

BOOL LLTaskGestureBridge::removeItem()
{
	// Don't need to deactivate gesture because gestures inside objects
	// can never be active.
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskAnimationBridge
///----------------------------------------------------------------------------

class LLTaskAnimationBridge : public LLTaskInvFVBridge
{
public:
	LLTaskAnimationBridge(LLPanelObjectInventory* panel,
						  const LLUUID& uuid,
						  const std::string& name) :
		LLTaskInvFVBridge(panel, uuid, name) {}

	virtual BOOL canOpenItem() const { return TRUE; }
	virtual void openItem();
	virtual BOOL removeItem();
};

void LLTaskAnimationBridge::openItem()
{
	if(LLPreview::show(mUUID))
	{
		return;
	}
	LLViewerObject* object = gObjectList.findObject(mPanel->getTaskUUID());
	if(!object || object->isInventoryPending())
	{
		return;
	}

	// TODO: what permissions allow looking at animation?
	if(object->permModify() || gAgent.isGodlike())
	{
		// TODO: save rectangle
		std::string title = getName();
		LLUUID item_id = mUUID;
		LLUUID object_id = mPanel->getTaskUUID();

		if(!LLPreview::show(mUUID))
		{
			// There isn't one, so make a new preview
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewAnimRect");
			rect.translate(left - rect.mLeft, top - rect.mTop);

			LLPreviewAnim* preview = new LLPreviewAnim("preview anim",
									rect,
								   getName(),
								   mUUID,
								   LLPreviewAnim::NONE,
								   mPanel->getTaskUUID());
			preview->setFocus(TRUE);  // take focus if you're looking at one of these

			// Force to be entirely onscreen.
			gFloaterView->adjustToFitScreen(preview, FALSE);
		}
	}
}

BOOL LLTaskAnimationBridge::removeItem()
{
	LLPreview::hide(mUUID);
	return LLTaskInvFVBridge::removeItem();
}

///----------------------------------------------------------------------------
/// Class LLTaskWearableBridge
///----------------------------------------------------------------------------

class LLTaskWearableBridge : public LLTaskInvFVBridge
{
public:
	LLTaskWearableBridge(LLPanelObjectInventory* panel,
						 const LLUUID& uuid,
						 const std::string& name,
						 U32 flags) :
		LLTaskInvFVBridge(panel, uuid, name, flags) {}

	virtual LLUIImagePtr getIcon() const;
};

LLUIImagePtr LLTaskWearableBridge::getIcon() const
{
	return LLInventoryIcon::getIcon(mAssetType, mInventoryType, mFlags, FALSE );
}

///----------------------------------------------------------------------------
/// LLTaskInvFVBridge impl
//----------------------------------------------------------------------------

LLTaskInvFVBridge* LLTaskInvFVBridge::createObjectBridge(LLPanelObjectInventory* panel,
														 LLInventoryObject* object)
{
	LLTaskInvFVBridge* new_bridge = NULL;
	const LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(object);
	const U32 itemflags = ( NULL == item ? 0 : item->getFlags() );
	LLAssetType::EType type = object ? object->getType() : LLAssetType::AT_CATEGORY;
	LLUUID object_id = object ? object->getUUID() : LLUUID::null;
	std::string object_name = object ? object->getName() : std::string();

	switch(type)
	{
	case LLAssetType::AT_TEXTURE:
		new_bridge = new LLTaskTextureBridge(panel,
						     object_id,
						     object_name);
		break;
	case LLAssetType::AT_SOUND:
		new_bridge = new LLTaskSoundBridge(panel,
						   object_id,
						   object_name);
		break;
	case LLAssetType::AT_LANDMARK:
		new_bridge = new LLTaskLandmarkBridge(panel,
						      object_id,
						      object_name);
		break;
	case LLAssetType::AT_CALLINGCARD:
		new_bridge = new LLTaskCallingCardBridge(panel,
							 object_id,
							 object_name);
		break;
	case LLAssetType::AT_SCRIPT:
		// OLD SCRIPTS DEPRECATED - JC
		LL_WARNS() << "Old script" << LL_ENDL;
		//new_bridge = new LLTaskOldScriptBridge(panel,
		//									   object_id,
		//									   object_name);
		break;
	case LLAssetType::AT_OBJECT:
		new_bridge = new LLTaskObjectBridge(panel,
						    object_id,
						    object_name,
						    itemflags);
		break;
	case LLAssetType::AT_NOTECARD:
		new_bridge = new LLTaskNotecardBridge(panel,
						      object_id,
						      object_name);
		break;
	case LLAssetType::AT_ANIMATION:
		new_bridge = new LLTaskAnimationBridge(panel,
						       object_id,
						       object_name);
		break;
	case LLAssetType::AT_GESTURE:
		new_bridge = new LLTaskGestureBridge(panel,
						     object_id,
						     object_name);
		break;
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
		new_bridge = new LLTaskWearableBridge(panel,
						      object_id,
						      object_name,
						      itemflags);
		break;
	case LLAssetType::AT_CATEGORY:
		new_bridge = new LLTaskCategoryBridge(panel,
						      object_id,
						      object_name);
		break;
	case LLAssetType::AT_LSL_TEXT:
		new_bridge = new LLTaskLSLBridge(panel,
						 object_id,
						 object_name);
		break;
		break;
	default:
		LL_INFOS() << "Unhandled inventory type (llassetstorage.h): "
				<< (S32)type << LL_ENDL;
		break;
	}
	return new_bridge;
}


///----------------------------------------------------------------------------
/// Class LLPanelObjectInventory
///----------------------------------------------------------------------------

// Default constructor
LLPanelObjectInventory::LLPanelObjectInventory(const std::string& name, const LLRect& rect) :
	LLPanel(name, rect),
	mScroller(NULL),
	mFolders(NULL),
	mHaveInventory(FALSE),
	mIsInventoryEmpty(TRUE),
	mInventoryNeedsUpdate(FALSE)
{
	reset();
	// Callbacks
	init_object_inventory_panel_actions(this);
	gIdleCallbacks.addFunction(idle, this);
}

// Destroys the object
LLPanelObjectInventory::~LLPanelObjectInventory()
{
	if (!gIdleCallbacks.deleteFunction(idle, this))
	{
		LL_WARNS() << "LLPanelObjectInventory::~LLPanelObjectInventory() failed to delete callback" << LL_ENDL;
	}
}


void LLPanelObjectInventory::clearContents()
{
	mHaveInventory = FALSE;
	mIsInventoryEmpty = TRUE;
	if (LLToolDragAndDrop::getInstance() && LLToolDragAndDrop::getInstance()->getSource() == LLToolDragAndDrop::SOURCE_WORLD)
	{
		LLToolDragAndDrop::getInstance()->endDrag();
	}

	if( mScroller )
	{
		// removes mFolders
		removeChild( mScroller ); //*TODO: Really shouldn't do this during draw()/refresh()
		mScroller->die();
		mScroller = NULL;
		mFolders = NULL;
	}
}


void LLPanelObjectInventory::reset()
{
	clearContents();

	setBorderVisible(FALSE);

	LLRect dummy_rect(0, 1, 1, 0);
	mFolders = new LLFolderView(std::string("task inventory"), dummy_rect, getTaskUUID(), this, LLTaskInvFVBridge::createObjectBridge(this, NULL));
	// this ensures that we never say "searching..." or "no items found"
	mFolders->getFilter().setShowFolderState(LLInventoryFilter::SHOW_ALL_FOLDERS);

	LLRect scroller_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
	mScroller = new LLScrollContainer(std::string("task inventory scroller"), scroller_rect, mFolders );
	mScroller->setFollowsAll();
	addChild(mScroller);

	mFolders->setScrollContainer( mScroller );
}

void LLPanelObjectInventory::inventoryChanged(LLViewerObject* object,
										LLInventoryObject::object_list_t* inventory,
										S32 serial_num,
										void* data)
{
	if(!object) return;

	//LL_INFOS() << "invetnory arrived: \n"
	//		<< " panel UUID: " << panel->mTaskUUID << "\n"
	//		<< " task  UUID: " << object->mID << LL_ENDL;
	if(mTaskUUID == object->mID)
	{
		mInventoryNeedsUpdate = TRUE;
	}

	// refresh any properties floaters that are hanging around.
	if(inventory)
	{
		// We need to copy the ones that need refreshing onto a
		// temporary object because we cannot iterate through the
		// object inventory twice... A pox on stateful iteration!
		LLFloaterProperties* floater = NULL;
		std::vector<LLFloaterProperties*> refresh;

		LLInventoryObject::object_list_t::const_iterator it = inventory->begin();
		LLInventoryObject::object_list_t::const_iterator end = inventory->end();
		for( ;	it != end;	++it)
		{
			floater = LLFloaterProperties::find((*it)->getUUID(),
												object->getID());
			if(floater)
			{
				refresh.push_back(floater);
			}
		}
		S32 count = refresh.size();
		for(S32 i = 0; i < count; ++i)
		{
			refresh.at(i)->refresh();
		}
	}
}

void LLPanelObjectInventory::updateInventory()
{
	//LL_INFOS() << "inventory arrived: \n"
	//		<< " panel UUID: " << panel->mTaskUUID << "\n"
	//		<< " task  UUID: " << object->mID << LL_ENDL;
	// We're still interested in this task's inventory.
	uuid_set_t selected_items;
	BOOL inventory_has_focus = FALSE;
	if (mHaveInventory && mFolders)
	{
		selected_items = mFolders->getSelectionList();
		inventory_has_focus = gFocusMgr.childHasKeyboardFocus(mFolders);
	}

	LLViewerObject* objectp = gObjectList.findObject(mTaskUUID);
	if (objectp)
	{
		LLInventoryObject* inventory_root = objectp->getInventoryRoot();
		LLInventoryObject::object_list_t contents;
		objectp->getInventoryContents(contents);

		if (inventory_root)
		{
			reset();
			mIsInventoryEmpty = FALSE;
			createFolderViews(inventory_root, contents);
			mFolders->setEnabled(TRUE);
		}
		else
		{
			// TODO: create an empty inventory
			mIsInventoryEmpty = TRUE;
		}

		mHaveInventory = TRUE;
	}
	else
	{
		// TODO: create an empty inventory
		mIsInventoryEmpty = TRUE;
		mHaveInventory = TRUE;
	}

	// restore previous selection
	bool first_item = true;
	for (const auto id : selected_items)
	{
		LLFolderViewItem* selected_item = mFolders->getItemByID(id);

		if (selected_item)
		{
			//HACK: "set" first item then "change" each other one to get keyboard focus right
			if (first_item)
			{
				mFolders->setSelection(selected_item, TRUE, inventory_has_focus);
				first_item = FALSE;
			}
			else
			{
				mFolders->changeSelection(selected_item, TRUE);
			}
		}
	}

	if (mFolders)
	{
		mFolders->requestArrange();
	}
	mInventoryNeedsUpdate = FALSE;
	// Edit menu handler is set in onFocusReceived
}

// *FIX: This is currently a very expensive operation, because we have
// to iterate through the inventory one time for each category. This
// leads to an N^2 based on the category count. This could be greatly
// speeded with an efficient multimap implementation, but we don't
// have that in our current arsenal.
void LLPanelObjectInventory::createFolderViews(LLInventoryObject* inventory_root, LLInventoryObject::object_list_t& contents)
{
	if (!inventory_root)
	{
		return;
	}
	// Create a visible root category.
	LLTaskInvFVBridge* bridge = NULL;
	bridge = LLTaskInvFVBridge::createObjectBridge(this, inventory_root);
	if(bridge)
	{
		LLFolderViewFolder* new_folder = NULL;
		new_folder = new LLFolderViewFolder(inventory_root->getName(),
											bridge->getIcon(),
											bridge->getIconOpen(),
											NULL,
											mFolders,
											bridge);
		new_folder->addToFolder(mFolders, mFolders);
		new_folder->toggleOpen();

		createViewsForCategory(&contents, inventory_root, new_folder);
	}
}

typedef std::pair<LLInventoryObject*, LLFolderViewFolder*> obj_folder_pair;

void LLPanelObjectInventory::createViewsForCategory(LLInventoryObject::object_list_t* inventory, 
											  LLInventoryObject* parent,
											  LLFolderViewFolder* folder)
{
	// Find all in the first pass
	std::vector<obj_folder_pair*> child_categories;
	LLTaskInvFVBridge* bridge;
	LLFolderViewItem* view;

	LLInventoryObject::object_list_t::iterator it = inventory->begin();
	LLInventoryObject::object_list_t::iterator end = inventory->end();
	for( ; it != end; ++it)
	{
		LLInventoryObject* obj = *it;

		if(parent->getUUID() == obj->getParentUUID())
		{
			bridge = LLTaskInvFVBridge::createObjectBridge(this, obj);
			if(!bridge)
			{
				continue;
			}
			if(LLAssetType::AT_CATEGORY == obj->getType())
			{
				view = new LLFolderViewFolder(obj->getName(),
											  bridge->getIcon(),
											  bridge->getIconOpen(),
											  NULL,
											  mFolders,
											  bridge);
				child_categories.push_back(new obj_folder_pair(obj,
														 (LLFolderViewFolder*)view));
			}
			else
			{
				view = new LLFolderViewItem(obj->getName(),
											bridge->getIcon(),
											bridge->getIconOpen(),
											NULL,
											bridge->getCreationDate(),
											mFolders,
											bridge);
			}
			view->addToFolder(folder, mFolders);
		}
	}

	// now, for each category, do the second pass
	for(U32 i = 0; i < child_categories.size(); i++)
	{
		createViewsForCategory(inventory, child_categories[i]->first,
							   child_categories[i]->second );
		delete child_categories[i];
	}
}

void LLPanelObjectInventory::refresh()
{
	//LL_INFOS() << "LLPanelObjectInventory::refresh()" << LL_ENDL;
	BOOL has_inventory = FALSE;
	const BOOL non_root_ok = TRUE;
	LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstRootNode(NULL, non_root_ok);
	if(node)
	{
		LLViewerObject* object = node->getObject();
		if(object && ((LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() == 1)
					  || (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1)))
		{
			// determine if we need to make a request. Start with a
			// default based on if we have inventory at all.
			BOOL make_request = !mHaveInventory;

			// If the task id is different than what we've stored,
			// then make the request.
			if(mTaskUUID != object->mID)
			{
				mTaskUUID = object->mID;
				make_request = TRUE;

				// This is a new object so pre-emptively clear the contents
				// Otherwise we show the old stuff until the update comes in
				clearContents();

				// Register for updates from this object,
				registerVOInventoryListener(object,NULL);
			}

			// Based on the node information, we may need to dirty the
			// object inventory and get it again.
			if(node->mValid)
			{
				if(node->mInventorySerial != object->getInventorySerial() || object->isInventoryDirty())
				{
					make_request = TRUE;
				}
			}

			// do the request if necessary.
			if(make_request)
			{
				requestVOInventory();
			}
			has_inventory = TRUE;
		}
	}
	if(!has_inventory)
	{
		clearInventoryTask();
	}
	//LL_INFOS() << "LLPanelObjectInventory::refresh() " << mTaskUUID << LL_ENDL;
}

void LLPanelObjectInventory::clearInventoryTask()
{
	mTaskUUID = LLUUID::null;
	removeVOInventoryListener();
	clearContents();
}

void LLPanelObjectInventory::removeSelectedItem()
{
	if(mFolders)
	{
		mFolders->removeSelectedItems();
	}
}

void LLPanelObjectInventory::startRenamingSelectedItem()
{
	if(mFolders)
	{
		mFolders->startRenamingSelectedItem();
	}
}

void LLPanelObjectInventory::draw()
{
	LLPanel::draw();

	if(mIsInventoryEmpty)
	{
		if((LLUUID::null != mTaskUUID) && (!mHaveInventory))
		{
			LLFontGL::getFontSansSerif()->renderUTF8(LLTrans::getString("LoadingContents"), 0,
													 (S32)(getRect().getWidth() * 0.5f),
													 10,
													 LLColor4( 1, 1, 1, 1 ),
													 LLFontGL::HCENTER,
													 LLFontGL::BOTTOM);
		}
		else if(mHaveInventory)
		{
			LLFontGL::getFontSansSerif()->renderUTF8(LLTrans::getString("NoContents"), 0,
													 (S32)(getRect().getWidth() * 0.5f),
													 10,
													 LLColor4( 1, 1, 1, 1 ),
													 LLFontGL::HCENTER,
													 LLFontGL::BOTTOM);
		}
	}
}

void LLPanelObjectInventory::deleteAllChildren()
{
	mScroller = NULL;
	mFolders = NULL;
	LLView::deleteAllChildren();
}

BOOL LLPanelObjectInventory::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, EAcceptance *accept, std::string& tooltip_msg)
{
	if (mFolders && mHaveInventory)
	{
		LLFolderViewItem* folderp = mFolders->getNextFromChild(NULL);
		if (!folderp)
		{
			return FALSE;
		}
		// Try to pass on unmodified mouse coordinates
		S32 local_x = x - mFolders->getRect().mLeft;
		S32 local_y = y - mFolders->getRect().mBottom;

		if (mFolders->pointInView(local_x, local_y))
		{
			return mFolders->handleDragAndDrop(local_x, local_y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}
		else
		{
			//force mouse coordinates to be inside folder rectangle
			return mFolders->handleDragAndDrop(5, 1, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}
	}
	else
	{
		return FALSE;
	}
}

//static
void LLPanelObjectInventory::idle(void* user_data)
{
	LLPanelObjectInventory* self = (LLPanelObjectInventory*)user_data;

	if (self->mInventoryNeedsUpdate)
	{
		self->updateInventory();
	}
}

void LLPanelObjectInventory::onFocusLost()
{
	// inventory no longer handles cut/copy/paste/delete
	if (LLEditMenuHandler::gEditMenuHandler == mFolders)
	{
		LLEditMenuHandler::gEditMenuHandler = NULL;
	}
	
	LLPanel::onFocusLost();
}

void LLPanelObjectInventory::onFocusReceived()
{
	// inventory now handles cut/copy/paste/delete
	LLEditMenuHandler::gEditMenuHandler = mFolders;
	
	LLPanel::onFocusReceived();
}
