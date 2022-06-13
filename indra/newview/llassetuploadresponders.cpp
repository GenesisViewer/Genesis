/**
 * @file llassetuploadresponders.cpp
 * @brief Processes responses received for asset upload requests.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llassetuploadresponders.h"

// viewer includes
#include "llagent.h"
#include "llagentbenefits.h"
#include "llcompilequeue.h"
#include "llfloaterbuycurrency.h"
#include "statemachine/aifilepicker.h"
#include "llinventorydefines.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
#include "llpanelmaininventory.h"
#include "llpermissionsflags.h"
#include "llpreviewnotecard.h"
#include "llpreviewscript.h"
#include "llpreviewgesture.h"
#include "llgesturemgr.h"
#include "llstatusbar.h"		// sendMoneyBalanceRequest()
#include "llsdserialize.h"
#include "lluploaddialog.h"
#include "llviewerobject.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewertexlayer.h"
#include "llviewerwindow.h"
#include "lltrans.h"

// library includes
#include "lldir.h"
#include "llfocusmgr.h"
#include "llnotificationsutil.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llvfs.h"

// When uploading multiple files, don't display any of them when uploading more than this number.
static const S32 FILE_COUNT_DISPLAY_THRESHOLD = 5;

void dialog_refresh_all();

void on_new_single_inventory_upload_complete(
	LLAssetType::EType asset_type,
	LLInventoryType::EType inventory_type,
	const std::string inventory_type_string,
	const LLUUID& item_folder_id,
	const std::string& item_name,
	const std::string& item_description,
	const LLSD& server_response,
	S32 upload_price)
{
	if (upload_price > 0)
	{
		// this upload costed us L$, update our balance
		// and display something saying that it cost L$
		LLStatusBar::sendMoneyBalanceRequest();

		LLSD args;
		args["AMOUNT"] = llformat("%d", upload_price);
		LLNotificationsUtil::add("UploadPayment", args);
	}

	if (item_folder_id.notNull())
	{
		U32 everyone_perms = PERM_NONE;
		U32 group_perms = PERM_NONE;
		U32 next_owner_perms = PERM_ALL;
		if (server_response.has("new_next_owner_mask"))
		{
			// The server provided creation perms so use them.
			// Do not assume we got the perms we asked for
			// since the server may not have granted them all.
			everyone_perms = server_response["new_everyone_mask"].asInteger();
			group_perms = server_response["new_group_mask"].asInteger();
			next_owner_perms = server_response["new_next_owner_mask"].asInteger();
		}
		else 
		{
			// The server doesn't provide creation perms
			// so use old assumption-based perms.
			if (inventory_type_string != "snapshot")
			{
				next_owner_perms = PERM_MOVE | PERM_TRANSFER;
			}
		}

		LLPermissions new_perms;
		new_perms.init(
			gAgent.getID(),
			gAgent.getID(),
			LLUUID::null,
			LLUUID::null);

		new_perms.initMasks(
			PERM_ALL,
			PERM_ALL,
			everyone_perms,
			group_perms,
			next_owner_perms);

		U32 inventory_item_flags = 0;
		if (server_response.has("inventory_flags"))
		{
			inventory_item_flags = (U32) server_response["inventory_flags"].asInteger();
			if (inventory_item_flags != 0)
			{
				LL_INFOS() << "inventory_item_flags " << inventory_item_flags << LL_ENDL;
			}
		}
		S32 creation_date_now = time_corrected();
		LLPointer<LLViewerInventoryItem> item = new LLViewerInventoryItem(
			server_response["new_inventory_item"].asUUID(),
			item_folder_id,
			new_perms,
			server_response["new_asset"].asUUID(),
			asset_type,
			inventory_type,
			item_name,
			item_description,
			LLSaleInfo::DEFAULT,
			inventory_item_flags,
			creation_date_now);

		gInventory.updateItem(item);
		gInventory.notifyObservers();

		// Show the preview panel for textures and sounds to let
		// user know that the image (or snapshot) arrived intact.
		LLInventoryPanel* panel = LLInventoryPanel::getActiveInventoryPanel();
		if ( panel )
		{
			LLFocusableElement* focus = gFocusMgr.getKeyboardFocus();

			panel->setSelection(
				server_response["new_inventory_item"].asUUID(),
				TAKE_FOCUS_NO);
			if ((LLAssetType::AT_TEXTURE == asset_type || LLAssetType::AT_SOUND == asset_type)
				/* FIXME: && LLFilePicker::instance().getFileCount() <= FILE_COUNT_DISPLAY_THRESHOLD */)
			{
				panel->openSelected();
			}

			// restore keyboard focus
			gFocusMgr.setKeyboardFocus(focus);
		}
	}
	else
	{
		LL_WARNS() << "Can't find a folder to put it in" << LL_ENDL;
	}

	// remove the "Uploading..." message
	LLUploadDialog::modalUploadFinished();
}

