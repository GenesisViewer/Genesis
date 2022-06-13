/** 
 * @file lluictrl.cpp
 * @author James Cook, Richard Nelson, Tom Yedwab
 * @brief Abstract base class for UI controls
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
#include "lluictrl.h"
#include "llfocusmgr.h"
#include "llpanel.h"

static LLRegisterWidget<LLUICtrl> r("ui_ctrl");

F32 LLUICtrl::sActiveControlTransparency = 1.0f;
F32 LLUICtrl::sInactiveControlTransparency = 1.0f;

LLUICtrl::CallbackParam::CallbackParam()
:	name("name"),
	function_name("function"),
	parameter("parameter"),
	control_name("control") // Shortcut to control -> "control_name" for backwards compatability
{
	addSynonym(parameter, "userdata");
}

LLUICtrl::EnableControls::EnableControls()
:	enabled("enabled_control"),
	disabled("disabled_control")
{}

LLUICtrl::ControlVisibility::ControlVisibility()
:	visible("visibility_control"),
	invisible("invisibility_control")
{
	addSynonym(visible, "visiblity_control");
	addSynonym(invisible, "invisiblity_control");
}

LLUICtrl::Params::Params()
:	tab_stop("tab_stop", true),
	chrome("chrome", false),
	requests_front("requests_front", false),
	label("label"),
	initial_value("value"),
	init_callback("init_callback"),
	commit_callback("commit_callback"),
	validate_callback("validate_callback"),
	mouseenter_callback("mouseenter_callback"),
	mouseleave_callback("mouseleave_callback"),
	control_name("control_name"),
	font("font", LLFontGL::getFontSansSerif()),
	font_halign("halign"),
	font_valign("valign"),
	length("length"), 	// ignore LLXMLNode cruft
	type("type")   		// ignore LLXMLNode cruft
{
	addSynonym(initial_value, "initial_value");
}

// NOTE: the LLFocusableElement implementation has been moved from here to llfocusmgr.cpp.

//static
const LLUICtrl::Params& LLUICtrl::getDefaultParams()
{
	// Singu Note: We diverge here, not using LLUICtrlFactory::getDefaultParams
	static const Params p;
	return p;
}


LLUICtrl::LLUICtrl(const LLUICtrl::Params& p, const LLViewModelPtr& viewmodel)
:	LLView(p),
	mCommitSignal(nullptr),
	mValidateSignal(nullptr),
	mMouseEnterSignal(nullptr),
	mMouseLeaveSignal(nullptr),
	mMouseDownSignal(nullptr),
	mMouseUpSignal(nullptr),
	mRightMouseDownSignal(nullptr),
	mRightMouseUpSignal(nullptr),
	mDoubleClickSignal(nullptr),
	mViewModel(viewmodel),
	mEnabledControlVariable(nullptr),
	mDisabledControlVariable(nullptr),
	mMakeVisibleControlVariable(nullptr),
	mMakeInvisibleControlVariable(nullptr),
	mIsChrome(FALSE),
	mRequestsFront(p.requests_front),
	mTabStop(TRUE),
	mTentative(FALSE),
	mCommitOnReturn(FALSE),
	mTransparencyType(TT_DEFAULT)
{
}

void LLUICtrl::initFromParams(const Params& p)
{
	LLView::initFromParams(p);

	mRequestsFront = p.requests_front;

	setIsChrome(p.chrome);
	if(p.enabled_controls.isProvided())
	{
		if (p.enabled_controls.enabled.isChosen())
		{
			LLControlVariable* control = findControl(p.enabled_controls.enabled);
			if (control)
				setEnabledControlVariable(control);
		}
		else if(p.enabled_controls.disabled.isChosen())
		{
			LLControlVariable* control = findControl(p.enabled_controls.disabled);
			if (control)
				setDisabledControlVariable(control);
		}
	}
	if(p.controls_visibility.isProvided())
	{
		if (p.controls_visibility.visible.isChosen())
		{
			LLControlVariable* control = findControl(p.controls_visibility.visible);
			if (control)
				setMakeVisibleControlVariable(control);
		}
		else if (p.controls_visibility.invisible.isChosen())
		{
			LLControlVariable* control = findControl(p.controls_visibility.invisible);
			if (control)
				setMakeInvisibleControlVariable(control);
		}
	}

	setTabStop(p.tab_stop);

	if (p.initial_value.isProvided()
		&& !p.control_name.isProvided())
	{
		setValue(p.initial_value);
	}

	if (p.commit_callback.isProvided())
	{
		setCommitCallback(initCommitCallback(p.commit_callback));
	}

	if (p.validate_callback.isProvided())
	{
		setValidateCallback(initEnableCallback(p.validate_callback));
	}

	if (p.init_callback.isProvided())
	{
		if (p.init_callback.function.isProvided())
		{
			p.init_callback.function()(this, p.init_callback.parameter);
		}
		else
		{
			commit_callback_t* initfunc = (CommitCallbackRegistry::getValue(p.init_callback.function_name));
			if (initfunc)
			{
				(*initfunc)(this, p.init_callback.parameter);
			}
		}
	}

	if(p.mouseenter_callback.isProvided())
	{
		setMouseEnterCallback(initCommitCallback(p.mouseenter_callback));
	}

	if(p.mouseleave_callback.isProvided())
	{
		setMouseLeaveCallback(initCommitCallback(p.mouseleave_callback));
	}
}

LLUICtrl::LLUICtrl(const std::string& name, const LLRect rect, BOOL mouse_opaque,
	commit_callback_t commit_callback,
	U32 reshape)
:	// can't make this automatically follow top and left, breaks lots
	// of buttons in the UI. JC 7/20/2002
	LLView( name, rect, mouse_opaque, reshape ),
	mIsChrome(FALSE),
	mRequestsFront(false),
	mTabStop( TRUE ),
	mTentative( FALSE ),
	mViewModel(LLViewModelPtr(new LLViewModel)),
	mEnabledControlVariable(nullptr),
	mDisabledControlVariable(nullptr),
	mMakeVisibleControlVariable(nullptr),
	mMakeInvisibleControlVariable(nullptr),
	mCommitSignal(nullptr),
	mValidateSignal(nullptr),
	mMouseEnterSignal(nullptr),
	mMouseLeaveSignal(nullptr),
	mMouseDownSignal(nullptr),
	mMouseUpSignal(nullptr),
	mRightMouseDownSignal(nullptr),
	mRightMouseUpSignal(nullptr),
	mDoubleClickSignal(nullptr),
	mCommitOnReturn(FALSE),
	mTransparencyType(TT_DEFAULT)
{
	if(commit_callback)
		setCommitCallback(commit_callback);
}

LLUICtrl::~LLUICtrl()
{
	gFocusMgr.releaseFocusIfNeeded( this ); // calls onCommit()

	if( gFocusMgr.getTopCtrl() == this )
	{
		LL_WARNS() << "UI Control holding top ctrl deleted: " << getName() << ".  Top view removed." << LL_ENDL;
		gFocusMgr.removeTopCtrlWithoutCallback( this );
	}

	delete mCommitSignal;
	delete mValidateSignal;
	delete mMouseEnterSignal;
	delete mMouseLeaveSignal;
	delete mMouseDownSignal;
	delete mMouseUpSignal;
	delete mRightMouseDownSignal;
	delete mRightMouseUpSignal;
	delete mDoubleClickSignal;
}

void default_commit_handler(LLUICtrl* ctrl, const LLSD& param)
{}

bool default_enable_handler(LLUICtrl* ctrl, const LLSD& param)
{
	return true;
}


LLUICtrl::commit_signal_t::slot_type LLUICtrl::initCommitCallback(const CommitCallbackParam& cb)
{
	if (cb.function.isProvided())
	{
		if (cb.parameter.isProvided())
			return boost::bind(cb.function(), _1, cb.parameter);
		else
			return cb.function();
	}
	else
	{
		std::string function_name = cb.function_name;
		commit_callback_t* func = (CommitCallbackRegistry::getValue(function_name));
		if (func)
		{
			if (cb.parameter.isProvided())
				return boost::bind((*func), _1, cb.parameter);
			else
				return commit_signal_t::slot_type(*func);
		}
		else if (!function_name.empty())
		{
			LL_WARNS() << "No callback found for: '" << function_name << "' in control: " << getName() << LL_ENDL;
		}
	}
	return default_commit_handler;
}

LLUICtrl::enable_signal_t::slot_type LLUICtrl::initEnableCallback(const EnableCallbackParam& cb)
{
	// Set the callback function
	if (cb.function.isProvided())
	{
		if (cb.parameter.isProvided())
			return boost::bind(cb.function(), this, cb.parameter);
		else
			return cb.function();
	}
	else
	{
		enable_callback_t* func = (EnableCallbackRegistry::getValue(cb.function_name));
		if (func)
		{
			if (cb.parameter.isProvided())
				return boost::bind((*func), this, cb.parameter);
			else
				return enable_signal_t::slot_type(*func);
		}
	}
	return default_enable_handler;
}

// virtual
void LLUICtrl::onMouseEnter(S32 x, S32 y, MASK mask)
{
	if (mMouseEnterSignal)
	{
		(*mMouseEnterSignal)(this, getValue());
	}
}

// virtual
void LLUICtrl::onMouseLeave(S32 x, S32 y, MASK mask)
{
	if(mMouseLeaveSignal)
	{
		(*mMouseLeaveSignal)(this, getValue());
	}
}

//virtual 
BOOL LLUICtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled  = LLView::handleMouseDown(x,y,mask);

	if (mMouseDownSignal)
	{
		(*mMouseDownSignal)(this,x,y,mask);
	}
	LL_DEBUGS() << "LLUICtrl::handleMousedown - handled is returning as: " << handled << "	  " << LL_ENDL;

	return handled;
}

//virtual
BOOL LLUICtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	BOOL handled  = LLView::handleMouseUp(x,y,mask);
	if (mMouseUpSignal)
	{
		(*mMouseUpSignal)(this,x,y,mask);
	}

	return handled;
}

//virtual
BOOL LLUICtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled  = LLView::handleRightMouseDown(x,y,mask);
	if (mRightMouseDownSignal)
	{
		(*mRightMouseDownSignal)(this,x,y,mask);
	}
	return handled;
}

//virtual
BOOL LLUICtrl::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	BOOL handled  = LLView::handleRightMouseUp(x,y,mask);
	if(mRightMouseUpSignal)
	{
		(*mRightMouseUpSignal)(this,x,y,mask);
	}
	return handled;
}

BOOL LLUICtrl::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLView::handleDoubleClick(x, y, mask);
	if (mDoubleClickSignal)
	{
		(*mDoubleClickSignal)(this, x, y, mask);
	}
	return handled;
}

// can't tab to children of a non-tab-stop widget
BOOL LLUICtrl::canFocusChildren() const
{
	return TRUE;//hasTabStop();
}


void LLUICtrl::onCommit()
{
	if (mCommitSignal)
		(*mCommitSignal)(this, getValue());
}

//virtual
BOOL LLUICtrl::isCtrl() const
{
	return TRUE;
}

//virtual 
void LLUICtrl::setValue(const LLSD& value)
{
	mViewModel->setValue(value);
}

//virtual
LLSD LLUICtrl::getValue() const
{
	return mViewModel->getValue();
}

/// When two widgets are displaying the same data (e.g. during a skin
/// change), share their ViewModel.
void    LLUICtrl::shareViewModelFrom(const LLUICtrl& other)
{
	// Because mViewModel is an LLViewModelPtr, this assignment will quietly
	// dispose of the previous LLViewModel -- unless it's already shared by
	// somebody else.
	mViewModel = other.mViewModel;
}

//virtual
LLViewModel* LLUICtrl::getViewModel() const
{
	return mViewModel;
}

//virtual
BOOL LLUICtrl::postBuild()
{
	//
	// Find all of the children that want to be in front and move them to the front
	//

	if (getChildCount() > 0)
	{
		std::vector<LLUICtrl*> childrenToMoveToFront;

		for (LLView::child_list_const_iter_t child_it = beginChild(); child_it != endChild(); ++child_it)
		{
			LLUICtrl* uictrl = dynamic_cast<LLUICtrl*>(*child_it);

			if (uictrl && uictrl->mRequestsFront)
			{
				childrenToMoveToFront.push_back(uictrl);
			}
		}

		for (std::vector<LLUICtrl*>::iterator it = childrenToMoveToFront.begin(); it != childrenToMoveToFront.end(); ++it)
		{
			sendChildToFront(*it);
		}
	}

	return LLView::postBuild();
}

void LLUICtrl::setEnabledControlVariable(LLControlVariable* control)
{
	if (mEnabledControlVariable)
	{
		mEnabledControlConnection.disconnect(); // disconnect current signal
		mEnabledControlVariable = nullptr;
	}
	if (control)
	{
		mEnabledControlVariable = control;
		mEnabledControlConnection = mEnabledControlVariable->getSignal()->connect(boost::bind(&controlListener, _2, getHandle(), std::string("enabled")));
		setEnabled(mEnabledControlVariable->getValue().asBoolean());
	}
}

void LLUICtrl::setDisabledControlVariable(LLControlVariable* control)
{
	if (mDisabledControlVariable)
	{
		mDisabledControlConnection.disconnect(); // disconnect current signal
		mDisabledControlVariable = nullptr;
	}
	if (control)
	{
		mDisabledControlVariable = control;
		mDisabledControlConnection = mDisabledControlVariable->getSignal()->connect(boost::bind(&controlListener, _2, getHandle(), std::string("disabled")));
		setEnabled(!(mDisabledControlVariable->getValue().asBoolean()));
	}
}

void LLUICtrl::setMakeVisibleControlVariable(LLControlVariable* control)
{
	if (mMakeVisibleControlVariable)
	{
		mMakeVisibleControlConnection.disconnect(); // disconnect current signal
		mMakeVisibleControlVariable = nullptr;
	}
	if (control)
	{
		mMakeVisibleControlVariable = control;
		mMakeVisibleControlConnection = mMakeVisibleControlVariable->getSignal()->connect(boost::bind(&controlListener, _2, getHandle(), std::string("visible")));
		setVisible(mMakeVisibleControlVariable->getValue().asBoolean());
	}
}

void LLUICtrl::setMakeInvisibleControlVariable(LLControlVariable* control)
{
	if (mMakeInvisibleControlVariable)
	{
		mMakeInvisibleControlConnection.disconnect(); // disconnect current signal
		mMakeInvisibleControlVariable = nullptr;
	}
	if (control)
	{
		mMakeInvisibleControlVariable = control;
		mMakeInvisibleControlConnection = mMakeInvisibleControlVariable->getSignal()->connect(boost::bind(&controlListener, _2, getHandle(), std::string("invisible")));
		setVisible(!(mMakeInvisibleControlVariable->getValue().asBoolean()));
	}
}
// static
bool LLUICtrl::controlListener(const LLSD& newvalue, LLHandle<LLUICtrl> handle, std::string type)
{
	LLUICtrl* ctrl = handle.get();
	if (ctrl)
	{
		if (type == "value")
		{
			ctrl->setValue(newvalue);
			return true;
		}
		else if (type == "enabled")
		{
			ctrl->setEnabled(newvalue.asBoolean());
			return true;
		}
		else if(type =="disabled")
		{
			ctrl->setEnabled(!newvalue.asBoolean());
			return true;
		}
		else if (type == "visible")
		{
			ctrl->setVisible(newvalue.asBoolean());
			return true;
		}
		else if (type == "invisible")
		{
			ctrl->setVisible(!newvalue.asBoolean());
			return true;
		}
	}
	return false;
}

// virtual
BOOL LLUICtrl::setTextArg( const std::string& key, const LLStringExplicit& text ) 
{ 
	return FALSE; 
}

// virtual
BOOL LLUICtrl::setLabelArg( const std::string& key, const LLStringExplicit& text ) 
{ 
	return FALSE; 
}

// virtual
LLCtrlSelectionInterface* LLUICtrl::getSelectionInterface()	
{ 
	return nullptr;
}

// virtual
LLCtrlListInterface* LLUICtrl::getListInterface()				
{ 
	return nullptr;
}

// virtual
LLCtrlScrollInterface* LLUICtrl::getScrollInterface()			
{ 
	return nullptr;
}

BOOL LLUICtrl::hasFocus() const
{
	return (gFocusMgr.childHasKeyboardFocus(this));
}

void LLUICtrl::setFocus(BOOL b)
{
	// focus NEVER goes to ui ctrls that are disabled!
	if (!getEnabled())
	{
		return;
	}
	if( b )
	{
		if (!hasFocus())
		{
			gFocusMgr.setKeyboardFocus( this );
		}
	}
	else
	{
		if( gFocusMgr.childHasKeyboardFocus(this))
		{
			gFocusMgr.setKeyboardFocus(nullptr );
		}
	}
}

// virtual
void LLUICtrl::setTabStop( BOOL b )	
{ 
	mTabStop = b;
}

// virtual
BOOL LLUICtrl::hasTabStop() const		
{ 
	return mTabStop;
}

// virtual
BOOL LLUICtrl::acceptsTextInput() const
{ 
	return FALSE; 
}

//virtual
BOOL LLUICtrl::isDirty() const
{
	return mViewModel->isDirty();
};

//virtual
void LLUICtrl::resetDirty()
{
	mViewModel->resetDirty();
}

// virtual
void LLUICtrl::onTabInto()				
{
}

// virtual
void LLUICtrl::clear()					
{
}

// virtual
void LLUICtrl::setIsChrome(BOOL is_chrome)
{
	mIsChrome = is_chrome; 
}

// virtual
BOOL LLUICtrl::getIsChrome() const
{ 
	LLView* parent_ctrl = getParent();
	while(parent_ctrl)
	{
		if(parent_ctrl->isCtrl())
		{
			break;	
		}
		parent_ctrl = parent_ctrl->getParent();
	}
	
	if(parent_ctrl)
	{
		return mIsChrome || ((LLUICtrl*)parent_ctrl)->getIsChrome();
	}
	else
	{
		return mIsChrome ; 
	}
}

// this comparator uses the crazy disambiguating logic of LLCompareByTabOrder,
// but to switch up the order so that children that have the default tab group come first
// and those that are prior to the default tab group come last
class CompareByDefaultTabGroup: public LLCompareByTabOrder
{
public:
	CompareByDefaultTabGroup(LLView::child_tab_order_t order, S32 default_tab_group):
			LLCompareByTabOrder(order),
			mDefaultTabGroup(default_tab_group) {}
private:
	/*virtual*/ bool compareTabOrders(const LLView::tab_order_t & a, const LLView::tab_order_t & b) const
	{
		S32 ag = a.first; // tab group for a
		S32 bg = b.first; // tab group for b
		// these two ifs have the effect of moving elements prior to the default tab group to the end of the list 
		// (still sorted relative to each other, though)
		if(ag < mDefaultTabGroup && bg >= mDefaultTabGroup) return false;
		if(bg < mDefaultTabGroup && ag >= mDefaultTabGroup) return true;
		return a < b;  // sort correctly if they're both on the same side of the default tab group
	}
	S32 mDefaultTabGroup;
};


