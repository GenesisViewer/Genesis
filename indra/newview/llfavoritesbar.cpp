// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llfavoritesbar.cpp
 * @brief LLFavoritesBarCtrl class implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llfavoritesbar.h"

//#include "llfloaterreg.h"
#include "llfocusmgr.h"
#include "llinventory.h"
#include "lllandmarkactions.h"
//#include "lltoolbarview.h"
#include "lltrans.h"
#include "llmenugl.h"
//#include "lltooltip.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavatarnamecache.h"
#include "llclipboard.h"
#include "llinventorybridge.h"
#include "llinventoryclipboard.h"
#include "llinventoryfunctions.h"
//#include "llfloatersidepanelcontainer.h"
#include "llfloaterworldmap.h"
#include "lllandmarkactions.h"
//#include "lllogininstance.h"
#include "llnotificationsutil.h"
//#include "lltoggleablemenu.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "hippogridmanager.h" //"llviewernetwork.h"
#include "lltooldraganddrop.h"
#include "llsdserialize.h"

void open_landmark(LLViewerInventoryItem* inv_item, const std::string& title, BOOL show_keep_discard, const LLUUID& source_id, BOOL take_focus);

static LLRegisterWidget<LLFavoritesBarCtrl> r("favorites_bar");

const S32 DROP_DOWN_MENU_WIDTH = 250;
const S32 DROP_DOWN_MENU_TOP_PAD = 13;

/**
 * Helper for LLFavoriteLandmarkButton and LLFavoriteLandmarkMenuItem.
 * Performing requests for SLURL for given Landmark ID
 */
class LLLandmarkInfoGetter
{
public:
	LLLandmarkInfoGetter()
	:	mLandmarkID(LLUUID::null),
		mName("(Loading...)"),
		mPosX(0),
		mPosY(0),
		mPosZ(0),
		mLoaded(false) 
	{
		mHandle.bind(this);
	}

	void setLandmarkID(const LLUUID& id) { mLandmarkID = id; }
	const LLUUID& getLandmarkId() const { return mLandmarkID; }

	const std::string& getName()
	{
		if(!mLoaded)
			requestNameAndPos();

		return mName;
	}

	S32 getPosX()
	{
		if (!mLoaded)
			requestNameAndPos();
		return mPosX;
	}

	S32 getPosY()
	{
		if (!mLoaded)
			requestNameAndPos();
		return mPosY;
	}

	S32 getPosZ()
	{
		if (!mLoaded)
			requestNameAndPos();
		return mPosZ;
	}

private:
	/**
	 * Requests landmark data from server.
	 */
	void requestNameAndPos()
	{
		if (mLandmarkID.isNull())
			return;

		LLVector3d g_pos;
		if(LLLandmarkActions::getLandmarkGlobalPos(mLandmarkID, g_pos))
		{
			LLLandmarkActions::getRegionNameAndCoordsFromPosGlobal(g_pos,
				boost::bind(&LLLandmarkInfoGetter::landmarkNameCallback, static_cast<LLHandle<LLLandmarkInfoGetter> >(mHandle), _1, _2, _3, _4));
		}
	}

	static void landmarkNameCallback(LLHandle<LLLandmarkInfoGetter> handle, const std::string& name, S32 x, S32 y, S32 z)
	{
		LLLandmarkInfoGetter* getter = handle.get();
		if (getter)
		{
			getter->mPosX = x;
			getter->mPosY = y;
			getter->mPosZ = z;
			getter->mName = name;
			getter->mLoaded = true;
		}
	}

	LLUUID mLandmarkID;
	std::string mName;
	S32 mPosX;
	S32 mPosY;
	S32 mPosZ;
	bool mLoaded;
	LLRootHandle<LLLandmarkInfoGetter> mHandle;
};

/**
 * This class is needed to override LLButton default handleToolTip function and
 * show SLURL as button tooltip.
 * *NOTE: dzaporozhan: This is a workaround. We could set tooltips for buttons
 * in createButtons function but landmark data is not available when Favorites Bar is
 * created. Thats why we are requesting landmark data after 
 */
class LLFavoriteLandmarkButton : public LLButton
{
public:

	BOOL handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect) override
	{
		std::string region_name = mLandmarkInfoGetter.getName();
		
		if (!region_name.empty())
		{
			std::string extra_message = llformat("%s (%d, %d, %d)", region_name.c_str(), 
				mLandmarkInfoGetter.getPosX(), mLandmarkInfoGetter.getPosY(), mLandmarkInfoGetter.getPosZ());

			msg = llformat("%s\n%s", getLabelSelected().c_str(), extra_message.c_str());
			*sticky_rect = calcScreenRect();
		}
		return TRUE;
	}

	/*virtual*/ BOOL	handleHover(S32 x, S32 y, MASK mask) override
    {
		LLFavoritesBarCtrl* fb = dynamic_cast<LLFavoritesBarCtrl*>(getParent());

		if (fb)
		{
			fb->handleHover(x, y, mask);
		}

		return LLButton::handleHover(x, y, mask);
	}
	
	void setLandmarkID(const LLUUID& id){ mLandmarkInfoGetter.setLandmarkID(id); }
	const LLUUID& getLandmarkId() const { return mLandmarkInfoGetter.getLandmarkId(); }

	void onMouseEnter(S32 x, S32 y, MASK mask) override
	{
		if (LLToolDragAndDrop::getInstance()->hasMouseCapture())
		{
			LLUICtrl::onMouseEnter(x, y, mask);
		}
		else
		{
			LLButton::onMouseEnter(x, y, mask);
		}
	}

//protected:
	LLFavoriteLandmarkButton(const LLButton::Params& p) : LLButton(p) {}
	friend class LLUICtrlFactory;

private:
	LLLandmarkInfoGetter mLandmarkInfoGetter;
};

/**
 * This class is needed to override LLMenuItemCallGL default handleToolTip function and
 * show SLURL as button tooltip.
 * *NOTE: dzaporozhan: This is a workaround. We could set tooltips for buttons
 * in showDropDownMenu function but landmark data is not available when Favorites Bar is
 * created. Thats why we are requesting landmark data after 
 */
class LLFavoriteLandmarkMenuItem : public LLMenuItemCallGL
{
public:
	BOOL handleToolTip(S32 x, S32 y, std::string& msg, LLRect* sticky_rect) override
	{
		std::string region_name = mLandmarkInfoGetter.getName();
		if (!region_name.empty())
		{
			msg = llformat("%s\n%s (%d, %d)", getLabel().c_str(), region_name.c_str(), mLandmarkInfoGetter.getPosX(), mLandmarkInfoGetter.getPosY());
			*sticky_rect = calcScreenRect();
		}
		return TRUE;
	}
	
	void setLandmarkID(const LLUUID& id){ mLandmarkInfoGetter.setLandmarkID(id); }

    BOOL handleMouseDown(S32 x, S32 y, MASK mask) override
	{
		if (mMouseDownSignal)
			(*mMouseDownSignal)(this, x, y, mask);
		return LLMenuItemCallGL::handleMouseDown(x, y, mask);
	}

    BOOL handleMouseUp(S32 x, S32 y, MASK mask) override
	{
		if (mMouseUpSignal)
			(*mMouseUpSignal)(this, x, y, mask);
		return LLMenuItemCallGL::handleMouseUp(x, y, mask);
	}

    BOOL handleHover(S32 x, S32 y, MASK mask) override
	{
		if (fb)
		{
			fb->handleHover(x, y, mask);
		}

		return TRUE;
	}

