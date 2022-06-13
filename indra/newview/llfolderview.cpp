/** 
 * @file llfolderview.cpp
 * @brief Implementation of the folder view collection of classes.
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

#include "llviewerprecompiledheaders.h"

#include "llfolderview.h"

#include "llcallbacklist.h"
#include "llinventorybridge.h"
#include "llinventoryclipboard.h" // *TODO: remove this once hack below gone.
#include "llinventoryfilter.h"
#include "llinventoryfunctions.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventorypanel.h"
#include "llfoldertype.h"
#include "llfloaterinventory.h"// hacked in for the bonus context menu items.
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llresmgr.h"
#include "llpreview.h"
#include "llscrollcontainer.h" // hack to allow scrolling
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llui.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewerjointattachment.h"
#include "llviewermenu.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llfloaterproperties.h"
#include "llnotificationsutil.h"

// Linden library includes
#include "lldbstrings.h"
#include "llfavoritesbar.h" // Singu TODO: Favorites bar.
#include "llfocusmgr.h"
#include "llfontgl.h"
#include "llgl.h" 
#include "llrender.h"
#include "llinventory.h"

// Third-party library includes
#include <algorithm>

///----------------------------------------------------------------------------
/// Local function declarations, constants, enums, and typedefs
///----------------------------------------------------------------------------

const S32 RENAME_WIDTH_PAD = 4;
const S32 RENAME_HEIGHT_PAD = 1;
const S32 AUTO_OPEN_STACK_DEPTH = 16;
const S32 MIN_ITEM_WIDTH_VISIBLE = LLFolderViewItem::ICON_WIDTH
			+ LLFolderViewItem::ICON_PAD 
			+ LLFolderViewItem::ARROW_SIZE 
			+ LLFolderViewItem::TEXT_PAD 
			+ /*first few characters*/ 40;
const S32 MINIMUM_RENAMER_WIDTH = 80;

// *TODO: move in params in xml if necessary. Requires modification of LLFolderView & LLInventoryPanel Params.
const S32 STATUS_TEXT_HPAD = 6;
const S32 STATUS_TEXT_VPAD = 8;

enum {
	SIGNAL_NO_KEYBOARD_FOCUS = 1,
	SIGNAL_KEYBOARD_FOCUS = 2
};

F32 LLFolderView::sAutoOpenTime = 1.f;

void delete_selected_item(void* user_data);
void copy_selected_item(void* user_data);
void open_selected_items(void* user_data);
void properties_selected_items(void* user_data);
void paste_items(void* user_data);


//---------------------------------------------------------------------------

// Tells all folders in a folderview to sort their items
// (and only their items, not folders) by a certain function.
class LLSetItemSortFunction : public LLFolderViewFunctor
{
public:
	LLSetItemSortFunction(U32 ordering)
		: mSortOrder(ordering) {}
	virtual ~LLSetItemSortFunction() {}
	virtual void doFolder(LLFolderViewFolder* folder);
	virtual void doItem(LLFolderViewItem* item);

	U32 mSortOrder;
};


// Set the sort order.
void LLSetItemSortFunction::doFolder(LLFolderViewFolder* folder)
{
	folder->setItemSortOrder(mSortOrder);
}

// Do nothing.
void LLSetItemSortFunction::doItem(LLFolderViewItem* item)
{
	return;
}

//---------------------------------------------------------------------------

// Tells all folders in a folderview to close themselves
// For efficiency, calls setOpenArrangeRecursively().
// The calling function must then call:
//	LLFolderView* root = getRoot();
//	if( root )
//	{
//		root->arrange( NULL, NULL );
//		root->scrollToShowSelection();
//	}
// to patch things up.
class LLCloseAllFoldersFunctor : public LLFolderViewFunctor
{
public:
	LLCloseAllFoldersFunctor(BOOL close) { mOpen = !close; }
	virtual ~LLCloseAllFoldersFunctor() {}
	virtual void doFolder(LLFolderViewFolder* folder);
	virtual void doItem(LLFolderViewItem* item);

	BOOL mOpen;
};


// Set the sort order.
void LLCloseAllFoldersFunctor::doFolder(LLFolderViewFolder* folder)
{
	folder->setOpenArrangeRecursively(mOpen);
}

// Do nothing.
void LLCloseAllFoldersFunctor::doItem(LLFolderViewItem* item)
{ }

///----------------------------------------------------------------------------
/// Class LLFolderView
///----------------------------------------------------------------------------

// Default constructor
LLFolderView::LLFolderView( const std::string& name,
						   const LLRect& rect, const LLUUID& source_id, LLPanel* parent_panel, LLFolderViewEventListener* listener, LLFolderViewGroupedItemModel* group_model ) :
#if LL_WINDOWS
#pragma warning( push )
#pragma warning( disable : 4355 ) // warning C4355: 'this' : used in base member initializer list
#endif
	LLFolderViewFolder( name, NULL, NULL, NULL, this, listener ),
#if LL_WINDOWS
#pragma warning( pop )
#endif
	mRunningHeight(0),
	mScrollContainer( NULL ),
	mPopupMenuHandle(),
	mAllowMultiSelect(TRUE),
	mShowEmptyMessage(TRUE),
	mShowFolderHierarchy(FALSE),
	mSourceID(source_id),
	mRenameItem( NULL ),
	mNeedsScroll( FALSE ),
	mUseLabelSuffix( TRUE ),
	mPinningSelectedItem(FALSE),
	mNeedsAutoSelect( FALSE ),
	mAutoSelectOverride(FALSE),
	mNeedsAutoRename(FALSE),
	mDebugFilters(FALSE),
	mSortOrder(LLInventoryFilter::SO_FOLDERS_BY_NAME),	// This gets overridden by a pref immediately
	mFilter(LLInventoryFilter::Params().name(name)),
	mShowSelectionContext(FALSE),
	mShowSingleSelection(FALSE),
	mArrangeGeneration(0),
	mSignalSelectCallback(0),
	mMinWidth(0),
	mDragAndDropThisFrame(FALSE),
	mUseEllipses(FALSE),
	mDraggingOverItem(NULL),
	mStatusTextBox(NULL),
	mSearchType(1),
	mGroupedItemModel(group_model)
{
	LLPanel* panel = parent_panel;
	mParentPanel = panel->getHandle();
	mRoot = this;

	mShowLoadStatus = TRUE;
	LLRect new_rect(rect.mLeft, rect.mBottom + getRect().getHeight(), rect.mLeft + getRect().getWidth(), rect.mBottom);
	setRect( rect );
	reshape(rect.getWidth(), rect.getHeight());
	mIsOpen = TRUE; // this view is always open.
	mAutoOpenItems.setDepth(AUTO_OPEN_STACK_DEPTH);
	mAutoOpenCandidate = NULL;
	mAutoOpenTimer.stop();
	mKeyboardSelection = FALSE;
	mIndentation = -LEFT_INDENTATION; // children start at indentation 0
	gIdleCallbacks.addFunction(idle, this);

	//clear label
	// go ahead and render root folder as usual
	// just make sure the label ("Inventory Folder") never shows up
	mLabel = LLStringUtil::null;

	mRenamer = new LLLineEditor(std::string("ren"), getRect(), LLStringUtil::null, getLabelFontForStyle(LLFontGL::NORMAL),
								DB_INV_ITEM_NAME_STR_LEN,
								boost::bind(&LLFolderView::commitRename,this),
								NULL,
								NULL,
								&LLLineEditor::prevalidatePrintableNotPipe);
	//mRenamer->setWriteableBgColor(LLColor4::white);
	// Escape is handled by reverting the rename, not commiting it (default behavior)
	mRenamer->setCommitOnFocusLost(TRUE);
	mRenamer->setVisible(FALSE);
	addChild(mRenamer);
	
	LLFontGL* font = getLabelFontForStyle(mLabelStyle);
	LLRect new_r = LLRect(rect.mLeft + ICON_PAD,
			      rect.mTop - TEXT_PAD,
			      rect.mRight,
			      rect.mTop - TEXT_PAD - llfloor(font->getLineHeight()));
	mStatusTextBox = new LLTextBox(name, new_r, std::string(), font, false);
	mStatusTextBox->setVisible(true);
	mStatusTextBox->setHPad(STATUS_TEXT_HPAD);
	mStatusTextBox->setVPad(STATUS_TEXT_VPAD);
	mStatusTextBox->setFollows(FOLLOWS_LEFT|FOLLOWS_TOP);
	// make the popup menu available
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->buildMenu("menu_inventory.xml", parent_panel);
	if (!menu)
	{
		menu = new LLMenuGL(LLStringUtil::null);
	}
	menu->setBackgroundColor(gColors.getColor("MenuPopupBgColor"));
	menu->setVisible(FALSE);
	mPopupMenuHandle = menu->getHandle();

	setTabStop(TRUE);
	
	mListener->openItem();
}

