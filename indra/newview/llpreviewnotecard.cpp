/** 
 * @file llpreviewnotecard.cpp
 * @brief Implementation of the notecard editor
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewnotecard.h"

#include "llinventory.h"
#include "llinventorydefines.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llassetuploadresponders.h"
#include "llviewerwindow.h"
#include "llbutton.h"
#include "llfloatersearchreplace.h"
#include "llinventorymodel.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "roles_constants.h"
#include "llscrollbar.h"
#include "llselectmgr.h"
#include "llviewertexteditor.h"
#include "llvfile.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "lldir.h"
//#include "llfloaterchat.h"
#include "llviewerstats.h"
#include "llviewercontrol.h"		// gSavedSettings
#include "llappviewer.h"		// app_abort_quit()
#include "lllineeditor.h"
#include "lluictrlfactory.h"
// <edit>
#include "statemachine/aifilepicker.h"
// </edit>

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

const S32 PREVIEW_MIN_WIDTH =
	2 * PREVIEW_BORDER +
	2 * PREVIEW_BUTTON_WIDTH + 
	PREVIEW_PAD + RESIZE_HANDLE_WIDTH +
	PREVIEW_PAD;
const S32 PREVIEW_MIN_HEIGHT = 
	2 * PREVIEW_BORDER +
	3*(20 + PREVIEW_PAD) +
	2 * SCROLLBAR_SIZE + 128;

///----------------------------------------------------------------------------
/// Class LLPreviewNotecard
///----------------------------------------------------------------------------

// Default constructor
LLPreviewNotecard::LLPreviewNotecard(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id, 
									 const LLUUID& object_id,
									 const LLUUID& asset_id,
									 BOOL show_keep_discard,
									 LLPointer<LLViewerInventoryItem> inv_item) :
	LLPreview(name, rect, title, item_id, object_id, TRUE,
			  PREVIEW_MIN_WIDTH,
			  PREVIEW_MIN_HEIGHT,
			  inv_item),
	mAssetID( asset_id ),
	mNotecardItemID(item_id),
	mObjectID(object_id)
{
	LLRect curRect = rect;

	if (show_keep_discard)
	{
		LLUICtrlFactory::getInstance()->buildFloater(this,"floater_preview_notecard_keep_discard.xml");
		childSetAction("Keep",onKeepBtn,this);
		childSetAction("Discard",onDiscardBtn,this);
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildFloater(this,"floater_preview_notecard.xml");
		// <edit>
		childSetAction("Get Items", onClickGetItems, this);
		// </edit>

		if( mAssetID.isNull() )
		{
			const LLInventoryItem* item = getItem();
			if( item )
			{
				mAssetID = item->getAssetUUID();
			}
		}
	}	
	if (hasChild("Save", true))
		childSetAction("Save",onClickSave,this);

	// only assert shape if not hosted in a multifloater
	if (!getHost())
	{
		reshape(curRect.getWidth(), curRect.getHeight(), TRUE);
		setRect(curRect);
	}
			
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("lock"))
		ctrl->setVisible(false);
	
	if (hasChild("desc", true))
	{
		childSetCommitCallback("desc", LLPreview::onText, this);
		if (const LLInventoryItem* item = getItem())
			childSetText("desc", item->getDescription());
		getChild<LLLineEditor>("desc")->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);
	}

	setTitle(title);
	
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	if (editor)
	{
		editor->setWordWrap(TRUE);
		editor->setSourceID(item_id);
		editor->setHandleEditKeysDirectly(TRUE);
	}

	initMenu();
}

LLPreviewNotecard::~LLPreviewNotecard()
{
}

BOOL LLPreviewNotecard::postBuild()
{
	LLViewerTextEditor* ed = findChild<LLViewerTextEditor>("Notecard Editor");
	if (ed)
	{
		ed->setNotecardInfo(mNotecardItemID, mObjectID);
		ed->makePristine();
	}
	return TRUE;
}

bool LLPreviewNotecard::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	LLInventoryItem* item = NULL;
	if (itemptr && itemptr->notNull())
	{
		item = (LLInventoryItem*)(*itemptr);
	}
	bool res = saveIfNeeded(item);
	if (res)
	{
		delete itemptr;
	}
	return res;
}

void LLPreviewNotecard::setEnabled( BOOL enabled )
{
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	if (editor)
		editor->setEnabled(enabled);
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("lock"))
		ctrl->setVisible(!enabled);
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("desc"))
		ctrl->setEnabled(enabled);
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("Save"))
		ctrl->setEnabled(enabled && editor && !editor->isPristine());

}


void LLPreviewNotecard::draw()
{
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");
	BOOL script_changed = editor && !editor->isPristine();
	
	if (LLUICtrl* ctrl = findChild<LLUICtrl>("Save"))
		ctrl->setEnabled(script_changed && getEnabled());
	
	LLPreview::draw();
}

// virtual
BOOL LLPreviewNotecard::handleKeyHere(KEY key, MASK mask)
{
	if(('S' == key) && (MASK_CONTROL == (mask & MASK_CONTROL)))
	{
		saveIfNeeded();
		return TRUE;
	}

	if ('F' == key && (mask & MASK_CONTROL) && !(mask & (MASK_SHIFT | MASK_ALT)))
	{
		LLFloaterSearchReplace::show(findChild<LLViewerTextEditor>("Notecard Editor"));
		return TRUE;
	}

	return LLPreview::handleKeyHere(key, mask);
}

// virtual
BOOL LLPreviewNotecard::canClose()
{
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	if (mForceClose || (editor && editor->isPristine()))
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLPreviewNotecard::handleSaveChangesDialog,this, _1, _2));
								  
		return FALSE;
	}
}

const LLInventoryItem* LLPreviewNotecard::getDragItem()
{
	if (LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor"))
	{
		return editor->getDragItem();
	}
	return NULL;
}

bool LLPreviewNotecard::hasEmbeddedInventory()
{
	if (LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->hasEmbeddedInventory();
	return false;
}

void LLPreviewNotecard::refreshFromInventory()
{
	LL_DEBUGS() << "LLPreviewNotecard::refreshFromInventory()" << LL_ENDL;
	loadAsset();
}

void LLPreviewNotecard::loadAsset()
{
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	if (!editor)
		return;

	// request the asset.
	if (const LLInventoryItem* item = getItem())
	{
		bool modify = gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE);
		if (modify) editor->setParseHTML(false); // Don't do the url parsing or we'll lose text!
		if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),
									GP_OBJECT_MANIPULATE)
			|| gAgent.isGodlike())
		{
			mAssetID = item->getAssetUUID();
			if(mAssetID.isNull())
			{
				editor->setText(LLStringUtil::null);
				editor->makePristine();
				editor->setEnabled(TRUE);
				mAssetStatus = PREVIEW_ASSET_LOADED;
			}
			else
			{
				LLUUID* new_uuid = new LLUUID(mItemUUID);
				LLHost source_sim = LLHost::invalid;
				if (mObjectUUID.notNull())
				{
					LLViewerObject *objectp = gObjectList.findObject(mObjectUUID);
					if (objectp && objectp->getRegion())
					{
						source_sim = objectp->getRegion()->getHost();
					}
					else
					{
						// The object that we're trying to look at disappeared, bail.
						LL_WARNS() << "Can't find object " << mObjectUUID << " associated with notecard." << LL_ENDL;
						mAssetID.setNull();
						editor->setText(getString("no_object"));
						editor->makePristine();
						editor->setEnabled(FALSE);
						mAssetStatus = PREVIEW_ASSET_LOADED;
						delete new_uuid;
						return;
					}
				}
				gAssetStorage->getInvItemAsset(source_sim,
												gAgent.getID(),
												gAgent.getSessionID(),
												item->getPermissions().getOwner(),
												mObjectUUID,
												item->getUUID(),
												item->getAssetUUID(),
												item->getType(),
												&onLoadComplete,
												(void*)new_uuid,
												TRUE);
				mAssetStatus = PREVIEW_ASSET_LOADING;
			}
		}
		else
		{
			mAssetID.setNull();
			editor->setText(getString("not_allowed"));
			editor->makePristine();
			editor->setEnabled(FALSE);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		if(!modify)
		{
			editor->setEnabled(FALSE);
			// <edit> You can always save in task inventory
			if(!mObjectUUID.isNull()) editor->setEnabled(TRUE);
			// </edit>
			if (LLUICtrl* ctrl = findChild<LLUICtrl>("lock"))
				ctrl->setVisible(true);
		}
	}
	else
	{
		editor->setText(LLStringUtil::null);
		editor->makePristine();
		editor->setEnabled(TRUE);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
}

// static
void LLPreviewNotecard::onLoadComplete(LLVFS *vfs,
									   const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data, S32 status, LLExtStat ext_status)
{
	LL_INFOS() << "LLPreviewNotecard::onLoadComplete()" << LL_ENDL;
	LLUUID* item_id = (LLUUID*)user_data;
	LLPreviewNotecard* preview = LLPreviewNotecard::getInstance(*item_id);
	if( preview )
	{
		if(0 == status)
		{
			LLVFile file(vfs, asset_uuid, type, LLVFile::READ);

			S32 file_length = file.getSize();

			char* buffer = new char[file_length+1];
			file.read((U8*)buffer, file_length);		/*Flawfinder: ignore*/

			// put a EOS at the end
			buffer[file_length] = 0;

			// Singu Note: Set Enabled first, it determines whether or not our text will be linked
			const LLInventoryItem* item = preview->getItem();
			BOOL modifiable = item && gAgent.allowOperation(PERM_MODIFY,
								item->getPermissions(), GP_OBJECT_MANIPULATE);
			preview->setEnabled(modifiable);
			if (LLViewerTextEditor* previewEditor = preview->findChild<LLViewerTextEditor>("Notecard Editor"))
			{
				if ((file_length > 19) && !strncmp(buffer, "Linden text version", 19))
				{
					if (!previewEditor->importBuffer(buffer, file_length+1))
					{
						LL_WARNS() << "Problem importing notecard" << LL_ENDL;
					}
				}
				else
				{
					// Version 0 (just text, doesn't include version number)
					previewEditor->setText(LLStringExplicit(buffer), false);
				}

				previewEditor->makePristine();
			}
			delete[] buffer;
			preview->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			LLViewerStats::getInstance()->incStat( LLViewerStats::ST_DOWNLOAD_FAILED );

			if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				LLNotificationsUtil::add("NotecardMissing");
			}
			else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
			{
				LLNotificationsUtil::add("NotecardNoPermissions");
			}
			else
			{
				LLNotificationsUtil::add("UnableToLoadNotecard");
			}

			LL_WARNS() << "Problem loading notecard: " << status << LL_ENDL;
			preview->mAssetStatus = PREVIEW_ASSET_ERROR;
		}
	}
	delete item_id;
}