	void initFavoritesBarPointer(LLFavoritesBarCtrl* fb) { this->fb = fb; }

//protected:

	LLFavoriteLandmarkMenuItem(const LLMenuItemCallGL::Params& p) : LLMenuItemCallGL(p.name(), p.label(), nullptr), fb(nullptr) {}
	friend class LLUICtrlFactory;

private:
	LLLandmarkInfoGetter mLandmarkInfoGetter;
	LLFavoritesBarCtrl* fb;
};

/**
 * This class was introduced just for fixing the following issue:
 * EXT-836 Nav bar: Favorites overflow menu passes left-mouse click through.
 * We must explicitly handle drag and drop event by returning TRUE
 * because otherwise LLToolDragAndDrop will initiate drag and drop operation
 * with the world.
 */
class LLFavoriteLandmarkToggleableMenu : public LLMenuGL
{
public:
    BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg) override
	{
		*accept = ACCEPT_NO;
		return TRUE;
	}

//protected:
	LLFavoriteLandmarkToggleableMenu():
		LLMenuGL("favorites menu")
	{
	}

	friend class LLUICtrlFactory;
};

/**
 * This class is needed to update an item being copied to the favorites folder
 * with a sort field value (required to save favorites bar's tabs order).
 * See method handleNewFavoriteDragAndDrop for more details on how this class is used.
 */
class LLItemCopiedCallback : public LLInventoryCallback
{
public:
	LLItemCopiedCallback(S32 sortField): mSortField(sortField) {}

    void fire(const LLUUID& inv_item) override
	{
		LLViewerInventoryItem* item = gInventory.getItem(inv_item);

		if (item)
		{
			LLFavoritesOrderStorage::instance().setSortIndex(item, mSortField);

			item->setComplete(TRUE);
			item->updateServer(FALSE);

			gInventory.updateItem(item);
			gInventory.notifyObservers();
			LLFavoritesOrderStorage::instance().saveOrder();
		}

		LLView::getWindow()->setCursor(UI_CURSOR_ARROW);
	}

private:
	S32 mSortField;
};

// updateButtons's helper
struct LLFavoritesSort
{
	// Sorting by creation date and name
	// TODO - made it customizible using gSavedSettings
	bool operator()(const LLViewerInventoryItem* const& a, const LLViewerInventoryItem* const& b) const
	{
		S32 sortField1 = LLFavoritesOrderStorage::instance().getSortIndex(a->getUUID());
		S32 sortField2 = LLFavoritesOrderStorage::instance().getSortIndex(b->getUUID());

		if (!(sortField1 < 0 && sortField2 < 0))
		{
			return sortField2 > sortField1;
		}

		time_t first_create = a->getCreationDate();
		time_t second_create = b->getCreationDate();
		if (first_create == second_create)
		{
			return (LLStringUtil::compareDict(a->getName(), b->getName()) < 0);
		}
		else
		{
			return (first_create > second_create);
		}
	}
};

LLFavoritesBarCtrl::Params::Params()
: image_drag_indication("image_drag_indication"),
  more_button("more_button"),
  label("label")
{
}

LLFavoritesBarCtrl::LLFavoritesBarCtrl(const LLFavoritesBarCtrl::Params& p)
:	LLUICtrl(p),
	mOverflowMenuHandle(),
	mContextMenuHandle(),
	mFont(p.font.isProvided() ? p.font() : LLFontGL::getFontSansSerifSmall()),
	mUpdateDropDownItems(true),
	mRestoreOverflowMenu(false),
	mImageDragIndication(p.image_drag_indication),
	mShowDragMarker(FALSE),
	mLandingTab(NULL)
  , mLastTab(NULL)
,	mTabsHighlightEnabled(TRUE)
{
	// Register callback for menus with current registrar (will be parent panel's registrar)
	LLUICtrl::CommitCallbackRegistry::currentRegistrar().add("Favorites.DoToSelected",
		boost::bind(&LLFavoritesBarCtrl::doToSelected, this, _2));

	// Add this if we need to selectively enable items
	LLUICtrl::EnableCallbackRegistry::currentRegistrar().add("Favorites.EnableSelected",
		boost::bind(&LLFavoritesBarCtrl::enableSelected, this, _2));
	
	gInventory.addObserver(this);

	//make chevron button                                                                                                                               
	mMoreTextBox = new LLTextBox(">>", "\u0032", 50);
	mMoreTextBox->setFollows(FOLLOWS_LEFT | FOLLOWS_BOTTOM);
	mMoreTextBox->setToolTip(LLStringExplicit("Show more of My Favorites"));
	mMoreTextBox->setTabStop(false);
	mMoreTextBox->setClickedCallback(boost::bind(&LLFavoritesBarCtrl::showDropDownMenu, this));
	addChild(mMoreTextBox);

	mDropDownItemsCount = 0;

	mBarLabel = new LLTextBox(p.label.name, p.label.label);
	addChild(mBarLabel);
}

LLFavoritesBarCtrl::~LLFavoritesBarCtrl()
{
	gInventory.removeObserver(this);

	if (mOverflowMenuHandle.get()) mOverflowMenuHandle.get()->die();
	if (mContextMenuHandle.get()) mContextMenuHandle.get()->die();
}

BOOL LLFavoritesBarCtrl::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg)
{
	*accept = ACCEPT_NO;

	LLToolDragAndDrop::ESource source = LLToolDragAndDrop::getInstance()->getSource();
	if (LLToolDragAndDrop::SOURCE_AGENT != source && LLToolDragAndDrop::SOURCE_LIBRARY != source) return FALSE;

	switch (cargo_type)
	{

	case DAD_LANDMARK:
		{
			/*
			 * add a callback to the end drag event.
			 * the callback will disconnet itself immediately after execution
			 * this is done because LLToolDragAndDrop is a common tool so it shouldn't
			 * be overloaded with redundant callbacks.
			 */
			if (!mEndDragConnection.connected())
			{
				mEndDragConnection = LLToolDragAndDrop::getInstance()->setEndDragCallback(boost::bind(&LLFavoritesBarCtrl::onEndDrag, this));
			}

			// Copy the item into the favorites folder (if it's not already there).
			LLInventoryItem *item = (LLInventoryItem *)cargo_data;

			if (LLFavoriteLandmarkButton* dest = dynamic_cast<LLFavoriteLandmarkButton*>(findChildByLocalCoords(x, y)))
			{
				setLandingTab(dest);
			}
			else if (mLastTab && (x >= mLastTab->getRect().mRight))
			{
				/*
				 * the condition dest == NULL can be satisfied not only in the case
				 * of dragging to the right from the last tab of the favbar. there is a
				 * small gap between each tab. if the user drags something exactly there
				 * then mLandingTab will be set to NULL and the dragged item will be pushed
				 * to the end of the favorites bar. this is incorrect behavior. that's why
				 * we need an additional check which excludes the case described previously
				 * making sure that the mouse pointer is beyond the last tab.
				 */
				setLandingTab(NULL);
			}

			// check if we are dragging an existing item from the favorites bar
			if (item && mDragItemId == item->getUUID())
			{
				*accept = ACCEPT_YES_SINGLE;

				showDragMarker(TRUE);

				if (drop)
				{
					handleExistingFavoriteDragAndDrop(x, y);
				}
			}
			else
			{
				const LLUUID favorites_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
				if (item && item->getParentUUID() == favorites_id)
				{
					LL_WARNS("FavoritesBar") << "Attemt to copy a favorite item into the same folder." << LL_ENDL;
					break;
				}

				*accept = ACCEPT_YES_COPY_MULTI;

				showDragMarker(TRUE);

				if (drop)
				{
					if (mItems.empty())
					{
						setLandingTab(NULL);
					}
					handleNewFavoriteDragAndDrop(item, favorites_id, x, y);
				}
			}
		}
		break;
	default:
		break;
	}

	return TRUE;
}