// Destroys the object
LLFolderView::~LLFolderView( void )
{
	closeRenamer();

	// The release focus call can potentially call the
	// scrollcontainer, which can potentially be called with a partly
	// destroyed scollcontainer. Just null it out here, and no worries
	// about calling into the invalid scroll container.
	// Same with the renamer.
	mScrollContainer = NULL;
	mRenameItem = NULL;
	mRenamer = NULL;
	mStatusTextBox = NULL;

	mAutoOpenItems.removeAllNodes();
	gIdleCallbacks.deleteFunction(idle, this);

	if (mPopupMenuHandle.get()) mPopupMenuHandle.get()->die();

	mAutoOpenItems.removeAllNodes();
	clearSelection();
	mItems.clear();
	mFolders.clear();

	mItemMap.clear();
}

BOOL LLFolderView::canFocusChildren() const
{
	return FALSE;
}

static LLTrace::BlockTimerStatHandle FTM_SORT("Sort Inventory");

void LLFolderView::setSortOrder(U32 order)
{
	if (order != mSortOrder)
	{
		LL_RECORD_BLOCK_TIME(FTM_SORT);
		
		mSortOrder = order;

		sortBy(order);
		arrangeAll();
	}
}


U32 LLFolderView::getSortOrder() const
{
	return mSortOrder;
}

U32 LLFolderView::toggleSearchType(std::string toggle)
{
	if (toggle == "name")
	{
		if (mSearchType & 1)
		{
			mSearchType &= 6;
		}
		else
		{
			mSearchType |= 1;
		}
	}
	else if (toggle == "description")
	{
		if (mSearchType & 2)
		{
			mSearchType &= 5;
		}
		else
		{
			mSearchType |= 2;
		}
	}
	else if (toggle == "creator")
	{
		if (mSearchType & 4)
		{
			mSearchType &= 3;
		}
		else
		{
			mSearchType |= 4;
		}
	}
	if (mSearchType == 0)
	{
		mSearchType = 1;
	}

	if (getFilterSubString().length())
	{
		mFilter.setModified(LLInventoryFilter::FILTER_RESTART);
	}

	return mSearchType;
}

U32 LLFolderView::getSearchType() const
{
	return mSearchType;
}

BOOL LLFolderView::addFolder( LLFolderViewFolder* folder)
{
	// enforce sort order of My Inventory followed by Library
	if (folder->getListener()->getUUID() == gInventory.getLibraryRootFolderID())
	{
		mFolders.push_back(folder);
	}
	else
	{
		mFolders.insert(mFolders.begin(), folder);
	}
	folder->setShowLoadStatus(mShowLoadStatus);
	folder->setOrigin(0, 0);
	folder->reshape(getRect().getWidth(), 0);
	folder->setVisible(FALSE);
	addChild( folder );
	folder->dirtyFilter();
	folder->requestArrange();
	return TRUE;
}

void LLFolderView::closeAllFolders()
{
	// Close all the folders
	setOpenArrangeRecursively(FALSE, LLFolderViewFolder::RECURSE_DOWN);
	arrangeAll();
}

void LLFolderView::openTopLevelFolders()
{
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end();)
	{
		folders_t::iterator fit = iter++;
		(*fit)->setOpen(TRUE);
	}
}

void LLFolderView::setOpenArrangeRecursively(BOOL openitem, ERecurseType recurse)
{
	// call base class to do proper recursion
	LLFolderViewFolder::setOpenArrangeRecursively(openitem, recurse);
	// make sure root folder is always open
	mIsOpen = TRUE;
}

static LLTrace::BlockTimerStatHandle FTM_ARRANGE("Arrange");

// This view grows and shrinks to enclose all of its children items and folders.
S32 LLFolderView::arrange( S32* unused_width, S32* unused_height, S32 filter_generation )
{
	if(!mScrollContainer)
		return 1;
	if (getListener()->getUUID().notNull())
	{
		if (mNeedsSort)
		{
			mFolders.sort(mSortFunction);
			mItems.sort(mSortFunction);
			mNeedsSort = false;
		}
	}

	LL_RECORD_BLOCK_TIME(FTM_ARRANGE);

	filter_generation = mFilter.getFirstSuccessGeneration();
	mMinWidth = 0;

	mHasVisibleChildren = hasFilteredDescendants(filter_generation);
	// arrange always finishes, so optimistically set the arrange generation to the most current
	mLastArrangeGeneration = getRoot()->getArrangeGeneration();

	LLInventoryFilter::EFolderShow show_folder_state =
		getRoot()->getFilter().getShowFolderState();

	S32 total_width = LEFT_PAD;
	S32 running_height = mDebugFilters ? llceil(LLFontGL::getFontMonospace()->getLineHeight()) : 0;
	S32 target_height = running_height;
	S32 parent_item_height = getRect().getHeight();

	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end();)
	{
		folders_t::iterator fit = iter++;
		LLFolderViewFolder* folderp = (*fit);
		if (getDebugFilters())
		{
			folderp->setVisible(TRUE);
		}
		else
		{
			folderp->setVisible(show_folder_state == LLInventoryFilter::SHOW_ALL_FOLDERS || // always show folders?
									(folderp->getFiltered(filter_generation) || folderp->hasFilteredDescendants(filter_generation))); // passed filter or has descendants that passed filter
		}

		if (folderp->getVisible())
		{
			S32 child_height = 0;
			S32 child_width = 0;
			S32 child_top = parent_item_height - running_height;
			
			target_height += folderp->arrange( &child_width, &child_height, filter_generation );

			mMinWidth = llmax(mMinWidth, child_width);
			total_width = llmax( total_width, child_width );
			running_height += child_height;
			folderp->setOrigin( ICON_PAD, child_top - (*fit)->getRect().getHeight() );
		}
	}

	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end();)
	{
		items_t::iterator iit = iter++;
		LLFolderViewItem* itemp = (*iit);
		itemp->setVisible(itemp->getFiltered(filter_generation));

		if (itemp->getVisible())
		{
			S32 child_width = 0;
			S32 child_height = 0;
			S32 child_top = parent_item_height - running_height;
			
			target_height += itemp->arrange( &child_width, &child_height, filter_generation );
			itemp->reshape(itemp->getRect().getWidth(), child_height);

			mMinWidth = llmax(mMinWidth, child_width);
			total_width = llmax( total_width, child_width );
			running_height += child_height;
			itemp->setOrigin( ICON_PAD, child_top - itemp->getRect().getHeight() );
		}
	}

	if(!mHasVisibleChildren)// is there any filtered items ?
	{
		//Nope. We need to display status textbox, let's reserve some place for it
		running_height = mStatusTextBox->getTextPixelHeight();
		target_height = running_height;
	}

	mRunningHeight = running_height;
	LLRect scroll_rect = mScrollContainer->getContentWindowRect();
	reshape( llmax(scroll_rect.getWidth(), total_width), running_height );

	LLRect new_scroll_rect = mScrollContainer->getContentWindowRect();
	if (new_scroll_rect.getWidth() != scroll_rect.getWidth())
	{
		reshape( llmax(scroll_rect.getWidth(), total_width), running_height );
	}

	// move item renamer text field to item's new position
	updateRenamerPosition();

	mTargetHeight = (F32)target_height;
	return ll_round(mTargetHeight);
}

const std::string LLFolderView::getFilterSubString(BOOL trim)
{
	return mFilter.getFilterSubString(trim);
}

static LLTrace::BlockTimerStatHandle FTM_FILTER("Filter Inventory");

void LLFolderView::filter( LLInventoryFilter& filter )
{
	LL_RECORD_BLOCK_TIME(FTM_FILTER);
	filter.setFilterCount(llclamp(gSavedSettings.getS32("FilterItemsPerFrame"), 1, 5000));

	if (getCompletedFilterGeneration() < filter.getCurrentGeneration())
	{
		mPassedFilter = FALSE;
		mMinWidth = 0;
		LLFolderViewFolder::filter(filter);
	}
	else
	{
		mPassedFilter = TRUE;
	}
}

void LLFolderView::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLRect scroll_rect;
	if (mScrollContainer)
	{
		LLView::reshape(width, height, called_from_parent);
		scroll_rect = mScrollContainer->getContentWindowRect();
	}
	width = llmax(mMinWidth, scroll_rect.getWidth());
	height = llmax(mRunningHeight, scroll_rect.getHeight());

	// restrict width with scroll container's width
	if (mUseEllipses)
		width = scroll_rect.getWidth();

	LLView::reshape(width, height, called_from_parent);

	mReshapeSignal(mSelectedItems, FALSE);
}

void LLFolderView::addToSelectionList(LLFolderViewItem* item)
{
	if (item->isSelected())
	{
		removeFromSelectionList(item);
	}
	if (mSelectedItems.size())
	{
		mSelectedItems.back()->setIsCurSelection(FALSE);
	}
	item->setIsCurSelection(TRUE);
	mSelectedItems.push_back(item);
}

void LLFolderView::removeFromSelectionList(LLFolderViewItem* item)
{
	if (mSelectedItems.size())
	{
		mSelectedItems.back()->setIsCurSelection(FALSE);
	}

	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin(); item_iter != mSelectedItems.end();)
	{
		if (*item_iter == item)
		{
			item_iter = mSelectedItems.erase(item_iter);
		}
		else
		{
			++item_iter;
		}
	}
	if (mSelectedItems.size())
	{
		mSelectedItems.back()->setIsCurSelection(TRUE);
	}
}

