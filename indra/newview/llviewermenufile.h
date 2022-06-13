/** 
 * @file llviewermenufile.h
 * @brief "File" menu in the main menu bar.
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

#ifndef LLVIEWERMENUFILE_H
#define LLVIEWERMENUFILE_H

#include "llassettype.h"
#include "llinventorytype.h"
// <edit>
#include "llviewerinventory.h"

#include "statemachine/aifilepicker.h"

class NewResourceItemCallback : public LLInventoryCallback
{
 void fire(const LLUUID& inv_item);
};
// </edit>

class LLTransactionID;

void init_menu_file();

void upload_new_resource(const std::string& src_filename,
						 std::string name,
						 std::string desc,
						 S32 compression_info,
						 LLFolderType::EType destination_folder_type,
						 LLInventoryType::EType inv_type,
						 U32 next_owner_perms,
						 U32 group_perms,
						 U32 everyone_perms,
						 const std::string& display_name,
						 LLAssetStorage::LLStoreAssetCallback callback,
						 S32 expected_upload_cost,
						 void *userdata);

// Return false if no upload attempt was done (and the callback will not be called).
bool upload_new_resource(const LLTransactionID &tid, 
						 LLAssetType::EType type,
						 std::string name,
						 std::string desc, 
						 S32 compression_info,
						 LLFolderType::EType destination_folder_type,
						 LLInventoryType::EType inv_type,
						 U32 next_owner_perms,
						 U32 group_perms,
						 U32 everyone_perms,
						 const std::string& display_name,
						 LLAssetStorage::LLStoreAssetCallback callback,
						 S32 expected_upload_cost,
						 void *userdata);

// The default callback functions, called when 'callback' == NULL (for normal and temporary uploads).
// user_data must be a LLResourceData allocated with new (or NULL).
void upload_done_callback(const LLUUID& uuid, void* user_data, S32 result, LLExtStat ext_status);
void temp_upload_callback(const LLUUID& uuid, void* user_data, S32 result, LLExtStat ext_status);

LLAssetID generate_asset_id_for_new_upload(const LLTransactionID& tid);

void increase_new_upload_stats(LLAssetType::EType asset_type);

void assign_defaults_and_show_upload_message(LLAssetType::EType asset_type,
											 LLInventoryType::EType& inventory_type,
											 std::string& name,
											 const std::string& display_name,
											 std::string& description);

LLSD generate_new_resource_upload_capability_body(LLAssetType::EType asset_type,
												  const std::string& name,
												  const std::string& desc,
												  LLFolderType::EType destination_folder_type,
												  LLInventoryType::EType inv_type,
												  U32 next_owner_perms,
												  U32 group_perms,
												  U32 everyone_perms);


class LLFilePickerThread : public LLThread
{	//multi-threaded file picker (runs system specific file picker in background and calls "notify" from main thread)
public:

	static std::queue<LLFilePickerThread*> sDeadQ;
	static LLMutex* sMutex;

	static void initClass();
	static void cleanupClass();
	static void clearDead();

	std::string mFile; 

	ELoadFilter mFilter;

	LLFilePickerThread(ELoadFilter filter)
	:	LLThread("file picker"), mFilter(filter)
	{
	}

	void getFile();

	virtual void run();

	virtual void notify(const std::string& filename) = 0;
};

#endif