void LLFavoritesBarCtrl::handleExistingFavoriteDragAndDrop(S32 x, S32 y)
{
	// Identify the button hovered and the side to drop
	LLFavoriteLandmarkButton* dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLandingTab);
	bool insert_before = true;	
	if (!dest)
	{
		insert_before = false;
		dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLastTab);
	}

	// There is no need to handle if an item was dragged onto itself
	if (dest && dest->getLandmarkId() == mDragItemId)
	{
		return;
	}

	// Insert the dragged item in the right place
	if (dest)
	{
		LLInventoryModel::updateItemsOrder(mItems, mDragItemId, dest->getLandmarkId(), insert_before);
	}
	else
	{
		// This can happen when the item list is empty
		mItems.push_back(gInventory.getItem(mDragItemId));
	}

	LLFavoritesOrderStorage::instance().saveItemsOrder(mItems);

	LLMenuGL* menu = (LLMenuGL*) mOverflowMenuHandle.get();

	if (menu && menu->getVisible())
	{
		menu->setVisible(FALSE);
		showDropDownMenu();
	}
}

void LLFavoritesBarCtrl::handleNewFavoriteDragAndDrop(LLInventoryItem *item, const LLUUID& favorites_id, S32 x, S32 y)
{
	if (!item) return;
	// Identify the button hovered and the side to drop
	LLFavoriteLandmarkButton* dest = NULL;
	bool insert_before = true;
	if (!mItems.empty())
	{
		// [MAINT-2386] When multiple landmarks are selected and dragged onto an empty favorites bar,
		//		the viewer would crash when casting mLastTab below, as mLastTab is still null when the
		//		second landmark is being added.
		//		To ensure mLastTab is valid, we need to call updateButtons() at the end of this function
		dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLandingTab);
		if (!dest)
		{
			insert_before = false;
			dest = dynamic_cast<LLFavoriteLandmarkButton*>(mLastTab);
		}
	}
	
	// There is no need to handle if an item was dragged onto itself
	if (dest && dest->getLandmarkId() == mDragItemId)
	{
		return;
	}
	
	LLPointer<LLViewerInventoryItem> viewer_item = new LLViewerInventoryItem(item);

	// Insert the dragged item in the right place
	if (dest)
	{
		insertItem(mItems, dest->getLandmarkId(), viewer_item, insert_before);
	}
	else
	{
		// This can happen when the item list is empty
		mItems.push_back(viewer_item);
	}

	int sortField = 0;
	LLPointer<LLItemCopiedCallback> cb;

	// current order is saved by setting incremental values (1, 2, 3, ...) for the sort field
	for (LLInventoryModel::item_array_t::iterator i = mItems.begin(); i != mItems.end(); ++i)
	{
		LLViewerInventoryItem* currItem = *i;

		if (currItem->getUUID() == item->getUUID())
		{
			cb = new LLItemCopiedCallback(++sortField);
		}
		else
		{
			LLFavoritesOrderStorage::instance().setSortIndex(currItem, ++sortField);

			currItem->setComplete(TRUE);
			currItem->updateServer(FALSE);

			gInventory.updateItem(currItem);
		}
	}

	LLToolDragAndDrop* tool_dad = LLToolDragAndDrop::getInstance();
	if (tool_dad->getSource() == LLToolDragAndDrop::SOURCE_NOTECARD)
	{
		viewer_item->setType(LLAssetType::AT_LANDMARK);
		copy_inventory_from_notecard(favorites_id,
									 tool_dad->getObjectID(),
									 tool_dad->getSourceID(),
									 viewer_item.get(),
									 gInventoryCallbacks.registerCB(cb));
	}
	else
	{
		copy_inventory_item(
				gAgent.getID(),
				item->getPermissions().getOwner(),
				item->getUUID(),
				favorites_id,
				std::string(),
				cb);
	}

	// [MAINT-2386] Ensure the favorite button has been created and is valid.
	//		This also ensures that mLastTab will be valid when dropping multiple
	//		landmarks to an empty favorites bar.
	updateButtons();
	
	LL_INFOS("FavoritesBar") << "Copied inventory item #" << item->getUUID() << " to favorites." << LL_ENDL;
}

//virtual
void LLFavoritesBarCtrl::changed(U32 mask)
{
	if (mFavoriteFolderId.isNull())
	{
		mFavoriteFolderId = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
		
		if (mFavoriteFolderId.notNull())
		{
			gInventory.fetchDescendentsOf(mFavoriteFolderId);
		}
	}	
	else
	{
		LLInventoryModel::item_array_t items;
		LLInventoryModel::cat_array_t cats;
		LLIsType is_type(LLAssetType::AT_LANDMARK);
		gInventory.collectDescendentsIf(mFavoriteFolderId, cats, items, LLInventoryModel::EXCLUDE_TRASH, is_type);

		for (LLInventoryModel::item_array_t::iterator i = items.begin(); i != items.end(); ++i)
		{
			LLFavoritesOrderStorage::instance().getSLURL((*i)->getAssetUUID());
		}

		updateButtons();
	}
}

//virtual
void LLFavoritesBarCtrl::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLUICtrl::reshape(width, height, called_from_parent);
	updateButtons();
}

void LLFavoritesBarCtrl::draw()
{
	LLUICtrl::draw();

	if (mShowDragMarker)
	{
		S32 w = mImageDragIndication->getWidth();
		S32 h = mImageDragIndication->getHeight();

		if (mLandingTab)
		{
			// mouse pointer hovers over an existing tab
			LLRect rect = mLandingTab->getRect();
			mImageDragIndication->draw(rect.mLeft, rect.getHeight(), w, h);
		}
		else if (mLastTab)
		{
			// mouse pointer hovers over the favbar empty space (right to the last tab)
			LLRect rect = mLastTab->getRect();
			mImageDragIndication->draw(rect.mRight, rect.getHeight(), w, h);
		}
		// Once drawn, mark this false so we won't draw it again (unless we hit the favorite bar again)
		mShowDragMarker = FALSE;
	}
}

const LLButton::Params& LLFavoritesBarCtrl::getButtonParams()
{
	static LLButton::Params button_params;
	static bool params_initialized = false;

	if (!params_initialized)
	{
		button_params.image_overlay(LLUI::getUIImage("Favorite_Link_Over"))
			.hover_glow_amount(0.15f)
			.use_ellipses(true).tab_stop(false)
			.name("favorites_bar_btn")
			.follows.flags(FOLLOWS_LEFT|FOLLOWS_BOTTOM);
		params_initialized = true;
	}

	return button_params;
}

