/** 
 * @file llviewerprecompiledheaders.h
 * @brief precompiled headers for newview project
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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


#ifndef LL_LLVIEWERPRECOMPILEDHEADERS_H
#define LL_LLVIEWERPRECOMPILEDHEADERS_H

// This file MUST be the first one included by each .cpp file
// in viewer.
// It is used to precompile headers for improved build speed.

#include "linden_common.h"

#include "llwin32headerslean.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <set>

#ifdef LL_WINDOWS
#pragma warning (3 : 4702) // we like level 3, not 4
#endif

// Library headers from llcommon project:
#include "bitpack.h"
#include "imageids.h"
#include "indra_constants.h"
#include "llinitparam.h"
#include "llapp.h"
#include "llapr.h"
#include "llcriticaldamp.h"
#include "lldefs.h"
#include "lldepthstack.h"
#include "llendianswizzle.h"
#include "llerror.h"
#include "llfasttimer.h"
#include "llframetimer.h"
#include "lllocalidhashmap.h"
#include "llmap.h"
#include "llmemory.h"
#include "llnametable.h"
#include "llpriqueuemap.h"
#include "llprocessor.h"
#include "llstat.h"
#include "llstl.h"
#include "llstring.h"
#include "llstringtable.h"
#include "llsys.h"
#include "llthread.h"
#include "lltimer.h"
#include "stdenums.h"
#include "stdtypes.h"
#include "timing.h"
#include "u64.h"

// Library includes from llmath project
#include "llmath.h"
#include "llbboxlocal.h"
#include "llcamera.h"
#include "llcoord.h"
#include "llcoordframe.h"
#include "llcrc.h"
#include "llplane.h"
#include "llquantize.h"
#include "llrand.h"
#include "llrect.h"
#include "lluuid.h"
#include "m3math.h"
#include "m4math.h"
#include "llquaternion.h"
#include "v2math.h"
#include "v3color.h"
#include "v3dmath.h"
#include "v3math.h"
#include "v4color.h"
#include "v4coloru.h"
#include "v4math.h"
#include "xform.h"

// Library includes from llmessage project
#include "llcachename.h"
#include "llcircuit.h"
#include "lldatapacker.h"
#include "lldbstrings.h"
#include "lldispatcher.h"
#include "lleventflags.h"
#include "llhost.h"
#include "llinstantmessage.h"
#include "llinvite.h"
#include "llmessagethrottle.h"
#include "llnamevalue.h"
#include "llpacketack.h"
#include "llpacketbuffer.h"
#include "llpartdata.h"
#include "llregionhandle.h"
#include "lltaskname.h"
#include "llteleportflags.h"
#include "llthrottle.h"
#include "lltransfermanager.h"
#include "lltransfersourceasset.h"
#include "lltransfersourcefile.h"
#include "lltransfertargetfile.h"
#include "lltransfertargetvfile.h"
#include "lluseroperation.h"
#include "llvehicleparams.h"
#include "llxfer.h"
#include "llxfer_file.h"
#include "llxfer_mem.h"
#include "llxfer_vfile.h"
#include "llxfermanager.h"
#include "machine.h"
#include "mean_collision_data.h"
#include "message.h"
#include "message_prehash.h"
#include "net.h"
#include "patch_code.h"
#include "patch_dct.h"
#include "sound_ids.h"

// Library includes from llprimitive
#include "imageids.h"
#include "legacy_object_types.h"
#include "llmaterialtable.h"
#include "lltextureanim.h"
#include "lltreeparams.h"
#include "material_codes.h"

// Library includes from llxml
#include "llxmlnode.h"

// Library includes from llvfs
#include "llassettype.h"
#include "lldir.h"
#include "llvfile.h"
#include "llvfs.h"

#endif
