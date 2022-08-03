/**
 * @file llfastset.h
 *
 * $LicenseInfo:firstyear=2020&license=viewergpl$
 *
 * Copyright (c) 2020, Henri Beauchamp.
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

#ifndef LL_LLFASTSET_H
#define LL_LLFASTSET_H

// safe_hset, fast_hset and flat_hset are the macros to use (for example, in
// place of boost::unordered_set or boost::container::flat_set) for unordered
// sets you wish to (potentially) speed up.
// safe_hset is guaranteed not to invalidate all the map iterators on erase()
// of one of its elements and to preserve pointers.

// The hset_erase #define is provided for a minor optimization with phmap
// containers, that may call a special _erase() method, that does not return
// an iterator (unlike erase()) and is therefore slightly faster. It is only
// valid when passed an iterator (const or not).

#if LL_PHMAP
# include "parallel_hashmap/phmap.h"
# define safe_hset phmap::node_hash_set
# define fast_hset phmap::flat_hash_set
# define flat_hset phmap::flat_hash_set
# define hset_erase(it) _erase(it)
#else
# include "boost/unordered_set.hpp"
# include "boost/container/flat_set.hpp"
# define safe_hset boost::unordered_set
# define fast_hset boost::unordered_set
# define flat_hset boost::container::flat_set
# define hset_erase(it) erase(it)
#endif

#endif	// LL_LLFASTSET_H
