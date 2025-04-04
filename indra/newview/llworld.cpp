/** 
 * @file llworld.cpp
 * @brief Initial test structure to organize viewer regions
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

#include "llworld.h"
#include "llrender.h"

#include "indra_constants.h"
#include "llstl.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "lldrawpool.h"
#include "llglheaders.h"
#include "llhttpnode.h"
#include "llregionhandle.h"
#include "llsurface.h"
#include "lltrans.h"
#include "llviewercamera.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "llviewernetwork.h"
#include "llviewerobjectlist.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llvlcomposition.h"
#include "llvoavatar.h"
#include "llvocache.h"
#include "llvowater.h"
#include "message.h"
#include "pipeline.h"
#include "llappviewer.h"		// for do_disconnect()
#include "llpacketring.h"
#include "hippogridmanager.h"

#include <deque>
#include <queue>
#include <map>
#include <cstring>


//
// Globals
//
U32			gAgentPauseSerialNum = 0;

//
// Constants
//
const S32 MAX_NUMBER_OF_CLOUDS	= 750;
const S32 WORLD_PATCH_SIZE = 16;

extern LLColor4U MAX_WATER_COLOR;

// <FS:CR> Aurora Sim
//const U32 LLWorld::mWidth = 256;
U32 LLWorld::mWidth = 256;
U32 LLWorld::mLength = 256;
// </FS:CR> Aurora Sim

// meters/point, therefore mWidth * mScale = meters per edge
const F32 LLWorld::mScale = 1.f;

// <FS:CR> Aurora Sim
//const F32 LLWorld::mWidthInMeters = mWidth * mScale;
F32 LLWorld::mWidthInMeters = mWidth * mScale;
// </FS:CR> Aurora Sim

//
// Functions
//

// allocate the stack
LLWorld::LLWorld() :
	mLandFarClip(DEFAULT_FAR_PLANE),
	mLastPacketsIn(0),
	mLastPacketsOut(0),
	mLastPacketsLost(0),
	mSpaceTimeUSec(0)
{
	for (S32 i = 0; i < 8; i++)
	{
		mEdgeWaterObjects[i] = NULL;
	}

	if (gNoRender)
	{
		return;
	}

	LLPointer<LLImageRaw> raw = new LLImageRaw(1,1,4);
	U8 *default_texture = raw->getData();
	*(default_texture++) = MAX_WATER_COLOR.mV[0];
	*(default_texture++) = MAX_WATER_COLOR.mV[1];
	*(default_texture++) = MAX_WATER_COLOR.mV[2];
	*(default_texture++) = MAX_WATER_COLOR.mV[3];
	
	mDefaultWaterTexturep = LLViewerTextureManager::getLocalTexture(raw.get(), FALSE);
	gGL.getTexUnit(0)->bind(mDefaultWaterTexturep);
	mDefaultWaterTexturep->setAddressMode(LLTexUnit::TAM_CLAMP);

}


void LLWorld::destroyClass()
{
	mHoleWaterObjects.clear();
	gObjectList.destroy();
	for(region_list_t::iterator region_it = mRegionList.begin(), region_it_end(mRegionList.end()); region_it != region_it_end; )
	{
		LLViewerRegion* region_to_delete = *region_it++;
		removeRegion(region_to_delete->getHost());
	}

	if(LLVOCache::hasInstance())
	{
		LLVOCache::getInstance()->destroyClass() ;
	}
	LLViewerPartSim::getInstance()->destroyClass();

	mDefaultWaterTexturep = NULL ;
	for (S32 i = 0; i < 8; i++)
	{
		mEdgeWaterObjects[i] = NULL;
	}

	//make all visible drawbles invisible.
	LLDrawable::incrementVisible();
	
}

void LLWorld::setRegionSize(const U32& width, const U32& length)
{
	mWidth = width ? width : 256; // Width of 0 is really 256
	mLength = length ? length : 256; // Length of 0 is really 256
	mWidthInMeters = mWidth * mScale;
}


LLViewerRegion* LLWorld::addRegion(const U64 &region_handle, const LLHost &host)
{
	LL_INFOS() << "Add region with handle: " << region_handle << " on host " << host << LL_ENDL;
	LLViewerRegion *regionp = getRegionFromHandle(region_handle);
	std::string seedUrl;
	if (regionp)
	{
		LLHost old_host = regionp->getHost();
		// region already exists!
		if (host == old_host && regionp->isAlive())
		{
			// This is a duplicate for the same host and it's alive, don't bother.
			LL_INFOS() << "Region already exists and is alive, using existing region" << LL_ENDL;
			return regionp;
		}

		if (host != old_host)
		{
			LL_WARNS() << "LLWorld::addRegion exists, but old host " << old_host
					<< " does not match new host " << host
					<< ", removing old region and creating new" << LL_ENDL;
		}
		if (!regionp->isAlive())
		{
			LL_WARNS() << "LLWorld::addRegion exists, but isn't alive. Removing old region and creating new" << LL_ENDL;
		}

		// Save capabilities seed URL
		seedUrl = regionp->getCapability("Seed");

		// Kill the old host, and then we can continue on and add the new host.  We have to kill even if the host
		// matches, because all the agent state for the new camera is completely different.
		removeRegion(old_host);
	}
	else
	{
		LL_INFOS() << "Region does not exist, creating new one" << LL_ENDL;
	}

	U32 iindex = 0;
	U32 jindex = 0;
	from_region_handle(region_handle, &iindex, &jindex);
// <FS:CR> Aurora Sim
	//S32 x = (S32)(iindex/mWidth);
	//S32 y = (S32)(jindex/mWidth);
	S32 x = (S32)(iindex/256); //MegaRegion
	S32 y = (S32)(jindex/256); //MegaRegion
// </FS:CR> Aurora Sim
	LL_INFOS() << "Adding new region (" << x << ":" << y << ")"
		<< " on host: " << host << LL_ENDL;

	LLVector3d origin_global;

	origin_global = from_region_handle(region_handle);

	regionp = new LLViewerRegion(region_handle,
								    host,
									mWidth,
									WORLD_PATCH_SIZE,
									getRegionWidthInMeters() );
	if (!regionp)
	{
		LL_ERRS() << "Unable to create new region!" << LL_ENDL;
	}

	if ( !seedUrl.empty() )
	{
		regionp->setCapability("Seed", seedUrl);
	}

	//Classic clouds
#if ENABLE_CLASSIC_CLOUDS
	regionp->mCloudLayer.create(regionp);
	regionp->mCloudLayer.setWidth((F32)mWidth);
	regionp->mCloudLayer.setWindPointer(&regionp->mWind);
#endif

	mRegionList.push_back(regionp);
	mActiveRegionList.push_back(regionp);
	mCulledRegionList.push_back(regionp);


	// Find all the adjacent regions, and attach them.
	// Generate handles for all of the adjacent regions, and attach them in the correct way.
	// connect the edges
	F32 adj_x = 0.f;
	F32 adj_y = 0.f;
	F32 region_x = 0.f;
	F32 region_y = 0.f;
	U64 adj_handle = 0;

	F32 width = getRegionWidthInMeters();

	LLViewerRegion *neighborp;
// <FS:CR> Aurora Sim
	LLViewerRegion *last_neighborp;
// </FS:CR> Aurora Sim
	from_region_handle(region_handle, &region_x, &region_y);

	// Iterate through all directions, and connect neighbors if there.
	S32 dir;
	for (dir = 0; dir < 8; dir++)
	{
// <FS:CR> Aurora Sim
		last_neighborp = NULL;
// </FS:CR> Aurora Sim
		adj_x = region_x + width * gDirAxes[dir][0];
		adj_y = region_y + width * gDirAxes[dir][1];
// <FS:CR> Aurora Sim
		//to_region_handle(adj_x, adj_y, &adj_handle);
		if(gDirAxes[dir][0] < 0) adj_x = region_x - WORLD_PATCH_SIZE;
		if(gDirAxes[dir][1] < 0) adj_y = region_y - WORLD_PATCH_SIZE;

		for(S32 offset = 0; offset < width; offset += WORLD_PATCH_SIZE)
		{
			to_region_handle(adj_x, adj_y, &adj_handle);
			neighborp = getRegionFromHandle(adj_handle);
			
			if (neighborp && last_neighborp != neighborp)
			{
				//LL_INFOS() << "Connecting " << region_x << ":" << region_y << " -> " << adj_x << ":" << adj_y << LL_ENDL;
				regionp->connectNeighbor(neighborp, dir);
				last_neighborp = neighborp;
			}

			if(dir == NORTHEAST ||
			   dir == NORTHWEST ||
			   dir == SOUTHWEST ||
			   dir == SOUTHEAST)
			{
				break;
			}

			if(dir == NORTH || dir == SOUTH) adj_x += WORLD_PATCH_SIZE;
			if(dir == EAST || dir == WEST) adj_y += WORLD_PATCH_SIZE;
// </FS:CR> Aurora Sim
		}
	}

	updateWaterObjects();

	return regionp;
}


void LLWorld::removeRegion(const LLHost &host)
{
	F32 x, y;

	LLViewerRegion *regionp = getRegion(host);
	if (!regionp)
	{
		LL_WARNS() << "Trying to remove region that doesn't exist!" << LL_ENDL;
		return;
	}
	
	if (regionp == gAgent.getRegion())
	{
		for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
			 iter != iter_end; ++iter)
		{
			LLViewerRegion* reg = *iter;
			LL_WARNS() << "RegionDump: " << reg->getName()
				<< " " << reg->getHost()
				<< " " << reg->getOriginGlobal()
				<< LL_ENDL;
		}

		LL_WARNS() << "Agent position global " << gAgent.getPositionGlobal() 
			<< " agent " << gAgent.getPositionAgent()
			<< LL_ENDL;

		LL_WARNS() << "Regions visited " << gAgent.getRegionsVisited() << LL_ENDL;

		LL_WARNS() << "gFrameTimeSeconds " << gFrameTimeSeconds << LL_ENDL;

		LL_WARNS() << "Disabling region " << regionp->getName() << " that agent is in!" << LL_ENDL;
		LLAppViewer::instance()->forceDisconnect(LLTrans::getString("YouHaveBeenDisconnected"));

		regionp->saveObjectCache() ; //force to save objects here in case that the object cache is about to be destroyed.
		return;
	}

	from_region_handle(regionp->getHandle(), &x, &y);
	LL_INFOS() << "Removing region " << x << ":" << y << LL_ENDL;

	mRegionList.remove(regionp);
	mActiveRegionList.remove(regionp);
	mCulledRegionList.remove(regionp);
	mVisibleRegionList.remove(regionp);

	mRegionRemovedSignal(regionp);

	updateWaterObjects();

	//double check all objects of this region are removed.
	gObjectList.clearAllMapObjectsInRegion(regionp) ;
	//llassert_always(!gObjectList.hasMapObjectInRegion(regionp)) ;

	delete regionp; // <alchemy/> - Use after free fix?
}


LLViewerRegion* LLWorld::getRegion(const LLHost &host)
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->getHost() == host)
		{
			return regionp;
		}
	}
	return NULL;
}

LLViewerRegion* LLWorld::getRegionFromPosAgent(const LLVector3 &pos)
{
	return getRegionFromPosGlobal(gAgent.getPosGlobalFromAgent(pos));
}

LLViewerRegion* LLWorld::getRegionFromPosGlobal(const LLVector3d &pos)
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end = mRegionList.end();
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->pointInRegionGlobal(pos))
		{
			return regionp;
		}
	}
	return NULL;
}


LLVector3d	LLWorld::clipToVisibleRegions(const LLVector3d &start_pos, const LLVector3d &end_pos)
{
	if (positionRegionValidGlobal(end_pos))
	{
		return end_pos;
	}

	LLViewerRegion* regionp = getRegionFromPosGlobal(start_pos);
	if (!regionp) 
	{
		return start_pos;
	}

	LLVector3d delta_pos = end_pos - start_pos;
	LLVector3d delta_pos_abs;
	delta_pos_abs.setVec(delta_pos);
	delta_pos_abs.abs();

	LLVector3 region_coord = regionp->getPosRegionFromGlobal(end_pos);
	F64 clip_factor = 1.0;
	F32 region_width = regionp->getWidth();
	if (region_coord.mV[VX] < 0.f)
	{
		if (region_coord.mV[VY] < region_coord.mV[VX])
		{
			// clip along y -
			clip_factor = -(region_coord.mV[VY] / delta_pos_abs.mdV[VY]);
		}
		else
		{
			// clip along x -
			clip_factor = -(region_coord.mV[VX] / delta_pos_abs.mdV[VX]);
		}
	}
	else if (region_coord.mV[VX] > region_width)
	{
		if (region_coord.mV[VY] > region_coord.mV[VX])
		{
			// clip along y +
			clip_factor = (region_coord.mV[VY] - region_width) / delta_pos_abs.mdV[VY];
		}
		else
		{
			//clip along x +
			clip_factor = (region_coord.mV[VX] - region_width) / delta_pos_abs.mdV[VX];
		}
	}
	else if (region_coord.mV[VY] < 0.f)
	{
		// clip along y -
		clip_factor = -(region_coord.mV[VY] / delta_pos_abs.mdV[VY]);
	}
	else if (region_coord.mV[VY] > region_width)
	{ 
		// clip along y +
		clip_factor = (region_coord.mV[VY] - region_width) / delta_pos_abs.mdV[VY];
	}

	// clamp to within region dimensions
	LLVector3d final_region_pos = LLVector3d(region_coord) - (delta_pos * clip_factor);
	final_region_pos.mdV[VX] = llclamp(final_region_pos.mdV[VX], 0.0,
									   (F64)(region_width - F_ALMOST_ZERO));
	final_region_pos.mdV[VY] = llclamp(final_region_pos.mdV[VY], 0.0,
									   (F64)(region_width - F_ALMOST_ZERO));
	final_region_pos.mdV[VZ] = llclamp(final_region_pos.mdV[VZ], 0.0,
									   (F64)(LLWorld::getInstance()->getRegionMaxHeight() - F_ALMOST_ZERO));
	return regionp->getPosGlobalFromRegion(LLVector3(final_region_pos));
}

LLViewerRegion* LLWorld::getRegionFromHandle(const U64 &handle)
{
// <FS:CR> Aurora Sim
	U32 x, y;
	from_region_handle(handle, &x, &y);
// </FS:CR> Aurora Sim

	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
// <FS:CR> Aurora Sim
		//if (regionp->getHandle() == handle)
		U32 checkRegionX, checkRegionY;
		F32 checkRegionWidth = regionp->getWidth();
		from_region_handle(regionp->getHandle(), &checkRegionX, &checkRegionY);

		if (x >= checkRegionX && x < (checkRegionX + checkRegionWidth) &&
			y >= checkRegionY && y < (checkRegionY + checkRegionWidth))
// <FS:CR> Aurora Sim
		{
			return regionp;
		}
	}
	return NULL;
}

LLViewerRegion* LLWorld::getRegionFromID(const LLUUID& region_id)
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->getRegionID() == region_id)
		{
			return regionp;
		}
	}
	return NULL;
}

void LLWorld::updateAgentOffset(const LLVector3d &offset_global)
{
#if 0
	for (region_list_t::iterator iter = mRegionList.begin();
		 iter != mRegionList.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->setAgentOffset(offset_global);
	}
#endif
}


BOOL LLWorld::positionRegionValidGlobal(const LLVector3d &pos_global)
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp->pointInRegionGlobal(pos_global))
		{
			return TRUE;
		}
	}
	return FALSE;
}


// Allow objects to go up to their radius underground.
F32 LLWorld::getMinAllowedZ(LLViewerObject* object, const LLVector3d &global_pos)
{
	F32 land_height = resolveLandHeightGlobal(global_pos);
	F32 radius = 0.5f * object->getScale().length();
	return land_height - radius;
}


LLViewerRegion* LLWorld::resolveRegionGlobal(LLVector3 &pos_region, const LLVector3d &pos_global)
{
	LLViewerRegion *regionp = getRegionFromPosGlobal(pos_global);

	if (regionp)
	{
		pos_region = regionp->getPosRegionFromGlobal(pos_global);
		return regionp;
	}

	return NULL;
}


LLViewerRegion* LLWorld::resolveRegionAgent(LLVector3 &pos_region, const LLVector3 &pos_agent)
{
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	LLViewerRegion *regionp = getRegionFromPosGlobal(pos_global);

	if (regionp)
	{
		pos_region = regionp->getPosRegionFromGlobal(pos_global);
		return regionp;
	}

	return NULL;
}


F32 LLWorld::resolveLandHeightAgent(const LLVector3 &pos_agent)
{
	LLVector3d pos_global = gAgent.getPosGlobalFromAgent(pos_agent);
	return resolveLandHeightGlobal(pos_global);
}


F32 LLWorld::resolveLandHeightGlobal(const LLVector3d &pos_global)
{
	LLViewerRegion *regionp = getRegionFromPosGlobal(pos_global);
	if (regionp)
	{
		return regionp->getLand().resolveHeightGlobal(pos_global);
	}
	return 0.0f;
}


// Takes a line defined by "point_a" and "point_b" and determines the closest (to point_a) 
// point where the the line intersects an object or the land surface.  Stores the results
// in "intersection" and "intersection_normal" and returns a scalar value that represents
// the normalized distance along the line from "point_a" to "intersection".
//
// Currently assumes point_a and point_b only differ in z-direction, 
// but it may eventually become more general.
F32 LLWorld::resolveStepHeightGlobal(const LLVOAvatar* avatarp, const LLVector3d &point_a, const LLVector3d &point_b, 
							   LLVector3d &intersection, LLVector3 &intersection_normal,
							   LLViewerObject **viewerObjectPtr)
{
	// initialize return value to null
	if (viewerObjectPtr)
	{
		*viewerObjectPtr = NULL;
	}

	LLViewerRegion *regionp = getRegionFromPosGlobal(point_a);
	if (!regionp)
	{
		// We're outside the world 
		intersection = 0.5f * (point_a + point_b);
		intersection_normal.setVec(0.0f, 0.0f, 1.0f);
		return 0.5f;
	}
	
	// calculate the length of the segment
	F32 segment_length = (F32)((point_a - point_b).length());
	if (0.0f == segment_length)
	{
		intersection = point_a;
		intersection_normal.setVec(0.0f, 0.0f, 1.0f);
		return segment_length;
	}

	// get land height	
	// Note: we assume that the line is parallel to z-axis here
	LLVector3d land_intersection = point_a;
	F32 normalized_land_distance;

	land_intersection.mdV[VZ] = regionp->getLand().resolveHeightGlobal(point_a);
	normalized_land_distance = (F32)(point_a.mdV[VZ] - land_intersection.mdV[VZ]) / segment_length;
	intersection = land_intersection;
	intersection_normal = resolveLandNormalGlobal(land_intersection);

	if (avatarp && !avatarp->mFootPlane.isExactlyClear())
	{
		LLVector3 foot_plane_normal(avatarp->mFootPlane.mV);
		LLVector3 start_pt = avatarp->getRegion()->getPosRegionFromGlobal(point_a);
		// added 0.05 meters to compensate for error in foot plane reported by Havok
		F32 norm_dist_from_plane = ((start_pt * foot_plane_normal) - avatarp->mFootPlane.mV[VW]) + 0.05f;
		norm_dist_from_plane = llclamp(norm_dist_from_plane / segment_length, 0.f, 1.f);
		if (norm_dist_from_plane < normalized_land_distance)
		{
			// collided with object before land
			normalized_land_distance = norm_dist_from_plane;
			intersection = point_a;
			intersection.mdV[VZ] -= norm_dist_from_plane * segment_length;
			intersection_normal = foot_plane_normal;
		}
		else
		{
			intersection = land_intersection;
			intersection_normal = resolveLandNormalGlobal(land_intersection);
		}
	}

	return normalized_land_distance;
}


LLSurfacePatch * LLWorld::resolveLandPatchGlobal(const LLVector3d &pos_global)
{
	//  returns a pointer to the patch at this location
	LLViewerRegion *regionp = getRegionFromPosGlobal(pos_global);
	if (!regionp)
	{
		return NULL;
	}

	return regionp->getLand().resolvePatchGlobal(pos_global);
}


LLVector3 LLWorld::resolveLandNormalGlobal(const LLVector3d &pos_global)
{
	LLViewerRegion *regionp = getRegionFromPosGlobal(pos_global);
	if (!regionp)
	{
		return LLVector3::z_axis;
	}

	return regionp->getLand().resolveNormalGlobal(pos_global);
}


void LLWorld::updateVisibilities()
{
	F32 cur_far_clip = LLViewerCamera::getInstance()->getFar();

	// Go through the culled list and check for visible regions (region is visible if land is visible)
	for (region_list_t::iterator iter = mCulledRegionList.begin(), iter_end(mCulledRegionList.end());
			iter != iter_end; )
	{
		region_list_t::iterator curiter = iter++;
		LLViewerRegion* regionp = *curiter;

		LLSpatialPartition* part = regionp->getSpatialPartition(LLViewerRegion::PARTITION_TERRAIN);
		if (part)
		{
			LLSpatialGroup* group = (LLSpatialGroup*) part->mOctree->getListener(0);
			const LLVector4a* bounds = group->getBounds();
			if (LLViewerCamera::getInstance()->AABBInFrustum(bounds[0], bounds[1]))
			{
				mCulledRegionList.erase(curiter);
				mVisibleRegionList.push_back(regionp);
			}
		}
	}

	// Update all of the visible regions 
	for (region_list_t::iterator iter = mVisibleRegionList.begin(), iter_end(mVisibleRegionList.end());
		 iter != iter_end; )
	{
		region_list_t::iterator curiter = iter++;
		LLViewerRegion* regionp = *curiter;
		if (!regionp->getLand().hasZData())
		{
			continue;
		}

		LLSpatialPartition* part = regionp->getSpatialPartition(LLViewerRegion::PARTITION_TERRAIN);
		if (part)
		{
			LLSpatialGroup* group = (LLSpatialGroup*) part->mOctree->getListener(0);
			const LLVector4a* bounds = group->getBounds();
			if (LLViewerCamera::getInstance()->AABBInFrustum(bounds[0], bounds[1]))
			{
				regionp->calculateCameraDistance();
				regionp->getLand().updatePatchVisibilities(gAgent);
			}
			else
			{
				mVisibleRegionList.erase(curiter);
				mCulledRegionList.push_back(regionp);
			}
		}
	}

	// Sort visible regions
	mVisibleRegionList.sort(LLViewerRegion::CompareDistance());
	
	LLViewerCamera::getInstance()->setFar(cur_far_clip);
}

void LLWorld::updateRegions(F32 max_update_time)
{
	LLTimer update_timer;
	BOOL did_one = FALSE;
	
	// Perform idle time updates for the regions (and associated surfaces)
	for (region_list_t::iterator iter = mActiveRegionList.begin()/*mRegionList.begin()*/, iter_end(mActiveRegionList.end()/*mRegionList.end()*/);
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		F32 max_time = max_update_time - update_timer.getElapsedTimeF32();
		if (did_one && max_time <= 0.f)
			break;
		max_time = llmin(max_time, max_update_time*.1f);
		if (regionp->idleUpdate(max_update_time))
		{
			did_one = TRUE;
		}
	}
}


