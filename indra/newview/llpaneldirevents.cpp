/** 
 * @file llpaneldirevents.cpp
 * @brief Events listing in the Find directory.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llpaneldirevents.h"

#include <sstream>

// linden library includes
#include "message.h"
#include "llqueryflags.h"

// viewer project includes
#include "llagent.h"
#include "llviewercontrol.h"
#include "lleventinfo.h"
#include "llpaneldirbrowser.h"
#include "llresmgr.h"
#include "lluiconstants.h"
#include "llpanelevent.h"
#include "llappviewer.h"
#include "llnotificationsutil.h"
#include "llviewerregion.h"

LLPanelDirEvents::LLPanelDirEvents(const std::string& name, LLFloaterDirectory* floater)
	:	LLPanelDirBrowser(name, floater),
	mDoneQuery(FALSE),
	mDay(0)
{
	// more results per page for this
	mResultsPerPage = 200;
}

BOOL LLPanelDirEvents::postBuild()
{
	LLPanelDirBrowser::postBuild();

	getChild<LLUICtrl>("date_mode")->setCommitCallback(boost::bind(&LLPanelDirEvents::onDateModeCallback,this));

	getChild<LLButton>("<<")->setClickedCallback(boost::bind(&LLPanelDirEvents::onBackBtn,this));
	getChild<LLButton>(">>")->setClickedCallback(boost::bind(&LLPanelDirEvents::onForwardBtn,this));

	getChild<LLButton>("Today")->setClickedCallback(boost::bind(&LLPanelDirEvents::onClickToday,this));

	getChild<LLButton>("Search")->setClickedCallback(boost::bind(&LLPanelDirEvents::onClickSearchCore,this));
	setDefaultBtn("Search");

	getChild<LLButton>("Delete")->setClickedCallback(boost::bind(&LLPanelDirEvents::onClickDelete,this));
	childDisable("Delete");
	childHide("Delete");

	onDateModeCallback();

	mCurrentSortColumn = "time";

	setDay(0);	// for today

	LLViewerRegion* region(gAgent.getRegion());
	getChildView("filter_gaming")->setVisible(region && (gAgent.getRegion()->getGamingFlags() & REGION_GAMING_PRESENT) && !(gAgent.getRegion()->getGamingFlags() & REGION_GAMING_HIDE_FIND_EVENTS));

	return TRUE;
}

LLPanelDirEvents::~LLPanelDirEvents()
{
	// Children all cleaned up by default view destructor.
}


void LLPanelDirEvents::draw()
{
	refresh();

	LLPanelDirBrowser::draw();
}

void LLPanelDirEvents::refresh()
{
	BOOL godlike = gAgent.isGodlike();
	childSetVisible("Delete", godlike);
	childSetEnabled("Delete", godlike);

	updateMaturityCheckbox();
}


void LLPanelDirEvents::setDay(S32 day)
{
	mDay = day;

	// Get time UTC
	time_t utc_time = time_corrected();

	// Correct for offset
	utc_time += day * 24 * 60 * 60;

	// There's only one internal tm buffer.
	struct tm* internal_time;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	internal_time = utc_to_pacific_time(utc_time, gPacificDaylightTime);
	std::string date;
	timeStructToFormattedString(internal_time, "%m-%d", date);
	childSetValue("date_text", date);
}

// virtual
void LLPanelDirEvents::performQuery()
{
	// event_id 0 will perform no delete action.
	performQueryOrDelete(0);
}

void LLPanelDirEvents::performQueryOrDelete(U32 event_id)
{
	S32 relative_day = mDay;
	// Update the date field to show the date IN THE SERVER'S
	// TIME ZONE, as that is what will be displayed in each event

	// Get time UTC
	time_t utc_time = time_corrected();

	// Correct for offset
	utc_time += relative_day * 24 * 60 * 60;

	// There's only one internal tm buffer.
	struct tm* internal_time;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	internal_time = utc_to_pacific_time(utc_time, gPacificDaylightTime);
	std::string date;
	timeStructToFormattedString(internal_time, "%m-%d", date);
	childSetValue("date_text", date);

	// Record the relative day so back and forward buttons
	// offset from this day.
	mDay = relative_day;

	mDoneQuery = TRUE;

	U32 scope = DFQ_DATE_EVENTS;
	if ( gAgent.wantsPGOnly()) scope |= DFQ_PG_SIMS_ONLY;
	if ( childGetValue("incpg").asBoolean() ) scope |= DFQ_INC_PG;
	if ( childGetValue("incmature").asBoolean() ) scope |= DFQ_INC_MATURE;
	if ( childGetValue("incadult").asBoolean() ) scope |= DFQ_INC_ADULT;
	if (childGetValue("filter_gaming").asBoolean()) scope |= DFQ_FILTER_GAMING;
	
	// Add old query flags in case we are talking to an old server
	if ( childGetValue("incpg").asBoolean() && !childGetValue("incmature").asBoolean())
	{
		scope |= DFQ_PG_EVENTS_ONLY;
	}
	
	if ( !( scope & (DFQ_INC_PG | DFQ_INC_MATURE | DFQ_INC_ADULT )))
	{
		LLNotificationsUtil::add("NoContentToSearch");
		return;
	}
	
	setupNewSearch();

	std::ostringstream params;

	// Date mode for the search
	if ("current" == childGetValue("date_mode").asString())
	{
		params << "u|";
	}
	else
	{
		params << mDay << "|";
	}

	// Categories are stored in the database in table indra.event_category
	// XML must match.
	U32 cat_id = childGetValue("category combo").asInteger();

	params << cat_id << "|";
	params << childGetValue("name").asString();

	// send the message
	if (0 == event_id)
	{
		
		sendDirFindQuery(gMessageSystem, mSearchID, params.str(), scope, mSearchStart);
	}
	else
	{
		// This delete will also perform a query.
		LLMessageSystem* msg = gMessageSystem;

		msg->newMessageFast(_PREHASH_EventGodDelete);

		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

		msg->nextBlockFast(_PREHASH_EventData);
		msg->addU32Fast(_PREHASH_EventID, event_id);

		msg->nextBlockFast(_PREHASH_QueryData);
		msg->addUUIDFast(_PREHASH_QueryID, mSearchID);
		msg->addStringFast(_PREHASH_QueryText, params.str());
		msg->addU32Fast(_PREHASH_QueryFlags, scope);
		msg->addS32Fast(_PREHASH_QueryStart, mSearchStart);
		gAgent.sendReliableMessage();
	}
}

void LLPanelDirEvents::onDateModeCallback()
{
	if (childGetValue("date_mode").asString() == "date")
	{
		childEnable("Today");
		childEnable(">>");
		childEnable("<<");
	}
	else
	{
		childDisable("Today");
		childDisable(">>");
		childDisable("<<");
	}
}

void LLPanelDirEvents::onClickToday()
{
	resetSearchStart();
	setDay(0);
	performQuery();
}

void LLPanelDirEvents::onBackBtn()
{
	resetSearchStart();
	setDay(mDay - 1);
	performQuery();
}

void LLPanelDirEvents::onForwardBtn()
{
	resetSearchStart();
	setDay(mDay + 1);
	performQuery();
}

void LLPanelDirEvents::onClickDelete()
{
	U32 event_id;
	event_id = getSelectedEventID();
	if (!event_id) return;

	performQueryOrDelete(event_id);
}