// Sorter for plugging into the query.
// I'd have defined it local to the one method that uses it but that broke the VS 05 compiler. -MG
class LLUICtrl::DefaultTabGroupFirstSorter : public LLQuerySorter, public LLSingleton<DefaultTabGroupFirstSorter>
{
public:
	/*virtual*/ void operator() (LLView * parent, viewList_t &children) const
	{
		children.sort(CompareByDefaultTabGroup(parent->getCtrlOrder(), parent->getDefaultTabGroup()));
	}
};

LLTrace::BlockTimerStatHandle FTM_FOCUS_FIRST_ITEM("Focus First Item");

BOOL LLUICtrl::focusFirstItem(BOOL prefer_text_fields, BOOL focus_flash)
{
	LL_RECORD_BLOCK_TIME(FTM_FOCUS_FIRST_ITEM);
	// try to select default tab group child
	LLCtrlQuery query = getTabOrderQuery();
	// sort things such that the default tab group is at the front
	query.setSorter(DefaultTabGroupFirstSorter::getInstance());
	viewList_t result = query(this);
	if(!result.empty())
	{
		LLUICtrl * ctrl = static_cast<LLUICtrl*>(result.front());
		if(!ctrl->hasFocus())
		{
			ctrl->setFocus(TRUE);
			ctrl->onTabInto();  
			if(focus_flash)
			{
				gFocusMgr.triggerFocusFlash();
			}
		}
		return TRUE;
	}	
	// search for text field first
	if(prefer_text_fields)
	{
		LLCtrlQuery query = getTabOrderQuery();
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
		viewList_t result = query(this);
		if(result.size() > 0)
		{
			LLUICtrl * ctrl = static_cast<LLUICtrl*>(result.front());
			if(!ctrl->hasFocus())
			{
				ctrl->setFocus(TRUE);
				ctrl->onTabInto();  
				gFocusMgr.triggerFocusFlash();
			}
			return TRUE;
		}
	}
	// no text field found, or we don't care about text fields
	result = getTabOrderQuery().run(this);
	if(result.size() > 0)
	{
		LLUICtrl * ctrl = static_cast<LLUICtrl*>(result.front());
		if(!ctrl->hasFocus())
		{
			ctrl->setFocus(TRUE);
			ctrl->onTabInto();  
			gFocusMgr.triggerFocusFlash();
		}
		return TRUE;
	}	
	return FALSE;
}

