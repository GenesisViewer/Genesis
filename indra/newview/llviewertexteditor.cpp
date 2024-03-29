/** 
 * @file llviewertexteditor.cpp
 * @brief Text editor widget to let users enter a multi-line document.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * 
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

#include "llviewertexteditor.h"

#include "llagent.h"
#include "llaudioengine.h"
#include "llavataractions.h"
#include "llfloaterworldmap.h"
#include "llfocusmgr.h"
#include "llinventorybridge.h"	// for landmark prefix string
#include "llinventorydefines.h"
#include "llinventorymodel.h"
#include "llmemorystream.h"
#include "llmenugl.h"
#include "llnotecard.h"
#include "llnotificationsutil.h"
#include "llpreview.h"
#include "llpreviewtexture.h"
#include "llpreviewnotecard.h"
#include "llscrollbar.h"
#include "lltooldraganddrop.h"
#include "lluictrlfactory.h"
#include "llviewerassettype.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

void open_landmark(LLViewerInventoryItem* inv_item, const std::string& title, BOOL show_keep_discard, const LLUUID& source_id, BOOL take_focus);
extern BOOL gPacificDaylightTime;

static LLRegisterWidget<LLViewerTextEditor> r("text_editor");

///----------------------------------------------------------------------------
/// Class LLEmbeddedNotecardOpener
///----------------------------------------------------------------------------
class LLEmbeddedNotecardOpener : public LLInventoryCallback
{
	LLViewerTextEditor* mTextEditor;

public:
	LLEmbeddedNotecardOpener()
		: mTextEditor(NULL)
	{
	}

	void setEditor(LLViewerTextEditor* e) {mTextEditor = e;}

	// override
	void fire(const LLUUID& inv_item)
	{
		if(!mTextEditor)
		{
			// The parent text editor may have vanished by now. 
            // In that case just quit.
			return;
		}

		LLInventoryItem* item = gInventory.getItem(inv_item);
		if(!item)
		{
			LL_WARNS() << "Item add reported, but not found in inventory!: " << inv_item << LL_ENDL;
		}
		else
		{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
			if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE))
			{
				RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_NOTECARD);
				return;
			}
// [/RLVa:KB]

			// See if we can bring an existing preview to the front
			if(!LLPreview::show(item->getUUID(), true))
			{
				if (gSavedSettings.getBOOL("ShowNewInventory")) // Singu Note: ShowNewInventory is true, not false, when they want a preview
				{
					// There isn't one, so make a new preview
					S32 left, top;
					gFloaterView->getNewFloaterPosition(&left, &top);
					LLRect rect = gSavedSettings.getRect("NotecardEditorRect");
					rect.translate(left - rect.mLeft, top - rect.mTop);
					LLPreviewNotecard* preview;
					preview = new LLPreviewNotecard("preview notecard", 
													rect, 
													std::string("Embedded Note: ") + item->getName(),
													item->getUUID(), 
													LLUUID::null, 
													item->getAssetUUID(),
													true, 
													(LLViewerInventoryItem*)item);
					preview->setSourceID(LLUUID::null);
					preview->setFocus(TRUE);

					// Force to be entirely onscreen.
					gFloaterView->adjustToFitScreen(preview, FALSE);
				}
			}
		}
	}
};

////////////////////////////////////////////////////////////
// LLEmbeddedItems
//
// Embedded items are stored as:
// * A global map of llwchar to LLInventoryItem
// ** This is unique for each item embedded in any notecard
//    to support copy/paste across notecards
// * A per-notecard set of embeded llwchars for easy removal
//   from the global list
// * A per-notecard vector of embedded lwchars for mapping from
//   old style 0x80 + item format notechards

class LLEmbeddedItems
{
public:
	LLEmbeddedItems(const LLViewerTextEditor* editor);
	~LLEmbeddedItems();
	void clear();

	// return true if there are no embedded items.
	bool empty();
	
	void	bindEmbeddedChars(const LLFontGL* font) const;
	void	unbindEmbeddedChars(const LLFontGL* font) const;

	BOOL	insertEmbeddedItem(LLInventoryItem* item, llwchar* value, bool is_new);
	BOOL	removeEmbeddedItem( llwchar ext_char );

	BOOL	hasEmbeddedItem(llwchar ext_char); // returns TRUE if /this/ editor has an entry for this item

	void	getEmbeddedItemList( std::vector<LLPointer<LLInventoryItem> >& items );
	void	addItems(const std::vector<LLPointer<LLInventoryItem> >& items);

	llwchar	getEmbeddedCharFromIndex(S32 index);

	void 	removeUnusedChars();
	void	copyUsedCharsToIndexed();
	S32		getIndexFromEmbeddedChar(llwchar wch);

	void	markSaved();
	
	static LLInventoryItem* getEmbeddedItem(llwchar ext_char); // returns item from static list
	static BOOL getEmbeddedItemSaved(llwchar ext_char); // returns whether item from static list is saved

private:

	struct embedded_info_t
	{
		LLPointer<LLInventoryItem> mItem;
		BOOL mSaved;
	};
	typedef std::map<llwchar, embedded_info_t > item_map_t;
	static item_map_t sEntries;
	static std::stack<llwchar> sFreeEntries;

	std::set<llwchar> mEmbeddedUsedChars;	 // list of used llwchars
	std::vector<llwchar> mEmbeddedIndexedChars; // index -> wchar for 0x80 + index format
	const LLViewerTextEditor* mEditor;
};

//statics
LLEmbeddedItems::item_map_t LLEmbeddedItems::sEntries;
std::stack<llwchar> LLEmbeddedItems::sFreeEntries;

LLEmbeddedItems::LLEmbeddedItems(const LLViewerTextEditor* editor)
	: mEditor(editor)
{
}

LLEmbeddedItems::~LLEmbeddedItems()
{
	clear();
}

void LLEmbeddedItems::clear()
{
	// Remove entries for this editor from static list
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin();
		 iter != mEmbeddedUsedChars.end();)
	{
		std::set<llwchar>::iterator nextiter = iter++;
		removeEmbeddedItem(*nextiter);
	}
	mEmbeddedUsedChars.clear();
	mEmbeddedIndexedChars.clear();
}

bool LLEmbeddedItems::empty()
{
	removeUnusedChars();
	return mEmbeddedUsedChars.empty();
}

// Inserts a new unique entry
BOOL LLEmbeddedItems::insertEmbeddedItem( LLInventoryItem* item, llwchar* ext_char, bool is_new)
{
	// Now insert a new one
	llwchar wc_emb;
	if (!sFreeEntries.empty())
	{
		wc_emb = sFreeEntries.top();
		sFreeEntries.pop();
	}
	else if (sEntries.empty())
	{
		wc_emb = LLTextEditor::FIRST_EMBEDDED_CHAR;
	}
	else
	{
		item_map_t::iterator last = sEntries.end();
		--last;
		wc_emb = last->first;
		if (wc_emb >= LLTextEditor::LAST_EMBEDDED_CHAR)
		{
			return FALSE;
		}
		++wc_emb;
	}

	sEntries[wc_emb].mItem = item;
	sEntries[wc_emb].mSaved = is_new ? FALSE : TRUE;
	*ext_char = wc_emb;
	mEmbeddedUsedChars.insert(wc_emb);
	return TRUE;
}

// Removes an entry (all entries are unique)
BOOL LLEmbeddedItems::removeEmbeddedItem( llwchar ext_char )
{
	mEmbeddedUsedChars.erase(ext_char);
	item_map_t::iterator iter = sEntries.find(ext_char);
	if (iter != sEntries.end())
	{
		sEntries.erase(ext_char);
		sFreeEntries.push(ext_char);
		return TRUE;
	}
	return FALSE;
}
	
// static
LLInventoryItem* LLEmbeddedItems::getEmbeddedItem(llwchar ext_char)
{
	if( ext_char >= LLTextEditor::FIRST_EMBEDDED_CHAR && ext_char <= LLTextEditor::LAST_EMBEDDED_CHAR )
	{
		item_map_t::iterator iter = sEntries.find(ext_char);
		if (iter != sEntries.end())
		{
			return iter->second.mItem;
		}
	}
	return NULL;
}

// static
BOOL LLEmbeddedItems::getEmbeddedItemSaved(llwchar ext_char)
{
	if( ext_char >= LLTextEditor::FIRST_EMBEDDED_CHAR && ext_char <= LLTextEditor::LAST_EMBEDDED_CHAR )
	{
		item_map_t::iterator iter = sEntries.find(ext_char);
		if (iter != sEntries.end())
		{
			return iter->second.mSaved;
		}
	}
	return FALSE;
}

llwchar	LLEmbeddedItems::getEmbeddedCharFromIndex(S32 index)
{
	if (index >= (S32)mEmbeddedIndexedChars.size())
	{
		LL_WARNS() << "No item for embedded char " << index << " using LL_UNKNOWN_CHAR" << LL_ENDL;
		return LL_UNKNOWN_CHAR;
	}
	return mEmbeddedIndexedChars[index];
}

void LLEmbeddedItems::removeUnusedChars()
{
	std::set<llwchar> used = mEmbeddedUsedChars;
	const LLWString& wtext = mEditor->getWText();
	for (S32 i=0; i<(S32)wtext.size(); i++)
	{
		llwchar wc = wtext[i];
		if( wc >= LLTextEditor::FIRST_EMBEDDED_CHAR && wc <= LLTextEditor::LAST_EMBEDDED_CHAR )
		{
			used.erase(wc);
		}
	}
	// Remove chars not actually used
	for (std::set<llwchar>::iterator iter = used.begin();
		 iter != used.end(); ++iter)
	{
		removeEmbeddedItem(*iter);
	}
}

void LLEmbeddedItems::copyUsedCharsToIndexed()
{
	// Prune unused items
	removeUnusedChars();

	// Copy all used llwchars to mEmbeddedIndexedChars
	mEmbeddedIndexedChars.clear();
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin();
		 iter != mEmbeddedUsedChars.end(); ++iter)
	{
		mEmbeddedIndexedChars.push_back(*iter);
	}
}

S32 LLEmbeddedItems::getIndexFromEmbeddedChar(llwchar wch)
{
	S32 idx = 0;
	for (std::vector<llwchar>::iterator iter = mEmbeddedIndexedChars.begin();
		 iter != mEmbeddedIndexedChars.end(); ++iter)
	{
		if (wch == *iter)
			break;
		++idx;
	}
	if (idx < (S32)mEmbeddedIndexedChars.size())
	{
		return idx;
	}
	else
	{
		LL_WARNS() << "Embedded char " << wch << " not found, using 0" << LL_ENDL;
		return 0;
	}
}

BOOL LLEmbeddedItems::hasEmbeddedItem(llwchar ext_char)
{
	std::set<llwchar>::iterator iter = mEmbeddedUsedChars.find(ext_char);
	if (iter != mEmbeddedUsedChars.end())
	{
		return TRUE;
	}
	return FALSE;
}

void LLEmbeddedItems::bindEmbeddedChars( const LLFontGL* font ) const
{
	if( sEntries.empty() )
	{
		return; 
	}

	for (std::set<llwchar>::const_iterator iter1 = mEmbeddedUsedChars.begin(); iter1 != mEmbeddedUsedChars.end(); ++iter1)
	{
		llwchar wch = *iter1;
		item_map_t::iterator iter2 = sEntries.find(wch);
		if (iter2 == sEntries.end())
		{
			continue;
		}
		LLInventoryItem* item = iter2->second.mItem;
		if (!item)
		{
			continue;
		}
		const char* img_name;
		switch( item->getType() )
		{
		  case LLAssetType::AT_TEXTURE:
			if(item->getInventoryType() == LLInventoryType::IT_SNAPSHOT)
			{
				img_name = "inv_item_snapshot.tga";
			}
			else
			{
				img_name = "inv_item_texture.tga";
			}

			break;
		  case LLAssetType::AT_SOUND:			img_name = "inv_item_sound.tga";	break;
		  case LLAssetType::AT_CALLINGCARD:		img_name = "inv_item_callingcard_online.tga";   break;
		  case LLAssetType::AT_LANDMARK:		
			if (item->getFlags() & LLInventoryItemFlags::II_FLAGS_LANDMARK_VISITED)
			{
				img_name = "inv_item_landmark_visited.tga";	
			}
			else
			{
				img_name = "inv_item_landmark.tga";	
			}
			break;
		  case LLAssetType::AT_CLOTHING:		img_name = "inv_item_clothing.tga";	break;
		  case LLAssetType::AT_OBJECT:			
			if (item->getFlags() & LLInventoryItemFlags::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS)
			{
				img_name = "inv_item_object_multi.tga";	
			}
			else
			{
				img_name = "inv_item_object.tga";	
			}
			break;
		  case LLAssetType::AT_NOTECARD:		img_name = "inv_item_notecard.tga";	break;
		  case LLAssetType::AT_LSL_TEXT:		img_name = "inv_item_script.tga";	break;
		  case LLAssetType::AT_BODYPART:		img_name = "inv_item_skin.tga";	break;
		  case LLAssetType::AT_ANIMATION:		img_name = "inv_item_animation.tga";break;
		  case LLAssetType::AT_GESTURE:			img_name = "inv_item_gesture.tga";	break;
			case LLAssetType::AT_MESH:      	img_name = "Inv_Mesh";	    break;
            case LLAssetType::AT_SETTINGS:      img_name = "Inv_Settings"; break;
			default:                        	img_name = "Inv_Invalid";  break; // use the Inv_Invalid icon for undefined object types (see MAINT-3981)
		}

		LLUIImagePtr image = LLUI::getUIImage(img_name);

		font->addEmbeddedChar( wch, image->getImage(), item->getName() );
	}
}

void LLEmbeddedItems::unbindEmbeddedChars( const LLFontGL* font ) const
{
	if( sEntries.empty() )
	{
		return; 
	}

	for (std::set<llwchar>::const_iterator iter1 = mEmbeddedUsedChars.begin(); iter1 != mEmbeddedUsedChars.end(); ++iter1)
	{
		font->removeEmbeddedChar(*iter1);
	}
}

void LLEmbeddedItems::addItems(const std::vector<LLPointer<LLInventoryItem> >& items)
{
	for (std::vector<LLPointer<LLInventoryItem> >::const_iterator iter = items.begin();
		 iter != items.end(); ++iter)
	{
		LLInventoryItem* item = *iter;
		if (item)
		{
			llwchar wc;
			if (!insertEmbeddedItem( item, &wc, false ))
			{
				break;
			}
			mEmbeddedIndexedChars.push_back(wc);
		}
	}
}

void LLEmbeddedItems::getEmbeddedItemList( std::vector<LLPointer<LLInventoryItem> >& items )
{
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(); iter != mEmbeddedUsedChars.end(); ++iter)
	{
		llwchar wc = *iter;
		LLPointer<LLInventoryItem> item = getEmbeddedItem(wc);
		if (item)
		{
			items.push_back(item);
		}
	}
}

void LLEmbeddedItems::markSaved()
{
	for (std::set<llwchar>::iterator iter = mEmbeddedUsedChars.begin(); iter != mEmbeddedUsedChars.end(); ++iter)
	{
		llwchar wc = *iter;
		sEntries[wc].mSaved = TRUE;
	}
}

///////////////////////////////////////////////////////////////////

class LLViewerTextEditor::LLTextCmdInsertEmbeddedItem : public LLTextEditor::LLTextCmd
{
public:
	LLTextCmdInsertEmbeddedItem( S32 pos, LLInventoryItem* item )
		: LLTextCmd(pos, FALSE), 
		  mExtCharValue(0)
	{
		mItem = item;
	}

	virtual BOOL execute( LLTextEditor* editor, S32* delta )
	{
		LLViewerTextEditor* viewer_editor = (LLViewerTextEditor*)editor;
		// Take this opportunity to remove any unused embedded items from this editor
		viewer_editor->mEmbeddedItemList->removeUnusedChars();
		if(viewer_editor->mEmbeddedItemList->insertEmbeddedItem( mItem, &mExtCharValue, true ) )
		{
			LLWString ws;
			ws.assign(1, mExtCharValue);
			*delta = insert(editor, getPosition(), ws );
			return (*delta != 0);
		}
		return FALSE;
	}
	
	virtual S32 undo( LLTextEditor* editor )
	{
		remove(editor, getPosition(), 1);
		return getPosition(); 
	}
	
	virtual S32 redo( LLTextEditor* editor )
	{ 
		LLWString ws;
		ws += mExtCharValue;
		insert(editor, getPosition(), ws );
		return getPosition() + 1;
	}
	virtual BOOL hasExtCharValue( llwchar value ) const
	{
		return (value == mExtCharValue);
	}

private:
	LLPointer<LLInventoryItem> mItem;
	llwchar mExtCharValue;
};

struct LLNotecardCopyInfo
{
	LLNotecardCopyInfo(LLViewerTextEditor *ed, LLInventoryItem *item)
		: mTextEd(ed)
	{
		mItem = item;
	}

	LLViewerTextEditor* mTextEd;
	// need to make this be a copy (not a * here) because it isn't stable.
	// I wish we had passed LLPointers all the way down, but we didn't
	LLPointer<LLInventoryItem> mItem;
};

//----------------------------------------------------------------------------

//
// Member functions
//

LLViewerTextEditor::LLViewerTextEditor(const std::string& name, 
									   const LLRect& rect, 
									   S32 max_length, 
									   const std::string& default_text, 
									   const LLFontGL* font,
									   BOOL allow_embedded_items,
									   bool parse_html)
	: LLTextEditor(name, rect, max_length, default_text, font, allow_embedded_items, parse_html),
	  mDragItemChar(0),
	  mDragItemSaved(FALSE),
	  mInventoryCallback(new LLEmbeddedNotecardOpener)
{
	mEmbeddedItemList = new LLEmbeddedItems(this);
	mInventoryCallback->setEditor(this);
}

LLViewerTextEditor::~LLViewerTextEditor()
{
	delete mEmbeddedItemList;
	// The inventory callback may still be in use by gInventoryCallbackManager...
	// so set its reference to this to null.
	mInventoryCallback->setEditor(NULL); 
}

///////////////////////////////////////////////////////////////////
// virtual
void LLViewerTextEditor::makePristine()
{
	mEmbeddedItemList->markSaved();
	LLTextEditor::makePristine();
}

///////////////////////////////////////////////////////////////////

BOOL LLViewerTextEditor::handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect_screen)
{
	for (child_list_const_iter_t child_iter = getChildList()->begin();
			child_iter != getChildList()->end(); ++child_iter)
	{
		LLView *viewp = *child_iter;
		S32 local_x = x - viewp->getRect().mLeft;
		S32 local_y = y - viewp->getRect().mBottom;
		if( viewp->pointInView(local_x, local_y) 
			&& viewp->getVisible() 
			&& viewp->getEnabled()
			&& viewp->handleToolTip(local_x, local_y, msg, sticky_rect_screen ) )
		{
			return TRUE;
		}
	}

	if( mSegments.empty() )
	{
		return TRUE;
	}

	const LLTextSegment* cur_segment = getSegmentAtLocalPos( x, y );
	if( cur_segment )
	{
		BOOL has_tool_tip = FALSE;
		if( cur_segment->getStyle()->getIsEmbeddedItem() )
		{
			LLWString wtip;
			has_tool_tip = getEmbeddedItemToolTipAtPos(cur_segment->getStart(), wtip);
			msg = wstring_to_utf8str(wtip);
		}
		else
		{
			has_tool_tip = cur_segment->getToolTip( msg );
		}
		if( has_tool_tip )
		{
			// Just use a slop area around the cursor
			// Convert rect local to screen coordinates
			S32 SLOP = 8;
			localPointToScreen( 
				x - SLOP, y - SLOP, 
				&(sticky_rect_screen->mLeft), &(sticky_rect_screen->mBottom) );
			sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
			sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;
		}
	}
	return TRUE;
}

// Singu TODO: This is mostly duplicated from LLTextEditor
BOOL LLViewerTextEditor::handleMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL	handled = FALSE;

	// Let scrollbar have first dibs
	handled = LLView::childrenHandleMouseDown(x, y, mask) != NULL;

	// enable I Agree checkbox if the user scrolled through entire text
	BOOL was_scrolled_to_bottom = (mScrollbar->getDocPos() == mScrollbar->getDocPosMax());
	if (mOnScrollEndCallback && was_scrolled_to_bottom)
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	if( !handled && mTakesNonScrollClicks)
	{
		if (!(mask & MASK_SHIFT))
		{
			deselect();
		}

		BOOL start_select = TRUE;
		if( allowsEmbeddedItems() )
		{
			setCursorAtLocalPos( x, y, FALSE );
			llwchar wc = 0;
			if (mCursorPos < getLength())
			{
				wc = getWChar(mCursorPos);
			}
			LLInventoryItem* item_at_pos = LLEmbeddedItems::getEmbeddedItem(wc);
			if (item_at_pos)
			{
				mDragItem = item_at_pos;
				mDragItemChar = wc;
				mDragItemSaved = LLEmbeddedItems::getEmbeddedItemSaved(wc);
				gFocusMgr.setMouseCapture( this );
				mMouseDownX = x;
				mMouseDownY = y;
				S32 screen_x;
				S32 screen_y;
				localPointToScreen(x, y, &screen_x, &screen_y );
				LLToolDragAndDrop::getInstance()->setDragStart( screen_x, screen_y );

				start_select = FALSE;
			}
			else
			{
				mDragItem = NULL;
			}
		}

		if( start_select )
		{
			// If we're not scrolling (handled by child), then we're selecting
			if (mask & MASK_SHIFT)
			{
				S32 old_cursor_pos = mCursorPos;
				setCursorAtLocalPos( x, y, TRUE );

				if (hasSelection())
				{
					/* Mac-like behavior - extend selection towards the cursor
					if (mCursorPos < mSelectionStart
						&& mCursorPos < mSelectionEnd)
					{
						// ...left of selection
						mSelectionStart = llmax(mSelectionStart, mSelectionEnd);
						mSelectionEnd = mCursorPos;
					}
					else if (mCursorPos > mSelectionStart
						&& mCursorPos > mSelectionEnd)
					{
						// ...right of selection
						mSelectionStart = llmin(mSelectionStart, mSelectionEnd);
						mSelectionEnd = mCursorPos;
					}
					else
					{
						mSelectionEnd = mCursorPos;
					}
					*/
					// Windows behavior
					mSelectionEnd = mCursorPos;
				}
				else
				{
					mSelectionStart = old_cursor_pos;
					mSelectionEnd = mCursorPos;
				}
				// assume we're starting a drag select
				mIsSelecting = TRUE;

			}
			else
			{
				setCursorAtLocalPos( x, y, TRUE );
				startSelection();
			}
			gFocusMgr.setMouseCapture( this );
		}

		handled = TRUE;
	}

	if (hasTabStop())
	{
		setFocus(TRUE);
		handled = TRUE;
	}

	// Delay cursor flashing
	resetKeystrokeTimer();

	return handled;
}