void LLFavoritesBarCtrl::updateButtons()
{
	mItems.clear();

	if (!collectFavoriteItems(mItems))
	{
		return;
	}

	const LLButton::Params& button_params = getButtonParams();

	if(mItems.empty())
	{
		mBarLabel->setVisible(TRUE);
	}
	else
	{
		mBarLabel->setVisible(FALSE);
	}
	const child_list_t* childs = getChildList();
	child_list_const_iter_t child_it = childs->begin();
	size_t first_changed_item_index = 0;
	int rightest_point = getRect().mRight - mMoreTextBox->getRect().getWidth();
	//lets find first changed button
	while (child_it != childs->end() && first_changed_item_index < mItems.size())
	{
		LLFavoriteLandmarkButton* button = dynamic_cast<LLFavoriteLandmarkButton*> (*child_it);
		if (button)
		{
			const LLViewerInventoryItem *item = mItems[first_changed_item_index].get();
			if (item)
			{
			    // an child's order  and mItems  should be same
				if (button->getLandmarkId() != item->getUUID() // sort order has been changed
					|| button->getLabelSelected() != item->getName() // favorite's name has been changed
					|| button->getRect().mRight < rightest_point) // favbar's width has been changed
				{
					break;
				}
			}
			first_changed_item_index++;
		}
		child_it++;
	}
	// now first_changed_item_index should contains a number of button that need to change

	if (first_changed_item_index <= mItems.size())
	{
		// Rebuild the buttons only
		// child_list_t is a linked list, so safe to erase from the middle if we pre-increment the iterator

		while (child_it != childs->end())
		{
			//lets remove other landmarks button and rebuild it
			child_list_const_iter_t cur_it = child_it++;
			LLFavoriteLandmarkButton* button =
					dynamic_cast<LLFavoriteLandmarkButton*> (*cur_it);
			if (button)
			{
				removeChild(button);
				delete button;
			}
		}
		// we have to remove ChevronButton to make sure that the last item will be LandmarkButton to get the right aligning
		// keep in mind that we are cutting all buttons in space between the last visible child of favbar and ChevronButton
		if (mMoreTextBox->getParent() == this)
		{
			removeChild(mMoreTextBox);
		}
		int last_right_edge = 0;
		//calculate new buttons offset
		if (getChildList()->size() > 0)
		{
			//find last visible child to get the rightest button offset
			child_list_const_reverse_iter_t last_visible_it = std::find_if(childs->rbegin(), childs->rend(), 
					std::mem_fn(&LLView::getVisible));
			if(last_visible_it != childs->rend())
			{
				last_right_edge = (*last_visible_it)->getRect().mRight;
			}
		}
		//last_right_edge is saving coordinates
		LLButton* last_new_button = NULL;
		size_t j = first_changed_item_index;
		for (; j < mItems.size(); j++)
		{
			last_new_button = createButton(mItems[j], button_params, last_right_edge);
			if (!last_new_button)
			{
				break;
			}
			sendChildToBack(last_new_button);
			last_right_edge = last_new_button->getRect().mRight;

			mLastTab = last_new_button;
		}
		mFirstDropDownItem = j;
		// Chevron button
		if (mFirstDropDownItem < mItems.size())
		{
			// if updateButton had been called it means:
			//or there are some new favorites, or width had been changed
			// so if we need to display chevron button,  we must update dropdown items too. 
			mUpdateDropDownItems = true;
			S32 buttonHGap = button_params.rect.left; // default value
			LLRect rect;
			// Chevron button should stay right aligned
			rect.setOriginAndSize(getRect().mRight - mMoreTextBox->getRect().getWidth() - buttonHGap, 0,
					mMoreTextBox->getRect().getWidth(),
					mMoreTextBox->getRect().getHeight());

			addChild(mMoreTextBox);
			mMoreTextBox->setRect(rect);
			mMoreTextBox->setVisible(TRUE);
		}
		// Update overflow menu
		LLMenuGL* overflow_menu = static_cast <LLMenuGL*> (mOverflowMenuHandle.get());
		if (overflow_menu && overflow_menu->getVisible() && (overflow_menu->getItemCount() != mDropDownItemsCount))
		{
			overflow_menu->setVisible(FALSE);
			if (mUpdateDropDownItems)
			{
				showDropDownMenu();
			}
		}
	}
	else
	{
		mUpdateDropDownItems = false;
	}

}

LLButton* LLFavoritesBarCtrl::createButton(const LLPointer<LLViewerInventoryItem> item, const LLButton::Params& button_params, S32 x_offset)
{
	S32 def_button_width = button_params.rect.width;
	S32 button_x_delta = button_params.rect.left; // default value
	S32 curr_x = x_offset;

	/**
	 * WORKAROUND:
	 * There are some problem with displaying of fonts in buttons. 
	 * Empty space or ellipsis might be displayed instead of last symbols, even though the width of the button is enough.
	 * The problem disappears if we pad the button with 20 pixels.
	 */
	int required_width = mFont->getWidth(item->getName()) + 20;
	int width = required_width > def_button_width? def_button_width : required_width;
	LLFavoriteLandmarkButton* fav_btn = NULL;

	// do we have a place for next button + double buttonHGap + mMoreTextBox ?
	if(curr_x + width + 2*button_x_delta +  mMoreTextBox->getRect().getWidth() > getRect().mRight )
	{
		return NULL;
	}
	LLButton::Params fav_btn_params(button_params);
	fav_btn = new LLFavoriteLandmarkButton(fav_btn_params);
	if (NULL == fav_btn)
	{
		LL_WARNS("FavoritesBar") << "Unable to create LLFavoriteLandmarkButton widget: " << item->getName() << LL_ENDL;
		return NULL;
	}
	
	addChild(fav_btn);

	LLRect butt_rect (fav_btn->getRect());
	fav_btn->setLandmarkID(item->getUUID());
	butt_rect.setOriginAndSize(curr_x + button_x_delta, fav_btn->getRect().mBottom, width, fav_btn->getRect().getHeight());
	
	fav_btn->setRect(butt_rect);
	// change only left and save bottom
	fav_btn->setFont(mFont);
	fav_btn->setLabel(item->getName());
	fav_btn->setToolTip(item->getName());
	fav_btn->setCommitCallback(boost::bind(&LLFavoritesBarCtrl::onButtonClick, this, item->getUUID()));
	fav_btn->setRightMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonRightClick, this, item->getUUID(), _1, _2, _3,_4 ));

	fav_btn->LLUICtrl::setMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseDown, this, item->getUUID(), _1, _2, _3, _4));
	fav_btn->LLUICtrl::setMouseUpCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseUp, this, item->getUUID(), _1, _2, _3, _4));

	return fav_btn;
}


BOOL LLFavoritesBarCtrl::postBuild()
{
	// make the popup menu available
	LLMenuGL* menu = LLUICtrlFactory::getInstance()->buildMenu("menu_favorites.xml", gMenuHolder);
	if (!menu)
	{
		return FALSE; //menu = LLUICtrlFactory::getDefaultWidget<LLMenuGL>("inventory_menu");
	}
	menu->setBackgroundColor(gColors.getColor("MenuPopupBgColor"));
	mContextMenuHandle = menu->getHandle();

	return TRUE;
}

BOOL collectFavoriteItems(LLInventoryModel::item_array_t& items)
{

	auto mFavoriteFolderId = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
	if (mFavoriteFolderId.isNull())
		return FALSE;
	

	LLInventoryModel::cat_array_t cats;

	LLIsType is_type(LLAssetType::AT_LANDMARK);
	gInventory.collectDescendentsIf(mFavoriteFolderId, cats, items, LLInventoryModel::EXCLUDE_TRASH, is_type);

	std::sort(items.begin(), items.end(), LLFavoritesSort());

	BOOL needToSaveItemsOrder(const LLInventoryModel::item_array_t& items);
	//if (needToSaveItemsOrder(items))
	{
		S32 sortField = 0;
		for (LLInventoryModel::item_array_t::iterator i = items.begin(); i != items.end(); ++i)
		{
			LLFavoritesOrderStorage::instance().setSortIndex((*i), ++sortField);
		}
	}

	return TRUE;
}
BOOL LLFavoritesBarCtrl::collectFavoriteItems(LLInventoryModel::item_array_t& items) { return ::collectFavoriteItems(items); }