LLFolderViewItem* LLFolderView::getCurSelectedItem( void )
{
	if(mSelectedItems.size())
	{
		LLFolderViewItem* itemp = mSelectedItems.back();
		llassert(itemp->getIsCurSelection());
		return itemp;
	}
	return NULL;
}

LLFolderView::selected_items_t& LLFolderView::getSelectedItems( void )
{
	return mSelectedItems;
}

// Record the selected item and pass it down the hierachy.
BOOL LLFolderView::setSelection(LLFolderViewItem* selection, BOOL openitem,
								BOOL take_keyboard_focus)
{
	mSignalSelectCallback = take_keyboard_focus ? SIGNAL_KEYBOARD_FOCUS : SIGNAL_NO_KEYBOARD_FOCUS;

	if( selection == this )
	{
		return FALSE;
	}

	if( selection && take_keyboard_focus)
	{
		mParentPanel.get()->setFocus(TRUE);
	}

	// clear selection down here because change of keyboard focus can potentially
	// affect selection
	clearSelection();

	if(selection)
	{
		addToSelectionList(selection);
	}

	BOOL rv = LLFolderViewFolder::setSelection(selection, openitem, take_keyboard_focus);
	if(openitem && selection)
	{
		selection->getParentFolder()->requestArrange();
	}

	llassert(mSelectedItems.size() <= 1);

	return rv;
}

void LLFolderView::setSelectionByID(const LLUUID& obj_id, BOOL take_keyboard_focus)
{
	LLFolderViewItem* itemp = getItemByID(obj_id);
	if(itemp && itemp->getListener())
	{
		itemp->arrangeAndSet(TRUE, take_keyboard_focus);
		mSelectThisID.setNull();
		return;
	}
	else
	{
		// save the desired item to be selected later (if/when ready)
		mSelectThisID = obj_id;
	}
}

void LLFolderView::updateSelection()
{
	if (mSelectThisID.notNull())
	{
		setSelectionByID(mSelectThisID, false);
	}
}

BOOL LLFolderView::changeSelection(LLFolderViewItem* selection, BOOL selected)
{
	BOOL rv = FALSE;

	// can't select root folder
	if(!selection || selection == this)
	{
		return FALSE;
	}

	if (!mAllowMultiSelect)
	{
		clearSelection();
	}

	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin(); item_iter != mSelectedItems.end(); ++item_iter)
	{
		if (*item_iter == selection)
		{
			break;
		}
	}

	BOOL on_list = (item_iter != mSelectedItems.end());

	if(selected && !on_list)
	{
		addToSelectionList(selection);
	}
	if(!selected && on_list)
	{
		removeFromSelectionList(selection);
	}

	rv = LLFolderViewFolder::changeSelection(selection, selected);

	mSignalSelectCallback = SIGNAL_KEYBOARD_FOCUS;
	
	return rv;
}

static LLTrace::BlockTimerStatHandle FTM_SANITIZE_SELECTION("Sanitize Selection");
void LLFolderView::sanitizeSelection()
{
	LL_RECORD_BLOCK_TIME(FTM_SANITIZE_SELECTION);
	// store off current item in case it is automatically deselected
	// and we want to preserve context
	LLFolderViewItem* original_selected_item = getCurSelectedItem();

	// Cache "Show all folders" filter setting
	BOOL show_all_folders = (getRoot()->getFilter().getShowFolderState() == LLInventoryFilter::SHOW_ALL_FOLDERS);

	std::vector<LLFolderViewItem*> items_to_remove;
	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin(); item_iter != mSelectedItems.end(); ++item_iter)
	{
		LLFolderViewItem* item = *item_iter;

		// ensure that each ancestor is open and potentially passes filtering
		BOOL visible = item->potentiallyVisible(); // initialize from filter state for this item
		// modify with parent open and filters states
		LLFolderViewFolder* parent_folder = item->getParentFolder();
		if ( parent_folder )
		{
			if ( show_all_folders )
			{	// "Show all folders" is on, so this folder is visible
				visible = TRUE;
			}
			else
			{	// Move up through parent folders and see what's visible
				while(parent_folder)
				{
					visible = visible && parent_folder->isOpen() && parent_folder->potentiallyVisible();
					parent_folder = parent_folder->getParentFolder();
				}
			}
		}

		//  deselect item if any ancestor is closed or didn't pass filter requirements.
		if (!visible)
		{
			items_to_remove.push_back(item);
		}

		// disallow nested selections (i.e. folder items plus one or more ancestors)
		// could check cached mum selections count and only iterate if there are any
		// but that may be a premature optimization.
		selected_items_t::iterator other_item_iter;
		for (other_item_iter = mSelectedItems.begin(); other_item_iter != mSelectedItems.end(); ++other_item_iter)
		{
			LLFolderViewItem* other_item = *other_item_iter;
			for( parent_folder = other_item->getParentFolder(); parent_folder; parent_folder = parent_folder->getParentFolder())
			{
				if (parent_folder == item)
				{
					// this is a descendent of the current folder, remove from list
					items_to_remove.push_back(other_item);
					break;
				}
			}
		}

		// Don't allow invisible items (such as root folders) to be selected.
		if (item == getRoot())
		{
			items_to_remove.push_back(item);
		}
	}

	std::vector<LLFolderViewItem*>::iterator item_it;
	for (item_it = items_to_remove.begin(); item_it != items_to_remove.end(); ++item_it )
	{
		changeSelection(*item_it, FALSE); // toggle selection (also removes from list)
	}

	// if nothing selected after prior constraints...
	if (mSelectedItems.empty())
	{
		// ...select first available parent of original selection, or "My Inventory" otherwise
		LLFolderViewItem* new_selection = NULL;
		if (original_selected_item)
		{
			for(LLFolderViewFolder* parent_folder = original_selected_item->getParentFolder();
				parent_folder;
				parent_folder = parent_folder->getParentFolder())
			{
				if (parent_folder->potentiallyVisible())
				{
					// give initial selection to first ancestor folder that potentially passes the filter
					if (!new_selection)
					{
						new_selection = parent_folder;
					}

					// if any ancestor folder of original item is closed, move the selection up 
					// to the highest closed
					if (!parent_folder->isOpen())
					{	
						new_selection = parent_folder;
					}
				}
			}
		}
		else
		{
			new_selection = NULL;
		}

		if (new_selection)
		{
			setSelection(new_selection, FALSE, FALSE);
		}
	}
}

void LLFolderView::clearSelection()
{
	for (selected_items_t::const_iterator item_it = mSelectedItems.begin(); 
		 item_it != mSelectedItems.end(); 
		 ++item_it)
	{
		(*item_it)->setUnselected();
	}

	mSelectedItems.clear();
	mSelectThisID.setNull();
}

uuid_set_t LLFolderView::getSelectionList() const
{
	uuid_set_t selection;
	for (const auto& item : mSelectedItems)
	{
		selection.insert(item->getListener()->getUUID());
	}
	return selection;
}

bool LLFolderView::startDrag(LLToolDragAndDrop::ESource source)
{
	std::vector<EDragAndDropType> types;
	uuid_vec_t cargo_ids;
	selected_items_t::iterator item_it;
	bool can_drag = true;

	if (!mSelectedItems.empty())
	{
		for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
		{
			EDragAndDropType type = DAD_NONE;
			LLUUID id = LLUUID::null;
			can_drag = can_drag && (*item_it)->getListener()->startDrag(&type, &id);

			types.push_back(type);
			cargo_ids.push_back(id);
		}

		LLToolDragAndDrop::getInstance()->beginMultiDrag(types, cargo_ids, source, mSourceID); 
	}
	return can_drag;
}

void LLFolderView::commitRename( )
{
	finishRenamingItem();
}

