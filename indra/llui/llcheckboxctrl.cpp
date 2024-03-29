/** 
 * @file llcheckboxctrl.cpp
 * @brief LLCheckBoxCtrl base class
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

// The mutants are coming!

#include "linden_common.h"

#include "llcheckboxctrl.h"

#include "llgl.h"
#include "llui.h"
#include "lluiconstants.h"
#include "lluictrlfactory.h"
#include "llcontrol.h"

#include "llstring.h"
#include "llfontgl.h"
#include "lltextbox.h"
#include "llkeyboard.h"

const U32 MAX_STRING_LENGTH = 10;

static LLRegisterWidget<LLCheckBoxCtrl> r("check_box");

 
LLCheckBoxCtrl::LLCheckBoxCtrl(const std::string& name, const LLRect& rect, 
							    const std::string& label, 
								const LLFontGL* font,
								commit_callback_t commit_callback,
								BOOL initial_value,
								BOOL use_radio_style,
								const std::string& control_which)
:	LLUICtrl(name, rect, TRUE, commit_callback, FOLLOWS_LEFT | FOLLOWS_TOP),
	mTextEnabledColor( LLUI::sColorsGroup->getColor( "LabelTextColor" ) ),
	mTextDisabledColor( LLUI::sColorsGroup->getColor( "LabelDisabledColor" ) ),
	mRadioStyle( use_radio_style ),
	mInitialValue( initial_value ),
	mSetValue( initial_value )
{
	if (font)
	{
		mFont = font;
	}
	else
	{
		mFont = LLFontGL::getFontSansSerifSmall();
	}

	// must be big enough to hold all children
	setUseBoundingRect(TRUE);

	mKeyboardFocusOnClick = TRUE;

	// Label (add a little space to make sure text actually renders)
	const S32 FUDGE = 10;
	S32 text_width = mFont->getWidth( label ) + FUDGE;
	S32 text_height = ll_round(mFont->getLineHeight());
	LLRect label_rect;
	label_rect.setOriginAndSize(
		LLCHECKBOXCTRL_HPAD + LLCHECKBOXCTRL_BTN_SIZE + LLCHECKBOXCTRL_SPACING,
		LLCHECKBOXCTRL_VPAD + 1, // padding to get better alignment
		text_width + LLCHECKBOXCTRL_HPAD,
		text_height );

	// *HACK Get rid of this with SL-55508... 
	// this allows blank check boxes and radio boxes for now
	// Singu Note: Don't do this. Slows rendering down dramatically, and also seems to not fix anything?
	/*std::string local_label = label;
	if(local_label.empty())
	{
		local_label = " ";
	}*/

	mLabel = new LLTextBox( std::string("CheckboxCtrl Label"), label_rect, label, mFont );
	mLabel->setFollowsLeft();
	mLabel->setFollowsBottom();
	addChild(mLabel);

	// Button
	// Note: button cover the label by extending all the way to the right.
	LLRect btn_rect;
	btn_rect.setOriginAndSize(
		LLCHECKBOXCTRL_HPAD,
		LLCHECKBOXCTRL_VPAD,
		LLCHECKBOXCTRL_BTN_SIZE + LLCHECKBOXCTRL_SPACING + text_width + LLCHECKBOXCTRL_HPAD,
		llmax( text_height, LLCHECKBOXCTRL_BTN_SIZE ) + LLCHECKBOXCTRL_VPAD);

	std::string active_true_id		= mRadioStyle ? "UIImgRadioActiveSelectedUUID" : "UIImgCheckboxActiveSelectedUUID";
	std::string active_false_id		= mRadioStyle ? "UIImgRadioActiveUUID" : "UIImgCheckboxActiveUUID";
	std::string inactive_true_id	= mRadioStyle ? "UIImgRadioInactiveSelectedUUID" : "UIImgCheckboxInactiveSelectedUUID";
	std::string inactive_false_id	= mRadioStyle ? "UIImgRadioInactiveUUID" : "UIImgCheckboxInactiveUUID";
	std::string button_name			= mRadioStyle ? "Radio control button" : "Checkbox control button";

	mButton = new LLButton(	button_name, btn_rect,
							active_false_id, active_true_id, control_which,
							boost::bind(&LLCheckBoxCtrl::onButtonPress, this, _2), LLFontGL::getFontSansSerif() ); 

	mButton->setImageDisabledSelected(LLUI::getUIImage(inactive_true_id));
	mButton->setImageDisabled(LLUI::getUIImage(inactive_false_id));
	mButton->setHoverGlowStrength(0.35f);

	mButton->setIsToggle(TRUE);
	mButton->setToggleState( initial_value );
	mButton->setFollowsLeft();
	mButton->setFollowsBottom();
	mButton->setCommitOnReturn(FALSE);
	mButton->setScaleImage(FALSE);
	addChild(mButton);
}

LLCheckBoxCtrl::~LLCheckBoxCtrl()
{
	// Children all cleaned up by default view destructor.
}


// static
void LLCheckBoxCtrl::onButtonPress( const LLSD& data )
{
	//if (mRadioStyle)
	//{
	//	setValue(TRUE);
	//}

	onCommit();
}