void LLFavoritesBarCtrl::showDropDownMenu()
{
	if (mOverflowMenuHandle.isDead())
	{
		createOverflowMenu();
	}

	LLMenuGL* menu = (LLMenuGL*)mOverflowMenuHandle.get();
	if (menu)
	{

		if (mUpdateDropDownItems)
		{
			updateMenuItems(menu);
		}

		menu->buildDrawLabels();
		menu->updateParent(LLMenuGL::sMenuContainer);
		//menu->setButtonRect(mMoreTextBox->getRect(), this);
		positionAndShowMenu(menu);
		mDropDownItemsCount = menu->getItemCount();
	}
}

void LLFavoritesBarCtrl::createOverflowMenu()
{
	LLMenuGL* menu = new LLFavoriteLandmarkToggleableMenu();
	menu->setCanTearOff(false);
	menu->setVisible(false);
	menu->setScrollable(true);
	//menu_p.max_scrollable_items = 10;
	//menu_p.preferred_width = DROP_DOWN_MENU_WIDTH;
	mOverflowMenuHandle = menu->getHandle();
}

void LLFavoritesBarCtrl::updateMenuItems(LLMenuGL* menu)
{
	menu->empty();

	U32 widest_item = 0;

	for (size_t i = mFirstDropDownItem; i < mItems.size(); i++)
	{
		LLViewerInventoryItem* item = mItems.at(i);
		const std::string& item_name = item->getName();

		LLFavoriteLandmarkMenuItem::Params item_params;
		item_params.name(item_name);
		item_params.label(item_name);

		LLFavoriteLandmarkMenuItem *menu_item = new LLFavoriteLandmarkMenuItem(item_params);
		menu_item->setMouseUpCallback(std::bind(&LLFavoritesBarCtrl::onButtonClick, this, item->getUUID()));
		menu_item->initFavoritesBarPointer(this);
		menu_item->setRightMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonRightClick, this, item->getUUID(), _1, _2, _3, _4));
		menu_item->LLUICtrl::setMouseDownCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseDown, this, item->getUUID(), _1, _2, _3, _4));
		menu_item->LLUICtrl::setMouseUpCallback(boost::bind(&LLFavoritesBarCtrl::onButtonMouseUp, this, item->getUUID(), _1, _2, _3, _4));
		menu_item->setLandmarkID(item->getUUID());

		fitLabelWidth(menu_item);

		widest_item = llmax(widest_item, menu_item->getNominalWidth());

		menu->addChild(menu_item);
	}

	addOpenLandmarksMenuItem(menu);
	mUpdateDropDownItems = false;
}

void LLFavoritesBarCtrl::fitLabelWidth(LLMenuItemCallGL* menu_item)
{
	U32 max_width = llmin(DROP_DOWN_MENU_WIDTH, getRect().getWidth());
	std::string item_name = menu_item->getName();

	// Check whether item name wider than menu
	if (menu_item->getNominalWidth() > max_width)
	{
		S32 chars_total = item_name.length();
		S32 chars_fitted = 1;
		menu_item->setLabel(LLStringExplicit(""));
		S32 label_space = max_width - menu_item->getFont()->getWidth("...") -
				menu_item->getNominalWidth();// This returns width of menu item with empty label (pad pixels)

		while (chars_fitted < chars_total
				&& menu_item->getFont()->getWidth(item_name, 0, chars_fitted) < label_space)
		{
			chars_fitted++;
		}
		chars_fitted--; // Rolling back one char, that doesn't fit

		menu_item->setLabel(item_name.substr(0, chars_fitted) + "...");
	}
}

void LLFavoritesBarCtrl::addOpenLandmarksMenuItem(LLMenuGL* menu)
{
	std::string label_untrans = "Open landmarks";
	std::string	label_transl;
	bool translated = LLTrans::findString(label_transl, label_untrans);

	LLMenuItemCallGL* menu_item = new LLMenuItemCallGL(std::string("open_my_landmarks"), std::string(translated ? label_transl : label_untrans), nullptr);
	void show_floater(const std::string& floater_name);
	menu_item->setMouseUpCallback(std::bind(show_floater, "floater_inventory_favs.xml"));

	fitLabelWidth(menu_item);

	LLMenuItemSeparatorGL* separator = new LLMenuItemSeparatorGL;

	menu->addChild(separator);
	menu->addChild(menu_item);
}

void LLFavoritesBarCtrl::positionAndShowMenu(LLMenuGL* menu)
{
	U32 max_width = llmin(DROP_DOWN_MENU_WIDTH, getRect().getWidth());

	S32 menu_x = getRect().getWidth() - max_width;
	S32 menu_y = getParent()->getRect().mBottom - DROP_DOWN_MENU_TOP_PAD;

	/* Singu TODO: Side Toolbar?
	// the menu should be offset of the right edge of the window
	// so it's no covered by buttons in the right-side toolbar.
	LLToolBar* right_toolbar = gToolBarView->getChild<LLToolBar>("toolbar_right");
	if (right_toolbar && right_toolbar->hasButtons())
	{
		S32 toolbar_top = 0;

		if (LLView* top_border_panel = right_toolbar->getChild<LLView>("button_panel"))
		{
			toolbar_top = top_border_panel->calcScreenRect().mTop;
		}

		// Calculating the bottom (in screen coord) of the drop down menu
		S32 menu_top = getParent()->getRect().mBottom - DROP_DOWN_MENU_TOP_PAD;
		S32 menu_bottom = menu_top - menu->getRect().getHeight();
		S32 menu_bottom_screen = 0;

		localPointToScreen(0, menu_bottom, &menu_top, &menu_bottom_screen);

		if (menu_bottom_screen < toolbar_top)
		{
			menu_x -= right_toolbar->getRect().getWidth();
		}
	}
	*/

	LLMenuGL::showPopup(this, menu, menu_x, menu_y);
}

void LLFavoritesBarCtrl::onButtonClick(LLUUID item_id)
{
	// We only have one Inventory, gInventory. Some day this should be better abstracted.
	LLInvFVBridgeAction::doAction(item_id,&gInventory);
}

void LLFavoritesBarCtrl::onButtonRightClick( LLUUID item_id,LLView* fav_button,S32 x,S32 y,MASK mask)
{
	mSelectedItemID = item_id;
	
	LLMenuGL* menu = (LLMenuGL*)mContextMenuHandle.get();
	if (!menu)
	{
		return;
	}

	// Remember that the context menu was shown simultaneously with the overflow menu,
	// so that we can restore the overflow menu when user clicks a context menu item
	// (which hides the overflow menu).
	{
		LLView* overflow_menu = mOverflowMenuHandle.get();
		mRestoreOverflowMenu = overflow_menu && overflow_menu->getVisible();
	}
	
	// Release mouse capture so hover events go to the popup menu
	// because this is happening during a mouse down.
	gFocusMgr.setMouseCapture(NULL);

	menu->updateParent(LLMenuGL::sMenuContainer);
	LLMenuGL::showPopup(fav_button, menu, x, y);
}