BOOL LLUICtrl::focusLastItem(BOOL prefer_text_fields)
{
	// search for text field first
	if(prefer_text_fields)
	{
		LLCtrlQuery query = getTabOrderQuery();
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
		viewList_t result = query(this);
		if(result.size() > 0)
		{
			LLUICtrl * ctrl = static_cast<LLUICtrl*>(result.back());
			if(!ctrl->hasFocus())
			{
				ctrl->setFocus(TRUE);
				ctrl->onTabInto();  
				gFocusMgr.triggerFocusFlash();
			}
			return TRUE;
		}
	}
	// no text field found, or we don't care about text fields
	viewList_t result = getTabOrderQuery().run(this);
	if(result.size() > 0)
	{
		LLUICtrl * ctrl = static_cast<LLUICtrl*>(result.back());
		if(!ctrl->hasFocus())
		{
			ctrl->setFocus(TRUE);
			ctrl->onTabInto();  
			gFocusMgr.triggerFocusFlash();
		}
		return TRUE;
	}	
	return FALSE;
}


BOOL LLUICtrl::focusNextItem(BOOL text_fields_only)
{
	// this assumes that this method is called on the focus root.
	LLCtrlQuery query = getTabOrderQuery();
	static LLUICachedControl<bool> tab_to_text_fields_only ("TabToTextFieldsOnly", false);
	if(text_fields_only || tab_to_text_fields_only)
	{
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
	}
	viewList_t result = query(this);
	return focusNext(result);
}