// static
LLPreviewNotecard* LLPreviewNotecard::getInstance(const LLUUID& item_id)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_id);
	if(found_it != LLPreview::sInstances.end())
	{
		instance = found_it->second;
	}
	return (LLPreviewNotecard*)instance;
}

// static
void LLPreviewNotecard::onClickSave(void* user_data)
{
	//LL_INFOS() << "LLPreviewNotecard::onBtnSave()" << LL_ENDL;
	LLPreviewNotecard* preview = (LLPreviewNotecard*)user_data;
	if(preview)
	{
		preview->saveIfNeeded();
	}
}
// <edit>
// static
void LLPreviewNotecard::onClickGetItems(void* user_data)
{
	LLPreviewNotecard* preview = static_cast<LLPreviewNotecard*>(user_data);
	if (LLViewerTextEditor* editor = preview->findChild<LLViewerTextEditor>("Notecard Editor"))
	{
		std::vector<LLPointer<LLInventoryItem> > items = editor->getEmbeddedItems();
		if (items.size())
		{
			std::vector<LLPointer<LLInventoryItem> >::iterator iter = items.begin();
			std::vector<LLPointer<LLInventoryItem> >::iterator end = items.end();
			for ( ; iter != end; ++iter)
			{
				LLInventoryItem* item = static_cast<LLInventoryItem*>(*iter);
				#if 0 //use_caps
				{
					copy_inventory_from_notecard(LLUUID::null, preview->getObjectID(), preview->getNotecardItemID(), item, 0);
				}
				#else
				{
					// Only one item per message actually works
					gMessageSystem->newMessageFast(_PREHASH_CopyInventoryFromNotecard);
					gMessageSystem->nextBlockFast(_PREHASH_AgentData);
					gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					gMessageSystem->nextBlockFast(_PREHASH_NotecardData);
					gMessageSystem->addUUIDFast(_PREHASH_NotecardItemID, preview->getNotecardItemID());
					gMessageSystem->addUUIDFast(_PREHASH_ObjectID, preview->getObjectID());
					gMessageSystem->nextBlockFast(_PREHASH_InventoryData);
					gMessageSystem->addUUIDFast(_PREHASH_ItemID, item->getUUID());
					gMessageSystem->addUUIDFast(_PREHASH_FolderID, gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(item->getType())));
					gAgent.sendReliableMessage();
				}
				#endif
			}
		}
	}
}
// </edit>
struct LLSaveNotecardInfo
{
	LLPreviewNotecard* mSelf;
	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	LLTransactionID mTransactionID;
	LLPointer<LLInventoryItem> mCopyItem;
	LLSaveNotecardInfo(LLPreviewNotecard* self, const LLUUID& item_id, const LLUUID& object_id,
					   const LLTransactionID& transaction_id, LLInventoryItem* copyitem) :
		mSelf(self), mItemUUID(item_id), mObjectUUID(object_id), mTransactionID(transaction_id), mCopyItem(copyitem)
	{
	}
};