BOOL LLViewerTextEditor::handleHover(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;

	if (!mIsSelecting && mDragItem && hasMouseCapture())
	{
		S32 screen_x;
		S32 screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y );
		if( LLToolDragAndDrop::getInstance()->isOverThreshold( screen_x, screen_y ) )
		{
			LLToolDragAndDrop::getInstance()->beginDrag(
				LLViewerAssetType::lookupDragAndDropType( mDragItem->getType() ),
				mDragItem->getUUID(),
				LLToolDragAndDrop::SOURCE_NOTECARD,
				getSourceID(), mObjectID);

			return LLToolDragAndDrop::getInstance()->handleHover( x, y, mask );
		}
		getWindow()->setCursor(UI_CURSOR_HAND);

		LL_DEBUGS("UserInput") << "hover handled by " << getName() << " (active)" << LL_ENDL;
		handled = TRUE;
	}

	return handled || LLTextEditor::handleHover(x, y, mask);
}


BOOL LLViewerTextEditor::handleMouseUp(S32 x, S32 y, MASK mask)
{
	BOOL handled = FALSE;

	if( hasMouseCapture() )
	{
		if (mDragItem)
		{
			// mouse down was on an item
			S32 dx = x - mMouseDownX;
			S32 dy = y - mMouseDownY;
			if (-2 < dx && dx < 2 && -2 < dy && dy < 2)
			{
				if(mDragItemSaved)
				{
					openEmbeddedItem(mDragItem, mDragItemChar);
				}
				else
				{
					showUnsavedAlertDialog(mDragItem);
				}
			}
		}
		mDragItem = NULL;
	}

	handled = LLTextEditor::handleMouseUp(x,y,mask);

	// Used to enable I Agree checkbox if the user scrolled through entire text
	BOOL was_scrolled_to_bottom = (mScrollbar->getDocPos() == mScrollbar->getDocPosMax());
	if (mOnScrollEndCallback && was_scrolled_to_bottom)
	{
		mOnScrollEndCallback(mOnScrollEndData);
	}

	return handled;
}