void LLFolderView::draw()
{
	if (mDebugFilters)
	{
		std::string current_filter_string = llformat("Current Filter: %d, Least Filter: %d, Auto-accept Filter: %d",
										mFilter.getCurrentGeneration(), mFilter.getFirstSuccessGeneration(), mFilter.getFirstRequiredGeneration());
		LLFontGL::getFontMonospace()->renderUTF8(current_filter_string, 0, 2, 
			getRect().getHeight() - LLFontGL::getFontMonospace()->getLineHeight(), LLColor4(0.5f, 0.5f, 0.8f, 1.f), 
			LLFontGL::LEFT, LLFontGL::BOTTOM, LLFontGL::NORMAL, LLFontGL::NO_SHADOW,  S32_MAX, S32_MAX, NULL, FALSE );
	}

	// if cursor has moved off of me during drag and drop
	// close all auto opened folders
	if (!mDragAndDropThisFrame)
	{
		closeAutoOpenedFolders();
	}

	// while dragging, update selection rendering to reflect single/multi drag status
	LLToolDragAndDrop& dad_inst(LLToolDragAndDrop::instance());
	if (dad_inst.hasMouseCapture())
	{
		EAcceptance last_accept = dad_inst.getLastAccept();
		setShowSingleSelection(last_accept == ACCEPT_YES_SINGLE || last_accept == ACCEPT_YES_COPY_SINGLE);
	}
	else
	{
		setShowSingleSelection(FALSE);
	}

	static LLUICachedControl<F32> type_ahead_timeout("TypeAheadTimeout", 0);
	if (mSearchTimer.getElapsedTimeF32() > type_ahead_timeout || !mSearchString.size())
	{
		mSearchString.clear();
	}

	if (hasVisibleChildren()
		|| mFilter.getShowFolderState() == LLInventoryFilter::SHOW_ALL_FOLDERS)
	{
		mStatusText.clear();
		mStatusTextBox->setVisible( FALSE );
	}
	else if (mShowEmptyMessage)
	{
		static LLCachedControl<LLColor4> sSearchStatusColor(gColors, "InventorySearchStatusColor", LLColor4::white );
		if (LLInventoryModelBackgroundFetch::instance().folderFetchActive() || mCompletedFilterGeneration < mFilter.getFirstSuccessGeneration())
		{
			mStatusText = LLTrans::getString("Searching");
		}
		else
		{
			mStatusText = getFilter().getEmptyLookupMessage();
		}
		mStatusTextBox->setWrappedText(mStatusText);
		mStatusTextBox->setVisible( TRUE );

		// firstly reshape message textbox with current size. This is necessary to
		// LLTextBox::getTextPixelHeight works properly
		const LLRect local_rect = getLocalRect();
		mStatusTextBox->setShape(local_rect);

		// get preferable text height...
		S32 pixel_height = mStatusTextBox->getTextPixelHeight();
		bool height_changed = (local_rect.getHeight() != pixel_height);
		if (height_changed)
		{
			// ... if it does not match current height, lets rearrange current view.
			// This will indirectly call ::arrange and reshape of the status textbox.
			// We should call this method to also notify parent about required rect.
			// See EXT-7564, EXT-7047.
			arrangeFromRoot();
		}
	}

	// skip over LLFolderViewFolder::draw since we don't want the folder icon, label, 
	// and arrow for the root folder
	LLView::draw();

	mDragAndDropThisFrame = FALSE;
}

void LLFolderView::finishRenamingItem( void )
{
	if(!mRenamer)
	{
		return;
	}
	if( mRenameItem )
	{
		mRenameItem->rename( mRenamer->getText() );
	}

	closeRenamer();

	// List is re-sorted alphabetically, so scroll to make sure the selected item is visible.
	scrollToShowSelection();
}

void LLFolderView::closeRenamer( void )
{
	if (mRenamer && mRenamer->getVisible())
	{
		// Triggers onRenamerLost() that actually closes the renamer.
		gFocusMgr.setTopCtrl( NULL );
	}
}

bool isDescendantOfASelectedItem(LLFolderViewItem* item, const std::vector<LLFolderViewItem*>& selectedItems)
{
	LLFolderViewItem* item_parent = dynamic_cast<LLFolderViewItem*>(item->getParent());

	if (item_parent)
	{
		for(std::vector<LLFolderViewItem*>::const_iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
		{
			const LLFolderViewItem* const selected_item = (*it);

			LLFolderViewItem* parent = item_parent;

			while (parent)
			{
				if (selected_item == parent)
				{
					return true;
				}

				parent = dynamic_cast<LLFolderViewItem*>(parent->getParent());
			}
		}
	}

	return false;
}

// static
void LLFolderView::removeCutItems()
{
	// There's no item in "cut" mode on the clipboard -> exit
	if (!LLInventoryClipboard::instance().isCutMode())
		return;

	// Get the list of clipboard item uuids and iterate through them
	uuid_vec_t objects;
	LLInventoryClipboard::instance().retrieve(objects);
	for (auto iter = objects.begin();
		 iter != objects.end();
		 ++iter)
	{
		gInventory.removeObject(*iter);
	}
}

void LLFolderView::removeSelectedItems()
{
	if(getVisible() && getEnabled())
	{
		// just in case we're removing the renaming item.
		mRenameItem = NULL;

		// create a temporary structure which we will use to remove
		// items, since the removal will futz with internal data
		// structures.
		std::vector<LLFolderViewItem*> items;
		if(mSelectedItems.empty()) return;
		LLFolderViewItem* item = NULL;
		selected_items_t::iterator item_it;
		for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
		{
			item = *item_it;
			if (item && item->isRemovable())
			{
				items.push_back(item);
			}
			else
			{
				LL_INFOS() << "Cannot delete " << item->getName() << LL_ENDL;
				return;
			}
		}

		// iterate through the new container.
		size_t count = items.size();
		LLUUID new_selection_id;
		if(count == 1)
		{
			LLFolderViewItem* item_to_delete = items[0];
			LLFolderViewFolder* parent = item_to_delete->getParentFolder();
			LLFolderViewItem* new_selection = item_to_delete->getNextOpenNode(FALSE);
			if (!new_selection)
			{
				new_selection = item_to_delete->getPreviousOpenNode(FALSE);
			}
			if(parent)
			{
				if (parent->removeItem(item_to_delete))
				{
					// change selection on successful delete
					if (new_selection)
					{
						setSelectionFromRoot(new_selection, new_selection->isOpen(), mParentPanel.get()->hasFocus());
					}
					else
					{
						setSelectionFromRoot(NULL, mParentPanel.get()->hasFocus());
					}
				}
			}
			arrangeAll();
		}
		else if (count > 1)
		{
			std::vector<LLFolderViewEventListener*> listeners;
			LLFolderViewEventListener* listener;
			LLFolderViewItem* last_item = items[count - 1];
			LLFolderViewItem* new_selection = last_item->getNextOpenNode(FALSE);
			while(new_selection && new_selection->isSelected())
			{
				new_selection = new_selection->getNextOpenNode(FALSE);
			}
			if (!new_selection)
			{
				new_selection = last_item->getPreviousOpenNode(FALSE);
				while (new_selection && (new_selection->isSelected() || isDescendantOfASelectedItem(new_selection, items)))
				{
					new_selection = new_selection->getPreviousOpenNode(FALSE);
				}
			}
			if (new_selection)
			{
				setSelectionFromRoot(new_selection, new_selection->isOpen(), mParentPanel.get()->hasFocus());
			}
			else
			{
				setSelectionFromRoot(NULL, mParentPanel.get()->hasFocus());
			}

			for(size_t i = 0; i < count; ++i)
			{
				listener = items[i]->getListener();
				if(listener && (std::find(listeners.begin(), listeners.end(), listener) == listeners.end()))
				{
					listeners.push_back(listener);
				}
			}
			listener = listeners.at(0);
			if(listener)
			{
				listener->removeBatch(listeners);
			}
		}
		arrangeAll();
		scrollToShowSelection();
	}
}

// open the selected item.
void LLFolderView::openSelectedItems( void )
{
	if(getVisible() && getEnabled())
	{
		if (mSelectedItems.size() == 1)
		{
			mSelectedItems.front()->openItem();
		}
		else
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLMultiPreview* multi_previewp = new LLMultiPreview(LLRect(left, top, left + 300, top - 100));
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLMultiProperties* multi_propertiesp = new LLMultiProperties(LLRect(left, top, left + 300, top - 100));

			selected_items_t::iterator item_it;
			for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
			{
				// IT_{OBJECT,ATTACHMENT} creates LLProperties
				// floaters; others create LLPreviews.  Put
				// each one in the right type of container.
				LLFolderViewEventListener* listener = (*item_it)->getListener();
				bool is_prop = listener && (listener->getInventoryType() == LLInventoryType::IT_OBJECT || listener->getInventoryType() == LLInventoryType::IT_ATTACHMENT);
				if (is_prop)
					LLFloater::setFloaterHost(multi_propertiesp);
				else
					LLFloater::setFloaterHost(multi_previewp);
				(*item_it)->openItem();
			}

			LLFloater::setFloaterHost(NULL);
			// *NOTE: LLMulti* will safely auto-delete when open'd
			// without any children.
			multi_previewp->open();
			multi_propertiesp->open();
		}
	}
}

void LLFolderView::propertiesSelectedItems( void )
{
	if(getVisible() && getEnabled())
	{
		if (mSelectedItems.size() == 1)
		{
			LLFolderViewItem* folder_item = mSelectedItems.front();
			if(!folder_item) return;
			folder_item->getListener()->showProperties();
		}
		else
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);

			LLMultiProperties* multi_propertiesp = new LLMultiProperties(LLRect(left, top, left + 100, top - 100));

			LLFloater::setFloaterHost(multi_propertiesp);

			selected_items_t::iterator item_it;
			for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
			{
				(*item_it)->getListener()->showProperties();
			}

			LLFloater::setFloaterHost(NULL);
			multi_propertiesp->open();		/* Flawfinder: ignore */
		}
	}
}

