/** 
 * @file llcombobox.cpp
 * @brief LLComboBox base class
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

// A control that displays the name of the chosen item, which when
// clicked shows a scrolling box of options.

#include "linden_common.h"

// file includes
#include "llcombobox.h"

// common includes
#include "llstring.h"

// newview includes
#include "llbutton.h"
#include "llkeyboard.h"
#include "llscrolllistctrl.h"
#include "llwindow.h"
#include "llfloater.h"
#include "llscrollbar.h"
#include "llscrolllistcell.h"
#include "llscrolllistitem.h"
#include "llcontrol.h"
#include "llfocusmgr.h"
#include "lllineeditor.h"
#include "v2math.h"

// Globals
S32 LLCOMBOBOX_HEIGHT = 0;
S32 LLCOMBOBOX_WIDTH = 0;
S32 MAX_COMBO_WIDTH = 500;

static LLRegisterWidget<LLComboBox> register_combo_box("combo_box");

LLComboBox::LLComboBox(const std::string& name, const LLRect& rect, const std::string& label, commit_callback_t commit_callback)
:	LLUICtrl(name, rect, TRUE, commit_callback, FOLLOWS_LEFT | FOLLOWS_TOP),
	mTextEntry(NULL),
	mTextEntryTentative(TRUE),
	mArrowImage(NULL),
	mHasAutocompletedText(false),
	mAllowTextEntry(false),
	mAllowNewValues(false),
	mMaxChars(20),
	mPrearrangeCallback( NULL ),
	mTextEntryCallback( NULL ),
	mListPosition(BELOW),
	mSuppressTentative( false ),
	mSuppressAutoComplete( false ),
	mListColor(LLUI::sColorsGroup->getColor("ComboBoxBg")),
	mLastSelectedIndex(-1),
	mLabel(label)
{
	// Always use text box 
	// Text label button
	mButton = new LLButton(mLabel, LLRect(), LLStringUtil::null);
	mButton->setImageUnselected(LLUI::getUIImage("square_btn_32x128.tga"));
	mButton->setImageSelected(LLUI::getUIImage("square_btn_selected_32x128.tga"));
	mButton->setImageDisabled(LLUI::getUIImage("square_btn_32x128.tga"));
	mButton->setImageDisabledSelected(LLUI::getUIImage("square_btn_selected_32x128.tga"));
	mButton->setScaleImage(TRUE);

	mButton->setMouseDownCallback(boost::bind(&LLComboBox::onButtonMouseDown,this));
	mButton->setFont(LLFontGL::getFontSansSerifSmall());
	mButton->setFollows(FOLLOWS_LEFT | FOLLOWS_BOTTOM | FOLLOWS_RIGHT);
	mButton->setHAlign( LLFontGL::LEFT );
	mButton->setRightHPad(2);

	addChild(mButton);

	// disallow multiple selection
	mList = new LLScrollListCtrl(std::string("ComboBox"), LLRect(),
								 boost::bind(&LLComboBox::onItemSelected, this, _2), FALSE);
	mList->setVisible(false);
	mList->setBgWriteableColor(mListColor);
	mList->setCommitOnKeyboardMovement(false);
	addChild(mList);

	// Mouse-down on button will transfer mouse focus to the list
	// Grab the mouse-up event and make sure the button state is correct
	mList->setMouseUpCallback(boost::bind(&LLComboBox::onListMouseUp, this));

	mArrowImage = LLUI::getUIImage("combobox_arrow.tga");
	mButton->setImageOverlay("combobox_arrow.tga", LLFontGL::RIGHT);

	updateLayout();

	mTopLostSignalConnection = setTopLostCallback(boost::bind(&LLComboBox::hideList, this));
}


// virtual
LLXMLNodePtr LLComboBox::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_COMBO_BOX_TAG);

	// Attributes
	node->createChild("allow_text_entry", TRUE)->setBoolValue(mAllowTextEntry);
	node->createChild("max_chars", TRUE)->setIntValue(mMaxChars);

	// Contents
	std::vector<LLScrollListItem*> data_list = mList->getAllData();
	for (std::vector<LLScrollListItem*>::iterator data_itor = data_list.begin(); data_itor != data_list.end(); ++data_itor)
	{
		LLScrollListItem* item = *data_itor;
		LLScrollListCell* cell = item->getColumn(0);
		if (cell)
		{
			LLXMLNodePtr item_node = node->createChild("combo_item", FALSE);
			LLSD value = item->getValue();
			item_node->createChild("value", TRUE)->setStringValue(value.asString());
			item_node->createChild("enabled", TRUE)->setBoolValue(item->getEnabled());
			item_node->setStringValue(cell->getValue().asString());
		}
	}

	return node;
}

// static
LLView* LLComboBox::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	std::string label("");
	node->getAttributeString("label", label);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	BOOL allow_text_entry = FALSE;
	node->getAttributeBOOL("allow_text_entry", allow_text_entry);

	S32 max_chars = 20;
	node->getAttributeS32("max_chars", max_chars);

	LLComboBox* combo_box = new LLComboBox("combo_box", rect, label);
	combo_box->setAllowTextEntry(allow_text_entry, max_chars);

	if (LLFontGL* font = selectFont(node))
		combo_box->mButton->setFont(font);

	const std::string& contents = node->getValue();

	if (contents.find_first_not_of(" \n\t") != contents.npos)
	{
		LL_ERRS() << "Legacy combo box item format used! Please convert to <combo_item> tags!" << LL_ENDL;
	}
	else
	{
		LLXMLNodePtr child;
		for (child = node->getFirstChild(); child.notNull(); child = child->getNextSibling())
		{
			if (child->hasName("combo_item") || child->hasName("combo_box.item"))
			{
				std::string label = child->getTextContents();
				child->getAttributeString("label", label);

				std::string value = label;
				child->getAttributeString("value", value);
				
				LLScrollListItem * item=combo_box->add(label, LLSD(value) );
				
				if(item && child->hasAttribute("tool_tip"))
				{
					std::string tool_tip = label;
					child->getAttributeString("tool_tip", tool_tip);
					item->getColumn(0)->setToolTip(tool_tip);
				}
			}
		}
	}

	//Do this AFTER combo_items are set up so setValue is actually able to select the correct initial entry.
	combo_box->initFromXML(node, parent);

	// if we haven't already gotten a value from our control_name and
	// if providing user text entry or descriptive label
	// don't select an item under the hood
	if (combo_box->getControlName().empty())
	{
		const auto text = combo_box->acceptsTextInput();
		std::string label;
		if (node->getAttributeString("label", label))
			text ? combo_box->setLabel(label) : (void)combo_box->mList->selectItemByLabel(label, FALSE);
		else if (!text && combo_box->mLabel.empty())
			combo_box->selectFirstItem();
	}

	return combo_box;
}

void LLComboBox::setEnabled(BOOL enabled)
{
	LLView::setEnabled(enabled);
	mButton->setEnabled(enabled);
}


LLComboBox::~LLComboBox()
{
	// children automatically deleted, including mMenu, mButton

	// explicitly disconect this signal, since base class destructor might fire top lost
	mTopLostSignalConnection.disconnect();
}


void LLComboBox::clear()
{ 
	if (mTextEntry)
	{
		mTextEntry->setText(LLStringUtil::null);
	}
	mButton->setLabelSelected(LLStringUtil::null);
	mButton->setLabelUnselected(LLStringUtil::null);
	mList->deselectAllItems();
	mLastSelectedIndex = -1;
}

void LLComboBox::onCommit()
{
	if (mAllowTextEntry && getCurrentIndex() != -1)
	{
		// we have selected an existing item, blitz the manual text entry with
		// the properly capitalized item
		mTextEntry->setValue(getSimple());
		mTextEntry->setTentative(FALSE);
	}
	setControlValue(getValue());
	LLUICtrl::onCommit();
}

// virtual
BOOL LLComboBox::isDirty() const
{
	BOOL grubby = FALSE;
	if ( mList )
	{
		grubby = mList->isDirty();
	}
	return grubby;
}

BOOL LLComboBox::isTextDirty() const
{
	BOOL grubby = FALSE;
	if ( mTextEntry )
	{
		grubby = mTextEntry->isDirty();
	}
	return grubby;
}

// virtual   Clear dirty state
void	LLComboBox::resetDirty()
{
	if ( mList )
	{
		mList->resetDirty();
	}
}

bool LLComboBox::itemExists(const std::string& name)
{
	return mList->getItemByLabel(name);
}

void LLComboBox::resetTextDirty()
{
	if ( mTextEntry )
	{
		mTextEntry->resetDirty();
	}
}

// add item "name" to menu
LLScrollListItem* LLComboBox::add(const std::string& name, EAddPosition pos, BOOL enabled)
{
	LLScrollListItem* item = mList->addSimpleElement(name, pos);
	item->setEnabled(enabled);
	if (!mAllowTextEntry && mLabel.empty())
	{
		selectFirstItem();
	}
	return item;
}

// add item "name" with a unique id to menu
LLScrollListItem* LLComboBox::add(const std::string& name, const LLUUID& id, EAddPosition pos, BOOL enabled )
{
	LLScrollListItem* item = mList->addSimpleElement(name, pos, id);
	item->setEnabled(enabled);
	if (!mAllowTextEntry && mLabel.empty())
	{
		selectFirstItem();
	}
	return item;
}

// add item "name" with attached userdata
LLScrollListItem* LLComboBox::add(const std::string& name, void* userdata, EAddPosition pos, BOOL enabled )
{
	LLScrollListItem* item = mList->addSimpleElement(name, pos);
	item->setEnabled(enabled);
	item->setUserdata( userdata );
	if (!mAllowTextEntry && mLabel.empty())
	{
		selectFirstItem();
	}
	return item;
}

// add item "name" with attached generic data
LLScrollListItem* LLComboBox::add(const std::string& name, LLSD value, EAddPosition pos, BOOL enabled )
{
	LLScrollListItem* item = mList->addSimpleElement(name, pos, value);
	item->setEnabled(enabled);
	if (!mAllowTextEntry && mLabel.empty())
	{
		selectFirstItem();
	}
	return item;
}

LLScrollListItem* LLComboBox::addSeparator(EAddPosition pos)
{
	return mList->addSeparator(pos);
}

void LLComboBox::sortByName(BOOL ascending)
{
	mList->sortOnce(0, ascending);
}


// Choose an item with a given name in the menu.
// Returns TRUE if the item was found.
BOOL LLComboBox::setSimple(const LLStringExplicit& name)
{
	BOOL found = mList->selectItemByLabel(name, FALSE);

	if (found)
	{
		setLabel(name);
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}

	return found;
}

// virtual
void LLComboBox::setValue(const LLSD& value)
{
	BOOL found = mList->selectByValue(value);
	if (found)
	{
		LLScrollListItem* item = mList->getFirstSelected();
		if (item)
		{
			updateLabel();
		}
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}
	else
	{
		mLastSelectedIndex = -1;
	}
}

const std::string LLComboBox::getSimple() const
{
	const std::string res = getSelectedItemLabel();
	if (res.empty() && mAllowTextEntry)
	{
		return mTextEntry->getText();
	}
	else
	{
		return res;
	}
}

const std::string LLComboBox::getSelectedItemLabel(S32 column) const
{
	return mList->getSelectedItemLabel(column);
}

// virtual
LLSD LLComboBox::getValue() const
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return item->getValue();
	}
	else if (mAllowTextEntry)
	{
		return mTextEntry->getValue();
	}
	else
	{
		return LLSD();
	}
}

void LLComboBox::setLabel(const LLStringExplicit& name)
{
	if ( mTextEntry )
	{
		mTextEntry->setText(name);
		if (mList->selectItemByLabel(name, FALSE))
		{
			mTextEntry->setTentative(FALSE);
			mLastSelectedIndex = mList->getFirstSelectedIndex();
		}
		else
		{
			if (!mSuppressTentative) mTextEntry->setTentative(mTextEntryTentative);
		}
		mTextEntry->setCursor(0); // Be scrolled to the beginning!
	}
	
	if (!mAllowTextEntry)
	{
		mButton->setLabel(name);
	}
}

void LLComboBox::updateLabel()
{
	// Update the combo editor with the selected
	// item label.
	if (mTextEntry)
	{
		mTextEntry->setText(getSelectedItemLabel());
		mTextEntry->setTentative(FALSE);
	}

	// If combo box doesn't allow text entry update
	// the combo button label.
	if (!mAllowTextEntry)
	{
		mButton->setLabel(getSelectedItemLabel());
	}
}

BOOL LLComboBox::remove(const std::string& name)
{
	BOOL found = mList->selectItemByLabel(name);

	if (found)
	{
		LLScrollListItem* item = mList->getFirstSelected();
		if (item)
		{
			mList->deleteSingleItem(mList->getItemIndex(item));
		}
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}

	return found;
}

BOOL LLComboBox::remove(S32 index)
{
	if (index < mList->getItemCount())
	{
		mList->deleteSingleItem(index);
		setLabel(getSelectedItemLabel());
		return TRUE;
	}
	return FALSE;
}

// Keyboard focus lost.
void LLComboBox::onFocusLost()
{
	hideList();
	// if valid selection
	if (mAllowTextEntry && getCurrentIndex() != -1)
	{
		mTextEntry->selectAll();
	}
	LLUICtrl::onFocusLost();
}

void LLComboBox::setButtonVisible(BOOL visible)
{
	static LLUICachedControl<S32> drop_shadow_button ("DropShadowButton", 0);

	mButton->setVisible(visible);
	if (mTextEntry)
	{
		LLRect text_entry_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
		if (visible)
		{
			S32 arrow_width = mArrowImage ? mArrowImage->getWidth() : 0;
			text_entry_rect.mRight -= llmax(8,arrow_width) + 2 * drop_shadow_button;
		}
		//mTextEntry->setRect(text_entry_rect);
		mTextEntry->reshape(text_entry_rect.getWidth(), text_entry_rect.getHeight(), TRUE);
	}
}

void LLComboBox::draw()
{
	mButton->setEnabled(getEnabled() /*&& !mList->isEmpty()*/);

	// Draw children normally
	LLUICtrl::draw();
}