void LLWorld::clearAllVisibleObjects()
{
	for (region_list_t::iterator iter = mRegionList.begin();
		 iter != mRegionList.end(); ++iter)
	{
		//clear all cached visible objects.
		(*iter)->clearCachedVisibleObjects();
	}
}

void LLWorld::updateParticles()
{
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time)
	{
		// don't move particles in snapshot mode
		return;
	}
	LLViewerPartSim::getInstance()->updateSimulation();
}

#if ENABLE_CLASSIC_CLOUDS
void LLWorld::updateClouds(const F32 dt)
{
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	static const LLCachedControl<bool> sky_use_classic_clouds("SkyUseClassicClouds",false);
	if (freeze_time ||
		!sky_use_classic_clouds)
	{
		// don't move clouds in snapshot mode
		return;
	}
	if (mActiveRegionList.size())
	{
		// Update all the cloud puff positions, and timer based stuff
		// such as death decay
		region_list_t::iterator iter_end(mActiveRegionList.end());
		for (region_list_t::iterator iter = mActiveRegionList.begin();
			 iter != iter_end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffs(dt);
		}

		// Reshuffle who owns which puffs
		for (region_list_t::iterator iter = mActiveRegionList.begin();
			 iter != iter_end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffOwnership();
		}

		// Add new puffs
		for (region_list_t::iterator iter = mActiveRegionList.begin();
			 iter != iter_end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			regionp->mCloudLayer.updatePuffCount();
		}
	}
}

