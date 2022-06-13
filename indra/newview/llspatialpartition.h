/** 
 * @file llspatialpartition.h
 * @brief LLSpatialGroup header file including definitions for supporting functions
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 * 
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLSPATIALPARTITION_H
#define LL_LLSPATIALPARTITION_H

#define SG_MIN_DIST_RATIO 0.00001f

#include "llmemory.h"
#include "lldrawable.h"
#include "lloctree.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llvertexbuffer.h"
#include "llgltypes.h"
#include "llcubemap.h"
#include "lldrawpool.h"
#include "llface.h"
#include "llviewercamera.h"
#include "llvector4a.h"
#include <queue>

#define SG_STATE_INHERIT_MASK (OCCLUDED)
#define SG_INITIAL_STATE_MASK (DIRTY | GEOM_DIRTY)

class LLViewerOctreePartition;
class LLSpatialPartition;
class LLSpatialBridge;
class LLSpatialGroup;
class LLViewerRegion;

void pushVerts(LLFace* face, U32 mask);



class LLDrawInfo : public LLRefCount 
{
protected:
	~LLDrawInfo();	
	
public:
	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}


	LLDrawInfo(const LLDrawInfo& rhs)
	{
		*this = rhs;
	}

	const LLDrawInfo& operator=(const LLDrawInfo& rhs)
	{
		LL_ERRS() << "Illegal operation!" << LL_ENDL;
		return *this;
	}

	LLDrawInfo(U16 start, U16 end, U32 count, U32 offset, 
				LLViewerTexture* image, LLVertexBuffer* buffer, 
				BOOL fullbright = FALSE, U8 bump = 0, BOOL particle = FALSE, F32 part_size = 0);
	

	void validate();

	LL_ALIGN_16(LLVector4a mExtents[2]);
	
	LLPointer<LLVertexBuffer> mVertexBuffer;
	LLPointer<LLViewerTexture>     mTexture;
	std::vector<LLPointer<LLViewerTexture> > mTextureList;

	S32 mDebugColor;
	const LLMatrix4a* mTextureMatrix;
	const LLMatrix4a* mModelMatrix;
	U16 mStart;
	U16 mEnd;
	U32 mCount;
	U32 mOffset;
	BOOL mFullbright;
	U8 mBump;
	U8 mShiny;
	BOOL mParticle;
	F32 mPartSize;
	F32 mVSize;
	LLPointer<LLSpatialGroup> mGroup;
	LL_ALIGN_16(LLFace* mFace); //associated face
	F32 mDistance;
	U32 mDrawMode;
	LLMaterialPtr mMaterial; // If this is null, the following parameters are unused.
	LLMaterialID mMaterialID;
	U32 mShaderMask;
	U32 mBlendFuncSrc;
	U32 mBlendFuncDst;
	BOOL mHasGlow;
	LLPointer<LLViewerTexture> mSpecularMap;
	LLPointer<LLViewerTexture> mNormalMap;
	LLVector4 mSpecColor; // XYZ = Specular RGB, W = Specular Exponent
	F32  mEnvIntensity;
	F32  mAlphaMaskCutoff;
	U8   mDiffuseAlphaMode;


	struct CompareTexture
	{
		bool operator()(const LLDrawInfo& lhs, const LLDrawInfo& rhs)
		{
			return lhs.mTexture > rhs.mTexture;
		}
	};

	struct CompareTexturePtr
	{ //sort by texture
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mTexture.get() > rhs->mTexture.get()));
		}
	};

	struct CompareVertexBuffer
	{ //sort by texture
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			// sort by pointer, sort NULL down to the end
			return lhs.get() != rhs.get() 
					&& (lhs.isNull() || (rhs.notNull() && lhs->mVertexBuffer.get() > rhs->mVertexBuffer.get()));
		}
	};

	struct CompareTexturePtrMatrix
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && (lhs->mTexture.get() > rhs->mTexture.get() ||
															   (lhs->mTexture.get() == rhs->mTexture.get() && lhs->mModelMatrix > rhs->mModelMatrix))));
		}

	};

	struct CompareMatrixTexturePtr
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs)	
		{
			return lhs.get() != rhs.get() 
				&& (lhs.isNull() || (rhs.notNull() && (lhs->mModelMatrix > rhs->mModelMatrix ||
													   (lhs->mModelMatrix == rhs->mModelMatrix && lhs->mTexture.get() > rhs->mTexture.get()))));
		}

	};

	struct CompareBump
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs) 
		{
			// sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mBump > rhs->mBump));
		}
	};

	struct CompareDistanceGreater
	{
		bool operator()(const LLPointer<LLDrawInfo>& lhs, const LLPointer<LLDrawInfo>& rhs) 
		{
			// sort by mBump value, sort NULL down to the end
			return lhs.get() != rhs.get() 
						&& (lhs.isNull() || (rhs.notNull() && lhs->mDistance > rhs->mDistance));
		}
	};
};

LL_ALIGN_PREFIX(64)
class LLSpatialGroup : public LLOcclusionCullingGroup
{
	friend class LLSpatialPartition;
	friend class LLOctreeStateCheck;
public:

	LLSpatialGroup(const LLSpatialGroup& rhs) : LLOcclusionCullingGroup(rhs)
	{
		*this = rhs;
	}

	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	const LLSpatialGroup& operator=(const LLSpatialGroup& rhs)
	{
		LL_ERRS() << "Illegal operation!" << LL_ENDL;
		return *this;
	}

	static U32 sNodeCount;
	static BOOL sNoDelete; //deletion of spatial groups and draw info not allowed if TRUE

	typedef std::vector<LLPointer<LLSpatialGroup> > sg_vector_t;
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_list_t;
	typedef std::vector<LLPointer<LLDrawInfo> > drawmap_elem_t; 
	typedef std::map<U32, drawmap_elem_t > draw_map_t;	
	typedef std::vector<LLPointer<LLVertexBuffer> > buffer_list_t;
	typedef std::vector<std::pair<LLFace*, buffer_list_t> > buffer_texture_vec_t;
	typedef std::vector<std::pair<U32, buffer_texture_vec_t> > buffer_vec_t;



	struct CompareDistanceGreater
	{
		bool operator()(const LLSpatialGroup* const& lhs, const LLSpatialGroup* const& rhs)
		{
			return lhs->mDistance > rhs->mDistance;
		}
	};

	struct CompareUpdateUrgency
	{
		bool operator()(const LLPointer<LLSpatialGroup> lhs, const LLPointer<LLSpatialGroup> rhs)
		{
			return lhs->getUpdateUrgency() > rhs->getUpdateUrgency();
		}
	};

	struct CompareDepthGreater
	{
		bool operator()(const LLSpatialGroup* const& lhs, const LLSpatialGroup* const& rhs)
		{
			return lhs->mDepth > rhs->mDepth;
		}
	};

	typedef enum
	{
		GEOM_DIRTY				= LLViewerOctreeGroup::INVALID_STATE,
		ALPHA_DIRTY				= (GEOM_DIRTY << 1),
		IN_IMAGE_QUEUE			= (ALPHA_DIRTY << 1),
		IMAGE_DIRTY				= (IN_IMAGE_QUEUE << 1),
		MESH_DIRTY				= (IMAGE_DIRTY << 1),
		NEW_DRAWINFO			= (MESH_DIRTY << 1),
		IN_BUILD_Q1				= (NEW_DRAWINFO << 1),
		IN_BUILD_Q2				= (IN_BUILD_Q1 << 1),
		STATE_MASK				= 0x0000FFFF,
	} eSpatialState;

	LLSpatialGroup(OctreeNode* node, LLSpatialPartition* part);

	BOOL isHUDGroup() ;
	
	void clearDrawMap();
	void validate();
	void validateDrawMap();
	
	void setState(eSpatialState state, S32 mode);
	void clearState(eSpatialState state, S32 mode);
	void clearState(eSpatialState state)     {mState &= ~state;}
	bool hasState(eSpatialState state) const {return mState & state;}

	LLSpatialGroup* getParent();

	BOOL addObject(LLDrawable *drawablep);
	BOOL removeObject(LLDrawable *drawablep, BOOL from_octree = FALSE);
	BOOL updateInGroup(LLDrawable *drawablep, BOOL immediate = FALSE); // Update position if it's in the group
	void shift(const LLVector4a &offset);
	void destroyGL(bool keep_occlusion = false);
	
	void updateDistance(LLCamera& camera);
	F32 getUpdateUrgency() const;
	BOOL changeLOD();
	void rebuildGeom();
	void rebuildMesh();

	void setState(eSpatialState state)       {mState |= state;}
	void dirtyGeom() { setState(GEOM_DIRTY); }
	void dirtyMesh() { setState(MESH_DIRTY); }

	void drawObjectBox(LLColor4 col);

	LLSpatialPartition* getSpatialPartition() const {return mSpatialPartition;}

	 //LISTENER FUNCTIONS
	void handleInsertion(const TreeNode* node, LLViewerOctreeEntry* face) final override;
	void handleRemoval(const TreeNode* node, LLViewerOctreeEntry* face) final override;
	void handleDestruction(const TreeNode* node) final override;
	void handleChildAddition(const OctreeNode* parent, OctreeNode* child) final override;
	

	LL_ALIGN_16(LLVector4a mViewAngle);
	LL_ALIGN_16(LLVector4a mLastUpdateViewAngle);

	F32 mObjectBoxSize; //cached mObjectBounds[1].getLength3()
		
protected:
	virtual ~LLSpatialGroup();

	static S32 sLODSeed;

public:
	bridge_list_t mBridgeList;
	buffer_vec_t mBufferVec; //used by volume buffers to store unique buffers per texture

	U32 mGeometryBytes; //used by volumes to track how many bytes of geometry data are in this node
	F32 mSurfaceArea; //used by volumes to track estimated surface area of geometry in this node

	F32 mBuilt;

	LLPointer<LLVertexBuffer> mVertexBuffer;

	U32 mBufferUsage;
	draw_map_t mDrawMap;
	
	F32 mDistance;
	F32 mDepth;
	F32 mLastUpdateDistance;
	F32 mLastUpdateTime;
	
	F32 mPixelArea;
	F32 mRadius;
} LL_ALIGN_POSTFIX(64);

inline LLSpatialGroup::eOcclusionState operator|(const LLSpatialGroup::eOcclusionState &a, const LLSpatialGroup::eOcclusionState &b) 
{
  return LLSpatialGroup::eOcclusionState(+a | +b);
}
inline LLSpatialGroup::eSpatialState operator|(const LLSpatialGroup::eSpatialState &a, const LLSpatialGroup::eSpatialState &b) 
{
  return LLSpatialGroup::eSpatialState(+a | +b);
}

class LLGeometryManager
{
public:
	std::vector<LLFace*> mFaceList;
	virtual ~LLGeometryManager() { }
	virtual void rebuildGeom(LLSpatialGroup* group) = 0;
	virtual void rebuildMesh(LLSpatialGroup* group) = 0;
	virtual void getGeometry(LLSpatialGroup* group) = 0;
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32 &index_count);
	
	virtual LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage);
};

class LLSpatialPartition: public LLViewerOctreePartition, public LLGeometryManager
{
public:
	//static BOOL sFreezeState; //if true, no spatialgroup state updates will be made
	LLSpatialPartition(U32 data_mask,  BOOL render_by_group, U32 mBufferUsage, LLViewerRegion* regionp);
	virtual ~LLSpatialPartition();

	LLSpatialGroup *put(LLDrawable *drawablep, BOOL was_visible = FALSE);
	BOOL remove(LLDrawable *drawablep, LLSpatialGroup *curp);
	
	LLDrawable* lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
									 BOOL pick_transparent, 
									 BOOL pick_rigged,
									 S32* face_hit,                          // return the face hit
									 LLVector4a* intersection = NULL,         // return the intersection point
									 LLVector2* tex_coord = NULL,            // return the texture coordinates of the intersection point
									 LLVector4a* normal = NULL,               // return the surface normal at the intersection point
									 LLVector4a* tangent = NULL             // return the surface tangent at the intersection point
		);
	
	
	// If the drawable moves, move it here.
	virtual void move(LLDrawable *drawablep, LLSpatialGroup *curp, BOOL immediate = FALSE);
	virtual void shift(const LLVector4a &offset);

	virtual F32 calcDistance(LLSpatialGroup* group, LLCamera& camera);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);

	virtual void rebuildGeom(LLSpatialGroup* group);
	virtual void rebuildMesh(LLSpatialGroup* group);

	BOOL visibleObjectsInFrustum(LLCamera& camera);
	/*virtual*/ S32 cull(LLCamera &camera, bool do_occlusion=false); // Cull on arbitrary frustum
	S32 cull(LLCamera &camera, std::vector<LLDrawable *>* results); // Cull on arbitrary frustum
	
	BOOL isVisible(const LLVector3& v);
	bool isHUDPartition() ;
	
	LLSpatialBridge* asBridge() { return mBridge; }
	BOOL isBridge() { return asBridge() != NULL; }

	void renderPhysicsShapes();
	void renderDebug();
	void renderIntersectingBBoxes(LLCamera* camera);
	void restoreGL();
	void resetVertexBuffers();
	BOOL getVisibleExtents(LLCamera& camera, LLVector3& visMin, LLVector3& visMax);

