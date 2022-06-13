/** 
 * @file llfloaterreporter.h
 * @author Andrew Meadows
 * @brief Abuse reports.
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

#ifndef LL_LLFLOATERREPORTER_H
#define LL_LLFLOATERREPORTER_H

#include "llfloater.h"
#include "lluuid.h"
#include "v3math.h"

class LLAvatarName;
class LLViewerTexture;
class LLInventoryItem;
class LLViewerObject;
class LLAgent;
class LLToolObjPicker;
class LLMeanCollisionData;
struct LLResourceData;

// these flags are used to label info requests to the server
//constexpr U32 BUG_REPORT_REQUEST 		= 0x01 << 0; // DEPRECATED
constexpr U32 COMPLAINT_REPORT_REQUEST 	= 0x01 << 1;
constexpr U32 OBJECT_PAY_REQUEST		= 0x01 << 2;


// ************************************************************
// THESE ENUMS ARE IN THE DATABASE!!!
//
// The process for adding a new report type is to:
// 1. Issue a command to the database to insert the new value:
//    insert into user_report_type (description)
//                values ('${new type name}');
// 2. Record the integer value assigned:
//    select type from user_report_type
//           where description='${new type name}';
// 3. Add it here.
//     ${NEW TYPE NAME}_REPORT = ${type_number};
//
// Failure to follow this process WILL result in incorrect
// queries on user reports.
// ************************************************************
enum EReportType
{
	NULL_REPORT = 0,		// don't use this value anywhere
	UNKNOWN_REPORT = 1,
	//BUG_REPORT = 2, // DEPRECATED
	COMPLAINT_REPORT = 3,
	CS_REQUEST_REPORT = 4
};

class LLFloaterReporter final
:	public LLFloater, public LLSingleton<LLFloaterReporter>
{
public:
	LLFloaterReporter();
	/*virtual*/ ~LLFloaterReporter();
	/*virtual*/ BOOL postBuild() override;

	void setReportType(EReportType type) { mReportType = type; }

	// Enables all buttons
	static void showFromMenu(EReportType report_type);

	static void showFromObject(const LLUUID& object_id, const LLUUID& experience_id = LLUUID::null);
	static void showFromAvatar(const LLUUID& avatar_id, const std::string avatar_name);
	static void showFromExperience(const LLUUID& experience_id);

	static void onClickSend			(void *userdata);
	static void onClickCancel		(void *userdata);
	static void onClickObjPicker	(void *userdata);
	void onClickSelectAbuser ();
	static void closePickTool	(void *userdata);
	static void uploadDoneCallback(const LLUUID &uuid, void* user_data, S32 result, LLExtStat ext_status);
	static void addDescription(const std::string& description, LLMeanCollisionData *mcd = nullptr);
	static void setDescription(const std::string& description, LLMeanCollisionData *mcd = nullptr);
	
	void setPickedObjectProperties(const std::string& object_name, const std::string& owner_name, const LLUUID owner_id);

private:
	static void show(const LLUUID& object_id, const std::string& avatar_name = LLStringUtil::null, const LLUUID& experience_id = LLUUID::null);

	void takeScreenshot();
	void sendReportViaCaps(std::string url);
	void uploadImage();
	bool validateReport();
	void setReporterID();
	LLSD gatherReport();
	void sendReportViaLegacy(const LLSD & report);
	void sendReportViaCaps(std::string url, std::string sshot_url, const LLSD & report);
	void setPosBox(const LLVector3d &pos);
	void enableControls(BOOL own_avatar);
	void getExperienceInfo(const LLUUID& object_id);
	void getObjectInfo(const LLUUID& object_id);
	void callbackAvatarID(const uuid_vec_t& ids, const std::vector<LLAvatarName>& names);
	void setFromAvatarID(const LLUUID& avatar_id);
	void onAvatarNameCache(const LLUUID& avatar_id, const LLAvatarName& av_name);

	static void requestAbuseCategoriesCoro(const struct LLCoroResponder& responder, std::string url, LLHandle<LLFloater> handle);

private:
	EReportType		mReportType;
	LLUUID 			mObjectID;
	LLUUID			mScreenID;
	LLUUID			mAbuserID;
	LLUUID			mExperienceID;
	// Store the real name, not the link, for upstream reporting
	std::string		mOwnerName;
	BOOL			mDeselectOnClose;
	BOOL 			mPicking;
	LLVector3		mPosition;
	BOOL			mCopyrightWarningSeen;
	std::list<LLMeanCollisionData*> mMCDList;
	std::string		mDefaultSummary;
	LLResourceData* mResourceDatap;
	boost::signals2::connection mAvatarNameCacheConnection;
};

#endif