LLCloudGroup* LLWorld::findCloudGroup(const LLCloudPuff &puff)
{
	if (mActiveRegionList.size())
	{
		// Update all the cloud puff positions, and timer based stuff
		// such as death decay
		for (region_list_t::iterator iter = mActiveRegionList.begin(), iter_end(mActiveRegionList.end());
			 iter != iter_end; ++iter)
		{
			LLViewerRegion* regionp = *iter;
			LLCloudGroup *groupp = regionp->mCloudLayer.findCloudGroup(puff);
			if (groupp)
			{
				return groupp;
			}
		}
	}
	return NULL;
}
#endif

void LLWorld::renderPropertyLines()
{
	S32 region_count = 0;
	S32 vertex_count = 0;

	for (region_list_t::iterator iter = mVisibleRegionList.begin(), iter_end(mVisibleRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		region_count++;
		vertex_count += regionp->renderPropertyLines();
	}
}


void LLWorld::updateNetStats()
{
	F32 bits = 0.f;
	U32 packets = 0;

	for (region_list_t::iterator iter = mActiveRegionList.begin(), iter_end(mActiveRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->updateNetStats();
		bits += regionp->mBitStat.getCurrent();
		packets += llfloor( regionp->mPacketsStat.getCurrent() );
	}

	S32 packets_in = gMessageSystem->mPacketsIn - mLastPacketsIn;
	S32 packets_out = gMessageSystem->mPacketsOut - mLastPacketsOut;
	S32 packets_lost = gMessageSystem->mDroppedPackets - mLastPacketsLost;

	S32 actual_in_bits = gMessageSystem->mPacketRing->getAndResetActualInBits();
	S32 actual_out_bits = gMessageSystem->mPacketRing->getAndResetActualOutBits();
	LLViewerStats::getInstance()->mActualInKBitStat.addValue(actual_in_bits/1024.f);
	LLViewerStats::getInstance()->mActualOutKBitStat.addValue(actual_out_bits/1024.f);
	LLViewerStats::getInstance()->mKBitStat.addValue(bits/1024.f);
	LLViewerStats::getInstance()->mPacketsInStat.addValue(packets_in);
	LLViewerStats::getInstance()->mPacketsOutStat.addValue(packets_out);
	LLViewerStats::getInstance()->mPacketsLostStat.addValue(gMessageSystem->mDroppedPackets);
	if (packets_in)
	{
		LLViewerStats::getInstance()->mPacketsLostPercentStat.addValue(100.f*((F32)packets_lost/(F32)packets_in));
	}
	else
	{
		LLViewerStats::getInstance()->mPacketsLostPercentStat.addValue(0.f);
	}

	mLastPacketsIn = gMessageSystem->mPacketsIn;
	mLastPacketsOut = gMessageSystem->mPacketsOut;
	mLastPacketsLost = gMessageSystem->mDroppedPackets;
}


void LLWorld::printPacketsLost()
{
	LL_INFOS() << "Simulators:" << LL_ENDL;
	LL_INFOS() << "----------" << LL_ENDL;

	LLCircuitData *cdp = NULL;
	for (region_list_t::iterator iter = mActiveRegionList.begin(), iter_end(mActiveRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		cdp = gMessageSystem->mCircuitInfo.findCircuit(regionp->getHost());
		if (cdp)
		{
			LLVector3d range = regionp->getCenterGlobal() - gAgent.getPositionGlobal();
				
			LL_INFOS() << regionp->getHost() << ", range: " << range.length()
					<< " packets lost: " << cdp->getPacketsLost() << LL_ENDL;
		}
	}
}

void LLWorld::processCoarseUpdate(LLMessageSystem* msg, void** user_data)
{
	LLViewerRegion* region = LLWorld::getInstance()->getRegion(msg->getSender());
	if( region )
	{
		region->updateCoarseLocations(msg);
	}
}

F32 LLWorld::getLandFarClip() const
{
	return mLandFarClip;
}

void LLWorld::setLandFarClip(const F32 far_clip)
{
// <FS:CR> Aurora Sim
	//static S32 const rwidth = (S32)REGION_WIDTH_U32;
	S32 const rwidth = (S32)getRegionWidthInMeters();
// </FS:CR> Aurora Sim
	S32 const n1 = (llceil(mLandFarClip) - 1) / rwidth;
	S32 const n2 = (llceil(far_clip) - 1) / rwidth;
	bool need_water_objects_update = n1 != n2;

	mLandFarClip = far_clip;

	if (need_water_objects_update)
	{
		updateWaterObjects();
	}
}

// Some region that we're connected to, but not the one we're in, gave us
// a (possibly) new water height. Update it in our local copy.
void LLWorld::waterHeightRegionInfo(std::string const& sim_name, F32 water_height)
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end()); iter != iter_end; ++iter)
	{
		if ((*iter)->getName() == sim_name)
		{
			(*iter)->setWaterHeight(water_height);
			break;
		}
	}
}

