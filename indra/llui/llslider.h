/** 
 * @file llslider.h
 * @brief A simple slider with no label.
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

#ifndef LL_LLSLIDER_H
#define LL_LLSLIDER_H

#include "lluictrl.h"
#include "v4color.h"
#include "lluiimage.h"

class LLSlider : public LLUICtrl
{
public:
	LLSlider( 
		const std::string& name,
		const LLRect& rect,
		commit_callback_t commit_callback,
		F32 initial_value,
		F32 min_value,
		F32 max_value,
		F32 increment,
		const std::string& control_name = LLStringUtil::null );

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static  LLView* fromXML(LLXMLNodePtr node, LLView *parent, class LLUICtrlFactory *factory);

	virtual ~LLSlider();
	void			setValue( F32 value, BOOL from_event = FALSE );
	F32				getValueF32() const { return mValue; }

	virtual void	setValue(const LLSD& value )	{ setValue((F32)value.asReal(), TRUE); }
	virtual LLSD	getValue() const		{ return LLSD(getValueF32()); }

	virtual void	setMinValue(LLSD min_value)	{ setMinValue((F32)min_value.asReal()); }
	virtual void	setMaxValue(LLSD max_value)	{ setMaxValue((F32)max_value.asReal());  }

	F32				getInitialValue() const { return mInitialValue; }
	F32				getMinValue() const		{ return mMinValue; }
	F32				getMaxValue() const		{ return mMaxValue; }
	F32				getIncrement() const	{ return mIncrement; }
	void			setMinValue(F32 min_value) {mMinValue = min_value; updateThumbRect(); }
	void			setMaxValue(F32 max_value) {mMaxValue = max_value; updateThumbRect(); }
	void			setIncrement(F32 increment) {mIncrement = increment;}

	boost::signals2::connection setMouseDownCallback( const commit_signal_t::slot_type& cb );
	boost::signals2::connection setMouseUpCallback(	const commit_signal_t::slot_type& cb );

	BOOL	handleHover(S32 x, S32 y, MASK mask) override;
	BOOL	handleMouseUp(S32 x, S32 y, MASK mask) override;
	BOOL	handleMouseDown(S32 x, S32 y, MASK mask) override;
	BOOL	handleKeyHere(KEY key, MASK mask) override;
	BOOL	handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void	draw() override;

private:
	void			setValueAndCommit(F32 value);
	void			updateThumbRect();

	F32				mValue;
	F32				mInitialValue;
	F32				mMinValue;
	F32				mMaxValue;
	F32				mIncrement;

	S32				mMouseOffset;
	LLRect			mDragStartThumbRect;

	LLPointer<LLUIImage>	mThumbImage;
	LLPointer<LLUIImage>		mTrackImage;
	LLPointer<LLUIImage>		mTrackHighlightImage;

	LLRect			mThumbRect;
	LLColor4		mTrackColor;
	LLColor4		mThumbOutlineColor;
	LLColor4		mThumbCenterColor;
	
	commit_signal_t*	mMouseDownSignal;
	commit_signal_t*	mMouseUpSignal;
};

#endif  // LL_LLSLIDER_H