void LLFolderView::changeType(LLInventoryModel *model, LLFolderType::EType new_folder_type)
{
	LLFolderBridge *folder_bridge = LLFolderBridge::sSelf.get();

	if (!folder_bridge) return;
	LLViewerInventoryCategory *cat = folder_bridge->getCategory();
	if (!cat) return;
	cat->changeType(new_folder_type);
}
void LLFolderView::autoOpenItem( LLFolderViewFolder* item )
{
	if ((mAutoOpenItems.check() == item) || 
		(mAutoOpenItems.getDepth() >= (U32)AUTO_OPEN_STACK_DEPTH) ||
		item->isOpen())
	{
		return;
	}

	// close auto-opened folders
	LLFolderViewFolder* close_item = mAutoOpenItems.check();
	while (close_item && close_item != item->getParentFolder())
	{
		mAutoOpenItems.pop();
		close_item->setOpenArrangeRecursively(FALSE);
		close_item = mAutoOpenItems.check();
	}

	item->requestArrange();

	mAutoOpenItems.push(item);
	
	item->setOpen(TRUE);
	LLRect content_rect = mScrollContainer->getContentWindowRect();
	LLRect constraint_rect(0,content_rect.getHeight(), content_rect.getWidth(), 0);
	scrollToShowItem(item, constraint_rect);
}

void LLFolderView::closeAutoOpenedFolders()
{
	while (mAutoOpenItems.check())
	{
		LLFolderViewFolder* close_item = mAutoOpenItems.pop();
		close_item->setOpen(FALSE);
	}

	if (mAutoOpenCandidate)
	{
		mAutoOpenCandidate->setAutoOpenCountdown(0.f);
	}
	mAutoOpenCandidate = NULL;
	mAutoOpenTimer.stop();
}

BOOL LLFolderView::autoOpenTest(LLFolderViewFolder* folder)
{
	if (folder && mAutoOpenCandidate == folder)
	{
		if (mAutoOpenTimer.getStarted())
		{
			if (!mAutoOpenCandidate->isOpen())
			{
				mAutoOpenCandidate->setAutoOpenCountdown(clamp_rescale(mAutoOpenTimer.getElapsedTimeF32(), 0.f, sAutoOpenTime, 0.f, 1.f));
			}
			if (mAutoOpenTimer.getElapsedTimeF32() > sAutoOpenTime)
			{
				autoOpenItem(folder);
				mAutoOpenTimer.stop();
				return TRUE;
			}
		}
		return FALSE;
	}

	// otherwise new candidate, restart timer
	if (mAutoOpenCandidate)
	{
		mAutoOpenCandidate->setAutoOpenCountdown(0.f);
	}
	mAutoOpenCandidate = folder;
	mAutoOpenTimer.start();
	return FALSE;
}

BOOL LLFolderView::canCopy() const
{
	if (!(getVisible() && getEnabled() && (mSelectedItems.size() > 0)))
	{
		return FALSE;
	}
	
	for (selected_items_t::const_iterator selected_it = mSelectedItems.begin(); selected_it != mSelectedItems.end(); ++selected_it)
	{
		const LLFolderViewItem* item = *selected_it;
		if (!item->getListener()->isItemCopyable())
		{
			return FALSE;
		}
	}
	return TRUE;
}

// copy selected item
void LLFolderView::copy() const
{
	// *NOTE: total hack to clear the inventory clipboard
	LLInventoryClipboard::instance().reset();
	S32 count = mSelectedItems.size();
	if(getVisible() && getEnabled() && (count > 0))
	{
		for (auto item : mSelectedItems)
		{
			if(auto listener = item->getListener())
			{
				listener->copyToClipboard();
			}
		}
	}
	//mSearchString.clear(); // Singu Note: There's no good reason to clear out the jumpto item search string now, it'll time out anyway, let's remain const
}

BOOL LLFolderView::canCut() const
{
	if (!(getVisible() && getEnabled() && (mSelectedItems.size() > 0)))
	{
		return FALSE;
	}
	
	for (selected_items_t::const_iterator selected_it = mSelectedItems.begin(); selected_it != mSelectedItems.end(); ++selected_it)
	{
		const LLFolderViewItem* item = *selected_it;
		const LLFolderViewEventListener* listener = item->getListener();

		if (!listener || !listener->isItemRemovable())
		{
			return FALSE;
		}
	}
	return TRUE;
}

void LLFolderView::cut()
{
	// clear the inventory clipboard
	LLInventoryClipboard::instance().reset();
	S32 count = mSelectedItems.size();
	if(getVisible() && getEnabled() && (count > 0))
	{
		LLFolderViewEventListener* listener = NULL;
		selected_items_t::iterator item_it;
		for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
		{
			listener = (*item_it)->getListener();
			if(listener)
			{
				listener->cutToClipboard();
			}
		}
		LLFolderView::removeCutItems();
	}
	mSearchString.clear();
}

BOOL LLFolderView::canPaste() const
{
	if (mSelectedItems.empty())
	{
		return FALSE;
	}

	if(getVisible() && getEnabled())
	{
		for (selected_items_t::const_iterator item_it = mSelectedItems.begin();
			 item_it != mSelectedItems.end(); ++item_it)
		{
			// *TODO: only check folders and parent folders of items
			const LLFolderViewItem* item = (*item_it);
			const LLFolderViewEventListener* listener = item->getListener();
			if(!listener || !listener->isClipboardPasteable())
			{
				const LLFolderViewFolder* folderp = item->getParentFolder();
				listener = folderp->getListener();
				if (!listener || !listener->isClipboardPasteable())
				{
					return FALSE;
				}
			}
		}
		return TRUE;
	}
	return FALSE;
}

// paste selected item
void LLFolderView::paste()
{
	if(getVisible() && getEnabled())
	{
		// find set of unique folders to paste into
		std::set<LLFolderViewItem*> folder_set;

		selected_items_t::iterator selected_it;
		for (selected_it = mSelectedItems.begin(); selected_it != mSelectedItems.end(); ++selected_it)
		{
			LLFolderViewItem* item = *selected_it;
			LLFolderViewEventListener* listener = item->getListener();
			if (listener->getInventoryType() != LLInventoryType::IT_CATEGORY)
			{
				item = item->getParentFolder();
			}
			folder_set.insert(item);
		}

		std::set<LLFolderViewItem*>::iterator set_iter;
		for(set_iter = folder_set.begin(); set_iter != folder_set.end(); ++set_iter)
		{
			LLFolderViewEventListener* listener = (*set_iter)->getListener();
			if(listener && listener->isClipboardPasteable())
			{
				listener->pasteFromClipboard();
			}
		}
	}
	mSearchString.clear();
}

// public rename functionality - can only start the process
void LLFolderView::startRenamingSelectedItem( void )
{
	// make sure selection is visible
	scrollToShowSelection();

	S32 count = mSelectedItems.size();
	LLFolderViewItem* item = NULL;
	if(count > 0)
	{
		item = mSelectedItems.front();
	}
	if(getVisible() && getEnabled() && (count == 1) && item && item->getListener() &&
	   item->getListener()->isItemRenameable())
	{
		mRenameItem = item;

		updateRenamerPosition();


		mRenamer->setText(item->getName());
		mRenamer->selectAll();
		mRenamer->setVisible( TRUE );
		// set focus will fail unless item is visible
		mRenamer->setFocus( TRUE );
		mRenamer->setTopLostCallback(boost::bind(&LLFolderView::onRenamerLost, this));
		gFocusMgr.setTopCtrl( mRenamer );
	}
}