// There are three types of water objects:
// Region water objects: the water in a region.
// Hole water objects: water in the void but within current draw distance.
// Edge water objects: the water outside the draw distance, up till the horizon.
//
// For example:
//
// -----------------------horizon-------------------------
// |                 |                 |                 |
// |  Edge Water     |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |      rwidth     |                 |
// |                 |     <----->     |                 |
// -------------------------------------------------------
// |                 |Hole |other|     |                 |
// |                 |Water|reg. |     |                 |
// |                 |-----------------|                 |
// |                 |other|cur. |<--> |                 |
// |                 |reg. | reg.|  \__|_ draw distance  |
// |                 |-----------------|                 |
// |                 |     |     |<--->|                 |
// |                 |     |     |  \__|_ range          |
// -------------------------------------------------------
// |                 |<----width------>|<--horizon ext.->|
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// |                 |                 |                 |
// -------------------------------------------------------
//
void LLWorld::updateWaterObjects()
{
	if (!gAgent.getRegion())
	{
		return;
	}
	if (mRegionList.empty())
	{
		LL_WARNS() << "No regions!" << LL_ENDL;
		return;
	}

	LLViewerRegion const* regionp = gAgent.getRegion();

	// Region width in meters.
	S32 const rwidth = (S32)regionp->getWidth();

	// The distance we might see into the void
	// when standing on the edge of a region, in meters.
	S32 const draw_distance = llceil(mLandFarClip);

	// We can only have "holes" in the water (where there no region) if we
	// can have existing regions around it. Taking into account that this
	// code is only executed when we enter a region, and not when we walk
	// around in it, we (only) need to take into account regions that fall
	// within the draw_distance.
	//
	// Set 'range' to draw_distance, rounded up to the nearest multiple of rwidth.
	S32 const nsims = (draw_distance + rwidth - 1) / rwidth;
	S32 const range = nsims * rwidth;

	// Get South-West corner of current region.
	U32 region_x, region_y;
	from_region_handle(regionp->getHandle(), &region_x, &region_y);

	// The min. and max. coordinates of the South-West corners of the Hole water objects.
	S32 const min_x = (S32)region_x - range;
	S32 const min_y = (S32)region_y - range;
	S32 const max_x = (S32)region_x + rwidth-256 + range;
	S32 const max_y = (S32)region_y + rwidth-256 + range;

	// Attempt to determine a sensible water height for all the
	// Hole Water objects.
	//
	// It make little sense to try to guess what the best water
	// height should be when that isn't completely obvious: if it's
	// impossible to satisfy every region's water height without
	// getting a jump in the water height.
	//
	// In order to keep the reasoning simple, we assume something
	// logical as a group of connected regions, where the coastline
	// is at the outer edge. Anything more complex that would "break"
	// under such an assumption would probably break anyway (would
	// depend on terrain editing and existing mega prims, say, if
	// anything would make sense at all).
	//
	// So, what we do is find all connected regions within the
	// draw distance that border void, and then pick the lowest
	// water height of those (coast) regions.
	S32 const n = 2 * nsims + 1;
	S32 const origin = nsims + nsims * n;
	std::vector<F32> water_heights(n * n);
	std::vector<U8> checked(n * n, 0);		// index = nx + ny * n + origin;
	U8 const region_bit = 1;
	U8 const hole_bit = 2;
	U8 const bordering_hole_bit = 4;
	U8 const bordering_edge_bit = 8;
	// Use the legacy waterheight for the Edge water in the case
	// that we don't find any Hole water at all.
	F32 water_height = DEFAULT_WATER_HEIGHT;
	int max_count = 0;
	LL_DEBUGS("WaterHeight") << "Current region: " << regionp->getName() << "; water height: " << regionp->getWaterHeight() << " m." << LL_ENDL;
	std::map<S32, int> water_height_counts;
	typedef std::queue<std::pair<S32, S32>, std::deque<std::pair<S32, S32> > > nxny_pairs_type;
	nxny_pairs_type nxny_pairs;
	nxny_pairs.push(nxny_pairs_type::value_type(0, 0));
	water_heights[origin] = regionp->getWaterHeight();
	checked[origin] = region_bit;
	// For debugging purposes.
	int number_of_connected_regions = 1;
	int uninitialized_regions = 0;
	int bordering_hole = 0;
	int bordering_edge = 0;
	while(!nxny_pairs.empty())
	{
		S32 const nx = nxny_pairs.front().first;
		S32 const ny = nxny_pairs.front().second;
		LL_DEBUGS("WaterHeight") << "nx,ny = " << nx << "," << ny << LL_ENDL;
		S32 const index = nx + ny * n + origin;
		nxny_pairs.pop();
		for (S32 dir = 0; dir < 4; ++dir)
		{
			S32 const cnx = nx + gDirAxes[dir][0];
			S32 const cny = ny + gDirAxes[dir][1];
			LL_DEBUGS("WaterHeight") << "dir = " << dir << "; cnx,cny = " << cnx << "," << cny << LL_ENDL;
			S32 const cindex = cnx + cny * n + origin;
			bool is_hole = false;
			bool is_edge = false;
			LLViewerRegion* new_region_found = NULL;
			if (cnx < -nsims || cnx > nsims ||
			    cny < -nsims || cny > nsims)
			{
				LL_DEBUGS("WaterHeight") << "  Edge Water!" << LL_ENDL;
				// Bumped into Edge water object.
				is_edge = true;
			}
			else if (checked[cindex])
			{
				LL_DEBUGS("WaterHeight") << "  Already checked before!" << LL_ENDL;
				// Already checked.
				is_hole = (checked[cindex] & hole_bit);
			}
			else
			{
				S32 x = (S32)region_x + cnx * rwidth;
				S32 y = (S32)region_y + cny * rwidth;
				U64 region_handle = to_region_handle(x, y);
				new_region_found = getRegionFromHandle(region_handle);
				is_hole = !new_region_found;
				checked[cindex] = is_hole ? hole_bit : region_bit;
			}
			if (is_hole)
			{
				// This was a region that borders at least one 'hole'.
				// Count the found coastline.
				F32 new_water_height = water_heights[index];
				LL_DEBUGS("WaterHeight") << "  This is void; counting coastline with water height of " << new_water_height << LL_ENDL;
				S32 new_water_height_cm = ll_round(new_water_height * 100);
				int count = (water_height_counts[new_water_height_cm] += 1);
				// Just use the lowest water height: this is mainly about the horizon water,
				// and whatever we do, we don't want it to be possible to look under the water
				// when looking in the distance: it is better to make a step downwards in water
				// height when going away from the avie than a step upwards. However, since
				// everyone is used to DEFAULT_WATER_HEIGHT, don't allow a single region
				// to drag the water level below DEFAULT_WATER_HEIGHT on it's own.
				if (bordering_hole == 0 ||			// First time we get here.
				    (new_water_height >= DEFAULT_WATER_HEIGHT &&
					 new_water_height < water_height) ||
				    (new_water_height < DEFAULT_WATER_HEIGHT &&
					 count > max_count)
				   )
				{
					water_height = new_water_height;
				}
				if (count > max_count)
				{
					max_count = count;
				}
				if (!(checked[index] & bordering_hole_bit))
				{
					checked[index] |= bordering_hole_bit;
					++bordering_hole;
				}
			}
			else if (is_edge && !(checked[index] & bordering_edge_bit))
			{
				checked[index] |= bordering_edge_bit;
				++bordering_edge;
			}
			if (!new_region_found)
			{
				// Dead end, there is no region here.
				continue;
			}
			// Found a new connected region.
			++number_of_connected_regions;
			if (new_region_found->getName().empty())
			{
				// Uninitialized LLViewerRegion, don't use it's water height.
				LL_DEBUGS("WaterHeight") << "  Uninitialized region." << LL_ENDL;
				++uninitialized_regions;
				continue;
			}
			nxny_pairs.push(nxny_pairs_type::value_type(cnx, cny));
			water_heights[cindex] = new_region_found->getWaterHeight();
			LL_DEBUGS("WaterHeight") << "  Found a new region (name: " << new_region_found->getName() << "; water height: " << water_heights[cindex] << " m)!" << LL_ENDL;
		}
	}
	LL_INFOS() << "Number of connected regions: " << number_of_connected_regions << " (" << uninitialized_regions <<
		" uninitialized); number of regions bordering Hole water: " << bordering_hole <<
		"; number of regions bordering Edge water: " << bordering_edge << LL_ENDL;
	LL_INFOS() << "Coastline count (height, count): ";
	bool first = true;
	for (std::map<S32, int>::iterator iter = water_height_counts.begin(); iter != water_height_counts.end(); ++iter)
	{
		if (!first) LL_CONT << ", ";
		LL_CONT << "(" << (iter->first / 100.f) << ", " << iter->second << ")";
		first = false;
	}
	LL_CONT << LL_ENDL;
	LL_INFOS() << "Water height used for Hole and Edge water objects: " << water_height << LL_ENDL;

	// Update all Region water objects.
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end()); iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		LLVOWater* waterp = regionp->getLand().getWaterObj();
		if (waterp)
		{
			gObjectList.updateActive(waterp);
		}
	}

	// Clean up all existing Hole water objects.
	for (std::list<LLVOWater*>::iterator iter = mHoleWaterObjects.begin();
		 iter != mHoleWaterObjects.end(); ++iter)
	{
		LLVOWater* waterp = *iter;
		gObjectList.killObject(waterp);
	}
	mHoleWaterObjects.clear();

	// Let the Edge and Hole water boxes be 1024 meter high so that they
	// are never too small to be drawn (A LL_VO_*_WATER box has water
	// rendered on it's bottom surface only), and put their bottom at
	// the current regions water height.
	F32 const box_height = 1024;
	F32 const water_center_z = water_height + box_height / 2;
	const S32 step = 256;
	// Create new Hole water objects within 'range' where there is no region.
	for (S32 x = min_x; x <= max_x; x += step)
	{
		for (S32 y = min_y; y <= max_y; y += step)
		{
			U64 region_handle = to_region_handle(x, y);
			if (!getRegionFromHandle(region_handle))
			{
				LLVOWater* waterp = (LLVOWater*)gObjectList.createObjectViewer(LLViewerObject::LL_VO_VOID_WATER, gAgent.getRegion());
				waterp->setUseTexture(FALSE);
				waterp->setPositionGlobal(LLVector3d(x + step / 2, y + step / 2, water_center_z));
				waterp->setScale(LLVector3((F32)step, (F32)step, box_height));
				gPipeline.createObject(waterp);
				mHoleWaterObjects.push_back(waterp);
			}
		}
	}

	// Center of the region.
	S32 const center_x = region_x + step / 2;
	S32 const center_y = region_y + step / 2;
	// Width of the area with Hole water objects.
	S32 const width = step + 2 * range;
	S32 const horizon_extend = 2048 + 512 - range;	// Legacy value.
	// The overlap is needed to get rid of sky pixels being visible between the
	// Edge and Hole water object at greater distances (due to floating point
	// round off errors).
	S32 const edge_hole_overlap = 1;		// Twice the actual overlap.
		
	for (S32 dir = 0; dir < 8; ++dir)
	{
		// Size of the Edge water objects.
		S32 const dim_x = (gDirAxes[dir][0] == 0) ? width : (horizon_extend + edge_hole_overlap);
		S32 const dim_y = (gDirAxes[dir][1] == 0) ? width : (horizon_extend + edge_hole_overlap);
		// And their position.
		S32 const water_center_x = center_x + (width + horizon_extend) / 2 * gDirAxes[dir][0];
		S32 const water_center_y = center_y + (width + horizon_extend) / 2 * gDirAxes[dir][1];

		LLVOWater* waterp = mEdgeWaterObjects[dir];
		if (!waterp || waterp->isDead())
		{
			// The edge water objects can be dead because they're attached to the region that the
			// agent was in when they were originally created.
			mEdgeWaterObjects[dir] = (LLVOWater *)gObjectList.createObjectViewer(LLViewerObject::LL_VO_VOID_WATER, gAgent.getRegion());
			waterp = mEdgeWaterObjects[dir];
			waterp->setUseTexture(FALSE);
			waterp->setIsEdgePatch(TRUE);		// Mark that this is edge water and not hole water.
			gPipeline.createObject(waterp);
		}

		waterp->setRegion(gAgent.getRegion());
		LLVector3d water_pos(water_center_x, water_center_y, water_center_z);
		LLVector3 water_scale((F32) dim_x, (F32) dim_y, box_height);

		waterp->setPositionGlobal(water_pos);
		waterp->setScale(water_scale);

		gObjectList.updateActive(waterp);
	}
}