public:
	LLSpatialBridge* mBridge; // NULL for non-LLSpatialBridge instances, otherwise, mBridge == this
							// use a pointer instead of making "isBridge" and "asBridge" virtual so it's safe
							// to call asBridge() from the destructor

	BOOL mInfiniteFarClip; // if TRUE, frustum culling ignores far clip plane
	U32 mBufferUsage;
	const BOOL mRenderByGroup;
	U32 mVertexDataMask;
	F32 mSlopRatio; //percentage distance must change before drawables receive LOD update (default is 0.25);
	BOOL mDepthMask; //if TRUE, objects in this partition will be written to depth during alpha rendering

	static BOOL sTeleportRequested; //started to issue a teleport request
};

// class for creating bridges between spatial partitions
class LLSpatialBridge : public LLDrawable, public LLSpatialPartition
{
protected:
	~LLSpatialBridge();

public:
	typedef std::vector<LLPointer<LLSpatialBridge> > bridge_vector_t;
	
	LLSpatialBridge(LLDrawable* root, BOOL render_by_group, U32 data_mask, LLViewerRegion* regionp);
	
	void destroyTree();

	virtual BOOL isSpatialBridge() const		{ return TRUE; }
	virtual void updateSpatialExtents();
	virtual void updateBinRadius();
	virtual void setVisible(LLCamera& camera_in, std::vector<LLDrawable*>* results = NULL, BOOL for_select = FALSE);
	virtual void updateDistance(LLCamera& camera_in, bool force_update);
	virtual void makeActive();
	virtual void move(LLDrawable *drawablep, LLSpatialGroup *curp, BOOL immediate = FALSE);
	virtual BOOL updateMove();
	virtual void shiftPos(const LLVector4a& vec);
	virtual void cleanupReferences();
	virtual LLSpatialPartition* asPartition()		{ return this; }
		