BOOL LLFolderView::handleKeyHere( KEY key, MASK mask )
{
	BOOL handled = FALSE;

	// SL-51858: Key presses are not being passed to the Popup menu.
	// A proper fix is non-trivial so instead just close the menu.
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu && menu->isOpen())
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}

	switch( key )
	{
	case KEY_F2:
		mSearchString.clear();
		startRenamingSelectedItem();
		handled = TRUE;
		break;

	case KEY_RETURN:
		if (mask == MASK_NONE)
		{
			if( mRenameItem && mRenamer->getVisible() )
			{
				finishRenamingItem();
				mSearchString.clear();
				handled = TRUE;
			}
			else
			{
				LLFolderView::openSelectedItems();
				handled = TRUE;
			}
		}
		break;

	case KEY_ESCAPE:
		if( mRenameItem && mRenamer->getVisible() )
		{
			closeRenamer();
			handled = TRUE;
		}
		mSearchString.clear();
		break;

	case KEY_PAGE_UP:
		mSearchString.clear();
		if (mScrollContainer)
		{
			mScrollContainer->pageUp(30);
		}
		handled = TRUE;
		break;

	case KEY_PAGE_DOWN:
		mSearchString.clear();
		if (mScrollContainer)
		{
			mScrollContainer->pageDown(30);
		}
		handled = TRUE;
		break;

	case KEY_HOME:
		mSearchString.clear();
		if (mScrollContainer)
		{
			mScrollContainer->goToTop();
		}
		handled = TRUE;
		break;

	case KEY_END:
		mSearchString.clear();
		if (mScrollContainer)
		{
			mScrollContainer->goToBottom();
		}
		break;

	case KEY_DOWN:
		if((mSelectedItems.size() > 0) && mScrollContainer)
		{
			LLFolderViewItem* last_selected = getCurSelectedItem();
			bool shift_select = mask & MASK_SHIFT;
			LLFolderViewItem* next = last_selected->getNextOpenNode(!shift_select);

			if (!mKeyboardSelection || (!shift_select && (!next || next == last_selected)))
			{
				setSelection(last_selected, FALSE, TRUE);
				mKeyboardSelection = TRUE;
			}

			if (shift_select)
			{
				if (next)
				{
					if (next->isSelected())
					{
						// shrink selection
						changeSelection(last_selected, FALSE);
					}
					else if (last_selected->getParentFolder() == next->getParentFolder())
					{
						// grow selection
						changeSelection(next, TRUE);
					}
				}
			}
			else
			{
				if( next )
				{
					if (next == last_selected)
					{
						//special case for LLAccordionCtrl
						if(notifyParent(LLSD().with("action","select_next")) > 0 )//message was processed
						{
							clearSelection();
							return TRUE;
						}
						return FALSE;
					}
					setSelection( next, FALSE, TRUE );
				}
				else
				{
					//special case for LLAccordionCtrl
					if(notifyParent(LLSD().with("action","select_next")) > 0 )//message was processed
					{
						clearSelection();
						return TRUE;
					}
					return FALSE;
				}
			}
			scrollToShowSelection();
			mSearchString.clear();
			handled = TRUE;
		}
		break;

	case KEY_UP:
		if((mSelectedItems.size() > 0) && mScrollContainer)
		{
			LLFolderViewItem* last_selected = mSelectedItems.back();
			bool shift_select = mask & MASK_SHIFT;
			// don't shift select down to children of folders (they are implicitly selected through parent)
			LLFolderViewItem* prev = last_selected->getPreviousOpenNode(!shift_select);

			if (!mKeyboardSelection || (!shift_select && prev == this))
			{
				setSelection(last_selected, FALSE, TRUE);
				mKeyboardSelection = TRUE;
			}

			if (shift_select)
			{
				if (prev)
				{
					if (prev->isSelected())
					{
						// shrink selection
						changeSelection(last_selected, FALSE);
					}
					else if (last_selected->getParentFolder() == prev->getParentFolder())
					{
						// grow selection
						changeSelection(prev, TRUE);
					}
				}
			}
			else
			{
				if( prev )
				{
					if (prev == this)
					{
						// If case we are in accordion tab notify parent to go to the previous accordion
						if(notifyParent(LLSD().with("action","select_prev")) > 0 )//message was processed
						{
							clearSelection();
							return TRUE;
						}

						return FALSE;
					}
					setSelection( prev, FALSE, TRUE );
				}
			}
			scrollToShowSelection();
			mSearchString.clear();

			handled = TRUE;
		}
		break;

	case KEY_RIGHT:
		if(mSelectedItems.size())
		{
			LLFolderViewItem* last_selected = getCurSelectedItem();
			last_selected->setOpen( TRUE );
			mSearchString.clear();
			handled = TRUE;
		}
		break;

	case KEY_LEFT:
		if(mSelectedItems.size())
		{
			LLFolderViewItem* last_selected = getCurSelectedItem();
			LLFolderViewItem* parent_folder = last_selected->getParentFolder();
			if (!last_selected->isOpen() && parent_folder && parent_folder->getParentFolder())
			{
				setSelection(parent_folder, FALSE, TRUE);
			}
			else
			{
				last_selected->setOpen( FALSE );
			}
			mSearchString.clear();
			scrollToShowSelection();
			handled = TRUE;
		}
		break;
	}

	if (!handled && mParentPanel.get()->hasFocus())
	{
		if (key == KEY_BACKSPACE)
		{
			mSearchTimer.reset();
			if (mSearchString.size())
			{
				mSearchString.erase(mSearchString.size() - 1, 1);
			}
			search(getCurSelectedItem(), wstring_to_utf8str(mSearchString), FALSE);
			handled = TRUE;
		}
	}

	return handled;
}


BOOL LLFolderView::handleUnicodeCharHere(llwchar uni_char)
{
	if ((uni_char < 0x20) || (uni_char == 0x7F)) // Control character or DEL
	{
		return FALSE;
	}

	BOOL handled = FALSE;
	if (mParentPanel.get()->hasFocus())
	{
		// SL-51858: Key presses are not being passed to the Popup menu.
		// A proper fix is non-trivial so instead just close the menu.
		LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
		if (menu && menu->isOpen())
		{
			LLMenuGL::sMenuContainer->hideMenus();
		}

		//do text search
		static LLUICachedControl<F32> type_ahead_timeout("TypeAheadTimeout", 0.f);
		if (mSearchTimer.getElapsedTimeF32() > type_ahead_timeout)
		{
			mSearchString.clear();
		}
		mSearchTimer.reset();
		if (mSearchString.size() < 128)
		{
			mSearchString += uni_char;
		}
		search(getCurSelectedItem(), wstring_to_utf8str(mSearchString), FALSE);

		handled = TRUE;
	}

	return handled;
}


BOOL LLFolderView::canDoDelete() const
{
	if (mSelectedItems.size() == 0) return FALSE;

	for (selected_items_t::const_iterator item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
	{
		if (!(*item_it)->getListener()->isItemRemovable())
		{
			return FALSE;
		}
	}
	return TRUE;
}

void LLFolderView::doDelete()
{
	if(mSelectedItems.size() > 0)
	{				
		removeSelectedItems();
	}
}


BOOL LLFolderView::handleMouseDown( S32 x, S32 y, MASK mask )
{
	mKeyboardSelection = FALSE;
	mSearchString.clear();

	mParentPanel.get()->setFocus(TRUE);

	LLEditMenuHandler::gEditMenuHandler = this;

	return LLView::handleMouseDown( x, y, mask );
}

BOOL LLFolderView::search(LLFolderViewItem* first_item, const std::string &search_string, BOOL backward)
{
	// get first selected item
	LLFolderViewItem* search_item = first_item;

	// make sure search string is upper case
	std::string upper_case_string = search_string;
	LLStringUtil::toUpper(upper_case_string);

	// if nothing selected, select first item in folder
	if (!search_item)
	{
		// start from first item
		search_item = getNextFromChild(NULL);
	}

	// search over all open nodes for first substring match (with wrapping)
	BOOL found = FALSE;
	LLFolderViewItem* original_search_item = search_item;
	do
	{
		// wrap at end
		if (!search_item)
		{
			if (backward)
			{
				search_item = getPreviousFromChild(NULL);
			}
			else
			{
				search_item = getNextFromChild(NULL);
			}
			if (!search_item || search_item == original_search_item)
			{
				break;
			}
		}

		std::string current_item_label(search_item->getSearchableLabel());
		S32 search_string_length = llmin(upper_case_string.size(), current_item_label.size());
		if (!current_item_label.compare(0, search_string_length, upper_case_string))
		{
			found = TRUE;
			break;
		}
		if (backward)
		{
			search_item = search_item->getPreviousOpenNode();
		}
		else
		{
			search_item = search_item->getNextOpenNode();
		}

	} while(search_item != original_search_item);
	

	if (found)
	{
		setSelection(search_item, FALSE, TRUE);
		scrollToShowSelection();
	}

	return found;
}

BOOL LLFolderView::handleDoubleClick( S32 x, S32 y, MASK mask )
{
	// skip LLFolderViewFolder::handleDoubleClick()
	return LLView::handleDoubleClick( x, y, mask );
}

BOOL LLFolderView::handleRightMouseDown( S32 x, S32 y, MASK mask )
{
	// all user operations move keyboard focus to inventory
	// this way, we know when to stop auto-updating a search
	mParentPanel.get()->setFocus(TRUE);

	BOOL handled = childrenHandleRightMouseDown(x, y, mask) != NULL;
	S32 count = mSelectedItems.size();
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (   handled
		&& ( count > 0 && (hasVisibleChildren() || mFilter.getShowFolderState() == LLInventoryFilter::SHOW_ALL_FOLDERS) ) // show menu only if selected items are visible
		&& menu )
	{
		updateMenuOptions(menu);
	   
		menu->updateParent(LLMenuGL::sMenuContainer);
		LLMenuGL::showPopup(this, menu, x, y);
		//menu->needsArrange(); // update menu height if needed
	}
	else
	{
		if(menu && menu->getVisible())
		{
			menu->setVisible(FALSE);
		}
		setSelection(NULL, FALSE, TRUE);
	}
	return handled;
}

// Add "--no options--" if the menu is completely blank.
BOOL LLFolderView::addNoOptions(LLMenuGL* menu) const
{
	const std::string nooptions_str = "--no options--";
	LLView *nooptions_item = NULL;
	
	const LLView::child_list_t *list = menu->getChildList();
	for (LLView::child_list_t::const_iterator itor = list->begin(); 
		 itor != list->end(); 
		 ++itor)
	{
		LLView *menu_item = (*itor);
		if (menu_item->getVisible())
		{
			return FALSE;
		}
		std::string name = menu_item->getName();
		if (menu_item->getName() == nooptions_str)
		{
			nooptions_item = menu_item;
		}
	}
	if (nooptions_item)
	{
		nooptions_item->setVisible(TRUE);
		nooptions_item->setEnabled(FALSE);
		return TRUE;
	}
	return FALSE;
}

BOOL LLFolderView::handleHover( S32 x, S32 y, MASK mask )
{
	return LLView::handleHover( x, y, mask );
}

BOOL LLFolderView::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data, 
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	mDragAndDropThisFrame = TRUE;
	// have children handle it first
	BOOL handled = LLView::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data,
											 accept, tooltip_msg);

	// when drop is not handled by child, it should be handled
	// by the folder which is the hierarchy root.
	if (!handled)
	{
		if (getListener()->getUUID().notNull())
		{
			handled = LLFolderViewFolder::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg);
		}
		else
		{
			if (!mFolders.empty())
			{
				// dispatch to last folder as a hack to support "Contents" folder in object inventory
				handled = mFolders.back()->handleDragAndDropFromChild(mask,drop,cargo_type,cargo_data,accept,tooltip_msg);
			}
		}
	}

	if (handled)
	{
		LL_DEBUGS("UserInput") << "dragAndDrop handled by LLFolderView" << LL_ENDL;
	}

	return handled;
}