void LLWorld::shiftRegions(const LLVector3& offset)
{
	for (region_list_t::const_iterator i = getRegionList().begin(); i != getRegionList().end(); ++i)
	{
		LLViewerRegion* region = *i;
		region->updateRenderMatrix();
	}

	LLViewerPartSim::getInstance()->shift(offset);
}

LLViewerTexture* LLWorld::getDefaultWaterTexture()
{
	return mDefaultWaterTexturep;
}

void LLWorld::setSpaceTimeUSec(const U64 space_time_usec)
{
	mSpaceTimeUSec = space_time_usec;
}

U64 LLWorld::getSpaceTimeUSec() const
{
	return mSpaceTimeUSec;
}

void LLWorld::requestCacheMisses()
{
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		regionp->requestCacheMisses();
	}
}

void LLWorld::getInfo(LLSD& info)
{
	LLSD region_info;
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{	
		LLViewerRegion* regionp = *iter;
		regionp->getInfo(region_info);
		info["World"].append(region_info);
	}
}

void LLWorld::disconnectRegions()
{
	LLMessageSystem* msg = gMessageSystem;
	for (region_list_t::iterator iter = mRegionList.begin(), iter_end(mRegionList.end());
		 iter != iter_end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (regionp == gAgent.getRegion())
		{
			// Skip the main agent
			continue;
		}

		LL_INFOS() << "Sending AgentQuitCopy to: " << regionp->getHost() << LL_ENDL;
		msg->newMessageFast(_PREHASH_AgentQuitCopy);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_FuseBlock);
		msg->addU32Fast(_PREHASH_ViewerCircuitCode, gMessageSystem->mOurCircuitCode);
		msg->sendMessage(regionp->getHost());
	}
}

