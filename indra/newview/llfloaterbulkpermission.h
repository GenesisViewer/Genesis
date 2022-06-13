/** 
 * @file llfloaterbulkpermissions.h
 * @brief Allow multiple task inventory properties to be set in one go.
 * @author Michelle2 Zenovka
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

#ifndef LL_LLBULKPERMISSION_H
#define LL_LLBULKPERMISSION_H

#include "llinventory.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"
#include "llmap.h"
#include "lluuid.h"

#include "llfloater.h"
#include "llscrolllistctrl.h"

class LLFloaterBulkPermission : public LLFloater, public LLVOInventoryListener, public LLFloaterSingleton<LLFloaterBulkPermission>
{
public:

	LLFloaterBulkPermission(const LLSD& seed);

private:
	virtual ~LLFloaterBulkPermission() {}

	BOOL start(); // returns TRUE if the queue has started, otherwise FALSE.
	BOOL nextObject();
	BOOL popNext();

	// This is the callback method for the viewer object currently
	// being worked on.
	/*virtual*/ void inventoryChanged(LLViewerObject* obj,
								 LLInventoryObject::object_list_t* inv,
								 S32 serial_num,
								 void* queue);
	
	// This is called by inventoryChanged
	void handleInventory(LLViewerObject* viewer_obj,
								LLInventoryObject::object_list_t* inv);


	void updateInventory(LLViewerObject* object,
								LLViewerInventoryItem* item,
								U8 key,
								bool is_new);

	static void onHelpBtn();
	void onCloseBtn();
	void onApplyBtn();
	void onCommitCopy();
	void onCheckAll() { doCheckUncheckAll(TRUE); }
	void onUncheckAll() { doCheckUncheckAll(FALSE); }
	
	// returns true if this is done
	BOOL isDone() const { return (mCurrentObjectID.isNull() || (mObjectIDs.size() == 0)); }

	//Read the settings and Apply the permissions
	void doApply();
	void doCheckUncheckAll(BOOL check);

private:
	// Object Queue
	uuid_vec_t mObjectIDs;
	LLUUID mCurrentObjectID;
	BOOL mDone;

	LLUUID mID;
};

#endif

