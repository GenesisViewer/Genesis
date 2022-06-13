/** 
 * @file llfloaterbulkpermissions.cpp
 * @author Michelle2 Zenovka
 * @brief A floater which allows task inventory item's properties to be changed on mass.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2008-2009, Linden Research, Inc.
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
#include "llfloaterbulkpermission.h"
#include "llfloaterperms.h" // for utilities
#include "llagent.h"
#include "llchat.h"
#include "llinventorydefines.h"
#include "llviewerwindow.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
//#include "lscript_rt_interface.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llresmgr.h"
#include "llbutton.h"
#include "lldir.h"
#include "llviewerstats.h"
#include "lluictrlfactory.h"
#include "llselectmgr.h"
#include "llcheckboxctrl.h"
#include "llnotificationsutil.h"

#include "roles_constants.h" // for GP_OBJECT_MANIPULATE


LLFloaterBulkPermission::LLFloaterBulkPermission(const LLSD& seed)
:	LLFloater(),
	mDone(FALSE)
{
	mID.generate();
	mCommitCallbackRegistrar.add("BulkPermission.Help",	boost::bind(LLFloaterBulkPermission::onHelpBtn));
	mCommitCallbackRegistrar.add("BulkPermission.Apply",	boost::bind(&LLFloaterBulkPermission::onApplyBtn, this));
	mCommitCallbackRegistrar.add("BulkPermission.Close",	boost::bind(&LLFloaterBulkPermission::onCloseBtn, this));
	mCommitCallbackRegistrar.add("BulkPermission.CheckAll",	boost::bind(&LLFloaterBulkPermission::onCheckAll, this));
	mCommitCallbackRegistrar.add("BulkPermission.UncheckAll",	boost::bind(&LLFloaterBulkPermission::onUncheckAll, this));
	mCommitCallbackRegistrar.add("BulkPermission.CommitCopy",	boost::bind(&LLFloaterBulkPermission::onCommitCopy, this));
	LLUICtrlFactory::getInstance()->buildFloater(this,"floater_bulk_perms.xml");
	childSetEnabled("next_owner_transfer", gSavedSettings.getBOOL("BulkChangeNextOwnerCopy"));
}

void LLFloaterBulkPermission::doApply()
{
	// Inspects a stream of selected object contents and adds modifiable ones to the given array.
	class ModifiableGatherer : public LLSelectedNodeFunctor
	{
	public:
		ModifiableGatherer(uuid_vec_t& q) : mQueue(q) {}
		virtual bool apply(LLSelectNode* node)
		{
			if( node->allowOperationOnNode(PERM_MODIFY, GP_OBJECT_MANIPULATE) )
			{
				mQueue.push_back(node->getObject()->getID());
			}
			return true;
		}
	private:
		uuid_vec_t& mQueue;
	};
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");
	list->deleteAllItems();
	ModifiableGatherer gatherer(mObjectIDs);
	LLSelectMgr::getInstance()->getSelection()->applyToNodes(&gatherer);
	if(mObjectIDs.empty())
	{
		list->setCommentText(getString("nothing_to_modify_text"));
	}
	else
	{
		mDone = FALSE;
		if (!start())
		{
			LL_WARNS() << "Unexpected bulk permission change failure." << LL_ENDL;
		}
	}
}


// This is the callback method for the viewer object currently being
// worked on.
// NOT static, virtual!
void LLFloaterBulkPermission::inventoryChanged(LLViewerObject* viewer_object,
											 LLInventoryObject::object_list_t* inv,
											 S32,
											 void* q_id)
{
	//LL_INFOS() << "changed object: " << viewer_object->getID() << LL_ENDL;

	//Remove this listener from the object since its
	//listener callback is now being executed.
	
	//We remove the listener here because the function
	//removeVOInventoryListener removes the listener from a ViewerObject
	//which it internally stores.
	
	//If we call this further down in the function, calls to handleInventory
	//and nextObject may update the interally stored viewer object causing
	//the removal of the incorrect listener from an incorrect object.
	
	//Fixes SL-6119:Recompile scripts fails to complete
	removeVOInventoryListener();

	if (viewer_object && inv && (viewer_object->getID() == mCurrentObjectID) )
	{
		handleInventory(viewer_object, inv);
	}
	else
	{
		// something went wrong...
		// note that we're not working on this one, and move onto the
		// next object in the list.
		LL_WARNS() << "No inventory for " << mCurrentObjectID << LL_ENDL;
		nextObject();
	}
}

void LLFloaterBulkPermission::onApplyBtn()
{
	doApply();
}

void LLFloaterBulkPermission::onHelpBtn()
{
	LLNotificationsUtil::add("HelpBulkPermission");
}

void LLFloaterBulkPermission::onCloseBtn()
{
	onClose(false);
}

//static 
void LLFloaterBulkPermission::onCommitCopy()
{
	// Implements fair use
	BOOL copyable = gSavedSettings.getBOOL("BulkChangeNextOwnerCopy");
	if(!copyable)
	{
		gSavedSettings.setBOOL("BulkChangeNextOwnerTransfer", TRUE);
	}
	LLCheckBoxCtrl* xfer = getChild<LLCheckBoxCtrl>("next_owner_transfer");
	xfer->setEnabled(copyable);
}

BOOL LLFloaterBulkPermission::start()
{
	// note: number of top-level objects to modify is mObjectIDs.count().
	getChild<LLScrollListCtrl>("queue output")->addSimpleElement(getString("start_text"));
	return nextObject();
}

// Go to the next object and start if found. Returns false if no objects left, true otherwise.
BOOL LLFloaterBulkPermission::nextObject()
{
	S32 count;
	BOOL successful_start = FALSE;
	do
	{
		count = mObjectIDs.size();
		//LL_INFOS() << "Objects left to process = " << count << LL_ENDL;
		mCurrentObjectID.setNull();
		if(count > 0)
		{
			successful_start = popNext();
			//LL_INFOS() << (successful_start ? "successful" : "unsuccessful") << LL_ENDL; 
		}
	} while((mObjectIDs.size() > 0) && !successful_start);

	if(isDone() && !mDone)
	{
		getChild<LLScrollListCtrl>("queue output")->addSimpleElement(getString("done_text"));
		mDone = TRUE;
	}
	return successful_start;
}

// Pop the top object off of the queue.
// Return TRUE if the queue has started, otherwise FALSE.
BOOL LLFloaterBulkPermission::popNext()
{
	// get the head element from the container, and attempt to get its inventory.
	BOOL rv = FALSE;
	S32 count = mObjectIDs.size();
	if(mCurrentObjectID.isNull() && (count > 0))
	{
		mCurrentObjectID = mObjectIDs.at(0);
		//LL_INFOS() << "mCurrentID: " << mCurrentObjectID << LL_ENDL;
		mObjectIDs.erase(mObjectIDs.begin());
		LLViewerObject* obj = gObjectList.findObject(mCurrentObjectID);
		if(obj)
		{
			//LL_INFOS() << "requesting inv for " << mCurrentObjectID << LL_ENDL;
			LLUUID* id = new LLUUID(mID);
			registerVOInventoryListener(obj,id);
			requestVOInventory();
			rv = TRUE;
		}
		else
		{
			LL_INFOS() <<"NULL LLViewerObject" <<LL_ENDL;
		}
	}

	return rv;
}


void LLFloaterBulkPermission::doCheckUncheckAll(BOOL check)
{
	gSavedSettings.setBOOL("BulkChangeIncludeAnimations", check);
	gSavedSettings.setBOOL("BulkChangeIncludeBodyParts" , check);
	gSavedSettings.setBOOL("BulkChangeIncludeClothing"  , check);
	gSavedSettings.setBOOL("BulkChangeIncludeGestures"  , check);
	gSavedSettings.setBOOL("BulkChangeIncludeLandmarks" , check);
	gSavedSettings.setBOOL("BulkChangeIncludeNotecards" , check);
	gSavedSettings.setBOOL("BulkChangeIncludeObjects"   , check);
	gSavedSettings.setBOOL("BulkChangeIncludeScripts"   , check);
	gSavedSettings.setBOOL("BulkChangeIncludeSounds"    , check);
	gSavedSettings.setBOOL("BulkChangeIncludeTextures"  , check);
}


void LLFloaterBulkPermission::handleInventory(LLViewerObject* viewer_obj, LLInventoryObject::object_list_t* inv)
{
	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("queue output");

	LLInventoryObject::object_list_t::const_iterator it = inv->begin();
	LLInventoryObject::object_list_t::const_iterator end = inv->end();
	for ( ; it != end; ++it)
	{
		LLAssetType::EType asstype = (*it)->getType();
		if(
			( asstype == LLAssetType::AT_ANIMATION && gSavedSettings.getBOOL("BulkChangeIncludeAnimations")) ||
			( asstype == LLAssetType::AT_BODYPART  && gSavedSettings.getBOOL("BulkChangeIncludeBodyParts" )) ||
			( asstype == LLAssetType::AT_CLOTHING  && gSavedSettings.getBOOL("BulkChangeIncludeClothing"  )) ||
			( asstype == LLAssetType::AT_GESTURE   && gSavedSettings.getBOOL("BulkChangeIncludeGestures"  )) ||
			( asstype == LLAssetType::AT_LANDMARK  && gSavedSettings.getBOOL("BulkChangeIncludeLandmarks" )) ||
			( asstype == LLAssetType::AT_NOTECARD  && gSavedSettings.getBOOL("BulkChangeIncludeNotecards" )) ||
			( asstype == LLAssetType::AT_OBJECT    && gSavedSettings.getBOOL("BulkChangeIncludeObjects"   )) ||
			( asstype == LLAssetType::AT_LSL_TEXT  && gSavedSettings.getBOOL("BulkChangeIncludeScripts"   )) ||
			( asstype == LLAssetType::AT_SOUND     && gSavedSettings.getBOOL("BulkChangeIncludeSounds"    )) ||
			( asstype == LLAssetType::AT_TEXTURE   && gSavedSettings.getBOOL("BulkChangeIncludeTextures"  )))
		{
			LLViewerObject* object = gObjectList.findObject(viewer_obj->getID());

			if (object)
			{
				LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
				LLViewerInventoryItem* new_item = (LLViewerInventoryItem*)item;
				LLPermissions perm(new_item->getPermissions());
				U32 flags = new_item->getFlags();

				U32 desired_next_owner_perms = LLFloaterPerms::getNextOwnerPerms("BulkChange");
				U32 desired_everyone_perms = LLFloaterPerms::getEveryonePerms("BulkChange");
				U32 desired_group_perms = LLFloaterPerms::getGroupPerms("BulkChange");

				// If next owner permissions have changed (and this is an object)
				// then set the slam permissions flag so that they are applied on rez.
				if((perm.getMaskNextOwner() != desired_next_owner_perms)
				   && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
				}
				// If everyone permissions have changed (and this is an object)
				// then set the overwrite everyone permissions flag so they
				// are applied on rez.
				if ((perm.getMaskEveryone() != desired_everyone_perms)
				    && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
				}
				// If group permissions have changed (and this is an object)
				// then set the overwrite group permissions flag so they
				// are applied on rez.
				if ((perm.getMaskGroup() != desired_group_perms)
				    && (new_item->getType() == LLAssetType::AT_OBJECT))
				{
					flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
				}

				// chomp the inventory name so it fits in the scroll window nicely
				// and the user can see the [OK]
				std::string invname;
				invname=item->getName().substr(0,item->getName().size() < 30 ? item->getName().size() : 30 );
				
				LLUIString status_text = getString("status_text");
				status_text.setArg("[NAME]", invname.c_str());
				// Check whether we appear to have the appropriate permissions to change permission on this item.
				// Although the server will disallow any forbidden changes, it is a good idea to guess correctly
				// so that we can warn the user. The risk of getting this check wrong is therefore the possibility
				// of incorrectly choosing to not attempt to make a valid change.
				//
				// Trouble is this is extremely difficult to do and even when we know the results
				// it is difficult to design the best messaging. Therefore in this initial implementation
				// we'll always try to set the requested permissions and consider all cases successful
				// and perhaps later try to implement a smarter, friendlier solution. -MG
				if(true
					//gAgent.allowOperation(PERM_MODIFY, perm, GP_OBJECT_MANIPULATE) // for group and everyone masks
					//|| something else // for next owner perms
					)
				{
					perm.setMaskNext(desired_next_owner_perms);
					perm.setMaskEveryone(desired_everyone_perms);
					perm.setMaskGroup(desired_group_perms);
					new_item->setPermissions(perm); // here's the beef
					new_item->setFlags(flags); // and the tofu
					updateInventory(object,new_item,TASK_INVENTORY_ITEM_KEY,FALSE);
					//status_text.setArg("[STATUS]", getString("status_ok_text"));
					status_text.setArg("[STATUS]", "");
				}
#if 0
				else
				{
					//status_text.setArg("[STATUS]", getString("status_bad_text"));
					status_text.setArg("[STATUS]", "");
				}
#endif
				list->addSimpleElement(status_text.getString());

				//TODO if we are an object inside an object we should check a recuse flag and if set
				//open the inventory of the object and recurse - Michelle2 Zenovka

				//	if(recurse &&  ( (*it)->getType() == LLAssetType::AT_OBJECT && processObject))
				//	{
				//		I think we need to get the UUID of the object inside the inventory
				//		call item->fetchFromServer();
				//		we need a call back to say item has arrived *sigh*
				//		we then need to do something like
				//		LLUUID* id = new LLUUID(mID);
				//		registerVOInventoryListener(obj,id);
				//		requestVOInventory();
				//	}
			}
		}
	}

	nextObject();
}


// Avoid inventory callbacks etc by just fire and forgetting the message with the permissions update
// we could do this via LLViewerObject::updateInventory but that uses inventory call backs and buggers
// us up and we would have a dodgy item iterator

void LLFloaterBulkPermission::updateInventory(LLViewerObject* object, LLViewerInventoryItem* item, U8 key, bool is_new)
{
	// This slices the object into what we're concerned about on the viewer. 
	// The simulator will take the permissions and transfer ownership.
	LLPointer<LLViewerInventoryItem> task_item =
		new LLViewerInventoryItem(item->getUUID(), mID, item->getPermissions(),
								  item->getAssetUUID(), item->getType(),
								  item->getInventoryType(),
								  item->getName(), item->getDescription(),
								  item->getSaleInfo(),
								  item->getFlags(),
								  item->getCreationDate());
	task_item->setTransactionID(item->getTransactionID());
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_UpdateTaskInventory);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_UpdateData);
	msg->addU32Fast(_PREHASH_LocalID, object->mLocalID);
	msg->addU8Fast(_PREHASH_Key, key);
	msg->nextBlockFast(_PREHASH_InventoryData);
	task_item->packMessage(msg);
	msg->sendReliable(object->getRegion()->getHost());
}

