/** 
 * @file llfloater.cpp
 * @brief LLFloater base class
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

// Floating "windows" within the GL display, like the inventory floater,
// mini-map floater, etc.

#include "linden_common.h"

#include "llfocusmgr.h"

#include "lluictrlfactory.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldraghandle.h"
#include "llfocusmgr.h"
#include "llresizebar.h"
#include "llresizehandle.h"
#include "llkeyboard.h"
#include "llmenugl.h"	// MENU_BAR_HEIGHT
#include "lltextbox.h"
#include "llresmgr.h"
#include "llui.h"
#include "llwindow.h"
#include "llstl.h"
#include "llcontrol.h"
#include "lltabcontainer.h"
#include "v2math.h"
#include "lltrans.h"
#include "llmultifloater.h"
#include "llfasttimer.h"
#include "airecursive.h"
#include "llnotifications.h"


const S32 MINIMIZED_WIDTH = 160;
const S32 CLOSE_BOX_FROM_TOP = 1;
// use this to control "jumping" behavior when Ctrl-Tabbing
const S32 TABBED_FLOATER_OFFSET = 0;

std::string	LLFloater::sButtonActiveImageNames[BUTTON_COUNT] = 
{
	"UIImgBtnCloseActiveUUID",		//BUTTON_CLOSE
	"UIImgBtnRestoreActiveUUID",	//BUTTON_RESTORE
	"UIImgBtnMinimizeActiveUUID",	//BUTTON_MINIMIZE
	"UIImgBtnTearOffActiveUUID",	//BUTTON_TEAR_OFF
	"UIImgBtnCloseActiveUUID",		//BUTTON_EDIT
};

std::string	LLFloater::sButtonInactiveImageNames[BUTTON_COUNT] = 
{
	"UIImgBtnCloseInactiveUUID",	//BUTTON_CLOSE
	"UIImgBtnRestoreInactiveUUID",	//BUTTON_RESTORE
	"UIImgBtnMinimizeInactiveUUID",	//BUTTON_MINIMIZE
	"UIImgBtnTearOffInactiveUUID",	//BUTTON_TEAR_OFF
	"UIImgBtnCloseInactiveUUID",	//BUTTON_EDIT
};

std::string	LLFloater::sButtonPressedImageNames[BUTTON_COUNT] = 
{
	"UIImgBtnClosePressedUUID",		//BUTTON_CLOSE
	"UIImgBtnRestorePressedUUID",	//BUTTON_RESTORE
	"UIImgBtnMinimizePressedUUID",	//BUTTON_MINIMIZE
	"UIImgBtnTearOffPressedUUID",	//BUTTON_TEAR_OFF
	"UIImgBtnClosePressedUUID",		//BUTTON_EDIT
};

std::string	LLFloater::sButtonNames[BUTTON_COUNT] = 
{
	"llfloater_close_btn",	//BUTTON_CLOSE
	"llfloater_restore_btn",	//BUTTON_RESTORE
	"llfloater_minimize_btn",	//BUTTON_MINIMIZE
	"llfloater_tear_off_btn",	//BUTTON_TEAR_OFF
	"llfloater_edit_btn",		//BUTTON_EDIT
};

std::string	LLFloater::sButtonToolTips[BUTTON_COUNT] = 
{
#ifdef LL_DARWIN
	"BUTTON_CLOSE_DARWIN",	//"Close (Cmd-W)",	//BUTTON_CLOSE
#else
	"BUTTON_CLOSE_WIN",		//"Close (Ctrl-W)",	//BUTTON_CLOSE
#endif
	"BUTTON_RESTORE",		//"Restore",	//BUTTON_RESTORE
	"BUTTON_MINIMIZE",		//"Minimize",	//BUTTON_MINIMIZE
	"BUTTON_TEAR_OFF",		//"Tear Off",	//BUTTON_TEAR_OFF
	"BUTTON_EDIT",			//"Edit",		//BUTTON_EDIT
};


LLFloater::button_callback LLFloater::sButtonCallbacks[BUTTON_COUNT] =
{
	&LLFloater::onClickClose,	//BUTTON_CLOSE
	&LLFloater::onClickMinimize, //BUTTON_RESTORE
	&LLFloater::onClickMinimize, //BUTTON_MINIMIZE
	&LLFloater::onClickTearOff,	//BUTTON_TEAR_OFF
	&LLFloater::onClickEdit,	//BUTTON_EDIT
};

LLMultiFloater* LLFloater::sHostp = NULL;
BOOL			LLFloater::sEditModeEnabled;
LLFloater::handle_map_t	LLFloater::sFloaterMap;

LLFloaterView* gFloaterView = NULL;

//static
void LLFloater::initClass()
{
	// translate tooltips for floater buttons
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		sButtonToolTips[i] = LLTrans::getString( sButtonToolTips[i] );
	}

	LLControlVariable* ctrl = LLUI::sConfigGroup->getControl("ActiveFloaterTransparency");
	if (ctrl)
	{
		ctrl->getSignal()->connect(boost::bind(&LLFloater::updateActiveFloaterTransparency));
		updateActiveFloaterTransparency();
	}

	ctrl = LLUI::sConfigGroup->getControl("InactiveFloaterTransparency");
	if (ctrl)
	{
		ctrl->getSignal()->connect(boost::bind(&LLFloater::updateInactiveFloaterTransparency));
		updateInactiveFloaterTransparency();
	}

}

LLFloater::LLFloater() :
	//FIXME: we should initialize *all* member variables here
	LLPanel(), mAutoFocus(TRUE),
	mResizable(FALSE),
	mDragOnLeft(FALSE),
	mMinWidth(0),
	mMinHeight(0)
{
	// automatically take focus when opened
	mAutoFocus = TRUE;
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		mButtonsEnabled[i] = FALSE;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; i++) 
	{
		mResizeBar[i] = NULL; 
		mResizeHandle[i] = NULL;
	}
	mDragHandle = NULL;
	mNotificationContext = new LLFloaterNotificationContext(getHandle());
}

LLFloater::LLFloater(const std::string& name)
:	LLPanel(name), mAutoFocus(TRUE) // automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		mButtonsEnabled[i] = FALSE;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; i++) 
	{
		mResizeBar[i] = NULL; 
		mResizeHandle[i] = NULL;
	}
	std::string title; // null string
	initFloater(title, FALSE, DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT, FALSE, TRUE, TRUE); // defaults
}


LLFloater::LLFloater(const std::string& name, const LLRect& rect, const std::string& title, 
	BOOL resizable, 
	S32 min_width, 
	S32 min_height,
	BOOL drag_on_left,
	BOOL minimizable,
	BOOL close_btn,
	BOOL bordered)
:	LLPanel(name, rect, bordered), mAutoFocus(TRUE) // automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		mButtonsEnabled[i] = FALSE;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; i++) 
	{
		mResizeBar[i] = NULL; 
		mResizeHandle[i] = NULL;
	}
	initFloater( title, resizable, min_width, min_height, drag_on_left, minimizable, close_btn);
}

LLFloater::LLFloater(const std::string& name, const std::string& rect_control, const std::string& title, 
	BOOL resizable, 
	S32 min_width, 
	S32 min_height,
	BOOL drag_on_left,
	BOOL minimizable,
	BOOL close_btn,
	BOOL bordered)
:	LLPanel(name, rect_control, bordered), mAutoFocus(TRUE) // automatically take focus when opened
{
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		mButtonsEnabled[i] = FALSE;
		mButtons[i] = NULL;
	}
	for (S32 i = 0; i < 4; i++) 
	{
		mResizeBar[i] = NULL; 
		mResizeHandle[i] = NULL;
	}
	initFloater( title, resizable, min_width, min_height, drag_on_left, minimizable, close_btn);
}


// Note: Floaters constructed from XML call init() twice!
void LLFloater::initFloater(const std::string& title,
					 BOOL resizable, S32 min_width, S32 min_height,
					 BOOL drag_on_left, BOOL minimizable, BOOL close_btn)
{
	mNotificationContext = new LLFloaterNotificationContext(getHandle());

	// Init function can be called more than once, so clear out old data.
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		mButtonsEnabled[i] = FALSE;
		if (mButtons[i] != NULL)
		{
			removeChild(mButtons[i]);
			delete mButtons[i];
			mButtons[i] = NULL;
		}
	}
	mButtonScale = 1.f;

	//sjb: Thia is a bit of a hack:
	BOOL need_border = hasBorder();
	// remove the border since deleteAllChildren() will also delete the border (but not clear mBorder)
	removeBorder();
	// this will delete mBorder too
	deleteAllChildren();
	// add the border back if we want it
	if (need_border)
	{
	    addBorder();
	}

	// chrome floaters don't take focus at all
	setFocusRoot(!getIsChrome());

	// Reset cached pointers
	mDragHandle = NULL;
	for (S32 i = 0; i < 4; i++) 
	{
		mResizeBar[i] = NULL;
		mResizeHandle[i] = NULL;
	}
	mCanTearOff = TRUE;
	mEditing = FALSE;

	// Clicks stop here.
	setMouseOpaque(TRUE);

	mFirstLook = TRUE;
	mForeground = FALSE;
	mDragOnLeft = drag_on_left == TRUE;

	// Floaters always draw their background, unlike every other panel.
	setBackgroundVisible(TRUE);

	// Floaters start not minimized.  When minimized, they save their
	// prior rectangle to be used on restore.
	mMinimized = FALSE;
	mExpandedRect.set(0,0,0,0);
	
	S32 close_box_size;		// For layout purposes, how big is the close box?
	if (close_btn)
	{
		close_box_size = LLFLOATER_CLOSE_BOX_SIZE;
	}
	else
	{
		close_box_size = 0;
	}

	// Drag Handle
	// Add first so it's in the background.
//	const S32 drag_pad = 2;
	if (drag_on_left)
	{
		LLRect drag_handle_rect;
		drag_handle_rect.setOriginAndSize(
			0, 0,
			DRAG_HANDLE_WIDTH,
			getRect().getHeight() - LLPANEL_BORDER_WIDTH - close_box_size);
		mDragHandle = new LLDragHandleLeft(std::string("drag"), drag_handle_rect, title );
	}
	else // drag on top
	{
		LLRect drag_handle_rect( 0, getRect().getHeight(), getRect().getWidth(), 0 );
		mDragHandle = new LLDragHandleTop( std::string("Drag Handle"), drag_handle_rect, title );
	}
	addChild(mDragHandle);

	// Resize Handle
	mResizable = resizable;
	mMinWidth = min_width;
	mMinHeight = min_height;

	if( mResizable )
	{
		addResizeCtrls();
	}

	// Close button.
	if (close_btn)
	{
		mButtonsEnabled[BUTTON_CLOSE] = TRUE;
	}

	// Minimize button only for top draggers
	if ( !drag_on_left && minimizable )
	{
		mButtonsEnabled[BUTTON_MINIMIZE] = TRUE;
	}

	// Keep track of whether this window has ever been dragged while it
	// was minimized.  If it has, we'll remember its position for the
	// next time it's minimized.
	mHasBeenDraggedWhileMinimized = FALSE;
	mPreviousMinimizedLeft = 0;
	mPreviousMinimizedBottom = 0;

	buildButtons();

	// JC - Don't do this here, because many floaters first construct themselves,
	// then show themselves.  Put it in setVisibleAndFrontmost.
	// make_ui_sound("UISndWindowOpen");

	// RN: floaters are created in the invisible state	
	setVisible(FALSE);

	// add self to handle->floater map
	sFloaterMap[getHandle()] = this;

	if (!getParent())
	{
		gFloaterView->addChild(this);
	}
}

// static
void LLFloater::updateActiveFloaterTransparency()
{
	sActiveControlTransparency = LLUI::sConfigGroup->getF32("ActiveFloaterTransparency");
}

// static
void LLFloater::updateInactiveFloaterTransparency()
{
	sInactiveControlTransparency = LLUI::sConfigGroup->getF32("InactiveFloaterTransparency");
}

void LLFloater::addResizeCtrls()
{	
	// Resize bars (sides)
	LLResizeBar::Params p;
	p.name("resizebar_left");
	p.resizing_view(this);
	p.min_size(mMinWidth);
	p.side(LLResizeBar::LEFT);
	mResizeBar[LLResizeBar::LEFT] = LLUICtrlFactory::create<LLResizeBar>(p);
	addChild( mResizeBar[LLResizeBar::LEFT] );

	p.name("resizebar_top");
	p.min_size(mMinHeight);
	p.side(LLResizeBar::TOP);

	mResizeBar[LLResizeBar::TOP] = LLUICtrlFactory::create<LLResizeBar>(p);
	addChild( mResizeBar[LLResizeBar::TOP] );

	p.name("resizebar_right");
	p.min_size(mMinWidth);
	p.side(LLResizeBar::RIGHT);	
	mResizeBar[LLResizeBar::RIGHT] = LLUICtrlFactory::create<LLResizeBar>(p);
	addChild( mResizeBar[LLResizeBar::RIGHT] );

	p.name("resizebar_bottom");
	p.min_size(mMinHeight);
	p.side(LLResizeBar::BOTTOM);
	mResizeBar[LLResizeBar::BOTTOM] = LLUICtrlFactory::create<LLResizeBar>(p);
	addChild( mResizeBar[LLResizeBar::BOTTOM] );

	// Resize handles (corners)
	LLResizeHandle::Params handle_p;
	// handles must not be mouse-opaque, otherwise they block hover events
	// to other buttons like the close box. JC
	handle_p.mouse_opaque(false);
	handle_p.min_width(mMinWidth);
	handle_p.min_height(mMinHeight);
	handle_p.corner(LLResizeHandle::RIGHT_BOTTOM);
	mResizeHandle[0] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
	addChild(mResizeHandle[0]);

	handle_p.corner(LLResizeHandle::RIGHT_TOP);
	mResizeHandle[1] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
	addChild(mResizeHandle[1]);
	
	handle_p.corner(LLResizeHandle::LEFT_BOTTOM);
	mResizeHandle[2] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
	addChild(mResizeHandle[2]);

	handle_p.corner(LLResizeHandle::LEFT_TOP);
	mResizeHandle[3] = LLUICtrlFactory::create<LLResizeHandle>(handle_p);
	addChild(mResizeHandle[3]);

	layoutResizeCtrls();
}

void LLFloater::layoutResizeCtrls()
{
	LLRect rect;

	// Resize bars (sides)
	const S32 RESIZE_BAR_THICKNESS = 3;
	rect = LLRect( 0, getRect().getHeight(), RESIZE_BAR_THICKNESS, 0);
	mResizeBar[LLResizeBar::LEFT]->setRect(rect);

	rect = LLRect( 0, getRect().getHeight(), getRect().getWidth(), getRect().getHeight() - RESIZE_BAR_THICKNESS);
	mResizeBar[LLResizeBar::TOP]->setRect(rect);

	rect = LLRect(getRect().getWidth() - RESIZE_BAR_THICKNESS, getRect().getHeight(), getRect().getWidth(), 0);
	mResizeBar[LLResizeBar::RIGHT]->setRect(rect);

	rect = LLRect(0, RESIZE_BAR_THICKNESS, getRect().getWidth(), 0);
	mResizeBar[LLResizeBar::BOTTOM]->setRect(rect);

	// Resize handles (corners)
	rect = LLRect( getRect().getWidth() - RESIZE_HANDLE_WIDTH, RESIZE_HANDLE_HEIGHT, getRect().getWidth(), 0);
	mResizeHandle[0]->setRect(rect);

	rect = LLRect( getRect().getWidth() - RESIZE_HANDLE_WIDTH, getRect().getHeight(), getRect().getWidth(), getRect().getHeight() - RESIZE_HANDLE_HEIGHT);
	mResizeHandle[1]->setRect(rect);
	
	rect = LLRect( 0, RESIZE_HANDLE_HEIGHT, RESIZE_HANDLE_WIDTH, 0 );
	mResizeHandle[2]->setRect(rect);

	rect = LLRect( 0, getRect().getHeight(), RESIZE_HANDLE_WIDTH, getRect().getHeight() - RESIZE_HANDLE_HEIGHT );
	mResizeHandle[3]->setRect(rect);
}

void LLFloater::enableResizeCtrls(bool enable, bool width, bool height)
{
	mResizeBar[LLResizeBar::LEFT]->setVisible(enable && width);
	mResizeBar[LLResizeBar::LEFT]->setEnabled(enable && width);

	mResizeBar[LLResizeBar::TOP]->setVisible(enable && height);
	mResizeBar[LLResizeBar::TOP]->setEnabled(enable && height);
	
	mResizeBar[LLResizeBar::RIGHT]->setVisible(enable && width);
	mResizeBar[LLResizeBar::RIGHT]->setEnabled(enable && width);
	
	mResizeBar[LLResizeBar::BOTTOM]->setVisible(enable && height);
	mResizeBar[LLResizeBar::BOTTOM]->setEnabled(enable && height);

	for (S32 i = 0; i < 4; ++i)
	{
		mResizeHandle[i]->setVisible(enable && width && height);
		mResizeHandle[i]->setEnabled(enable && width && height);
	}
}

// virtual
LLFloater::~LLFloater()
{
	delete mNotificationContext;
	mNotificationContext = NULL;

	control_map_t::iterator itor;
	for (itor = mFloaterControls.begin(); itor != mFloaterControls.end(); ++itor)
	{
		delete itor->second;
	}
	mFloaterControls.clear();

	//// am I not hosted by another floater?
	//if (mHostHandle.isDead())
	//{
	//	LLFloaterView* parent = (LLFloaterView*) getParent();

	//	if( parent )
	//	{
	//		parent->removeChild( this );
	//	}
	//}

	// Just in case we might still have focus here, release it.
	releaseFocus();

	// This is important so that floaters with persistent rects (i.e., those
	// created with rect control rather than an LLRect) are restored in their
	// correct, non-minimized positions.
	setMinimized( FALSE );

	sFloaterMap.erase(getHandle());

	delete mDragHandle;
	for (S32 i = 0; i < 4; i++) 
	{
		delete mResizeBar[i];
		delete mResizeHandle[i];
	}
}


void LLFloater::setVisible( BOOL visible )
{
	LLPanel::setVisible(visible);
	if( visible && mFirstLook )
	{
		mFirstLook = FALSE;
	}

	if( !visible )
	{
		if( gFocusMgr.childIsTopCtrl( this ) )
		{
			gFocusMgr.setTopCtrl(NULL);
		}

		if( gFocusMgr.childHasMouseCapture( this ) )
		{
			gFocusMgr.setMouseCapture(NULL);
		}
	}

	for(handle_set_iter_t dependent_it = mDependents.begin();
		dependent_it != mDependents.end(); )
	{
		LLFloater* floaterp = dependent_it->get();

		if (floaterp)
		{
			floaterp->setVisible(visible);
		}
		++dependent_it;
	}
}

void LLFloater::open()	/* Flawfinder: ignore */
{
	if (getSoundFlags() != SILENT 
	// don't play open sound for hosted (tabbed) windows
		&& !getHost() 
		&& !getFloaterHost()
		&& (!getVisible() || isMinimized()))
	{
		make_ui_sound("UISndWindowOpen");
	}

	//RN: for now, we don't allow rehosting from one multifloater to another
	// just need to fix the bugs
	if (getFloaterHost() != NULL && getHost() == NULL)
	{
		// needs a host
		// only select tabs if window they are hosted in is visible
		getFloaterHost()->addFloater(this, getFloaterHost()->getVisible());
	}
	else if (getHost() != NULL)
	{
		// already hosted
		getHost()->showFloater(this);
	}
	else
	{
		setMinimized(FALSE);
		setVisibleAndFrontmost(mAutoFocus);
	}

	if (!getControlName().empty())
		setControlValue(true);

	onOpen();
}