LLAssetUploadResponder::LLAssetUploadResponder(const LLSD &post_data,
											   const LLUUID& vfile_id,
											   LLAssetType::EType asset_type)
:
	  mPostData(post_data),
	  mVFileID(vfile_id),
	  mAssetType(asset_type)
{
	if (!gVFS->getExists(vfile_id, asset_type))
	{
		LL_WARNS() << "LLAssetUploadResponder called with nonexistant vfile_id" << LL_ENDL;
		mVFileID.setNull();
		mAssetType = LLAssetType::AT_NONE;
		return;
	}
}

LLAssetUploadResponder::LLAssetUploadResponder(
	const LLSD &post_data,
											   const std::string& file_name, 
											   LLAssetType::EType asset_type)
:
	  mPostData(post_data),
	  mFileName(file_name),
	  mAssetType(asset_type)
{
}

LLAssetUploadResponder::~LLAssetUploadResponder()
{
	if (!mFileName.empty())
	{
		// Delete temp file
		LLFile::remove(mFileName);
	}
}

void on_failure(const LLAssetType::EType& mAssetType, const LLSD& mPostData, const LLSD& args)
{
	switch (mAssetType)
	{
	case LLAssetType::AT_NOTECARD:
	{
		if (LLPreviewNotecard* nc = (LLPreviewNotecard*)LLPreview::find(mPostData["item_id"]))
			nc->setEnabled(true);
		break;
	}
	case LLAssetType::AT_SCRIPT:
	case LLAssetType::AT_LSL_TEXT:
	case LLAssetType::AT_LSL_BYTECODE:
	{
		if (LLPreviewLSL* lsl = (LLPreviewLSL*)LLPreview::find(mPostData["item_id"]))
			lsl->callbackLSLCompileFailed(LLSD().with(0, "Upload Failure:").with(1, args["REASON"]));
		break;
	}
	default: break;
	}
}
// virtual
void LLAssetUploadResponder::httpFailure()
{
	LL_INFOS() << "LLAssetUploadResponder::error " << mStatus 
			<< " reason: " << mReason << LL_ENDL;
	LLSD args;
	switch(mStatus)
	{
		case 400:
			args["FILE"] = (mFileName.empty() ? mVFileID.asString() : mFileName);
			args["REASON"] = "Error in upload request.  Please visit "
				"http://secondlife.com/support for help fixing this problem.";
			LLNotificationsUtil::add("CannotUploadReason", args);
			break;
		case 500:
		default:
			args["FILE"] = (mFileName.empty() ? mVFileID.asString() : mFileName);
			args["REASON"] = "The server is experiencing unexpected "
				"difficulties.";
			LLNotificationsUtil::add("CannotUploadReason", args);
			break;
	}
	on_failure(mAssetType, mPostData, args);
	LLUploadDialog::modalUploadFinished();
}

//virtual 
void LLAssetUploadResponder::httpSuccess()
{
	const LLSD& content = getContent();
	if (!content.isMap())
	{
		failureResult(HTTP_INTERNAL_ERROR_OTHER, "Malformed response contents", content);
		return;
	}
	LL_DEBUGS() << "LLAssetUploadResponder::result from capabilities" << LL_ENDL;

	const std::string& state = content["state"].asString();

	if (state == "upload")
	{
		uploadUpload(content);
	}
	else if (state == "complete")
	{
		// rename file in VFS with new asset id
		if (mFileName.empty())
		{
			// rename the file in the VFS to the actual asset id
			// LL_INFOS() << "Changing uploaded asset UUID to " << content["new_asset"].asUUID() << LL_ENDL;
			gVFS->renameFile(mVFileID, mAssetType, content["new_asset"].asUUID(), mAssetType);
		}
		uploadComplete(content);
	}
	else
	{
		uploadFailure(content);
	}
}