BOOL LLComboBox::setCurrentByIndex( S32 index )
{
	BOOL found = mList->selectNthItem( index );
	if (found)
	{
		setLabel(getSelectedItemLabel());
		mLastSelectedIndex = index;
	}
	return found;
}

S32 LLComboBox::getCurrentIndex() const
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return mList->getItemIndex( item );
	}
	return -1;
}


void LLComboBox::updateLayout()
{
	static LLUICachedControl<S32> drop_shadow_button ("DropShadowButton", 0);
	LLRect rect = getLocalRect();
	if (mAllowTextEntry)
	{
		S32 arrow_width = mArrowImage ? mArrowImage->getWidth() : 0;
		S32 shadow_size = drop_shadow_button;
		mButton->setRect(LLRect( getRect().getWidth() - llmax(8,arrow_width) - 2 * shadow_size,
								rect.mTop, rect.mRight, rect.mBottom));
		mButton->setTabStop(FALSE);
		mButton->setHAlign(LLFontGL::HCENTER);

		if (!mTextEntry)
		{
			LLRect text_entry_rect(0, getRect().getHeight(), getRect().getWidth(), 0);
			text_entry_rect.mRight -= llmax(8,arrow_width) + 2 * drop_shadow_button;
			// clear label on button
			std::string cur_label = mButton->getLabelSelected();
			mTextEntry = new LLLineEditor(std::string("combo_text_entry"),
										text_entry_rect,
										LLStringUtil::null,
										LLFontGL::getFontSansSerifSmall(),
										mMaxChars,
										boost::bind(&LLComboBox::onTextCommit, this, _2),
										boost::bind(&LLComboBox::onTextEntry, this, _1));
			mTextEntry->setSelectAllonFocusReceived(TRUE);
			mTextEntry->setHandleEditKeysDirectly(TRUE);
			mTextEntry->setCommitOnFocusLost(FALSE);
			mTextEntry->setFollows(FOLLOWS_ALL);
			mTextEntry->setText(cur_label);
			mTextEntry->setIgnoreTab(TRUE);
			addChild(mTextEntry);
		}
		else
		{
			mTextEntry->setVisible(TRUE);
			mTextEntry->setMaxTextLength(mMaxChars);
		}

		// clear label on button
		setLabel(LLStringUtil::null);

		mButton->setFollows(FOLLOWS_BOTTOM | FOLLOWS_TOP | FOLLOWS_RIGHT);
	}
	else
	{
		mButton->setRect(rect);
		mButton->setTabStop(TRUE);
		mButton->setLabelUnselected(mLabel);
		mButton->setLabelSelected(mLabel);

		if (mTextEntry)
		{
			mTextEntry->setVisible(FALSE);
		}
		mButton->setFollows(FOLLOWS_ALL);
	}
}