BOOL LLViewerTextEditor::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	return childrenHandleRightMouseDown(x, y, mask) != NULL || LLTextEditor::handleRightMouseDown(x, y, mask);
}

BOOL LLViewerTextEditor::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	return childrenHandleMiddleMouseDown(x, y, mask) != NULL || LLTextEditor::handleMiddleMouseDown(x, y, mask);
}

BOOL LLViewerTextEditor::handleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
	return childrenHandleMiddleMouseUp(x, y, mask) != NULL || LLTextEditor::handleMiddleMouseUp(x, y, mask);
}

BOOL LLViewerTextEditor::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (mTakesNonScrollClicks && allowsEmbeddedItems())
	{
		const LLTextSegment* cur_segment = getSegmentAtLocalPos(x, y);
		if (cur_segment && cur_segment->getStyle()->getIsEmbeddedItem())
		{
			if( openEmbeddedItemAtPos(cur_segment->getStart()))
			{
				deselect();
				setFocus(FALSE);
				return TRUE;
			}
		}
	}

	return LLTextEditor::handleDoubleClick(x, y, mask);
}


// Allow calling cards to be dropped onto text fields.  Append the name and
// a carriage return.
// virtual
BOOL LLViewerTextEditor::handleDragAndDrop(S32 x, S32 y, MASK mask,
					  BOOL drop, EDragAndDropType cargo_type, void *cargo_data,
					  EAcceptance *accept,
					  std::string& tooltip_msg)
{
	BOOL handled = FALSE;
	
	LLToolDragAndDrop::ESource source = LLToolDragAndDrop::getInstance()->getSource();
	if (LLToolDragAndDrop::SOURCE_NOTECARD == source)
	{
		// We currently do not handle dragging items from one notecard to another
		// since items in a notecard must be in Inventory to be verified. See DEV-2891.
		return FALSE;
	}
	
	if (mTakesNonScrollClicks)
	{
		if (getEnabled() && acceptsTextInput())
		{
			switch( cargo_type )
			{
			// <edit>
			// This does not even appear to be used maybe
			// Throwing it out so I can embed calling cards
			/*
			case DAD_CALLINGCARD:
				if(acceptsCallingCardNames())
				{
					if (drop)
					{
						LLInventoryItem *item = (LLInventoryItem *)cargo_data;
						std::string name = item->getName();
						appendText(name, true, true);
					}
					*accept = ACCEPT_YES_COPY_SINGLE;
				}
				else
				{
					*accept = ACCEPT_NO;
				}
				break;
			*/
			case DAD_CALLINGCARD:
			// </edit>
			case DAD_TEXTURE:
			case DAD_SOUND:
			case DAD_LANDMARK:
			case DAD_SCRIPT:
			case DAD_CLOTHING:
			case DAD_OBJECT:
			case DAD_NOTECARD:
			case DAD_BODYPART:
			case DAD_ANIMATION:
			case DAD_GESTURE:
				{
					LLInventoryItem *item = (LLInventoryItem *)cargo_data;
					// <edit>
					if((item->getPermissions().getMaskOwner() & PERM_ITEM_UNRESTRICTED) != PERM_ITEM_UNRESTRICTED)
					{
						if(gSavedSettings.getBOOL("ForceNotecardDragCargoPermissive"))
						{
							item = new LLInventoryItem((LLInventoryItem *)cargo_data);
							LLPermissions old = item->getPermissions();
							LLPermissions perm;
							perm.init(old.getCreator(), old.getOwner(), old.getLastOwner(), old.getGroup());
							perm.setMaskBase(PERM_ITEM_UNRESTRICTED);
							perm.setMaskEveryone(PERM_ITEM_UNRESTRICTED);
							perm.setMaskGroup(PERM_ITEM_UNRESTRICTED);
							perm.setMaskNext(PERM_ITEM_UNRESTRICTED);
							perm.setMaskOwner(PERM_ITEM_UNRESTRICTED);
							item->setPermissions(perm);
						}
					}
					// </edit>
					if( item && allowsEmbeddedItems() )
					{
						U32 mask_next = item->getPermissions().getMaskNextOwner();
						// <edit>
						//if((mask_next & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED)
						if(((mask_next & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED) || gSavedSettings.getBOOL("ForceNotecardDragCargoAcceptance"))
						{
							if( drop )
							{
								deselect();
								S32 old_cursor = mCursorPos;
								setCursorAtLocalPos( x, y, TRUE );
								S32 insert_pos = mCursorPos;
								setCursorPos(old_cursor);
								BOOL inserted = insertEmbeddedItem( insert_pos, item );
								if( inserted && (old_cursor > mCursorPos) )
								{
									setCursorPos(mCursorPos + 1);
								}

								updateLineStartList();
							}
							*accept = ACCEPT_YES_COPY_MULTI;
						}
						else
						{
							*accept = ACCEPT_NO;
							if (tooltip_msg.empty())
							{
								tooltip_msg.assign("Only items with unrestricted\n"
													"'next owner' permissions \n"
													"can be attached to notecards.");
							}
						}
					}
					else
					{
						*accept = ACCEPT_NO;
					}
					break;
				}

			default:
				*accept = ACCEPT_NO;
				break;
			}
		}
		else
		{
			// Not enabled
			*accept = ACCEPT_NO;
		}

		handled = TRUE;
		LL_DEBUGS("UserInput") << "dragAndDrop handled by LLViewerTextEditor " << getName() << LL_ENDL;
	}

	return handled;
}

void LLViewerTextEditor::setASCIIEmbeddedText(const std::string& instr)
{
	LLWString wtext;
	const U8* buffer = (U8*)(instr.c_str());
	while (*buffer)
	{
		llwchar wch;
		U8 c = *buffer++;
		if (c >= 0x80)
		{
			S32 index = (S32)(c - 0x80);
			wch = mEmbeddedItemList->getEmbeddedCharFromIndex(index);
		}
		else
		{
			wch = (llwchar)c;
		}
		wtext.push_back(wch);
	}
	setWText(wtext);
}

void LLViewerTextEditor::setEmbeddedText(const std::string& instr)
{
	LLWString wtext = utf8str_to_wstring(instr);
	for (S32 i=0; i<(S32)wtext.size(); i++)
	{
		llwchar wch = wtext[i];
		if( wch >= FIRST_EMBEDDED_CHAR && wch <= LAST_EMBEDDED_CHAR )
		{
			S32 index = wch - FIRST_EMBEDDED_CHAR;
			wtext[i] = mEmbeddedItemList->getEmbeddedCharFromIndex(index);
		}
	}
	setWText(wtext);
}

std::string LLViewerTextEditor::getEmbeddedText()
{
	// New version (Version 2)
	mEmbeddedItemList->copyUsedCharsToIndexed();
	LLWString outtextw;
	for (S32 i=0; i<(S32)getWText().size(); i++)
	{
		llwchar wch = getWChar(i);
		if( wch >= FIRST_EMBEDDED_CHAR && wch <= LAST_EMBEDDED_CHAR )
		{
			S32 index = mEmbeddedItemList->getIndexFromEmbeddedChar(wch);
			wch = FIRST_EMBEDDED_CHAR + index;
		}
		outtextw.push_back(wch);
	}
	std::string outtext = wstring_to_utf8str(outtextw);
	return outtext;
}

std::string LLViewerTextEditor::appendTime(bool prepend_newline, U32 timestamp)
{
	time_t utc_time;
	if (timestamp == 0)
		utc_time = time_corrected();
	else 
		utc_time=timestamp;

	// There's only one internal tm buffer.
	struct tm* timep;

	// Convert to Pacific, based on server's opinion of whether
	// it's daylight savings time there.
	timep = utc_to_pacific_time(utc_time, gPacificDaylightTime);

	std::string format = "";
	if (gSavedSettings.getBOOL("SecondsInChatAndIMs"))
	{
		format = gSavedSettings.getString("LongTimeFormat");
	}
	else
	{
		format = gSavedSettings.getString("ShortTimeFormat");
	}
	std::string text;
	timeStructToFormattedString(timep, format, text);
	text = "[" + text + "]  ";
	appendColoredText(text, false, prepend_newline, LLColor4::grey);

	return text;
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

llwchar LLViewerTextEditor::pasteEmbeddedItem(llwchar ext_char)
{
	if (mEmbeddedItemList->hasEmbeddedItem(ext_char))
	{
		return ext_char; // already exists in my list
	}
	LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem(ext_char);
	if (item)
	{
		// Add item to my list and return new llwchar associated with it
		llwchar new_wc;
		if (mEmbeddedItemList->insertEmbeddedItem( item, &new_wc, true ))
		{
			return new_wc;
		}
	}
	return LL_UNKNOWN_CHAR; // item not found or list full
}

void LLViewerTextEditor::bindEmbeddedChars(const LLFontGL* font) const
{
	mEmbeddedItemList->bindEmbeddedChars( font );
}

void LLViewerTextEditor::unbindEmbeddedChars(const LLFontGL* font) const
{
	mEmbeddedItemList->unbindEmbeddedChars( font );
}

BOOL LLViewerTextEditor::getEmbeddedItemToolTipAtPos(S32 pos, LLWString &msg) const
{
	if (pos < getLength())
	{
		LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem(getWChar(pos));
		if( item )
		{
			msg = utf8str_to_wstring(item->getName());
			msg += '\n';
			msg += utf8str_to_wstring(item->getDescription());
			return TRUE;
		}
	}
	return FALSE;
}


BOOL LLViewerTextEditor::openEmbeddedItemAtPos(S32 pos)
{
	if( pos < getLength())
	{
		llwchar wc = getWChar(pos);
		LLInventoryItem* item = LLEmbeddedItems::getEmbeddedItem( wc );
		if( item )
		{
			BOOL saved = LLEmbeddedItems::getEmbeddedItemSaved( wc );
			if (saved)
			{
				return openEmbeddedItem(item, wc); 
			}
			else
			{
				showUnsavedAlertDialog(item);
			}
		}
	}
	return FALSE;
}


BOOL LLViewerTextEditor::openEmbeddedItem(LLInventoryItem* item, llwchar wc)
{

	switch( item->getType() )
	{
	case LLAssetType::AT_TEXTURE:
	  	openEmbeddedTexture( item, wc );
		return TRUE;

	case LLAssetType::AT_SOUND:
		openEmbeddedSound( item, wc );
		return TRUE;

	case LLAssetType::AT_NOTECARD:
		openEmbeddedNotecard( item, wc );
		return TRUE;

	case LLAssetType::AT_LANDMARK:
		openEmbeddedLandmark( item, wc );
		return TRUE;

	case LLAssetType::AT_CALLINGCARD:
		openEmbeddedCallingcard( item, wc );
		return TRUE;

	case LLAssetType::AT_LSL_TEXT:
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_OBJECT:
	case LLAssetType::AT_BODYPART:
	case LLAssetType::AT_ANIMATION:
	case LLAssetType::AT_GESTURE:
		showCopyToInvDialog( item, wc );
		return TRUE;
	default:
		return FALSE;
	}

}


void LLViewerTextEditor::openEmbeddedTexture( LLInventoryItem* item, llwchar wc )
{
// [RLVa:KB] - Checked: 2009-11-11 (RLVa-1.1.0a) | Modified: RLVa-1.1.0a
	if (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE))
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_TEXTURE);
		return;
	}