BOOL LLUICtrl::focusPrevItem(BOOL text_fields_only)
{
	// this assumes that this method is called on the focus root.
	LLCtrlQuery query = getTabOrderQuery();
	static LLUICachedControl<bool> tab_to_text_fields_only ("TabToTextFieldsOnly", false);
	if(text_fields_only || tab_to_text_fields_only)
	{
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
	}
	viewList_t result = query(this);
	return focusPrev(result);
}

LLUICtrl* LLUICtrl::findRootMostFocusRoot()
{
	LLUICtrl* focus_root = NULL;
	LLUICtrl* next_view = this;
	while(next_view/* && next_view->hasTabStop()*/)
	{
		if (next_view->isFocusRoot())
		{
			focus_root = next_view;
		}
		next_view = next_view->getParentUICtrl();
	}

	return focus_root;
}


/*
// Don't let the children handle the tool tip.  Handle it here instead.
BOOL LLUICtrl::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen)
{
	BOOL handled = FALSE;
	if (getVisible() && pointInView( x, y ) ) 
	{
		if( !mToolTipMsg.empty() )
		{
			msg = mToolTipMsg;

			// Convert rect local to screen coordinates
			localPointToScreen( 
				0, 0, 
				&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
			localPointToScreen(
				getRect().getWidth(), getRect().getHeight(),
				&(sticky_rect_screen->mRight), &(sticky_rect_screen->mTop) );

			handled = TRUE;
		}
	}

	if (!handled)
	{
		return LLView::handleToolTip(x, y, msg, sticky_rect_screen);
	}

	return handled;
}*/