void LLFloater::close(bool app_quitting)
{
	// Always unminimize before trying to close.
	// Most of the time the user will never see this state.
	setMinimized(FALSE);

	if (canClose())
	{
		if (getHost())
		{
			((LLMultiFloater*)getHost())->removeFloater(this);
			gFloaterView->addChild(this);
		}

		if (getSoundFlags() != SILENT
			&& getVisible()
			&& !getHost()
			&& !app_quitting)
		{
			make_ui_sound("UISndWindowClose");
		}

		// now close dependent floater
		for(handle_set_iter_t dependent_it = mDependents.begin();
			dependent_it != mDependents.end(); )
		{
			
			LLFloater* floaterp = dependent_it->get();
			if (floaterp)
			{
				++dependent_it;
				floaterp->close();
			}
			else
			{
				mDependents.erase(dependent_it++);
			}
		}
		
		cleanupHandles();
		gFocusMgr.clearLastFocusForGroup(this);

		if (hasFocus())
		{
			// Do this early, so UI controls will commit before the
			// window is taken down.
			releaseFocus();

			// give focus to dependee floater if it exists, and we had focus first
			if (isDependent())
			{
				LLFloater* dependee = mDependeeHandle.get();
				if (dependee && !dependee->isDead())
				{
					dependee->setFocus(TRUE);
				}
			}
		}

		//If floater is a dependent, remove it from parent (dependee)
		LLFloater* dependee = mDependeeHandle.get();
		if (dependee)
		{
			dependee->removeDependentFloater(this);
		}

		// now close dependent floater
		for (handle_set_iter_t dependent_it = mDependents.begin();
			dependent_it != mDependents.end(); )
		{
			LLFloater* floaterp = dependent_it->get();
			if (floaterp)
			{
				++dependent_it;
				floaterp->close(app_quitting);
			}
			else
			{
				mDependents.erase(dependent_it++);
			}
		}

		cleanupHandles();

		if (!app_quitting && !getControlName().empty())
			setControlValue(false);

		// Let floater do cleanup.
		onClose(app_quitting);
	}
}

