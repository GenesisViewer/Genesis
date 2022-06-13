/** 
 * @file llscrollbar.cpp
 * @brief Scrollbar UI widget
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llscrollbar.h"

#include "llmath.h"
#include "lltimer.h"
#include "v3color.h"

#include "llbutton.h"
#include "llcriticaldamp.h"
#include "llkeyboard.h"
#include "llui.h"
#include "llfocusmgr.h"
#include "llwindow.h"
#include "llcontrol.h"
#include "llrender.h"
#include "lluictrlfactory.h"

LLScrollbar::LLScrollbar(
		const std::string& name, LLRect rect,
		LLScrollbar::ORIENTATION orientation,
		S32 doc_size, S32 doc_pos, S32 page_size,
		callback_t change_callback,
		S32 step_size)
:		LLUICtrl( name, rect ),

		mChangeCallback( change_callback ),
		mOrientation( orientation ),
		mDocSize( doc_size ),
		mDocPos( doc_pos ),
		mPageSize( page_size ),
		mStepSize( step_size ),
		mDocChanged(FALSE),
		mDragStartX( 0 ),
		mDragStartY( 0 ),
		mHoverGlowStrength(0.15f),
		mCurGlowStrength(0.f),
		mTrackColor( LLUI::sColorsGroup->getColor("ScrollbarTrackColor") ),
		mThumbColor ( LLUI::sColorsGroup->getColor("ScrollbarThumbColor") ),
		mHighlightColor ( LLUI::sColorsGroup->getColor("DefaultHighlightLight") ),
		mShadowColor ( LLUI::sColorsGroup->getColor("DefaultShadowLight") ),
		mOnScrollEndCallback( NULL ),
		mOnScrollEndData( NULL ),
		mThickness( SCROLLBAR_SIZE )
{
	//llassert( 0 <= mDocSize );
	//llassert( 0 <= mDocPos && mDocPos <= mDocSize );
	
	setTabStop(FALSE);
	updateThumbRect();
	
	// Page up and page down buttons
	LLRect line_up_rect;
	std::string line_up_img;
	std::string line_up_selected_img;
	std::string line_down_img;
	std::string line_down_selected_img;

	LLRect line_down_rect;

	if( LLScrollbar::VERTICAL == mOrientation )
	{
		line_up_rect.setLeftTopAndSize( 0, getRect().getHeight(), mThickness, mThickness );
		line_up_img="UIImgBtnScrollUpOutUUID";
		line_up_selected_img="UIImgBtnScrollUpInUUID";

		line_down_rect.setOriginAndSize( 0, 0, mThickness, mThickness );
		line_down_img="UIImgBtnScrollDownOutUUID";
		line_down_selected_img="UIImgBtnScrollDownInUUID";
	}
	else
	{
		// Horizontal
		line_up_rect.setOriginAndSize( 0, 0, mThickness, mThickness );
		line_up_img="UIImgBtnScrollLeftOutUUID";
		line_up_selected_img="UIImgBtnScrollLeftInUUID";

		line_down_rect.setOriginAndSize( getRect().getWidth() - mThickness, 0, mThickness, mThickness );
		line_down_img="UIImgBtnScrollRightOutUUID";
		line_down_selected_img="UIImgBtnScrollRightInUUID";
	}

	LLButton* line_up_btn = new LLButton(std::string("Line Up"), line_up_rect,
										 line_up_img, line_up_selected_img, LLStringUtil::null,
										 boost::bind(&LLScrollbar::onLineUpBtnPressed, this, _2), LLFontGL::getFontSansSerif() );
	if( LLScrollbar::VERTICAL == mOrientation )
	{
		line_up_btn->setFollowsRight();
		line_up_btn->setFollowsTop();
	}
	else
	{
		// horizontal
		line_up_btn->setFollowsLeft();
		line_up_btn->setFollowsBottom();
	}
	line_up_btn->setHeldDownCallback( boost::bind(&LLScrollbar::onLineUpBtnPressed, this, _2) );
	line_up_btn->setTabStop(FALSE);
	line_up_btn->setScaleImage(TRUE);

	addChild(line_up_btn);

	LLButton* line_down_btn = new LLButton(std::string("Line Down"), line_down_rect,
										   line_down_img, line_down_selected_img, LLStringUtil::null,
										   boost::bind(&LLScrollbar::onLineDownBtnPressed, this, _2), LLFontGL::getFontSansSerif() );
	line_down_btn->setFollowsRight();
	line_down_btn->setFollowsBottom();
	line_down_btn->setHeldDownCallback( boost::bind(&LLScrollbar::onLineDownBtnPressed, this, _2) );
	line_down_btn->setTabStop(FALSE);
	line_down_btn->setScaleImage(TRUE);
	addChild(line_down_btn);
}


LLScrollbar::~LLScrollbar()
{
	// Children buttons killed by parent class
}

void LLScrollbar::setDocParams( S32 size, S32 pos )
{
	mDocSize = size;
	setDocPos(pos);
	mDocChanged = TRUE;

	updateThumbRect();
}

// returns true if document position really changed
bool LLScrollbar::setDocPos(S32 pos, BOOL update_thumb)
{
	pos = llclamp(pos, 0, getDocPosMax());
	if (pos != mDocPos)
	{
		mDocPos = pos;
		mDocChanged = TRUE;

		if( mChangeCallback )
		{
			mChangeCallback( mDocPos, this );
		}

		if( update_thumb )
		{
			updateThumbRect();
		}
		return true;
	}
	return false;
}

void LLScrollbar::setDocSize(S32 size)
{
	if (size != mDocSize)
	{
		mDocSize = size;
		setDocPos(mDocPos);
		mDocChanged = TRUE;

		updateThumbRect();
	}
}

void LLScrollbar::setPageSize( S32 page_size )
{
	if (page_size != mPageSize)
	{
		mPageSize = page_size;
		setDocPos(mDocPos);
		mDocChanged = TRUE;

		updateThumbRect();
	}
}

BOOL LLScrollbar::isAtBeginning()
{
	return mDocPos == 0;
}

BOOL LLScrollbar::isAtEnd()
{
	return mDocPos == getDocPosMax();
}


void LLScrollbar::updateThumbRect()
{
//	llassert( 0 <= mDocSize );
//	llassert( 0 <= mDocPos && mDocPos <= getDocPosMax() );
	
	const S32 THUMB_MIN_LENGTH = 16;

	S32 window_length = (mOrientation == LLScrollbar::HORIZONTAL) ? getRect().getWidth() : getRect().getHeight();
	S32 thumb_bg_length = llmax(0, window_length - 2 * mThickness);
	S32 visible_lines = llmin( mDocSize, mPageSize );
	S32 thumb_length = mDocSize ? llmin(llmax( visible_lines * thumb_bg_length / mDocSize, THUMB_MIN_LENGTH), thumb_bg_length) : thumb_bg_length;

	S32 variable_lines = mDocSize - visible_lines;

	if( mOrientation == LLScrollbar::VERTICAL )
	{ 
		S32 thumb_start_max = thumb_bg_length + mThickness;
		S32 thumb_start_min = mThickness + THUMB_MIN_LENGTH;
		S32 thumb_start = variable_lines ? llmin( llmax(thumb_start_max - (mDocPos * (thumb_bg_length - thumb_length)) / variable_lines, thumb_start_min), thumb_start_max ) : thumb_start_max;

		mThumbRect.mLeft =  0;
		mThumbRect.mTop = thumb_start;
		mThumbRect.mRight = mThickness;
		mThumbRect.mBottom = thumb_start - thumb_length;
	}
	else
	{
		// Horizontal
		S32 thumb_start_max = thumb_bg_length + mThickness - thumb_length;
		S32 thumb_start_min = mThickness;
		S32 thumb_start = variable_lines ? llmin(llmax( thumb_start_min + (mDocPos * (thumb_bg_length - thumb_length)) / variable_lines, thumb_start_min), thumb_start_max ) : thumb_start_min;
	
		mThumbRect.mLeft = thumb_start;
		mThumbRect.mTop = mThickness;
		mThumbRect.mRight = thumb_start + thumb_length;
		mThumbRect.mBottom = 0;
	}
	
	if (mOnScrollEndCallback && mOnScrollEndData && (mDocPos == getDocPosMax()))
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}
}

BOOL LLScrollbar::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Check children first
	BOOL handled_by_child = LLView::childrenHandleMouseDown(x, y, mask) != NULL;
	if( !handled_by_child )
	{
		if( mThumbRect.pointInRect(x,y) )
		{
			// Start dragging the thumb
			// No handler needed for focus lost since this clas has no state that depends on it.
			gFocusMgr.setMouseCapture( this );  
			mDragStartX = x;
			mDragStartY = y;
			mOrigRect.mTop = mThumbRect.mTop;
			mOrigRect.mBottom = mThumbRect.mBottom;
			mOrigRect.mLeft = mThumbRect.mLeft;
			mOrigRect.mRight = mThumbRect.mRight;
			mLastDelta = 0;
		}
		else
		{
			if( 
				( (LLScrollbar::VERTICAL == mOrientation) && (mThumbRect.mTop < y) ) ||
				( (LLScrollbar::HORIZONTAL == mOrientation) && (x < mThumbRect.mLeft) )
			)
			{
				// Page up
				pageUp(0);
			}
			else
			if(
				( (LLScrollbar::VERTICAL == mOrientation) && (y < mThumbRect.mBottom) ) ||
				( (LLScrollbar::HORIZONTAL == mOrientation) && (mThumbRect.mRight < x) )
			)
			{
				// Page down
				pageDown(0);
			}
		}
	}

	return TRUE;
}


BOOL LLScrollbar::handleHover(S32 x, S32 y, MASK mask)
{
	// Note: we don't bother sending the event to the children (the arrow buttons)
	// because they'll capture the mouse whenever they need hover events.
	
	BOOL handled = FALSE;
	if( hasMouseCapture() )
	{
		S32 height = getRect().getHeight();
		S32 width = getRect().getWidth();

		if( VERTICAL == mOrientation )
		{
//			S32 old_pos = mThumbRect.mTop;

			S32 delta_pixels = y - mDragStartY;
			if( mOrigRect.mBottom + delta_pixels < mThickness )
			{
				delta_pixels = mThickness - mOrigRect.mBottom - 1;
			}
			else
			if( mOrigRect.mTop + delta_pixels > height - mThickness )
			{
				delta_pixels = height - mThickness - mOrigRect.mTop + 1;
			}

			mThumbRect.mTop = mOrigRect.mTop + delta_pixels;
			mThumbRect.mBottom = mOrigRect.mBottom + delta_pixels;

			S32 thumb_length = mThumbRect.getHeight();
			S32 thumb_track_length = height - 2 * mThickness;


			if( delta_pixels != mLastDelta || mDocChanged)
			{	
				// Note: delta_pixels increases as you go up.  mDocPos increases down (line 0 is at the top of the page).
				S32 usable_track_length = thumb_track_length - thumb_length;
				if( 0 < usable_track_length )
				{
					S32 variable_lines = getDocPosMax();
					S32 pos = mThumbRect.mTop;
					F32 ratio = F32(pos - mThickness - thumb_length) / usable_track_length;	
	
					S32 new_pos = llclamp( S32(variable_lines - ratio * variable_lines + 0.5f), 0, variable_lines );
					// Note: we do not call updateThumbRect() here.  Instead we let the thumb and the document go slightly
					// out of sync (less than a line's worth) to make the thumb feel responsive.
					changeLine( new_pos - mDocPos, FALSE );
				}
			}

			mLastDelta = delta_pixels;
		
		}
		else
		{
			// Horizontal
//			S32 old_pos = mThumbRect.mLeft;

			S32 delta_pixels = x - mDragStartX;

			if( mOrigRect.mLeft + delta_pixels < mThickness )
			{
				delta_pixels = mThickness - mOrigRect.mLeft - 1;
			}
			else
			if( mOrigRect.mRight + delta_pixels > width - mThickness )
			{
				delta_pixels = width - mThickness - mOrigRect.mRight + 1;
			}

			mThumbRect.mLeft = mOrigRect.mLeft + delta_pixels;
			mThumbRect.mRight = mOrigRect.mRight + delta_pixels;
			
			S32 thumb_length = mThumbRect.getWidth();
			S32 thumb_track_length = width - 2 * mThickness;

			if( delta_pixels != mLastDelta || mDocChanged)
			{	
				// Note: delta_pixels increases as you go up.  mDocPos increases down (line 0 is at the top of the page).
				S32 usable_track_length = thumb_track_length - thumb_length;
				if( 0 < usable_track_length )
				{
					S32 variable_lines = getDocPosMax();
					S32 pos = mThumbRect.mLeft;
					F32 ratio = F32(pos - mThickness) / usable_track_length;	
	
					S32 new_pos = llclamp( S32(ratio * variable_lines + 0.5f), 0, variable_lines);
	
					// Note: we do not call updateThumbRect() here.  Instead we let the thumb and the document go slightly
					// out of sync (less than a line's worth) to make the thumb feel responsive.
					changeLine( new_pos - mDocPos, FALSE );
				}
			}

			mLastDelta = delta_pixels;
		}

		getWindow()->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << " (active)" << LL_ENDL;
		handled = TRUE;
	}
	else
	{
		handled = childrenHandleHover( x, y, mask ) != NULL;
	}

	// Opaque
	if( !handled )
	{
		getWindow()->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << " (inactive)"  << LL_ENDL;
		handled = TRUE;
	}

	mDocChanged = FALSE;
	return handled;
} // end handleHover


BOOL LLScrollbar::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	BOOL handled = changeLine( clicks * mStepSize, TRUE );
	return handled;
}

BOOL LLScrollbar::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									EDragAndDropType cargo_type, void *cargo_data, EAcceptance *accept, std::string &tooltip_msg)
{
	// enable this to get drag and drop to control scrollbars
	//if (!drop)
	//{
	//	//TODO: refactor this
	//	S32 variable_lines = getDocPosMax();
	//	S32 pos = (VERTICAL == mOrientation) ? y : x;
	//	S32 thumb_length = (VERTICAL == mOrientation) ? mThumbRect.getHeight() : mThumbRect.getWidth();
	//	S32 thumb_track_length = (VERTICAL == mOrientation) ? (getRect().getHeight() - 2 * SCROLLBAR_SIZE) : (getRect().getWidth() - 2 * SCROLLBAR_SIZE);
	//	S32 usable_track_length = thumb_track_length - thumb_length;
	//	F32 ratio = (VERTICAL == mOrientation) ? F32(pos - SCROLLBAR_SIZE - thumb_length) / usable_track_length
	//		: F32(pos - SCROLLBAR_SIZE) / usable_track_length;	
	//	S32 new_pos = (VERTICAL == mOrientation) ? llclamp( S32(variable_lines - ratio * variable_lines + 0.5f), 0, variable_lines )
	//		: llclamp( S32(ratio * variable_lines + 0.5f), 0, variable_lines );
	//	changeLine( new_pos - mDocPos, TRUE );
	//}
	//return TRUE;
	return FALSE;
}

BOOL LLScrollbar::handleMouseUp(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;
	if( hasMouseCapture() )
	{
		gFocusMgr.setMouseCapture( NULL );
		handled = TRUE;
	}
	else
	{
		// Opaque, so don't just check children	
		handled = LLView::handleMouseUp( x, y, mask );
	}

	return handled;
}

BOOL LLScrollbar::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// just treat a double click as a second click
	return handleMouseDown(x, y, mask);
}


void LLScrollbar::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	if (width == getRect().getWidth() && height == getRect().getHeight()) return;
	LLView::reshape( width, height, called_from_parent );
	LLButton* up_button = getChild<LLButton>("Line Up");
	LLButton* down_button = getChild<LLButton>("Line Down");

	if (mOrientation == VERTICAL)
	{
		up_button->reshape(up_button->getRect().getWidth(), llmin(getRect().getHeight() / 2, mThickness));
		down_button->reshape(down_button->getRect().getWidth(), llmin(getRect().getHeight() / 2, mThickness));
		up_button->setOrigin(up_button->getRect().mLeft, getRect().getHeight() - up_button->getRect().getHeight());
	}
	else
	{
		up_button->reshape(llmin(getRect().getWidth() / 2, mThickness), up_button->getRect().getHeight());
		down_button->reshape(llmin(getRect().getWidth() / 2, mThickness), down_button->getRect().getHeight());
		down_button->setOrigin(getRect().getWidth() - down_button->getRect().getWidth(), down_button->getRect().mBottom);
	}
	updateThumbRect();
}


void LLScrollbar::draw()
{
	if (!getRect().isValid()) return;

	S32 local_mouse_x;
	S32 local_mouse_y;
	LLUI::getMousePositionLocal(this, &local_mouse_x, &local_mouse_y);
	BOOL other_captor = gFocusMgr.getMouseCapture() && gFocusMgr.getMouseCapture() != this;
	BOOL hovered = getEnabled() && !other_captor && (hasMouseCapture() || mThumbRect.pointInRect(local_mouse_x, local_mouse_y));
	if (hovered)
	{
		mCurGlowStrength = lerp(mCurGlowStrength, mHoverGlowStrength, LLSmoothInterpolation::getInterpolant(0.05f));
	}
	else
	{
		mCurGlowStrength = lerp(mCurGlowStrength, 0.f, LLSmoothInterpolation::getInterpolant(0.05f));
	}

	// Draw background and thumb.
	LLUIImage* rounded_rect_imagep = LLUI::getUIImage("Rounded_Square");

	if (!rounded_rect_imagep)
	{
		gl_rect_2d(mOrientation == HORIZONTAL ? mThickness : 0, 
		mOrientation == VERTICAL ? getRect().getHeight() - 2 * mThickness : getRect().getHeight(),
		mOrientation == HORIZONTAL ? getRect().getWidth() - 2 * mThickness : getRect().getWidth(), 
		mOrientation == VERTICAL ? mThickness : 0, mTrackColor, TRUE);

		gl_rect_2d(mThumbRect, mThumbColor, TRUE);

	}
	else
	{
		// Thumb
		LLRect outline_rect = mThumbRect;
		outline_rect.stretch(2);
		// Background
		rounded_rect_imagep->drawSolid(mOrientation == HORIZONTAL ? SCROLLBAR_SIZE : 0, 
			mOrientation == VERTICAL ? SCROLLBAR_SIZE : 0,
			mOrientation == HORIZONTAL ? getRect().getWidth() - 2 * SCROLLBAR_SIZE : getRect().getWidth(), 
			mOrientation == VERTICAL ? getRect().getHeight() - 2 * SCROLLBAR_SIZE : getRect().getHeight(),
			mTrackColor);


		if (gFocusMgr.getKeyboardFocus() == this)
		{
			rounded_rect_imagep->draw(outline_rect, gFocusMgr.getFocusColor());
		}

		rounded_rect_imagep->draw(mThumbRect, mThumbColor);
		if (mCurGlowStrength > 0.01f)
		{
			gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);
			rounded_rect_imagep->drawSolid(mThumbRect, LLColor4(1.f, 1.f, 1.f, mCurGlowStrength));
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}

	}

	BOOL was_scrolled_to_bottom = (getDocPos() == getDocPosMax());
	if (mOnScrollEndCallback && was_scrolled_to_bottom)
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	// Draw children
	LLView::draw();
} // end draw


bool LLScrollbar::changeLine( S32 delta, BOOL update_thumb )
{
	return setDocPos(mDocPos + delta, update_thumb);
}

void LLScrollbar::setValue(const LLSD& value) 
{ 
	setDocPos((S32) value.asInteger());
}


BOOL LLScrollbar::handleKeyHere(KEY key, MASK mask)
{
	BOOL handled = FALSE;

	switch( key )
	{
	case KEY_HOME:
		setDocPos( 0 );
		handled = TRUE;
		break;
	
	case KEY_END:
		setDocPos( getDocPosMax() );
		handled = TRUE;
		break;
	
	case KEY_DOWN:
		setDocPos( getDocPos() + mStepSize );
		handled = TRUE;
		break;
	
	case KEY_UP:
		setDocPos( getDocPos() - mStepSize );
		handled = TRUE;
		break;

	case KEY_PAGE_DOWN:
		pageDown(1);
		break;

	case KEY_PAGE_UP:
		pageUp(1);
		break;
	}

	return handled;
}

void LLScrollbar::pageUp(S32 overlap)
{
	if (mDocSize > mPageSize)
	{
		changeLine( -(mPageSize - overlap), TRUE );
	}
}

void LLScrollbar::pageDown(S32 overlap)
{
	if (mDocSize > mPageSize)
	{
		changeLine( mPageSize - overlap, TRUE );
	}
}

void LLScrollbar::onLineUpBtnPressed( const LLSD& data )
{
	changeLine( -mStepSize, TRUE );
}

void LLScrollbar::onLineDownBtnPressed( const LLSD& data )
{
	changeLine( mStepSize, TRUE );
}

void LLScrollbar::setThickness(S32 thickness)
{
	mThickness = thickness < 0 ? SCROLLBAR_SIZE : thickness;
}