void LLUICtrl::initFromXML(LLXMLNodePtr node, LLView* parent)
{
	std::string name;
	if(node->getAttributeString("name", name))
		setName(name);

	BOOL has_tab_stop = hasTabStop();
	node->getAttributeBOOL("tab_stop", has_tab_stop);

	setTabStop(has_tab_stop);

	node->getAttributeBOOL("requests_front", mRequestsFront);

	std::string str = node->getName()->mString;
	std::string attrib_str;
	LLXMLNodePtr child_node;

	//Since so many other callback 'types' piggyback off of the commitcallback registrar as well as use the same callback signature
	//we can assemble a nice little static list to iterate down instead of copy-pasting mostly similar code for each instance.
	//Validate/enable callbacks differ, as it uses its own registry/callback signature. This could be worked around with a template, but keeping
	//all the code local to this scope is more beneficial.
	typedef boost::signals2::connection (LLUICtrl::*commit_fn)( const commit_signal_t::slot_type& cb );
	static std::pair<std::string,commit_fn> sCallbackRegistryMap[3] = 
	{
		std::pair<std::string,commit_fn>(".commit_callback",&LLUICtrl::setCommitCallback),
		std::pair<std::string,commit_fn>(".mouseenter_callback",&LLUICtrl::setMouseEnterCallback),
		std::pair<std::string,commit_fn>(".mouseleave_callback",&LLUICtrl::setMouseLeaveCallback)
	};
	for(S32 i= 0; i < sizeof(sCallbackRegistryMap)/sizeof(sCallbackRegistryMap[0]);++i)
	{
		if(node->getChild((str+sCallbackRegistryMap[i].first).c_str(),child_node,false))
		{
			if(child_node->getAttributeString("function",attrib_str))
			{
				commit_callback_t* func = (CommitCallbackRegistry::getValue(attrib_str));
				if (func)
				{
					if(child_node->getAttributeString("parameter",attrib_str))
						(this->*sCallbackRegistryMap[i].second)(boost::bind((*func), this, LLSD(attrib_str)));
					else
						(this->*sCallbackRegistryMap[i].second)(commit_signal_t::slot_type(*func));
				}
			}
		}
	}

	if(node->getChild((str+".validate_callback").c_str(),child_node,false))
	{
		if(child_node->getAttributeString("function",attrib_str))
		{
			enable_callback_t* func = (EnableCallbackRegistry::getValue(attrib_str));
			if (func)
			{
				if(child_node->getAttributeString("parameter",attrib_str))
					setValidateCallback(boost::bind((*func), this, LLSD(attrib_str)));
				else
					setValidateCallback(enable_signal_t::slot_type(*func));
			}
		}
	}
	LLView::initFromXML(node, parent);
	
	if (node->getAttributeString("enabled_control", attrib_str))
	{
		LLControlVariable* control = findControl(attrib_str);
		if (control)
			setEnabledControlVariable(control);
	}

	if (node->getAttributeString("disabled_control", attrib_str))
	{
		LLControlVariable* control = findControl(attrib_str);
		if (control)
			setDisabledControlVariable(control);
	}

	if(node->getAttributeString("visibility_control",attrib_str) || node->getAttributeString("visiblity_control",attrib_str))
	{
		LLControlVariable* control = findControl(attrib_str);
		if (control)
			setMakeVisibleControlVariable(control);
	}
	if(node->getAttributeString("invisibility_control",attrib_str) || node->getAttributeString("invisiblity_control",attrib_str))
	{
		LLControlVariable* control = findControl(attrib_str);
		if (control)
			setMakeInvisibleControlVariable(control);
	}
}