void LLAssetUploadResponder::uploadUpload(const LLSD& content)
{
	const std::string& uploader = content["uploader"].asString();
	if (mFileName.empty())
	{
		LLHTTPClient::postFile(uploader, mVFileID, mAssetType, this);
	}
	else
	{
		LLHTTPClient::postFile(uploader, mFileName, this);
	}
}

void LLAssetUploadResponder::uploadFailure(const LLSD& content)
{
	// remove the "Uploading..." message
	LLUploadDialog::modalUploadFinished();
	
	const std::string& reason = content["state"].asString();
	// deal with L$ errors
	if (reason == "insufficient funds")
	{
		S32 price;
		if (content.has("upload_price"))
			price = content["upload_price"];
		else
			LLAgentBenefitsMgr::current().findUploadCost(mAssetType, price);
		LLFloaterBuyCurrency::buyCurrency("Uploading costs", price);
	}
	else
	{
		LLSD args;
		args["FILE"] = (mFileName.empty() ? mVFileID.asString() : mFileName);
		args["REASON"] = content["message"].asString();
		LLNotificationsUtil::add("CannotUploadReason", args);
		on_failure(mAssetType, mPostData, args);
	}
}

void LLAssetUploadResponder::uploadComplete(const LLSD& content)
{
}

LLNewAgentInventoryResponder::LLNewAgentInventoryResponder(
	const LLSD& post_data,
	const LLUUID& vfile_id,
	LLAssetType::EType asset_type)
	: LLAssetUploadResponder(post_data, vfile_id, asset_type)
{
}

LLNewAgentInventoryResponder::LLNewAgentInventoryResponder(
	const LLSD& post_data,
	const std::string& file_name,
	LLAssetType::EType asset_type)
	: LLAssetUploadResponder(post_data, file_name, asset_type)
{
}

// virtual
void LLNewAgentInventoryResponder::httpFailure()
{
	LLAssetUploadResponder::httpFailure();
	//LLImportColladaAssetCache::getInstance()->assetUploaded(mVFileID, LLUUID(), FALSE);
}


//virtual 
void LLNewAgentInventoryResponder::uploadFailure(const LLSD& content)
{
	LLAssetUploadResponder::uploadFailure(content);

	//LLImportColladaAssetCache::getInstance()->assetUploaded(mVFileID, content["new_asset"], FALSE);
}

