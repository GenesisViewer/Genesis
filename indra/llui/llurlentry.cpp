/** 
 * @file llurlentry.cpp
 * @author Martin Reddy
 * @brief Describes the Url types that can be registered in LLUrlRegistry
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

#include "linden_common.h"
#include "llurlentry.h"
#include "lluictrl.h"
#include "lluri.h"
#include "llurlmatch.h"
#include "llurlregistry.h"
#include "lluriparser.h"

#include "llavatarnamecache.h"
#include "llcachename.h"
#include "lltrans.h"
//#include "lluicolortable.h"
#include "message.h"
#include "llexperiencecache.h"

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp> // <alchemy/>

#define APP_HEADER_REGEX "((((x-grid-info://)|(x-grid-location-info://))[-\\w\\.]+(:\\d+)?/app)|(secondlife:///app))"
#define X_GRID_OR_SECONDLIFE_HEADER_REGEX "((((x-grid-info://)|(x-grid-location-info://))[-\\w\\.]+(:\\d+)?/)|(secondlife://))"

// Utility functions
std::string localize_slapp_label(const std::string& url, const std::string& full_name);


LLUrlEntryBase::LLUrlEntryBase()
{
}

LLUrlEntryBase::~LLUrlEntryBase()
{
}

std::string LLUrlEntryBase::getUrl(const std::string &string) const
{
	return escapeUrl(string);
}

//virtual
std::string LLUrlEntryBase::getIcon(const std::string &url)
{
	return mIcon;
}

LLStyleSP LLUrlEntryBase::getStyle() const
{
	static const LLUICachedControl<LLColor4> color("HTMLLinkColor");
	LLStyleSP style_params(new LLStyle(true, color, LLStringUtil::null));
	//style_params->mUnderline = true; // Singu Note: We're not gonna bother here, underlining on hover
	return style_params;
}


std::string LLUrlEntryBase::getIDStringFromUrl(const std::string &url) const
{
	// return the id from a SLURL in the format /app/{cmd}/{id}/about
	LLURI uri(url);
	LLSD path_array = uri.pathArray();
	if (path_array.size() == 4) 
	{
		return path_array.get(2).asString();
	}
	return "";
}

std::string LLUrlEntryBase::unescapeUrl(const std::string &url) const
{
	return LLURI::unescape(url);
}

std::string LLUrlEntryBase::escapeUrl(const std::string &url) const
{
	static std::string no_escape_chars;
	static bool initialized = false;
	if (!initialized)
	{
		no_escape_chars = 
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz"
			"0123456789"
			"-._~!$?&()*+,@:;=/%#";

		std::sort(no_escape_chars.begin(), no_escape_chars.end());
		initialized = true;
	}
	return LLURI::escape(url, no_escape_chars, true);
}

std::string LLUrlEntryBase::getLabelFromWikiLink(const std::string &url) const
{
	// return the label part from [http://www.example.org Label]
	const char *text = url.c_str();
	size_t start = 0;
	while (! isspace(text[start]))
	{
		++start;
	}
	while (text[start] == ' ' || text[start] == '\t')
	{
		++start;
	}
	return unescapeUrl(url.substr(start, url.size()-start-1));
}

std::string LLUrlEntryBase::getUrlFromWikiLink(const std::string &string) const
{
	// return the url part from [http://www.example.org Label]
	const char *text = string.c_str();
	size_t end = 0;
	while (! isspace(text[end]))
	{
		++end;
	}
	return escapeUrl(string.substr(1, end-1));
}

void LLUrlEntryBase::addObserver(const std::string &id,
								 const std::string &url,
								 const LLUrlLabelCallback &cb)
{
	// add a callback to be notified when we have a label for the uuid
	LLUrlEntryObserver observer;
	observer.url = url;
	try
	{
		observer.signal = new LLUrlLabelSignal();
		observer.signal->connect(cb);
		mObservers.emplace(id, observer);
	}
	catch (...)
	{
	}
}

// *NOTE: See also LLUrlEntryAgent::callObservers()
void LLUrlEntryBase::callObservers(const std::string &id,
								   const std::string &label,
								   const std::string &icon)
{
	// notify all callbacks waiting on the given uuid
	typedef std::multimap<std::string, LLUrlEntryObserver>::iterator observer_it;
	std::pair<observer_it, observer_it> matching_range = mObservers.equal_range(id);
	for (observer_it it = matching_range.first; it != matching_range.second;)
	{
		// call the callback - give it the new label
		LLUrlEntryObserver &observer = it->second;
		(*observer.signal)(it->second.url, label, icon);
		// then remove the signal - we only need to call it once
		delete observer.signal;
		mObservers.erase(it++);
	}
}

/// is this a match for a URL that should not be hyperlinked?
bool LLUrlEntryBase::isLinkDisabled() const
{
	// this allows us to have a global setting to turn off text hyperlink highlighting/action
	bool globally_disabled = LLUI::sConfigGroup->getBOOL("DisableTextHyperlinkActions");

	return globally_disabled;
}

bool LLUrlEntryBase::isWikiLinkCorrect(const std::string& url)
{
	LLWString label = utf8str_to_wstring(getLabelFromWikiLink(url));
	label.erase(std::remove(label.begin(), label.end(), L'\u200B'), label.end());
	return (LLUrlRegistry::instance().hasUrl(wstring_to_utf8str(label))) ? false : true;
}

std::string LLUrlEntryBase::urlToLabelWithGreyQuery(const std::string &url) const
{
	LLUriParser up(unescapeUrl(url));
	up.normalize();

	std::string label;
	up.extractParts();
	up.glueFirst(label);

	return label;
}

std::string LLUrlEntryBase::urlToGreyQuery(const std::string &url) const
{
	LLUriParser up(unescapeUrl(url));

	std::string label;
	up.extractParts();
	up.glueFirst(label, false);

	size_t pos = url.find(label);
	if (pos == std::string::npos)
	{
		return "";
	}
	pos += label.size();
	return url.substr(pos);
}


static std::string getStringAfterToken(const std::string& str, const std::string& token)
{
	size_t pos = str.find(token);
	if (pos == std::string::npos)
	{
		return LLStringUtil::null;
	}

	pos += token.size();
	return str.substr(pos, str.size() - pos);
}

//
// LLUrlEntryHTTP Describes generic http: and https: Urls
//
LLUrlEntryHTTP::LLUrlEntryHTTP()
	: LLUrlEntryBase()
{
	mPattern = boost::regex("https?://([^\\s/?\\.#]+\\.?)+\\.\\w+(:\\d+)?(/\\S*)?",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

std::string LLUrlEntryHTTP::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return unescapeUrl(url);
}

std::string LLUrlEntryHTTP::getUrl(const std::string &string) const
{
	if (string.find("://") == std::string::npos)
	{
		return "http://" + escapeUrl(string);
	}
	return escapeUrl(string);
}

std::string LLUrlEntryHTTP::getTooltip(const std::string &url) const
{
	return mTooltip;;
}

//
// LLUrlEntryHTTP Describes generic http: and https: Urls with custom label
// We use the wikipedia syntax of [http://www.example.org Text]
//
LLUrlEntryHTTPLabel::LLUrlEntryHTTPLabel()
{
	mPattern = boost::regex("\\[(https?://\\S+|\\S+\\.([^\\s<]*)?)[ \t]+[^\\]]+\\]",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

std::string LLUrlEntryHTTPLabel::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	std::string label = getLabelFromWikiLink(url);
	return (!LLUrlRegistry::instance().hasUrl(label)) ? label : getUrl(url);
}

std::string LLUrlEntryHTTPLabel::getTooltip(const std::string &string) const
{
	return getUrl(string);
}

std::string LLUrlEntryHTTPLabel::getUrl(const std::string &string) const
{
	auto url = getUrlFromWikiLink(string);
	if (!boost::istarts_with(url, "http"))
		return "http://" + url;
	return url;
}

//
// LLUrlEntryHTTPNoProtocol Describes generic Urls like www.google.com
//
LLUrlEntryHTTPNoProtocol::LLUrlEntryHTTPNoProtocol()
	: LLUrlEntryBase()
{
	mPattern = boost::regex("\\bwww\\.\\S+\\.([^\\s<]*)?\\b", // i.e. www.FOO.BAR
		boost::regex::perl | boost::regex::icase);
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

std::string LLUrlEntryHTTPNoProtocol::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return urlToLabelWithGreyQuery(url);
}

std::string LLUrlEntryHTTPNoProtocol::getQuery(const std::string &url) const
{
	return urlToGreyQuery(url);
}

std::string LLUrlEntryHTTPNoProtocol::getUrl(const std::string &string) const
{
	if (string.find("://") == std::string::npos)
	{
		return "http://" + escapeUrl(string);
	}
	return escapeUrl(string);
}

std::string LLUrlEntryHTTPNoProtocol::getTooltip(const std::string &url) const
{
	return unescapeUrl(url);
}

LLUrlEntryInvalidSLURL::LLUrlEntryInvalidSLURL()
	: LLUrlEntryBase()
{
	mPattern = boost::regex("(https?://(maps.secondlife.com|slurl.com)/secondlife/|secondlife://(/app/(worldmap|teleport)/)?)[^ /]+(/-?[0-9]+){1,3}(/?(\\?\\S*)?)?",
									boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

std::string LLUrlEntryInvalidSLURL::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{

	return escapeUrl(url);
}

std::string LLUrlEntryInvalidSLURL::getUrl(const std::string &string) const
{
	return escapeUrl(string);
}

std::string LLUrlEntryInvalidSLURL::getTooltip(const std::string &url) const
{
	return unescapeUrl(url);
}

bool LLUrlEntryInvalidSLURL::isSLURLvalid(const std::string &url) const
{
	S32 actual_parts;

	if(url.find(".com/secondlife/") != std::string::npos)
	{
	   actual_parts = 5;
	}
	else if(url.find("/app/") != std::string::npos)
	{
		actual_parts = 6;
	}
	else
	{
		actual_parts = 3;
	}

	LLURI uri(url);
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();
	S32 x,y,z;

	if (path_parts == actual_parts)
	{
		// handle slurl with (X,Y,Z) coordinates
		LLStringUtil::convertToS32(path_array[path_parts-3].asString(),x);
		LLStringUtil::convertToS32(path_array[path_parts-2].asString(),y);
		LLStringUtil::convertToS32(path_array[path_parts-1].asString(),z);

		if((x>= 0 && x<= 256) && (y>= 0 && y<= 256) && (z>= 0))
		{
			return TRUE;
		}
	}
	else if (path_parts == (actual_parts-1))
	{
		// handle slurl with (X,Y) coordinates

		LLStringUtil::convertToS32(path_array[path_parts-2].asString(),x);
		LLStringUtil::convertToS32(path_array[path_parts-1].asString(),y);
		;
		if((x>= 0 && x<= 256) && (y>= 0 && y<= 256))
		{
				return TRUE;
		}
	}
	else if (path_parts == (actual_parts-2))
	{
		// handle slurl with (X) coordinate
		LLStringUtil::convertToS32(path_array[path_parts-1].asString(),x);
		if(x>= 0 && x<= 256)
		{
			return TRUE;
		}
	}

	return FALSE;
}

//
// LLUrlEntrySLURL Describes generic http: and https: Urls
//
LLUrlEntrySLURL::LLUrlEntrySLURL()
{
	// see http://slurl.com/about.php for details on the SLURL format
	mPattern = boost::regex("https?://(maps.secondlife.com|slurl.com)/secondlife/[^ /]+(/\\d+){0,3}(/?(\\?\\S*)?)?",
							boost::regex::perl|boost::regex::icase);
	mIcon = "Hand";
	mMenuName = "menu_url_slurl.xml";
	mTooltip = LLTrans::getString("TooltipSLURL");
}

std::string LLUrlEntrySLURL::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	//
	// we handle SLURLs in the following formats:
	//   - http://slurl.com/secondlife/Place/X/Y/Z
	//   - http://slurl.com/secondlife/Place/X/Y
	//   - http://slurl.com/secondlife/Place/X
	//   - http://slurl.com/secondlife/Place
	//

	LLURI uri(url);
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();
	if (path_parts == 5)
	{
		// handle slurl with (X,Y,Z) coordinates
		std::string location = unescapeUrl(path_array[path_parts-4]);
		std::string x = path_array[path_parts-3];
		std::string y = path_array[path_parts-2];
		std::string z = path_array[path_parts-1];
		return location + " (" + x + "," + y + "," + z + ")";
	}
	else if (path_parts == 4)
	{
		// handle slurl with (X,Y) coordinates
		std::string location = unescapeUrl(path_array[path_parts-3]);
		std::string x = path_array[path_parts-2];
		std::string y = path_array[path_parts-1];
		return location + " (" + x + "," + y + ")";
	}
	else if (path_parts == 3)
	{
		// handle slurl with (X) coordinate
		std::string location = unescapeUrl(path_array[path_parts-2]);
		std::string x = path_array[path_parts-1];
		return location + " (" + x + ")";
	}
	else if (path_parts == 2)
	{
		// handle slurl with no coordinates
		std::string location = unescapeUrl(path_array[path_parts-1]);
		return location;
	}

	return url;
}

std::string LLUrlEntrySLURL::getLocation(const std::string &url) const
{
	// return the part of the Url after slurl.com/secondlife/
	const std::string search_string = "/secondlife";
	size_t pos = url.find(search_string);
	if (pos == std::string::npos)
	{
		return "";
	}

	pos += search_string.size() + 1;
	return url.substr(pos, url.size() - pos);
}

//
// LLUrlEntrySeconlifeURL Describes *secondlife.com/ and *lindenlab.com/ urls to substitute icon 'hand.png' before link
//
LLUrlEntrySecondlifeURL::LLUrlEntrySecondlifeURL()
{ 
	mPattern = boost::regex("\\b(https?://)?([-\\w\\.]*\\.)?(secondlife|lindenlab)\\.com(:\\d{1,5})?(/\\S*)?",
		boost::regex::perl|boost::regex::icase);
	
	mIcon = "Hand";
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

/// Return the url from a string that matched the regex
std::string LLUrlEntrySecondlifeURL::getUrl(const std::string &string) const
{
	if (string.find("://") == std::string::npos)
	{
		return "https://" + escapeUrl(string);
	}
	return escapeUrl(string);
}

std::string LLUrlEntrySecondlifeURL::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return urlToLabelWithGreyQuery(url);
}

std::string LLUrlEntrySecondlifeURL::getQuery(const std::string &url) const
{
	return urlToGreyQuery(url);
}

std::string LLUrlEntrySecondlifeURL::getTooltip(const std::string &url) const
{
	return url;
}

//
// LLUrlEntrySimpleSecondlifeURL Describes *secondlife.com and *lindenlab.com urls to substitute icon 'hand.png' before link
//
LLUrlEntrySimpleSecondlifeURL::LLUrlEntrySimpleSecondlifeURL()
  {
	mPattern = boost::regex("(https?://)?([-\\w\\.]*\\.)?(secondlife|lindenlab)\\.com(?!\\S)",
		boost::regex::perl|boost::regex::icase);

	mIcon = "Hand";
	mMenuName = "menu_url_http.xml";
}

//
// LLUrlEntryAgent Describes a Second Life agent Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/about
//
LLUrlEntryAgent::LLUrlEntryAgent()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/agent/[\\da-f-]+/\\w+",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_agent.xml";
	mIcon = "Generic_Person";
}

// virtual
void LLUrlEntryAgent::callObservers(const std::string &id,
								    const std::string &label,
								    const std::string &icon)
{
	// notify all callbacks waiting on the given uuid
	typedef std::multimap<std::string, LLUrlEntryObserver>::iterator observer_it;
	std::pair<observer_it, observer_it> matching_range = mObservers.equal_range(id);
	for (observer_it it = matching_range.first; it != matching_range.second;)
	{
		// call the callback - give it the new label
		LLUrlEntryObserver &observer = it->second;
		std::string final_label = localize_slapp_label(observer.url, label);
		(*observer.signal)(observer.url, final_label, icon);
		// then remove the signal - we only need to call it once
		delete observer.signal;
		mObservers.erase(it++);
	}
}

void LLUrlEntryAgent::onAvatarNameCache(const LLUUID& id,
										const LLAvatarName& av_name)
{
	auto range = mAvatarNameCacheConnections.equal_range(id);
	for (auto it = range.first; it != range.second; ++it)
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
	}
	mAvatarNameCacheConnections.erase(range.first, range.second);

	std::string label = av_name.getNSName();

	// received the agent name from the server - tell our observers
	callObservers(id.asString(), label, mIcon);
}

LLUUID	LLUrlEntryAgent::getID(const std::string &string) const
{
	return LLUUID(getIDStringFromUrl(string));
}

std::string LLUrlEntryAgent::getTooltip(const std::string &string) const
{
	// return a tooltip corresponding to the URL type instead of the generic one
	std::string url = getUrl(string);

	if (LLStringUtil::endsWith(url, "/inspect"))
	{
		return LLTrans::getString("TooltipAgentInspect");
	}
	if (LLStringUtil::endsWith(url, "/mute"))
	{
		return LLTrans::getString("TooltipAgentMute");
	}
	if (LLStringUtil::endsWith(url, "/unmute"))
	{
		return LLTrans::getString("TooltipAgentUnmute");
	}
	if (LLStringUtil::endsWith(url, "/im"))
	{
		return LLTrans::getString("TooltipAgentIM");
	}
	if (LLStringUtil::endsWith(url, "/pay"))
	{
		return LLTrans::getString("TooltipAgentPay");
	}
	if (LLStringUtil::endsWith(url, "/offerteleport"))
	{
		return LLTrans::getString("TooltipAgentOfferTeleport");
	}
	if (LLStringUtil::endsWith(url, "/requestfriend"))
	{
		return LLTrans::getString("TooltipAgentRequestFriend");
	}
	return LLTrans::getString("TooltipAgentUrl");
}

bool LLUrlEntryAgent::underlineOnHoverOnly(const std::string &string) const
{
	std::string url = getUrl(string);
	return LLStringUtil::endsWith(url, "/about") || LLStringUtil::endsWith(url, "/inspect");
}

std::string LLUrlEntryAgent::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	static std::string name_wait_str = LLTrans::getString("LoadingData");
	if (!gCacheName)
	{
		// probably at the login screen, use short string for layout
		return name_wait_str;
	}

	std::string agent_id_string = getIDStringFromUrl(url);
	if (agent_id_string.empty())
	{
		// something went wrong, just give raw url
		return unescapeUrl(url);
	}

	LLUUID agent_id(agent_id_string);
	if (agent_id.isNull())
	{
		return LLTrans::getString("AvatarNameNobody");
	}

	LLAvatarName av_name;
	if (LLAvatarNameCache::get(agent_id, &av_name))
	{
		std::string label = av_name.getNSName();

		// handle suffixes like /mute or /offerteleport
		label = localize_slapp_label(url, label);
		return label;
	}
	else
	{
		auto connection = LLAvatarNameCache::get(agent_id, boost::bind(&LLUrlEntryAgent::onAvatarNameCache, this, _1, _2));
		mAvatarNameCacheConnections.emplace(agent_id, connection);
		addObserver(agent_id_string, url, cb);
		return name_wait_str;
	}
}

LLStyleSP LLUrlEntryAgent::getStyle() const
{
	static const LLUICachedControl<LLColor4> color("HTMLAgentColor");
	LLStyleSP style_params(new LLStyle(true, color, LLStringUtil::null));
	return style_params;
}

std::string localize_slapp_label(const std::string& url, const std::string& full_name)
{
	// customize label string based on agent SLapp suffix
	if (LLStringUtil::endsWith(url, "/mute"))
	{
		return LLTrans::getString("SLappAgentMute") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/unmute"))
	{
		return LLTrans::getString("SLappAgentUnmute") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/im"))
	{
		return LLTrans::getString("SLappAgentIM") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/pay"))
	{
		return LLTrans::getString("SLappAgentPay") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/offerteleport"))
	{
		return LLTrans::getString("SLappAgentOfferTeleport") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/requestfriend"))
	{
		return LLTrans::getString("SLappAgentRequestFriend") + " " + full_name;
	}
	if (LLStringUtil::endsWith(url, "/removefriend"))
	{
		return LLTrans::getString("SLappAgentRemoveFriend") + " " + full_name;
	}
	return full_name;
}


std::string LLUrlEntryAgent::getIcon(const std::string &url)
{
	// *NOTE: Could look up a badge here by calling getIDStringFromUrl()
	// and looking up the badge for the agent.
	return mIcon;
}

//
// LLUrlEntryAgentName describes a Second Life agent name Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/(completename|displayname|username)
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/(completename|displayname|username)
//
LLUrlEntryAgentName::LLUrlEntryAgentName()
{
	mMenuName = "menu_url_agent.xml";
}

void LLUrlEntryAgentName::onAvatarNameCache(const LLUUID& id,
										const LLAvatarName& av_name)
{
	auto range = mAvatarNameCacheConnections.equal_range(id);
	for (auto it = range.first; it != range.second; ++it)
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
	}
	mAvatarNameCacheConnections.erase(range.first, range.second);

	std::string label = getName(av_name);
	// received the agent name from the server - tell our observers
	callObservers(id.asString(), label, mIcon);
}

std::string LLUrlEntryAgentName::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	static std::string name_wait_str = LLTrans::getString("LoadingData");
	if (!gCacheName)
	{
		// probably at the login screen, use short string for layout
		return name_wait_str;
	}

	std::string agent_id_string = getIDStringFromUrl(url);
	if (agent_id_string.empty())
	{
		// something went wrong, just give raw url
		return unescapeUrl(url);
	}

	LLUUID agent_id(agent_id_string);
	if (agent_id.isNull())
	{
		return LLTrans::getString("AvatarNameNobody");
	}

	LLAvatarName av_name;
	if (LLAvatarNameCache::get(agent_id, &av_name))
	{
		return getName(av_name);
	}
	else
	{
		auto connection = LLAvatarNameCache::get(agent_id, boost::bind(&LLUrlEntryAgentName::onAvatarNameCache, this, _1, _2));
		mAvatarNameCacheConnections.emplace(agent_id, connection);
		addObserver(agent_id_string, url, cb);
		return name_wait_str;
	}
}

LLStyleSP LLUrlEntryAgentName::getStyle() const
{
	static const LLUICachedControl<LLColor4> color("HTMLAgentColor");
	LLStyleSP style_params(new LLStyle(true, color, LLStringUtil::null));
	return style_params;
}

//
// LLUrlEntryAgentCompleteName describes a Second Life agent complete name Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/completename
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/completename
//
LLUrlEntryAgentCompleteName::LLUrlEntryAgentCompleteName()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/agent/[\\da-f-]+/completename",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryAgentCompleteName::getName(const LLAvatarName& avatar_name)
{
	return avatar_name.getCompleteName();
}

//
// LLUrlEntryAgentLegacyName describes a Second Life agent legacy name Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/legacyname
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/legacyname
//
LLUrlEntryAgentLegacyName::LLUrlEntryAgentLegacyName()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/agent/[\\da-f-]+/legacyname",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryAgentLegacyName::getName(const LLAvatarName& avatar_name)
{
	return avatar_name.getLegacyName();
}

//
// LLUrlEntryAgentDisplayName describes a Second Life agent display name Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/displayname
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/displayname
//
LLUrlEntryAgentDisplayName::LLUrlEntryAgentDisplayName()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/agent/[\\da-f-]+/displayname",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryAgentDisplayName::getName(const LLAvatarName& avatar_name)
{
	return avatar_name.getDisplayName();
}

//
// LLUrlEntryAgentUserName describes a Second Life agent user name Url, e.g.,
// secondlife:///app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/username
// x-grid-info://lincoln.lindenlab.com/app/agent/0e346d8b-4433-4d66-a6b0-fd37083abc4c/username
//
LLUrlEntryAgentUserName::LLUrlEntryAgentUserName()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/agent/[\\da-f-]+/username",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryAgentUserName::getName(const LLAvatarName& avatar_name)
{
	return avatar_name.getAccountName();
}

//
// LLUrlEntryGroup Describes a Second Life group Url, e.g.,
// secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/about
// secondlife:///app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/inspect
// x-grid-info://lincoln.lindenlab.com/app/group/00005ff3-4044-c79f-9de8-fb28ae0df991/inspect
//
LLUrlEntryGroup::LLUrlEntryGroup()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/group/[\\da-f-]+/\\w+",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_group.xml";
	mIcon = "Generic_Group";
	mTooltip = LLTrans::getString("TooltipGroupUrl");
}



void LLUrlEntryGroup::onGroupNameReceived(const LLUUID& id,
										  const std::string& name,
										  bool is_group)
{
	// received the group name from the server - tell our observers
	callObservers(id.asString(), name, mIcon);
}

LLUUID	LLUrlEntryGroup::getID(const std::string &string) const
{
	return LLUUID(getIDStringFromUrl(string));
}


std::string LLUrlEntryGroup::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	if (!gCacheName)
	{
		// probably at login screen, give something short for layout
		return LLTrans::getString("LoadingData");
	}

	std::string group_id_string = getIDStringFromUrl(url);
	if (group_id_string.empty())
	{
		// something went wrong, give raw url
		return unescapeUrl(url);
	}

	LLUUID group_id(group_id_string);
	std::string group_name;
	if (group_id.isNull())
	{
		return LLTrans::getString("GroupNameNone");
	}
	else if (gCacheName->getGroupName(group_id, group_name))
	{
		return group_name;
	}
	else
	{
		gCacheName->getGroup(group_id,
			boost::bind(&LLUrlEntryGroup::onGroupNameReceived,
				this, _1, _2, _3));
		addObserver(group_id_string, url, cb);
		return LLTrans::getString("LoadingData");
	}
}

LLStyleSP LLUrlEntryGroup::getStyle() const
{
	LLStyleSP style_params = LLUrlEntryBase::getStyle();
	//style_params->mUnderline = false; // Singu Note: We're not gonna bother here, underlining on hover
	return style_params;
}


//
// LLUrlEntryInventory Describes a Second Life inventory Url, e.g.,
// secondlife:///app/inventory/0e346d8b-4433-4d66-a6b0-fd37083abc4c/select
//
LLUrlEntryInventory::LLUrlEntryInventory()
{
	//*TODO: add supporting of inventory item names with whitespaces
	//this pattern cann't parse for example 
	//secondlife:///app/inventory/0e346d8b-4433-4d66-a6b0-fd37083abc4c/select?name=name with spaces&param2=value
	//x-grid-info://lincoln.lindenlab.com/app/inventory/0e346d8b-4433-4d66-a6b0-fd37083abc4c/select?name=name with spaces&param2=value
	mPattern = boost::regex(APP_HEADER_REGEX "/inventory/[\\da-f-]+/\\w+\\S*",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_inventory.xml";
}

std::string LLUrlEntryInventory::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	std::string label = getStringAfterToken(url, "name=");
	return LLURI::unescape(label.empty() ? url : label);
}

//
// LLUrlEntryObjectIM Describes a Second Life inspector for the object Url, e.g.,
// secondlife:///app/objectim/7bcd7864-da6b-e43f-4486-91d28a28d95b?name=Object&owner=3de548e1-57be-cfea-2b78-83ae3ad95998&slurl=Danger!%20Danger!/200/200/30/&groupowned=1
//
LLUrlEntryObjectIM::LLUrlEntryObjectIM()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/objectim/[\\da-f-]+\?\\S*\\w",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_objectim.xml";
}

std::string LLUrlEntryObjectIM::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	LLURI uri(url);
	LLSD query_map = uri.queryMap();
	if (query_map.has("name"))
		return query_map["name"];
	return unescapeUrl(url);
}

std::string LLUrlEntryObjectIM::getLocation(const std::string &url) const
{
	LLURI uri(url);
	LLSD query_map = uri.queryMap();
	if (query_map.has("slurl"))
		return query_map["slurl"];
	return LLUrlEntryBase::getLocation(url);
}

// LLUrlEntryParcel statics.
LLUUID	LLUrlEntryParcel::sAgentID(LLUUID::null);
LLUUID	LLUrlEntryParcel::sSessionID(LLUUID::null);
LLHost	LLUrlEntryParcel::sRegionHost(LLHost::invalid);
bool	LLUrlEntryParcel::sDisconnected(false);
std::set<LLUrlEntryParcel*> LLUrlEntryParcel::sParcelInfoObservers;

///
/// LLUrlEntryParcel Describes a Second Life parcel Url, e.g.,
/// secondlife:///app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about
/// x-grid-info://lincoln.lindenlab.com/app/parcel/0000060e-4b39-e00b-d0c3-d98b1934e3a8/about
///
LLUrlEntryParcel::LLUrlEntryParcel()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/parcel/[\\da-f-]+/about",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_parcel.xml";
	mTooltip = LLTrans::getString("TooltipParcelUrl");

	sParcelInfoObservers.insert(this);
}

LLUrlEntryParcel::~LLUrlEntryParcel()
{
	sParcelInfoObservers.erase(this);
}

std::string LLUrlEntryParcel::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	LLSD path_array = LLURI(url).pathArray();
	S32 path_parts = path_array.size();

	if (path_parts < 3) // no parcel id
	{
		LL_WARNS() << "Failed to parse url [" << url << "]" << LL_ENDL;
		return url;
	}

	std::string parcel_id_string = unescapeUrl(path_array[2]); // parcel id

	// Add an observer to call LLUrlLabelCallback when we have parcel name.
	addObserver(parcel_id_string, url, cb);

	LLUUID parcel_id(parcel_id_string);

	sendParcelInfoRequest(parcel_id);

	return unescapeUrl(url);
}

void LLUrlEntryParcel::sendParcelInfoRequest(const LLUUID& parcel_id)
{
	if (sRegionHost == LLHost::invalid || sDisconnected) return;

	LLMessageSystem *msg = gMessageSystem;
	msg->newMessage("ParcelInfoRequest");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, sAgentID );
	msg->addUUID("SessionID", sSessionID);
	msg->nextBlock("Data");
	msg->addUUID("ParcelID", parcel_id);
	msg->sendReliable(sRegionHost);
}

void LLUrlEntryParcel::onParcelInfoReceived(const std::string &id, const std::string &label)
{
	callObservers(id, label.empty() ? LLTrans::getString("RegionInfoError") : label, mIcon);
}

// static
void LLUrlEntryParcel::processParcelInfo(const LLParcelData& parcel_data)
{
	std::string label(LLStringUtil::null);
	if (!parcel_data.name.empty())
	{
		label = parcel_data.name;
	}
	// If parcel name is empty use Sim_name (x, y, z) for parcel label.
	else if (!parcel_data.sim_name.empty())
	{
		S32 region_x = ll_round(parcel_data.global_x) % REGION_WIDTH_UNITS;
		S32 region_y = ll_round(parcel_data.global_y) % REGION_WIDTH_UNITS;
		S32 region_z = ll_round(parcel_data.global_z);

		label = llformat("%s (%d, %d, %d)",
				parcel_data.sim_name.c_str(), region_x, region_y, region_z);
	}

	for (auto url_entry : sParcelInfoObservers)
	{
		if (url_entry)
		{
			url_entry->onParcelInfoReceived(parcel_data.parcel_id.asString(), label);
		}
	}
}

//
// LLUrlEntryPlace Describes secondlife://<location> URLs
//
LLUrlEntryPlace::LLUrlEntryPlace()
{
	mPattern = boost::regex("((((x-grid-info://)|(x-grid-location-info://))[-\\w\\.]+(:\\d+)?/region/)|(secondlife://))\\S+/?(\\d+/\\d+/\\d+|\\d+/\\d+)/?",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_slurl.xml";
	mTooltip = LLTrans::getString("TooltipSLURL");
}

std::string LLUrlEntryPlace::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	//
	// we handle SLURLs in the following formats:
	//   - secondlife://Place/X/Y/Z
	//   - secondlife://Place/X/Y
	//
	LLURI uri(url);
	std::string location = unescapeUrl(uri.hostName());
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();
	if (path_parts == 3)
	{
		// handle slurl with (X,Y,Z) coordinates
		std::string x = path_array[0];
		std::string y = path_array[1];
		std::string z = path_array[2];
		return location + " (" + x + "," + y + "," + z + ")";
	}
	else if (path_parts == 2)
	{
		// handle slurl with (X,Y) coordinates
		std::string x = path_array[0];
		std::string y = path_array[1];
		return location + " (" + x + "," + y + ")";
	}
	else if (path_parts > 3) // xgrid, show as grid: place(x, y, z)
	{
		auto place = unescapeUrl(path_array[1].asStringRef());
		const auto& x = path_array[2].asStringRef();
		const auto& y = path_array[3].asStringRef();
		location += ": " + place + " (" + x + ',' + y;
		if (path_parts == 5) location += ',' + path_array[4].asStringRef();
		return location + ')';
	}

	return url;
}

std::string LLUrlEntryPlace::getLocation(const std::string &url) const
{
	// return the part of the Url after secondlife:// part
	const auto uri = LLURI(url);
	bool xgrid = boost::algorithm::starts_with(uri.scheme(), "x-grid");
	if (xgrid)
	{
		return ::getStringAfterToken(url, "region/");
	}
	return ::getStringAfterToken(url, "://");
}

//
// LLUrlEntryRegion Describes secondlife:///app/region/REGION_NAME/X/Y/Z URLs, e.g.
// secondlife:///app/region/Ahern/128/128/0
//
LLUrlEntryRegion::LLUrlEntryRegion()
{
	mPattern = boost::regex(X_GRID_OR_SECONDLIFE_HEADER_REGEX"//app/region/[^/\\s]+(/\\d+)?(/\\d+)?(/\\d+)?/?",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_slurl.xml";
	mTooltip = LLTrans::getString("TooltipSLURL");
}

std::string LLUrlEntryRegion::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	//
	// we handle SLURLs in the following formats:
	//   - secondlife:///app/region/Place/X/Y/Z
	//   - secondlife:///app/region/Place/X/Y
	//   - secondlife:///app/region/Place/X
	//   - secondlife:///app/region/Place
	//

	const auto uri = LLURI(url);
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();

	bool xgrid = boost::algorithm::starts_with(uri.scheme(), "x-grid");
	auto i = xgrid ? 3 : 2;

	if (path_parts < i+1) // no region name
	{
		LL_WARNS() << "Failed to parse url [" << url << "]" << LL_ENDL;
		return url;
	}

	std::string label =
		xgrid ? unescapeUrl(path_array[0].asStringRef()) + ": " + unescapeUrl(path_array[i].asStringRef()) : // grid and region name
		unescapeUrl(path_array[i].asStringRef()); // region name

	++i;
	if (path_parts > i+1) // secondlife:///app/region/Place/X
	{
		std::string x = path_array[i++];
		label += " (" + x;

		if (path_parts > i+1) // secondlife:///app/region/Place/X/Y
		{
			std::string y = path_array[i++];
			label += "," + y;

			if (path_parts > i+1) // secondlife:///app/region/Place/X/Y/Z
			{
				std::string z = path_array[i];
				label = label + "," + z;
			}
		}

		label += ")";
	}

	return label;
}

std::string LLUrlEntryRegion::getLocation(const std::string &url) const
{
	const auto uri = LLURI(url);
	LLSD path_array = uri.pathArray();
	bool xgrid = boost::algorithm::starts_with(uri.scheme(), "x-grid");
	std::string region_name = unescapeUrl(path_array[xgrid ? 3 : 2]);
	return region_name;
}

//
// LLUrlEntryTeleport Describes a Second Life teleport Url, e.g.,
// secondlife:///app/teleport/Ahern/50/50/50/
// x-grid-info://lincoln.lindenlab.com/app/teleport/Ahern/50/50/50/
//
LLUrlEntryTeleport::LLUrlEntryTeleport()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/teleport/\\S+(/\\d+)?(/\\d+)?(/\\d+)?/?\\S*",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_teleport.xml";
	mTooltip = LLTrans::getString("TooltipTeleportUrl");
}

std::string LLUrlEntryTeleport::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	//
	// we handle teleport SLURLs in the following formats:
	//   - secondlife:///app/teleport/Place/X/Y/Z
	//   - secondlife:///app/teleport/Place/X/Y
	//   - secondlife:///app/teleport/Place/X
	//   - secondlife:///app/teleport/Place
	//
	LLURI uri(url);
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();
	std::string host = uri.hostName();
	std::string label = LLTrans::getString("SLurlLabelTeleport");
	if (!host.empty())
	{
		label += " " + host;
	}
	if (path_parts == 6)
	{
		// handle teleport url with (X,Y,Z) coordinates
		std::string location = unescapeUrl(path_array[path_parts-4]);
		std::string x = path_array[path_parts-3];
		std::string y = path_array[path_parts-2];
		std::string z = path_array[path_parts-1];
		return label + " " + location + " (" + x + "," + y + "," + z + ")";
	}
	else if (path_parts == 5)
	{
		// handle teleport url with (X,Y) coordinates
		std::string location = unescapeUrl(path_array[path_parts-3]);
		std::string x = path_array[path_parts-2];
		std::string y = path_array[path_parts-1];
		return label + " " + location + " (" + x + "," + y + ")";
	}
	else if (path_parts == 4)
	{
		// handle teleport url with (X) coordinate only
		std::string location = unescapeUrl(path_array[path_parts-2]);
		std::string x = path_array[path_parts-1];
		return label + " " + location + " (" + x + ")";
	}
	else if (path_parts == 3)
	{
		// handle teleport url with no coordinates
		std::string location = unescapeUrl(path_array[path_parts-1]);
		return label + " " + location;
	}

	return url;
}

std::string LLUrlEntryTeleport::getLocation(const std::string &url) const
{
	// return the part of the Url after ///app/teleport
	return ::getStringAfterToken(url, "app/teleport/");
}

//
// LLUrlEntrySL Describes a generic SLURL, e.g., a Url that starts
// with secondlife:// (used as a catch-all for cases not matched above)
//
LLUrlEntrySL::LLUrlEntrySL()
{
	mPattern = boost::regex(X_GRID_OR_SECONDLIFE_HEADER_REGEX "(\\w+)?(:\\d+)?/\\S+",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_slapp.xml";
	mTooltip = LLTrans::getString("TooltipSLAPP");
}

std::string LLUrlEntrySL::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return unescapeUrl(url);
}

//
// LLUrlEntrySLLabel Describes a generic SLURL, e.g., a Url that starts
/// with secondlife:// with the ability to specify a custom label.
//
LLUrlEntrySLLabel::LLUrlEntrySLLabel()
{
	mPattern = boost::regex("\\[" X_GRID_OR_SECONDLIFE_HEADER_REGEX "\\S+[ \t]+[^\\]]+\\]",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_slapp.xml";
	mTooltip = LLTrans::getString("TooltipSLAPP");
}

std::string LLUrlEntrySLLabel::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	std::string label = getLabelFromWikiLink(url);
	return (!LLUrlRegistry::instance().hasUrl(label)) ? label : getUrl(url);
}

std::string LLUrlEntrySLLabel::getUrl(const std::string &string) const
{
	return getUrlFromWikiLink(string);
}

std::string LLUrlEntrySLLabel::getTooltip(const std::string &string) const
{
	// return a tooltip corresponding to the URL type instead of the generic one (EXT-4574)
	std::string url = getUrl(string);
	LLUrlMatch match;
	if (LLUrlRegistry::instance().findUrl(url, match))
	{
		return match.getTooltip();
	}

	// unrecognized URL? should not happen
	return LLUrlEntryBase::getTooltip(string);
}

bool LLUrlEntrySLLabel::underlineOnHoverOnly(const std::string &string) const
{
	std::string url = getUrl(string);
	LLUrlMatch match;
	if (LLUrlRegistry::instance().findUrl(url, match))
	{
		return match.underlineOnHoverOnly();
	}

	// unrecognized URL? should not happen
	return LLUrlEntryBase::underlineOnHoverOnly(string);
}

//
// LLUrlEntryWorldMap Describes secondlife:///<location> URLs
//
LLUrlEntryWorldMap::LLUrlEntryWorldMap()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/worldmap/\\S+/?(\\d+)?/?(\\d+)?/?(\\d+)?/?\\S*",
							boost::regex::perl|boost::regex::icase);
	mMenuName = "menu_url_map.xml";
	mTooltip = LLTrans::getString("TooltipMapUrl");
}

std::string LLUrlEntryWorldMap::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	//
	// we handle SLURLs in the following formats:
	//   - secondlife:///app/worldmap/PLACE/X/Y/Z
	//   - secondlife:///app/worldmap/PLACE/X/Y
	//   - secondlife:///app/worldmap/PLACE/X
	//
	LLURI uri(url);
	LLSD path_array = uri.pathArray();
	S32 path_parts = path_array.size();
	if (path_parts < 3)
	{
		return url;
	}

	const std::string label = LLTrans::getString("SLurlLabelShowOnMap");
	std::string location = unescapeUrl(path_array[2]);
	std::string x = (path_parts > 3) ? path_array[3] : "128";
	std::string y = (path_parts > 4) ? path_array[4] : "128";
	std::string z = (path_parts > 5) ? path_array[5] : "0";
	return label + " " + location + " (" + x + "," + y + "," + z + ")";
}

std::string LLUrlEntryWorldMap::getLocation(const std::string &url) const
{
	// return the part of the Url after secondlife:///app/worldmap/ part
	return ::getStringAfterToken(url, "app/worldmap/");
}

//
// LLUrlEntryNoLink lets us turn of URL detection with <nolink>...</nolink> tags
//
LLUrlEntryNoLink::LLUrlEntryNoLink()
{
	mPattern = boost::regex("<nolink>.*?</nolink>",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryNoLink::getUrl(const std::string &url) const
{
	// return the text between the <nolink> and </nolink> tags
	return url.substr(8, url.size()-8-9);
}

std::string LLUrlEntryNoLink::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return getUrl(url);
}

LLStyleSP LLUrlEntryNoLink::getStyle() const
{ 
	// Don't render as URL (i.e. no context menu or hand cursor).
	LLStyleSP style(new LLStyle());
	style->setLinkHREF(" ");
	return style;
}


//
// LLUrlEntryIcon describes an icon with <icon>...</icon> tags
//
LLUrlEntryIcon::LLUrlEntryIcon()
{
	mPattern = boost::regex("<icon\\s*>\\s*([^<]*)?\\s*</icon\\s*>",
							boost::regex::perl|boost::regex::icase);
}

std::string LLUrlEntryIcon::getUrl(const std::string &url) const
{
	return LLStringUtil::null;
}

std::string LLUrlEntryIcon::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return LLStringUtil::null;
}

std::string LLUrlEntryIcon::getIcon(const std::string &url)
{
	// Grep icon info between <icon>...</icon> tags
	// matches[1] contains the icon name/path
	boost::match_results<std::string::const_iterator> matches;
	mIcon = (boost::regex_match(url, matches, mPattern) && matches[1].matched)
		? matches[1]
		: LLStringUtil::null;
	LLStringUtil::trim(mIcon);
	return mIcon;
}

//
// LLUrlEntryEmail Describes a generic mailto: Urls
//
LLUrlEntryEmail::LLUrlEntryEmail()
	: LLUrlEntryBase()
{
	mPattern = boost::regex("(mailto:)?[\\w\\.\\-]+@[\\w\\.\\-]+\\.[a-z]{2,63}",
							boost::regex::perl | boost::regex::icase);
	mMenuName = "menu_url_email.xml";
	mTooltip = LLTrans::getString("TooltipEmail");
}

std::string LLUrlEntryEmail::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	size_t pos = url.find("mailto:");

	if (pos == std::string::npos)
	{
		return escapeUrl(url);
	}

	std::string ret = escapeUrl(url.substr(pos + 7, url.length() - pos + 8));
	return ret;
}

std::string LLUrlEntryEmail::getUrl(const std::string &string) const
{
	if (string.find("mailto:") == std::string::npos)
	{
		return "mailto:" + escapeUrl(string);
	}
	return escapeUrl(string);
}

LLUrlEntryExperienceProfile::LLUrlEntryExperienceProfile()
{
	mPattern = boost::regex(APP_HEADER_REGEX "/experience/[\\da-f-]+/\\w+\\S*",
		boost::regex::perl|boost::regex::icase);
	mIcon = "Generic_Experience";
	mMenuName = "menu_url_experience.xml";
}

std::string LLUrlEntryExperienceProfile::getLabel(const std::string& url, const LLUrlLabelCallback& cb)
{
	if (!gCacheName)
	{
		// probably at the login screen, use short string for layout
		return LLTrans::getString("LoadingData");
	}

	std::string experience_id_string = getIDStringFromUrl(url);
	if (experience_id_string.empty())
	{
		// something went wrong, just give raw url
		return unescapeUrl(url);
	}

	LLUUID experience_id(experience_id_string);
	if (experience_id.isNull())
	{
		return LLTrans::getString("ExperienceNameNull");
	}

    const LLSD& experience_details = LLExperienceCache::instance().get(experience_id);
	if (!experience_details.isUndefined())
	{
		std::string experience_name_string = experience_details[LLExperienceCache::NAME].asString();
        return experience_name_string.empty() ? LLTrans::getString("ExperienceNameUntitled") : experience_name_string;
	}

	addObserver(experience_id_string, url, cb);
    LLExperienceCache::instance().get(experience_id, boost::bind(&LLUrlEntryExperienceProfile::onExperienceDetails, this, _1));
	return LLTrans::getString("LoadingData");

}

void LLUrlEntryExperienceProfile::onExperienceDetails(const LLSD& experience_details)
{
	std::string name = experience_details[LLExperienceCache::NAME].asString();
	if(name.empty())
	{
		name = LLTrans::getString("ExperienceNameUntitled");
	}
    callObservers(experience_details[LLExperienceCache::EXPERIENCE_ID].asString(), name, LLStringUtil::null);
}

// <alchemy>
//
// LLUrlEntryJIRA describes a Jira Issue Tracker entry
//
LLUrlEntryJira::LLUrlEntryJira()
{
	mPattern = boost::regex("(\\b(?:ALCH|SV|BUG|CHOP|FIRE|MAINT|OPEN|SCR|STORM|SVC|VWR|WEB)-\\d+)",
							boost::regex::perl);
	mMenuName = "menu_url_http.xml";
	mTooltip = LLTrans::getString("TooltipHttpUrl");
}

std::string LLUrlEntryJira::getLabel(const std::string &url, const LLUrlLabelCallback &cb)
{
	return unescapeUrl(url);
}

std::string LLUrlEntryJira::getTooltip(const std::string &string) const
{
	return getUrl(string);
}

std::string LLUrlEntryJira::getUrl(const std::string &url) const
{
	return (boost::format(
		(url.find("ALCH") != std::string::npos) ?
			"http://alchemy.atlassian.net/browse/%1%" :
		(url.find("SV") != std::string::npos) ?
			"https://singularityviewer.atlassian.net/browse/%1%" :
		(url.find("FIRE") != std::string::npos) ?
			"https://jira.firestormviewer.com/browse/%1%" :
		"http://jira.secondlife.com/browse/%1%"
	) % url).str();
}
// </alchemy>