// [/RLVa:KB]

	// See if we can bring an existing preview to the front
	// *NOTE:  Just for embedded Texture , we should use getAssetUUID(), 
	// not getUUID(), because LLPreviewTexture pass in AssetUUID into 
	// LLPreview constructor ItemUUID parameter.

	if( !LLPreview::show( item->getAssetUUID() ) )
	{
		// There isn't one, so make a new preview
		if(item)
		{
			S32 left, top;
			gFloaterView->getNewFloaterPosition(&left, &top);
			LLRect rect = gSavedSettings.getRect("PreviewTextureRect");
			rect.translate( left - rect.mLeft, top - rect.mTop );

			LLPreviewTexture* preview = new LLPreviewTexture("preview texture",
				rect,
				item->getName(),
				item->getAssetUUID(),
				TRUE);
			preview->setAuxItem( item );
			preview->setNotecardInfo(mNotecardInventoryID, mObjectID);
		}
	}
}

void LLViewerTextEditor::openEmbeddedSound( LLInventoryItem* item, llwchar wc )
{
	// Play sound locally
	LLVector3d lpos_global = gAgent.getPositionGlobal();
	const F32 SOUND_GAIN = 1.0f;
	if(gAudiop)
	{
		gAudiop->triggerSound(item->getAssetUUID(), gAgentID, SOUND_GAIN, LLAudioEngine::AUDIO_TYPE_UI, lpos_global);
	}
	showCopyToInvDialog( item, wc );
}