static LLTrace::BlockTimerStatHandle FTM_ENABLE_SIMULATOR("Enable Sim");

void process_enable_simulator(LLMessageSystem *msg, void **user_data)
{
	LL_RECORD_BLOCK_TIME(FTM_ENABLE_SIMULATOR);

	// if (!gAgent.getRegion())
	// 	return;

	// static LLCachedControl<bool> connectToNeighbors(gSavedSettings, "AlchemyConnectToNeighbors");
	// if ((!connectToNeighbors) && ((gAgent.getTeleportState() == LLAgent::TELEPORT_LOCAL)
	// 	|| (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)))
	// 	return;

	// enable the appropriate circuit for this simulator and 
	// add its values into the gSimulator structure
	U64		handle;
	U32		ip_u32;
	U16		port;

	msg->getU64Fast(_PREHASH_SimulatorInfo, _PREHASH_Handle, handle);
	msg->getIPAddrFast(_PREHASH_SimulatorInfo, _PREHASH_IP, ip_u32);
	msg->getIPPortFast(_PREHASH_SimulatorInfo, _PREHASH_Port, port);

	// which simulator should we modify?
	LLHost sim(ip_u32, port);

	// Viewer trusts the simulator.
	msg->enableCircuit(sim, TRUE);
	// <FS:CR> Aurora Sim
	U32 region_size_x = 256;
	U32 region_size_y = 256;

#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		msg->getU32Fast(_PREHASH_SimulatorInfo, _PREHASH_RegionSizeX, region_size_x);
		msg->getU32Fast(_PREHASH_SimulatorInfo, _PREHASH_RegionSizeY, region_size_y);

		if (region_size_y == 0 || region_size_x == 0)
		{
			region_size_x = 256;
			region_size_y = 256;
		}
	}