bool LLPreviewNotecard::saveIfNeeded(LLInventoryItem* copyitem)
{
	if(!gAssetStorage)
	{
		LL_WARNS() << "Not connected to an asset storage system." << LL_ENDL;
		return false;
	}

	
	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	if (editor && !editor->isPristine())
	{
		// We need to update the asset information
		LLTransactionID tid;
		LLAssetID asset_id;
		tid.generate();
		asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

		LLVFile file(gVFS, asset_id, LLAssetType::AT_NOTECARD, LLVFile::APPEND);

		std::string buffer;
		if (!editor->exportBuffer(buffer))
		{
			return false;
		}

		editor->makePristine();

		S32 size = buffer.length() + 1;
		file.setMaxSize(size);
		file.write((U8*)buffer.c_str(), size);

		const LLInventoryItem* item = getItem();
		// save it out to database
		if (item)
		{			
			std::string agent_url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
			std::string task_url = gAgent.getRegion()->getCapability("UpdateNotecardTaskInventory");
			if (mObjectUUID.isNull() && !agent_url.empty())
			{
				// Saving into agent inventory
				mAssetStatus = PREVIEW_ASSET_LOADING;
				setEnabled(FALSE);
				LLSD body;
				body["item_id"] = mItemUUID;
				LL_INFOS() << "Saving notecard " << mItemUUID
					<< " into agent inventory via " << agent_url << LL_ENDL;
				LLHTTPClient::post(agent_url, body,
					new LLUpdateAgentInventoryResponder(body, asset_id, LLAssetType::AT_NOTECARD));
			}
			else if (!mObjectUUID.isNull() && !task_url.empty())
			{
				// Saving into task inventory
				mAssetStatus = PREVIEW_ASSET_LOADING;
				setEnabled(FALSE);
				LLSD body;
				body["task_id"] = mObjectUUID;
				body["item_id"] = mItemUUID;
				LL_INFOS() << "Saving notecard " << mItemUUID << " into task "
					<< mObjectUUID << " via " << task_url << LL_ENDL;
				LLHTTPClient::post(task_url, body,
					new LLUpdateTaskInventoryResponder(body, asset_id, LLAssetType::AT_NOTECARD));
			}
			else if (gAssetStorage)
			{
				LLSaveNotecardInfo* info = new LLSaveNotecardInfo(this, mItemUUID, mObjectUUID,
																tid, copyitem);
				gAssetStorage->storeAssetData(tid, LLAssetType::AT_NOTECARD,
												&onSaveComplete,
												(void*)info,
												FALSE);
			}
		}
	}
	return true;
}