BOOL LLFavoritesBarCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = childrenHandleRightMouseDown( x, y, mask) != NULL;
	/*if(!handled && !gMenuHolder->hasVisibleMenu())
	{
		show_navbar_context_menu(this,x,y);
		handled = true;
	}*/

	return handled;
}
void copy_slurl_to_clipboard_cb(std::string& slurl)
{
	LLView::getWindow()->copyTextToClipboard(utf8str_to_wstring(slurl));

	LLSD args;
	args["SLURL"] = slurl;
	LLNotificationsUtil::add("CopySLURL", args);
}


bool LLFavoritesBarCtrl::enableSelected(const LLSD& userdata)
{
    std::string param = userdata.asString();

    if (param == std::string("can_paste"))
    {
        return isClipboardPasteable();
    }

    return false;
}

void LLFavoritesBarCtrl::doToSelected(const LLSD& userdata)
{
	std::string action = userdata.asString();
	LL_INFOS("FavoritesBar") << "Action = " << action << " Item = " << mSelectedItemID.asString() << LL_ENDL;
	
	LLViewerInventoryItem* item = gInventory.getItem(mSelectedItemID);
	if (!item)
		return;
	
	if (action == "open")
	{
		onButtonClick(item->getUUID());
	}
	else if (action == "about")
	{
		LLSD key;
		key["type"] = "landmark";
		key["id"] = mSelectedItemID;

		open_landmark(item, "Favorite", false, mSelectedItemID, true);
	}
	else if (action == "copy_slurl")
	{
		LLVector3d posGlobal;
		LLLandmarkActions::getLandmarkGlobalPos(mSelectedItemID, posGlobal);

		if (!posGlobal.isExactlyZero())
		{
			LLLandmarkActions::getSLURLfromPosGlobal(posGlobal, copy_slurl_to_clipboard_cb);
		}
	}
	else if (action == "show_on_map")
	{
		LLFloaterWorldMap* worldmap_instance = gFloaterWorldMap;

		LLVector3d posGlobal;
		LLLandmarkActions::getLandmarkGlobalPos(mSelectedItemID, posGlobal);

		if (!posGlobal.isExactlyZero() && worldmap_instance)
		{
			worldmap_instance->trackLocation(posGlobal);
			LLFloaterWorldMap::show(true);
		}
	}
	else if (action == "cut")
	{
	}
	else if (action == "copy")
	{
		LLInventoryClipboard::instance().add(mSelectedItemID);
	}
	else if (action == "paste")
	{
		pasteFromClipboard();
	}
	else if (action == "delete")
	{
		gInventory.removeItem(mSelectedItemID);
	}

	// Pop-up the overflow menu again (it gets hidden whenever the user clicks a context menu item).
	// See EXT-4217 and STORM-207.
	LLMenuGL* menu = (LLMenuGL*) mOverflowMenuHandle.get();
	if (mRestoreOverflowMenu && menu && !menu->getVisible())
	{
		menu->resetScrollPositionOnShow(false);
		showDropDownMenu();
		menu->resetScrollPositionOnShow(true);
	}
}

BOOL LLFavoritesBarCtrl::isClipboardPasteable() const
{
	if (!LLInventoryClipboard::instance().hasContents())
	{
		return FALSE;
	}

	uuid_vec_t objects;
	LLInventoryClipboard::instance().retrieve(objects);
	S32 count = objects.size();
	for(S32 i = 0; i < count; i++)
	{
		const LLUUID &item_id = objects.at(i);

		// Can't paste folders
		const LLInventoryCategory *cat = gInventory.getCategory(item_id);
		if (cat)
		{
			return FALSE;
		}

		const LLInventoryItem *item = gInventory.getItem(item_id);
		if (item && LLAssetType::AT_LANDMARK != item->getType())
		{
			return FALSE;
		}
	}
	return TRUE;
}

void LLFavoritesBarCtrl::pasteFromClipboard() const
{
	LLInventoryModel* model = &gInventory;
	if(model && isClipboardPasteable())
	{
		LLInventoryItem* item = NULL;
		uuid_vec_t objects;
		LLInventoryClipboard::instance().retrieve(objects);
		S32 count = objects.size();
		LLUUID parent_id(mFavoriteFolderId);
		for(S32 i = 0; i < count; i++)
		{
			item = model->getItem(objects.at(i));
			if (item)
			{
				copy_inventory_item(
					gAgent.getID(),
					item->getPermissions().getOwner(),
					item->getUUID(),
					parent_id,
					std::string(),
					LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}
}

void LLFavoritesBarCtrl::onButtonMouseDown(LLUUID id, LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
	// EXT-6997 (Fav bar: Pop-up menu for LM in overflow dropdown is kept after LM was dragged away)
	// mContextMenuHandle.get() - is a pop-up menu (of items) in already opened dropdown menu.
	// We have to check and set visibility of pop-up menu in such a way instead of using
	// LLMenuHolderGL::hideMenus() because it will close both menus(dropdown and pop-up), but
	// we need to close only pop-up menu while dropdown one should be still opened.
	LLMenuGL* menu = (LLMenuGL*)mContextMenuHandle.get();
	if(menu && menu->getVisible())
	{
		menu->setVisible(FALSE);
	}

	mDragItemId = id;
	mStartDrag = TRUE;

	S32 screenX, screenY;
	localPointToScreen(x, y, &screenX, &screenY);

	LLToolDragAndDrop::getInstance()->setDragStart(screenX, screenY);
}

void LLFavoritesBarCtrl::onButtonMouseUp(LLUUID id, LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
	mStartDrag = FALSE;
	mDragItemId = LLUUID::null;
}

void LLFavoritesBarCtrl::onEndDrag()
{
	mEndDragConnection.disconnect();

	showDragMarker(FALSE);
	mDragItemId = LLUUID::null;
	LLView::getWindow()->setCursor(UI_CURSOR_ARROW);
}

BOOL LLFavoritesBarCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	if (mDragItemId != LLUUID::null && mStartDrag)
	{
		S32 screenX, screenY;
		localPointToScreen(x, y, &screenX, &screenY);

		if(LLToolDragAndDrop::getInstance()->isOverThreshold(screenX, screenY))
		{
			LLToolDragAndDrop::getInstance()->beginDrag(
				DAD_LANDMARK, mDragItemId,
				LLToolDragAndDrop::SOURCE_LIBRARY);

			mStartDrag = FALSE;

			return LLToolDragAndDrop::getInstance()->handleHover(x, y, mask);
		}
	}

	return TRUE;
}

LLUICtrl* LLFavoritesBarCtrl::findChildByLocalCoords(S32 x, S32 y)
{
	LLUICtrl* ctrl = NULL;
	const child_list_t* list = getChildList();

	for (child_list_const_iter_t i = list->begin(); i != list->end(); ++i)
	{
		// Look only for children that are favorite buttons
		if ((*i)->getName() == "favorites_bar_btn")
		{
			LLRect rect = (*i)->getRect();
			// We consider a button hit if the cursor is left of the right side
			// This makes the hit a bit less finicky than hitting directly on the button itself
			if (x <= rect.mRight)
			{
				ctrl = dynamic_cast<LLUICtrl*>(*i);
				break;
			}
		}
	}
	return ctrl;
}

BOOL needToSaveItemsOrder(const LLInventoryModel::item_array_t& items)
{
	BOOL result = FALSE;

	// if there is an item without sort order field set, we need to save items order
	for (LLInventoryModel::item_array_t::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		if (LLFavoritesOrderStorage::instance().getSortIndex((*i)->getUUID()) < 0)
		{
			result = TRUE;
			break;
		}
	}

	return result;
}
BOOL LLFavoritesBarCtrl::needToSaveItemsOrder(const LLInventoryModel::item_array_t& items) { return ::needToSaveItemsOrder(items); }

void LLFavoritesBarCtrl::insertItem(LLInventoryModel::item_array_t& items, const LLUUID& dest_item_id, LLViewerInventoryItem* insertedItem, bool insert_before)
{
	// Get the iterator to the destination item
	LLInventoryModel::item_array_t::iterator it_dest = LLInventoryModel::findItemIterByUUID(items, dest_item_id);
	if (it_dest == items.end())
		return;

	// Go to the next element if one wishes to insert after the dest element
	if (!insert_before)
	{
		++it_dest;
	}
	
	// Insert the source item in the right place
	if (it_dest != items.end())
	{
		items.insert(it_dest, insertedItem);
	}
	else 
	{
		// Append to the list if it_dest reached the end
		items.push_back(insertedItem);
	}
}

const std::string LLFavoritesOrderStorage::SORTING_DATA_FILE_NAME = "landmarks_sorting.xml";
const S32 LLFavoritesOrderStorage::NO_INDEX = -1;

void LLFavoritesOrderStorage::setSortIndex(const LLViewerInventoryItem* inv_item, S32 sort_index)
{
	mSortIndexes[inv_item->getUUID()] = sort_index;
	mIsDirty = true;
	getSLURL(inv_item->getAssetUUID());
}

S32 LLFavoritesOrderStorage::getSortIndex(const LLUUID& inv_item_id)
{
	sort_index_map_t::const_iterator it = mSortIndexes.find(inv_item_id);
	if (it != mSortIndexes.end())
	{
		return it->second;
	}
	return NO_INDEX;
}

void LLFavoritesOrderStorage::removeSortIndex(const LLUUID& inv_item_id)
{
	mSortIndexes.erase(inv_item_id);
	mIsDirty = true;
}

void LLFavoritesOrderStorage::getSLURL(const LLUUID& asset_id)
{
	slurls_map_t::iterator slurl_iter = mSLURLs.find(asset_id);
	if (slurl_iter != mSLURLs.end()) return; // SLURL for current landmark is already cached

	LLLandmark* lm = gLandmarkList.getAsset(asset_id,
		boost::bind(&LLFavoritesOrderStorage::onLandmarkLoaded, this, asset_id, _1));
	if (lm)
	{
        LL_DEBUGS("FavoritesBar") << "landmark for " << asset_id << " already loaded" << LL_ENDL;
		onLandmarkLoaded(asset_id, lm);
	}
}

// static
std::string LLFavoritesOrderStorage::getStoredFavoritesFilename()
{
	std::string user_dir = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "");

    return (user_dir.empty() ? ""
            : gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
                                             "stored_favorites_"
                                          + gHippoGridManager->getCurrentGridName()
                                          + ".xml")
            );
}