LLXMLNodePtr LLUICtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML(save_children);
	node->createChild("tab_stop", TRUE)->setBoolValue(hasTabStop());

	return node;
}

//static 
LLView* LLUICtrl::fromXML(LLXMLNodePtr node, LLView* parent, class LLUICtrlFactory* factory)
{
	LLUICtrl* ctrl = new LLUICtrl();
	ctrl->initFromXML(node, parent);
	return ctrl;
}


// Skip over any parents that are not LLUICtrl's
//  Used in focus logic since only LLUICtrl elements can have focus
LLUICtrl* LLUICtrl::getParentUICtrl() const
{
	LLView* parent = getParent();
	while (parent)
	{
		if (parent->isCtrl())
		{
			return (LLUICtrl*)(parent);
		}
		else
		{
			parent =  parent->getParent();
		}
	}
	return NULL;
}

// *TODO: Deprecate; for backwards compatability only:
boost::signals2::connection LLUICtrl::setCommitCallback( std::function<void (LLUICtrl*,void*)> cb, void* data)
{
	return setCommitCallback( boost::bind(cb, _1, data));
}
boost::signals2::connection LLUICtrl::setValidateBeforeCommit( std::function<bool (const LLSD& data)> cb )
{
	if (!mValidateSignal) mValidateSignal = new enable_signal_t();
	return mValidateSignal->connect(boost::bind(cb, _2));
}