#endif

// </FS:CR> Aurora Sim
	LLWorld::getInstance()->setRegionSize(region_size_x, region_size_y);
	LLWorld::getInstance()->addRegion(handle, sim);

	// give the simulator a message it can use to get ip and port
	LL_INFOS() << "simulator_enable() Enabling " << sim << " with code " << msg->getOurCircuitCode() << LL_ENDL;
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_ID, gAgent.getID());
	msg->sendReliable(sim);
}

class LLEstablishAgentCommunication : public LLHTTPNode
{
	LOG_CLASS(LLEstablishAgentCommunication);
public:
 	virtual void describe(Description& desc) const
	{
		desc.shortInfo("seed capability info for a region");
		desc.postAPI();
		desc.input(
			"{ seed-capability: ..., sim-ip: ..., sim-port }");
		desc.source(__FILE__, __LINE__);
	}

	virtual void post(ResponsePtr response, const LLSD& context, const LLSD& input) const
	{
		if (!input["body"].has("agent-id") ||
			!input["body"].has("sim-ip-and-port") ||
			!input["body"].has("seed-capability"))
		{
			LL_WARNS() << "invalid parameters" << LL_ENDL;
            return;
		}

		LLHost sim(input["body"]["sim-ip-and-port"].asString());
	
		LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(sim);
		if (!regionp)
		{
			LL_WARNS() << "Got EstablishAgentCommunication for unknown region "
					<< sim << LL_ENDL;
			return;
		}
		LL_DEBUGS("CrossingCaps") << "Calling setSeedCapability from LLEstablishAgentCommunication::post. Seed cap == "
				<< input["body"]["seed-capability"] << LL_ENDL;
		regionp->setSeedCapability(input["body"]["seed-capability"]);
	}
};

// disable the circuit to this simulator
// Called in response to "DisableSimulator" message.
void process_disable_simulator(LLMessageSystem *mesgsys, void **user_data)
{	
	LLHost host = mesgsys->getSender();

	//LL_INFOS() << "Disabling simulator with message from " << host << LL_ENDL;
	LLWorld::getInstance()->removeRegion(host);

	mesgsys->disableCircuit(host);
}


