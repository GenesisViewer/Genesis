/** 
 * @file llbutton.h
 * @brief Header for buttons
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLBUTTON_H
#define LL_LLBUTTON_H

#include "lluuid.h"
#include "llcontrol.h"
#include "lluictrl.h"
#include "v4color.h"
#include "llframetimer.h"
#include "llfontgl.h"
#include "lluiimage.h"
#include "lluistring.h"
#include "llinitparam.h"
#include "lluicolor.h"

//
// Constants
//

// PLEASE please use these "constants" when building your own buttons.
// They are loaded from settings.xml at run time.
extern S32	LLBUTTON_H_PAD;
extern S32	LLBUTTON_V_PAD;
extern S32	BTN_HEIGHT_SMALL;
extern S32	BTN_HEIGHT;

// All button widths should be rounded up to this size
extern S32	BTN_GRID;

//
// Helpful functions
//
S32 round_up(S32 grid, S32 value);


class LLUICtrlFactory;

//
// Classes
//

class LLButton
: public LLUICtrl
{
public:
	struct Params
		: public LLInitParam::Block<Params, LLUICtrl::Params>
	{
		// text label
		Optional<std::string>	label_selected;
		Optional<bool>			label_shadow;
		Optional<bool>			auto_resize;
		Optional<bool>			use_ellipses;

		// images
		Optional<LLUIImage*>	image_unselected,
			image_selected,
			image_hover_selected,
			image_hover_unselected,
			image_disabled_selected,
			image_disabled,
			image_flash,
			image_pressed,
			image_pressed_selected,
			image_overlay,
			image_overlay_selected;

		Optional<std::string>	image_overlay_alignment;
		Optional<bool>			image_overlay_enable;

		// colors
		Optional<LLUIColor>		label_color,
			label_color_selected,
			label_color_disabled,
			label_color_disabled_selected,
			image_color,
			image_color_disabled,
			image_overlay_color,
			image_overlay_selected_color,
			image_overlay_disabled_color,
			flash_color;

		// layout
		Optional<S32>			pad_right;
		Optional<S32>			pad_left;
		Optional<S32>			pad_bottom; // under text label

		//image overlay paddings
		Optional<S32>			image_top_pad;
		Optional<S32>			image_bottom_pad;

		/**
		 * Space between image_overlay and label
		 */
		Optional<S32>			imgoverlay_label_space;

		// callbacks
		Optional<CommitCallbackParam>	click_callback, // alias -> commit_callback
			mouse_down_callback,
			mouse_up_callback,
			mouse_held_callback;

		// misc
		Optional<bool>			is_toggle,
			scale_image,
			commit_on_return,
			display_pressed_state;

		Optional<F32>				hover_glow_amount;
		//Optional<TimeIntervalParam>	held_down_delay;

		Optional<bool>				use_draw_context_alpha;

		//Optional<LLBadge::Params>	badge;

		Optional<bool>				handle_right_mouse;

		Optional<bool>				button_flash_enable;
		Optional<S32>				button_flash_count;
		Optional<F32>				button_flash_rate;
		Optional<bool>				fade_when_disabled;
		Optional<std::string>		help_url;

		Params();
	};

	// simple button with text label
	LLButton(const std::string& name, const LLRect &rect = LLRect(), const std::string& control_name = std::string(), 
			 commit_callback_t commit_callback = NULL);

	LLButton(const std::string& name, const LLRect& rect, 
			 const std::string &unselected_image,
			 const std::string &selected_image,
			 const std::string& control_name,	
			 commit_callback_t commit_callback,
			 const LLFontGL* mGLFont = NULL,
			 const std::string& unselected_label = LLStringUtil::null,
			 const std::string& selected_label = LLStringUtil::null );

	void initFromParams(const Params& p);
	static const LLButton::Params& getDefaultParams();
	LLButton(const Params& p = getDefaultParams());