	virtual LLCamera transformCamera(LLCamera& camera);
	
	LLDrawable* mDrawable;
};

class LLCullResult 
{
public:
	LLCullResult() {}

	typedef std::vector<LLPointer<LLSpatialGroup> > sg_list_t;
	typedef std::vector<LLDrawable*> drawable_list_t;
	typedef std::vector<LLSpatialBridge*> bridge_list_t;
	typedef std::vector<LLDrawInfo*> drawinfo_list_t;

	typedef sg_list_t::const_iterator sg_iterator;
	typedef bridge_list_t::const_iterator bridge_iterator;
	typedef drawinfo_list_t::const_iterator drawinfo_iterator;
	typedef drawable_list_t::const_iterator drawable_iterator;

	void clear();
	
	const sg_iterator beginVisibleGroups()			const { return mVisibleGroups.begin(); }
	const sg_iterator endVisibleGroups()			const { return mVisibleGroups.end(); }

	const sg_iterator beginAlphaGroups()			const { return mAlphaGroups.begin(); }
	const sg_iterator endAlphaGroups()				const { return mAlphaGroups.end(); }
	const sg_list_t::iterator beginAlphaGroups()		  { return mAlphaGroups.begin(); }
	const sg_list_t::iterator endAlphaGroups()			  { return mAlphaGroups.end(); }

