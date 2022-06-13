/**
 * @file lltrans.cpp
 * @brief LLTrans implementation
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2009, Linden Research, Inc.
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


#include "linden_common.h"
#include "lltrans.h"
#include "llxmlnode.h"
#include "lluictrlfactory.h"
#include "llnotificationsutil.h"

#include <map>

LLTrans::template_map_t LLTrans::sStringTemplates;
LLStringUtil::format_map_t LLTrans::sDefaultArgs;

//static 
bool LLTrans::parseStrings(const std::string& xml_filename, const std::set<std::string>& default_args)
{
	LLXMLNodePtr root;
	BOOL success  = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);

	if (!success || root.isNull() || !root->hasName( "strings" ))
	{
		LL_ERRS() << "Problem reading strings: " << xml_filename << LL_ENDL;
		return false;
	}
	
	sDefaultArgs.clear();
	for (LLXMLNode* string = root->getFirstChild();
		 string != NULL; string = string->getNextSibling())
	{
		if (!string->hasName("string"))
		{
			continue;
		}
		
		std::string string_name;

		if (! string->getAttributeString("name", string_name))
		{
			LL_WARNS() << "Unable to parse string with no name" << LL_ENDL;
			continue;
		}
	
		std::string contents;
		//value is used for strings with preceeding spaces. If not present, fall back to getTextContents()
		if(!string->getAttributeString("value",contents))
			contents=string->getTextContents();
		LLTransTemplate xml_template(string_name, contents);
		sStringTemplates[xml_template.mName] = xml_template;

		std::set<std::string>::const_iterator iter = default_args.find(xml_template.mName);
		if (iter != default_args.end())
		{
			std::string name = *iter;
			if (name[0] != '[')
				name = llformat("[%s]",name.c_str());
			sDefaultArgs[name] = xml_template.mText;
		}
	}

	return true;
}

static LLAtomicU32 sStringTemplates_accesses;
int const access_increment = 1000;

static void log_sStringTemplates_accesses(void)
{
  LL_DEBUGS() << "LLTrans::getString/findString called " << sStringTemplates_accesses << " in total." << LL_ENDL;
}

//static 
std::string LLTrans::getString(const std::string &xml_desc, const LLStringUtil::format_map_t& msg_args)
{
	// Singu note: make sure LLTrans isn't used in a tight loop.
	if (sStringTemplates_accesses++ % access_increment == access_increment - 1) log_sStringTemplates_accesses();

	template_map_t::iterator iter = sStringTemplates.find(xml_desc);

	if (iter != sStringTemplates.end())
	{
		std::string text = iter->second.mText;
		LLStringUtil::format_map_t args = sDefaultArgs;
		args.insert(msg_args.begin(), msg_args.end());
		LLStringUtil::format(text, args);
		
		return text;
	}
	else
	{
		LLSD args;
		args["STRING_NAME"] = xml_desc;
		LL_WARNS_ONCE("configuration") << "Missing String in strings.xml: [" << xml_desc << "]" << LL_ENDL;
		LLNotificationsUtil::add("MissingString", args);
		
		return "MissingString("+xml_desc+")";
	}
}

//static
std::string LLTrans::getString(const std::string &xml_desc, const LLSD& msg_args)
{
	// Singu note: make sure LLTrans isn't used in a tight loop.
	if (sStringTemplates_accesses++ % access_increment == 0) log_sStringTemplates_accesses();

	// Since sStringTemplates is read-only after it's initial initialization during start up,
	// this function is already thread-safe.

	template_map_t::iterator iter = sStringTemplates.find(xml_desc);
	if (iter != sStringTemplates.end())
	{
		std::string text = iter->second.mText;
		LLStringUtil::format(text, msg_args);
		return text;
	}
	else
	{
		LL_WARNS_ONCE("configuration") << "Missing String in strings.xml: [" << xml_desc << "]" << LL_ENDL;
		return "MissingString("+xml_desc+")";
	}
}

//static 
bool LLTrans::findString(std::string &result, const std::string &xml_desc, const LLStringUtil::format_map_t& msg_args)
{
	// Singu note: make sure LLTrans isn't used in a tight loop.
	if (sStringTemplates_accesses++ % access_increment == 0) log_sStringTemplates_accesses();

	template_map_t::iterator iter = sStringTemplates.find(xml_desc);
	if (iter != sStringTemplates.end())
	{
		std::string text = iter->second.mText;
		LLStringUtil::format_map_t args = sDefaultArgs;
		args.insert(msg_args.begin(), msg_args.end());
		LLStringUtil::format(text, args);
		result = text;
		return true;
	}
	else
	{
		LL_WARNS_ONCE("configuration") << "Missing String in strings.xml: [" << xml_desc << "]" << LL_ENDL;	
		return false;
	}
}

//static
bool LLTrans::findString(std::string &result, const std::string &xml_desc, const LLSD& msg_args)
{
	// Singu note: make sure LLTrans isn't used in a tight loop.
	if (sStringTemplates_accesses++ % access_increment == 0) log_sStringTemplates_accesses();

	template_map_t::iterator iter = sStringTemplates.find(xml_desc);
	if (iter != sStringTemplates.end())
	{
		std::string text = iter->second.mText;
		LLStringUtil::format(text, msg_args);
		result = text;
		return true;
	}
	else
	{
		LL_WARNS_ONCE("configuration") << "Missing String in strings.xml: [" << xml_desc << "]" << LL_ENDL;	
		return false;
	}
}

void LLTrans::setDefaultArg(const std::string& name, const std::string& value)
{
	sDefaultArgs[name] = value;
}