//virtual 
void LLNewAgentInventoryResponder::uploadComplete(const LLSD& content)
{
	LL_DEBUGS() << "LLNewAgentInventoryResponder::result from capabilities" << LL_ENDL;

	//std::ostringstream llsdxml;
	//LLSDSerialize::toXML(content, llsdxml);
	//LL_INFOS() << "upload complete content:\n " << llsdxml.str() << LL_ENDL;

	LLAssetType::EType asset_type = LLAssetType::lookup(mPostData["asset_type"].asString());
	LLInventoryType::EType inventory_type = LLInventoryType::lookup(mPostData["inventory_type"].asString());
	S32 expected_upload_cost = 0;

	// Update L$ and ownership credit information
	// since it probably changed on the server
	if (content.has("upload_price"))
		expected_upload_cost = content["upload_price"];
	else if (asset_type == LLAssetType::AT_TEXTURE ||
		asset_type == LLAssetType::AT_SOUND ||
		asset_type == LLAssetType::AT_ANIMATION ||
		asset_type == LLAssetType::AT_MESH)
	{
		LLAgentBenefitsMgr::current().findUploadCost(asset_type, expected_upload_cost);
	}

	LL_INFOS() << "Adding " << content["new_inventory_item"].asUUID() << " "
			<< content["new_asset"].asUUID() << " to inventory." << LL_ENDL;
	on_new_single_inventory_upload_complete(
		asset_type,
		inventory_type,
		mPostData["asset_type"].asString(),
		mPostData["folder_id"].asUUID(),
		mPostData["name"],
		mPostData["description"],
		content,
		expected_upload_cost);

	// continue uploading for bulk uploads
	
	/* Singu Note: sUploadQueue was never getting populated, anywhere! Therefore, this entire block never was reached.
	** I have condensed it to here in the hopes it may one day see use.  Apparently, it came in with Siana's prep work
	** for mesh upload (697dd7e9298282590f8cf858a58335f70302532b), but we never needed it.
	static std::deque<std::string> sUploadQueue;
	if (!sUploadQueue.empty())
	{
		std::string next_file = sUploadQueue.front();
		sUploadQueue.pop_front();
		if (next_file.empty()) return;
		std::string name = gDirUtilp->getBaseFileName(next_file, true);

		std::string asset_name = name;
		LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
		LLStringUtil::replaceChar(asset_name, '|', '?');
		LLStringUtil::stripNonprintable(asset_name);
		LLStringUtil::trim(asset_name);

		// We need to extract the originally requested permissions data, if any,
		// and use them for each next file to be uploaded. Note the requested
		// perms are not the same as the granted ones found in the given
		// "content" structure but can still be found in mPostData. -MG
		U32 everyone_perms =
			mPostData.has("everyone_mask") ?
			mPostData.get("everyone_mask").asInteger() :
			PERM_NONE;

		U32 group_perms =
			mPostData.has("group_mask") ?
			mPostData.get("group_mask").asInteger() :
			PERM_NONE;

		U32 next_owner_perms =
			mPostData.has("next_owner_mask") ?
			mPostData.get("next_owner_mask").asInteger() :
			PERM_NONE;

		std::string display_name = LLStringUtil::null;
		LLAssetStorage::LLStoreAssetCallback callback = NULL;
		void *userdata = NULL;

		upload_new_resource(
			next_file,
			asset_name,
			asset_name,
			0,
			LLFolderType::FT_NONE,
			LLInventoryType::IT_NONE,
			next_owner_perms,
			group_perms,
			everyone_perms,
			display_name,
			callback,
			expected_upload_cost,
			userdata);
	}
	*/
}

LLUpdateAgentInventoryResponder::LLUpdateAgentInventoryResponder(
	const LLSD& post_data,
	const LLUUID& vfile_id,
	LLAssetType::EType asset_type)
	: LLAssetUploadResponder(post_data, vfile_id, asset_type)
{
}

LLUpdateAgentInventoryResponder::LLUpdateAgentInventoryResponder(
	const LLSD& post_data,
	const std::string& file_name,
	LLAssetType::EType asset_type)
	: LLAssetUploadResponder(post_data, file_name, asset_type)
{
}

//virtual 
void LLUpdateAgentInventoryResponder::uploadComplete(const LLSD& content)
{
	LL_INFOS() << "LLUpdateAgentInventoryResponder::result from capabilities" << LL_ENDL;
	LLUUID item_id = mPostData["item_id"];

	LLViewerInventoryItem* item = (LLViewerInventoryItem*)gInventory.getItem(item_id);
	if(!item)
	{
		LL_WARNS() << "Inventory item for " << mVFileID
			<< " is no longer in agent inventory." << LL_ENDL;
		return;
	}

	// Update viewer inventory item
	LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
	new_item->setAssetUUID(content["new_asset"].asUUID());
	gInventory.updateItem(new_item);
	gInventory.notifyObservers();

	LL_INFOS() << "Inventory item " << item->getName() << " saved into "
		<< content["new_asset"].asString() << LL_ENDL;

	LLInventoryType::EType inventory_type = new_item->getInventoryType();
	switch(inventory_type)
	{
		case LLInventoryType::IT_NOTECARD:
		{
			// Update the UI with the new asset.
			LLPreviewNotecard* nc = (LLPreviewNotecard*)LLPreview::find(new_item->getUUID());
			if (nc)
			{
				// *HACK: we have to delete the asset in the VFS so
				// that the viewer will redownload it. This is only
				// really necessary if the asset had to be modified by
				// the uploader, so this can be optimized away in some
				// cases. A better design is to have a new uuid if the
				// script actually changed the asset.
				if(nc->hasEmbeddedInventory())
				{
					gVFS->removeFile(content["new_asset"].asUUID(), LLAssetType::AT_NOTECARD);
				}
				nc->refreshFromInventory();
			}
			break;
		}
		case LLInventoryType::IT_LSL:
		{
			// Find our window and close it if requested.
			LLPreviewLSL* preview = (LLPreviewLSL*)LLPreview::find(item_id);
			if (preview)
			{
				// Bytecode save completed
				if (content["compiled"])
				{
					preview->callbackLSLCompileSucceeded();
				}
				else
				{
					preview->callbackLSLCompileFailed(content["errors"]);
				}
			}
			break;
		}

		case LLInventoryType::IT_GESTURE:
		{
			// If this gesture is active, then we need to update the in-memory
		  // active map with the new pointer.				
		  if (LLGestureMgr::instance().isGestureActive(item_id))
		  {
			  LLUUID asset_id = new_item->getAssetUUID();
			  LLGestureMgr::instance().replaceGesture(item_id, asset_id);
			  gInventory.notifyObservers();
			}

			//gesture will have a new asset_id
			LLPreviewGesture* previewp = (LLPreviewGesture*)LLPreview::find(item_id);
			if (previewp)
			{
				previewp->onUpdateSucceeded();
			}

			break;
		}
		case LLInventoryType::IT_WEARABLE:
		default:
			break;
	}
}