/*virtual*/
void LLFloater::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
}

void LLFloater::releaseFocus()
{
	if( gFocusMgr.childIsTopCtrl( this ) )
	{
		gFocusMgr.setTopCtrl(NULL);
	}

	if( gFocusMgr.childHasKeyboardFocus( this ) )
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	if( gFocusMgr.childHasMouseCapture( this ) )
	{
		gFocusMgr.setMouseCapture(NULL);
	}
}


void LLFloater::setResizeLimits( S32 min_width, S32 min_height )
{
	mMinWidth = min_width;
	mMinHeight = min_height;

	for( S32 i = 0; i < 4; i++ )
	{
		if( mResizeBar[i] )
		{
			if (i == LLResizeBar::LEFT || i == LLResizeBar::RIGHT)
			{
				mResizeBar[i]->setResizeLimits( min_width, S32_MAX );
			}
			else
			{
				mResizeBar[i]->setResizeLimits( min_height, S32_MAX );
			}
		}
		if( mResizeHandle[i] )
		{
			mResizeHandle[i]->setResizeLimits( min_width, min_height );
		}
	}
}


void LLFloater::center()
{
	if(getHost())
	{
		// hosted floaters can't move
		return;
	}
	centerWithin(gFloaterView->getRect());
}

void LLFloater::applyRectControl()
{
	if (!getRectControl().empty())
	{
		const LLRect& rect = LLUI::sConfigGroup->getRect(getRectControl());
		translate( rect.mLeft - getRect().mLeft, rect.mBottom - getRect().mBottom);
		if (mResizable)
		{
			reshape(llmax(mMinWidth, rect.getWidth()), llmax(mMinHeight, rect.getHeight()));
		}
	}
}

void LLFloater::applyTitle()
{
	if (!mDragHandle)
	{
		return;
	}

	if (isMinimized() && !mShortTitle.empty())
	{
		mDragHandle->setTitle( mShortTitle );
	}
	else
	{
		mDragHandle->setTitle ( mTitle );
	}
}

const std::string& LLFloater::getCurrentTitle() const
{
	return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
}

void LLFloater::setTitle( const std::string& title )
{
	if(mTitle == title)
		return;
	mTitle = title;
	applyTitle();
}

std::string LLFloater::getTitle() const
{
	if (mTitle.empty())
	{
		return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
	}
	else
	{
		return mTitle;
	}
}

void LLFloater::setShortTitle( const std::string& short_title )
{
	mShortTitle = short_title;
	applyTitle();
}

std::string LLFloater::getShortTitle() const
{
	if (mShortTitle.empty())
	{
		return mDragHandle ? mDragHandle->getTitle() : LLStringUtil::null;
	}
	else
	{
		return mShortTitle;
	}
}

BOOL LLFloater::canSnapTo(const LLView* other_view)
{
	if (NULL == other_view)
	{
		LL_WARNS() << "other_view is NULL" << LL_ENDL;
		return FALSE;
	}

	if (other_view != getParent())
	{
		const LLFloater* other_floaterp = dynamic_cast<const LLFloater*>(other_view);		
		if (other_floaterp && other_floaterp->getSnapTarget() == getHandle() && mDependents.find(other_floaterp->getHandle()) != mDependents.end())
		{
			// this is a dependent that is already snapped to us, so don't snap back to it
			return FALSE;
		}
	}

	return LLPanel::canSnapTo(other_view);
}

void LLFloater::setSnappedTo(const LLView* snap_view)
{
	if (!snap_view || snap_view == getParent())
	{
		clearSnapTarget();
	}
	else
	{
		//RN: assume it's a floater as it must be a sibling to our parent floater
		const LLFloater* floaterp = dynamic_cast<const LLFloater*>(snap_view);
		if (floaterp)
		{
			setSnapTarget(floaterp->getHandle());
		}
	}
}

void LLFloater::handleReshape(const LLRect& new_rect, bool by_user)
{
	const LLRect old_rect = getRect();
	LLView::handleReshape(new_rect, by_user);

	// if not minimized, adjust all snapped dependents to new shape
	if (!isMinimized())
	{
		// gather all snapped dependents
		for(handle_set_iter_t dependent_it = mDependents.begin();
			dependent_it != mDependents.end(); ++dependent_it)
		{
			LLFloater* floaterp = dependent_it->get();
			// is a dependent snapped to us?
			if (floaterp && floaterp->getSnapTarget() == getHandle())
			{
				S32 delta_x = 0;
				S32 delta_y = 0;
				// check to see if it snapped to right or top, and move if dependee floater is resizing
				LLRect dependent_rect = floaterp->getRect();
				if (dependent_rect.mLeft - getRect().mLeft >= old_rect.getWidth() || // dependent on my right?
					dependent_rect.mRight == getRect().mLeft + old_rect.getWidth()) // dependent aligned with my right
				{
					// was snapped directly onto right side or aligned with it
					delta_x += new_rect.getWidth() - old_rect.getWidth();
				}
				if (dependent_rect.mBottom - getRect().mBottom >= old_rect.getHeight() ||
					dependent_rect.mTop == getRect().mBottom + old_rect.getHeight())
				{
					// was snapped directly onto top side or aligned with it
					delta_y += new_rect.getHeight() - old_rect.getHeight();
				}

				// take translation of dependee floater into account as well
				delta_x += new_rect.mLeft - old_rect.mLeft;
				delta_y += new_rect.mBottom - old_rect.mBottom;

				dependent_rect.translate(delta_x, delta_y);
				floaterp->setShape(dependent_rect, by_user);
			}
		}
	}
	else
	{
		// If minimized, and origin has changed, set
		// mHasBeenDraggedWhileMinimized to TRUE
		if ((new_rect.mLeft != old_rect.mLeft) ||
			(new_rect.mBottom != old_rect.mBottom))
		{
			mHasBeenDraggedWhileMinimized = TRUE;
		}
	}
}

void LLFloater::setMinimized(BOOL minimize)
{
	if (minimize == mMinimized) return;

	if (minimize)
	{
		mExpandedRect = getRect();

		// If the floater has been dragged while minimized in the
		// past, then locate it at its previous minimized location.
		// Otherwise, ask the view for a minimize position.
		if (mHasBeenDraggedWhileMinimized)
		{
			setOrigin(mPreviousMinimizedLeft, mPreviousMinimizedBottom);
		}
		else
		{
			S32 left, bottom;
			gFloaterView->getMinimizePosition(&left, &bottom);
			setOrigin( left, bottom );
		}

		if (mButtonsEnabled[BUTTON_MINIMIZE])
		{
			mButtonsEnabled[BUTTON_MINIMIZE] = FALSE;
			mButtonsEnabled[BUTTON_RESTORE] = TRUE;
		}

		if (mDragHandle)
		{
			mDragHandle->setVisible(TRUE);
		}
		setBorderVisible(TRUE);

		for(handle_set_iter_t dependent_it = mDependents.begin();
			dependent_it != mDependents.end();
			++dependent_it)
		{
			LLFloater* floaterp = dependent_it->get();
			if (floaterp)
			{
				if (floaterp->isMinimizeable())
				{
					floaterp->setMinimized(TRUE);
				}
				else if (!floaterp->isMinimized())
				{
					floaterp->setVisible(FALSE);
				}
			}
		}

		// Lose keyboard focus when minimized
		releaseFocus();
		// Also reset mLockedView and mLastKeyboardFocus, to avoid that we get focus back somehow.
		gFocusMgr.removeKeyboardFocusWithoutCallback(this);

		for (S32 i = 0; i < 4; i++)
		{
			if (mResizeBar[i] != NULL)
			{
				mResizeBar[i]->setEnabled(FALSE);
			}
			if (mResizeHandle[i] != NULL)
			{
				mResizeHandle[i]->setEnabled(FALSE);
			}
		}
		
		mMinimized = TRUE;

		// Reshape *after* setting mMinimized
		reshape( MINIMIZED_WIDTH, LLFLOATER_HEADER_SIZE, TRUE);
	}
	else
	{
		// If this window has been dragged while minimized (at any time),
		// remember its position for the next time it's minimized.
		if (mHasBeenDraggedWhileMinimized)
		{
			const LLRect& currentRect = getRect();
			mPreviousMinimizedLeft = currentRect.mLeft;
			mPreviousMinimizedBottom = currentRect.mBottom;
		}

		setOrigin( mExpandedRect.mLeft, mExpandedRect.mBottom );

		if (mButtonsEnabled[BUTTON_RESTORE])
		{
			mButtonsEnabled[BUTTON_MINIMIZE] = TRUE;
			mButtonsEnabled[BUTTON_RESTORE] = FALSE;
		}

		// show dependent floater
		for(handle_set_iter_t dependent_it = mDependents.begin();
			dependent_it != mDependents.end();
			++dependent_it)
		{
			LLFloater* floaterp = dependent_it->get();
			if (floaterp)
			{
				floaterp->setMinimized(FALSE);
				floaterp->setVisible(TRUE);
			}
		}

		for (S32 i = 0; i < 4; i++)
		{
			if (mResizeBar[i] != NULL)
			{
				mResizeBar[i]->setEnabled(isResizable());
			}
			if (mResizeHandle[i] != NULL)
			{
				mResizeHandle[i]->setEnabled(isResizable());
			}
		}
		
		mMinimized = FALSE;

		// Reshape *after* setting mMinimized
		reshape( mExpandedRect.getWidth(), mExpandedRect.getHeight(), TRUE );
	}
	
	applyTitle ();

	make_ui_sound("UISndWindowClose");
	updateButtons();
}