void process_region_handshake(LLMessageSystem* msg, void** user_data)
{
	LLHost host = msg->getSender();
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegion(host);
	if (!regionp)
	{
		LL_WARNS() << "Got region handshake for unknown region "
			<< host << LL_ENDL;
		return;
	}

	regionp->unpackRegionHandshake();
}


void send_agent_pause()
{
	// *NOTE:Mani Pausing the mainloop timeout. Otherwise a long modal event may cause
	// the thread monitor to timeout.
	LLAppViewer::instance()->pauseMainloopTimeout();
	
	// Note: used to check for LLWorld initialization before it became a singleton.
	// Rather than just remove this check I'm changing it to assure that the message 
	// system has been initialized. -MG
	if (!gMessageSystem)
	{
		return;
	}
	
	gMessageSystem->newMessageFast(_PREHASH_AgentPause);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgentID);
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	gAgentPauseSerialNum++;
	gMessageSystem->addU32Fast(_PREHASH_SerialNum, gAgentPauseSerialNum);

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
		 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		gMessageSystem->sendReliable(regionp->getHost());
	}

	gObjectList.mWasPaused = TRUE;
}


void send_agent_resume()
{
	// Note: used to check for LLWorld initialization before it became a singleton.
	// Rather than just remove this check I'm changing it to assure that the message 
	// system has been initialized. -MG
	if (!gMessageSystem)
	{
		return;
	}

	gMessageSystem->newMessageFast(_PREHASH_AgentResume);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgentID);
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

	gAgentPauseSerialNum++;
	gMessageSystem->addU32Fast(_PREHASH_SerialNum, gAgentPauseSerialNum);
	

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
		 iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		gMessageSystem->sendReliable(regionp->getHost());
	}

	// Reset the FPS counter to avoid an invalid fps
	LLViewerStats::getInstance()->mFPSStat.start();

	LLAppViewer::instance()->resumeMainloopTimeout();
}

LLVector3d unpackLocalToGlobalPosition(U32 compact_local, const LLVector3d& region_origin)
{
	LLVector3d pos_local;

	pos_local.mdV[VZ] = (compact_local & 0xFFU) * 4;
	pos_local.mdV[VY] = (compact_local >> 8) & 0xFFU;
	pos_local.mdV[VX] = (compact_local >> 16) & 0xFFU;

	return region_origin + pos_local;
}

void LLWorld::getAvatars(uuid_vec_t* avatar_ids, std::vector<LLVector3d>* positions, const LLVector3d& relative_to, F32 radius) const
{
	F32 radius_squared = radius * radius;
	
	if(avatar_ids != NULL)
	{
		avatar_ids->clear();
	}
	if(positions != NULL)
	{
		positions->clear();
	}
	// get the list of avatars from the character list first, so distances are correct
	// when agent is above 1020m and other avatars are nearby
	for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
		iter != LLCharacter::sInstances.end(); ++iter)
	{
		LLVOAvatar* pVOAvatar = (LLVOAvatar*) *iter;

		if (!pVOAvatar->isDead() && !pVOAvatar->isSelf() && !pVOAvatar->mIsDummy)
		{
			LLVector3d pos_global = pVOAvatar->getPositionGlobal();
			LLUUID uuid = pVOAvatar->getID();

			if (!uuid.isNull()
				&& dist_vec_squared(pos_global, relative_to) <= radius_squared)
			{
				if(positions != NULL)
				{
					positions->push_back(pos_global);
				}
				if(avatar_ids !=NULL)
				{
					avatar_ids->push_back(uuid);
				}
			}
		}
	}
	// region avatars added for situations where radius is greater than RenderFarClip
	for (LLWorld::region_list_t::const_iterator iter = getRegionList().begin();
		iter != getRegionList().end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		const LLVector3d& origin_global = regionp->getOriginGlobal();
		S32 count = regionp->mMapAvatars.size();
		for (S32 i = 0; i < count; i++)
		{
			LLVector3d pos_global = unpackLocalToGlobalPosition(regionp->mMapAvatars.at(i), origin_global);
			if(dist_vec_squared(pos_global, relative_to) <= radius_squared)
			{
				LLUUID uuid = regionp->mMapAvatarIDs.at(i);
				// if this avatar doesn't already exist in the list, add it
				if(uuid.notNull() && avatar_ids != NULL && std::find(avatar_ids->begin(), avatar_ids->end(), uuid) == avatar_ids->end())
				{
					if(positions != NULL)
					{
						positions->push_back(pos_global);
					}
					avatar_ids->push_back(uuid);
				}
			}
		}
	}
}

void LLWorld::getAvatars(pos_map_t* umap, const LLVector3d& relative_to, F32 radius) const
{
	F32 radius_squared = radius * radius;

	if (!umap->empty())
	{
		umap->clear();
	}
	// get the list of avatars from the character list first, so distances are correct
	// when agent is above 1020m and other avatars are nearby
	for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
		 iter != LLCharacter::sInstances.end(); ++iter)
	{
		LLVOAvatar* pVOAvatar = (LLVOAvatar*) *iter;

		if (!pVOAvatar->isDead() && !pVOAvatar->isSelf() && !pVOAvatar->mIsDummy)
		{
			LLVector3d pos_global = pVOAvatar->getPositionGlobal();
			LLUUID uuid = pVOAvatar->getID();

			if (!uuid.isNull()
				&& dist_vec_squared(pos_global, relative_to) <= radius_squared)
			{
				umap->emplace(uuid, pos_global);
			}
		}
	}
	// region avatars added for situations where radius is greater than RenderFarClip
	for (LLWorld::region_list_t::const_iterator iter = getRegionList().begin();
		 iter != getRegionList().end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		const LLVector3d& origin_global = regionp->getOriginGlobal();
		S32 count = regionp->mMapAvatars.size();
		for (S32 i = 0; i < count; i++)
		{
			LLVector3d pos_global = unpackLocalToGlobalPosition(regionp->mMapAvatars.at(i), origin_global);
			if(dist_vec_squared(pos_global, relative_to) <= radius_squared)
			{
				LLUUID uuid = regionp->mMapAvatarIDs.at(i);
				// if this avatar doesn't already exist in the list, add it
				if(uuid.notNull())
				{
					umap->emplace(uuid, pos_global);
				}
			}
		}
	}
}

bool LLWorld::isRegionListed(const LLViewerRegion* region) const
{
	region_list_t::const_iterator it = find(mRegionList.begin(), mRegionList.end(), region);
	return it != mRegionList.end();
}

boost::signals2::connection LLWorld::setRegionRemovedCallback(const region_remove_signal_t::slot_type& cb)
{
	return mRegionRemovedSignal.connect(cb);
}

LLHTTPRegistration<LLEstablishAgentCommunication>
	gHTTPRegistrationEstablishAgentCommunication(
							"/message/EstablishAgentCommunication");