void* LLComboBox::getCurrentUserdata()
{
	LLScrollListItem* item = mList->getFirstSelected();
	if( item )
	{
		return item->getUserdata();
	}
	return NULL;
}


void LLComboBox::showList()
{
	// Make sure we don't go off top of screen.
	LLCoordWindow window_size;
	getWindow()->getSize(&window_size);
	//HACK: shouldn't have to know about scale here
	mList->fitContents( 192, llfloor((F32)window_size.mY / LLUI::getScaleFactor().mV[VY]) - 50 );

	// Make sure that we can see the whole list
	LLRect root_view_local;
	LLView* root_view = getRootView();
	root_view->localRectToOtherView(root_view->getLocalRect(), &root_view_local, this);
	
	LLRect rect = mList->getRect();

	S32 min_width = getRect().getWidth();
	S32 max_width = llmax(min_width, MAX_COMBO_WIDTH);
	// make sure we have up to date content width metrics
	mList->updateColumnWidths();
	S32 list_width = llclamp(mList->getMaxContentWidth(), min_width, max_width);

	if (mListPosition == BELOW)
	{
		if (rect.getHeight() <= -root_view_local.mBottom)
		{
			// Move rect so it hangs off the bottom of this view
			rect.setLeftTopAndSize(0, 0, list_width, rect.getHeight() );
		}
		else
		{	
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}
	}
	else // ABOVE
	{
		if (rect.getHeight() <= root_view_local.mTop - getRect().getHeight())
		{
			// move rect so it stacks on top of this view (clipped to size of screen)
			rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
		}
		else
		{
			// stack on top or bottom, depending on which has more room
			if (-root_view_local.mBottom > root_view_local.mTop - getRect().getHeight())
			{
				// Move rect so it hangs off the bottom of this view
				rect.setLeftTopAndSize(0, 0, list_width, llmin(-root_view_local.mBottom, rect.getHeight()));
			}
			else
			{
				// move rect so it stacks on top of this view (clipped to size of screen)
				rect.setOriginAndSize(0, getRect().getHeight(), list_width, llmin(root_view_local.mTop - getRect().getHeight(), rect.getHeight()));
			}
		}

	}
	mList->setOrigin(rect.mLeft, rect.mBottom);
	mList->reshape(rect.getWidth(), rect.getHeight());
	mList->translateIntoRect(root_view_local, FALSE);

	// Make sure we didn't go off bottom of screen
	S32 x, y;
	mList->localPointToScreen(0, 0, &x, &y);

	if (y < 0)
	{
		mList->translate(0, -y);
	}

	// NB: this call will trigger the focuslost callback which will hide the list, so do it first
	// before finally showing the list

	mList->setFocus(TRUE);

	// register ourselves as a "top" control
	// effectively putting us into a special draw layer
	// and not affecting the bounding rectangle calculation
	gFocusMgr.setTopCtrl(this);

	// Show the list and push the button down
	mButton->setToggleState(TRUE);
	mList->setVisible(TRUE);
	
	setUseBoundingRect(TRUE);
//	updateBoundingRect();
}

