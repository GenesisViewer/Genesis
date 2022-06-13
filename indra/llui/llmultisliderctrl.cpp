/** 
 * @file llmultisliderctrl.cpp
 * @brief LLMultiSliderCtrl base class
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

#include "linden_common.h"

#include "llmultisliderctrl.h"

#include "llmath.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmultislider.h"
#include "llstring.h"
#include "lltextbox.h"
#include "llui.h"
#include "lluiconstants.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "llresmgr.h"

static LLRegisterWidget<LLMultiSliderCtrl> r("multi_slider");

const U32 MAX_STRING_LENGTH = 10;

 
LLMultiSliderCtrl::LLMultiSliderCtrl(const std::string& name, const LLRect& rect, 
						   const std::string& label,
						   const LLFontGL* font,
						   S32 label_width,
						   S32 text_left,
						   BOOL show_text,
						   BOOL can_edit_text,
						   F32 initial_value, F32 min_value, F32 max_value, F32 increment,
						   S32 max_sliders, BOOL allow_overlap,
						   BOOL draw_track,
						   BOOL use_triangle,
						   const std::string& control_which)
	: LLUICtrl(name, rect, TRUE ),
	  mFont(font),
	  mShowText( show_text ),
	  mCanEditText( can_edit_text ),
	  mPrecision( 3 ),
	  mLabelBox( NULL ),
	  mLabelWidth( label_width ),

	  mEditor( NULL ),
	  mTextBox( NULL ),
	  mTextEnabledColor( LLUI::sColorsGroup->getColor( "LabelTextColor" ) ),
	  mTextDisabledColor( LLUI::sColorsGroup->getColor( "LabelDisabledColor" ) )
{
	S32 top = getRect().getHeight();
	S32 bottom = 0;
	S32 left = 0;

	// Label
	if( !label.empty() )
	{
		if (label_width == 0)
		{
			label_width = font->getWidth(label);
		}
		LLRect label_rect( left, top, label_width, bottom );
		mLabelBox = new LLTextBox( std::string("MultiSliderCtrl Label"), label_rect, label, font );
		addChild(mLabelBox);
	}

	S32 slider_right = getRect().getWidth();
	if( show_text )
	{
		slider_right = text_left - MULTI_SLIDERCTRL_SPACING;
	}

	S32 slider_left = label_width ? label_width + MULTI_SLIDERCTRL_SPACING : 0;
	LLRect slider_rect( slider_left, top, slider_right, bottom );
	mMultiSlider = new LLMultiSlider( 
		std::string("multi_slider"),
		slider_rect, 
		boost::bind(&LLMultiSliderCtrl::onSliderCommit,this,_2), 
		initial_value, min_value, max_value, increment,
		max_sliders, allow_overlap, draw_track,
		use_triangle,
		control_which );
	addChild( mMultiSlider );
	mCurValue = mMultiSlider->getCurSliderValue();
	
	if( show_text )
	{
		LLRect text_rect( text_left, top, getRect().getWidth(), bottom );
		if( can_edit_text )
		{
			mEditor = new LLLineEditor( std::string("MultiSliderCtrl Editor"), text_rect,
				LLStringUtil::null, font,
				MAX_STRING_LENGTH,
				boost::bind(&LLMultiSliderCtrl::onEditorCommit,this,_2),
				NULL,
				NULL,
				boost::bind(&LLLineEditor::prevalidateFloat, _1) );
			mEditor->setFollowsLeft();
			mEditor->setFollowsBottom();
			mEditor->setFocusReceivedCallback( boost::bind(&LLMultiSliderCtrl::onFocusReceived, this) );
			mEditor->setIgnoreTab(TRUE);
			// don't do this, as selecting the entire text is single clicking in some cases
			// and double clicking in others
			//mEditor->setSelectAllonFocusReceived(TRUE);
			addChild(mEditor);
		}
		else
		{
			mTextBox = new LLTextBox( std::string("MultiSliderCtrl Text"), text_rect,	LLStringUtil::null,	font);
			mTextBox->setFollowsLeft();
			mTextBox->setFollowsBottom();
			addChild(mTextBox);
		}
	}

	updateText();
}

LLMultiSliderCtrl::~LLMultiSliderCtrl()
{
	// Children all cleaned up by default view destructor.
}

void LLMultiSliderCtrl::setValue(const LLSD& value)
{
	mMultiSlider->setValue(value);
	mCurValue = mMultiSlider->getCurSliderValue();
	updateText();
}

void LLMultiSliderCtrl::setSliderValue(const std::string& name, F32 v, BOOL from_event)
{
	mMultiSlider->setSliderValue(name, v, from_event );
	mCurValue = mMultiSlider->getCurSliderValue();
	updateText();
}

void LLMultiSliderCtrl::setCurSlider(const std::string& name)
{
	mMultiSlider->setCurSlider(name);
	mCurValue = mMultiSlider->getCurSliderValue();
}

BOOL LLMultiSliderCtrl::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	BOOL res = FALSE;
	if (mLabelBox)
	{
		res = mLabelBox->setTextArg(key, text);
		if (res && mLabelWidth == 0)
		{
			S32 label_width = mFont->getWidth(mLabelBox->getText());
			LLRect rect = mLabelBox->getRect();
			S32 prev_right = rect.mRight;
			rect.mRight = rect.mLeft + label_width;
			mLabelBox->setRect(rect);
				
			S32 delta = rect.mRight - prev_right;
			rect = mMultiSlider->getRect();
			S32 left = rect.mLeft + delta;
			left = llclamp(left, 0, rect.mRight-MULTI_SLIDERCTRL_SPACING);
			rect.mLeft = left;
			mMultiSlider->setRect(rect);
		}
	}
	return res;
}

const std::string& LLMultiSliderCtrl::addSlider()
{
	const std::string& name = mMultiSlider->addSlider();
	
	// if it returns null, pass it on
	if(name == LLStringUtil::null) {
		return LLStringUtil::null;
	}

	// otherwise, update stuff
	mCurValue = mMultiSlider->getCurSliderValue();
	updateText();
	return name;
}

const std::string& LLMultiSliderCtrl::addSlider(F32 val)
{
	const std::string& name = mMultiSlider->addSlider(val);

	// if it returns null, pass it on
	if(name == LLStringUtil::null) {
		return LLStringUtil::null;
	}

	// otherwise, update stuff
	mCurValue = mMultiSlider->getCurSliderValue();
	updateText();
	return name;
}

void LLMultiSliderCtrl::deleteSlider(const std::string& name)
{
	mMultiSlider->deleteSlider(name);
	mCurValue = mMultiSlider->getCurSliderValue();
	updateText();
}


void LLMultiSliderCtrl::clear()
{
	setCurSliderValue(0.0f);
	if( mEditor )
	{
		mEditor->setText(std::string(""));
	}
	if( mTextBox )
	{
		mTextBox->setText(std::string(""));
	}

	// get rid of sliders
	mMultiSlider->clear();

}

BOOL LLMultiSliderCtrl::isMouseHeldDown()
{
	return gFocusMgr.getMouseCapture() == mMultiSlider;
}

void LLMultiSliderCtrl::updateText()
{
	if( mEditor || mTextBox )
	{
		LLLocale locale(LLLocale::USER_LOCALE);

		// Don't display very small negative values as -0.000
		F32 displayed_value = (F32)(floor(getCurSliderValue() * pow(10.0, (F64)mPrecision) + 0.5) / pow(10.0, (F64)mPrecision));

		std::string format = llformat("%%.%df", mPrecision);
		std::string text = llformat(format.c_str(), displayed_value);
		if( mEditor )
		{
			mEditor->setText( text );
		}
		else
		{
			mTextBox->setText( text );
		}
	}
}

void LLMultiSliderCtrl::onEditorCommit(const LLSD& value)
{
	BOOL success = FALSE;
	F32 val = mCurValue;
	F32 saved_val = mCurValue;

	std::string text = value.asString();
	if( LLLineEditor::postvalidateFloat( text ) )
	{
		LLLocale locale(LLLocale::USER_LOCALE);
		val = (F32) atof( text.c_str() );
		if( mMultiSlider->getMinValue() <= val && val <= mMultiSlider->getMaxValue() )
		{
			setCurSliderValue( val );  // set the value temporarily so that the callback can retrieve it.
			if( !mValidateSignal || (*(mValidateSignal))( this, val ) )
			{
				success = TRUE;
			}
		}
	}

	if( success )
	{
		onCommit();
	}
	else
	{
		if( getCurSliderValue() != saved_val )
		{
			setCurSliderValue( saved_val );
		}
		reportInvalidData();		
	}
	updateText();
}

void LLMultiSliderCtrl::onSliderCommit(const LLSD& value)
{
	BOOL success = FALSE;
	F32 saved_val = mCurValue;
	F32 new_val = mMultiSlider->getCurSliderValue();

	mCurValue = new_val;  // set the value temporarily so that the callback can retrieve it.
	if( !mValidateSignal || (*(mValidateSignal))( this, new_val ) )
	{
			success = TRUE;
	}

	if( success )
	{
		onCommit();
	}
	else
	{
		if( mCurValue != saved_val )
		{
			setCurSliderValue( saved_val );
		}
		reportInvalidData();		
	}
	updateText();
}

void LLMultiSliderCtrl::setEnabled(BOOL b)
{
	LLUICtrl::setEnabled( b );

	if( mLabelBox )
	{
		mLabelBox->setColor( b ? mTextEnabledColor : mTextDisabledColor );
	}

	mMultiSlider->setEnabled( b );

	if( mEditor )
	{
		mEditor->setEnabled( b );
	}

	if( mTextBox )
	{
		mTextBox->setColor( b ? mTextEnabledColor : mTextDisabledColor );
	}
}


void LLMultiSliderCtrl::setTentative(BOOL b)
{
	if( mEditor )
	{
		mEditor->setTentative(b);
	}
	LLUICtrl::setTentative(b);
}


void LLMultiSliderCtrl::onCommit()
{
	setTentative(FALSE);

	if( mEditor )
	{
		mEditor->setTentative(FALSE);
	}

	LLUICtrl::onCommit();
}


void LLMultiSliderCtrl::setPrecision(S32 precision)
{
	if (precision < 0 || precision > 10)
	{
		LL_ERRS() << "LLMultiSliderCtrl::setPrecision - precision out of range" << LL_ENDL;
		return;
	}

	mPrecision = precision;
	updateText();
}

boost::signals2::connection LLMultiSliderCtrl::setSliderMouseDownCallback( const commit_signal_t::slot_type& cb )
{
	return mMultiSlider->setMouseDownCallback( cb );
}

boost::signals2::connection LLMultiSliderCtrl::setSliderMouseUpCallback( const commit_signal_t::slot_type& cb )
{
	return mMultiSlider->setMouseUpCallback( cb );
}

void LLMultiSliderCtrl::onTabInto()
{
	if( mEditor )
	{
		mEditor->onTabInto(); 
	}
}

void LLMultiSliderCtrl::reportInvalidData()
{
	make_ui_sound("UISndBadKeystroke");
}

//virtual
std::string LLMultiSliderCtrl::getControlName() const
{
	return mMultiSlider->getControlName();
}

// virtual
void LLMultiSliderCtrl::setControlName(const std::string& control_name, LLView* context)
{
	mMultiSlider->setControlName(control_name, context);
}

// virtual
LLXMLNodePtr LLMultiSliderCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_MULTI_SLIDER_CTRL_TAG);

	node->createChild("show_text", TRUE)->setBoolValue(mShowText);

	node->createChild("can_edit_text", TRUE)->setBoolValue(mCanEditText);

	node->createChild("decimal_digits", TRUE)->setIntValue(mPrecision);

	if (mLabelBox)
	{
		node->createChild("label", TRUE)->setStringValue(mLabelBox->getText());
	}

	// TomY TODO: Do we really want to export the transient state of the slider?
	node->createChild("value", TRUE)->setFloatValue(mCurValue);

	if (mMultiSlider)
	{
		node->createChild("initial_val", TRUE)->setFloatValue(mMultiSlider->getInitialValue());
		node->createChild("min_val", TRUE)->setFloatValue(mMultiSlider->getMinValue());
		node->createChild("max_val", TRUE)->setFloatValue(mMultiSlider->getMaxValue());
		node->createChild("increment", TRUE)->setFloatValue(mMultiSlider->getIncrement());
	}
	addColorXML(node, mTextEnabledColor, "text_enabled_color", "LabelTextColor");
	addColorXML(node, mTextDisabledColor, "text_disabled_color", "LabelDisabledColor");

	return node;
}

LLView* LLMultiSliderCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string label;
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLFontGL* font = LLView::selectFont(node);

	// HACK: Font might not be specified.
	if (!font)
	{
		font = LLFontGL::getFontSansSerifSmall();
	}

	S32 label_width = 0;
	node->getAttributeS32("label_width", label_width);

	BOOL show_text = TRUE;
	node->getAttributeBOOL("show_text", show_text);

	BOOL can_edit_text = FALSE;
	node->getAttributeBOOL("can_edit_text", can_edit_text);
	
	BOOL allow_overlap = FALSE;
	node->getAttributeBOOL("allow_overlap", allow_overlap);

	BOOL draw_track = TRUE;
	node->getAttributeBOOL("draw_track", draw_track);

	BOOL use_triangle = FALSE;
	node->getAttributeBOOL("use_triangle", use_triangle);

	F32 initial_value = 0.f;
	node->getAttributeF32("initial_val", initial_value);

	F32 min_value = 0.f;
	node->getAttributeF32("min_val", min_value);

	F32 max_value = 1.f; 
	node->getAttributeF32("max_val", max_value);

	F32 increment = 0.1f;
	node->getAttributeF32("increment", increment);

	U32 precision = 3;
	node->getAttributeU32("decimal_digits", precision);

	S32 max_sliders = 1;
	node->getAttributeS32("max_sliders", max_sliders);

	S32 value_width = 0;
	node->getAttributeS32("val_width", value_width);

	S32 text_left = 0;
	if (show_text)
	{
		if(value_width > 0)	
		{
			//Fixed width. Be wary of precision and sign causing text to take more space than expected!
			text_left = value_width;
		}
		else
		{
			// calculate the size of the text box (log max_value is number of digits - 1 so plus 1)
			if ( max_value )
				text_left = font->getWidth(std::string("0")) * ( static_cast < S32 > ( log10  ( max_value ) ) + precision + 1 );

			if ( increment < 1.0f )
				text_left += font->getWidth(std::string("."));	// (mostly) take account of decimal point in value

			if ( min_value < 0.0f || max_value < 0.0f )
				text_left += font->getWidth(std::string("-"));	// (mostly) take account of minus sign 

			// padding to make things look nicer
			text_left += 8;
		}
	}

	if (label.empty())
	{
		label.assign(node->getTextContents());
	}

	LLMultiSliderCtrl* slider = new LLMultiSliderCtrl("multi_slider",
							rect,
							label,
							font,
							label_width,
							rect.getWidth() - text_left,
							show_text,
							can_edit_text,
							initial_value,
							min_value, 
							max_value,
							increment,
							max_sliders,
							allow_overlap,
							draw_track,
							use_triangle);

	slider->setPrecision(precision);

	slider->initFromXML(node, parent);

	slider->updateText();
	
	return slider;
}
