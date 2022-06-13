/**
 * @file llfloatersearch.cpp
 * @author Martin Reddy
 * @brief Search floater - uses an embedded web browser control
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llcommandhandler.h"
#include "llfloatersearch.h"
#include "llfloaterdirectory.h"
#include "llmediactrl.h"
#include "llnotificationsutil.h"
#include "lluserauth.h"
#include "lluri.h"
#include "llagent.h"
#include "llui.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llweb.h"

void toggle_search_floater()
{
	if (!gSavedSettings.getString("SearchURL").empty() && gSavedSettings.getBOOL("UseWebSearch"))
	{
		if (LLFloaterSearch::instanceExists() && LLFloaterSearch::instance().getVisible())
			LLFloaterSearch::instance().close();
		else
			LLFloaterSearch::getInstance()->open();
	}
	else
	{
		LLFloaterDirectory::toggleFind(0);
	}
}

// support secondlife:///app/search/{CATEGORY}/{QUERY} SLapps
class LLSearchHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLSearchHandler() : LLCommandHandler("search", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& tokens, const LLSD& query_map, LLMediaCtrl* web)
	{
		/*if (!LLUI::sSettingGroups["config"]->getBOOL("EnableSearch"))
		{
			LLNotificationsUtil::add("NoSearch", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}*/

		const size_t parts = tokens.size();

		// get the (optional) category for the search
		std::string category;
		if (parts > 0)
		{
			category = tokens[0].asString();
		}

		// get the (optional) search string
		std::string search_text;
		if (parts > 1)
		{
			search_text = tokens[1].asString();
		}

		// create the LLSD arguments for the search floater
		LLFloaterSearch::Params p;
		p.search.category = category;
		p.search.query = LLURI::unescape(search_text);

		// open the search floater and perform the requested search
		//LLFloaterReg::showInstance("search", p);
		LLFloaterSearch::showInstance(p.search, gSavedSettings.getBOOL("UseWebSearchSLURL"));
		return true;
	}
};
LLSearchHandler gSearchHandler;

LLFloaterSearch::SearchQuery::SearchQuery()
:	category("category", ""),
	query("query")
{}

// Singu Note: We use changeDefault instead of setting these in onOpen
LLFloaterSearch::_Params::_Params()
{
	changeDefault(trusted_content, true);
	changeDefault(allow_address_entry, false);
	changeDefault(window_class, "search"); // Don't include this in the count with "web_content"
	changeDefault(id, "search"); // Don't include this in the count with "web_content"
}

LLFloaterSearch::LLFloaterSearch(const Params& key) :
	LLFloaterWebContent(key)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_web_content.xml");
}

BOOL LLFloaterSearch::postBuild()
{
	LLFloaterWebContent::postBuild();
	mWebBrowser->addObserver(this);
	// Singu Note: Initialize ui and browser now
	mWebBrowser->setTrustedContent(true);
	mWebBrowser->setFocus(true);
	getChild<LLPanel>("status_bar")->setVisible(true);
	getChild<LLPanel>("nav_controls")->setVisible(true);
	getChildView("address")->setEnabled(false);
	getChildView("popexternal")->setEnabled(false);
	setRectControl("FloaterSearchRect");
	applyRectControl();
	search(SearchQuery(), mWebBrowser);
	gSavedSettings.getControl("SearchURL")->getSignal()->connect(boost::bind(LLFloaterSearch::search, SearchQuery(), mWebBrowser));

	return TRUE;
}

//static
void LLFloaterSearch::showInstance(const SearchQuery& search, bool web)
{
	if (!gSavedSettings.getString("SearchURL").empty() && (web || gSavedSettings.getBOOL("UseWebSearch")))
	{
		LLFloaterSearch* floater = getInstance();
		floater->open(); // May not be open
		floater->search(search, floater->mWebBrowser);
	}
	else
	{
		LLFloaterDirectory::search(search);
	}
}

/*void LLFloaterSearch::onOpen(const LLSD& key)
{
	Params p(key);
	p.trusted_content = true;
	p.allow_address_entry = false;

	LLFloaterWebContent::onOpen(p);
	search(p.search, mWebBrowser);
}*/

void LLFloaterSearch::onClose(bool app_quitting)
{
	/*if (!app_quitting) // Singu Note: Copy the behavior of the legacy search singleton retaining last search when reopened
	{
		setVisible(false);
		return;
	}*/
	LLFloaterWebContent::onClose(app_quitting);
	// tear down the web view so we don't show the previous search
	// result when the floater is opened next time
	destroy();
}

// static
void LLFloaterSearch::search(const SearchQuery &p, LLMediaCtrl* mWebBrowser)
{
	if (! mWebBrowser || !p.validateBlock())
	{
		return;
	}

	// work out the subdir to use based on the requested category
	LLSD subs;
	// declare a map that transforms a category name into
	// the URL suffix that is used to search that category
	static LLSD mCategoryPaths = LLSD::emptyMap();
	if (mCategoryPaths.size() == 0)
	{
		mCategoryPaths["all"]          = "search";
		mCategoryPaths["people"]       = "search/people";
		mCategoryPaths["places"]       = "search/places";
		mCategoryPaths["events"]       = "search/events";
		mCategoryPaths["groups"]       = "search/groups";
		mCategoryPaths["wiki"]         = "search/wiki";
		mCategoryPaths["destinations"] = "destinations";
		mCategoryPaths["classifieds"]  = "classifieds";
	}
	if (mCategoryPaths.has(p.category))
	{
		subs["CATEGORY"] = mCategoryPaths[p.category].asString();
	}
	else
	{
		subs["CATEGORY"] = mCategoryPaths["all"].asString();
	}

	// add the search query string
	subs["QUERY"] = LLURI::escape(p.query);

	// add the permissions token that login.cgi gave us
	// We use "search_token", and fallback to "auth_token" if not present.
	LLSD search_token = LLUserAuth::getInstance()->getResponse("search_token");
	if (search_token.asString().empty())
	{
		search_token = LLUserAuth::getInstance()->getResponse("auth_token");
	}
	subs["AUTH_TOKEN"] = search_token.asString();

	// add the user's preferred maturity (can be changed via prefs)
	std::string maturity;
	if (gAgent.prefersAdult())
	{
		maturity = "42";  // PG,Mature,Adult
	}
	else if (gAgent.prefersMature())
	{
		maturity = "21";  // PG,Mature
	}
	else
	{
		maturity = "13";  // PG
	}
	subs["MATURITY"] = maturity;

	// add the user's god status
	subs["GODLIKE"] = gAgent.isGodlike() ? "1" : "0";

	// get the search URL and expand all of the substitutions
	// (also adds things like [LANGUAGE], [VERSION], [OS], etc.)
	std::string url = gSavedSettings.getString("SearchURL");
	url = LLWeb::expandURLSubstitutions(url, subs);

	// and load the URL in the web view
	mWebBrowser->navigateTo(url, "text/html");
}