	bool hasOcclusionGroups()						const {  return !mOcclusionGroups.empty(); }
	const sg_iterator beginOcclusionGroups()		const { return mOcclusionGroups.begin(); }
	const sg_iterator endOcclusionGroups()			const { return mOcclusionGroups.end(); }

	const sg_iterator beginDrawableGroups()			const { return mDrawableGroups.begin(); }
	const sg_iterator endDrawableGroups()			const { return mDrawableGroups.end(); }

	const drawable_iterator beginVisibleList()		const { return mVisibleList.begin(); }
	const drawable_iterator endVisibleList()		const { return mVisibleList.end(); }

	const bridge_iterator beginVisibleBridge()		const { return mVisibleBridge.begin(); }
	const bridge_iterator endVisibleBridge()		const { return mVisibleBridge.end(); }

	bool hasRenderMap(U32 type)						const { return !mRenderMap[type].empty(); }
	const drawinfo_iterator beginRenderMap(U32 type)const { return mRenderMap[type].begin(); }
	const drawinfo_iterator endRenderMap(U32 type)	const { return mRenderMap[type].end(); }

	void pushVisibleGroup(LLSpatialGroup* group)		  {  mVisibleGroups.push_back(group); }
	void pushAlphaGroup(LLSpatialGroup* group)			  {  mAlphaGroups.push_back(group); }
	void pushOcclusionGroup(LLSpatialGroup* group)		  {  mOcclusionGroups.push_back(group); }
	void pushDrawableGroup(LLSpatialGroup* group)		  {  mDrawableGroups.push_back(group); }
	void pushDrawable(LLDrawable* drawable)				  {  mVisibleList.push_back(drawable); }
	void pushBridge(LLSpatialBridge* bridge)			  {  mVisibleBridge.push_back(bridge); }
	void pushDrawInfo(U32 type, LLDrawInfo* draw_info)	  {  mRenderMap[type].push_back(draw_info); }

