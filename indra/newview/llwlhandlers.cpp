/** 
 * @file llwlhandlers.cpp
 * @brief Various classes which handle Windlight-related messaging
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
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

#include "llwlhandlers.h"

#include "llagent.h"
#include "llviewerregion.h"
#include "llenvmanager.h"
#include "llnotificationsutil.h"

/****
 * LLEnvironmentRequest
 ****/
// static
bool LLEnvironmentRequest::initiate()
{
	LLViewerRegion* cur_region = gAgent.getRegion();

	if (!cur_region)
	{
		LL_WARNS("WindlightCaps") << "Viewer region not set yet, skipping env. settings request" << LL_ENDL;
		return false;
	}

	if (!cur_region->capabilitiesReceived())
	{
		LL_INFOS("WindlightCaps") << "Deferring windlight settings request until we've got region caps" << LL_ENDL;
		cur_region->setCapabilitiesReceivedCallback(boost::bind(&LLEnvironmentRequest::onRegionCapsReceived, _1));
		return false;
	}

	return doRequest();
}

// static
void LLEnvironmentRequest::onRegionCapsReceived(const LLUUID& region_id)
{
	if (region_id != gAgent.getRegion()->getRegionID())
	{
		LL_INFOS("WindlightCaps") << "Got caps for a non-current region" << LL_ENDL;
		return;
	}

	LL_DEBUGS("WindlightCaps") << "Received region capabilities" << LL_ENDL;
	doRequest();
}

// static
bool LLEnvironmentRequest::doRequest()
{
	std::string url = gAgent.getRegion()->getCapability("EnvironmentSettings");
	if (url.empty())
	{
		LL_INFOS("WindlightCaps") << "Skipping windlight setting request - we don't have this capability" << LL_ENDL;
		// region is apparently not capable of this; don't respond at all
		return false;
	}

	LL_INFOS("WindlightCaps") << "Requesting region windlight settings via " << url << LL_ENDL;
	LLHTTPClient::get(url, new LLEnvironmentRequestResponder());
	return true;
}

/****
 * LLEnvironmentRequestResponder
 ****/
int LLEnvironmentRequestResponder::sCount = 0; // init to 0

LLEnvironmentRequestResponder::LLEnvironmentRequestResponder()
{
	mID = ++sCount;
}
/*virtual*/ void LLEnvironmentRequestResponder::httpSuccess(void)
{
	LL_INFOS("WindlightCaps") << "Received region windlight settings" << LL_ENDL;

	if (mID != sCount)
	{
		LL_INFOS("WindlightCaps") << "Got superseded by another responder; ignoring..." << LL_ENDL;
	}
	else if (!gAgent.getRegion() || gAgent.getRegion()->getRegionID().isNull())
	{
		LL_WARNS("WindlightCaps") << "Ignoring responder. Current region is invalid." << LL_ENDL;
	}
	else if (mContent[0]["regionID"].asUUID().isNull())
	{
		LL_WARNS("WindlightCaps") << "Ignoring responder. Response from invalid region." << LL_ENDL;
	}
	else if (mContent[0]["regionID"].asUUID() != gAgent.getRegion()->getRegionID())
	{
		LL_WARNS("WindlightCaps") << "Not in the region from where this data was received (wanting "
			<< gAgent.getRegion()->getRegionID() << " but got " << mContent[0]["regionID"].asUUID()
			<< ") - ignoring..." << LL_ENDL;
	}
	else
	{
		LLEnvManagerNew::getInstance()->onRegionSettingsResponse(mContent);
	}
}
/*virtual*/ void LLEnvironmentRequestResponder::httpFailure(void)
{
	LL_INFOS("WindlightCaps") << "Got an error, not using region windlight..." << LL_ENDL;
	LLEnvManagerNew::getInstance()->onRegionSettingsResponse(LLSD());
}

/****
 * LLEnvironmentApply
 ****/

clock_t LLEnvironmentApply::UPDATE_WAIT_SECONDS = clock_t(3.f);
clock_t LLEnvironmentApply::sLastUpdate = clock_t(0.f);

// static
bool LLEnvironmentApply::initiateRequest(const LLSD& content)
{
	clock_t current = clock();

	// Make sure we don't update too frequently.
	if (current < sLastUpdate + (UPDATE_WAIT_SECONDS * CLOCKS_PER_SEC))
	{
		LLSD args(LLSD::emptyMap());
		args["WAIT"] = (F64)UPDATE_WAIT_SECONDS;
		LLNotificationsUtil::add("EnvUpdateRate", args);
		return false;
	}

	sLastUpdate = current;

	// Send update request.
	std::string url = gAgent.getRegionCapability("EnvironmentSettings");
	if (url.empty())
	{
		LL_WARNS("WindlightCaps") << "Applying windlight settings not supported" << LL_ENDL;
		return false;
	}

	LL_INFOS("WindlightCaps") << "Sending windlight settings to " << url << LL_ENDL;
	LL_DEBUGS("WindlightCaps") << "content: " << content << LL_ENDL;
	LLHTTPClient::post(url, content, new LLEnvironmentApplyResponder());
	return true;
}

/****
 * LLEnvironmentApplyResponder
 ****/
/*virtual*/ void LLEnvironmentApplyResponder::httpSuccess(void)
{
	if (mContent["regionID"].asUUID() != gAgent.getRegion()->getRegionID())
	{
		LL_WARNS("WindlightCaps") << "No longer in the region where data was sent (currently "
			<< gAgent.getRegion()->getRegionID() << ", reply is from " << mContent["regionID"].asUUID()
			<< "); ignoring..." << LL_ENDL;
		return;
	}
	else if (mContent["success"].asBoolean())
	{
		LL_DEBUGS("WindlightCaps") << "Success in applying windlight settings to region " << mContent["regionID"].asUUID() << LL_ENDL;
		LLEnvManagerNew::instance().onRegionSettingsApplyResponse(true);
	}
	else
	{
		LL_WARNS("WindlightCaps") << "Region couldn't apply windlight settings!  Reason from sim: " << mContent["fail_reason"].asString() << LL_ENDL;
		LLSD args(LLSD::emptyMap());
		args["FAIL_REASON"] = mContent["fail_reason"].asString();
		LLNotificationsUtil::add("WLRegionApplyFail", args);
		LLEnvManagerNew::instance().onRegionSettingsApplyResponse(false);
	}
}
/*virtual*/ void LLEnvironmentApplyResponder::httpFailure(void)
{
	std::stringstream msg;
	msg << mReason << " (Code " << mStatus << ")";

	LL_WARNS("WindlightCaps") << "Couldn't apply windlight settings to region!  Reason: " << msg.str() << LL_ENDL;

	LLSD args(LLSD::emptyMap());
	args["FAIL_REASON"] = msg.str();
	LLNotificationsUtil::add("WLRegionApplyFail", args);
}