LLUpdateTaskInventoryResponder::LLUpdateTaskInventoryResponder(const LLSD& post_data,
															   const LLUUID& vfile_id,
															   LLAssetType::EType asset_type)
:	LLAssetUploadResponder(post_data, vfile_id, asset_type)
{
}

LLUpdateTaskInventoryResponder::LLUpdateTaskInventoryResponder(const LLSD& post_data,
															   const std::string& file_name, 
															   LLAssetType::EType asset_type)
:	LLAssetUploadResponder(post_data, file_name, asset_type)
{
}

LLUpdateTaskInventoryResponder::LLUpdateTaskInventoryResponder(const LLSD& post_data,
															   const std::string& file_name,
															   const LLUUID& queue_id, 
															   LLAssetType::EType asset_type)
:	LLAssetUploadResponder(post_data, file_name, asset_type), mQueueId(queue_id)
{
}

//virtual 
void LLUpdateTaskInventoryResponder::uploadComplete(const LLSD& content)
{
	LL_INFOS() << "LLUpdateTaskInventoryResponder::result from capabilities" << LL_ENDL;
	LLUUID item_id = mPostData["item_id"];
	LLUUID task_id = mPostData["task_id"];

	dialog_refresh_all();
	
	switch(mAssetType)
	{
		case LLAssetType::AT_NOTECARD:
		{
			// Update the UI with the new asset.
			LLPreviewNotecard* nc = (LLPreviewNotecard*)LLPreview::find(item_id);
			if (nc)
			{
				// *HACK: we have to delete the asset in the VFS so
				// that the viewer will redownload it. This is only
				// really necessary if the asset had to be modified by
				// the uploader, so this can be optimized away in some
				// cases. A better design is to have a new uuid if the
				// script actually changed the asset.
				if (nc->hasEmbeddedInventory())
				{
					gVFS->removeFile(content["new_asset"].asUUID(),
									 LLAssetType::AT_NOTECARD);
				}
				nc->setAssetId(content["new_asset"].asUUID());
				nc->refreshFromInventory();
			}
			break;
		}
		case LLAssetType::AT_LSL_TEXT:
		{
			if (mQueueId.notNull())
			{
				LLFloaterCompileQueue* queue = (LLFloaterCompileQueue*) LLFloaterScriptQueue::findInstance(mQueueId);
				if (NULL != queue)
				{
					queue->removeItemByItemID(item_id);
				}
			}
			else
			{
				LLLiveLSLEditor* preview = static_cast<LLLiveLSLEditor*>(LLPreview::find(item_id));
				if (preview)
				{
					// Bytecode save completed
					if (content["compiled"])
					{
						preview->callbackLSLCompileSucceeded(task_id, item_id, mPostData["is_script_running"]);
					}
					else
					{
						preview->callbackLSLCompileFailed(content["errors"]);
					}
				}
			}
			break;
		}
		default:
			break;
	}
}


/////////////////////////////////////////////////////
// LLNewAgentInventoryVariablePriceResponder::Impl //
/////////////////////////////////////////////////////
class LLNewAgentInventoryVariablePriceResponder::Impl
{
public:
	Impl(
		const LLUUID& vfile_id,
		LLAssetType::EType asset_type,
		const LLSD& inventory_data) :
		mVFileID(vfile_id),
		mAssetType(asset_type),
		mInventoryData(inventory_data),
		mFileName("")
	{
		if (!gVFS->getExists(vfile_id, asset_type))
		{
			LL_WARNS()
				<< "LLAssetUploadResponder called with nonexistant "
				<< "vfile_id " << vfile_id << LL_ENDL;
			mVFileID.setNull();
			mAssetType = LLAssetType::AT_NONE;
		}
	}

