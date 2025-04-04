/** 
 * @file llfloatergroups.h
 * @brief LLFloaterGroups class definition
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

/*
 * Shown from Edit -> Groups...
 * Shows the agent's groups and allows the edit window to be invoked.
 * Also overloaded to allow picking of a single group for assigning 
 * objects and land to groups.
 */

#ifndef LL_LLFLOATERGROUPS_H
#define LL_LLFLOATERGROUPS_H

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class llfloatergroups
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#include "lluuid.h"
#include "llfloater.h"
#include "llinstancetracker.h"
#include <map>
#ifndef BOOST_FUNCTION_HPP_INCLUDED
#include <boost/function.hpp>
#define BOOST_FUNCTION_HPP_INCLUDED
#endif
#include <boost/signals2.hpp>

class LLUICtrl;
class LLTextBox;
class LLScrollListCtrl;
class LLButton;

class LLFloaterGroupPicker : public LLFloater, public LLInstanceTracker<LLFloaterGroupPicker, LLUUID>
{
public:
	static LLFloaterGroupPicker* showInstance(const LLSD& seed);
	LLFloaterGroupPicker(const LLSD& seed);
	~LLFloaterGroupPicker();

	// Note: Don't return connection; use boost::bind + boost::signals2::trackable to disconnect slots
	typedef boost::signals2::signal<void (LLUUID id)> signal_t;
	void setSelectGroupCallback(const signal_t::slot_type& cb) { mGroupSelectSignal.connect(cb); }
	void setPowersMask(U64 powers_mask);
	BOOL postBuild();

	// implementation of factory policy
	static LLFloaterGroupPicker* findInstance(const LLSD& seed);
	static LLFloaterGroupPicker* createInstance(const LLSD& seed);

	// for cases like inviting avatar to group we don't want the none option
	void removeNoneOption();

protected:
	void ok();
	static void onBtnOK(void* userdata);
	static void onBtnCancel(void* userdata);

protected:
	LLUUID mID;
	U64 mPowersMask;
	signal_t mGroupSelectSignal;

	typedef std::map<const LLUUID, LLFloaterGroupPicker*> instance_map_t;
	static instance_map_t sInstances;
};

class LLPanelGroups : public LLPanel, public LLOldEvents::LLSimpleListener
{
public:
	LLPanelGroups();
	virtual ~LLPanelGroups();

	//LLEventListener
	/*virtual*/ bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata);
	
	// clear the group list, and get a fresh set of info.
	void reset();

protected:
	// initialize based on the type
	BOOL postBuild();

	// set the text displays
	void setTexts();

	// highlight_id is a group id to highlight
	void enableButtons();

	void onGroupSortChanged();
	void onGroupList();
	static void onBtnInvite(void* userdata);
	void invite();
	void filterGroups(const std::string& search_name);
};


#endif // LL_LLFLOATERGROUPS_H
