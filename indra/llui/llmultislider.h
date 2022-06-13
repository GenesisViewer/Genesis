/** 
 * @file llmultislider.h
 * @brief A simple multislider
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

#ifndef LL_MULTI_SLIDER_H
#define LL_MULTI_SLIDER_H

#include "lluictrl.h"
#include "v4color.h"

class LLUICtrlFactory;

class LLMultiSlider : public LLUICtrl
{
public:
	LLMultiSlider( 
		const std::string& name,
		const LLRect& rect,
		commit_callback_t commit_callback,
		F32 initial_value,
		F32 min_value,
		F32 max_value,
		F32 increment,
		S32 max_sliders,
		BOOL allow_overlap,
		BOOL draw_track,
		BOOL use_triangle,
		const std::string& control_name = LLStringUtil::null );

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static  LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	virtual ~LLMultiSlider();
	void			setSliderValue(const std::string& name, F32 value, BOOL from_event = FALSE);
	F32				getSliderValue(const std::string& name) const;

	const std::string& getCurSlider() const					{ return mCurSlider; }
	F32				getCurSliderValue() const				{ return getSliderValue(mCurSlider); }
	void			setCurSlider(const std::string& name);
	void			setCurSliderValue(F32 val, BOOL from_event = false) { setSliderValue(mCurSlider, val, from_event); }

	virtual void	setValue(const LLSD& value);
	virtual LLSD	getValue() const		{ return mValue; }

	virtual void	setMinValue(LLSD min_value)	{ setMinValue((F32)min_value.asReal()); }
	virtual void	setMaxValue(LLSD max_value)	{ setMaxValue((F32)max_value.asReal());  }

	F32				getInitialValue() const { return mInitialValue; }
	F32				getMinValue() const		{ return mMinValue; }
	F32				getMaxValue() const		{ return mMaxValue; }
	F32				getIncrement() const	{ return mIncrement; }
	void			setMinValue(F32 min_value) { mMinValue = min_value; }
	void			setMaxValue(F32 max_value) { mMaxValue = max_value; }
	void			setIncrement(F32 increment) { mIncrement = increment; }

	boost::signals2::connection setMouseDownCallback( const commit_signal_t::slot_type& cb );
	boost::signals2::connection setMouseUpCallback(	const commit_signal_t::slot_type& cb );

	bool findUnusedValue(F32& initVal);
	const std::string&	addSlider();
	const std::string&	addSlider(F32 val);
	void				addSlider(F32 val, const std::string& name);
	void				deleteSlider(const std::string& name);
	void				deleteCurSlider()			{ deleteSlider(mCurSlider); }
	void				clear();

	virtual BOOL	handleHover(S32 x, S32 y, MASK mask);
	virtual BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);
	virtual void	draw();

protected:
	LLSD			mValue;
	F32				mInitialValue;
	F32				mMinValue;
	F32				mMaxValue;
	F32				mIncrement;
	std::string		mCurSlider;
	static S32		mNameCounter;

	S32				mMaxNumSliders;
	BOOL			mAllowOverlap;
	BOOL			mDrawTrack;
	BOOL			mUseTriangle;			/// hacked in toggle to use a triangle

	S32				mMouseOffset;
	LLRect			mDragStartThumbRect;
	S32				mThumbWidth;

	std::map<std::string, LLRect>	mThumbRects;
	LLColor4		mTrackColor;
	LLColor4		mThumbOutlineColor;
	LLColor4		mThumbCenterColor;
	LLColor4		mThumbCenterSelectedColor;
	LLColor4		mDisabledThumbColor;
	LLColor4		mTriangleColor;
	
	commit_signal_t*	mMouseDownSignal;
	commit_signal_t*	mMouseUpSignal;
};

#endif  // LL_LLSLIDER_H