BOOL LLFolderView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mScrollContainer)
	{
		return mScrollContainer->handleScrollWheel(x, y, clicks);
	}
	return FALSE;
}

void LLFolderView::deleteAllChildren()
{
	closeRenamer();
	if (mPopupMenuHandle.get()) mPopupMenuHandle.get()->die();
	mPopupMenuHandle = LLHandle<LLView>();
	mScrollContainer = NULL;
	mRenameItem = NULL;
	mRenamer = NULL;
	mStatusTextBox = NULL;
	
	clearSelection();
	LLView::deleteAllChildren();
}

void LLFolderView::scrollToShowSelection()
{
	// If items are filtered while background fetch is in progress
	// scrollbar resets to the first filtered item. See EXT-3981.
	// However we allow scrolling for folder views with mAutoSelectOverride
	// (used in Places SP) as an exception because the selection in them
	// is not reset during items filtering. See STORM-133.
	if ( (LLInventoryModelBackgroundFetch::instance().isEverythingFetched() || mAutoSelectOverride)
			&& mSelectedItems.size() )
	{
		mNeedsScroll = TRUE;
	}
}

// If the parent is scroll container, scroll it to make the selection
// is maximally visible.
void LLFolderView::scrollToShowItem(LLFolderViewItem* item, const LLRect& constraint_rect)
{
	if (!mScrollContainer) return;

	// don't scroll to items when mouse is being used to scroll/drag and drop
	if (gFocusMgr.childHasMouseCapture(mScrollContainer))
	{
		mNeedsScroll = FALSE;
		return;
	}

	// if item exists and is in visible portion of parent folder...
	if(item)
	{
		LLRect local_rect = item->getLocalRect();
		LLRect item_scrolled_rect; // item position relative to display area of scroller
		
		S32 icon_height = mIcon.isNull() ? 0 : mIcon->getHeight(); 
		S32 label_height = ll_round(getLabelFontForStyle(mLabelStyle)->getLineHeight()); 
		// when navigating with keyboard, only move top of opened folder on screen, otherwise show whole folder
		S32 max_height_to_show = item->isOpen() && mScrollContainer->hasFocus() ? (llmax( icon_height, label_height ) + ICON_PAD) : local_rect.getHeight(); 
		
		// get portion of item that we want to see...
		LLRect item_local_rect = LLRect(item->getIndentation(), 
										local_rect.getHeight(), 
										llmin(MIN_ITEM_WIDTH_VISIBLE, local_rect.getWidth()), 
										llmax(0, local_rect.getHeight() - max_height_to_show));

		LLRect item_doc_rect;

		item->localRectToOtherView(item_local_rect, &item_doc_rect, this); 

		mScrollContainer->scrollToShowRect( item_doc_rect, constraint_rect );

	}
}

void LLFolderView::setScrollContainer(LLScrollContainer* parent)
{
	mScrollContainer = parent;
	parent->setPassBackToChildren(false);
}

LLRect LLFolderView::getVisibleRect()
{
	S32 visible_height = mScrollContainer->getRect().getHeight();
	S32 visible_width = mScrollContainer->getRect().getWidth();
	LLRect visible_rect;
	visible_rect.setLeftTopAndSize(-getRect().mLeft, visible_height - getRect().mBottom, visible_width, visible_height);
	return visible_rect;
}

BOOL LLFolderView::getShowSelectionContext()
{
	if (mShowSelectionContext)
	{
		return TRUE;
	}
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu && menu->getVisible())
	{
		return TRUE;
	}
	return FALSE;
}

void LLFolderView::setShowSingleSelection(bool show)
{
	if (show != mShowSingleSelection)
	{
		mMultiSelectionFadeTimer.reset();
		mShowSingleSelection = show;
	}
}

void LLFolderView::addItemID(const LLUUID& id, LLFolderViewItem* itemp)
{
	mItemMap[id] = itemp;
}

void LLFolderView::removeItemID(const LLUUID& id)
{
	mItemMap.erase(id);
}

LLTrace::BlockTimerStatHandle FTM_GET_ITEM_BY_ID("Get FolderViewItem by ID");
LLFolderViewItem* LLFolderView::getItemByID(const LLUUID& id)
{
	LL_RECORD_BLOCK_TIME(FTM_GET_ITEM_BY_ID);
	if (id == getListener()->getUUID())
	{
		return this;
	}

	auto map_it = mItemMap.find(id);
	if (map_it != mItemMap.end())
	{
		return map_it->second;
	}

	return NULL;
}

LLFolderViewFolder* LLFolderView::getFolderByID(const LLUUID& id)
{
	if (id == getListener()->getUUID())
	{
		return this;
	}

	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end();
		 ++iter)
	{
		LLFolderViewFolder *folder = (*iter);
		if (folder->getListener()->getUUID() == id)
		{
			return folder;
		}
	}
	return NULL;
}


static LLTrace::BlockTimerStatHandle FTM_AUTO_SELECT("Open and Select");
static LLTrace::BlockTimerStatHandle FTM_INVENTORY("Inventory");
// Main idle routine
void LLFolderView::doIdle()
{
	// If this is associated with the user's inventory, don't do anything
	// until that inventory is loaded up.
	const LLInventoryPanel *inventory_panel = dynamic_cast<LLInventoryPanel*>(mParentPanel.get());
	if (inventory_panel && !inventory_panel->getIsViewsInitialized())
	{
		return;
	}

	// We have all items, now we can save them to favorites
	BOOL collectFavoriteItems(LLInventoryModel::item_array_t&); // Singu TODO: Proper Favorites Bar
	LLInventoryModel::item_array_t items;
	collectFavoriteItems(items);
	
	LL_RECORD_BLOCK_TIME(FTM_INVENTORY);

	BOOL debug_filters = gSavedSettings.getBOOL("DebugInventoryFilters");
	if (debug_filters != getDebugFilters())
	{
		mDebugFilters = debug_filters;
		arrangeAll();
	}

	mFilter.clearModified();
	BOOL filter_modified_and_active = mCompletedFilterGeneration < mFilter.getCurrentGeneration() && 
										mFilter.isNotDefault();
	mNeedsAutoSelect = filter_modified_and_active &&
							!(gFocusMgr.childHasKeyboardFocus(this) || gFocusMgr.getMouseCapture());
	
	// filter to determine visibility before arranging
	filterFromRoot();

	// automatically show matching items, and select first one
	// do this every frame until user puts keyboard focus into the inventory window
	// signaling the end of the automatic update
	// only do this when mNeedsFilter is set, meaning filtered items have
	// potentially changed
	if (mNeedsAutoSelect)
	{
		LL_RECORD_BLOCK_TIME(FTM_AUTO_SELECT);
		// select new item only if a filtered item not currently selected
		LLFolderViewItem* selected_itemp = mSelectedItems.empty() ? NULL : mSelectedItems.back();
		if ((selected_itemp && !selected_itemp->getFiltered()) && !mAutoSelectOverride)
		{
			// select first filtered item
			LLSelectFirstFilteredItem filter;
			applyFunctorRecursively(filter);
		}

		// Open filtered folders for folder views with mAutoSelectOverride=TRUE.
		// Used by LLPlacesFolderView.
		if (mAutoSelectOverride && !mFilter.getFilterSubString().empty())
		{
			LLOpenFilteredFolders filter;
			applyFunctorRecursively(filter);
		}

		scrollToShowSelection();
	}

	// during filtering process, try to pin selected item's location on screen
	// this will happen when searching your inventory and when new items arrive
	if (filter_modified_and_active)
	{
		// calculate rectangle to pin item to at start of animated rearrange
		if (!mPinningSelectedItem && !mSelectedItems.empty())
		{
			// lets pin it!
			mPinningSelectedItem = TRUE;

			LLRect visible_content_rect = mScrollContainer->getVisibleContentRect();
			LLFolderViewItem* selected_item = mSelectedItems.back();

			LLRect item_rect;
			selected_item->localRectToOtherView(selected_item->getLocalRect(), &item_rect, this);
			// if item is visible in scrolled region
			if (visible_content_rect.overlaps(item_rect))
			{
				// then attempt to keep it in same place on screen
				mScrollConstraintRect = item_rect;
				mScrollConstraintRect.translate(-visible_content_rect.mLeft, -visible_content_rect.mBottom);
			}
			else
			{
				// otherwise we just want it onscreen somewhere
				LLRect content_rect = mScrollContainer->getContentWindowRect();
				mScrollConstraintRect.setOriginAndSize(0, 0, content_rect.getWidth(), content_rect.getHeight());
			}
		}
	}
	else
	{
		// stop pinning selected item after folders stop rearranging
		if (!needsArrange())
		{
			mPinningSelectedItem = FALSE;
		}
	}

	LLRect constraint_rect;
	if (mPinningSelectedItem)
	{
		// use last known constraint rect for pinned item
		constraint_rect = mScrollConstraintRect;
	}
	else
	{
		// during normal use (page up/page down, etc), just try to fit item on screen
		LLRect content_rect = mScrollContainer->getContentWindowRect();
		constraint_rect.setOriginAndSize(0, 0, content_rect.getWidth(), content_rect.getHeight());
	}


	BOOL is_visible = isInVisibleChain();

	if ( is_visible )
	{
		sanitizeSelection();
		if( needsArrange() )
		{
			arrangeFromRoot();
		}
	}

	if (mSelectedItems.size() && mNeedsScroll)
	{
		scrollToShowItem(mSelectedItems.back(), constraint_rect);
		// continue scrolling until animated layout change is done
		if (!filter_modified_and_active 
			&& (!needsArrange() || !is_visible))
		{
			mNeedsScroll = FALSE;
		}
	}

	if (mSignalSelectCallback)
	{
		//RN: we use keyboard focus as a proxy for user-explicit actions
		BOOL take_keyboard_focus = (mSignalSelectCallback == SIGNAL_KEYBOARD_FOCUS);
		mSelectSignal(mSelectedItems, take_keyboard_focus);
	}
	mSignalSelectCallback = FALSE;
}