void LLComboBox::hideList()
{
	if (mList->getVisible())
	{
		// assert selection in list
		if(mAllowNewValues)
		{
			// mLastSelectedIndex = -1 means that we entered a new value, don't select
			// any of existing items in this case.
			if(mLastSelectedIndex >= 0)
				mList->selectNthItem(mLastSelectedIndex);
		}
		else if(mLastSelectedIndex >= 0)
			mList->selectNthItem(mLastSelectedIndex);

		mButton->setToggleState(FALSE);
		mList->setVisible(FALSE);
		mList->mouseOverHighlightNthItem(-1);

		setUseBoundingRect(FALSE);
		if( gFocusMgr.getTopCtrl() == this )
		{
			gFocusMgr.setTopCtrl(NULL);
		}
//		updateBoundingRect();
	}
}

void LLComboBox::onButtonMouseDown()
{
	if (!mList->getVisible())
	{
		// this might change selection, so do it first
		prearrangeList();

		// highlight the last selected item from the original selection before potentially selecting a new item
		// as visual cue to original value of combo box
		LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
		if (last_selected_item)
		{
			mList->mouseOverHighlightNthItem(mList->getItemIndex(last_selected_item));
		}

		if (mList->getItemCount() != 0)
		{
			showList();
		}

		setFocus( TRUE );

		// pass mouse capture on to list if button is depressed
		if (mButton->hasMouseCapture())
		{
			gFocusMgr.setMouseCapture(mList);

			// But keep the "pressed" look, which buttons normally lose when they
			// lose focus
			mButton->setForcePressedState(true);
		}
	}
	else
	{
		hideList();
	} 

}

