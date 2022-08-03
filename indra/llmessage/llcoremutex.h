/**
 * @file llcoremutex.h
 * @brief mutex type abstraction
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#ifndef LLCOREINT_MUTEX_H_
#define LLCOREINT_MUTEX_H_

// For LL_MUTEX_TYPE, LL_COND_TYPE and LL_UNIQ_LOCK_TYPE:
#include "llmutex.h"

namespace LLCoreInt
{
// MUTEX TYPES

// Unique mutex type
typedef LL_MUTEX_TYPE HttpMutex;

// CONDITION VARIABLES

// Standard condition variable
typedef LL_COND_TYPE HttpConditionVariable;

// LOCKS AND FENCES

// Scoped unique lock
typedef LL_UNIQ_LOCK_TYPE HttpScopedLock;
}

#endif	// LLCOREINT_MUTEX_H
