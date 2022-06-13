/**
 * @file llfloatersearch.h
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

#ifndef LL_LLFLOATERSEARCH_H
#define LL_LLFLOATERSEARCH_H

#include "llfloaterwebcontent.h"
#include "llviewermediaobserver.h"

#include <string>

class LLMediaCtrl;

///
/// The search floater allows users to perform all search operations.
/// All search functionality is now implemented via web services and
/// so this floater simply embeds a web view and displays the search
/// web page. The browser control is explicitly marked as "trusted"
/// so that the user can click on teleport links in search results.
///
class LLFloaterSearch :
	public LLFloaterWebContent, public LLSingleton<LLFloaterSearch>
{
public:
	struct SearchQuery : public LLInitParam::Block<SearchQuery>
	{
		Optional<std::string> category;
		Optional<std::string> query;

		SearchQuery();
	};

	struct _Params : public LLInitParam::Block<_Params, LLFloaterWebContent::Params>
	{
		Optional<SearchQuery> search;
		_Params(); // Singu Note: We use changeDefault instead of setting these in onOpen
	};

	typedef LLSDParamAdapter<_Params> Params;

	LLFloaterSearch(const Params& key = Params());

	using LLSingleton<LLFloaterSearch>::getInstance;
	static void showInstance(const SearchQuery& p, bool web = false);

	/// show the search floater with a new search
	/// see search() for details on the key parameter.
	/// Singu Note: Leave the browser (and position) alone, here, we'll call search() directly with a SearchQuery if opening the floater through showInstance()
	/*virtual*/ void onOpen(/*const LLSD& key*/) {}

	/*virtual*/ void onClose(bool app_quitting);

	/// perform a search with the specific search term.
	/// The key should be a map that can contain the following keys:
	///  - "id": specifies the text phrase to search for
	///  - "category": one of "all" (default), "people", "places",
	///    "events", "groups", "wiki", "destinations", "classifieds"
	static void search(const SearchQuery& query, LLMediaCtrl* mWebBrowser);

private:
	/*virtual*/ BOOL postBuild();
};

#endif  // LL_LLFLOATERSEARCH_H