void LLViewerTextEditor::openEmbeddedLandmark( LLInventoryItem* item, llwchar wc )
{
	std::string title =
		std::string("  ") + LLLandmarkBridge::prefix()	+ item->getName();
	open_landmark((LLViewerInventoryItem*)item, title, FALSE, item->getUUID(), TRUE);
}

void LLViewerTextEditor::openEmbeddedNotecard( LLInventoryItem* item, llwchar wc )
{
	copyInventory(item, gInventoryCallbacks.registerCB(mInventoryCallback));
}

void LLViewerTextEditor::openEmbeddedCallingcard( LLInventoryItem* item, llwchar wc )
{
	if(item && !item->getCreatorUUID().isNull())
	{
		LLAvatarActions::showProfile(item->getCreatorUUID());
	}
}

void LLViewerTextEditor::showUnsavedAlertDialog( LLInventoryItem* item )
{
	LLSD payload;
	payload["item_id"] = item->getUUID();
	payload["notecard_id"] = mNotecardInventoryID;
	LLNotificationsUtil::add( "ConfirmNotecardSave", LLSD(), payload, LLViewerTextEditor::onNotecardDialog);
}

// static
bool LLViewerTextEditor::onNotecardDialog(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if( option == 0 )
	{
		// itemptr is deleted by LLPreview::save
		LLPointer<LLInventoryItem>* itemptr = new LLPointer<LLInventoryItem>(gInventory.getItem(notification["payload"]["item_id"].asUUID()));
		LLPreview::save( notification["payload"]["notecard_id"].asUUID() , itemptr);
	}
	return false;
}



