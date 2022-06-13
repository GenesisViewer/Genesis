/** 
* @file llinventoryfilter.cpp
* @brief Support for filtering your inventory to only display a subset of the
* available items.
*
* $LicenseInfo:firstyear=2005&license=viewerlgpl$
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

#include "llinventoryfilter.h"

// viewer includes
#include "llappearancemgr.h"
#include "llfoldervieweventlistener.h"
#include "llfolderviewitem.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llinventoryfunctions.h"
#include "llmarketplacefunctions.h"
#include "llviewercontrol.h"
#include "llfolderview.h"
#include "llinventorybridge.h"
#include "llviewerfoldertype.h"
#include "llstartup.h"
#include "llagentwearables.h"
#include "llvoavatarself.h"

// linden library includes
#include "llinventoryclipboard.h"
#include "lltrans.h"

LLInventoryFilter::FilterOps::FilterOps(const Params& p)
:	mFilterTypes(p.types),
	mFilterObjectTypes(p.object_types),
	mFilterWearableTypes(p.wearable_types),
	mFilterCategoryTypes(p.category_types),
	mFilterLinks(p.links),
	mFilterUUID(p.uuid),
	mMinDate(p.date_range.min_date),
	mMaxDate(p.date_range.max_date),
	mHoursAgo(p.hours_ago),
	mDateSearchDirection(p.date_search_direction),
	mShowFolderState(p.show_folder_state),
	mPermissions(p.permissions),
	mFilterWornItems(p.worn_items)
{
}

///----------------------------------------------------------------------------
/// Class LLInventoryFilter
///----------------------------------------------------------------------------
LLInventoryFilter::LLInventoryFilter(const Params& p)
:	mFilterOps(p.filter_ops),
	mFilterSubString(p.substring),
	mName(p.name),
	mFilterModified(FILTER_NONE),
	mEmptyLookupMessage("InventoryNoMatchingItems"),
	mCurrentGeneration(0),
	mFirstRequiredGeneration(0),
	mFirstSuccessGeneration(0)
{
	mOrder = SO_FOLDERS_BY_NAME; // This gets overridden by a pref immediately

	mSubStringMatchOffset = std::string::npos;
	mFilterCount = 0;
	// Singu Note: Why aren't we calling fromParams here?

	// copy mFilterOps into mDefaultFilterOps
	markDefault();
}

bool LLInventoryFilter::check(LLFolderViewItem* item)
{
	// Clipboard cut items are *always* filtered so we need this value upfront
	const LLFolderViewEventListener* listener = item->getListener();
	const LLUUID item_id = listener ? listener->getUUID() : LLUUID::null;
	const bool passed_clipboard = listener && item_id.notNull() ? checkAgainstClipboard(item_id) : true;

	// If it's a folder and we're showing all folders, return automatically.
	const BOOL is_folder = listener->getInventoryType() == LLInventoryType::IT_CATEGORY;
	if (is_folder && (mFilterOps.mShowFolderState == LLInventoryFilter::SHOW_ALL_FOLDERS))
	{
		return passed_clipboard;
	}

	mSubStringMatchOffset = mFilterSubString.size() ? item->getSearchableLabel().find(mFilterSubString) : std::string::npos;

	const bool passed_filtertype = checkAgainstFilterType(item);
	const bool passed_permissions = checkAgainstPermissions(item);
	const bool passed_filterlink = checkAgainstFilterLinks(item);
	const bool passed_wearable = (mFilterOps.mFilterTypes & FILTERTYPE_WORN) != FILTERTYPE_WORN || (gAgentWearables.isWearingItem(item_id) || (gAgentAvatarp && gAgentAvatarp->isWearingAttachment(item_id)));
	const bool passed = (passed_filtertype &&
						 passed_permissions &&
						 passed_filterlink &&
						 passed_clipboard &&
						 passed_wearable &&
						 (mFilterSubString.size() == 0 || mSubStringMatchOffset != std::string::npos));
	return passed;
}

bool LLInventoryFilter::checkFolder(const LLFolderViewFolder* folder) const
{
	if (!folder)
	{
		LL_WARNS() << "The filter can not be checked on an invalid folder." << LL_ENDL;
		llassert(false); // crash in development builds
		return false;
	}

	const LLFolderViewEventListener* listener = folder->getListener();
	if (!listener)
	{
		LL_ERRS() << "Folder view event listener not found." << LL_ENDL;
		return false;
	}

	const LLUUID folder_id = listener->getUUID();

	return checkFolder(folder_id);
}

bool LLInventoryFilter::checkFolder(const LLUUID& folder_id) const
{
	// Always check against the clipboard
	const BOOL passed_clipboard = checkAgainstClipboard(folder_id);

	// we're showing all folders, overriding filter
	if (mFilterOps.mShowFolderState == LLInventoryFilter::SHOW_ALL_FOLDERS)
	{
		return passed_clipboard;
	}

	// when applying a filter, matching folders get their contents downloaded first
	// but make sure we are not interfering with pre-download
	if (isNotDefault()
		&& !gInventory.isCategoryComplete(folder_id)
		&& LLStartUp::getStartupState() > STATE_WEARABLES_WAIT)
	{
		LLInventoryModelBackgroundFetch::instance().start(folder_id);
	}

	// Marketplace folder filtering
	const U32 filterTypes = mFilterOps.mFilterTypes;
	const U32 marketplace_filter = FILTERTYPE_MARKETPLACE_ACTIVE | FILTERTYPE_MARKETPLACE_INACTIVE |
								   FILTERTYPE_MARKETPLACE_UNASSOCIATED | FILTERTYPE_MARKETPLACE_LISTING_FOLDER |
								   FILTERTYPE_NO_MARKETPLACE_ITEMS;
	if (filterTypes & marketplace_filter)
	{
		S32 depth = depth_nesting_in_marketplace(folder_id);

		if (filterTypes & FILTERTYPE_NO_MARKETPLACE_ITEMS)
		{
			if (depth >= 0)
			{
				return false;
			}
		}

		if (filterTypes & FILTERTYPE_MARKETPLACE_LISTING_FOLDER)
		{
			if (depth > 1)
			{
				return false;
			}
		}

		if (depth > 0)
		{
			LLUUID listing_uuid = nested_parent_id(folder_id, depth);
			if (filterTypes & FILTERTYPE_MARKETPLACE_ACTIVE)
			{
				if (!LLMarketplaceData::instance().getActivationState(listing_uuid))
				{
					return false;
				}
			}
			else if (filterTypes & FILTERTYPE_MARKETPLACE_INACTIVE)
			{
				if (!LLMarketplaceData::instance().isListed(listing_uuid) || LLMarketplaceData::instance().getActivationState(listing_uuid))
				{
					return false;
				}
			}
			else if (filterTypes & FILTERTYPE_MARKETPLACE_UNASSOCIATED)
			{
				if (LLMarketplaceData::instance().isListed(listing_uuid))
				{
					return false;
				}
			}
		}
	}

	// show folder links
	LLViewerInventoryItem* item = gInventory.getItem(folder_id);
	if (item && item->getActualType() == LLAssetType::AT_LINK_FOLDER)
	{
		return passed_clipboard;
	}

	if (mFilterOps.mFilterTypes & FILTERTYPE_CATEGORY)
	{
		// Can only filter categories for items in your inventory
		// (e.g. versus in-world object contents).
		const LLViewerInventoryCategory *cat = gInventory.getCategory(folder_id);
		if (!cat)
			return folder_id.isNull();
		LLFolderType::EType cat_type = cat->getPreferredType();
		if (cat_type != LLFolderType::FT_NONE && (1LL << cat_type & mFilterOps.mFilterCategoryTypes) == U64(0))
			return false;
	}

	return passed_clipboard;
}

bool LLInventoryFilter::checkAgainstFilterType(const LLFolderViewItem* item) const
{
	const LLFolderViewEventListener* listener = item->getListener();
	if (!listener) return FALSE;

	LLInventoryType::EType object_type = listener->getInventoryType();
	const LLUUID object_id = listener->getUUID();
	const LLInventoryObject *object = gInventory.getObject(object_id);

	const U32 filterTypes = mFilterOps.mFilterTypes;

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_OBJECT
	// Pass if this item's type is of the correct filter type
	if (filterTypes & FILTERTYPE_OBJECT)
	{

		// If it has no type, pass it, unless it's a link.
		if (object_type == LLInventoryType::IT_NONE)
		{
			if (object && object->getIsLinkType())
			{
				return FALSE;
			}
		}
		else if ((1LL << object_type & mFilterOps.mFilterObjectTypes) == U64(0))
		{
			return FALSE;
		}
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_UUID
	// Pass if this item is the target UUID or if it links to the target UUID
	if (filterTypes & FILTERTYPE_UUID)
	{
		if (!object) return FALSE;

		if (object->getLinkedUUID() != mFilterOps.mFilterUUID)
			return FALSE;
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_DATE
	// Pass if this item is within the date range.
	if (filterTypes & FILTERTYPE_DATE)
	{
		const U16 HOURS_TO_SECONDS = 3600;
		time_t earliest = time_corrected() - mFilterOps.mHoursAgo * HOURS_TO_SECONDS;

		if (mFilterOps.mMinDate > time_min() && mFilterOps.mMinDate < earliest)
		{
			earliest = mFilterOps.mMinDate;
		}
		else if (!mFilterOps.mHoursAgo)
		{
			earliest = 0;
		}

		if (FILTERDATEDIRECTION_NEWER == mFilterOps.mDateSearchDirection || isSinceLogoff())
		{
			if (listener->getCreationDate() < earliest ||
				listener->getCreationDate() > mFilterOps.mMaxDate)
				return FALSE;
		}
		else
		{
			if (listener->getCreationDate() > earliest ||
				listener->getCreationDate() > mFilterOps.mMaxDate)
				return FALSE;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_WEARABLE
	// Pass if this item is a wearable of the appropriate type
	if (filterTypes & FILTERTYPE_WEARABLE)
	{
		LLWearableType::EType type = listener->getWearableType();
		if ((0x1LL << type & mFilterOps.mFilterWearableTypes) == 0)
		{
			return FALSE;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_EMPTYFOLDERS
	// Pass if this item is a folder and is not a system folder that should be hidden
	if (filterTypes & FILTERTYPE_EMPTYFOLDERS)
	{
		if (object_type == LLInventoryType::IT_CATEGORY)
		{
			bool is_hidden_if_empty = LLViewerFolderType::lookupIsHiddenIfEmpty(listener->getPreferredType());
			if (is_hidden_if_empty)
			{
				// Force the fetching of those folders so they are hidden if they really are empty...
				// But don't interfere with startup download
				if (LLStartUp::getStartupState() > STATE_WEARABLES_WAIT)
				{
					gInventory.fetchDescendentsOf(object_id);
				}

				LLInventoryModel::cat_array_t* cat_array = NULL;
				LLInventoryModel::item_array_t* item_array = NULL;
				gInventory.getDirectDescendentsOf(object_id,cat_array,item_array);
				S32 descendents_actual = 0;
				if (cat_array && item_array)
				{
					descendents_actual = cat_array->size() + item_array->size();
				}
				if (descendents_actual == 0)
				{
					return FALSE;
				}
			}
		}
	}
	
	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_WORN
	// Pass if this item is worn
	if (filterTypes & FILTERTYPE_WORN)
	{
		if (!object) return FALSE;
		LLUUID cat_id = object->getParentUUID();
		const LLViewerInventoryCategory *cat = gInventory.getCategory(cat_id);
		return (get_is_item_worn(object_id)										// if it's worn
				&& !LLAppearanceMgr::instance().getIsInCOF(object_id)			// if it's not in CoF
				&& (!cat || cat->getPreferredType() != LLFolderType::FT_OUTFIT)	// if it's not in an outfit
				&& !object->getIsLinkType());									// and it's not a link
	}

	return TRUE;
}

bool LLInventoryFilter::checkAgainstFilterType(const LLInventoryItem* item) const
{
	LLInventoryType::EType object_type = item->getInventoryType();
	const LLUUID object_id = item->getUUID();

	const U32 filterTypes = mFilterOps.mFilterTypes;

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_OBJECT
	// Pass if this item's type is of the correct filter type
	if (filterTypes & FILTERTYPE_OBJECT)
	{
		// If it has no type, pass it, unless it's a link.
		if (object_type == LLInventoryType::IT_NONE)
		{
			if (item && item->getIsLinkType())
			{
				return false;
			}
		}
		else if ((1LL << object_type & mFilterOps.mFilterObjectTypes) == U64(0))
		{
			return false;
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_UUID
	// Pass if this item is the target UUID or if it links to the target UUID
	if (filterTypes & FILTERTYPE_UUID)
	{
		if (!item) return false;

		if (item->getLinkedUUID() != mFilterOps.mFilterUUID)
			return false;
	}

	////////////////////////////////////////////////////////////////////////////////
	// FILTERTYPE_DATE
	// Pass if this item is within the date range.
	if (filterTypes & FILTERTYPE_DATE)
	{
		// We don't get the updated item creation date for the task inventory or
		// a notecard embedded item. See LLTaskInvFVBridge::getCreationDate().
		return false;
	}

	return true;
}

// Items and folders that are on the clipboard or, recursively, in a folder which
// is on the clipboard must be filtered out if the clipboard is in the "cut" mode.
bool LLInventoryFilter::checkAgainstClipboard(const LLUUID& object_id) const
{
	if (LLInventoryClipboard::instance().isCutMode())
	{
		LLUUID current_id = object_id;
		LLInventoryObject *current_object = gInventory.getObject(object_id);
		while (current_id.notNull() && current_object)
		{
			if (LLInventoryClipboard::instance().isOnClipboard(current_id))
			{
				return false;
			}
			current_id = current_object->getParentUUID();
			if (current_id.notNull())
			{
				current_object = gInventory.getObject(current_id);
			}
		}
	}
	return true;
}

bool LLInventoryFilter::checkAgainstPermissions(const LLFolderViewItem* item) const
{
	const LLFolderViewEventListener* listener = item->getListener();
	if (!listener) return FALSE;

	PermissionMask perm = listener->getPermissionMask();
	const LLInvFVBridge *bridge = dynamic_cast<const LLInvFVBridge *>(item->getListener());
	if (bridge && bridge->isLink())
	{
		const LLUUID& linked_uuid = gInventory.getLinkedItemID(bridge->getUUID());
		const LLViewerInventoryItem *linked_item = gInventory.getItem(linked_uuid);
		if (linked_item)
			perm = linked_item->getPermissionMask();
	}
	return (perm & mFilterOps.mPermissions) == mFilterOps.mPermissions;
}

bool LLInventoryFilter::checkAgainstPermissions(const LLInventoryItem* item) const
{
	if (!item) return false;

	LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
	PermissionMask perm = new_item->getPermissionMask();
	new_item = NULL;

	return (perm & mFilterOps.mPermissions) == mFilterOps.mPermissions;
}

bool LLInventoryFilter::checkAgainstFilterLinks(const LLFolderViewItem* item) const
{
	const LLFolderViewEventListener* listener = item->getListener();
	if (!listener) return TRUE;

	const LLUUID& object_id = listener->getUUID();
	const LLInventoryObject *object = gInventory.getObject(object_id);
	if (!object) return TRUE;

	const BOOL is_link = object->getIsLinkType();
	if (is_link && (mFilterOps.mFilterLinks == FILTERLINK_EXCLUDE_LINKS))
		return FALSE;
	if (!is_link && (mFilterOps.mFilterLinks == FILTERLINK_ONLY_LINKS))
		return FALSE;
	return TRUE;
}

const std::string& LLInventoryFilter::getFilterSubString(BOOL trim) const
{
	return mFilterSubString;
}

std::string::size_type LLInventoryFilter::getStringMatchOffset() const
{
	return mSubStringMatchOffset;
}

bool LLInventoryFilter::isDefault() const
{
	return !isNotDefault();
}

// has user modified default filter params?
bool LLInventoryFilter::isNotDefault() const
{
	bool not_default = false;

	not_default |= (mFilterOps.mFilterObjectTypes != mDefaultFilterOps.mFilterObjectTypes);
	not_default |= (mFilterOps.mFilterCategoryTypes != mDefaultFilterOps.mFilterCategoryTypes);
	not_default |= (mFilterOps.mFilterWearableTypes != mDefaultFilterOps.mFilterWearableTypes);
	not_default |= (mFilterOps.mFilterTypes != mDefaultFilterOps.mFilterTypes);
	not_default |= (mFilterOps.mFilterLinks != mDefaultFilterOps.mFilterLinks);
	not_default |= (mFilterSubString.size() > 0);
	not_default |= (mFilterOps.mPermissions != mDefaultFilterOps.mPermissions);
	not_default |= (mFilterOps.mMinDate != mDefaultFilterOps.mMinDate);
	not_default |= (mFilterOps.mMaxDate != mDefaultFilterOps.mMaxDate);
	not_default |= (mFilterOps.mHoursAgo != mDefaultFilterOps.mHoursAgo);

	return not_default != 0;
}

bool LLInventoryFilter::isActive() const
{
	return mFilterOps.mFilterObjectTypes != 0xffffffffffffffffULL
		|| mFilterOps.mFilterCategoryTypes != 0xffffffffffffffffULL
		|| mFilterOps.mFilterWearableTypes != 0xffffffffffffffffULL
		|| mFilterOps.mFilterTypes != FILTERTYPE_OBJECT
		|| mFilterOps.mFilterLinks != FILTERLINK_INCLUDE_LINKS
		|| mFilterSubString.size() 
		|| mFilterOps.mPermissions != PERM_NONE 
		|| mFilterOps.mMinDate != time_min()
		|| mFilterOps.mMaxDate != time_max()
		|| mFilterOps.mHoursAgo != 0;
}

bool LLInventoryFilter::isModified() const
{
	return mFilterModified != FILTER_NONE;
}

void LLInventoryFilter::updateFilterTypes(U64 types, U64& current_types)
{
	if (current_types != types)
	{
		// keep current items only if no type bits getting turned off
		bool fewer_bits_set = (current_types & ~types) != 0;
		bool more_bits_set = (~current_types & types) != 0;

		current_types = types;
		if (more_bits_set && fewer_bits_set)
		{
			// neither less or more restrictive, both simultaneously
			// so we need to filter from scratch
			setModified(FILTER_RESTART);
		}
		else if (more_bits_set)
		{
			// target is only one of all requested types so more type bits == less restrictive
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (fewer_bits_set)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
	}
}

void LLInventoryFilter::setFilterObjectTypes(U64 types)
{
	if (types == mFilterOps.mFilterObjectTypes && mFilterOps.mFilterTypes & FILTERTYPE_OBJECT)
		return;
	updateFilterTypes(types, mFilterOps.mFilterObjectTypes);
	mFilterOps.mFilterTypes |= FILTERTYPE_OBJECT;
}

void LLInventoryFilter::setFilterCategoryTypes(U64 types)
{
	updateFilterTypes(types, mFilterOps.mFilterCategoryTypes);
	mFilterOps.mFilterTypes |= FILTERTYPE_CATEGORY;
}

void LLInventoryFilter::setFilterWearableTypes(U64 types)
{
	updateFilterTypes(types, mFilterOps.mFilterWearableTypes);
	mFilterOps.mFilterTypes |= FILTERTYPE_WEARABLE;
}

void LLInventoryFilter::setFilterEmptySystemFolders()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_EMPTYFOLDERS;
}

void LLInventoryFilter::setFilterWornItems()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_WORN;
}

void LLInventoryFilter::setFilterMarketplaceActiveFolders()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_MARKETPLACE_ACTIVE;
}

void LLInventoryFilter::setFilterMarketplaceInactiveFolders()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_MARKETPLACE_INACTIVE;
}

void LLInventoryFilter::setFilterMarketplaceUnassociatedFolders()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_MARKETPLACE_UNASSOCIATED;
}

void LLInventoryFilter::setFilterMarketplaceListingFolders(bool select_only_listing_folders)
{
	if (select_only_listing_folders)
	{
		mFilterOps.mFilterTypes |= FILTERTYPE_MARKETPLACE_LISTING_FOLDER;
		setModified(FILTER_MORE_RESTRICTIVE);
	}
	else
	{
		mFilterOps.mFilterTypes &= ~FILTERTYPE_MARKETPLACE_LISTING_FOLDER;
		setModified(FILTER_LESS_RESTRICTIVE);
	}
}

void LLInventoryFilter::setFilterNoMarketplaceFolder()
{
	mFilterOps.mFilterTypes |= FILTERTYPE_NO_MARKETPLACE_ITEMS;
}

void LLInventoryFilter::setFilterUUID(const LLUUID& object_id)
{
	if (mFilterOps.mFilterUUID == object_id)
	{
		return;
	}
	if (mFilterOps.mFilterUUID == LLUUID::null)
	{
		setModified(FILTER_MORE_RESTRICTIVE);
	}
	else
	{
		setModified(FILTER_RESTART);
	}
	mFilterOps.mFilterUUID = object_id;
	if(object_id.isNull())
		mFilterOps.mFilterTypes &= ~FILTERTYPE_UUID;
	else
		mFilterOps.mFilterTypes |= FILTERTYPE_UUID;
}

void LLInventoryFilter::setFilterSubString(const std::string& string)
{
	std::string filter_sub_string_new = string;
	mFilterSubStringOrig = string;
	LLStringUtil::trimHead(filter_sub_string_new);
	LLStringUtil::toUpper(filter_sub_string_new);

	if (mFilterSubString != filter_sub_string_new)
	{
		// hitting BACKSPACE, for example
		const BOOL less_restrictive = mFilterSubString.size() >= filter_sub_string_new.size()
			&& !mFilterSubString.substr(0, filter_sub_string_new.size()).compare(filter_sub_string_new);

		// appending new characters
		const BOOL more_restrictive = mFilterSubString.size() < filter_sub_string_new.size()
			&& !filter_sub_string_new.substr(0, mFilterSubString.size()).compare(mFilterSubString);

		mFilterSubString = filter_sub_string_new;
		if (less_restrictive)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (more_restrictive)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else
		{
			setModified(FILTER_RESTART);
		}

		// Cancel out UUID once the search string is modified
		if (mFilterOps.mFilterTypes & FILTERTYPE_UUID)
		{
			mFilterOps.mFilterTypes &= ~FILTERTYPE_UUID;
			mFilterOps.mFilterUUID == LLUUID::null;
			setModified(FILTER_RESTART);
		}
	}
}

void LLInventoryFilter::setFilterPermissions(PermissionMask perms)
{
	if (mFilterOps.mPermissions != perms)
	{
		// keep current items only if no perm bits getting turned off
		BOOL fewer_bits_set = (mFilterOps.mPermissions & ~perms);
		BOOL more_bits_set = (~mFilterOps.mPermissions & perms);
		mFilterOps.mPermissions = perms;

		if (more_bits_set && fewer_bits_set)
		{
			setModified(FILTER_RESTART);
		}
		else if (more_bits_set)
		{
			// target must have all requested permission bits, so more bits == more restrictive
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else if (fewer_bits_set)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
	}
}

void LLInventoryFilter::setDateRange(time_t min_date, time_t max_date)
{
	mFilterOps.mHoursAgo = 0;
	if (mFilterOps.mMinDate != min_date)
	{
		mFilterOps.mMinDate = min_date;
		setModified();
	}
	if (mFilterOps.mMaxDate != llmax(mFilterOps.mMinDate, max_date))
	{
		mFilterOps.mMaxDate = llmax(mFilterOps.mMinDate, max_date);
		setModified();
	}

	if (areDateLimitsSet())
	{
		mFilterOps.mFilterTypes |= FILTERTYPE_DATE;
	}
	else
	{
		mFilterOps.mFilterTypes &= ~FILTERTYPE_DATE;
	}
}

void LLInventoryFilter::setDateRangeLastLogoff(BOOL sl)
{
	static LLCachedControl<U32> s_last_logoff(gSavedPerAccountSettings, "LastLogoff", 0);
	if (sl && !isSinceLogoff())
	{
		setDateRange(s_last_logoff, time_max());
		setModified();
	}
	if (!sl && isSinceLogoff())
	{
		setDateRange(time_min(), time_max());
		setModified();
	}

	if (areDateLimitsSet())
	{
		mFilterOps.mFilterTypes |= FILTERTYPE_DATE;
	}
	else
	{
		mFilterOps.mFilterTypes &= ~FILTERTYPE_DATE;
	}
}

bool LLInventoryFilter::isSinceLogoff() const
{
	static LLCachedControl<U32> s_last_logoff(gSavedSettings, "LastLogoff", 0);
	bool min_date = (mFilterOps.mMinDate == (time_t)s_last_logoff);
	bool max_date = (mFilterOps.mMaxDate == time_max());
	bool is_filter = (mFilterOps.mFilterTypes & FILTERTYPE_DATE);
	return min_date && max_date && is_filter;
}

void LLInventoryFilter::clearModified()
{
	mFilterModified = FILTER_NONE;
}

void LLInventoryFilter::setHoursAgo(U32 hours)
{
	if (mFilterOps.mHoursAgo != hours)
	{
		bool are_date_limits_valid = mFilterOps.mMinDate == time_min() && mFilterOps.mMaxDate == time_max();

		// *NOTE: need to cache last filter time, in case filter goes stale
		bool less_restrictive = false;
		bool more_restrictive = false;

		switch (mFilterOps.mDateSearchDirection)
		{
			case FILTERDATEDIRECTION_NEWER:
				less_restrictive = ((are_date_limits_valid && (hours > mFilterOps.mHoursAgo
															   && mFilterOps.mHoursAgo))
									|| !hours);
				
				more_restrictive = ((are_date_limits_valid && (hours < mFilterOps.mHoursAgo
															   && hours))
									|| (hours > mFilterOps.mHoursAgo
										&& !mFilterOps.mHoursAgo
										&& !isSinceLogoff()));
				break;
			case FILTERDATEDIRECTION_OLDER:
				less_restrictive = ((are_date_limits_valid && (hours < mFilterOps.mHoursAgo
															   && mFilterOps.mHoursAgo))
								 || !hours);
				
				more_restrictive = ((are_date_limits_valid && (hours > mFilterOps.mHoursAgo
															   && hours))
									|| (hours < mFilterOps.mHoursAgo
										&& !mFilterOps.mHoursAgo
										&& !isSinceLogoff()));
				break;
			default:
				break;
		}

		mFilterOps.mHoursAgo = hours;
		mFilterOps.mMinDate = time_min();
		mFilterOps.mMaxDate = time_max();
		if (less_restrictive)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (more_restrictive)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else
		{
			setModified(FILTER_RESTART);
		}
	}

	if (areDateLimitsSet())
	{
		mFilterOps.mFilterTypes |= FILTERTYPE_DATE;
	}
	else
	{
		mFilterOps.mFilterTypes &= ~FILTERTYPE_DATE;
	}
}

void LLInventoryFilter::setDateSearchDirection(U32 direction)
{
	if (direction != mFilterOps.mDateSearchDirection)
	{
		mFilterOps.mDateSearchDirection = direction;
		setModified(FILTER_RESTART);
	}
}

U32 LLInventoryFilter::getDateSearchDirection() const
{
	return mFilterOps.mDateSearchDirection;
}

void LLInventoryFilter::setFilterLinks(EFilterLink filter_links)
{
	if (mFilterOps.mFilterLinks != filter_links)
	{
		mFilterOps.mFilterLinks = filter_links;
		if (filter_links == FILTERLINK_EXCLUDE_LINKS ||
			filter_links == FILTERLINK_ONLY_LINKS)
			setModified(FILTER_MORE_RESTRICTIVE);
		else
			setModified(FILTER_LESS_RESTRICTIVE);
	}
}

void LLInventoryFilter::setShowFolderState(EFolderShow state)
{
	if (mFilterOps.mShowFolderState != state)
	{
		mFilterOps.mShowFolderState = state;
		if (state == SHOW_NON_EMPTY_FOLDERS)
		{
			// showing fewer folders than before
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else if (state == SHOW_ALL_FOLDERS)
		{
			// showing same folders as before and then some
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else
		{
			setModified();
		}
	}
}

void LLInventoryFilter::setSortOrder(U32 order)
{
	if (mOrder != order)
	{
		mOrder = order;
		setModified();
	}
}

void LLInventoryFilter::markDefault()
{
	mDefaultFilterOps = mFilterOps;
}

void LLInventoryFilter::resetDefault()
{
	mFilterOps = mDefaultFilterOps;
	setModified();
}

void LLInventoryFilter::setModified(EFilterModified behavior)
{
	mFilterText.clear();
	mCurrentGeneration++;
	
	if (mFilterModified == FILTER_NONE)
	{
		mFilterModified = behavior;
	}
	else if (mFilterModified != behavior)
	{
		// trying to do both less restrictive and more restrictive filter
		// basically means restart from scratch
		mFilterModified = FILTER_RESTART;
	}

	// if not keeping current filter results, update last valid as well
	switch(mFilterModified)
	{
	case FILTER_RESTART:
		mFirstRequiredGeneration = mCurrentGeneration;
		mFirstSuccessGeneration = mCurrentGeneration;
		break;
	case FILTER_LESS_RESTRICTIVE:
		mFirstRequiredGeneration = mCurrentGeneration;
		break;
	case FILTER_MORE_RESTRICTIVE:
		mFirstSuccessGeneration = mCurrentGeneration;
		break;
	default:
		LL_ERRS() << "Bad filter behavior specified" << LL_ENDL;
	}
}

bool LLInventoryFilter::isFilterObjectTypesWith(LLInventoryType::EType t) const
{
	return mFilterOps.mFilterObjectTypes & (1LL << t);
}

const std::string& LLInventoryFilter::getFilterText()
{
	if (!mFilterText.empty())
	{
		return mFilterText;
	}

	std::string filtered_types;
	std::string not_filtered_types;
	BOOL filtered_by_type = FALSE;
	BOOL filtered_by_all_types = TRUE;
	S32 num_filter_types = 0;

	mFilterText.clear();

	if (isFilterObjectTypesWith(LLInventoryType::IT_ANIMATION))
	{
		filtered_types += LLTrans::getString("Animations");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types += LLTrans::getString("Animations");

		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_CALLINGCARD))
	{
		filtered_types += LLTrans::getString("Calling Cards");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types += LLTrans::getString("Calling Cards");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_WEARABLE))
	{
		filtered_types +=  LLTrans::getString("Clothing");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Clothing");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_GESTURE))
	{
		filtered_types +=  LLTrans::getString("Gestures");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Gestures");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_LANDMARK))
	{
		filtered_types +=  LLTrans::getString("Landmarks");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Landmarks");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_NOTECARD))
	{
		filtered_types +=  LLTrans::getString("Notecards");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Notecards");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_OBJECT) && isFilterObjectTypesWith(LLInventoryType::IT_ATTACHMENT))
	{
		filtered_types +=  LLTrans::getString("Objects");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Objects");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_LSL))
	{
		filtered_types +=  LLTrans::getString("Scripts");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Scripts");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_SOUND))
	{
		filtered_types +=  LLTrans::getString("Sounds");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Sounds");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_TEXTURE))
	{
		filtered_types +=  LLTrans::getString("Textures");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Textures");
		filtered_by_all_types = FALSE;
	}

	if (isFilterObjectTypesWith(LLInventoryType::IT_SNAPSHOT))
	{
		filtered_types +=  LLTrans::getString("Snapshots");
		filtered_by_type = TRUE;
		num_filter_types++;
	}
	else
	{
		not_filtered_types +=  LLTrans::getString("Snapshots");
		filtered_by_all_types = FALSE;
	}

	if (!LLInventoryModelBackgroundFetch::instance().folderFetchActive()
		&& filtered_by_type
		&& !filtered_by_all_types)
	{
		mFilterText += " - ";
		if (num_filter_types < 5)
		{
			mFilterText += filtered_types;
		}
		else
		{
			mFilterText += LLTrans::getString("No Filters");
			mFilterText += not_filtered_types;
		}
		// remove the ',' at the end
		mFilterText.erase(mFilterText.size() - 1, 1);
	}

	if (isSinceLogoff())
	{
		mFilterText += LLTrans::getString("Since Logoff");
	}

	if (getFilterWornItems())
	{
		mFilterText += LLTrans::getString("Worn");
	}

	return mFilterText;
}

LLInventoryFilter& LLInventoryFilter::operator=( const  LLInventoryFilter&  other )
{
	setFilterObjectTypes(other.getFilterObjectTypes());
	setDateRange(other.getMinDate(), other.getMaxDate());
	setHoursAgo(other.getHoursAgo());
	setDateSearchDirection(other.getDateSearchDirection());
	setShowFolderState(other.getShowFolderState());
	setFilterPermissions(other.getFilterPermissions());
	setFilterSubString(other.getFilterSubString());
	setDateRangeLastLogoff(other.isSinceLogoff());
	return *this;
}


void LLInventoryFilter::toParams(Params& params) const
{
	params.filter_ops.types = getFilterObjectTypes();
	params.filter_ops.category_types = getFilterCategoryTypes();
	if (getFilterObjectTypes() & FILTERTYPE_WEARABLE)
	{
		params.filter_ops.wearable_types = getFilterWearableTypes();
	}
	params.filter_ops.date_range.min_date = getMinDate();
	params.filter_ops.date_range.max_date = getMaxDate();
	params.filter_ops.hours_ago = getHoursAgo();
	params.filter_ops.date_search_direction = getDateSearchDirection();
	params.filter_ops.show_folder_state = getShowFolderState();
	params.filter_ops.permissions = getFilterPermissions();
	params.substring = getFilterSubString();
	params.since_logoff = isSinceLogoff();
}

void LLInventoryFilter::fromParams(const Params& params)
{
	if (!params.validateBlock())
	{
		return;
	}

	setFilterObjectTypes(params.filter_ops.types);
	setFilterCategoryTypes(params.filter_ops.category_types);
	if (params.filter_ops.wearable_types.isProvided())
	{
		setFilterWearableTypes(params.filter_ops.wearable_types);
	}

	setDateRange(params.filter_ops.date_range.min_date,   params.filter_ops.date_range.max_date);
	setHoursAgo(params.filter_ops.hours_ago);
	setDateSearchDirection(params.filter_ops.date_search_direction);
	setShowFolderState(params.filter_ops.show_folder_state);
	setFilterPermissions(params.filter_ops.permissions);
	setFilterSubString(params.substring);
	setDateRangeLastLogoff(params.since_logoff);
	if (params.filter_ops.links.isProvided())
	{
		setFilterLinks(params.filter_ops.links);
	}
}

U64 LLInventoryFilter::getFilterTypes() const
{
	return mFilterOps.mFilterTypes;
}

U64 LLInventoryFilter::getFilterObjectTypes() const
{
	return mFilterOps.mFilterObjectTypes;
}

U64 LLInventoryFilter::getFilterCategoryTypes() const
{
	return mFilterOps.mFilterCategoryTypes;
}

U64 LLInventoryFilter::getFilterWearableTypes() const
{
	return mFilterOps.mFilterWearableTypes;
}

U64 LLInventoryFilter::getFilterWornItems() const
{
	return mFilterOps.mFilterTypes & FILTERTYPE_WORN;
}

bool LLInventoryFilter::hasFilterString() const
{
	return mFilterSubString.size() > 0;
}

std::string::size_type LLInventoryFilter::getFilterStringSize() const
{
	return mFilterSubString.size();
}

PermissionMask LLInventoryFilter::getFilterPermissions() const
{
	return mFilterOps.mPermissions;
}

time_t LLInventoryFilter::getMinDate() const
{
	return mFilterOps.mMinDate;
}

time_t LLInventoryFilter::getMaxDate() const 
{ 
	return mFilterOps.mMaxDate; 
}
U32 LLInventoryFilter::getHoursAgo() const 
{ 
	return mFilterOps.mHoursAgo; 
}
LLInventoryFilter::EFilterLink LLInventoryFilter::getFilterLinks() const
{
	return mFilterOps.mFilterLinks;
}
LLInventoryFilter::EFolderShow LLInventoryFilter::getShowFolderState() const
{ 
	return mFilterOps.mShowFolderState; 
}
U32 LLInventoryFilter::getSortOrder() const 
{ 
	return mOrder; 
}

void LLInventoryFilter::setFilterCount(S32 count) 
{ 
	mFilterCount = count; 
}
S32 LLInventoryFilter::getFilterCount() const
{
	return mFilterCount;
}

void LLInventoryFilter::decrementFilterCount() 
{ 
	mFilterCount--; 
}

S32 LLInventoryFilter::getCurrentGeneration() const 
{ 
	return mCurrentGeneration; 
}
S32 LLInventoryFilter::getFirstSuccessGeneration() const 
{ 
	return mFirstSuccessGeneration; 
}
S32 LLInventoryFilter::getFirstRequiredGeneration() const 
{ 
	return mFirstRequiredGeneration; 
}

void LLInventoryFilter::setEmptyLookupMessage(const std::string& message)
{
	mEmptyLookupMessage = message;
}

std::string LLInventoryFilter::getEmptyLookupMessage() const
{
	LLStringUtil::format_map_t args;
	args["[SEARCH_TERM]"] = LLURI::escape(getFilterSubStringOrig());

	return LLTrans::getString(mEmptyLookupMessage, args);

}

bool LLInventoryFilter::areDateLimitsSet()
{
	return     mFilterOps.mMinDate != time_min()
			|| mFilterOps.mMaxDate != time_max()
			|| mFilterOps.mHoursAgo != 0;
}

bool LLInventoryFilter::showAllResults() const
{
	return hasFilterString();
}



bool LLInventoryFilter::FilterOps::DateRange::validateBlock( bool   emit_errors /*= true*/ ) const
{
	bool valid = LLInitParam::Block<DateRange>::validateBlock(emit_errors);
	if (valid)
	{
		if (max_date() < min_date())
		{
			if (emit_errors)
			{
				LL_WARNS() << "max_date should be greater or equal to min_date" <<   LL_ENDL;
			}
			valid = false;
		}
	}
	return valid;
}