void LLComboBox::onListMouseUp()
{
	// In some cases this is the termination of a mouse click that started on
	// the button, so clear its pressed state
	mButton->setForcePressedState(false);
}

//------------------------------------------------------------------
// static functions
//------------------------------------------------------------------

void LLComboBox::onItemSelected(const LLSD& data)
{
	mLastSelectedIndex = getCurrentIndex();
	if (mLastSelectedIndex != -1)
	{
		updateLabel();

		if (mAllowTextEntry)
		{
			gFocusMgr.setKeyboardFocus(mTextEntry);
			mTextEntry->selectAll();
		}
	}
	// hiding the list reasserts the old value stored in the text editor/dropdown button
	hideList();

	// commit does the reverse, asserting the value in the list
	onCommit();
}

BOOL LLComboBox::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen)
{
    std::string tool_tip;

	if(LLUICtrl::handleToolTip(x, y, msg, sticky_rect_screen))
	{
		return TRUE;
	}
	
	if (LLUI::sShowXUINames)
	{
		tool_tip = getShowNamesToolTip();
	}
	else
	{
		tool_tip = getToolTip();
		if (tool_tip.empty())
		{
			tool_tip = getSelectedItemLabel();
		}
	}
	
	if( !tool_tip.empty() )
	{
		msg = tool_tip;

		// Convert rect local to screen coordinates
		localPointToScreen( 
			0, 0, 
			&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
		localPointToScreen(
			getRect().getWidth(), getRect().getHeight(),
			&(sticky_rect_screen->mRight), &(sticky_rect_screen->mTop) );
	}
	return TRUE;
}

BOOL LLComboBox::handleKeyHere(KEY key, MASK mask)
{
	BOOL result = FALSE;
	if (hasFocus())
	{
		if (mList->getVisible() 
			&& key == KEY_ESCAPE && mask == MASK_NONE)
		{
			hideList();
			return TRUE;
		}
		//give list a chance to pop up and handle key
		LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
		if (last_selected_item)
		{
			// highlight the original selection before potentially selecting a new item
			mList->mouseOverHighlightNthItem(mList->getItemIndex(last_selected_item));
		}
		result = mList->handleKeyHere(key, mask);

		// will only see return key if it is originating from line editor
		// since the dropdown button eats the key
		if (key == KEY_RETURN)
		{
			// don't show list and don't eat key input when committing
			// free-form text entry with RETURN since user already knows
            // what they are trying to select
			return FALSE;
		}
		// if selection has changed, pop open list
		else if (mList->getLastSelectedItem() != last_selected_item
					|| ((key == KEY_DOWN || key == KEY_UP)
						&& mList->getCanSelect()
						&& !mList->isEmpty()))
		{
			showList();
		}
	}
	return result;
}

BOOL LLComboBox::handleUnicodeCharHere(llwchar uni_char)
{
	BOOL result = FALSE;
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		// space bar just shows the list
		if (' ' != uni_char )
		{
			LLScrollListItem* last_selected_item = mList->getLastSelectedItem();
			if (last_selected_item)
			{
				// highlight the original selection before potentially selecting a new item
				mList->mouseOverHighlightNthItem(mList->getItemIndex(last_selected_item));
			}
			result = mList->handleUnicodeCharHere(uni_char);
			if (mList->getLastSelectedItem() != last_selected_item)
			{
				showList();
			}
		}
	}
	return result;
}