//static
void LLFolderView::idle(void* user_data)
{
	LLFolderView* self = (LLFolderView*)user_data;
	if ( self )
	{	// Do the real idle 
		self->doIdle();
	}
}

void LLFolderView::dumpSelectionInformation()
{
	LL_INFOS() << "LLFolderView::dumpSelectionInformation()" << LL_NEWLINE
				<< "****************************************" << LL_ENDL;
	selected_items_t::iterator item_it;
	for (item_it = mSelectedItems.begin(); item_it != mSelectedItems.end(); ++item_it)
	{
		LL_INFOS() << "  " << (*item_it)->getName() << LL_ENDL;
	}
	LL_INFOS() << "****************************************" << LL_ENDL;
}

void LLFolderView::updateRenamerPosition()
{
	if(mRenameItem)
	{
		// See also LLFolderViewItem::draw()
		S32 x = ARROW_SIZE + TEXT_PAD + ICON_WIDTH + ICON_PAD + mRenameItem->getIndentation();
		S32 y = mRenameItem->getRect().getHeight() - mRenameItem->getItemHeight() - RENAME_HEIGHT_PAD;
		mRenameItem->localPointToScreen( x, y, &x, &y );
		screenPointToLocal( x, y, &x, &y );
		mRenamer->setOrigin( x, y );

		LLRect scroller_rect(0, 0, gViewerWindow->getWindowWidthScaled(), 0);
		if (mScrollContainer)
		{
			scroller_rect = mScrollContainer->getContentWindowRect();
		}

		S32 width = llmax(llmin(mRenameItem->getRect().getWidth() - x, scroller_rect.getWidth() - x - getRect().mLeft), MINIMUM_RENAMER_WIDTH);
		S32 height = mRenameItem->getItemHeight() - RENAME_HEIGHT_PAD;
		mRenamer->reshape( width, height, TRUE );
	}
}

// Update visibility and availability (i.e. enabled/disabled) of context menu items.
void LLFolderView::updateMenuOptions(LLMenuGL* menu)
{
	const LLView::child_list_t *list = menu->getChildList();

	LLView::child_list_t::const_iterator menu_itor;
	for (menu_itor = list->begin(); menu_itor != list->end(); ++menu_itor)
	{
		(*menu_itor)->setVisible(FALSE);
		(*menu_itor)->pushVisible(TRUE);
		(*menu_itor)->setEnabled(TRUE);
	}

	// Successively filter out invalid options
	U32 multi_select_flag = (mSelectedItems.size() > 1 ? ITEM_IN_MULTI_SELECTION : 0x0);
	U32 flags = multi_select_flag | FIRST_SELECTED_ITEM;
	for (selected_items_t::iterator item_itor = mSelectedItems.begin();
			item_itor != mSelectedItems.end();
			++item_itor)
	{
		LLFolderViewItem* selected_item = (*item_itor);
		selected_item->buildContextMenu(*menu, flags);
		flags = multi_select_flag;
	}

	// This adds a check for restrictions based on the entire
	// selection set - for example, any one wearable may not push you
	// over the limit, but all wearables together still might.
	if (getFolderViewGroupedItemModel())
	{
		getFolderViewGroupedItemModel()->groupFilterContextMenu(mSelectedItems, *menu);
	}

	addNoOptions(menu);
}

// Refresh the context menu (that is already shown).
void LLFolderView::updateMenu()
{
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu && menu->getVisible())
	{
		updateMenuOptions(menu);
		menu->needsArrange(); // update menu height if needed
	}
}

void LLFolderView::saveFolderState()
{
	mSavedFolderState = std::unique_ptr<LLSaveFolderState>(new LLSaveFolderState());
	applyFunctorRecursively(*mSavedFolderState);
}

void LLFolderView::restoreFolderState()
{
	if (mSavedFolderState)
	{
		mSavedFolderState->setApply(true);
		applyFunctorRecursively(*mSavedFolderState);
		mSavedFolderState.reset();
	}
}

bool LLFolderView::selectFirstItem()
{
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end();++iter)
	{
		LLFolderViewFolder* folder = (*iter );
		if (folder->getVisible())
		{
			LLFolderViewItem* itemp = folder->getNextFromChild(0,true);
			if(itemp)
				setSelection(itemp,FALSE,TRUE);
			return true;	
		}
		
	}
	for(items_t::iterator iit = mItems.begin();
		iit != mItems.end(); ++iit)
	{
		LLFolderViewItem* itemp = (*iit);
		if (itemp->getVisible())
		{
			setSelection(itemp,FALSE,TRUE);
			return true;	
		}
	}
	return false;
}
bool LLFolderView::selectLastItem()
{
	for(items_t::reverse_iterator iit = mItems.rbegin();
		iit != mItems.rend(); ++iit)
	{
		LLFolderViewItem* itemp = (*iit);
		if (itemp->getVisible())
		{
			setSelection(itemp,FALSE,TRUE);
			return true;	
		}
	}
	for (folders_t::reverse_iterator iter = mFolders.rbegin();
		 iter != mFolders.rend();++iter)
	{
		LLFolderViewFolder* folder = (*iter);
		if (folder->getVisible())
		{
			LLFolderViewItem* itemp = folder->getPreviousFromChild(0,true);
			if(itemp)
				setSelection(itemp,FALSE,TRUE);
			return true;	
		}
	}
	return false;
}


S32	LLFolderView::notify(const LLSD& info) 
{
	if(info.has("action"))
	{
		std::string str_action = info["action"];
		if(str_action == "select_first")
		{
			setFocus(true);
			selectFirstItem();
			scrollToShowSelection();
			return 1;

		}
		else if(str_action == "select_last")
		{
			setFocus(true);
			selectLastItem();
			scrollToShowSelection();
			return 1;
		}
	}
	return 0;
}


///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------

void LLFolderView::onRenamerLost()
{
	if (mRenamer && mRenamer->getVisible())
	{
		mRenamer->setVisible(FALSE);

		// will commit current name (which could be same as original name)
		mRenamer->setFocus(FALSE);
	}

	if( mRenameItem )
	{
		setSelectionFromRoot( mRenameItem, TRUE );
		mRenameItem = NULL;
	}
}

void LLFolderView::setFilterPermMask( PermissionMask filter_perm_mask )
{
	mFilter.setFilterPermissions(filter_perm_mask);
}

U32 LLFolderView::getFilterObjectTypes() const
{
	return mFilter.getFilterObjectTypes();
}

PermissionMask LLFolderView::getFilterPermissions() const
{
	return mFilter.getFilterPermissions();
}

BOOL LLFolderView::isFilterModified()
{
	return mFilter.isNotDefault();
}

void delete_selected_item(void* user_data)
{
	if(user_data)
	{
		LLFolderView* fv = reinterpret_cast<LLFolderView*>(user_data);
		fv->removeSelectedItems();
	}
}

void copy_selected_item(void* user_data)
{
	if(user_data)
	{
		LLFolderView* fv = reinterpret_cast<LLFolderView*>(user_data);
		fv->copy();
	}
}

void paste_items(void* user_data)
{
	if(user_data)
	{
		LLFolderView* fv = reinterpret_cast<LLFolderView*>(user_data);
		fv->paste();
	}
}

void open_selected_items(void* user_data)
{
	if(user_data)
	{
		LLFolderView* fv = reinterpret_cast<LLFolderView*>(user_data);
		fv->openSelectedItems();
	}
}

void properties_selected_items(void* user_data)
{
	if(user_data)
	{
		LLFolderView* fv = reinterpret_cast<LLFolderView*>(user_data);
		fv->propertiesSelectedItems();
	}
}