void LLViewerTextEditor::showCopyToInvDialog( LLInventoryItem* item, llwchar wc )
{
	LLSD payload;
	LLUUID item_id = item->getUUID();
	payload["item_id"] = item_id;
	payload["item_wc"] = LLSD::Integer(wc);
	LLNotificationsUtil::add( "ConfirmItemCopy", LLSD(), payload,
		boost::bind(&LLViewerTextEditor::onCopyToInvDialog, this, _1, _2));
}

bool LLViewerTextEditor::onCopyToInvDialog(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if( 0 == option )
	{
		LLUUID item_id = notification["payload"]["item_id"].asUUID();
		llwchar wc = llwchar(notification["payload"]["item_wc"].asInteger());
		LLInventoryItem* itemp = LLEmbeddedItems::getEmbeddedItem(wc);
		if (itemp)
			copyInventory(itemp);
	}
	return false;
}



// Returns change in number of characters in mWText
S32 LLViewerTextEditor::insertEmbeddedItem( S32 pos, LLInventoryItem* item )
{
	return execute( new LLTextCmdInsertEmbeddedItem( pos, item ) );
}

bool LLViewerTextEditor::importStream(std::istream& str)
{
	LLNotecard nc(LLNotecard::MAX_SIZE);
	bool success = nc.importStream(str);
	if (success)
	{
		mEmbeddedItemList->clear();
		const std::vector<LLPointer<LLInventoryItem> >& items = nc.getItems();
		mEmbeddedItemList->addItems(items);
		// Actually set the text
		if (allowsEmbeddedItems())
		{
			if (nc.getVersion() == 1)
				setASCIIEmbeddedText( nc.getText() );
			else
				setEmbeddedText( nc.getText() );
		}
		else
		{
			setText( nc.getText() );
		}
	}
	return success;
}