BOOL LLComboBox::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mList->getVisible()) return mList->handleScrollWheel(x, y, clicks);
	if (mAllowTextEntry) // We might be editable
		if (!mList->getFirstSelected()) // We aren't in the list, don't kill their text
			return false;

	setCurrentByIndex(llclamp(getCurrentIndex() + clicks, 0, getItemCount() - 1));
	prearrangeList();
	onCommit();
	return true;
}

void LLComboBox::setAllowTextEntry(BOOL allow, S32 max_chars, BOOL set_tentative)
{
	mAllowTextEntry = allow;

	mTextEntryTentative = set_tentative;
	mMaxChars = max_chars;

	updateLayout();
}

void LLComboBox::setTextEntry(const LLStringExplicit& text)
{
	if (mTextEntry)
	{
		mTextEntry->setText(text);
		mTextEntry->setCursor(0); // Singu Note: Move the cursor over to the beginning
		mHasAutocompletedText = FALSE;
		updateSelection();
	}
}

const std::string LLComboBox::getTextEntry() const
{
	return mTextEntry->getText();
}

void LLComboBox::onTextEntry(LLLineEditor* line_editor)
{
	if (mTextEntryCallback != NULL)
	{
		(mTextEntryCallback)(line_editor, LLSD());
	}

	KEY key = gKeyboard->currentKey();
	if (key == KEY_BACKSPACE || 
		key == KEY_DELETE)
	{
		if (mList->selectItemByLabel(line_editor->getText(), FALSE))
		{
			line_editor->setTentative(FALSE);
			mLastSelectedIndex = mList->getFirstSelectedIndex();
		}
		else
		{
			if (!mSuppressTentative)
				line_editor->setTentative(mTextEntryTentative);
			mList->deselectAllItems();
			mLastSelectedIndex = -1;
		}
		return;
	}

	if (key == KEY_LEFT || 
		key == KEY_RIGHT)
	{
		return;
	}

	if (key == KEY_DOWN)
	{
		setCurrentByIndex(llmin(getItemCount() - 1, getCurrentIndex() + 1));
		if (!mList->getVisible())
		{
			prearrangeList();

			if (mList->getItemCount() != 0)
			{
				showList();
			}
		}
		line_editor->selectAll();
		line_editor->setTentative(FALSE);
	}
	else if (key == KEY_UP)
	{
		setCurrentByIndex(llmax(0, getCurrentIndex() - 1));
		if (!mList->getVisible())
		{
			prearrangeList();

			if (mList->getItemCount() != 0)
			{
				showList();
			}
		}
		line_editor->selectAll();
		line_editor->setTentative(FALSE);
	}
	else
	{
		// RN: presumably text entry
		updateSelection();
	}
}