public:

	~LLButton();
	// For backward compatability only
	typedef boost::function<void(void*)> button_callback_t;

	void			addImageAttributeToXML(LLXMLNodePtr node, const LLPointer<LLUIImage>, const std::string& xmlTagName) const;
	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* 	fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	virtual void	setAlpha( F32 alpha ) { mAlpha = alpha; }
	virtual BOOL	handleUnicodeCharHere(llwchar uni_char);
	virtual BOOL	handleKeyHere(KEY key, MASK mask);
	virtual BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	virtual BOOL	handleHover(S32 x, S32 y, MASK mask);
	virtual BOOL	handleRightMouseDown(S32 x, S32 y, MASK mask);
	virtual BOOL	handleRightMouseUp(S32 x, S32 y, MASK mask);	
	virtual BOOL	handleDoubleClick(S32 x, S32 y, MASK mask);
	virtual void	draw();

	virtual void	onMouseCaptureLost();

	virtual void	onCommit();

	void			setUnselectedLabelColor( const LLColor4& c )		{ mUnselectedLabelColor = c; }
	void			setSelectedLabelColor( const LLColor4& c )			{ mSelectedLabelColor = c; }


	boost::signals2::connection setClickedCallback(const CommitCallbackParam& cb);
	boost::signals2::connection setMouseDownCallback(const CommitCallbackParam& cb);
	boost::signals2::connection setMouseUpCallback(const CommitCallbackParam& cb);
	boost::signals2::connection setHeldDownCallback(const CommitCallbackParam& cb);

	boost::signals2::connection setClickedCallback( const commit_signal_t::slot_type& cb ); // mouse down and up within button
	boost::signals2::connection setMouseDownCallback( const commit_signal_t::slot_type& cb );
	boost::signals2::connection setMouseUpCallback( const commit_signal_t::slot_type& cb ); // mouse up, EVEN IF NOT IN BUTTON
	// Passes a 'count' parameter in the commit param payload, i.e. param["count"])
	boost::signals2::connection setHeldDownCallback( const commit_signal_t::slot_type& cb ); // Mouse button held down and in button

	
	// *TODO: Deprecate (for backwards compatability only)
	boost::signals2::connection setClickedCallback( button_callback_t cb, void* data );
	boost::signals2::connection setMouseDownCallback( button_callback_t cb, void* data );
	boost::signals2::connection setMouseUpCallback( button_callback_t cb, void* data );
	boost::signals2::connection setHeldDownCallback( button_callback_t cb, void* data );
		
	void			setHeldDownDelay( F32 seconds, S32 frames = 0)		{ mHeldDownDelay = seconds; mHeldDownFrameDelay = frames; }

	F32				getHeldDownTime() const								{ return mMouseDownTimer.getElapsedTimeF32(); }

	BOOL			toggleState();
	BOOL			getToggleState() const;
	void			setToggleState(BOOL b);

	void			setHighlight(bool b);
	void			setFlashing( BOOL b );
	BOOL			getFlashing() const		{ return mFlashing; }

	void			setHAlign( LLFontGL::HAlign align )		{ mHAlign = align; }
	LLFontGL::HAlign getHAlign() const						{ return mHAlign; }
	void			setLeftHPad( S32 pad )					{ mLeftHPad = pad; }
	S32				getLeftHPad()	const					{ return mLeftHPad; }
	void			setRightHPad( S32 pad )					{ mRightHPad = pad; }
	S32				getRightHPad()	const					{ return mRightHPad; }

	void 			setImageOverlayTopPad( S32 pad )			{ mImageOverlayTopPad = pad; }
	S32 			getImageOverlayTopPad() const				{ return mImageOverlayTopPad; }
	void 			setImageOverlayBottomPad( S32 pad )			{ mImageOverlayBottomPad = pad; }
	S32 			getImageOverlayBottomPad() const			{ return mImageOverlayBottomPad; }

	const std::string	getLabelUnselected() const { return wstring_to_utf8str(mUnselectedLabel); }
	const std::string	getLabelSelected() const { return wstring_to_utf8str(mSelectedLabel); }

	void			setImageColor(const LLColor4& c);
	/*virtual*/ void	setColor(const LLColor4& c);

	void			setImages(const std::string &image_name, const std::string &selected_name);

	void			setDisabledImageColor(const LLColor4& c)		{ mDisabledImageColor = c; }

	void			setDisabledSelectedLabelColor( const LLColor4& c )	{ mDisabledSelectedLabelColor = c; }

	void			setImageOverlay(const std::string& image_name, LLFontGL::HAlign alignment = LLFontGL::HCENTER, const LLColor4& color = LLColor4::white);
	void 			setImageOverlay(const LLUUID& image_id, LLFontGL::HAlign alignment = LLFontGL::HCENTER, const LLColor4& color = LLColor4::white);
	LLPointer<LLUIImage> getImageOverlay() { return mImageOverlay; }
	LLFontGL::HAlign getImageOverlayHAlign() const	{ return mImageOverlayAlignment; }
	
	void            autoResize();	// resize with label of current btn state 
	void            resize(LLUIString label); // resize with label input
	void			setLabel( const LLStringExplicit& label);
	virtual BOOL	setLabelArg( const std::string& key, const LLStringExplicit& text );
	void			setLabelUnselected(const LLStringExplicit& label);
	void			setLabelSelected(const LLStringExplicit& label);
	void			setDisabledLabelColor( const LLColor4& c )		{ mDisabledLabelColor = c; }
	
	void			setFont(const LLFontGL *font)		
		{ mGLFont = ( font ? font : LLFontGL::getFontSansSerif()); }
	const LLFontGL* getFont() const { return mGLFont; }
	const LLUIString&	getCurrentLabel() const;

	void			setScaleImage(BOOL scale)			{ mScaleImage = scale; }
	BOOL			getScaleImage() const				{ return mScaleImage; }

	void			setDropShadowedText(BOOL b)			{ mDropShadowedText = b; }

	void			setBorderEnabled(BOOL b)					{ mBorderEnabled = b; }

	void			setHoverGlowStrength(F32 strength) { mHoverGlowStrength = strength; }

	void			setImageUnselected(LLPointer<LLUIImage> image);
	void			setImageSelected(LLPointer<LLUIImage> image);
	void			setImageHoverSelected(LLPointer<LLUIImage> image);
	void			setImageHoverUnselected(LLPointer<LLUIImage> image);
	void			setImageDisabled(LLPointer<LLUIImage> image);
	void			setImageDisabledSelected(LLPointer<LLUIImage> image);
	void			setImageFlash(LLPointer<LLUIImage> image);

	void			setHelpURLCallback(const std::string &help_url);
	const std::string&	getHelpURL() const { return mHelpURL; }

	void		setForcePressedState(bool b) { mForcePressedState = b; }
	
	void 		setAutoResize(bool auto_resize) { mAutoResize = auto_resize; }

	bool 			getIsToggle() const					{ return mIsToggle; }
	bool 			setIsToggle(bool toggle)			{ return mIsToggle = toggle; }