void LLFloater::setFocus( BOOL b )
{
	if (b && getIsChrome())
	{
		return;
	}
	LLUICtrl* last_focus = gFocusMgr.getLastFocusForGroup(this);
	// a descendent already has focus
	BOOL child_had_focus = gFocusMgr.childHasKeyboardFocus(this);

	// give focus to first valid descendent
	LLPanel::setFocus(b);

	if (b)
	{
		// only push focused floaters to front of stack if not in midst of ctrl-tab cycle
		LLFloaterView * parent = dynamic_cast<LLFloaterView *>(getParent());
		if (!getHost() && parent && !parent->getCycleMode())
		{
			if (!isFrontmost())
			{
				setFrontmost();
			}
		}

		// when getting focus, delegate to last descendent which had focus
		if (last_focus && !child_had_focus && 
			last_focus->isInEnabledChain() &&
			last_focus->isInVisibleChain())
		{
			// *FIX: should handle case where focus doesn't stick
			last_focus->setFocus(TRUE);
		}
	}
	updateTransparency(b ? TT_ACTIVE : TT_INACTIVE);
}

// virtual
void LLFloater::setIsChrome(BOOL is_chrome)
{
	// chrome floaters don't take focus at all
	if (is_chrome)
	{
		// remove focus if we're changing to chrome
		setFocus(FALSE);
		// can't Ctrl-Tab to "chrome" floaters
		setFocusRoot(FALSE);
	}
	
	// no titles displayed on "chrome" floaters
	if (mDragHandle)
		mDragHandle->setTitleVisible(!is_chrome);
	
	LLPanel::setIsChrome(is_chrome);
}

void LLFloater::setTitleVisible(bool visible)
{
	if (mDragHandle)
		mDragHandle->setTitleVisible(visible);
}

// Change the draw style to account for the foreground state.
void LLFloater::setForeground(BOOL front)
{
	if (front != mForeground)
	{
		mForeground = front;
		if (mDragHandle)
			mDragHandle->setForeground( front );

		if (!front)
		{
			releaseFocus();
		}

		setBackgroundOpaque( front );
		// Singu Note: Upstream isn't doing this, I can't see where they were actually going inactive. Maybe setFocus(false) isn't being called, but we have parity there..
		updateTransparency(front || getIsChrome() ? TT_ACTIVE : TT_INACTIVE);
	}
}

void LLFloater::cleanupHandles()
{
	// remove handles to non-existent dependents
	for(handle_set_iter_t dependent_it = mDependents.begin();
		dependent_it != mDependents.end(); )
	{
		LLFloater* floaterp = dependent_it->get();
		if (!floaterp)
		{
			mDependents.erase(dependent_it++);
		}
		else
		{
			++dependent_it;
		}
	}
}

void LLFloater::setHost(LLMultiFloater* host)
{
	if (mHostHandle.isDead() && host)
	{
		// make buttons smaller for hosted windows to differentiate from parent
		mButtonScale = 0.9f;

		// add tear off button
		if (mCanTearOff)
		{
			mButtonsEnabled[BUTTON_TEAR_OFF] = TRUE;
		}
	}
	else if (!mHostHandle.isDead() && !host)
	{
		mButtonScale = 1.f;
		//mButtonsEnabled[BUTTON_TEAR_OFF] = FALSE;
	}
	updateButtons();
	if (host)
	{
		mHostHandle = host->getHandle();
		mLastHostHandle = host->getHandle();
	}
	else
	{
		mHostHandle.markDead();
	}
}

void LLFloater::moveResizeHandlesToFront()
{
	for( S32 i = 0; i < 4; i++ )
	{
		if( mResizeBar[i] )
		{
			sendChildToFront(mResizeBar[i]);
		}
	}

	for( S32 i = 0; i < 4; i++ )
	{
		if( mResizeHandle[i] )
		{
			sendChildToFront(mResizeHandle[i]);
		}
	}
}

BOOL LLFloater::isFrontmost()
{
	return gFloaterView && gFloaterView->getFrontmost() == this && getVisible();
}

void LLFloater::addDependentFloater(LLFloater* floaterp, BOOL reposition)
{
	mDependents.insert(floaterp->getHandle());
	floaterp->mDependeeHandle = getHandle();

	if (reposition)
	{
		floaterp->setRect(gFloaterView->findNeighboringPosition(this, floaterp));
		floaterp->setSnapTarget(getHandle());
	}
	gFloaterView->adjustToFitScreen(floaterp, FALSE);
	if (floaterp->isFrontmost())
	{
		// make sure to bring self and sibling floaters to front
		gFloaterView->bringToFront(floaterp);
	}
}

void LLFloater::addDependentFloater(LLHandle<LLFloater> dependent, BOOL reposition)
{
	LLFloater* dependent_floaterp = dependent.get();
	if(dependent_floaterp)
	{
		addDependentFloater(dependent_floaterp, reposition);
	}
}

void LLFloater::removeDependentFloater(LLFloater* floaterp)
{
	mDependents.erase(floaterp->getHandle());
	floaterp->mDependeeHandle = LLHandle<LLFloater>();
}

BOOL LLFloater::offerClickToButton(S32 x, S32 y, MASK mask, EFloaterButton index)
{
	if( mButtonsEnabled[index] )
	{
		LLButton* my_butt = mButtons[index];
		S32 local_x = x - my_butt->getRect().mLeft;
		S32 local_y = y - my_butt->getRect().mBottom;

		if (
			my_butt->pointInView(local_x, local_y) &&
			my_butt->handleMouseDown(local_x, local_y, mask))
		{
			// the button handled it
			return TRUE;
		}
	}
	return FALSE;
}

// virtual
BOOL LLFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if( mMinimized )
	{
		// Offer the click to titlebar buttons.
		// Note: this block and the offerClickToButton helper method can be removed
		// because the parent container will handle it for us but we'll keep it here
		// for safety until after reworking the panel code to manage hidden children.
		if(offerClickToButton(x, y, mask, BUTTON_CLOSE)) return TRUE;
		if(offerClickToButton(x, y, mask, BUTTON_RESTORE)) return TRUE;
		if(offerClickToButton(x, y, mask, BUTTON_TEAR_OFF)) return TRUE;

		// Otherwise pass to drag handle for movement
		return mDragHandle->handleMouseDown(x, y, mask);
	}
	else
	{
		bringToFront( x, y );
		return LLPanel::handleMouseDown( x, y, mask );
	}
}

// virtual
BOOL LLFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL was_minimized = mMinimized;
	bringToFront( x, y );
	return was_minimized || LLPanel::handleRightMouseDown( x, y, mask );
}

BOOL LLFloater::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	bringToFront( x, y );
	return LLPanel::handleMiddleMouseDown( x, y, mask );
}


// virtual
BOOL LLFloater::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	BOOL was_minimized = mMinimized;
	setMinimized(FALSE);
	return was_minimized || LLPanel::handleDoubleClick(x, y, mask);
}

void LLFloater::bringToFront( S32 x, S32 y )
{
	if (getVisible() && pointInView(x, y))
	{
		LLMultiFloater* hostp = getHost();
		if (hostp)
		{
			hostp->showFloater(this);
		}
		else
		{
			LLFloaterView* parent = (LLFloaterView*) getParent();
			if (parent)
			{
				parent->bringToFront( this );
			}
		}
	}
}


// virtual
void LLFloater::setVisibleAndFrontmost(BOOL take_focus)
{
	setVisible(TRUE);
	setFrontmost(take_focus);
}

void LLFloater::setFrontmost(BOOL take_focus)
{
	LLMultiFloater* hostp = getHost();
	if (hostp)
	{
		// this will bring the host floater to the front and select
		// the appropriate panel
		hostp->showFloater(this);
	}
	else
	{
		// there are more than one floater view
		// so we need to query our parent directly
		LLFloaterView * parent = dynamic_cast<LLFloaterView*>( getParent() );
		if (parent)
		{
			parent->bringToFront(this, take_focus);
		}

		// Make sure to set the appropriate transparency type (STORM-732).
		updateTransparency(hasFocus() || getIsChrome() ? TT_ACTIVE : TT_INACTIVE);
	}
}