	Impl(
		const std::string& file_name,
		LLAssetType::EType asset_type,
		const LLSD& inventory_data) :
		mFileName(file_name),
		mAssetType(asset_type),
		mInventoryData(inventory_data)
	{
		mVFileID.setNull();
	}

	std::string getFilenameOrIDString() const
	{
		return (mFileName.empty() ? mVFileID.asString() : mFileName);
	}

	LLUUID getVFileID() const
	{
		return mVFileID;
	}

	std::string getFilename() const
	{
		return mFileName;
	}

	LLAssetType::EType getAssetType() const
	{
		return mAssetType;
	}

	LLInventoryType::EType getInventoryType() const
	{
		return LLInventoryType::lookup(
			mInventoryData["inventory_type"].asString());
	}

	std::string getInventoryTypeString() const
	{
		return mInventoryData["inventory_type"].asString();
	}

	LLUUID getFolderID() const
	{
		return mInventoryData["folder_id"].asUUID();
	}

	std::string getItemName() const
	{
		return mInventoryData["name"].asString();
	}

	std::string getItemDescription() const
	{
		return mInventoryData["description"].asString();
	}

	void displayCannotUploadReason(const std::string& reason)
	{
		LLSD args;
		args["FILE"] = getFilenameOrIDString();
		args["REASON"] = reason;


		LLNotificationsUtil::add("CannotUploadReason", args);
		LLUploadDialog::modalUploadFinished();
	}

	void onApplicationLevelError(const LLSD& error)
	{
		static const std::string _IDENTIFIER = "identifier";

		static const std::string _INSUFFICIENT_FUNDS =
			"NewAgentInventory_InsufficientLindenDollarBalance";
		static const std::string _MISSING_REQUIRED_PARAMETER =
			"NewAgentInventory_MissingRequiredParamater";
		static const std::string _INVALID_REQUEST_BODY =
			"NewAgentInventory_InvalidRequestBody";
		static const std::string _RESOURCE_COST_DIFFERS =
			"NewAgentInventory_ResourceCostDiffers";

		static const std::string _MISSING_PARAMETER = "missing_parameter";
		static const std::string _INVALID_PARAMETER = "invalid_parameter";
		static const std::string _MISSING_RESOURCE = "missing_resource";
		static const std::string _INVALID_RESOURCE = "invalid_resource";

		// TODO* Add the other error_identifiers

		std::string error_identifier = error[_IDENTIFIER].asString();

		// TODO*: Pull these user visible strings from an xml file
		// to be localized
		if (_INSUFFICIENT_FUNDS == error_identifier)
		{
			displayCannotUploadReason("You do not have a sufficient L$ balance to complete this upload.");
		}
		else if (_MISSING_REQUIRED_PARAMETER == error_identifier)
		{
			// Missing parameters
			if (error.has(_MISSING_PARAMETER) )
			{
				std::string message = 
					"Upload request was missing required parameter '[P]'";
				LLStringUtil::replaceString(
					message,
					"[P]",
					error[_MISSING_PARAMETER].asString());

				displayCannotUploadReason(message);
			}
			else
			{
				std::string message = 
					"Upload request was missing a required parameter";
				displayCannotUploadReason(message);					
			}
		}
		else if ( _INVALID_REQUEST_BODY == error_identifier )
		{
			// Invalid request body, check to see if 
			// a particular parameter was invalid
			if ( error.has(_INVALID_PARAMETER) )
			{
				std::string message = "Upload parameter '[P]' is invalid.";
				LLStringUtil::replaceString(
					message,
					"[P]",
					error[_INVALID_PARAMETER].asString());

				// See if the server also responds with what resource
				// is missing.
				if ( error.has(_MISSING_RESOURCE) )
				{
					message += "\nMissing resource '[R]'.";

					LLStringUtil::replaceString(
						message,
						"[R]",
						error[_MISSING_RESOURCE].asString());
				}
				else if ( error.has(_INVALID_RESOURCE) )
				{
					message += "\nInvalid resource '[R]'.";

					LLStringUtil::replaceString(
						message,
						"[R]",
						error[_INVALID_RESOURCE].asString());
				}

				displayCannotUploadReason(message);
			}
			else
			{
				std::string message = "Upload request was malformed";
				displayCannotUploadReason(message);					
			}
		}
		else if (_RESOURCE_COST_DIFFERS == error_identifier)
		{
			displayCannotUploadReason("The resource cost associated with this upload is not consistent with the server.");
		}
		else
		{
			displayCannotUploadReason("Unknown Error");
		}
	}