// static
void LLPreviewNotecard::onSaveComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLSaveNotecardInfo* info = static_cast<LLSaveNotecardInfo*>(user_data);
	if (0 == status)
	{
		if(info->mObjectUUID.isNull())
		{
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if(item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setAssetUUID(asset_uuid);
				new_item->setTransactionID(info->mTransactionID);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				LL_WARNS() << "Inventory item for notecard " << info->mItemUUID
						<< " is no longer in agent inventory." << LL_ENDL;
			}
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(info->mObjectUUID);
			LLViewerInventoryItem* item = NULL;
			if(object)
			{
				item = (LLViewerInventoryItem*)object->getInventoryObject(info->mItemUUID);
			}
			if(object && item)
			{
				item->setAssetUUID(asset_uuid);
				item->setTransactionID(info->mTransactionID);
				object->updateInventory(item, TASK_INVENTORY_ITEM_KEY, false);
				dialog_refresh_all();
			}
			else
			{
				LLNotificationsUtil::add("SaveNotecardFailObjectNotFound");
			}
		}
		// Perform item copy to inventory
		if (info->mCopyItem.notNull())
		{
			if (LLViewerTextEditor* editor = info->mSelf->findChild<LLViewerTextEditor>("Notecard Editor"))
			{
				editor->copyInventory(info->mCopyItem);
			}
		}
		
		// Find our window and close it if requested.
		LLPreviewNotecard* previewp = (LLPreviewNotecard*)LLPreview::find(info->mItemUUID);
		if (previewp && previewp->mCloseAfterSave)
		{
			previewp->close();
		}
	}
	else
	{
		LL_WARNS() << "Problem saving notecard: " << status << LL_ENDL;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("SaveNotecardFailReason", args);
	}

	std::string uuid_string;
	asset_uuid.toString(uuid_string);
	std::string filename;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,uuid_string) + ".tmp";
	LLFile::remove(filename);
	delete info;
}