//static
void LLFloater::setEditModeEnabled(BOOL enable)
{
	if (enable != sEditModeEnabled)
	{
		S32 count = 0;
		for(handle_map_iter_t iter = sFloaterMap.begin(); iter != sFloaterMap.end(); ++iter)
		{
			LLFloater* floater = iter->second;
			if (!floater->isDead())
			{
				iter->second->mButtonsEnabled[BUTTON_EDIT] = enable;
				iter->second->updateButtons();
			}
			count++;
		}
	}

	sEditModeEnabled = enable;
}


void LLFloater::onClickMinimize()
{
	setMinimized( !isMinimized() );
}

void LLFloater::onClickTearOff()
{
	LLMultiFloater* host_floater = getHost();
	if (host_floater) //Tear off
	{
		LLRect new_rect;
		host_floater->removeFloater(this);
		// reparent to floater view
		gFloaterView->addChild(this);

		open();	/* Flawfinder: ignore */
		
		// only force position for floaters that don't have that data saved
		if (getRectControl().empty())
		{
			new_rect.setLeftTopAndSize(host_floater->getRect().mLeft + 5, host_floater->getRect().mTop - LLFLOATER_HEADER_SIZE - 5, getRect().getWidth(), getRect().getHeight());
			setRect(new_rect);
		}
		gFloaterView->adjustToFitScreen(this, FALSE);
		// give focus to new window to keep continuity for the user
		setFocus(TRUE);
	}
	else  //Attach to parent.
	{
		LLMultiFloater* new_host = (LLMultiFloater*)mLastHostHandle.get();
		if (new_host)
		{
			setMinimized(FALSE); // to reenable minimize button if it was minimized
			new_host->showFloater(this);
			// make sure host is visible
			new_host->open();
		}
	}
}

void LLFloater::onClickEdit()
{
	mEditing = mEditing ? FALSE : TRUE;
}

// static 
LLFloater* LLFloater::getClosableFloaterFromFocus()
{
	LLFloater* focused_floater = gFloaterView->getFocusedFloater();

	if (!focused_floater)
	{
		return NULL;
	}

	// The focused floater may not be closable,
	// Find and close a parental floater that is closeable, if any.
	LLFloater* previous_floater = NULL;		// Guard against endless loop, because getParentFloater(x) can return x!
	for(LLFloater* floater_to_close = focused_floater; 
		NULL != floater_to_close; 
		floater_to_close = gFloaterView->getParentFloater(floater_to_close))
	{
		if(floater_to_close == previous_floater)
		{
			break;
		}
		if(floater_to_close->isCloseable())
		{
			return floater_to_close;
		}
		previous_floater = floater_to_close;
	}

	return NULL;
}

// static
void LLFloater::closeFocusedFloater()
{
	LLFloater* floater_to_close = LLFloater::getClosableFloaterFromFocus();
	if(floater_to_close)
	{
		floater_to_close->close();
	}

	// if nothing took focus after closing focused floater
	// give it to next floater (to allow closing multiple windows via keyboard in rapid succession)
	if (gFocusMgr.getKeyboardFocus() == NULL)
	{
		// HACK: use gFloaterView directly in case we are using Ctrl-W to close snapshot window
		// which sits in gSnapshotFloaterView, and needs to pass focus on to normal floater view
		gFloaterView->focusFrontFloater();
	}
}

LLNotificationPtr LLFloater::addContextualNotification(const std::string& name, const LLSD& substitutions)
{
	return LLNotifications::instance().add(LLNotification::Params(name).context(mNotificationContext).substitutions(substitutions));
}

void LLFloater::onClickClose()
{
	close();
}


// virtual
void LLFloater::draw()
{
	const F32 alpha = getCurrentTransparency();

	// draw background
	if( isBackgroundVisible() )
	{
		drawShadow(this);

		S32 left = LLPANEL_BORDER_WIDTH;
		S32 top = getRect().getHeight() - LLPANEL_BORDER_WIDTH;
		S32 right = getRect().getWidth() - LLPANEL_BORDER_WIDTH;
		S32 bottom = LLPANEL_BORDER_WIDTH;

		// No transparent windows in simple UI
		LLColor4 color;
		if (isBackgroundOpaque())
		{
			color = getBackgroundColor();
		}
		else
		{
			color = getTransparentColor();
		}

		{
			// We're not using images, use old-school flat colors
			gl_rect_2d( left, top, right, bottom, color % alpha );

			// draw highlight on title bar to indicate focus.  RDW
			if(gFocusMgr.childHasKeyboardFocus(this)
				&& !getIsChrome()
				&& !getCurrentTitle().empty())
			{
				static auto titlebar_focus_color = LLUI::sColorsGroup->getColor("TitleBarFocusColor");

				const LLFontGL* font = LLFontGL::getFontSansSerif();
				LLRect r = getRect();
				gl_rect_2d_offset_local(0, r.getHeight(), r.getWidth(), r.getHeight() - (S32)font->getLineHeight() - 1, 
					titlebar_focus_color % alpha, 0, TRUE);
			}
		}
	}

	LLPanel::updateDefaultBtn();

	if( getDefaultButton() )
	{
		if (hasFocus() && getDefaultButton()->getEnabled())
		{
			LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();
			// is this button a direct descendent and not a nested widget (e.g. checkbox)?
			BOOL focus_is_child_button = dynamic_cast<LLButton*>(focus_ctrl) != NULL && dynamic_cast<LLButton*>(focus_ctrl)->getParent() == this;
			// only enable default button when current focus is not a button
			getDefaultButton()->setBorderEnabled(!focus_is_child_button);
		}
		else
		{
			getDefaultButton()->setBorderEnabled(FALSE);
		}
	}
	if (isMinimized())
	{
		for (S32 i = 0; i < BUTTON_COUNT; i++)
		{
			drawChild(mButtons[i]);
		}
		drawChild(mDragHandle);
	}
	else
	{
		// don't call LLPanel::draw() since we've implemented custom background rendering
		LLView::draw();
	}

	if( isBackgroundVisible() )
	{
		// add in a border to improve spacialized visual aclarity ;)
		// use lines instead of gl_rect_2d so we can round the edges as per james' recommendation
		LLUI::setLineWidth(1.5f);
		LLColor4 outlineColor = gFocusMgr.childHasKeyboardFocus(this) ? LLUI::sColorsGroup->getColor("FloaterFocusBorderColor") : LLUI::sColorsGroup->getColor("FloaterUnfocusBorderColor");
		gl_rect_2d_offset_local(0, getRect().getHeight() + 1, getRect().getWidth() + 1, 0, outlineColor, -LLPANEL_BORDER_WIDTH, FALSE);
		LLUI::setLineWidth(1.f);
	}

	// update tearoff button for torn off floaters
	// when last host goes away
	if (mCanTearOff && !getHost())
	{
		LLFloater* old_host = mLastHostHandle.get();
		if (!old_host)
		{
			setCanTearOff(FALSE);
		}
	}
}

void	LLFloater::drawShadow(LLPanel* panel)
{
	S32 left = LLPANEL_BORDER_WIDTH;
	S32 top = panel->getRect().getHeight() - LLPANEL_BORDER_WIDTH;
	S32 right = panel->getRect().getWidth() - LLPANEL_BORDER_WIDTH;
	S32 bottom = LLPANEL_BORDER_WIDTH;

	static LLUICachedControl<S32> shadow_offset_S32 ("DropShadowFloater", 0);
	static LLColor4 shadow_color = LLUI::sColorsGroup->getColor("ColorDropShadow");
	F32 shadow_offset = (F32)shadow_offset_S32;

	if (!panel->isBackgroundOpaque())
	{
		shadow_offset *= 0.2f;
		shadow_color.mV[VALPHA] *= 0.5f;
	}
	gl_drop_shadow(left, top, right, bottom,
		shadow_color % getCurrentTransparency(),
		ll_round(shadow_offset));
}

void LLFloater::updateTransparency(LLView* view, ETypeTransparency transparency_type)
{
	if (view)
	{
		if (view->isCtrl())
		{
			static_cast<LLUICtrl*>(view)->setTransparencyType(transparency_type);
		}

		for (LLView* pChild : *view->getChildList())
		{
			if ((pChild->getChildCount()) || (pChild->isCtrl()))
				updateTransparency(pChild, transparency_type);
		}
	}
}

void LLFloater::updateTransparency(ETypeTransparency transparency_type)
{
	updateTransparency(this, transparency_type);
}

void	LLFloater::setCanMinimize(BOOL can_minimize)
{
	// if removing minimize/restore button programmatically,
	// go ahead and unminimize floater
	if (!can_minimize)
	{
		setMinimized(FALSE);
	}

	mButtonsEnabled[BUTTON_MINIMIZE] = can_minimize && !isMinimized();
	mButtonsEnabled[BUTTON_RESTORE]  = can_minimize &&  isMinimized();

	updateButtons();
}

void	LLFloater::setCanClose(BOOL can_close)
{
	mButtonsEnabled[BUTTON_CLOSE] = can_close;

	updateButtons();
}

void	LLFloater::setCanTearOff(BOOL can_tear_off)
{
	mCanTearOff = can_tear_off;
	mButtonsEnabled[BUTTON_TEAR_OFF] = mCanTearOff && !mHostHandle.isDead();

	updateButtons();
}


void	LLFloater::setCanResize(BOOL can_resize)
{
	if (mResizable && !can_resize)
	{
		for (S32 i = 0; i < 4; i++) 
		{
			removeChild(mResizeBar[i]);
			delete mResizeBar[i];
			mResizeBar[i] = NULL; 

			removeChild(mResizeHandle[i]);
			delete mResizeHandle[i];
			mResizeHandle[i] = NULL;
		}
	}
	else if (!mResizable && can_resize)
	{
		addResizeCtrls();
		enableResizeCtrls(can_resize);
	}
	mResizable = can_resize;
}