	void assertDrawMapsEmpty();

private:
	sg_list_t			mVisibleGroups;
	sg_list_t			mAlphaGroups;
	sg_list_t			mOcclusionGroups;
	sg_list_t			mDrawableGroups;
	drawable_list_t		mVisibleList;
	bridge_list_t		mVisibleBridge;
	drawinfo_list_t		mRenderMap[LLRenderPass::NUM_RENDER_TYPES];
};


//spatial partition for water (implemented in LLVOWater.cpp)
class LLWaterPartition : public LLSpatialPartition
{
public:
	LLWaterPartition(LLViewerRegion* regionp);
	virtual void getGeometry(LLSpatialGroup* group) {  }
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) { }
};

//spatial partition for hole and edge water (implemented in LLVOWater.cpp)
class LLVoidWaterPartition : public LLWaterPartition
{
public:
	LLVoidWaterPartition(LLViewerRegion* regionp);
};

//spatial partition for terrain (impelmented in LLVOSurfacePatch.cpp)
class LLTerrainPartition : public LLSpatialPartition
{
public:
	LLTerrainPartition(LLViewerRegion* regionp);
	virtual void getGeometry(LLSpatialGroup* group);
	virtual LLVertexBuffer* createVertexBuffer(U32 type_mask, U32 usage);
};

//spatial partition for trees
class LLTreePartition : public LLSpatialPartition
{
public:
	LLTreePartition(LLViewerRegion* regionp);
	virtual void getGeometry(LLSpatialGroup* group) { }
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) { }

};

//spatial partition for particles (implemented in LLVOPartGroup.cpp)
class LLParticlePartition : public LLSpatialPartition
{
public:
	LLParticlePartition(LLViewerRegion* regionp);
	virtual void rebuildGeom(LLSpatialGroup* group);
	virtual void getGeometry(LLSpatialGroup* group);
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);
protected:
	U32 mRenderPass;
};

class LLHUDParticlePartition : public LLParticlePartition
{
public:
	LLHUDParticlePartition(LLViewerRegion* regionp);
};

//spatial partition for grass (implemented in LLVOGrass.cpp)
class LLGrassPartition : public LLSpatialPartition
{
public:
	LLGrassPartition(LLViewerRegion* regionp);
	virtual void getGeometry(LLSpatialGroup* group);
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count);
protected:
	U32 mRenderPass;
};

//spatial partition for clouds (implemented in LLVOClouds.cpp)
#if ENABLE_CLASSIC_CLOUDS
class LLCloudPartition : public LLParticlePartition
{
public:
	LLCloudPartition(LLViewerRegion* regionp);
};
#endif