	void onTransportError()
	{
		displayCannotUploadReason(
				"The server is experiencing unexpected difficulties.");
	}

	void onTransportError(const LLSD& error)
	{
		static const std::string _IDENTIFIER = "identifier";

		static const std::string _SERVER_ERROR_AFTER_CHARGE =
			"NewAgentInventory_ServerErrorAfterCharge";

		std::string error_identifier = error[_IDENTIFIER].asString();

		// TODO*: Pull the user visible strings from an xml file
		// to be localized

		if ( _SERVER_ERROR_AFTER_CHARGE == error_identifier )
		{
			displayCannotUploadReason(
				"The server is experiencing unexpected difficulties.  You may have been charged for the upload.");
		}
		else
		{
			displayCannotUploadReason(
				"The server is experiencing unexpected difficulties.");
		}
	}

	bool uploadConfirmationCallback(
		const LLSD& notification,
		const LLSD& response,
		boost::intrusive_ptr<LLNewAgentInventoryVariablePriceResponder> responder)
	{
		S32 option;
		std::string confirmation_url;

		option = LLNotificationsUtil::getSelectedOption(
			notification,
			response);

		confirmation_url =
			notification["payload"]["confirmation_url"].asString();

		// Yay!  We are confirming or cancelling our upload
		switch(option)
		{
		case 0:
		    {
				confirmUpload(confirmation_url, responder);
			}
			break;
		case 1:
		default:
			break;
		}

		return false;
	}

	void confirmUpload(
		const std::string& confirmation_url,
		boost::intrusive_ptr<LLNewAgentInventoryVariablePriceResponder> responder)
	{
		if ( getFilename().empty() )
		{
			// we have no filename, use virtual file ID instead
			LLHTTPClient::postFile(
				confirmation_url,
				getVFileID(),
				getAssetType(),
				responder);
		}
		else
		{
			LLHTTPClient::postFile(
				confirmation_url,
				getFilename(),
				responder);
		}
	}


private:
	std::string mFileName;

	LLSD mInventoryData;
	LLAssetType::EType mAssetType;
	LLUUID mVFileID;
};

///////////////////////////////////////////////
// LLNewAgentInventoryVariablePriceResponder //
///////////////////////////////////////////////
LLNewAgentInventoryVariablePriceResponder::LLNewAgentInventoryVariablePriceResponder(
	const LLUUID& vfile_id,
	LLAssetType::EType asset_type,
	const LLSD& inventory_info)
{
	mImpl = new Impl(
		vfile_id,
		asset_type,
		inventory_info);
}

LLNewAgentInventoryVariablePriceResponder::LLNewAgentInventoryVariablePriceResponder(
	const std::string& file_name,
	LLAssetType::EType asset_type,
	const LLSD& inventory_info)
{
	mImpl = new Impl(
		file_name,
		asset_type,
		inventory_info);
}

LLNewAgentInventoryVariablePriceResponder::~LLNewAgentInventoryVariablePriceResponder()
{
	delete mImpl;
}

void LLNewAgentInventoryVariablePriceResponder::httpFailure()
{
	const LLSD& content = getContent();
	LL_WARNS("Upload") << dumpResponse() << LL_ENDL;

		static const std::string _ERROR = "error";
	if ( content.has(_ERROR) )
	{
		mImpl->onTransportError(content[_ERROR]);
	}
	else
	{
		mImpl->onTransportError();
	}
}