void LLFloater::setCanDrag(BOOL can_drag)
{
	// if we delete drag handle, we no longer have access to the floater's title
	// so just enable/disable it
	if (!can_drag && mDragHandle->getEnabled())
	{
		mDragHandle->setEnabled(FALSE);
	}
	else if (can_drag && !mDragHandle->getEnabled())
	{
		mDragHandle->setEnabled(TRUE);
	}
}

void LLFloater::updateButtons()
{
	S32 button_count = 0;
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		if(!mButtons[i]) continue;
		mButtons[i]->setEnabled(mButtonsEnabled[i]);

		if (mButtonsEnabled[i] 
			// *HACK: always render close button for hosted floaters
			// so that users don't accidentally hit the button when closing multiple windows
			// in the chatterbox
			|| (i == BUTTON_CLOSE && mButtonScale != 1.f))
		{
			button_count++;

			LLRect btn_rect;
			if (mDragOnLeft)
			{
				btn_rect.setLeftTopAndSize(
					LLPANEL_BORDER_WIDTH,
					getRect().getHeight() - CLOSE_BOX_FROM_TOP - (LLFLOATER_CLOSE_BOX_SIZE + 1) * button_count,
					ll_round((F32)LLFLOATER_CLOSE_BOX_SIZE * mButtonScale),
					ll_round((F32)LLFLOATER_CLOSE_BOX_SIZE * mButtonScale));
			}
			else
			{
				btn_rect.setLeftTopAndSize(
					getRect().getWidth() - LLPANEL_BORDER_WIDTH - (LLFLOATER_CLOSE_BOX_SIZE + 1) * button_count,
					getRect().getHeight() - CLOSE_BOX_FROM_TOP,
					ll_round((F32)LLFLOATER_CLOSE_BOX_SIZE * mButtonScale),
					ll_round((F32)LLFLOATER_CLOSE_BOX_SIZE * mButtonScale));
			}

			mButtons[i]->setRect(btn_rect);
			mButtons[i]->setVisible(TRUE);
			// the restore button should have a tab stop so that it takes action when you Ctrl-Tab to a minimized floater
			mButtons[i]->setTabStop(i == BUTTON_RESTORE);
		}
		else if (mButtons[i])
		{
			mButtons[i]->setVisible(FALSE);
		}
	}
	if (mDragHandle)
		mDragHandle->setMaxTitleWidth(getRect().getWidth() - (button_count * (LLFLOATER_CLOSE_BOX_SIZE + 1)));
}

void LLFloater::buildButtons()
{
	for (S32 i = 0; i < BUTTON_COUNT; i++)
	{
		LLRect btn_rect;
		if (mDragOnLeft)
		{
			btn_rect.setLeftTopAndSize(
				LLPANEL_BORDER_WIDTH,
				getRect().getHeight() - CLOSE_BOX_FROM_TOP - (LLFLOATER_CLOSE_BOX_SIZE + 1) * (i + 1),
				ll_round(LLFLOATER_CLOSE_BOX_SIZE * mButtonScale),
				ll_round(LLFLOATER_CLOSE_BOX_SIZE * mButtonScale));
		}
		else
		{
			btn_rect.setLeftTopAndSize(
				getRect().getWidth() - LLPANEL_BORDER_WIDTH - (LLFLOATER_CLOSE_BOX_SIZE + 1) * (i + 1),
				getRect().getHeight() - CLOSE_BOX_FROM_TOP,
				ll_round(LLFLOATER_CLOSE_BOX_SIZE * mButtonScale),
				ll_round(LLFLOATER_CLOSE_BOX_SIZE * mButtonScale));
		}

		LLButton* buttonp = new LLButton(
			sButtonNames[i],
			btn_rect,
			sButtonActiveImageNames[i],
			sButtonPressedImageNames[i],
			LLStringUtil::null,
			boost::bind(sButtonCallbacks[i],this),
			LLFontGL::getFontSansSerif());

		buttonp->setTabStop(FALSE);
		buttonp->setFollowsTop();
		buttonp->setFollowsRight();
		buttonp->setToolTip( sButtonToolTips[i] );
		buttonp->setImageColor(LLUI::sColorsGroup->getColor("FloaterButtonImageColor"));
		buttonp->setImageHoverSelected(LLUI::getUIImage(sButtonPressedImageNames[i]));
		buttonp->setImageHoverUnselected(LLUI::getUIImage(sButtonPressedImageNames[i]));
		buttonp->setScaleImage(TRUE);
		buttonp->setSaveToXML(false);
		addChild(buttonp);
		mButtons[i] = buttonp;
	}

	updateButtons();
}

/////////////////////////////////////////////////////
// LLFloaterView

LLFloaterView::LLFloaterView( const std::string& name, const LLRect& rect )
:	LLUICtrl( name, rect, FALSE, NULL, FOLLOWS_ALL ),
	mFocusCycleMode(FALSE),
	mSnapOffsetBottom(0)
{
	setTabStop(FALSE);
	resetStartingFloaterPosition();
}

// By default, adjust vertical.
void LLFloaterView::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	reshapeFloater(width, height, called_from_parent, ADJUST_VERTICAL_YES);
}

// When reshaping this view, make the floaters follow their closest edge.
void LLFloaterView::reshapeFloater(S32 width, S32 height, BOOL called_from_parent, BOOL adjust_vertical)
{
	S32 old_width = getRect().getWidth();
	S32 old_height = getRect().getHeight();

	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		LLFloater* floaterp = (LLFloater*)viewp;
		if (floaterp->isDependent())
		{
			// dependents use same follow flags as their "dependee"
			continue;
		}

		// Make if follow the edge it is closest to
		U32 follow_flags = 0x0;

		if (floaterp->isMinimized())
		{
			follow_flags |= (FOLLOWS_LEFT | FOLLOWS_TOP);
		}
		else
		{
			LLRect r = floaterp->getRect();

			// Compute absolute distance from each edge of screen
			S32 left_offset = llabs(r.mLeft - 0);
			S32 right_offset = llabs(old_width - r.mRight);

			S32 top_offset = llabs(old_height - r.mTop);
			S32 bottom_offset = llabs(r.mBottom - 0);


			if (left_offset < right_offset)
			{
				follow_flags |= FOLLOWS_LEFT;
			}
			else
			{
				follow_flags |= FOLLOWS_RIGHT;
			}

			// "No vertical adjustment" usually means that the bottom of the view
			// has been pushed up or down.  Hence we want the floaters to follow
			// the top.
			if (!adjust_vertical)
			{
				follow_flags |= FOLLOWS_TOP;
			}
			else if (top_offset < bottom_offset)
			{
				follow_flags |= FOLLOWS_TOP;
			}
			else
			{
				follow_flags |= FOLLOWS_BOTTOM;
			}
		}

		floaterp->setFollows(follow_flags);

		//RN: all dependent floaters copy follow behavior of "parent"
		for(LLFloater::handle_set_iter_t dependent_it = floaterp->mDependents.begin();
			dependent_it != floaterp->mDependents.end(); ++dependent_it)
		{
			LLFloater* dependent_floaterp = dependent_it->get();
			if (dependent_floaterp)
			{
				dependent_floaterp->setFollows(follow_flags);
			}
		}
	}

	LLView::reshape(width, height, called_from_parent);
}


void LLFloaterView::restoreAll()
{
	// make sure all subwindows aren't minimized
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLFloater* floaterp = (LLFloater*)*child_it;
		floaterp->setMinimized(FALSE);
	}

	// *FIX: make sure dependents are restored

	// children then deleted by default view constructor
}


void LLFloaterView::getNewFloaterPosition(S32* left,S32* top)
{
	// Workaround: mRect may change between when this object is created and the first time it is used.
	static BOOL first = TRUE;
	if( first )
	{
		resetStartingFloaterPosition();
		first = FALSE;
	}
	
	const S32 FLOATER_PAD = 16;
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	LLRect full_window(0, window_size.mY, window_size.mX, 0);
	LLRect floater_creation_rect(
		160,
		full_window.getHeight() - 2 * MENU_BAR_HEIGHT,
		full_window.getWidth() * 2 / 3,
		130 );
	floater_creation_rect.stretch( -FLOATER_PAD );

	*left = mNextLeft;
	*top = mNextTop;

	const S32 STEP = 25;
	S32 bottom = floater_creation_rect.mBottom + 2 * STEP;
	S32 right = floater_creation_rect.mRight - 4 * STEP;

	mNextTop -= STEP;
	mNextLeft += STEP;

	if( (mNextTop < bottom ) || (mNextLeft > right) )
	{
		mColumn++;
		mNextTop = floater_creation_rect.mTop;
		mNextLeft = STEP * mColumn;

		if( (mNextTop < bottom) || (mNextLeft > right) )
		{
			// Advancing the column didn't work, so start back at the beginning
			resetStartingFloaterPosition();
		}
	}
}

void LLFloaterView::resetStartingFloaterPosition()
{
	const S32 FLOATER_PAD = 16;
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	LLRect full_window(0, window_size.mY, window_size.mX, 0);
	LLRect floater_creation_rect(
		160,
		full_window.getHeight() - 2 * MENU_BAR_HEIGHT,
		full_window.getWidth() * 2 / 3,
		130 );
	floater_creation_rect.stretch( -FLOATER_PAD );

	mNextLeft = floater_creation_rect.mLeft;
	mNextTop = floater_creation_rect.mTop;
	mColumn = 0;
}

