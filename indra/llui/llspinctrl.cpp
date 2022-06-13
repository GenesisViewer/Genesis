/** 
 * @file llspinctrl.cpp
 * @brief LLSpinCtrl base class
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

#include "linden_common.h"
 
#include "llspinctrl.h"

#include "llgl.h"
#include "llui.h"
#include "lluiconstants.h"

#include "llstring.h"
#include "llfontgl.h"
#include "lllineeditor.h"
#include "llbutton.h"
#include "lltextbox.h"
#include "llkeyboard.h"
#include "llmath.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "llresmgr.h"

const U32 MAX_STRING_LENGTH = 32;

static LLRegisterWidget<LLSpinCtrl> r2("spinner");
 
LLSpinCtrl::LLSpinCtrl(	const std::string& name, const LLRect& rect, const std::string& label, const LLFontGL* font,
	commit_callback_t commit_callback,
	F32 initial_value, F32 min_value, F32 max_value, F32 increment,
	const std::string& control_name,
	S32 label_width)
	:
	LLUICtrl(name, rect, TRUE, commit_callback, FOLLOWS_LEFT | FOLLOWS_TOP ),
	mValue( initial_value ),
	mInitialValue( initial_value ),
	mMaxValue( max_value ),
	mMinValue( min_value ),
	mIncrement( increment ),
	mPrecision( 3 ),
	mLabelBox( NULL ),
	mTextEnabledColor( LLUI::sColorsGroup->getColor( "LabelTextColor" ) ),
	mTextDisabledColor( LLUI::sColorsGroup->getColor( "LabelDisabledColor" ) ),
	mbHasBeenSet( FALSE )
{
	S32 top = getRect().getHeight();
	S32 bottom = top - 2 * SPINCTRL_BTN_HEIGHT;
	S32 centered_top = top;
	S32 centered_bottom = bottom;
	S32 btn_left = 0;

	// Label
	if( !label.empty() )
	{
		LLRect label_rect( 0, centered_top, label_width, centered_bottom );
		mLabelBox = new LLTextBox( std::string("SpinCtrl Label"), label_rect, label, font );
		addChild(mLabelBox);

		btn_left += label_rect.mRight + SPINCTRL_SPACING;
	}

	S32 btn_right = btn_left + SPINCTRL_BTN_WIDTH;
	
	// Spin buttons
	LLRect up_rect( btn_left, top, btn_right, top - SPINCTRL_BTN_HEIGHT );
	std::string out_id = "UIImgBtnSpinUpOutUUID";
	std::string in_id = "UIImgBtnSpinUpInUUID";
	mUpBtn = new LLButton(std::string("SpinCtrl Up"), up_rect,
								   out_id,
								   in_id,
								   LLStringUtil::null,
								   boost::bind(&LLSpinCtrl::onUpBtn, this, _2), LLFontGL::getFontSansSerif() );
	mUpBtn->setFollowsLeft();
	mUpBtn->setFollowsBottom();
	mUpBtn->setHeldDownCallback(boost::bind(&LLSpinCtrl::onUpBtn,this, _2));
	mUpBtn->setTabStop(FALSE);
	addChild(mUpBtn);

	LLRect down_rect( btn_left, top - SPINCTRL_BTN_HEIGHT, btn_right, bottom );
	out_id = "UIImgBtnSpinDownOutUUID";
	in_id = "UIImgBtnSpinDownInUUID";
	mDownBtn = new LLButton(std::string("SpinCtrl Down"), down_rect,
							out_id,
							in_id,
							LLStringUtil::null,
							boost::bind(&LLSpinCtrl::onDownBtn, this, _2), LLFontGL::getFontSansSerif() );
	mDownBtn->setFollowsLeft();
	mDownBtn->setFollowsBottom();
	mDownBtn->setHeldDownCallback(boost::bind(&LLSpinCtrl::onDownBtn,this, _2));
	mDownBtn->setTabStop(FALSE);
	addChild(mDownBtn);

	LLRect editor_rect( btn_right + 1, centered_top, getRect().getWidth(), centered_bottom );
	mEditor = new LLLineEditor( std::string("SpinCtrl Editor"), editor_rect, LLStringUtil::null, font,
								MAX_STRING_LENGTH,
								boost::bind(&LLSpinCtrl::onEditorCommit, this, _2), NULL, NULL,
								&LLLineEditor::prevalidateASCII );
	mEditor->setFollowsLeft();
	mEditor->setFollowsBottom();
	mEditor->setFocusReceivedCallback( boost::bind(&LLSpinCtrl::onFocusReceived, this) );
	//RN: this seems to be a BAD IDEA, as it makes the editor behavior different when it has focus
	// than when it doesn't.  Instead, if you always have to double click to select all the text, 
	// it's easier to understand
	//mEditor->setSelectAllonFocusReceived(TRUE);
	mEditor->setIgnoreTab(TRUE);
	mEditor->setSelectAllonCommit(FALSE);
	addChild(mEditor);

	updateEditor();
	setUseBoundingRect( TRUE );
}


F32 clamp_precision(F32 value, S32 decimal_precision)
{
	// pow() isn't perfect
	
	F64 clamped_value = value;
	for (S32 i = 0; i < decimal_precision; i++)
		clamped_value *= 10.0;

	clamped_value = ll_round((F32)clamped_value);

	for (S32 i = 0; i < decimal_precision; i++)
		clamped_value /= 10.0;
	
	return (F32)clamped_value;
}


F32 get_increment(F32 inc, S32 decimal_precision) //CF: finetune increments
{
	if(gKeyboard->getKeyDown(KEY_ALT))
		inc = inc * 10.f;
	else if(gKeyboard->getKeyDown(KEY_CONTROL)) {
		if (ll_round(inc * 1000.f) == 25) // 0.025 gets 0.05 here
			inc = inc * 0.2f;
		else
			inc = inc * 0.1f;
	}
	else if(gKeyboard->getKeyDown(KEY_SHIFT)) {
		if (decimal_precision == 2 && ll_round(inc) == 1) // for rotations, finest step is 0.05
			inc = inc * 0.05f;
		else
			inc = inc * 0.01f;
	}

	if (decimal_precision == 1 && inc < 0.1f) // clamp inc to precision
		inc = 0.1f;
	else if (decimal_precision == 2 && inc < 0.01f)
		inc = 0.01f;
	else if (decimal_precision == 3 && inc < 0.001f)
		inc = 0.001f;

	return inc;
}


// static
void LLSpinCtrl::onUpBtn( const LLSD& data )
{
	if( getEnabled() )
	{
		// use getValue()/setValue() to force reload from/to control
		F32 val = (F32)getValue().asReal() + get_increment(mIncrement, mPrecision);
		val = clamp_precision(val, mPrecision);
		val = llmin( val, mMaxValue );
		if (val < mMinValue) val = mMinValue;
		if (val > mMaxValue) val = mMaxValue;
		
		F32 saved_val = (F32)getValue().asReal();
		setValue(val);
		if( mValidateSignal && !(*mValidateSignal)( this, val ) )
		{
			setValue( saved_val );
			reportInvalidData();
			updateEditor();
			return;
		}

		updateEditor();
		onCommit();
	}
}


void LLSpinCtrl::onDownBtn( const LLSD& data )
{
	if( getEnabled() )
	{
		F32 val = (F32)getValue().asReal() - get_increment(mIncrement, mPrecision);
		val = clamp_precision(val, mPrecision);
		val = llmax( val, mMinValue );


		if (val < mMinValue) val = mMinValue;
		if (val > mMaxValue) val = mMaxValue;

		F32 saved_val = (F32)getValue().asReal();
		setValue(val);
		if( mValidateSignal && !(*mValidateSignal)( this, val ) )
		{
			setValue( saved_val );
			reportInvalidData();
			updateEditor();
			return;
		}
		
		updateEditor();
		onCommit();
	}
}

void LLSpinCtrl::setValue(const LLSD& value )
{
	F32 v = (F32)value.asReal();
	if (mValue != v || !mbHasBeenSet)
	{
		mbHasBeenSet = TRUE;
		mValue = v;
		
		if (!mEditor->hasFocus())
		{
			updateEditor();
		}
	}
}

//no matter if Editor has the focus, update the value
void LLSpinCtrl::forceSetValue(const LLSD& value )
{
	F32 v = (F32)value.asReal();
	if (mValue != v || !mbHasBeenSet)
	{
		mbHasBeenSet = TRUE;
		mValue = v;
		
		updateEditor();
		mEditor->resetScrollPosition();
	}
}

void LLSpinCtrl::clear()
{
	setValue(mMinValue);
	mEditor->clear();
	mbHasBeenSet = FALSE;
}

void LLSpinCtrl::updateLabelColor()
{
	if( mLabelBox )
	{
		mLabelBox->setColor( getEnabled() ? mTextEnabledColor : mTextDisabledColor );
	}
}

void LLSpinCtrl::updateEditor()
{
	LLLocale locale(LLLocale::USER_LOCALE);

	// Don't display very small negative values as -0.000
	F32 displayed_value = clamp_precision((F32)getValue().asReal(), mPrecision);

//	if( S32( displayed_value * pow( 10, mPrecision ) ) == 0 )
//	{
//		displayed_value = 0.f;
//	}

	std::string format = llformat("%%.%df", mPrecision);
	std::string text = llformat(format.c_str(), displayed_value);
	mEditor->setText( text );
}

void LLSpinCtrl::onEditorCommit( const LLSD& data )
{
	BOOL success = FALSE;
	
	if( mEditor->evaluateFloat() )
	{
		std::string text = mEditor->getText();
		
		LLLocale locale(LLLocale::USER_LOCALE);
		F32 val = (F32) atof(text.c_str());

		if (val < mMinValue) val = mMinValue;
		if (val > mMaxValue) val = mMaxValue;

		F32 saved_val = mValue;
		mValue = val;
		if( !mValidateSignal || (*mValidateSignal)( this, val ) )
		{
			success = TRUE;
			onCommit();
		}
		else
		{
			mValue = saved_val;
		}
	}
	updateEditor();

	if( success )
	{
		// We committed and clamped value
		// try to display as much as possible
		mEditor->resetScrollPosition();
	}
	else
	{
		reportInvalidData();		
	}
}


void LLSpinCtrl::forceEditorCommit()
{
	onEditorCommit( LLSD() );
}


void LLSpinCtrl::setFocus(BOOL b)
{
	LLUICtrl::setFocus( b );
	mEditor->setFocus( b );
}

void LLSpinCtrl::setEnabled(BOOL b)
{
	LLView::setEnabled( b );
	mEditor->setEnabled( b );
	updateLabelColor();
}


void LLSpinCtrl::setTentative(BOOL b)
{
	mEditor->setTentative(b);
	LLUICtrl::setTentative(b);
}


BOOL LLSpinCtrl::isMouseHeldDown() const
{
	return 
		mDownBtn->hasMouseCapture()
		|| mUpBtn->hasMouseCapture();
}

void LLSpinCtrl::onCommit()
{
	setTentative(FALSE);
	setControlValue(mValue);
	LLUICtrl::onCommit();
}


void LLSpinCtrl::setPrecision(S32 precision)
{
	if (precision < 0 || precision > 10)
	{
		LL_ERRS() << "LLSpinCtrl::setPrecision - precision out of range" << LL_ENDL;
		return;
	}

	mPrecision = precision;
	updateEditor();
}

void LLSpinCtrl::setLabel(const LLStringExplicit& label)
{
	if (mLabelBox)
	{
		mLabelBox->setText(label);
	}
	else
	{
		LL_WARNS() << "Attempting to set label on LLSpinCtrl constructed without one " << getName() << LL_ENDL;
	}
	updateLabelColor();
}

BOOL LLSpinCtrl::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	if (mLabelBox)
	{
		BOOL res = mLabelBox->setTextArg(key, text);
		reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
		return res;
	}
	return FALSE;
}


void LLSpinCtrl::setAllowEdit(BOOL allow_edit)
{
	mEditor->setEnabled(allow_edit);
	mAllowEdit = allow_edit;
}

void LLSpinCtrl::onTabInto()
{
	mEditor->onTabInto(); 
}


void LLSpinCtrl::reportInvalidData()
{
	make_ui_sound("UISndBadKeystroke");
}

BOOL LLSpinCtrl::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if( clicks > 0 )
	{
		while( clicks-- )
		{
			onDownBtn(getValue());
		}
	}
	else
	while( clicks++ )
	{
		onUpBtn(getValue());
	}

	return TRUE;
}

BOOL LLSpinCtrl::handleKeyHere(KEY key, MASK mask)
{
	if (mEditor->hasFocus())
	{
		if (key == KEY_ESCAPE && mask == MASK_NONE)
		{
			// text editors don't support revert normally (due to user confusion)
			// but not allowing revert on a spinner seems dangerous
			updateEditor();
			mEditor->resetScrollPosition();
			mEditor->setFocus(FALSE);
			return TRUE;
		}
		if(key == KEY_UP)
		{
			onUpBtn(getValue());
			return TRUE;
		}
		if(key == KEY_DOWN)
		{
			onDownBtn(getValue());
			return TRUE;
		}
		if(key == KEY_RETURN)
		{
			forceEditorCommit();
			return TRUE;
		}
	}
	return FALSE;
}

// virtual
LLXMLNodePtr LLSpinCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SPIN_CTRL_TAG);

	node->createChild("decimal_digits", TRUE)->setIntValue(mPrecision);

	if (mLabelBox)
	{
		node->createChild("label", TRUE)->setStringValue(mLabelBox->getText());

		node->createChild("label_width", TRUE)->setIntValue(mLabelBox->getRect().getWidth());
	}

	node->createChild("initial_val", TRUE)->setFloatValue(mInitialValue);

	node->createChild("min_val", TRUE)->setFloatValue(mMinValue);

	node->createChild("max_val", TRUE)->setFloatValue(mMaxValue);

	node->createChild("increment", TRUE)->setFloatValue(mIncrement);
	
	addColorXML(node, mTextEnabledColor, "text_enabled_color", "LabelTextColor");
	addColorXML(node, mTextDisabledColor, "text_disabled_color", "LabelDisabledColor");

	return node;
}

LLView* LLSpinCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string label;
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLFontGL* font = LLView::selectFont(node);

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
	
	S32 label_width = llmin(40, rect.getWidth() - 40);
	node->getAttributeS32("label_width", label_width);

	BOOL allow_text_entry = TRUE;
	node->getAttributeBOOL("allow_text_entry", allow_text_entry);

	if(label.empty())
	{
		label.assign( node->getValue() );
	}

	LLSpinCtrl* spinner = new LLSpinCtrl("spinner",
							rect,
							label,
							font,
							NULL,
							initial_value, 
							min_value, 
							max_value, 
							increment,
							LLStringUtil::null,
							label_width);

	spinner->setPrecision(precision);

	spinner->initFromXML(node, parent);
	spinner->setAllowEdit(allow_text_entry);

	return spinner;
}