// static
void LLFavoritesOrderStorage::destroyClass()
{
	LLFavoritesOrderStorage::instance().cleanup();


	std::string old_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "stored_favorites.xml");
	llifstream file;
	file.open(old_filename.c_str());
	if (file.is_open())
	{
        file.close();
		std::string new_filename = getStoredFavoritesFilename();
        LL_INFOS("FavoritesBar") << "moving favorites from old name '" << old_filename
                                 << "' to new name '" << new_filename << "'"
                                 << LL_ENDL;
		LLFile::rename(old_filename,new_filename);
	}

	if (gSavedPerAccountSettings.getBOOL("ShowFavoritesOnLogin"))
	{
		LLFavoritesOrderStorage::instance().saveFavoritesSLURLs();
	}
	else
	{
		LLFavoritesOrderStorage::instance().removeFavoritesRecordOfUser();
	}
}

std::string LLFavoritesOrderStorage::getSavedOrderFileName()
{
	// If we quit from the login screen we will not have an SL account
	// name.  Don't try to save, otherwise we'll dump a file in
	// C:\Program Files\SecondLife\ or similar. JC
	std::string user_dir = gDirUtilp->getLindenUserDir();
    return (user_dir.empty() ? "" : gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, SORTING_DATA_FILE_NAME));
}

void LLFavoritesOrderStorage::load()
{
	// load per-resident sorting information
	std::string filename = getSavedOrderFileName();
	LLSD settings_llsd;
	llifstream file;
	file.open(filename.c_str());
	if (file.is_open())
	{
		LLSDSerialize::fromXML(settings_llsd, file);
        LL_INFOS("FavoritesBar") << "loaded favorites order from '" << filename << "' "
                                 << (settings_llsd.isMap() ? "" : "un") << "successfully"
                                 << LL_ENDL;
        file.close();
	}
    else
    {
        LL_WARNS("FavoritesBar") << "unable to open favorites order file at '" << filename << "'" << LL_ENDL;
    }

	for (LLSD::map_const_iterator iter = settings_llsd.beginMap();
		iter != settings_llsd.endMap(); ++iter)
	{
		mSortIndexes.insert(std::make_pair(LLUUID(iter->first), (S32)iter->second.asInteger()));
	}
}

void LLFavoritesOrderStorage::saveFavoritesSLURLs()
{
	// Do not change the file if we are not logged in yet.
	if (gAgentID.isNull()) //LLLoginInstance::getInstance()->authSuccess())
	{
		LL_WARNS("FavoritesBar") << "Cannot save favorites: not logged in" << LL_ENDL;
		return;
	}

	std::string filename = getStoredFavoritesFilename();
    if (!filename.empty())
    {
        llifstream in_file;
        in_file.open(filename.c_str());
        LLSD fav_llsd;
        if (in_file.is_open())
        {
            LLSDSerialize::fromXML(fav_llsd, in_file);
            LL_INFOS("FavoritesBar") << "loaded favorites from '" << filename << "' "
                                     << (fav_llsd.isMap() ? "" : "un") << "successfully"
                                     << LL_ENDL;
            in_file.close();
        }
        else
        {
            LL_WARNS("FavoritesBar") << "unable to open favorites from '" << filename << "'" << LL_ENDL;
        }

        const LLUUID fav_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        gInventory.collectDescendents(fav_id, cats, items, LLInventoryModel::EXCLUDE_TRASH);

        LLSD user_llsd;
        for (LLInventoryModel::item_array_t::const_iterator it = items.cbegin(); it != items.cend(); ++it)
        {
			if (it->get()->getActualType() != LLAssetType::AT_LANDMARK) continue;
            LLSD value;
            value["name"] = (*it)->getName();
            value["asset_id"] = (*it)->getAssetUUID();

            slurls_map_t::const_iterator slurl_iter = mSLURLs.find(value["asset_id"]);
            if (slurl_iter != mSLURLs.cend())
            {
                LL_DEBUGS("FavoritesBar") << "Saving favorite: idx=" << LLFavoritesOrderStorage::instance().getSortIndex((*it)->getUUID())
                    << ", SLURL=" <<  slurl_iter->second 
                    << ", value=" << value << LL_ENDL;
                value["slurl"] = slurl_iter->second;
                user_llsd[LLFavoritesOrderStorage::instance().getSortIndex((*it)->getUUID())] = value;
            }
            else
            {
                LL_WARNS("FavoritesBar") << "Not saving favorite " << value["name"] << ": no matching SLURL" << LL_ENDL;
            }
        }

        LLAvatarName av_name;
        LLAvatarNameCache::get( gAgentID, &av_name );
        // Note : use the "John Doe" and not the "john.doe" version of the name 
        // as we'll compare it with the stored credentials in the login panel.
        fav_llsd[av_name.getUserName()] = user_llsd;

        llofstream file;
        file.open(filename.c_str());
        if ( file.is_open() )
        {
            LLSDSerialize::toPrettyXML(fav_llsd, file);
            LL_INFOS("FavoritesBar") << "saved favorites for '" << av_name.getUserName()
                                     << "' to '" << filename << "' "
                                     << LL_ENDL;
            file.close();
        }
        else
        {
            LL_WARNS("FavoritesBar") << "unable to open favorites storage for '" << av_name.getUserName()
                                     << "' at '" << filename << "' "
                                     << LL_ENDL;
        }
    }
}