LLRect LLFloaterView::findNeighboringPosition( LLFloater* reference_floater, LLFloater* neighbor )
{
	LLRect base_rect = reference_floater->getRect();
	S32 width = neighbor->getRect().getWidth();
	S32 height = neighbor->getRect().getHeight();
	LLRect new_rect = neighbor->getRect();

	LLRect expanded_base_rect = base_rect;
	expanded_base_rect.stretch(10);
	for(LLFloater::handle_set_iter_t dependent_it = reference_floater->mDependents.begin();
		dependent_it != reference_floater->mDependents.end(); ++dependent_it)
	{
		LLFloater* sibling = dependent_it->get();
		// check for dependents within 10 pixels of base floater
		if (sibling && 
			sibling != neighbor && 
			sibling->getVisible() && 
			expanded_base_rect.overlaps(sibling->getRect()))
		{
			base_rect.unionWith(sibling->getRect());
		}
	}

	S32 left_margin = llmax(0, base_rect.mLeft);
	S32 right_margin = llmax(0, getRect().getWidth() - base_rect.mRight);
	S32 top_margin = llmax(0, getRect().getHeight() - base_rect.mTop);
	S32 bottom_margin = llmax(0, base_rect.mBottom);

	// find position for floater in following order
	// right->left->bottom->top
	for (S32 i = 0; i < 5; i++)
	{
		if (right_margin > width)
		{
			new_rect.translate(base_rect.mRight - neighbor->getRect().mLeft, base_rect.mTop - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (left_margin > width)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mRight, base_rect.mTop - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (bottom_margin > height)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft, base_rect.mBottom - neighbor->getRect().mTop);
			return new_rect;
		}
		else if (top_margin > height)
		{
			new_rect.translate(base_rect.mLeft - neighbor->getRect().mLeft, base_rect.mTop - neighbor->getRect().mBottom);
			return new_rect;
		}

		// keep growing margins to find "best" fit
		left_margin += 20;
		right_margin += 20;
		top_margin += 20;
		bottom_margin += 20;
	}

	// didn't find anything, return initial rect
	return new_rect;
}

void LLFloaterView::bringToFront(LLFloater* child, BOOL give_focus)
{
	// Stop recursive call sequence
	//   LLFloaterView::bringToFront calls
	//   LLFloater::setFocus         calls
	//   LLFloater::setFrontmost     calls this again.
	static bool recursive;
	if (recursive) { return; }
	AIRecursive enter(recursive);

	// *TODO: make this respect floater's mAutoFocus value, instead of
	// using parameter
	if (child->getHost())
 	{
		// this floater is hosted elsewhere and hence not one of our children, abort
		return;
	}
	std::vector<LLView*> floaters_to_move;
	// Look at all floaters...tab
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		LLFloater *floater = (LLFloater *)viewp;

		// ...but if I'm a dependent floater...
		if (child->isDependent())
		{
			// ...look for floaters that have me as a dependent...
			LLFloater::handle_set_iter_t found_dependent = floater->mDependents.find(child->getHandle());

			if (found_dependent != floater->mDependents.end())
			{
				// ...and make sure all children of that floater (including me) are brought to front...
				for(LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
					dependent_it != floater->mDependents.end(); )
				{
					LLFloater* sibling = dependent_it->get();
					if (sibling)
					{
						floaters_to_move.push_back(sibling);
					}
					++dependent_it;
				}
				//...before bringing my parent to the front...
				floaters_to_move.push_back(floater);
			}
		}
	}

	std::vector<LLView*>::iterator view_it;
	for(view_it = floaters_to_move.begin(); view_it != floaters_to_move.end(); ++view_it)
	{
		LLFloater* floaterp = (LLFloater*)(*view_it);
		sendChildToFront(floaterp);

		// always unminimize dependee, but allow dependents to stay minimized
		if (!floaterp->isDependent())
		{
			floaterp->setMinimized(FALSE);
		}
	}
	floaters_to_move.clear();

	// ...then bringing my own dependents to the front...
	for(LLFloater::handle_set_iter_t dependent_it = child->mDependents.begin();
		dependent_it != child->mDependents.end(); )
	{
		LLFloater* dependent = dependent_it->get();
		if (dependent)
		{
			sendChildToFront(dependent);
			//don't un-minimize dependent windows automatically
			// respect user's wishes
			//dependent->setMinimized(FALSE);
		}
		++dependent_it;
	}

	// ...and finally bringing myself to front 
	// (do this last, so that I'm left in front at end of this call)
	if( *getChildList()->begin() != child ) 
	{
		sendChildToFront(child);
	}
	child->setMinimized(FALSE);
	if (give_focus && !gFocusMgr.childHasKeyboardFocus(child))
	{
		child->setFocus(TRUE);
		// floater did not take focus, so relinquish focus to world
		if (!child->hasFocus())
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}
	}
}

void LLFloaterView::highlightFocusedFloater()
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLFloater *floater = (LLFloater *)(*child_it);

		// skip dependent floaters, as we'll handle them in a batch along with their dependee(?)
		if (floater->isDependent())
		{
			continue;
		}

		BOOL floater_or_dependent_has_focus = gFocusMgr.childHasKeyboardFocus(floater);
		for(LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
			dependent_it != floater->mDependents.end(); 
			++dependent_it)
		{
			LLFloater* dependent_floaterp = dependent_it->get();
			if (dependent_floaterp && gFocusMgr.childHasKeyboardFocus(dependent_floaterp))
			{
				floater_or_dependent_has_focus = TRUE;
			}
		}

		// now set this floater and all its dependents
		floater->setForeground(floater_or_dependent_has_focus);

		for(LLFloater::handle_set_iter_t dependent_it = floater->mDependents.begin();
			dependent_it != floater->mDependents.end(); )
		{
			LLFloater* dependent_floaterp = dependent_it->get();
			if (dependent_floaterp)
			{
				dependent_floaterp->setForeground(floater_or_dependent_has_focus);
			}
			++dependent_it;
		}
			
		floater->cleanupHandles();
	}
}

void LLFloaterView::unhighlightFocusedFloater()
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLFloater *floater = (LLFloater *)(*child_it);

		floater->setForeground(FALSE);
	}
}

void LLFloaterView::focusFrontFloater()
{
	LLFloater* floaterp = getFrontmost();
	if (floaterp)
	{
		floaterp->setFocus(TRUE);
	}
}

void LLFloaterView::getMinimizePosition(S32 *left, S32 *bottom)
{
	S32 col = 0;
	LLRect snap_rect_local = getLocalSnapRect();
	for(S32 row = snap_rect_local.mBottom;
		row < snap_rect_local.getHeight() - LLFLOATER_HEADER_SIZE;
		row += LLFLOATER_HEADER_SIZE ) //loop rows
	{
		for(col = snap_rect_local.mLeft;
			col < snap_rect_local.getWidth() - MINIMIZED_WIDTH;
			col += MINIMIZED_WIDTH)
		{
			bool foundGap = TRUE;
			for(child_list_const_iter_t child_it = getChildList()->begin();
				child_it != getChildList()->end();
				++child_it) //loop floaters
			{
				// Examine minimized children.
				LLFloater* floater = (LLFloater*)((LLView*)*child_it);
				if(floater->isMinimized()) 
				{
					LLRect r = floater->getRect();
					if((r.mBottom < (row + LLFLOATER_HEADER_SIZE))
					   && (r.mBottom > (row - LLFLOATER_HEADER_SIZE))
					   && (r.mLeft < (col + MINIMIZED_WIDTH))
					   && (r.mLeft > (col - MINIMIZED_WIDTH)))
					{
						// needs the check for off grid. can't drag,
						// but window resize makes them off
						foundGap = FALSE;
						break;
					}
				}
			} //done floaters
			if(foundGap)
			{
				*left = col;
				*bottom = row;
				return; //done
			}
		} //done this col
	}

	// crude - stack'em all at 0,0 when screen is full of minimized
	// floaters.
	*left = snap_rect_local.mLeft;
	*bottom = snap_rect_local.mBottom;
}


void LLFloaterView::destroyAllChildren()
{
	LLView::deleteAllChildren();
}

void LLFloaterView::closeAllChildren(bool app_quitting)
{
	// iterate over a copy of the list, because closing windows will destroy
	// some windows on the list.
	child_list_t child_list = *(getChildList());

	for (child_list_const_iter_t it = child_list.begin(); it != child_list.end(); ++it)
	{
		LLView* viewp = *it;
		child_list_const_iter_t exists = std::find(getChildList()->begin(), getChildList()->end(), viewp);
		if (exists == getChildList()->end())
		{
			// this floater has already been removed
			continue;
		}

		LLFloater* floaterp = (LLFloater*)viewp;

		// Attempt to close floater.  This will cause the "do you want to save"
		// dialogs to appear.
		// Skip invisible floaters if we're not quitting (STORM-192).
		if (floaterp->canClose() && !floaterp->isDead() &&
			(app_quitting || floaterp->getVisible()))
		{
			floaterp->close(app_quitting);
		}
	}
}

// <edit>
void LLFloaterView::minimizeAllChildren()
{
	// iterate over a copy of the list, because closing windows will destroy
	// some windows on the list.
	child_list_t child_list = *(getChildList());

	for (child_list_const_iter_t it = child_list.begin(); it != child_list.end(); ++it)
	{
		LLView* viewp = *it;
		child_list_const_iter_t exists = std::find(getChildList()->begin(), getChildList()->end(), viewp);
		if (exists == getChildList()->end())
		{
			// this floater has already been removed
			continue;
		}

		LLFloater* floaterp = (LLFloater*)viewp;

		if (!floaterp->isDead())
		{
			floaterp->setMinimized(TRUE);
		}
	}
}
// </edit>

BOOL LLFloaterView::allChildrenClosed()
{
	// see if there are any visible floaters (some floaters "close"
	// by setting themselves invisible)
	for (child_list_const_iter_t it = getChildList()->begin(); it != getChildList()->end(); ++it)
	{
		LLView* viewp = *it;
		LLFloater* floaterp = (LLFloater*)viewp;

		if (floaterp->getVisible() && !floaterp->isDead() && floaterp->isCloseable())
		{
			return false;
		}
	}
	return true;
}


void LLFloaterView::refresh()
{
	// Constrain children to be entirely on the screen
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLFloater* floaterp = dynamic_cast<LLFloater*>(*child_it);
		if (floaterp && floaterp->getVisible() )
		{
			// minimized floaters are kept fully onscreen
			adjustToFitScreen(floaterp, !floaterp->isMinimized());
		}
	}
}