void LLNewAgentInventoryVariablePriceResponder::httpSuccess()
{
	const LLSD& content = getContent();
	if (!content.isMap())
	{
		failureResult(HTTP_INTERNAL_ERROR_OTHER, "Malformed response contents", content);
		return;
	}
	// Parse out application level errors and the appropriate
	// responses for them
	static const std::string _ERROR = "error";
	static const std::string _STATE = "state";

	static const std::string _COMPLETE = "complete";
	static const std::string _CONFIRM_UPLOAD = "confirm_upload";

	static const std::string _UPLOAD_PRICE = "upload_price";
	static const std::string _RESOURCE_COST = "resource_cost";
	static const std::string _RSVP = "rsvp";

	// Check for application level errors
	if ( content.has(_ERROR) )
	{
		LL_WARNS("Upload") << dumpResponse() << LL_ENDL;
		onApplicationLevelError(content[_ERROR]);
		return;
	}

	std::string state = content[_STATE];
	LLAssetType::EType asset_type = mImpl->getAssetType();

	if (_COMPLETE == state)
	{
		// rename file in VFS with new asset id
		if (mImpl->getFilename().empty())
		{
			// rename the file in the VFS to the actual asset id
			// LL_INFOS() << "Changing uploaded asset UUID to " << content["new_asset"].asUUID() << LL_ENDL;
			gVFS->renameFile(
				mImpl->getVFileID(),
				asset_type,
				content["new_asset"].asUUID(),
				asset_type);
		}

 		on_new_single_inventory_upload_complete(
 			asset_type,
 			mImpl->getInventoryType(),
			mImpl->getInventoryTypeString(),
			mImpl->getFolderID(),
			mImpl->getItemName(),
			mImpl->getItemDescription(),
			content,
			content[_UPLOAD_PRICE].asInteger());

		// TODO* Add bulk (serial) uploading or add
		// a super class of this that does so
	}
	else if ( _CONFIRM_UPLOAD == state )
	{
		showConfirmationDialog(
			content[_UPLOAD_PRICE].asInteger(),
			content[_RESOURCE_COST].asInteger(),
			content[_RSVP].asString());
	}
	else
	{
		LL_WARNS("Upload") << dumpResponse() << LL_ENDL;
		onApplicationLevelError("");
	}
}

void LLNewAgentInventoryVariablePriceResponder::onApplicationLevelError(
	const LLSD& error)
{
	mImpl->onApplicationLevelError(error);
}

void LLNewAgentInventoryVariablePriceResponder::showConfirmationDialog(
	S32 upload_price,
	S32 resource_cost,
	const std::string& confirmation_url)
{
	if (0 == upload_price)
	{
		// don't show confirmation dialog for free uploads, I mean,
		// they're free!

		// The creating of a new instrusive_ptr(this)
		// creates a new boost::intrusive_ptr
		// which is a copy of this.  This code is required because
		// 'this' is always of type Class* and not the intrusive_ptr,
		// and thus, a reference to 'this' is not registered
		// by using just plain 'this'.

		// Since LLNewAgentInventoryVariablePriceResponder is a
		// reference counted class, it is possible (since the
		// reference to a plain 'this' would be missed here) that,
		// when using plain ol' 'this', that this object
		// would be deleted before the callback is triggered
		// and cause sadness.
		mImpl->confirmUpload(
			confirmation_url,
			boost::intrusive_ptr<LLNewAgentInventoryVariablePriceResponder>(this));
	}
	else
	{
		LLSD substitutions;
		LLSD payload;

		substitutions["PRICE"] = upload_price;

		payload["confirmation_url"] = confirmation_url;

		// The creating of a new instrusive_ptr(this)
		// creates a new boost::intrusive_ptr
		// which is a copy of this.  This code is required because
		// 'this' is always of type Class* and not the intrusive_ptr,
		// and thus, a reference to 'this' is not registered
		// by using just plain 'this'.

		// Since LLNewAgentInventoryVariablePriceResponder is a
		// reference counted class, it is possible (since the
		// reference to a plain 'this' would be missed here) that,
		// when using plain ol' 'this', that this object
		// would be deleted before the callback is triggered
		// and cause sadness.
		LLNotificationsUtil::add(
			"UploadCostConfirmation",
			substitutions,
			payload,
			boost::bind(
				&LLNewAgentInventoryVariablePriceResponder::Impl::uploadConfirmationCallback,
				mImpl,
				_1,
				_2,
				boost::intrusive_ptr<LLNewAgentInventoryVariablePriceResponder>(this)));
	}
}


