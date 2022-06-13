/** 
 * @file llwearabledata.h
 * @brief LLWearableData class header file
 *
 * $LicenseInfo:firstyear=20012license=viewerlgpl$
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

#ifndef LL_WEARABLEDATA_H
#define LL_WEARABLEDATA_H

#include "llavatarappearancedefines.h"
#include "llwearable.h"
#include "llerror.h"
#include <boost/array.hpp>

class LLAvatarAppearance;

class LLWearableData
{
	// *TODO: Figure out why this is causing compile error.
	//LOG_CLASS(LLWearableData);

	//--------------------------------------------------------------------
	// Constructors / destructors / Initializers
	//--------------------------------------------------------------------
public:
	LLWearableData();
	virtual ~LLWearableData();

	void setAvatarAppearance(LLAvatarAppearance* appearance) { mAvatarAppearance = appearance; }

protected:
	//--------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------
public:
	LLWearable*			getWearable(const LLWearableType::EType type, U32 index /*= 0*/); 
	const LLWearable* 	getWearable(const LLWearableType::EType type, U32 index /*= 0*/) const;
	LLWearable*			getTopWearable(const LLWearableType::EType type);
	const LLWearable*	getTopWearable(const LLWearableType::EType type) const;
	LLWearable*			getBottomWearable(const LLWearableType::EType type);
	const LLWearable*	getBottomWearable(const LLWearableType::EType type) const;
	U32				getWearableCount(const LLWearableType::EType type) const;
	U32				getWearableCount(const U32 tex_index) const;
	BOOL			getWearableIndex(const LLWearable *wearable, U32& index) const;
	U32				getClothingLayerCount() const;
	BOOL			canAddWearable(const LLWearableType::EType type) const;

	BOOL			isOnTop(LLWearable* wearable) const;
	
	static const U32 MAX_CLOTHING_LAYERS = 60;

	//--------------------------------------------------------------------
	// Setters
	//--------------------------------------------------------------------
protected:
	// Low-level data structure setter - public access is via setWearableItem, etc.
	void 			setWearable(const LLWearableType::EType type, U32 index, LLWearable *wearable);
	void 			pushWearable(const LLWearableType::EType type, LLWearable *wearable, 
								 bool trigger_updated = true);
	virtual void	wearableUpdated(LLWearable *wearable, BOOL removed);
	void 			eraseWearable(LLWearable *wearable);
	void			eraseWearable(const LLWearableType::EType type, U32 index);
	void			clearWearableType(const LLWearableType::EType type);
	bool			swapWearables(const LLWearableType::EType type, U32 index_a, U32 index_b);

private:
	void			pullCrossWearableValues(const LLWearableType::EType type);

	//--------------------------------------------------------------------
	// Server Communication
	//--------------------------------------------------------------------
public:
	LLUUID			computeBakedTextureHash(LLAvatarAppearanceDefines::EBakedTextureIndex baked_index,
											BOOL generate_valid_hash = TRUE);
protected:
	virtual void	invalidateBakedTextureHash(LLMD5& hash) const {}

	//--------------------------------------------------------------------
	// Member variables
	//--------------------------------------------------------------------
protected:
	LLAvatarAppearance* mAvatarAppearance;
	typedef std::vector<LLWearable*> wearableentry_vec_t; // all wearables of a certain type (EG all shirts)
	//typedef std::map<LLWearableType::EType, wearableentry_vec_t> wearableentry_map_t;	// wearable "categories" arranged by wearable type
	
	//Why this weird structure? LLWearableType::WT_COUNT small and known, therefore it's more efficient to make an array of vectors, indexed
	//by wearable type. This allows O(1) lookups. This structure simply lets us plug in this optimization without touching any code elsewhere.
	typedef std::array<std::pair<LLWearableType::EType, wearableentry_vec_t>, LLWearableType::WT_COUNT> wearable_array_t;
	struct wearableentry_map_t : public wearable_array_t
	{
		wearableentry_map_t()
		{
			for(wearable_array_t::size_type i=0;i<size();++i)
				at(i).first = (LLWearableType::EType)i;
		}
		wearable_array_t::iterator find(const LLWearableType::EType& index)
		{
			if(index < 0 || index >= (S32)size())
				return end();
			return begin() + index;
		}
		wearable_array_t::const_iterator find(const LLWearableType::EType& index) const
		{
			if(index < 0 || index >= (S32)size())
				return end();
			return begin() + index;
		}
		wearableentry_vec_t&       operator []	(const S32 index)		{ return at(index).second; }
		const wearableentry_vec_t& operator []	(const S32 index) const	{ return at(index).second; }
	};
	wearableentry_map_t mWearableDatas;	//Array for quicker lookups.

};



#endif // LL_WEARABLEDATA_H