void LLComboBox::updateSelection()
{
	if(mSuppressAutoComplete) return;

	LLWString left_wstring = mTextEntry->getWText().substr(0, mTextEntry->getCursor());
	// user-entered portion of string, based on assumption that any selected
    // text was a result of auto-completion
	LLWString user_wstring = mHasAutocompletedText ? left_wstring : mTextEntry->getWText();
	std::string full_string = mTextEntry->getText();

	// go ahead and arrange drop down list on first typed character, even
	// though we aren't showing it... some code relies on prearrange
	// callback to populate content
	if( mTextEntry->getWText().size() == 1 )
	{
		prearrangeList(mTextEntry->getText());
	}

	if (mList->selectItemByLabel(full_string, FALSE))
	{
		mTextEntry->setTentative(FALSE);
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}
	else if (mList->selectItemByPrefix(left_wstring, FALSE))
	{
		LLWString selected_item = utf8str_to_wstring(getSelectedItemLabel());
		LLWString wtext = left_wstring + selected_item.substr(left_wstring.size(), selected_item.size());
		mTextEntry->setText(wstring_to_utf8str(wtext));
		mTextEntry->setSelection(left_wstring.size(), mTextEntry->getWText().size());
		mTextEntry->endSelection();
		mTextEntry->setTentative(FALSE);
		mHasAutocompletedText = TRUE;
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}
	else // no matching items found
	{
		mList->deselectAllItems();
		mTextEntry->setText(wstring_to_utf8str(user_wstring)); // removes text added by autocompletion
		mTextEntry->setTentative(mTextEntryTentative);
		mHasAutocompletedText = FALSE;
		mLastSelectedIndex = -1;
	}
}

void LLComboBox::onTextCommit(const LLSD& data)
{
	std::string text = mTextEntry->getText();
	setSimple(text);
	onCommit();
	mTextEntry->selectAll();
}

void LLComboBox::setSuppressTentative(bool suppress)
{
	mSuppressTentative = suppress;
	if (mTextEntry && mSuppressTentative) mTextEntry->setTentative(FALSE);
}

