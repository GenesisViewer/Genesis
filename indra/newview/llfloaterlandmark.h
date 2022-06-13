/** 
 * @file llfloaterlandmark.h
 * @author Richard Nelson, James Cook, Sam Kolb
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERLANDMARK_H
#define LL_LLFLOATERLANDMARK_H

#include "llfloater.h"
#include "lluictrl.h"
#include "llpermissionsflags.h"

class LLButton;
class LLFloaterTexturePicker;
class LLInventoryItem;
class LLTextBox;
class LLViewBorder;
class LLFolderViewItem;
class LLSearchEditor;
class LLInventoryPanel;
class LLSaveFolderState;
class LLViewerTexture;
class LLFilterEditor;

// used for setting drag & drop callbacks.
typedef BOOL (*drag_n_drop_callback)(LLUICtrl*, LLInventoryItem*, void*);


//////////////////////////////////////////////////////////////////////////////////////////
// LLFloaterLandmark

class LLFloaterLandmark: public LLFloater, public LLFloaterSingleton<LLFloaterLandmark>
{
public:
	LLFloaterLandmark(const LLSD& data);
	virtual ~LLFloaterLandmark();

	BOOL postBuild();

	// LLView overrides
	virtual BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask,
		BOOL drop, EDragAndDropType cargo_type, void *cargo_data, 
		EAcceptance *accept,
		std::string& tooltip_msg);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);

	// LLFloater overrides
	virtual void	onClose(bool app_quitting);

	const LLUUID& getAssetID() { return mImageAssetID; }
	const LLUUID& findItemID(const LLUUID& asset_id, BOOL copyable_only);

	void			setDirty( BOOL b ) { mIsDirty = b; }
	BOOL			isDirty() const { return mIsDirty; }
	void			setActive( BOOL active );

	void		onBtnClose();
	void		onBtnNew();
	void		onBtnEdit();
	void		onBtnDelete();
	void		onBtnNewFolder();
	void		onBtnRename();
	void		onSelectionChange(const std::deque<LLFolderViewItem*> &items, BOOL user_action);
	void		onShowFolders(LLUICtrl* ctrl);
	void		onFilterEdit(const LLSD& value);

protected:
	LLPointer<LLViewerTexture> mLandmarkp;

	LLUUID				mImageAssetID; // Currently selected texture

	LLUUID				mWhiteImageAssetID;
	LLUUID				mSpecialCurrentImageAssetID;  // Used when the asset id has no corresponding texture in the user's inventory.
	LLUUID				mOriginalImageAssetID;

	LLTextBox*			mTentativeLabel;
	LLTextBox*			mResolutionLabel;

	std::string			mPendingName;
	BOOL				mIsDirty;
	BOOL				mActive;

	LLFilterEditor*		mFilterEdit;
	LLInventoryPanel*	mInventoryPanel;
	PermissionMask		mImmediateFilterPermMask;
	PermissionMask		mNonImmediateFilterPermMask;
	BOOL				mNoCopyLandmarkSelected;
	F32					mContextConeOpacity;
};

#endif  // LL_FLOATERLANDMARK_H
