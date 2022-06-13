/** 
 * @file llsurfacepatch.cpp
 * @brief LLSurfacePatch class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llsurfacepatch.h"
#include "llpatchvertexarray.h"
#include "llviewerobjectlist.h"
#include "llvosurfacepatch.h"
#include "llsurface.h"
#include "pipeline.h"
#include "llagent.h"
#include "timing.h"
#include "llsky.h"
#include "llviewercamera.h"

// For getting composition values
#include "llviewerregion.h"
#include "llvlcomposition.h"
#include "lldrawpool.h"
#include "llperlin.h"

extern bool gShiftFrame;
extern U64MicrosecondsImplicit gFrameTime;
extern LLPipeline gPipeline;

LLSurfacePatch::LLSurfacePatch(LLSurface* surface, U32 side)
:	mHasReceivedData(FALSE),
	mSTexUpdate(TRUE),
	mDirty(FALSE),
	mDirtyZStats(TRUE),
	mHeightsGenerated(FALSE),
	mDataOffset(0),
	mDataZ(NULL),
	mDataNorm(NULL),
	mVObjp(NULL),
	mOriginRegion(0.f, 0.f, 0.f),
	mCenterRegion(0.f, 0.f, 0.f),
	mMinZ(0.f),
	mMaxZ(0.f),
	mMeanZ(0.f),
	mRadius(0.f),
	mMinComposition(0.f),
	mMaxComposition(0.f),
	mMeanComposition(0.f),
	// This flag is used to communicate between adjacent surfaces and is
	// set to non-zero values by higher classes.  
	mConnectedEdge(NO_EDGE),
	mLastUpdateTime(0),
	mSurfacep(NULL),
	mNeighborPatches{ 0,0,0,0,0,0,0,0 },
	mNormalsInvalid{ 1,1,1,1,1,1,1,1,1 },
	mSide(side)
{
	setSurface(surface);
}


LLSurfacePatch::~LLSurfacePatch()
{
	mVObjp = NULL;
}


bool LLSurfacePatch::dirty()
{
	// These are outside of the loop in case we're still waiting for a dirty from the
	// texture being updated...
	if (mVObjp)
	{
		mVObjp->dirtyGeom();
	}
	else
	{
		LL_WARNS() << "No viewer object for this surface patch!" << LL_ENDL;
	}

	mDirtyZStats = TRUE;
	mHeightsGenerated = FALSE;
	
	if (!mDirty)
	{
		mDirty = TRUE;
		return true;
	}
	return false;
}


void LLSurfacePatch::setSurface(LLSurface *surfacep)
{
	mSurfacep = surfacep;
	if (mVObjp == (LLVOSurfacePatch *)NULL)
	{
		llassert(mSurfacep->mType == 'l');

		mVObjp = (LLVOSurfacePatch *)gObjectList.createObjectViewer(LLViewerObject::LL_VO_SURFACE_PATCH, mSurfacep->getRegion());
		mVObjp->setPatch(this);
		mVObjp->setPositionRegion(mCenterRegion);
		gPipeline.createObject(mVObjp);
	}
} 

void LLSurfacePatch::disconnectNeighbor(LLSurface *surfacep)
{
	U32 i;
	for (i = 0; i < 8; i++)
	{
		const auto& patch = getNeighborPatch(i);
		if (patch)
		{
			if (patch->mSurfacep == surfacep)
			{
				if (EAST == i)
				{
					mConnectedEdge &= EAST_EDGE;
				}
				else if (NORTH == i)
				{
					mConnectedEdge &= NORTH_EDGE;
				}
				else if (WEST == i)
				{
					mConnectedEdge &= WEST_EDGE;
				}
				else if (SOUTH == i)
				{
					mConnectedEdge &= SOUTH_EDGE;
				}
				setNeighborPatch(i, NULL);
			}
		}
	}
}

LLVector3 LLSurfacePatch::getPointAgent(const U32 x, const U32 y) const
{
	U32 surface_stride = mSurfacep->getGridsPerEdge();
	U32 point_offset = x + y*surface_stride;
	LLVector3 pos;
	pos = getOriginAgent();
	pos.mV[VX] += x	* mSurfacep->getMetersPerGrid();
	pos.mV[VY] += y * mSurfacep->getMetersPerGrid();
	pos.mV[VZ] = *(mDataZ + point_offset);
	return pos;
}

LLVector2 LLSurfacePatch::getTexCoords(const U32 x, const U32 y) const
{
	U32 surface_stride = mSurfacep->getGridsPerEdge();
	U32 point_offset = x + y*surface_stride;
	LLVector3 pos, rel_pos;
	pos = getOriginAgent();
	pos.mV[VX] += x	* mSurfacep->getMetersPerGrid();
	pos.mV[VY] += y * mSurfacep->getMetersPerGrid();
	pos.mV[VZ] = *(mDataZ + point_offset);
	rel_pos = pos - mSurfacep->getOriginAgent();
	rel_pos *= 1.f/surface_stride;
	return LLVector2(rel_pos.mV[VX], rel_pos.mV[VY]);
}


void LLSurfacePatch::eval(const U32 x, const U32 y, const U32 stride, LLVector3 *vertex, LLVector3 *normal,
						  LLVector2 *tex0, LLVector2 *tex1)
{
	if (!mSurfacep || !mSurfacep->getRegion() || !mSurfacep->getGridsPerEdge())
	{
		return; // failsafe
	}
	llassert_always(vertex && normal && tex0 && tex1);
	
	U32 surface_stride = mSurfacep->getGridsPerEdge();
	U32 point_offset = x + y*surface_stride;

	*normal = getNormal(x, y);

	LLVector3 pos_agent = getOriginAgent();
	pos_agent.mV[VX] += x * mSurfacep->getMetersPerGrid();
	pos_agent.mV[VY] += y * mSurfacep->getMetersPerGrid();
	pos_agent.mV[VZ]  = *(mDataZ + point_offset);
	*vertex     = pos_agent-mVObjp->getRegion()->getOriginAgent();

	LLVector3 rel_pos = pos_agent - mSurfacep->getOriginAgent();
	LLVector3 tex_pos = rel_pos * (1.f/surface_stride);
	tex0->mV[0]  = tex_pos.mV[0];
	tex0->mV[1]  = tex_pos.mV[1];
	tex1->mV[0] = mSurfacep->getRegion()->getCompositionXY(llfloor(mOriginRegion.mV[0])+x, llfloor(mOriginRegion.mV[1])+y);

	const F32 xyScale = 4.9215f*7.f; //0.93284f;
	const F32 xyScaleInv = (1.f / xyScale)*(0.2222222222f);

	LLVector2 vec(
		(F32)fmod((F32)(mOriginGlobal.mdV[0] + x)*xyScaleInv, 256.f), // <FS:ND/> Added (F32) for proper array initialization
		(F32)fmod((F32)(mOriginGlobal.mdV[1] + y)*xyScaleInv, 256.f) // <FS:ND/> Added (F32) for proper array initialization
		);
	F32 rand_val = llclamp(LLPerlinNoise::noise(vec)* 0.75f + 0.5f, 0.f, 1.f);
	tex1->mV[1] = rand_val;


}


void LLSurfacePatch::calcNormal(const U32 x, const U32 y, const U32 stride)
{
	U32 patch_width = mSurfacep->mPVArray.mPatchWidth;
	U32 surface_stride = mSurfacep->getGridsPerEdge();

	const F32 mpg = mSurfacep->getMetersPerGrid() * stride;

// <FS:CR> Aurora Sim
// Singu Note: poffsets gets an extra space each for surface stride (tag clutter removed)
	//S32 poffsets[2][2][2];
	S32 poffsets[2][2][3];
	poffsets[0][0][0] = x - stride;
	poffsets[0][0][1] = y - stride;
	poffsets[0][0][2] = surface_stride;

	poffsets[0][1][0] = x - stride;
	poffsets[0][1][1] = y + stride;
	poffsets[0][1][2] = surface_stride;

	poffsets[1][0][0] = x + stride;
	poffsets[1][0][1] = y - stride;
	poffsets[1][0][2] = surface_stride;

	poffsets[1][1][0] = x + stride;
	poffsets[1][1][1] = y + stride;
	poffsets[1][1][2] = surface_stride;
// </FS:CR> Aurora Sim

	const LLSurfacePatch *ppatches[2][2];

	// LLVector3 p1, p2, p3, p4;

	ppatches[0][0] = this;
	ppatches[0][1] = this;
	ppatches[1][0] = this;
	ppatches[1][1] = this;

	U32 i, j;
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 2; j++)
		{
			LLSurfacePatch* patch;
			if (poffsets[i][j][0] < 0)
			{
				if (patch = ppatches[i][j]->getNeighborPatch(WEST))
				{
					// <FS:CR> Aurora Sim
					ppatches[i][j] = patch;
					poffsets[i][j][0] += patch_width;
					poffsets[i][j][2] = patch->getSurface()->getGridsPerEdge();
// </FS:CR> Aurora Sim
				}
				else
				{
					poffsets[i][j][0] = 0;
				}
			}
			if (poffsets[i][j][1] < 0)
			{
				if (patch = ppatches[i][j]->getNeighborPatch(SOUTH))
				{
// <FS:CR> Aurora Sim
					ppatches[i][j] = patch;
					poffsets[i][j][1] += patch_width;
					poffsets[i][j][2] = patch->getSurface()->getGridsPerEdge();
// </FS>CR> Aurora Sim
				}
				else
				{
					poffsets[i][j][1] = 0;
				}
			}
			if (poffsets[i][j][0] >= (S32)patch_width)
			{
				if (patch = ppatches[i][j]->getNeighborPatch(EAST))
				{
// <FS:CR> Aurora Sim
					ppatches[i][j] = patch;
					poffsets[i][j][0] -= patch_width;
					poffsets[i][j][2] = patch->getSurface()->getGridsPerEdge();
// </FS:CR> Aurora Sim
				}
				else
				{
					poffsets[i][j][0] = patch_width - 1;
				}
			}
			if (poffsets[i][j][1] >= (S32)patch_width)
			{
				if (patch = ppatches[i][j]->getNeighborPatch(NORTH))
				{
// <FS:CR> Aurora Sim
					ppatches[i][j] = patch;
					poffsets[i][j][1] -= patch_width;
					poffsets[i][j][2] = patch->getSurface()->getGridsPerEdge();
// </FS:CR> Aurora Sim
				}
				else
				{
					poffsets[i][j][1] = patch_width - 1;
				}
			}
		}
	}

	LLVector3 p00(-mpg,-mpg,
				  *(ppatches[0][0]->mDataZ
				  + poffsets[0][0][0]
// <FS:CR> Aurora Sim
// Singu Note: multiply the y poffsets by its own surface stride (tag clutter removed)
				  + poffsets[0][0][1]*poffsets[0][0][2]));
	LLVector3 p01(-mpg,+mpg,
				  *(ppatches[0][1]->mDataZ
				  + poffsets[0][1][0]
				  + poffsets[0][1][1]*poffsets[0][1][2]));
	LLVector3 p10(+mpg,-mpg,
				  *(ppatches[1][0]->mDataZ
				  + poffsets[1][0][0]
				  + poffsets[1][0][1]*poffsets[1][0][2]));
	LLVector3 p11(+mpg,+mpg,
				  *(ppatches[1][1]->mDataZ
				  + poffsets[1][1][0]
				  + poffsets[1][1][1]*poffsets[1][1][2]));
// </FS:CR> Aurora Sim

	LLVector3 c1 = p11 - p00;
	LLVector3 c2 = p01 - p10;

	LLVector3 normal = c1;
	normal %= c2;
	normal.normVec();

	llassert(mDataNorm);
	*(mDataNorm + surface_stride * y + x) = normal;
}

const LLVector3 &LLSurfacePatch::getNormal(const U32 x, const U32 y) const
{
	U32 surface_stride = mSurfacep->getGridsPerEdge();
	llassert(mDataNorm);
	return *(mDataNorm + surface_stride * y + x);
}


void LLSurfacePatch::updateCameraDistanceRegion(const LLVector3 &pos_region)
{
	if (LLPipeline::sDynamicLOD)
	{
		if (!gShiftFrame)
		{
			LLVector3 dv = pos_region;
			dv -= mCenterRegion;
			mVisInfo.mDistance = llmax(0.f, (F32)(dv.magVec() - mRadius))/
				llmax(LLVOSurfacePatch::sLODFactor, 0.1f);
		}
	}
	else
	{
		mVisInfo.mDistance = 0.f;
	}
}

F32 LLSurfacePatch::getDistance() const
{
	return mVisInfo.mDistance;
}


// Called when a patch has changed its height field
// data.
void LLSurfacePatch::updateVerticalStats() 
{
	if (!mDirtyZStats)
	{
		return;
	}

	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();
	F32 meters_per_grid = mSurfacep->getMetersPerGrid();

	U32 i, j, k;
	F32 z, total;

	llassert(mDataZ);
	z = *(mDataZ);

	mMinZ = z;
	mMaxZ = z;

	k = 0;
	total = 0.0f;

	// Iterate to +1 because we need to do the edges correctly.
	for (j=0; j<(grids_per_patch_edge+1); j++) 
	{
		for (i=0; i<(grids_per_patch_edge+1); i++) 
		{
			z = *(mDataZ + i + j*grids_per_edge);

			if (z < mMinZ)
			{
				mMinZ = z;
			}
			if (z > mMaxZ)
			{
				mMaxZ = z;
			}
			total += z;
			k++;
		}
	}
	mMeanZ = total / (F32) k;
	mCenterRegion.mV[VZ] = 0.5f * (mMinZ + mMaxZ);

	LLVector3 diam_vec(meters_per_grid*grids_per_patch_edge,
						meters_per_grid*grids_per_patch_edge,
						mMaxZ - mMinZ);
	mRadius = diam_vec.magVec() * 0.5f;

	mSurfacep->mMaxZ = llmax(mMaxZ, mSurfacep->mMaxZ);
	mSurfacep->mMinZ = llmin(mMinZ, mSurfacep->mMinZ);
	mSurfacep->mHasZData = TRUE;
	mSurfacep->getRegion()->calculateCenterGlobal();

	if (mVObjp)
	{
		mVObjp->dirtyPatch();
	}
	mDirtyZStats = FALSE;
}


bool LLSurfacePatch::updateNormals() 
{
	if (mSurfacep->mType == 'w')
	{
		return false;
	}
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	BOOL dirty_patch = FALSE;

	U32 i, j;
	// update the east edge
	if (mNormalsInvalid[EAST] || mNormalsInvalid[NORTHEAST] || mNormalsInvalid[SOUTHEAST])
	{
		for (j = 0; j <= grids_per_patch_edge; j++)
		{
			calcNormal(grids_per_patch_edge, j, 2);
			calcNormal(grids_per_patch_edge - 1, j, 2);
			calcNormal(grids_per_patch_edge - 2, j, 2);
		}

		dirty_patch = TRUE;
	}

	// update the north edge
	if (mNormalsInvalid[NORTHEAST] || mNormalsInvalid[NORTH] || mNormalsInvalid[NORTHWEST])
	{
		for (i = 0; i <= grids_per_patch_edge; i++)
		{
			calcNormal(i, grids_per_patch_edge, 2);
			calcNormal(i, grids_per_patch_edge - 1, 2);
			calcNormal(i, grids_per_patch_edge - 2, 2);
		}

		dirty_patch = TRUE;
	}

	// update the west edge
	if (mNormalsInvalid[NORTHWEST] || mNormalsInvalid[WEST] || mNormalsInvalid[SOUTHWEST])
	{
		LLSurfacePatch* northwest_patchp = getNeighborPatch(NORTHWEST);
		LLSurfacePatch* north_patchp = getNeighborPatch(NORTH);
// <FS:CR> Aurora Sim
		if (!north_patchp && northwest_patchp && northwest_patchp->getHasReceivedData())
		{
			*(mDataZ + grids_per_patch_edge*grids_per_edge) = *(northwest_patchp->mDataZ + grids_per_patch_edge);
		}
// </FS:CR> Aurora Sim

		for (j = 0; j < grids_per_patch_edge; j++)
		{
			calcNormal(0, j, 2);
			calcNormal(1, j, 2);
		}
		dirty_patch = TRUE;
	}

	// update the south edge
	if (mNormalsInvalid[SOUTHWEST] || mNormalsInvalid[SOUTH] || mNormalsInvalid[SOUTHEAST])
	{
		LLSurfacePatch* southeast_patchp = getNeighborPatch(SOUTHEAST);
		LLSurfacePatch* east_patchp = getNeighborPatch(EAST);
// <FS:CR> Aurora Sim
		if (!east_patchp && southeast_patchp && southeast_patchp->getHasReceivedData())
		{
			*(mDataZ + grids_per_patch_edge) = *(southeast_patchp->mDataZ + grids_per_patch_edge * southeast_patchp->getSurface()->getGridsPerEdge());
		}
// </FS:CR> Aurora Sim

		for (i = 0; i < grids_per_patch_edge; i++)
		{
			calcNormal(i, 0, 2);
			calcNormal(i, 1, 2);
		}
		dirty_patch = TRUE;
	}

	// Invalidating the northeast corner is different, because depending on what the adjacent neighbors are,
	// we'll want to do different things.
	if (mNormalsInvalid[NORTHEAST])
	{
		LLSurfacePatch* northeast_patchp = getNeighborPatch(NORTHEAST);
		LLSurfacePatch* north_patchp = getNeighborPatch(NORTH);
		LLSurfacePatch* east_patchp = getNeighborPatch(EAST);
		if (!northeast_patchp)
		{
			if (!north_patchp)
			{
				if (!east_patchp)
				{
					// No north or east neighbors.  Pull from the diagonal in your own patch.
					*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
						*(mDataZ + grids_per_patch_edge - 1 + (grids_per_patch_edge - 1)*grids_per_edge);
				}
				else
				{
					if (east_patchp->getHasReceivedData())
					{
						// East, but not north.  Pull from your east neighbor's northwest point.
						*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
// <FS:CR> Aurora Sim
							//*(getNeighborPatch(EAST)->mDataZ + (grids_per_patch_edge - 1)*grids_per_edge);
							*(east_patchp->mDataZ + (east_patchp->getSurface()->getGridsPerPatchEdge() - 1)* east_patchp->getSurface()->getGridsPerEdge());
// </FS:CR> Aurora Sim
					}
					else
					{
						*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
							*(mDataZ + grids_per_patch_edge - 1 + (grids_per_patch_edge - 1)*grids_per_edge);
					}
				}
			}
			else
			{
				// We have a north.
				if (east_patchp)
				{
					// North and east neighbors, but not northeast.
					// Pull from diagonal in your own patch.
					*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
						*(mDataZ + grids_per_patch_edge - 1 + (grids_per_patch_edge - 1)*grids_per_edge);
				}
				else
				{
					if (north_patchp->getHasReceivedData())
					{
						// North, but not east.  Pull from your north neighbor's southeast corner.
						*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
// <FS:CR> Aurora Sim
							//*(getNeighborPatch(NORTH)->mDataZ + (grids_per_patch_edge - 1));
							*(north_patchp->mDataZ + (north_patchp->getSurface()->getGridsPerPatchEdge() - 1));
// </FS:CR> Aurora Sim
					}
					else
					{
						*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
							*(mDataZ + grids_per_patch_edge - 1 + (grids_per_patch_edge - 1)*grids_per_edge);
					}
				}
			}
		}
		else if (northeast_patchp->mSurfacep != mSurfacep)
		{
			if (
				(!north_patchp || (north_patchp->mSurfacep != mSurfacep))
				&&
				(!east_patchp || (east_patchp->mSurfacep != mSurfacep)))
			{
// <FS:CR> Aurora Sim
				U32 own_xpos, own_ypos, neighbor_xpos, neighbor_ypos;
				S32 own_offset = 0, neighbor_offset = 0;
				from_region_handle(mSurfacep->getRegion()->getHandle(), &own_xpos, &own_ypos);
				from_region_handle(northeast_patchp->mSurfacep->getRegion()->getHandle(), &neighbor_xpos, &neighbor_ypos);
				if (own_ypos >= neighbor_ypos)
					neighbor_offset = own_ypos - neighbor_ypos;
				else
					own_offset = neighbor_ypos - own_ypos;

				*(mDataZ + grids_per_patch_edge + grids_per_patch_edge*grids_per_edge) =
					*(northeast_patchp->mDataZ + (grids_per_edge + neighbor_offset - own_offset - 1) * northeast_patchp->getSurface()->getGridsPerEdge());
// </FS:CR> Aurora Sim
			}
		}
		else
		{
			// We've got a northeast patch in the same surface.
			// The z and normals will be handled by that patch.
		}
		calcNormal(grids_per_patch_edge, grids_per_patch_edge, 2);
		calcNormal(grids_per_patch_edge, grids_per_patch_edge - 1, 2);
		calcNormal(grids_per_patch_edge - 1, grids_per_patch_edge, 2);
		calcNormal(grids_per_patch_edge - 1, grids_per_patch_edge - 1, 2);
		dirty_patch = TRUE;
	}

	// update the middle normals
	if (mNormalsInvalid[MIDDLE])
	{
		for (j=2; j < grids_per_patch_edge - 2; j++)
		{
			for (i=2; i < grids_per_patch_edge - 2; i++)
			{
				calcNormal(i, j, 2);
			}
		}
		dirty_patch = TRUE;
	}

	for (i = 0; i < 9; i++)
	{
		mNormalsInvalid[i] = FALSE;
	}

	return dirty_patch;
}

void LLSurfacePatch::updateEastEdge()
{
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();
// <FS:CR> Aurora Sim
	U32 grids_per_edge_east = grids_per_edge;

	//U32 j, k;
	U32 j, k, h;
// <FS:CR> Aurora Sim
	F32 *west_surface, *east_surface;

	if (!getNeighborPatch(EAST))
	{
		west_surface = mDataZ + grids_per_patch_edge;
		east_surface = mDataZ + grids_per_patch_edge - 1;
	}
	else if (mConnectedEdge & EAST_EDGE)
	{
		west_surface = mDataZ + grids_per_patch_edge;
		east_surface = getNeighborPatch(EAST)->mDataZ;
// <FS:CR> Aurora Sim
		grids_per_edge_east = getNeighborPatch(EAST)->getSurface()->getGridsPerEdge();
// <FS:CR> Aurora Sim
	}
	else
	{
		return;
	}

	// If patchp is on the east edge of its surface, then we update the east
	// side buffer
	for (j=0; j < grids_per_patch_edge; j++)
	{
		k = j * grids_per_edge;
// <FS:CR> Aurora Sim
		h = j * grids_per_edge_east;
		*(west_surface + k) = *(east_surface + h);	// update buffer Z
		//*(west_surface + k) = *(east_surface + k);	// update buffer Z
// </FS:CR> Aurora Sim
	}
}


void LLSurfacePatch::updateNorthEdge()
{
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();
	U32 grids_per_edge = mSurfacep->getGridsPerEdge();

	U32 i;
	F32 *south_surface, *north_surface;

	LLSurfacePatch* patchp = getNeighborPatch(NORTH);
	if (!patchp)
	{
		south_surface = mDataZ + grids_per_patch_edge*grids_per_edge;
		north_surface = mDataZ + (grids_per_patch_edge - 1) * grids_per_edge;
	}
	else if (mConnectedEdge & NORTH_EDGE)
	{
		south_surface = mDataZ + grids_per_patch_edge*grids_per_edge;
		north_surface = patchp->mDataZ;
	}
	else
	{
		return;
	}

	// Update patchp's north edge ...
	for (i = 0; i<grids_per_patch_edge; i++)
	{
		*(south_surface + i) = *(north_surface + i);	// update buffer Z
	}
}

BOOL LLSurfacePatch::updateTexture()
{
	if (mSTexUpdate)		//  Update texture as needed
	{
		F32 meters_per_grid = getSurface()->getMetersPerGrid();
		F32 grids_per_patch_edge = (F32)getSurface()->getGridsPerPatchEdge();

		LLSurfacePatch* patchp;
		if ((!(patchp = getNeighborPatch(EAST)) || patchp->getHasReceivedData())
			&& (!(patchp = getNeighborPatch(WEST)) || patchp->getHasReceivedData())
			&& (!(patchp = getNeighborPatch(SOUTH)) || patchp->getHasReceivedData())
			&& (!(patchp = getNeighborPatch(NORTH)) || patchp->getHasReceivedData()))
		{
			LLViewerRegion *regionp = getSurface()->getRegion();
			LLVector3d origin_region = getOriginGlobal() - getSurface()->getOriginGlobal();

			// Have to figure out a better way to deal with these edge conditions...
			LLVLComposition* comp = regionp->getComposition();
			if (!mHeightsGenerated)
			{
				F32 patch_size = meters_per_grid*(grids_per_patch_edge+1);
				if (comp->generateHeights((F32)origin_region[VX], (F32)origin_region[VY],
										  patch_size, patch_size))
				{
					mHeightsGenerated = TRUE;
				}
				else
				{
					return FALSE;
				}
			}
			
			if (comp->generateComposition())
			{
				if (mVObjp)
				{
					mVObjp->dirtyGeom();
					gPipeline.markGLRebuild(mVObjp);
					return TRUE;
				}
			}
		}
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}

void LLSurfacePatch::updateGL()
{
	F32 meters_per_grid = getSurface()->getMetersPerGrid();
	F32 grids_per_patch_edge = (F32)getSurface()->getGridsPerPatchEdge();

	LLViewerRegion *regionp = getSurface()->getRegion();
	LLVector3d origin_region = getOriginGlobal() - getSurface()->getOriginGlobal();

	LLVLComposition* comp = regionp->getComposition();
	
	updateCompositionStats();
	F32 tex_patch_size = meters_per_grid*grids_per_patch_edge;
	if (comp->generateTexture((F32)origin_region[VX], (F32)origin_region[VY],
							  tex_patch_size, tex_patch_size))
	{
		mSTexUpdate = FALSE;

		// Also generate the water texture
		mSurfacep->generateWaterTexture((F32)origin_region.mdV[VX], (F32)origin_region.mdV[VY],
										tex_patch_size, tex_patch_size);
	}
}

bool LLSurfacePatch::dirtyZ()
{
	mSTexUpdate = TRUE;

	// Invalidate all normals in this patch
	U32 i;
	for (i = 0; i < 9; i++)
	{
		mNormalsInvalid[i] = TRUE;
	}

	// Invalidate normals in this and neighboring patches
	for (i = 0; i < 8; i++)
	{
		if (mNeighborPatches[i] == nullptr)
		{
			continue;
		}
		if (mNeighborPatches[i]->expired())
		{
			LL_WARNS() << "Expired neighbor patch detected. Side " << i << LL_ENDL;
			delete mNeighborPatches[i];
			mNeighborPatches[i] = nullptr;
			continue;
		}
		const surface_patch_ref& patchp = mNeighborPatches[i]->lock();
		if (patchp)
		{
			patchp->mNormalsInvalid[gDirOpposite[i]] = TRUE;
			if (patchp->dirty())
			{
				patchp->getSurface()->dirtySurfacePatch(patchp);
			}
			if (i < 4)
			{
				patchp->mNormalsInvalid[gDirAdjacent[gDirOpposite[i]][0]] = TRUE;
				patchp->mNormalsInvalid[gDirAdjacent[gDirOpposite[i]][1]] = TRUE;
			}
		}
	}

	mLastUpdateTime = gFrameTime;
	
	return dirty();
}


const U64 &LLSurfacePatch::getLastUpdateTime() const
{
	return mLastUpdateTime;
}

F32 LLSurfacePatch::getMaxZ() const
{
	return mMaxZ;
}

F32 LLSurfacePatch::getMinZ() const
{
	return mMinZ;
}

void LLSurfacePatch::setOriginGlobal(const LLVector3d &origin_global)
{
	mOriginGlobal = origin_global;

	LLVector3 origin_region;
	origin_region.setVec(mOriginGlobal - mSurfacep->getOriginGlobal());

	mOriginRegion = origin_region;
	mCenterRegion.mV[VX] = origin_region.mV[VX] + 0.5f*mSurfacep->getGridsPerPatchEdge()*mSurfacep->getMetersPerGrid();
	mCenterRegion.mV[VY] = origin_region.mV[VY] + 0.5f*mSurfacep->getGridsPerPatchEdge()*mSurfacep->getMetersPerGrid();

	mVisInfo.mbIsVisible = FALSE;
	mVisInfo.mDistance = 512.0f;
	mVisInfo.mRenderLevel = 0;
	mVisInfo.mRenderStride = mSurfacep->getGridsPerPatchEdge();
	
}

void LLSurfacePatch::connectNeighbor(const surface_patch_ref& neighbor_patchp, const U32 direction)
{
	llassert(neighbor_patchp);
	if (!neighbor_patchp) return;
	mNormalsInvalid[direction] = TRUE;

	setNeighborPatch(direction, neighbor_patchp);

	if (EAST == direction)
	{
		mConnectedEdge |= EAST_EDGE;
	}
	else if (NORTH == direction)
	{
		mConnectedEdge |= NORTH_EDGE;
	}
	else if (WEST == direction)
	{
		mConnectedEdge |= WEST_EDGE;
	}
	else if (SOUTH == direction)
	{
		mConnectedEdge |= SOUTH_EDGE;
	}
}

void LLSurfacePatch::updateVisibility()
{
	if (mVObjp.isNull())
	{
		return;
	}

	const F32 DEFAULT_DELTA_ANGLE 	= (0.15f);
	U32 old_render_stride, max_render_stride;
	U32 new_render_level;
	F32 stride_per_distance = DEFAULT_DELTA_ANGLE / mSurfacep->getMetersPerGrid();
	U32 grids_per_patch_edge = mSurfacep->getGridsPerPatchEdge();

	LLVector4a center;
	center.load3( (mCenterRegion + mSurfacep->getOriginAgent()).mV);
	LLVector4a radius;
	radius.splat(mRadius);

	// sphere in frustum on global coordinates
	if (LLViewerCamera::getInstance()->AABBInFrustumNoFarClip(center, radius))
	{
		// We now need to calculate the render stride based on patchp's distance 
		// from LLCamera render_stride is governed by a relation something like this...
		//
		//                       delta_angle * patch.distance
		// render_stride <=  ----------------------------------------
		//                           mMetersPerGrid
		//
		// where 'delta_angle' is the desired solid angle of the average polgon on a patch.
		//
		// Any render_stride smaller than the RHS would be 'satisfactory'.  Smaller 
		// strides give more resolution, but efficiency suggests that we use the largest 
		// of the render_strides that obey the relation.  Flexibility is achieved by 
		// modulating 'delta_angle' until we have an acceptable number of triangles.
	
		old_render_stride = mVisInfo.mRenderStride;

		// Calculate the render_stride using information in agent
		max_render_stride = lltrunc(mVisInfo.mDistance * stride_per_distance);
		max_render_stride = llmin(max_render_stride , 2*grids_per_patch_edge);

		// We only use render_strides that are powers of two, so we use look-up tables to figure out
		// the render_level and corresponding render_stride
		new_render_level = mVisInfo.mRenderLevel = mSurfacep->getRenderLevel(max_render_stride);
		mVisInfo.mRenderStride = mSurfacep->getRenderStride(new_render_level);

		if ((mVisInfo.mRenderStride != old_render_stride)) 
			// The reason we check !mbIsVisible is because non-visible patches normals 
			// are not updated when their data is changed.  When this changes we can get 
			// rid of mbIsVisible altogether.
		{
			if (mVObjp)
			{
				mVObjp->dirtyGeom();
				LLSurfacePatch* patchp;
				if (patchp = getNeighborPatch(WEST))
				{
					patchp->mVObjp->dirtyGeom();
				}
				if (patchp = getNeighborPatch(SOUTH))
				{
					patchp->mVObjp->dirtyGeom();
				}
			}
		}
		mVisInfo.mbIsVisible = TRUE;
	}
	else
	{
		mVisInfo.mbIsVisible = FALSE;
	}
}


const LLVector3d &LLSurfacePatch::getOriginGlobal() const
{
	return mOriginGlobal;
}

LLVector3 LLSurfacePatch::getOriginAgent() const
{
	return gAgent.getPosAgentFromGlobal(mOriginGlobal);
}

BOOL LLSurfacePatch::getVisible() const
{
	return mVisInfo.mbIsVisible;
}

U32 LLSurfacePatch::getRenderStride() const
{
	return mVisInfo.mRenderStride;
}

S32 LLSurfacePatch::getRenderLevel() const
{
	return mVisInfo.mRenderLevel;
}

void LLSurfacePatch::setHasReceivedData()
{
	mHasReceivedData = TRUE;
}

BOOL LLSurfacePatch::getHasReceivedData() const
{
	return mHasReceivedData;
}

const LLVector3 &LLSurfacePatch::getCenterRegion() const
{
	return mCenterRegion;
}


void LLSurfacePatch::updateCompositionStats()
{
	LLViewerLayer *vlp = mSurfacep->getRegion()->getComposition();

	F32 x, y, width, height, mpg, min, mean, max;

	LLVector3 origin = getOriginAgent() - mSurfacep->getOriginAgent();
	mpg = mSurfacep->getMetersPerGrid();
	x = origin.mV[VX];
	y = origin.mV[VY];
	width = mpg*(mSurfacep->getGridsPerPatchEdge()+1);
	height = mpg*(mSurfacep->getGridsPerPatchEdge()+1);

	mean = 0.f;
	min = vlp->getValueScaled(x, y);
	max= min;
	U32 count = 0;
	F32 i, j;
	for (j = 0; j < height; j += mpg)
	{
		for (i = 0; i < width; i += mpg)
		{
			F32 comp = vlp->getValueScaled(x + i, y + j);
			mean += comp;
			min = llmin(min, comp);
			max = llmax(max, comp);
			count++;
		}
	}
	mean /= count;

	mMinComposition = min;
	mMeanComposition = mean;
	mMaxComposition = max;
}

F32 LLSurfacePatch::getMeanComposition() const
{
	return mMeanComposition;
}

F32 LLSurfacePatch::getMinComposition() const
{
	return mMinComposition;
}

F32 LLSurfacePatch::getMaxComposition() const
{
	return mMaxComposition;
}

void LLSurfacePatch::setNeighborPatch(const U32 direction, const surface_patch_ref& neighborp)
{
	if (!neighborp)
	{
		delete mNeighborPatches[direction];
		mNeighborPatches[direction] = nullptr;
	}
	else
	{
		if (mNeighborPatches[direction] == nullptr)
		{
			mNeighborPatches[direction] = new surface_patch_weak_ref();
		}
		*mNeighborPatches[direction] = neighborp;
	}
	mNormalsInvalid[direction] = TRUE;
	if (direction < 4)
	{
		mNormalsInvalid[gDirAdjacent[direction][0]] = TRUE;
		mNormalsInvalid[gDirAdjacent[direction][1]] = TRUE;
	}
}

LLSurfacePatch *LLSurfacePatch::getNeighborPatch(const U32 direction) const
{
	if (mNeighborPatches[direction] == nullptr)
	{
		return nullptr;
	}
	else if (mNeighborPatches[direction]->expired())
	{
		LL_WARNS() << "Expired neighbor patch detected. Side " << direction << LL_ENDL;
	}
	return mNeighborPatches[direction]->lock().get();
}

void LLSurfacePatch::clearVObj()
{
	mVObjp = NULL;
}