// virtual
void LLUICtrl::setTentative(BOOL b)									
{ 
	mTentative = b; 
}

// virtual
BOOL LLUICtrl::getTentative() const									
{ 
	return mTentative; 
}

// virtual
void LLUICtrl::setColor(const LLColor4& color)							
{ }
// virtual

void LLUICtrl::setAlpha(F32 alpha)							
{ }

// virtual
void LLUICtrl::setMinValue(LLSD min_value)								
{ }

// virtual
void LLUICtrl::setMaxValue(LLSD max_value)								
{ }

F32 LLUICtrl::getCurrentTransparency()
{
	F32 alpha = 0;

	switch(mTransparencyType)
	{
	case TT_DEFAULT:
		alpha = getDrawContext().mAlpha;
		break;

	case TT_ACTIVE:
		alpha = sActiveControlTransparency;
		break;

	case TT_INACTIVE:
		alpha = sInactiveControlTransparency;
		break;

	case TT_FADING:
		alpha = sInactiveControlTransparency / 2.f;
		break;
	}

	return alpha;
}

void LLUICtrl::setTransparencyType(ETypeTransparency type)
{
	mTransparencyType = type;
}

boost::signals2::connection LLUICtrl::setCommitCallback(const CommitCallbackParam& cb)
{
	return setCommitCallback(initCommitCallback(cb));
}