void LLViewerTextEditor::copyInventory(const LLInventoryItem* item, U32 callback_id)
{
	copy_inventory_from_notecard(LLUUID::null,  // Don't specify a destination -- let the sim do that
								 mObjectID,
								 mNotecardInventoryID,
								 item,
								 callback_id);
}

bool LLViewerTextEditor::hasEmbeddedInventory()
{
	return ! mEmbeddedItemList->empty();
}

// <edit>
std::vector<LLPointer<LLInventoryItem> > LLViewerTextEditor::getEmbeddedItems()
{
	std::vector<LLPointer<LLInventoryItem> > items;
	mEmbeddedItemList->getEmbeddedItemList(items);
	return items;
}
// </edit>
////////////////////////////////////////////////////////////////////////////

BOOL LLViewerTextEditor::importBuffer( const char* buffer, S32 length )
{
	LLMemoryStream str((U8*)buffer, length);
	return importStream(str);
}

BOOL LLViewerTextEditor::exportBuffer( std::string& buffer )
{
	LLNotecard nc(LLNotecard::MAX_SIZE);

	// Get the embedded text and update the item list to just be the used items
	nc.setText(getEmbeddedText());

	// Now get the used items and copy the list to the notecard
	std::vector<LLPointer<LLInventoryItem> > embedded_items;
	mEmbeddedItemList->getEmbeddedItemList(embedded_items);	
	nc.setItems(embedded_items);
	
	std::stringstream out_stream;
	nc.exportStream(out_stream);
	
	buffer = out_stream.str();
	
	return TRUE;
}

