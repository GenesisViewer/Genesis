/** 
 * @file llfloaterbvhpreview.h
 * @brief LLFloaterBvhPreview class definition
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERBVHPREVIEW_H
#define LL_LLFLOATERBVHPREVIEW_H

#include "llfloaternamedesc.h"
#include "lldynamictexture.h"
#include "llcharacter.h"
#include "llquaternion.h"

class LLVOAvatar;
class LLViewerJointMesh;

LL_ALIGN_PREFIX(16)
class LLPreviewAnimation : public LLViewerDynamicTexture
{
public:
	virtual ~LLPreviewAnimation();

public:
	LLPreviewAnimation(S32 width, S32 height);

	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	/*virtual*/ S8 getType() const ;

	BOOL	render();
	void	requestUpdate();
	void	rotate(F32 yaw_radians, F32 pitch_radians);
	void	zoom(F32 zoom_delta);
	void	setZoom(F32 zoom_amt);
	void	pan(F32 right, F32 up);
	virtual BOOL needsUpdate() { return mNeedsUpdate; }

	LLVOAvatar* getDummyAvatar() { return mDummyAvatar; }

protected:
	BOOL				mNeedsUpdate;
	F32					mCameraDistance;
	F32					mCameraYaw;
	F32					mCameraPitch;
	F32					mCameraZoom;
	LLVector3			mCameraOffset;
	LLVector3			mCameraRelPos;
	LLPointer<LLVOAvatar>			mDummyAvatar;
} LL_ALIGN_POSTFIX(16);

class LLFloaterBvhPreview : public LLFloaterNameDesc
{
public:
	//<edit>
	LLFloaterBvhPreview(const std::string& filename, void* item = NULL);
	//<edit>
	virtual ~LLFloaterBvhPreview();
	
	BOOL postBuild();

	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleMouseUp(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks); 
	void onMouseCaptureLost();

	void refresh();

	void	onBtnPlay();
	void	onBtnStop();
	void onSliderMove();
	void onCommitBaseAnim();
	void onCommitLoop();
	void onCommitLoopIn();
	void onCommitLoopOut();
	bool validateLoopIn(const LLSD& data);
	bool validateLoopOut(const LLSD& data);
	void onCommitName();
	void onCommitHandPose();
	void onCommitEmote();
	void onCommitPriority();
	void onCommitEaseIn();
	void onCommitEaseOut();
	bool validateEaseIn(const LLSD& data);
	bool validateEaseOut(const LLSD& data);
	static void	onBtnOK(void*);
	static void onSaveComplete(const LLUUID& asset_uuid,
									   LLAssetType::EType type,
									   void* user_data,
									   S32 status, LLExtStat ext_status);
private:
	void setAnimCallbacks() ;
	
protected:
	void			draw();
	void			resetMotion();

	LLPointer< LLPreviewAnimation> mAnimPreview;
	S32					mLastMouseX;
	S32					mLastMouseY;
	LLButton*			mPlayButton;
	LLButton*			mStopButton;
	LLRect				mPreviewRect;
	LLRectf				mPreviewImageRect;
	LLAssetID			mMotionID;
	LLTransactionID		mTransactionID;
	BOOL				mInWorld;
	LLAnimPauseRequest	mPauseRequest;

	std::map<std::string, LLUUID>	mIDList;
	//<edit>
	void* mItem;
	//</edit>
};

#endif  // LL_LLFLOATERBVHPREVIEW_H
