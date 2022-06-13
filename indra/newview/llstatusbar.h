/** 
 * @file llstatusbar.h
 * @brief LLStatusBar class definition
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

#ifndef LL_LLSTATUSBAR_H
#define LL_LLSTATUSBAR_H

#include "llpanel.h"
#include "llpathfindingnavmesh.h"

// "Constants" loaded from settings.xml at start time
extern S32 STATUS_BAR_HEIGHT;

class LLButton;
class LLLineEditor;
class LLMessageSystem;
class LLTextBox;
class LLTextEditor;
class LLUICtrl;
class LLUUID;
class LLFrameTimer;
class LLStatGraph;
class LLPathfindingNavMeshStatus;

class LLStatusBar
:	public LLPanel
{
public:
	LLStatusBar(const std::string& name, const LLRect& rect );
	/*virtual*/ ~LLStatusBar();
	
	/*virtual*/ void draw();

	// MANIPULATORS
	void		setBalance(S32 balance);
	void		setUPC(S32 balance);
	void		debitBalance(S32 debit);
	void		creditBalance(S32 credit);

	// Request the latest currency balance from the server
	static void sendMoneyBalanceRequest(bool from_user = false);

	void		setHealth(S32 percent);

	void setLandCredit(S32 credit);
	void setLandCommitted(S32 committed);

	void		refresh();
	void setVisibleForMouselook(bool visible);
		// some elements should hide in mouselook

	// ACCESSORS
	S32			getBalance() const;
	S32			getHealth() const;

	BOOL isUserTiered() const;
	S32 getSquareMetersCredit() const;
	S32 getSquareMetersCommitted() const;
	S32 getSquareMetersLeft() const;

private:
	// simple method to setup the part that holds the date
	void setupDate();

	void onRegionBoundaryCrossed();
	void createNavMeshStatusListenerForCurrentRegion();
	void onNavMeshStatusChange(const LLPathfindingNavMeshStatus &pNavMeshStatus);

private:
	LLTextBox	*mTextBalance;
	LLTextBox	*mTextUPC;
	LLTextBox	*mTextHealth;
	LLTextBox	*mTextTime;

	LLTextBox*	mTextParcelName;

	LLStatGraph *mSGBandwidth;
	LLStatGraph *mSGPacketLoss;

	LLButton	*mBtnBuyCurrency;

	LLUICtrl*	mScriptOut;
	LLUICtrl*	mHealthV;
	LLUICtrl*	mNoFly;
	LLUICtrl*	mBuyLand;
	LLUICtrl*	mBuyCurrency;
	LLUICtrl*	mNoBuild;
	LLUICtrl*	mPFDirty;
	LLUICtrl*	mPFDisabled;
	LLUICtrl*	mStatusSeeAV;
	LLUICtrl*	mNoScripts;
	LLUICtrl*	mRestrictPush;
	LLUICtrl*	mStatusNoVoice;
	LLUICtrl*	mSearchEditor;
	LLUICtrl*	mSearchBtn;
	LLView*		mSearchBevel;
	LLTextBox*	mStatBtn;

	S32				mBalance;
	S32				mUPC;
	S32				mHealth;
	S32				mSquareMetersCredit;
	S32				mSquareMetersCommitted;
	LLFrameTimer*	mBalanceTimer;
	LLFrameTimer*	mHealthTimer;
	boost::signals2::connection	mRegionCrossingSlot;
	LLPathfindingNavMesh::navmesh_slot_t mNavMeshSlot;
	bool mIsNavMeshDirty;
	bool mUPCSupported;
	
	static std::vector<std::string> sDays;
	static std::vector<std::string> sMonths;
	static const U32 MAX_DATE_STRING_LENGTH;
};

// *HACK: Status bar owns your cached money balance. JC
BOOL can_afford_transaction(S32 cost);

extern LLStatusBar *gStatusBar;

#endif