void LLComboBox::setSuppressAutoComplete(bool suppress)
{
	mSuppressAutoComplete = suppress;
}


void LLComboBox::setFocusText(BOOL b)
{
	LLUICtrl::setFocus(b);

	if (b && mTextEntry)
	{
		if (mTextEntry->getVisible())
		{
			mTextEntry->setFocus(TRUE);
		}
	}
}

void LLComboBox::setFocus(BOOL b)
{
	LLUICtrl::setFocus(b);

	if (b)
	{
		mList->clearSearchString();
		if (mList->getVisible())
		{
			mList->setFocus(TRUE);
		}
	}
}

void LLComboBox::setPrevalidate( BOOL (*func)(const LLWString &) )
{
	if (mTextEntry) mTextEntry->setPrevalidate(func);
}

void LLComboBox::prearrangeList(std::string filter)
{
	if (mPrearrangeCallback)
	{
		mPrearrangeCallback(this, LLSD(filter));
	}
}

//============================================================================
// LLCtrlListInterface functions

S32 LLComboBox::getItemCount() const
{
	return mList->getItemCount();
}

void LLComboBox::addColumn(const LLSD& column, EAddPosition pos)
{
	mList->clearColumns();
	mList->addColumn(column, pos);
}

void LLComboBox::clearColumns()
{
	mList->clearColumns();
}

void LLComboBox::setColumnLabel(const std::string& column, const std::string& label)
{
	mList->setColumnLabel(column, label);
}

LLScrollListItem* LLComboBox::addElement(const LLSD& value, EAddPosition pos, void* userdata)
{
	return mList->addElement(value, pos, userdata);
}

LLScrollListItem* LLComboBox::addSimpleElement(const std::string& value, EAddPosition pos, const LLSD& id)
{
	return mList->addSimpleElement(value, pos, id);
}

void LLComboBox::clearRows()
{
	mList->clearRows();
}

void LLComboBox::sortByColumn(const std::string& name, BOOL ascending)
{
	mList->sortByColumn(name, ascending);
}

//============================================================================
//LLCtrlSelectionInterface functions

BOOL LLComboBox::setCurrentByID(const LLUUID& id)
{
	BOOL found = mList->selectByID( id );

	if (found)
	{
		setLabel(getSelectedItemLabel());
		mLastSelectedIndex = mList->getFirstSelectedIndex();
	}

	return found;
}

LLUUID LLComboBox::getCurrentID() const
{
	return mList->getStringUUIDSelectedItem();
}
BOOL LLComboBox::setSelectedByValue(const LLSD& value, BOOL selected)
{
	BOOL found = mList->setSelectedByValue(value, selected);
	if (found)
	{
		setLabel(getSelectedItemLabel());
	}
	return found;
}

LLSD LLComboBox::getSelectedValue()
{
	return mList->getSelectedValue();
}

BOOL LLComboBox::isSelected(const LLSD& value) const
{
	return mList->isSelected(value);
}

BOOL LLComboBox::operateOnSelection(EOperation op)
{
	if (op == OP_DELETE)
	{
		mList->deleteSelectedItems();
		return TRUE;
	}
	return FALSE;
}

BOOL LLComboBox::operateOnAll(EOperation op)
{
	if (op == OP_DELETE)
	{
		clearRows();
		return TRUE;
	}
	return FALSE;
}

BOOL LLComboBox::selectItemRange( S32 first, S32 last )
{
	return mList->selectItemRange(first, last);
}


/* Singu Note: This isn't very necessary for now, let's not bother.
static LLRegisterWidget<LLIconsComboBox> register_icons_combo_box("icons_combo_box");

LLIconsComboBox::Params::Params()
:	icon_column("icon_column", ICON_COLUMN),
	label_column("label_column", LABEL_COLUMN)
{}

LLIconsComboBox::LLIconsComboBox(const LLIconsComboBox::Params& p)
:	LLComboBox(p),
	mIconColumnIndex(p.icon_column),
	mLabelColumnIndex(p.label_column)
{}

const std::string LLIconsComboBox::getSelectedItemLabel(S32 column) const
{
	mButton->setImageOverlay(LLComboBox::getSelectedItemLabel(mIconColumnIndex), mButton->getImageOverlayHAlign());

	return LLComboBox::getSelectedItemLabel(mLabelColumnIndex);
}
*/