bool LLPreviewNotecard::handleSaveChangesDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch(option)
	{
	case 0:  // "Yes"
		mCloseAfterSave = TRUE;
		LLPreviewNotecard::onClickSave((void*)this);
		break;

	case 1:  // "No"
		mForceClose = TRUE;
		close();
		break;

	case 2: // "Cancel"
	default:
		// If we were quitting, we didn't really mean it.
		LLAppViewer::instance()->abortQuit();
		break;
	}
	return false;
}

void LLPreviewNotecard::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPreview::reshape( width, height, called_from_parent );

	if( !isMinimized() )
	{
		// So that next time you open a script it will have the same height and width 
		// (although not the same position).
		gSavedSettings.setRect("NotecardEditorRect", getRect());
	}
}

// <edit>
// virtual
BOOL LLPreviewNotecard::canSaveAs() const
{
	return TRUE;
}

// virtual
void LLPreviewNotecard::saveAs()
{
	std::string default_filename("untitled.notecard");
	const LLInventoryItem *item = getItem();
	if(item)
	{
	//	gAssetStorage->getAssetData(item->getAssetUUID(), LLAssetType::AT_NOTECARD, LLPreviewNotecard::gotAssetForSave, this, TRUE);
		default_filename = LLDir::getScrubbedFileName(item->getName()) + ".notecard";
	}

	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(default_filename, FFSAVE_NOTECARD);
	filepicker->run(boost::bind(&LLPreviewNotecard::saveAs_continued, this, filepicker));
}

void LLPreviewNotecard::saveAs_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	LLViewerTextEditor* editor = findChild<LLViewerTextEditor>("Notecard Editor");

	std::string buffer;
	if (editor && !editor->exportBuffer(buffer))
	{
		// FIXME: Notify the user!
		return;
	}

	S32 size = buffer.length();

	std::string filename = filepicker->getFilename();
	std::ofstream export_file(filename.c_str(), std::ofstream::binary);
	export_file.write(buffer.c_str(), size);
	export_file.close();
}