void LLFloaterView::adjustToFitScreen(LLFloater* floater, BOOL allow_partial_outside)
{
	if (floater->getParent() != this)
	{
		// floater is hosted elsewhere, so ignore
		return;
	}
	LLRect::tCoordType screen_width = getSnapRect().getWidth();
	LLRect::tCoordType screen_height = getSnapRect().getHeight();

	
	// only automatically resize non-minimized, resizable floaters
	if( floater->isResizable() && !floater->isMinimized() )
	{
		LLRect view_rect = floater->getRect();
		S32 old_width = view_rect.getWidth();
		S32 old_height = view_rect.getHeight();
		S32 min_width;
		S32 min_height;
		floater->getResizeLimits( &min_width, &min_height );

		// Make sure floater isn't already smaller than its min height/width?
		S32 new_width = llmax( min_width, old_width );
		S32 new_height = llmax( min_height, old_height);

		if((new_width > screen_width) || (new_height > screen_height))
		{
			// We have to make this window able to fit on screen
			new_width = llmin(new_width, screen_width);
			new_height = llmin(new_height, screen_height);

			// ...while respecting minimum width/height
			new_width = llmax(new_width, min_width);
			new_height = llmax(new_height, min_height);

			LLRect new_rect;
			new_rect.setLeftTopAndSize(view_rect.mLeft,view_rect.mTop,new_width, new_height);

			floater->setShape(new_rect);

			if (floater->followsRight())
			{
				floater->translate(old_width - new_width, 0);
			}

			if (floater->followsTop())
			{
				floater->translate(0, old_height - new_height);
			}
		}
	}

	// move window fully onscreen
	if (floater->translateIntoRect( getLocalSnapRect(), allow_partial_outside ))
	{
		floater->clearSnapTarget();
	}
}

void LLFloaterView::draw()
{
	refresh();

	// hide focused floater if in cycle mode, so that it can be drawn on top
	LLFloater* focused_floater = getFocusedFloater();

	if (mFocusCycleMode && focused_floater)
	{
		child_list_const_iter_t child_it = getChildList()->begin();
		for (;child_it != getChildList()->end(); ++child_it)
		{
			if ((*child_it) != focused_floater)
			{
				drawChild(*child_it);
			}
		}

		drawChild(focused_floater, -TABBED_FLOATER_OFFSET, TABBED_FLOATER_OFFSET);
	}
	else
	{
		LLView::draw();
	}
}

LLRect LLFloaterView::getSnapRect() const
{
	LLRect snap_rect = getRect();
	snap_rect.mBottom += mSnapOffsetBottom;

	return snap_rect;
}

LLFloater *LLFloaterView::getFocusedFloater() const
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLUICtrl* ctrlp = (*child_it)->isCtrl() ? static_cast<LLUICtrl*>(*child_it) : NULL;
		if ( ctrlp && ctrlp->hasFocus() )
		{
			return static_cast<LLFloater *>(ctrlp);
		}
	}
	return NULL;
}

LLFloater *LLFloaterView::getFrontmost() const
{
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		if ( viewp->getVisible() && !viewp->isDead())
		{
			return (LLFloater *)viewp;
		}
	}
	return NULL;
}

LLFloater *LLFloaterView::getBackmost() const
{
	LLFloater* back_most = NULL;
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		if ( viewp->getVisible() )
		{
			back_most = (LLFloater *)viewp;
		}
	}
	return back_most;
}

void LLFloaterView::syncFloaterTabOrder()
{
	// bring focused floater to front
	for ( child_list_const_reverse_iter_t child_it = getChildList()->rbegin(); child_it != getChildList()->rend(); ++child_it)
	{
		LLFloater* floaterp = (LLFloater*)*child_it;
		if (gFocusMgr.childHasKeyboardFocus(floaterp))
		{
			bringToFront(floaterp, FALSE);
			break;
		}
	}

	// then sync draw order to tab order
	for ( child_list_const_reverse_iter_t child_it = getChildList()->rbegin(); child_it != getChildList()->rend(); ++child_it)
	{
		LLFloater* floaterp = (LLFloater*)*child_it;
		moveChildToFrontOfTabGroup(floaterp);
	}
}

LLFloater*	LLFloaterView::getParentFloater(LLView* viewp) const
{
	LLView* parentp = viewp->getParent();

	while(parentp && parentp != this)
	{
		viewp = parentp;
		parentp = parentp->getParent();
	}

	if (parentp == this)
	{
		return (LLFloater*)viewp;
	}

	return NULL;
}

S32 LLFloaterView::getZOrder(LLFloater* child)
{
	S32 rv = 0;
	for ( child_list_const_iter_t child_it = getChildList()->begin(); child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		if(viewp == child)
		{
			break;
		}
		++rv;
	}
	return rv;
}

void LLFloaterView::pushVisibleAll(BOOL visible, const skip_list_t& skip_list)
{
	for (child_list_const_iter_t child_iter = getChildList()->begin();
		 child_iter != getChildList()->end(); ++child_iter)
	{
		LLView *view = *child_iter;
		if (skip_list.find(view) == skip_list.end())
		{
			view->pushVisible(visible);
		}
	}
}

void LLFloaterView::popVisibleAll(const skip_list_t& skip_list)
{
	// make a copy of the list since some floaters change their
	// order in the childList when changing visibility.
	child_list_t child_list_copy = *getChildList();

	for (child_list_const_iter_t child_iter = child_list_copy.begin();
		 child_iter != child_list_copy.end(); ++child_iter)
	{
		LLView *view = *child_iter;
		if (skip_list.find(view) == skip_list.end())
		{
			view->popVisible();
		}
	}
}



// virtual
LLXMLNodePtr LLFloater::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLPanel::getXML();

	node->setName(LL_FLOATER_TAG);

	node->createChild("title", TRUE)->setStringValue(getCurrentTitle());

	node->createChild("can_resize", TRUE)->setBoolValue(isResizable());

	node->createChild("can_minimize", TRUE)->setBoolValue(isMinimizeable());

	node->createChild("can_close", TRUE)->setBoolValue(isCloseable());

	node->createChild("can_drag_on_left", TRUE)->setBoolValue(isDragOnLeft());

	node->createChild("min_width", TRUE)->setIntValue(getMinWidth());

	node->createChild("min_height", TRUE)->setIntValue(getMinHeight());

	node->createChild("can_tear_off", TRUE)->setBoolValue(mCanTearOff);
	
	return node;
}

// static
LLView* LLFloater::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string name("floater");
	node->getAttributeString("name", name);

	LLFloater *floaterp = new LLFloater(name);

	std::string filename;
	node->getAttributeString("filename", filename);

	if (filename.empty())
	{
		// for local registry callbacks; define in constructor, referenced in XUI or postBuild
		floaterp->getCommitCallbackRegistrar().pushScope();
		floaterp->getEnableCallbackRegistrar().pushScope();
		// Load from node
		floaterp->initFloaterXML(node, parent, factory);
		floaterp->getCommitCallbackRegistrar().popScope();
		floaterp->getEnableCallbackRegistrar().popScope();
	}
	else
	{
		// Load from file
		factory->buildFloater(floaterp, filename);
	}

	return floaterp;
}

LLTrace::BlockTimerStatHandle POST_BUILD("Floater Post Build");
void LLFloater::initFloaterXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory, BOOL open)	/* Flawfinder: ignore */
{
	std::string name(getName());
	std::string title(getCurrentTitle());
	std::string short_title(getShortTitle());
	std::string rect_control("");
	BOOL resizable = isResizable();
	S32 min_width = getMinWidth();
	S32 min_height = getMinHeight();
	BOOL drag_on_left = isDragOnLeft();
	BOOL minimizable = isMinimizeable();
	BOOL close_btn = isCloseable();
	LLRect rect;

	node->getAttributeString("name", name);
	node->getAttributeString("title", title);
	node->getAttributeString("short_title", short_title);
	node->getAttributeString("rect_control", rect_control);
	node->getAttributeBOOL("can_resize", resizable);
	node->getAttributeBOOL("can_minimize", minimizable);
	node->getAttributeBOOL("can_close", close_btn);
	node->getAttributeBOOL("can_drag_on_left", drag_on_left);
	node->getAttributeS32("min_width", min_width);
	node->getAttributeS32("min_height", min_height);

	if (! rect_control.empty())
	{
		setRectControl(rect_control);
	}

	createRect(node, rect, parent, LLRect());
	
	setRect(rect);
	setName(name);
	
	initFloater(title,
			resizable,
			min_width,
			min_height,
			drag_on_left,
			minimizable,
			close_btn);

	setTitle(title);
	applyTitle ();

	setShortTitle(short_title);

	BOOL can_tear_off;
	if (node->getAttributeBOOL("can_tear_off", can_tear_off))
	{
		setCanTearOff(can_tear_off);
	}
	
	initFromXML(node, parent);

	LLMultiFloater* last_host = LLFloater::getFloaterHost();
	if (node->hasName("multi_floater"))
	{
		LLFloater::setFloaterHost((LLMultiFloater*) this);
	}

	initChildrenXML(node, factory);

	if (node->hasName("multi_floater"))
	{
		LLFloater::setFloaterHost(last_host);
	}

	BOOL result;
	{
		LL_RECORD_BLOCK_TIME(POST_BUILD);

		result = postBuild();
	}

	if (!result)
	{
		LL_ERRS() << "Failed to construct floater " << name << LL_ENDL;
	}

	applyRectControl();
	if (open)	/* Flawfinder: ignore */
	{
		this->open();	/* Flawfinder: ignore */
	}

	moveResizeHandlesToFront();
}

/*static */void VisibilityPolicy<LLFloater>::show(LLFloater* instance, const LLSD& key)
{
	if (instance) 
	{
		instance->open();
		if (instance->getHost())
		{
			instance->getHost()->open();
		}
	}
}