//class for wrangling geometry out of volumes (implemented in LLVOVolume.cpp)
class LLVolumeGeometryManager: public LLGeometryManager
{
 public:
	typedef enum
	{
		NONE = 0,
		BATCH_SORT,
		DISTANCE_SORT
	} eSortType;

	LLVolumeGeometryManager();
	virtual ~LLVolumeGeometryManager();
	virtual void rebuildGeom(LLSpatialGroup* group);
	virtual void rebuildMesh(LLSpatialGroup* group);
	virtual void getGeometry(LLSpatialGroup* group);
	void genDrawInfo(LLSpatialGroup* group, U32 mask, LLFace** faces, U32 face_count, BOOL distance_sort = FALSE, BOOL batch_textures = FALSE);
	void registerFace(LLSpatialGroup* group, LLFace* facep, U32 type);
private:
	void allocateFaces(U32 pMaxFaceCount);
	void freeFaces();

	static int32_t sInstanceCount;
	static LLFace** sFullbrightFaces;
	static LLFace** sBumpFaces;
	static LLFace** sSimpleFaces;
	static LLFace** sNormFaces;
	static LLFace** sSpecFaces;
	static LLFace** sNormSpecFaces;
	static LLFace** sAlphaFaces;
};

//spatial partition that uses volume geometry manager (implemented in LLVOVolume.cpp)
class LLVolumePartition : public LLSpatialPartition, public LLVolumeGeometryManager
{
public:
	LLVolumePartition(LLViewerRegion* regionp);
	virtual void rebuildGeom(LLSpatialGroup* group) { LLVolumeGeometryManager::rebuildGeom(group); }
	virtual void getGeometry(LLSpatialGroup* group) { LLVolumeGeometryManager::getGeometry(group); }
	virtual void rebuildMesh(LLSpatialGroup* group) { LLVolumeGeometryManager::rebuildMesh(group); }
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) { LLVolumeGeometryManager::addGeometryCount(group, vertex_count, index_count); }
};

//spatial bridge that uses volume geometry manager (implemented in LLVOVolume.cpp)
class LLVolumeBridge : public LLSpatialBridge, public LLVolumeGeometryManager
{
public:
	LLVolumeBridge(LLDrawable* drawable, LLViewerRegion* regionp);
	virtual void rebuildGeom(LLSpatialGroup* group) { LLVolumeGeometryManager::rebuildGeom(group); }
	virtual void getGeometry(LLSpatialGroup* group) { LLVolumeGeometryManager::getGeometry(group); }
	virtual void rebuildMesh(LLSpatialGroup* group) { LLVolumeGeometryManager::rebuildMesh(group); }
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) { LLVolumeGeometryManager::addGeometryCount(group, vertex_count, index_count); }
};

class LLAttachmentBridge : public LLVolumeBridge
{
public:
	LLAttachmentBridge(LLDrawable* drawable, LLViewerRegion* regionp);
};

class LLHUDBridge : public LLVolumeBridge
{
public:
	LLHUDBridge(LLDrawable* drawablep, LLViewerRegion* regionp);
	virtual void shiftPos(const LLVector4a& vec);
	virtual F32 calcPixelArea(LLSpatialGroup* group, LLCamera& camera);
};

//spatial partition that holds nothing but spatial bridges
class LLBridgePartition : public LLSpatialPartition
{
public:
	LLBridgePartition(LLViewerRegion* regionp);
	virtual void getGeometry(LLSpatialGroup* group) { }
	virtual void addGeometryCount(LLSpatialGroup* group, U32 &vertex_count, U32& index_count) {  }
};

//spatial partition that holds nothing but spatial bridges
class LLAttachmentPartition : public LLBridgePartition
{
public:
	LLAttachmentPartition(LLViewerRegion* regionp);
};

class LLHUDPartition : public LLBridgePartition
{
public:
	LLHUDPartition(LLViewerRegion* regionp);
	virtual void shift(const LLVector4a &offset) { }	//HUD objects don't shift with region crossing.  That would be silly.
};

extern const F32 SG_BOX_SIDE;
extern const F32 SG_BOX_OFFSET;
extern const F32 SG_BOX_RAD;

extern const F32 SG_OBJ_SIDE;
extern const F32 SG_MAX_OBJ_RAD;


#endif //LL_LLSPATIALPARTITION_H