LLUUID LLPreviewNotecard::getNotecardItemID()
{
	return mNotecardItemID;
}

LLUUID LLPreviewNotecard::getObjectID()
{
	return mObjectID;
}

// virtual
LLUUID LLPreviewNotecard::getItemID()
{
	const LLViewerInventoryItem* item = getItem();
	if(item)
	{
		return item->getUUID();
	}
	return LLUUID::null;
}
// </edit>

LLTextEditor* LLPreviewNotecard::getEditor()
{
	return findChild<LLViewerTextEditor>("Notecard Editor");
}

void LLPreviewNotecard::initMenu()
{
	LLMenuItemCallGL* menuItem = getChild<LLMenuItemCallGL>("Undo");
	menuItem->setMenuCallback(onUndoMenu, this);
	menuItem->setEnabledCallback(enableUndoMenu);

	menuItem = getChild<LLMenuItemCallGL>("Redo");
	menuItem->setMenuCallback(onRedoMenu, this);
	menuItem->setEnabledCallback(enableRedoMenu);

	menuItem = getChild<LLMenuItemCallGL>("Cut");
	menuItem->setMenuCallback(onCutMenu, this);
	menuItem->setEnabledCallback(enableCutMenu);

	menuItem = getChild<LLMenuItemCallGL>("Copy");
	menuItem->setMenuCallback(onCopyMenu, this);
	menuItem->setEnabledCallback(enableCopyMenu);

	menuItem = getChild<LLMenuItemCallGL>("Paste");
	menuItem->setMenuCallback(onPasteMenu, this);
	menuItem->setEnabledCallback(enablePasteMenu);

	menuItem = getChild<LLMenuItemCallGL>("Select All");
	menuItem->setMenuCallback(onSelectAllMenu, this);
	menuItem->setEnabledCallback(enableSelectAllMenu);

	menuItem = getChild<LLMenuItemCallGL>("Deselect");
	menuItem->setMenuCallback(onDeselectMenu, this);
	menuItem->setEnabledCallback(enableDeselectMenu);

	menuItem = getChild<LLMenuItemCallGL>("Search / Replace...");
	menuItem->setMenuCallback(onSearchMenu, this);
	menuItem->setEnabledCallback(NULL);
}

// static 
void LLPreviewNotecard::onSearchMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		LLFloaterSearchReplace::show(editor);
}

// static 
void LLPreviewNotecard::onUndoMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->undo();
}

// static 
void LLPreviewNotecard::onRedoMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->redo();
}

// static 
void LLPreviewNotecard::onCutMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->cut();
}

// static 
void LLPreviewNotecard::onCopyMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->copy();
}

// static 
void LLPreviewNotecard::onPasteMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->paste();
}

// static 
void LLPreviewNotecard::onSelectAllMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->selectAll();
}

// static 
void LLPreviewNotecard::onDeselectMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		editor->deselect();
}

// static 
BOOL LLPreviewNotecard::enableUndoMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canUndo();
	return false;
}

// static 
BOOL LLPreviewNotecard::enableRedoMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canRedo();
	return false;
}

// static 
BOOL LLPreviewNotecard::enableCutMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canCut();
	return false;
}

// static 
BOOL LLPreviewNotecard::enableCopyMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canCopy();
	return false;
}

// static 
BOOL LLPreviewNotecard::enablePasteMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canPaste();
	return false;
}

// static 
BOOL LLPreviewNotecard::enableSelectAllMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canSelectAll();
	return false;
}

// static 
BOOL LLPreviewNotecard::enableDeselectMenu(void* userdata)
{
	LLPreviewNotecard* self = static_cast<LLPreviewNotecard*>(userdata);
	if (LLViewerTextEditor* editor = self->findChild<LLViewerTextEditor>("Notecard Editor"))
		return editor->canDeselect();
	return false;
}

// EOF