protected:
	const LLPointer<LLUIImage>&	getImageUnselected() const	{ return mImageUnselected; }
	const LLPointer<LLUIImage>& getImageSelected() const	{ return mImageSelected; }
	void getOverlayImageSize(S32& overlay_width, S32& overlay_height);
	
	LLFrameTimer	mMouseDownTimer;
	bool			mNeedsHighlight;
	S32				mButtonFlashCount;
	F32				mButtonFlashRate;

	void			drawBorder(LLUIImage* imagep, const LLColor4& color, S32 size);
	void			resetMouseDownTimer();

	commit_signal_t* 			mMouseDownSignal;
	commit_signal_t* 			mMouseUpSignal;
	commit_signal_t* 			mHeldDownSignal;
	
	const LLFontGL	*mGLFont;
	
	U64							mMouseDownFrame;
	S32 						mMouseHeldDownCount; 	// Counter for parameter passed to held-down callback
	F32							mHeldDownDelay;			// seconds, after which held-down callbacks get called
	S32							mHeldDownFrameDelay;	// frames, after which held-down callbacks get called

	LLPointer<LLUIImage>	mImageOverlay;
	LLPointer<LLUIImage>	mImageOverlaySelected;
	LLFontGL::HAlign			mImageOverlayAlignment;
	LLUIColor					mImageOverlayColor;
	LLUIColor					mImageOverlaySelectedColor;
	LLUIColor					mImageOverlayDisabledColor;

	LLPointer<LLUIImage>		mImageUnselected;
	LLUIString					mUnselectedLabel;
	LLUIColor					mUnselectedLabelColor;

	LLPointer<LLUIImage>		mImageSelected;
	LLUIString					mSelectedLabel;
	LLUIColor					mSelectedLabelColor;

	LLPointer<LLUIImage>		mImageHoverSelected;

	LLPointer<LLUIImage>		mImageHoverUnselected;

	LLPointer<LLUIImage>		mImageDisabled;
	LLUIColor					mDisabledLabelColor;

	LLPointer<LLUIImage>		mImageDisabledSelected;
	LLUIString					mDisabledSelectedLabel;
	LLUIColor					mDisabledSelectedLabelColor;

	LLPointer<LLUIImage>		mImagePressed;
	LLPointer<LLUIImage>		mImagePressedSelected;

	/* There are two ways an image can flash- by making changes in color according to flash_color attribute
	   or by changing icon from current to the one specified in image_flash. Second way is used only if
	   flash icon name is set in attributes(by default it isn't). First way is used otherwise. */
	LLPointer<LLUIImage>		mImageFlash;

	LLUIColor					mFlashBgColor;

	LLUIColor					mImageColor;
	LLUIColor					mDisabledImageColor;

	bool						mIsToggle;
	bool						mScaleImage;

	bool						mDropShadowedText;
	bool						mAutoResize;
	bool						mBorderEnabled;
	bool						mFlashing;

	LLFontGL::HAlign			mHAlign;
	S32							mLeftHPad;
	S32							mRightHPad;
	S32							mBottomVPad;	// under text label

	S32							mImageOverlayTopPad;
	S32							mImageOverlayBottomPad;

	/*
	 * Space between image_overlay and label
	 */
	S32							mImgOverlayLabelSpace;

	F32							mHoverGlowStrength;
	F32							mCurGlowStrength;

	bool						mCommitOnReturn;
	bool						mFadeWhenDisabled;

	bool						mForcePressedState;
	bool						mDisplayPressedState;

	std::string					mHelpURL;

	LLFrameTimer				mFlashingTimer;

	bool						mHandleRightMouse;
	
	F32							mAlpha;
};

#endif  // LL_LLBUTTON_H
