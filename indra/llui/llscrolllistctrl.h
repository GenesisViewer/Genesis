/** 
 * @file llscrolllistctrl.h
 * @brief A scrolling list of items.  This is the one you want to use
 * in UI code.  LLScrollListCell, LLScrollListItem, etc. are utility
 * classes.
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

#ifndef LL_SCROLLLISTCTRL_H
#define LL_SCROLLLISTCTRL_H

#include <vector>
#include <deque>

#include "lfidbearer.h"
#include "lluictrl.h"
#include "llctrlselectioninterface.h"
#include "llfontgl.h"
#include "llui.h"
#include "llstring.h"	// LLWString
#include "lleditmenuhandler.h"
#include "llframetimer.h"

#include "llscrollbar.h"
#include "llscrolllistitem.h"
#include "llscrolllistcolumn.h"

class LLMenuGL;

class LLScrollListCtrl : public LLUICtrl, public LLEditMenuHandler, 
	public LLCtrlListInterface, public LLCtrlScrollInterface
,	public LFIDBearer
{
public:
	typedef boost::function<void (void)> callback_t;

	template<typename T> struct maximum
	{
		typedef T result_type;

		template<typename InputIterator>
		T operator()(InputIterator first, InputIterator last) const
		{
			// If there are no slots to call, just return the
			// default-constructed value
			if(first == last ) return T();
			T max_value = *first++;
			while (first != last) {
				if (max_value < *first)
				max_value = *first;
				++first;
			}

			return max_value;
		}
	};

	
	typedef boost::signals2::signal<S32 (S32,const LLScrollListItem*,const LLScrollListItem*),maximum<S32> > sort_signal_t;

	LLScrollListCtrl(const std::string& name, const LLRect& rect, commit_callback_t commit_callback, bool multi_select, bool has_border = true, bool draw_heading = false);

public:
	virtual ~LLScrollListCtrl();

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	void setScrollListParameters(LLXMLNodePtr node);
	static LLView* fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory);

	S32				isEmpty() const;

	void			deleteAllItems() { clearRows(); }
	
	// Sets an array of column descriptors
	void 	   		setColumnHeadings(const LLSD& headings);
	void   			sortByColumnIndex(U32 column, BOOL ascending);
	
	// LLCtrlListInterface functions
	virtual S32  getItemCount() const;
	// Adds a single column descriptor: ["name" : string, "label" : string, "width" : integer, "relwidth" : integer ]
	virtual void addColumn(const LLScrollListColumn::Params& column, EAddPosition pos = ADD_BOTTOM);
	virtual void addColumn(const LLSD& column, EAddPosition pos = ADD_BOTTOM);
	virtual void clearColumns();
	virtual void setColumnLabel(const std::string& column, const std::string& label);
	virtual LLScrollListColumn* getColumn(S32 index);
	virtual LLScrollListColumn* getColumn(const std::string& name);
	virtual S32 getNumColumns() const { return mColumnsIndexed.size(); }

	// Adds a single element, from an array of:
	// "columns" => [ "column" => column name, "value" => value, "type" => type, "font" => font, "font-style" => style ], "id" => uuid
	// Creates missing columns automatically.
	virtual LLScrollListItem* addElement(const LLSD& element, EAddPosition pos = ADD_BOTTOM, void* userdata = NULL);
	virtual LLScrollListItem* addRow(LLScrollListItem *new_item, const LLScrollListItem::Params& value, EAddPosition pos = ADD_BOTTOM);
	virtual LLScrollListItem* addRow(const LLScrollListItem::Params& value, EAddPosition pos = ADD_BOTTOM);
	// Simple add element. Takes a single array of:
	// [ "value" => value, "font" => font, "font-style" => style ]
	virtual void clearRows(); // clears all elements
	virtual void sortByColumn(const std::string& name, BOOL ascending);

	// These functions take and return an array of arrays of elements, as above
	virtual void	setValue(const LLSD& value );
	virtual LLSD	getValue() const;

	LLCtrlSelectionInterface*	getSelectionInterface()	{ return (LLCtrlSelectionInterface*)this; }
	LLCtrlListInterface*		getListInterface()		{ return (LLCtrlListInterface*)this; }
	LLCtrlScrollInterface*		getScrollInterface()	{ return (LLCtrlScrollInterface*)this; }

	// DEPRECATED: Use setSelectedByValue() below.
	BOOL			setCurrentByID( const LLUUID& id )	{ return selectByID(id); }
	virtual LLUUID	getCurrentID() const				{ return getStringUUIDSelectedItem(); }

	BOOL			operateOnSelection(EOperation op);
	BOOL			operateOnAll(EOperation op);

	// returns FALSE if unable to set the max count so low
	BOOL 			setMaxItemCount(S32 max_count);

	BOOL			selectByID( const LLUUID& id );		// FALSE if item not found

	// Match item by value.asString(), which should work for string, integer, uuid.
	// Returns FALSE if not found.
	BOOL			setSelectedByValue(const LLSD& value, BOOL selected);

	BOOL			isSorted() const { return mSorted; }

	virtual BOOL	isSelected(const LLSD& value) const;

	BOOL			handleClick(S32 x, S32 y, MASK mask);
	BOOL			selectFirstItem();
	BOOL			selectNthItem( S32 index );
	BOOL			selectItemRange( S32 first, S32 last );
	BOOL			selectItemAt(S32 x, S32 y, MASK mask);
	
	void			deleteSingleItem( S32 index );
	void			deleteItems(const LLSD& sd);
	void 			deleteSelectedItems();
	void			deselectAllItems(BOOL no_commit_on_change = FALSE);	// by default, go ahead and commit on selection change

	void			clearHighlightedItems();
	
	virtual void	mouseOverHighlightNthItem( S32 index );

	S32				getHighlightedItemInx() const { return mHighlightedItem; } 
	
	void			setDoubleClickCallback( callback_t cb ) { mOnDoubleClickCallback = cb; }
	void			setMaximumSelectCallback( callback_t cb) { mOnMaximumSelectCallback = cb; }
	void			setSortChangedCallback( callback_t cb) 	{ mOnSortChangedCallback = cb; }
	// Convenience function; *TODO: replace with setter above + boost::bind() in calling code
	void			setDoubleClickCallback( boost::function<void (void* userdata)> cb, void* userdata) { mOnDoubleClickCallback = boost::bind(cb, userdata); }

	void			swapWithNext(S32 index);
	void			swapWithPrevious(S32 index);
	void			moveToFront(S32 index);

	void			setCanSelect(BOOL can_select)		{ mCanSelect = can_select; }
	virtual BOOL	getCanSelect() const				{ return mCanSelect; }

	S32				getItemIndex( LLScrollListItem* item ) const;
	S32				getItemIndex( const LLUUID& item_id ) const;

	void setCommentText( const std::string& comment_text);
	LLScrollListItem* addSeparator(EAddPosition pos);

	// "Simple" interface: use this when you're creating a list that contains only unique strings, only
	// one of which can be selected at a time.
	virtual LLScrollListItem* addSimpleElement(const std::string& value, EAddPosition pos = ADD_BOTTOM, const LLSD& id = LLSD());

	BOOL			selectItemByLabel( const std::string& item, BOOL case_sensitive = TRUE );		// FALSE if item not found
	BOOL			selectItemByPrefix(const std::string& target, BOOL case_sensitive = TRUE);
	BOOL			selectItemByPrefix(const LLWString& target, BOOL case_sensitive = TRUE);
	LLScrollListItem*  getItemByLabel( const std::string& item, BOOL case_sensitive = TRUE, S32 column = 0 );
	const std::string	getSelectedItemLabel(S32 column = 0) const;
	LLSD			getSelectedValue();

	// DEPRECATED: Use LLSD versions of addCommentText() and getSelectedValue().
	// "StringUUID" interface: use this when you're creating a list that contains non-unique strings each of which
	// has an associated, unique UUID, and only one of which can be selected at a time.
	LLScrollListItem*	addStringUUIDItem(const std::string& item_text, const LLUUID& id, EAddPosition pos = ADD_BOTTOM, BOOL enabled = TRUE);
	LLUUID				getStringUUIDSelectedItem() const override final;

	LLScrollListItem*	getFirstSelected() const;
	virtual S32			getFirstSelectedIndex() const;
	std::vector<LLScrollListItem*> getAllSelected() const;
	uuid_vec_t 	getSelectedIDs() const override final; //Helper. Much like getAllSelected, but just provides a LLUUID vec
	S32                 getNumSelected() const;
	LLScrollListItem*	getLastSelectedItem() const { return mLastSelected; }

	// iterate over all items
	LLScrollListItem*	getFirstData() const;
	LLScrollListItem*	getLastData() const;
	std::vector<LLScrollListItem*>	getAllData() const;
	uuid_vec_t getAllIDs() const; //Helper. Much like getAllData, but just provides a LLUUID vec

	LLScrollListItem*	getItem(const LLSD& sd) const;
	
	void setAllowMultipleSelection(BOOL mult )	{ mAllowMultipleSelection = mult; }

	void setBgWriteableColor(const LLColor4 &c)	{ mBgWriteableColor = c; }
	void setReadOnlyBgColor(const LLColor4 &c)	{ mBgReadOnlyColor = c; }
	void setBgSelectedColor(const LLColor4 &c)	{ mBgSelectedColor = c; }
	void setBgStripeColor(const LLColor4& c)	{ mBgStripeColor = c; }
	void setFgSelectedColor(const LLColor4 &c)	{ mFgSelectedColor = c; }
	void setFgUnselectedColor(const LLColor4 &c){ mFgUnselectedColor = c; }
	void setHighlightedColor(const LLColor4 &c) { mHighlightedColor = c; }
	void setFgDisableColor(const LLColor4 &c)	{ mFgDisabledColor = c; }

	void setBackgroundVisible(BOOL b)			{ mBackgroundVisible = b; }
	void setDrawStripes(BOOL b)					{ mDrawStripes = b; }
	void setColumnPadding(const S32 c)          { mColumnPadding = c; }
	S32  getColumnPadding()						{ return mColumnPadding; }
	void setCommitOnKeyboardMovement(BOOL b)	{ mCommitOnKeyboardMovement = b; }
	void setCommitOnSelectionChange(BOOL b)		{ mCommitOnSelectionChange = b; }
	void setAllowKeyboardMovement(BOOL b)		{ mAllowKeyboardMovement = b; }

	void			setMaxSelectable(U32 max_selected) { mMaxSelectable = max_selected; }
	S32				getMaxSelectable() { return mMaxSelectable; }


	virtual S32		getScrollPos() const;
	virtual void	setScrollPos( S32 pos );

	// <edit>
	S32 getPageLines() { return mPageLines; }
	// </edit>

	S32 getSearchColumn();
	void			setSearchColumn(S32 column) { mSearchColumn = column; }
	S32				getColumnIndexFromOffset(S32 x);
	S32				getColumnOffsetFromIndex(S32 index);
	S32				getRowOffsetFromIndex(S32 index);

	void			clearSearchString() { mSearchString.clear(); }

	bool			filterItem(LLScrollListItem* item);
	void			setFilter(const std::string& filter);

	// Context Menus
	void setContextMenu(LLMenuGL* menu) { mPopupMenu = menu; }
	void setContextMenu(U8 index) { mPopupMenu = sMenus[index]; }
	void setContextMenu(const std::string& menu);

	Type getSelectedType() const override
	{
		for (auto i = 0; mPopupMenu && i < COUNT; ++i)
			if (sMenus[i] == mPopupMenu)
				return (Type)i;
		return LFIDBearer::getSelectedType();
	}

	// Overridden from LLView
	/*virtual*/ void    draw();
	/*virtual*/ BOOL	handleMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleMouseUp(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleRightMouseDown(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleDoubleClick(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleHover(S32 x, S32 y, MASK mask);
	/*virtual*/ BOOL	handleKeyHere(KEY key, MASK mask);
	/*virtual*/ BOOL	handleUnicodeCharHere(llwchar uni_char);
	/*virtual*/ BOOL	handleScrollWheel(S32 x, S32 y, S32 clicks);
	/*virtual*/ BOOL	handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect);
	/*virtual*/ void	setEnabled(BOOL enabled);
	/*virtual*/ void	setFocus( BOOL b );
	/*virtual*/ void	onFocusReceived();
	/*virtual*/ void	onFocusLost();
	/*virtual*/ void	onMouseLeave(S32 x, S32 y, MASK mask);
	/*virtual*/ void	reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);

	virtual BOOL	isDirty() const;
	virtual void	resetDirty();		// Clear dirty state

	virtual void	updateLayout();
	void			adjustScrollbar(S32 doc_size);
	virtual void	fitContents(S32 max_width, S32 max_height);

	virtual LLRect	getRequiredRect();
	static  BOOL    rowPreceeds(LLScrollListItem *new_row, LLScrollListItem *test_row);

	LLRect			getItemListRect() { return mItemListRect; }

	/// Returns rect, in local coords, of a given row/column
	LLRect			getCellRect(S32 row_index, S32 column_index);

	// Used "internally" by the scroll bar.
	void			onScrollChange( S32 new_pos, LLScrollbar* src );

	static void onClickColumn(void *userdata);

	virtual void updateColumns(bool force_update = false);
	S32 calcMaxContentWidth();
	bool updateColumnWidths();
	S32 getMaxContentWidth() { return mMaxContentWidth; }

	void setHeadingHeight(S32 heading_height);
	/**
	 * Sets  max visible  lines without scroolbar, if this value equals to 0,
	 * then display all items.
	 */
	void setPageLines(S32 page_lines );
	void setCollapseEmptyColumns(BOOL collapse);

	LLScrollListItem*	hitItem(S32 x,S32 y);
	virtual void		scrollToShowSelected();

	// LLEditMenuHandler functions
	void			copy() const override final;
	virtual BOOL	canCopy() const;
	virtual void	cut();
	virtual BOOL	canCut() const;
	virtual void	selectAll();
	virtual BOOL	canSelectAll() const;
	virtual void	deselect();
	virtual BOOL	canDeselect() const;

	void setNumDynamicColumns(S32 num) { mNumDynamicWidthColumns = num; }
	void updateStaticColumnWidth(LLScrollListColumn* col, S32 new_width);
	S32 getTotalStaticColumnWidth() { return mTotalStaticColumnWidth; }

	typedef std::pair<S32, bool> sort_column_t;
	typedef std::vector<sort_column_t> sort_order_t;
	const sort_order_t& getSortOrder() const { return mSortColumns; }
	std::string     getSortColumnName();
	BOOL			getSortAscending() { return mSortColumns.empty() ? TRUE : mSortColumns.back().second; }
	BOOL			hasSortOrder() const;
	void			setSortOrder(const sort_order_t& order);
	void			clearSortOrder();
	void			setSortEnabled(bool sort);

	template<typename T> S32 selectMultiple(const std::vector<T>& vec)
	{
		size_t count = 0;
		for (item_list::iterator iter = mItemList.begin(); iter != mItemList.end(); ++iter)
		{
			LLScrollListItem* item = *iter;
			for (typename std::vector<T>::const_iterator titr = vec.begin(); titr != vec.end(); ++titr)
				if (item->getEnabled() && static_cast<T>(item->getValue()) == (*titr))
				{
					selectItem(item, false);
					++count;
					break;
				}
		}
		if (mCommitOnSelectionChange) commitIfChanged();
		return count;
	}
	S32		selectMultiple( uuid_vec_t ids );
	// conceptually const, but mutates mItemList
	void			updateSort() const;
	// sorts a list without affecting the permanent sort order (so further list insertions can be unsorted, for example)
	void			sortOnce(S32 column, BOOL ascending);

	// manually call this whenever editing list items in place to flag need for resorting
	void			setNeedsSort(bool val = true) { mSorted = !val; }
	void			setNeedsSortColumn(S32 col)
	{
		if(!isSorted())return;
		for(auto it=mSortColumns.begin();it!=mSortColumns.end();++it)
		{
			if((*it).first == col)
			{
				setNeedsSort();
				break;
			}
		}
	}
	void			dirtyColumns(); // some operation has potentially affected column layout or ordering

	boost::signals2::connection setSortCallback(sort_signal_t::slot_type cb )
	{
		if (!mSortCallback) mSortCallback = new sort_signal_t();
		return mSortCallback->connect(cb);
	}

	S32				getLinesPerPage();

protected:
	// "Full" interface: use this when you're creating a list that has one or more of the following:
	// * contains icons
	// * contains multiple columns
	// * allows multiple selection
	// * has items that are not guarenteed to have unique names
	// * has additional per-item data (e.g. a UUID or void* userdata)
	//
	// To add items using this approach, create new LLScrollListItems and LLScrollListCells.  Add the
	// cells (column entries) to each item, and add the item to the LLScrollListCtrl.
	//
	// The LLScrollListCtrl owns its items and is responsible for deleting them
	// (except in the case that the addItem() call fails, in which case it is up
	// to the caller to delete the item)
	//
	// returns FALSE if item faile to be added to list, does NOT delete 'item'
	BOOL			addItem( LLScrollListItem* item, EAddPosition pos = ADD_BOTTOM, BOOL requires_column = TRUE );

	typedef std::deque<LLScrollListItem *> item_list;
	item_list&		getItemList() { return mItemList; }

	void			updateLineHeight();

private:
	void			selectPrevItem(BOOL extend_selection);
	void			selectNextItem(BOOL extend_selection);
	void			drawItems();

	void            updateLineHeightInsert(LLScrollListItem* item);
	void			reportInvalidInput();
	BOOL			isRepeatedChars(const LLWString& string) const;
	void			selectItem(LLScrollListItem* itemp, BOOL single_select = TRUE);
	void			deselectItem(LLScrollListItem* itemp);
	void			commitIfChanged();
	BOOL			setSort(S32 column, BOOL ascending);

	S32				mLineHeight;	// the max height of a single line
	S32				mScrollLines;	// how many lines we've scrolled down
	S32				mPageLines;		// max number of lines is it possible to see on the screen given mRect and mLineHeight
	S32				mHeadingHeight;	// the height of the column header buttons, if visible
	U32				mMaxSelectable; 
	LLScrollbar*	mScrollbar;
	bool 			mAllowMultipleSelection;
	bool			mAllowKeyboardMovement;
	bool			mCommitOnKeyboardMovement;
	bool			mCommitOnSelectionChange;
	bool			mSelectionChanged;
	bool			mNeedsScroll;
	bool			mMouseWheelOpaque;
	bool			mCanSelect;
	bool			mDisplayColumnHeaders;
	bool			mColumnsDirty;
	bool			mColumnWidthsDirty;
	bool			mSortEnabled;

	mutable item_list	mItemList;

	LLScrollListItem *mLastSelected;

	S32				mMaxItemCount; 

	LLRect			mItemListRect;
	S32				mMaxContentWidth;
	S32             mColumnPadding;

	BOOL			mBackgroundVisible;
	BOOL			mDrawStripes;

	LLColor4		mBgWriteableColor;
	LLColor4		mBgReadOnlyColor;
	LLColor4		mBgSelectedColor;
	LLColor4		mBgStripeColor;
	LLColor4		mFgSelectedColor;
	LLColor4		mFgUnselectedColor;
	LLColor4		mFgDisabledColor;
	LLColor4		mHighlightedColor;

	S32				mBorderThickness;
	callback_t		mOnDoubleClickCallback;
	callback_t 		mOnMaximumSelectCallback;
	callback_t 		mOnSortChangedCallback;

	S32				mHighlightedItem;
	class LLViewBorder*	mBorder;
	LLMenuGL	*mPopupMenu;

	LLView			*mCommentTextView;

	LLWString		mSearchString;
	LLFrameTimer	mSearchTimer;

	std::string		mFilter;
	
	S32				mSearchColumn;
	S32				mNumDynamicWidthColumns;
	S32				mTotalStaticColumnWidth;
	S32				mTotalColumnPadding;

	mutable bool	mSorted;
	
	typedef std::map<std::string, LLScrollListColumn*> column_map_t;
	column_map_t mColumns;

	bool			mDirty;
	S32				mOriginalSelection;

	typedef std::vector<LLScrollListColumn*> ordered_columns_t;
	ordered_columns_t	mColumnsIndexed;

	sort_order_t	mSortColumns;

	sort_signal_t*	mSortCallback;
}; // end class LLScrollListCtrl

#endif  // LL_SCROLLLISTCTRL_H