boost::signals2::connection LLUICtrl::setValidateCallback(const EnableCallbackParam& cb)
{
	return setValidateCallback(initEnableCallback(cb));
}

boost::signals2::connection LLUICtrl::setCommitCallback( const commit_signal_t::slot_type& cb ) 
{ 
	if (!mCommitSignal) mCommitSignal = new commit_signal_t();
	return mCommitSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setValidateCallback( const enable_signal_t::slot_type& cb ) 
{ 
	if (!mValidateSignal) mValidateSignal = new enable_signal_t();
	return mValidateSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setMouseEnterCallback( const commit_signal_t::slot_type& cb ) 
{ 
	if (!mMouseEnterSignal) mMouseEnterSignal = new commit_signal_t();
	return mMouseEnterSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setMouseLeaveCallback( const commit_signal_t::slot_type& cb ) 
{ 
	if (!mMouseLeaveSignal) mMouseLeaveSignal = new commit_signal_t();
	return mMouseLeaveSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setMouseDownCallback( const mouse_signal_t::slot_type& cb ) 
{ 
	if (!mMouseDownSignal) mMouseDownSignal = new mouse_signal_t();
	return mMouseDownSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setMouseUpCallback( const mouse_signal_t::slot_type& cb ) 
{ 
	if (!mMouseUpSignal) mMouseUpSignal = new mouse_signal_t();
	return mMouseUpSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setRightMouseDownCallback( const mouse_signal_t::slot_type& cb ) 
{ 
	if (!mRightMouseDownSignal) mRightMouseDownSignal = new mouse_signal_t();
	return mRightMouseDownSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setRightMouseUpCallback( const mouse_signal_t::slot_type& cb ) 
{ 
	if (!mRightMouseUpSignal) mRightMouseUpSignal = new mouse_signal_t();
	return mRightMouseUpSignal->connect(cb); 
}

boost::signals2::connection LLUICtrl::setDoubleClickCallback( const mouse_signal_t::slot_type& cb ) 
{ 
	if (!mDoubleClickSignal) mDoubleClickSignal = new mouse_signal_t();
	return mDoubleClickSignal->connect(cb); 
}