void LLCheckBoxCtrl::onCommit()
{
	if( getEnabled() )
	{
		setTentative(FALSE);
		setControlValue(getValue());
		LLUICtrl::onCommit();
	}
}

void LLCheckBoxCtrl::setEnabled(BOOL b)
{
	LLView::setEnabled(b);

	if (b)
	{
		mLabel->setColor( mTextEnabledColor.get() );
	}
	else
	{
		mLabel->setColor( mTextDisabledColor.get() );
	}
}


void LLCheckBoxCtrl::clear()
{
	setValue( FALSE );
}

void LLCheckBoxCtrl::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	//stretch or shrink bounding rectangle of label when rebuilding UI at new scale
	const S32 FUDGE = 10;
	S32 text_width = mFont->getWidth( mLabel->getText() ) + FUDGE;
	S32 text_height = ll_round(mFont->getLineHeight());
	LLRect label_rect;
	label_rect.setOriginAndSize(
		LLCHECKBOXCTRL_HPAD + LLCHECKBOXCTRL_BTN_SIZE + LLCHECKBOXCTRL_SPACING,
		LLCHECKBOXCTRL_VPAD,
		text_width,
		text_height );
	mLabel->setRect(label_rect);

	LLRect btn_rect;
	btn_rect.setOriginAndSize(
		LLCHECKBOXCTRL_HPAD,
		LLCHECKBOXCTRL_VPAD,
		LLCHECKBOXCTRL_BTN_SIZE + LLCHECKBOXCTRL_SPACING + text_width,
		llmax( text_height, LLCHECKBOXCTRL_BTN_SIZE ) );
	mButton->setRect( btn_rect );
	
	LLUICtrl::reshape(width, height, called_from_parent);
}

//virtual
void LLCheckBoxCtrl::setValue(const LLSD& value )
{
	mButton->setValue( value );
}

//virtual
LLSD LLCheckBoxCtrl::getValue() const
{
	return mButton->getValue();
}

//virtual
void LLCheckBoxCtrl::setTentative(BOOL b)
{
	mButton->setTentative(b);
}
//virtual
void LLCheckBoxCtrl::draw()
{
	mButton->setHighlighted(isHighlighted());
	
	mLabel->setHighlighted(isHighlighted());
	
	LLUICtrl::draw();
}

//virtual
BOOL LLCheckBoxCtrl::getTentative() const
{
	return mButton->getTentative();
}

void LLCheckBoxCtrl::setLabel( const LLStringExplicit& label )
{
	mLabel->setText( label );
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
}

std::string LLCheckBoxCtrl::getLabel() const
{
	return mLabel->getText();
}

BOOL LLCheckBoxCtrl::setLabelArg( const std::string& key, const LLStringExplicit& text )
{
	BOOL res = mLabel->setTextArg(key, text);
	reshape(getRect().getWidth(), getRect().getHeight(), FALSE);
	return res;
}

//virtual
std::string LLCheckBoxCtrl::getControlName() const
{
	return mButton->getControlName();
}

// virtual
void LLCheckBoxCtrl::setControlName(const std::string& control_name, LLView* context)
{
	mButton->setControlName(control_name, context);
}


// virtual		Returns TRUE if the user has modified this control.
BOOL	 LLCheckBoxCtrl::isDirty() const
{
	if ( mButton )
	{
		return mButton->isDirty();
	}
	return FALSE;		// Shouldn't get here
}


// virtual			Clear dirty state
void	LLCheckBoxCtrl::resetDirty()
{
	if ( mButton )
	{
		mButton->resetDirty();
	}
}



// virtual
LLXMLNodePtr LLCheckBoxCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_CHECK_BOX_CTRL_TAG);

	node->createChild("label", TRUE)->setStringValue(mLabel->getText());

	std::string control_name = mButton->getControlName();
	
	node->createChild("initial_value", TRUE)->setBoolValue(mInitialValue);

	node->createChild("font", TRUE)->setStringValue(LLFontGL::nameFromFont(mFont));

	node->createChild("radio_style", TRUE)->setBoolValue(mRadioStyle);

	return node;
}

// static
LLView* LLCheckBoxCtrl::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string label("");
	node->getAttributeString("label", label);

	LLFontGL* font = LLView::selectFont(node);

	BOOL radio_style = FALSE;
	node->getAttributeBOOL("radio_style", radio_style);

	if (label.empty())
	{
		label.assign(node->getTextContents());
	}

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLCheckBoxCtrl* checkbox = new LLCheckboxCtrl("checkbox",
		rect, 
		label,
		font,
		NULL,
		FALSE,
		radio_style); // if true, draw radio button style icons

	BOOL initial_value = checkbox->getValue().asBoolean();
	node->getAttributeBOOL("initial_value", initial_value);

	LLColor4 color;
	color = LLUI::sColorsGroup->getColor( "LabelTextColor" );
	LLUICtrlFactory::getAttributeColor(node,"text_enabled_color", color);
	checkbox->setEnabledColor(color);

	color = LLUI::sColorsGroup->getColor( "LabelDisabledColor" );
	LLUICtrlFactory::getAttributeColor(node,"text_disabled_color", color);
	checkbox->setDisabledColor(color);

	checkbox->setValue(initial_value);

	checkbox->initFromXML(node, parent);

	return checkbox;
}