void LLFavoritesOrderStorage::removeFavoritesRecordOfUser()
{
	std::string filename = getStoredFavoritesFilename();
    if (!filename.empty())
    {
        LLSD fav_llsd;
        llifstream file;
        file.open(filename.c_str());
        if (file.is_open())
        {
            LLSDSerialize::fromXML(fav_llsd, file);
            file.close();
        
            LLAvatarName av_name;
            LLAvatarNameCache::get( gAgentID, &av_name );
            // Note : use the "John Doe" and not the "john.doe" version of the name.
            // See saveFavoritesSLURLs() here above for the reason why.
            if (fav_llsd.has(av_name.getUserName()))
            {
                LL_INFOS("FavoritesBar") << "Removed favorites for " << av_name.getUserName() << LL_ENDL;
                fav_llsd.erase(av_name.getUserName());
            }
        
            llofstream out_file;
            out_file.open(filename.c_str());
            if ( out_file.is_open() )
            {
                LLSDSerialize::toPrettyXML(fav_llsd, out_file);
                LL_INFOS("FavoritesBar") << "saved favorites to '" << filename << "' "
                                         << LL_ENDL;
                out_file.close();
            }
        }
    }
}

void LLFavoritesOrderStorage::onLandmarkLoaded(const LLUUID& asset_id, LLLandmark* landmark)
{
	if (landmark)
    {
        LL_DEBUGS("FavoritesBar") << "landmark for " << asset_id << " loaded" << LL_ENDL;
	LLVector3d pos_global;
	if (!landmark->getGlobalPos(pos_global))
	{
		// If global position was unknown on first getGlobalPos() call
		// it should be set for the subsequent calls.
		landmark->getGlobalPos(pos_global);
	}

        if (!pos_global.isExactlyZero())
        {
        	LL_DEBUGS("FavoritesBar") << "requesting slurl for landmark " << asset_id << LL_ENDL;
        	LLLandmarkActions::getSLURLfromPosGlobal(pos_global,
                boost::bind(&LLFavoritesOrderStorage::storeFavoriteSLURL, this, asset_id, _1));
        }
    }
}

void LLFavoritesOrderStorage::storeFavoriteSLURL(const LLUUID& asset_id, std::string& slurl)
{
	LL_DEBUGS("FavoritesBar") << "Saving landmark SLURL '" << slurl << "' for " << asset_id << LL_ENDL;
	mSLURLs[asset_id] = slurl;
}

void LLFavoritesOrderStorage::save()
{
	if (mIsDirty)
    {
        // something changed, so save it
        std::string filename = LLFavoritesOrderStorage::getInstance()->getSavedOrderFileName();
        if (!filename.empty())
        {
            LLSD settings_llsd;

            for(sort_index_map_t::const_iterator iter = mSortIndexes.begin(); iter != mSortIndexes.end(); ++iter)
            {
                settings_llsd[iter->first.asString()] = iter->second;
            }

            llofstream file;
            file.open(filename.c_str());
            if ( file.is_open() )
            {
                LLSDSerialize::toPrettyXML(settings_llsd, file);
                LL_INFOS("FavoritesBar") << "saved favorites order to '" << filename << "' " << LL_ENDL;
            }
            else
            {
                LL_WARNS("FavoritesBar") << "failed to open favorites order file '" << filename << "' " << LL_ENDL;
            }
        }
        else
        {
            LL_DEBUGS("FavoritesBar") << "no user directory available to store favorites order file" << LL_ENDL;
        }
    }
}


void LLFavoritesOrderStorage::cleanup()
{
	// nothing to clean
	if (!mIsDirty) return;

	const LLUUID fav_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendents(fav_id, cats, items, LLInventoryModel::EXCLUDE_TRASH);

	IsNotInFavorites is_not_in_fav(items);

	sort_index_map_t  aTempMap;
	//copy unremoved values from mSortIndexes to aTempMap
	std::remove_copy_if(mSortIndexes.begin(), mSortIndexes.end(),
		inserter(aTempMap, aTempMap.begin()),
		is_not_in_fav);

	//Swap the contents of mSortIndexes and aTempMap
	mSortIndexes.swap(aTempMap);
}

// See also LLInventorySort where landmarks in the Favorites folder are sorted.
class LLViewerInventoryItemSort
{
public:
	bool operator()(const LLPointer<LLViewerInventoryItem>& a, const LLPointer<LLViewerInventoryItem>& b) const
	{
		return LLFavoritesOrderStorage::instance().getSortIndex(a->getUUID())
			< LLFavoritesOrderStorage::instance().getSortIndex(b->getUUID());
	}
};

void LLFavoritesOrderStorage::saveOrder()
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLIsType is_type(LLAssetType::AT_LANDMARK);
	LLUUID favorites_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
	gInventory.collectDescendentsIf(favorites_id, cats, items, LLInventoryModel::EXCLUDE_TRASH, is_type);
	std::sort(items.begin(), items.end(), LLViewerInventoryItemSort());
	saveItemsOrder(items);
}

void LLFavoritesOrderStorage::saveItemsOrder( const LLInventoryModel::item_array_t& items )
{

	int sortField = 0;
	// current order is saved by setting incremental values (1, 2, 3, ...) for the sort field
	for (LLInventoryModel::item_array_t::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		LLViewerInventoryItem* item = *i;

		setSortIndex(item, ++sortField);

		item->setComplete(TRUE);
		item->updateServer(FALSE);

		gInventory.updateItem(item);

		// Tell the parent folder to refresh its sort order.
		gInventory.addChangedMask(LLInventoryObserver::SORT, item->getParentUUID());
	}

	gInventory.notifyObservers();
}


// * @param source_item_id - LLUUID of the source item to be moved into new position
// * @param target_item_id - LLUUID of the target item before which source item should be placed.
void LLFavoritesOrderStorage::rearrangeFavoriteLandmarks(const LLUUID& source_item_id, const LLUUID& target_item_id)
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	//LLIsType is_type(LLAssetType::AT_LANDMARK); // Singu Note: We can have anything here~
	LLUUID favorites_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_FAVORITE);
	gInventory.collectDescendents(favorites_id, cats, items, LLInventoryModel::EXCLUDE_TRASH/*, is_type*/);

	// ensure items are sorted properly before changing order. EXT-3498
	std::sort(items.begin(), items.end(), LLViewerInventoryItemSort());

	// update order
	gInventory.updateItemsOrder(items, source_item_id, target_item_id);

	saveItemsOrder(items);
}

void AddFavoriteLandmarkCallback::fire(const LLUUID& inv_item_id)
{
	if (mTargetLandmarkId.isNull()) return;

	LLFavoritesOrderStorage::instance().rearrangeFavoriteLandmarks(inv_item_id, mTargetLandmarkId);
}
// EOF