// virtual
LLXMLNodePtr LLViewerTextEditor::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLTextEditor::getXML();

	node->setName(LL_TEXT_EDITOR_TAG);

	return node;
}

LLView* LLViewerTextEditor::fromXML(LLXMLNodePtr node, LLView *parent, LLUICtrlFactory *factory)
{
	LLRect rect;
	createRect(node, rect, parent, LLRect());

	U32 max_text_length = 255;
	node->getAttributeU32("max_length", max_text_length);

	BOOL allow_embedded_items = FALSE;
	node->getAttributeBOOL("embedded_items", allow_embedded_items);

	LLFontGL* font = LLView::selectFont(node);

	std::string text = node->getTextContents().substr(0, max_text_length - 1);

	LLViewerTextEditor* text_editor = new LLViewerTextEditor("text_editor", 
								rect,
								max_text_length,
								LLStringUtil::null,
								font,
								allow_embedded_items);

	BOOL ignore_tabs = text_editor->tabsToNextField();
	node->getAttributeBOOL("ignore_tab", ignore_tabs);
	text_editor->setTabsToNextField(ignore_tabs);

	text_editor->setTextEditorParameters(node);

	BOOL hide_scrollbar = FALSE;
	node->getAttributeBOOL("hide_scrollbar",hide_scrollbar);
	text_editor->setHideScrollbarForShortDocs(hide_scrollbar);

	BOOL hide_border = !text_editor->isBorderVisible();
	node->getAttributeBOOL("hide_border", hide_border);
	text_editor->setBorderVisible(!hide_border);

	BOOL parse_html = true;
	node->getAttributeBOOL("allow_html", parse_html);
	text_editor->setParseHTML(parse_html);

	BOOL commit_on_focus_lost = FALSE;
	node->getAttributeBOOL("commit_on_focus_lost",commit_on_focus_lost);
	text_editor->setCommitOnFocusLost(commit_on_focus_lost);
	
	text_editor->initFromXML(node, parent);

	// add text after all parameters have been set
	text_editor->appendText(text, FALSE, FALSE);

	return text_editor;
}
