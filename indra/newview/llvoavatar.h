/**
 * @file llvoavatar.h
 * @brief Declaration of LLVOAvatar class which is a derivation of
 * LLViewerObject
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

#ifndef LL_VOAVATAR_H
#define LL_VOAVATAR_H

#include <map>
#include <deque>
#include <string>
#include <vector>

#include <boost/signals2.hpp>

#include "imageids.h"			// IMG_INVISIBLE
#include "llavatarappearance.h"
#include "llchat.h"
#include "lldrawpoolalpha.h"
#include "llviewerobject.h"
#include "llcharacter.h"
#include "llcontrol.h"
#include "llviewerjointmesh.h"
#include "llviewerjointattachment.h"
#include "llrendertarget.h"
#include "llavatarappearancedefines.h"
#include "lltexglobalcolor.h"
#include "lldriverparam.h"
#include "llviewertexlayer.h"
#include "material_codes.h"		// LL_MCODE_END
#include "llrigginginfo.h"
#include "llviewerstats.h"
#include "llvovolume.h"
#include "llavatarname.h"

//<singu>
#if 0
// Hide these: should be using the bit masks everywhere.
extern const LLUUID ANIM_AGENT_BODY_NOISE;
extern const LLUUID ANIM_AGENT_BREATHE_ROT;
extern const LLUUID ANIM_AGENT_PHYSICS_MOTION;
extern const LLUUID ANIM_AGENT_EDITING;
extern const LLUUID ANIM_AGENT_EYE;
extern const LLUUID ANIM_AGENT_FLY_ADJUST;
extern const LLUUID ANIM_AGENT_HAND_MOTION;
extern const LLUUID ANIM_AGENT_HEAD_ROT;
extern const LLUUID ANIM_AGENT_PELVIS_FIX;
extern const LLUUID ANIM_AGENT_TARGET;
extern const LLUUID ANIM_AGENT_WALK_ADJUST;
#endif
//</singu>

class LLAPRFile;
class LLViewerWearable;
class LLVoiceVisualizer;
class LLHUDNameTag;
class LLHUDEffectSpiral;
class LLTexGlobalColor;
class LLViewerJoint;
struct LLAppearanceMessageContents;
class LLMeshSkinInfo;
class LLViewerJointMesh;
class LLControlAvatar;

class SHClientTagMgr : public LLSingleton<SHClientTagMgr>, public boost::signals2::trackable
{
public:
	SHClientTagMgr();
	//Fetch updated client_tags_sg1.xml from server
	bool fetchDefinitions() const;
	//Parse definitions from client_tags_sg1.xml
	bool parseDefinitions();
private:
	//Just refreshes the tag for the agent avatar.
	//Used for boost::bind to verify agent avatar is valid before calling updateAvatarTag
	void updateAgentAvatarTag();
	//Actually generate the tag information for the avatar
	const LLSD generateClientTag(const LLVOAvatar* pAvatar) const;
public:
	//Generates tag for particular avatar and sets dirty if the tag has changed. Call on received TE updates.
	void updateAvatarTag(LLVOAvatar* pAvatar);
	//Removes entry in mAvatarTags map. Call on destruction of avatar.
	void clearAvatarTag(const LLVOAvatar* pAvatar);

	//	Accessors

	//Returns true if tag system is enabled.
	bool getIsEnabled() const;
	//If a tag entry exists, returns the clientname if found. If is_friend is set, will return "Friend" if AscentShowFriendsTag is true.
	const std::string getClientName(const LLVOAvatar* pAvatar, bool is_friend) const;
	//Returns ID of tag entry if it exists, else returns a null LLUUID.
	const LLUUID getClientID(const LLVOAvatar* pAvatar) const;
	//Sets color to either 'status' color, or falls back to client color if status isn't relevant.
	//If neither status or client color are found, returns false.
	bool getClientColor(const LLVOAvatar* pAvatar, bool check_status, LLColor4& color) const;

private:
	//UUID map associating texture uuids to client info entries parsed from client_tags_sg1.xml
	std::map<LLUUID, LLSD> mClientDefinitions;
	//UUID map associating avatar uuids to their CURRENT client tag info.
	std::map<LLUUID, LLSD> mAvatarTags;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLVOAvatar
// 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLVOAvatar :
	public LLAvatarAppearance,
	public LLViewerObject,
	public boost::signals2::trackable
{
	LOG_CLASS(LLVOAvatar);

public:
	friend class LLVOAvatarSelf;

/********************************************************************************
 **                                                                            **
 **                    INITIALIZATION
 **/

public:
	void* operator new(size_t size)
	{
		return ll_aligned_malloc_16(size);
	}

	void operator delete(void* ptr)
	{
		ll_aligned_free_16(ptr);
	}

	LLVOAvatar(const LLUUID &id, const LLPCode pcode, LLViewerRegion *regionp);
	virtual void		markDead();
	static void			initClass(); // Initialize data that's only init'd once per class.
	static void			cleanupClass();	// Cleanup data that's only init'd once per class.
	virtual void 		initInstance(); // Called after construction to initialize the class.
protected:
	virtual				~LLVOAvatar();

/**                    Initialization
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    INHERITED
 **/

	//--------------------------------------------------------------------
	// LLViewerObject interface and related
	//--------------------------------------------------------------------
public:
	/*virtual*/ void			updateGL();
	/*virtual*/ LLVOAvatar*		asAvatar();
	virtual U32    	 	 	processUpdateMessage(LLMessageSystem *mesgsys,
													 void **user_data,
													 U32 block_num,
													 const EObjectUpdateType update_type,
													 LLDataPacker *dp);
	virtual void   	 	 	idleUpdate(LLAgent &agent, LLWorld &world, const F64 &time);
	/*virtual*/ BOOL   	 	 	updateLOD();
	BOOL  	 	 	 	 	updateJointLODs();
	void						updateLODRiggedAttachments( void );
	void						updateSoftwareSkinnedVertices(const LLMeshSkinInfo* skin, const LLVector4a* weight, const LLVolumeFace& vol_face, LLVertexBuffer *buffer);
	/*virtual*/ BOOL   	 	 	isActive() const; // Whether this object needs to do an idleUpdate.
	S32Bytes					totalTextureMemForUUIDS(uuid_set_t& ids);
	bool 						allTexturesCompletelyDownloaded(uuid_set_t& ids) const;
	bool 						allLocalTexturesCompletelyDownloaded() const;
	bool 						allBakedTexturesCompletelyDownloaded() const;
	void 						bakedTextureOriginCounts(S32 &sb_count, S32 &host_count,
														 S32 &both_count, S32 &neither_count);
	std::string 				bakedTextureOriginInfo();
	void 						collectLocalTextureUUIDs(uuid_set_t& ids) const;
	void 						collectBakedTextureUUIDs(uuid_set_t& ids) const;
	void 						collectTextureUUIDs(uuid_set_t& ids);
	void						releaseOldTextures();
	/*virtual*/ void   	 	 	updateTextures();
	LLViewerFetchedTexture*		getBakedTextureImage(const U8 te, const LLUUID& uuid);
	/*virtual*/ S32    	 	 	setTETexture(const U8 te, const LLUUID& uuid); // If setting a baked texture, need to request it from a non-local sim.
	/*virtual*/ void   	 	 	onShift(const LLVector4a& shift_vector);
	/*virtual*/ U32    	 	 	getPartitionType() const;
	/*virtual*/ const  	 	 	LLVector3 getRenderPosition() const;
	/*virtual*/ void   	 	 	updateDrawable(BOOL force_damped);
	/*virtual*/ LLDrawable* 	createDrawable(LLPipeline *pipeline);
	/*virtual*/ BOOL   	 	 	updateGeometry(LLDrawable *drawable);
	/*virtual*/ void   	 	 	setPixelAreaAndAngle(LLAgent &agent);
	/*virtual*/ void   	 	 	updateRegion(LLViewerRegion *regionp);
	/*virtual*/ void   	 	 	updateSpatialExtents(LLVector4a& newMin, LLVector4a &newMax);
	void			   	 	 	calculateSpatialExtents(LLVector4a& newMin, LLVector4a& newMax);
	/*virtual*/ BOOL   	 	 	lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
												 S32 face = -1,                    // which face to check, -1 = ALL_SIDES
												 BOOL pick_transparent = FALSE,
												 BOOL pick_rigged = FALSE,
												 S32* face_hit = NULL,             // which face was hit
												 LLVector4a* intersection = NULL,   // return the intersection point
												 LLVector2* tex_coord = NULL,      // return the texture coordinates of the intersection point
												 LLVector4a* normal = NULL,         // return the surface normal at the intersection point
												 LLVector4a* tangent = NULL);     // return the surface tangent at the intersection point
	virtual LLViewerObject*	lineSegmentIntersectRiggedAttachments(
                                                 const LLVector4a& start, const LLVector4a& end,
												 S32 face = -1,                    // which face to check, -1 = ALL_SIDES
												 BOOL pick_transparent = FALSE,
												 BOOL pick_rigged = FALSE,
												 S32* face_hit = NULL,             // which face was hit
												 LLVector4a* intersection = NULL,   // return the intersection point
												 LLVector2* tex_coord = NULL,      // return the texture coordinates of the intersection point
												 LLVector4a* normal = NULL,         // return the surface normal at the intersection point
												 LLVector4a* tangent = NULL);     // return the surface tangent at the intersection point

	//--------------------------------------------------------------------
	// LLCharacter interface and related
	//--------------------------------------------------------------------
public:
	/*virtual*/ LLVector3    	getCharacterPosition();
	/*virtual*/ LLQuaternion 	getCharacterRotation();
	/*virtual*/ LLVector3    	getCharacterVelocity();
	/*virtual*/ LLVector3    	getCharacterAngularVelocity();

	/*virtual*/ LLUUID			remapMotionID(const LLUUID& id);
	/*virtual*/ BOOL			startMotion(const LLUUID& id, F32 time_offset = 0.f);
	/*virtual*/ BOOL			stopMotion(const LLUUID& id, BOOL stop_immediate = FALSE);
	//<singu>
	void						startMotion(U32 bit, F32 start_offset = 0.f);
	void						stopMotion(U32 bit, BOOL stop_immediate = FALSE);
	//</singu>
	virtual bool			hasMotionFromSource(const LLUUID& source_id);
	virtual void			stopMotionFromSource(const LLUUID& source_id);
	virtual void			requestStopMotion(LLMotion* motion);
	LLMotion*				findMotion(const LLUUID& id) const;
	void					startDefaultMotions();
	void					dumpAnimationState();

	virtual LLJoint*		getJoint(const std::string &name);
	LLJoint*				getJoint(S32 num);
	
	void 					addAttachmentOverridesForObject(LLViewerObject *vo, uuid_set_t* meshes_seen = NULL, bool recursive = true);
	void					removeAttachmentOverridesForObject(const LLUUID& mesh_id);
	void					removeAttachmentOverridesForObject(LLViewerObject *vo);
	bool					jointIsRiggedTo(const LLJoint *joint) const;
	void					clearAttachmentOverrides();
	void					rebuildAttachmentOverrides();
	void					updateAttachmentOverrides();
	void					showAttachmentOverrides(bool verbose = false) const;
	void					getAttachmentOverrideNames(	std::set<std::string>& pos_names, 
														std::set<std::string>& scale_names) const;

    void 					getAssociatedVolumes(std::vector<LLVOVolume*>& volumes);

    // virtual
    void 					updateRiggingInfo();
	// This encodes mesh id and LOD, so we can see whether display is up-to-date.
	std::map<LLUUID,S32>	mLastRiggingInfoKey;
	
    uuid_set_t		mActiveOverrideMeshes;
    virtual void			onActiveOverrideMeshesChanged();
    
	/*virtual*/ const LLUUID&	getID() const;
	/*virtual*/ void			addDebugText(const std::string& text);
	/*virtual*/ F32				getTimeDilation();
	/*virtual*/ void			getGround(const LLVector3 &inPos, LLVector3 &outPos, LLVector3 &outNorm);
	/*virtual*/ F32				getPixelArea() const;
	/*virtual*/ LLVector3d		getPosGlobalFromAgent(const LLVector3 &position);
	/*virtual*/ LLVector3		getPosAgentFromGlobal(const LLVector3d &position);
	virtual void			updateVisualParams();


/**                    Inherited
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    STATE
 **/

public:
	virtual bool 	isSelf() const { return false; } // True if this avatar is for this viewer's agent

	bool 	isControlAvatar() const { return mIsControlAvatar; } // True if this avatar is a control av (no associated user)
	virtual LLControlAvatar* asControlAvatar() { return nullptr; }
	bool 	isUIAvatar() const { return mIsUIAvatar; } // True if this avatar is a supplemental av used in some UI views (no associated user)
private: //aligned members
	LL_ALIGN_16(LLVector4a	mImpostorExtents[2]);

private:
	BOOL			mSupportsAlphaLayers; // For backwards compatibility, TRUE for 1.23+ clients

	//--------------------------------------------------------------------
	// Updates
	//--------------------------------------------------------------------
public:
    void			updateAppearanceMessageDebugText();
	void 			updateAnimationDebugText();
	virtual void	updateDebugText();
	virtual BOOL 	updateCharacter(LLAgent &agent);
    void			updateFootstepSounds();
    void			computeUpdatePeriod();
    void			updateOrientation(LLAgent &agent, F32 speed, F32 delta_time);
    void			updateTimeStep();
    void			updateRootPositionAndRotation(LLAgent &agent, F32 speed, bool was_sit_ground_constrained);
    
	void 			idleUpdateVoiceVisualizer(bool voice_enabled);
	void 			idleUpdateMisc(bool detailed_update);
	virtual void	idleUpdateAppearanceAnimation();
	void 			idleUpdateLipSync(bool voice_enabled);
	void 			idleUpdateLoadingEffect();
	void 			idleUpdateWindEffect();
	void 			idleUpdateNameTag(const LLVector3& root_pos_last);
	void			idleUpdateNameTagText(BOOL new_name);
	LLVector3		idleUpdateNameTagPosition(const LLVector3& root_pos_last);
	void			idleUpdateNameTagAlpha(BOOL new_name, F32 alpha);
	LLColor4		getNameTagColor(bool is_friend);
	void			clearNameTag();
	static void		invalidateNameTag(const LLUUID& agent_id);
	// force all name tags to rebuild, useful when display names turned on/off
	static void		invalidateNameTags();
	void			addNameTagLine(const std::string& line, const LLColor4& color, S32 style, const LLFontGL* font);
	void 			idleUpdateRenderComplexity();
    void 			accountRenderComplexityForObject(const LLViewerObject *attached_object,
                                                     const F32 max_attachment_complexity,
                                                     LLVOVolume::texture_cost_t& textures,
                                                     U32& cost/*,
                                                     hud_complexity_list_t& hud_complexity_list*/);
	void			calculateUpdateRenderComplexity();
	static const U32 VISUAL_COMPLEXITY_UNKNOWN;
	void			updateVisualComplexity();

	U32				getVisualComplexity()			{ return mVisualComplexity;				};		// Numbers calculated here by rendering AV
	F32				getAttachmentSurfaceArea()		{ return mAttachmentSurfaceArea;		};		// estimated surface area of attachments

	U32				getReportedVisualComplexity()					{ return mReportedVisualComplexity;				};	// Numbers as reported by the SL server
	void			setReportedVisualComplexity(S32 value)			{ mReportedVisualComplexity = value;			};

	S32				getUpdatePeriod()				{ return mUpdatePeriod;			};

	static void		updateImpostorRendering(U32 newMaxNonImpostorsValue);

	void 			idleUpdateBelowWater();

	//--------------------------------------------------------------------
	// Static preferences (controlled by user settings/menus)
	//--------------------------------------------------------------------
public:
	static S32		sRenderName;
	static bool		sRenderGroupTitles;
	static U32		sMaxVisible; //(affected by control "RenderAvatarMaxVisible")
	static F32		sRenderDistance; //distance at which avatars will render.
	static BOOL		sShowAnimationDebug; // show animation debug info
	static bool		sUseImpostors; //use impostors for far away avatars
	static bool		sShowFootPlane;	// show foot collision plane reported by server
	static bool		sVisibleInFirstPerson;
	static S32		sNumLODChangesThisFrame;
	static S32		sNumVisibleChatBubbles;
	static BOOL		sDebugInvisible;
	static bool		sShowAttachmentPoints;
	static F32		sLODFactor; // user-settable LOD factor
	static F32		sPhysicsLODFactor; // user-settable physics LOD factor
	static BOOL		sJointDebug; // output total number of joints being touched for each avatar
	static BOOL		sDebugAvatarRotation;

	//--------------------------------------------------------------------
	// Region state
	//--------------------------------------------------------------------
public:
	LLHost			getObjectHost() const;

	//--------------------------------------------------------------------
	// Loading state
	//--------------------------------------------------------------------
public:
	BOOL			isFullyLoaded() const;
	bool			isTooComplex() const;
	bool 			visualParamWeightsAreDefault();
	virtual BOOL	getIsCloud() const;
	BOOL			isFullyTextured() const;
	BOOL			hasGray() const; 
	S32				getRezzedStatus() const; // 0 = cloud, 1 = gray, 2 = textured, 3 = textured and fully downloaded.
	void			updateRezzedStatusTimers();
	bool			isLangolier() const { return mFreezeTimeLangolier; }
	bool			isFrozenDead() const { return mFreezeTimeDead; }

	S32				mLastRezzedStatus;


	void 			startPhase(const std::string& phase_name);
	void 			stopPhase(const std::string& phase_name, bool err_check = true);
	void			clearPhases();
	void 			logPendingPhases();
	static void 	logPendingPhasesAllAvatars();
	void 			logMetricsTimerRecord(const std::string& phase_name, F32 elapsed, bool completed);

protected:
	LLViewerStats::PhaseMap& getPhases() { return mPhases; }
	BOOL			updateIsFullyLoaded();
	BOOL			processFullyLoadedChange(bool loading);
	void			updateRuthTimer(bool loading);
	F32 			calcMorphAmount();

private:
	BOOL			mFirstFullyVisible;
	BOOL			mFullyLoaded;
	BOOL			mPreviousFullyLoaded;
	BOOL			mFullyLoadedInitialized;
	S32				mFullyLoadedFrameCounter;
	LLFrameTimer	mFullyLoadedTimer;
	LLFrameTimer	mRuthTimer;
	bool			mFreezeTimeLangolier;	// True when this avatar was created during snapshot FreezeTime mode, and that mode is still active.
	bool			mFreezeTimeDead;		// True when the avatar was marked dead (ie, TP-ed away) while in FreezeTime mode.

private:
	LLViewerStats::PhaseMap mPhases;

protected:
	LLFrameTimer    mInvisibleTimer;
	
/**                    State
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SKELETON
 **/

protected:
	/*virtual*/ LLAvatarJoint*	createAvatarJoint(); // Returns LLViewerJoint
	/*virtual*/ LLAvatarJoint*	createAvatarJoint(S32 joint_num); // Returns LLViewerJoint
	/*virtual*/ LLAvatarJointMesh*	createAvatarJointMesh(); // Returns LLViewerJointMesh
public:
	void				updateHeadOffset();
	void				postPelvisSetRecalc( void );

	/*virtual*/ BOOL	loadSkeletonNode();
    void                initAttachmentPoints(bool ignore_hud_joints = false);
	/*virtual*/ void	buildCharacter();
	void				resetVisualParams();
	void				resetSkeleton(bool reset_animations);

	LLVector3			mCurRootToHeadOffset;
	LLVector3			mTargetRootToHeadOffset;

	S32					mLastSkeletonSerialNum;


/**                    Skeleton
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    RENDERING
 **/

public:
	U32 		renderImpostor(LLColor4U color = LLColor4U(255,255,255,255), S32 diffuse_channel = 0);
	bool		isVisuallyMuted() const;
	void		resetFreezeTime();

	U32 		renderRigid();
	U32 		renderSkinned(EAvatarRenderPass pass);
	F32			getLastSkinTime() { return mLastSkinTime; }
	U32			renderSkinnedAttachments();
	U32 		renderTransparent(BOOL first_pass);
	void 		renderCollisionVolumes();
	void		renderBones();
	void		renderJoints();
	static void	deleteCachedImages(bool clearAll=true);
	static void	destroyGL();
	static void	restoreGL();
	S32			mSpecialRenderMode; // special lighting
private:
	F32			mAttachmentSurfaceArea; //estimated surface area of attachments
	U32			mAttachmentVisibleTriangleCount;
	F32			mAttachmentEstTriangleCount;
	bool		shouldAlphaMask();

	BOOL 		mNeedsSkin; // avatar has been animated and verts have not been updated
	F32			mLastSkinTime; //value of gFrameTimeSeconds at last skin update

	S32	 		mUpdatePeriod;
	S32  		mNumInitFaces; //number of faces generated when creating the avatar drawable, does not inculde splitted faces due to long vertex buffer.

	// the isTooComplex method uses these mutable values to avoid recalculating too frequently
	mutable U32  mVisualComplexity;
	mutable bool mVisualComplexityStale;
	U32			 mReportedVisualComplexity; // from other viewers through the simulator

	//--------------------------------------------------------------------
	// animated object status
	//--------------------------------------------------------------------
public:
    bool mIsControlAvatar;
    bool mIsUIAvatar;
    bool mEnableDefaultMotions;

	//--------------------------------------------------------------------
	// Morph masks
	//--------------------------------------------------------------------
public:
	/*virtual*/ void	applyMorphMask(U8* tex_data, S32 width, S32 height, S32 num_components, LLAvatarAppearanceDefines::EBakedTextureIndex index = LLAvatarAppearanceDefines::BAKED_NUM_INDICES);
	BOOL 		morphMaskNeedsUpdate(LLAvatarAppearanceDefines::EBakedTextureIndex index = LLAvatarAppearanceDefines::BAKED_NUM_INDICES);

	
	//--------------------------------------------------------------------
	// Global colors
	//--------------------------------------------------------------------
public:
	/*virtual*/void onGlobalColorChanged(const LLTexGlobalColor* global_color, bool upload_bake = false);

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
protected:
	void 		updateVisibility();
private:
	U32	 		mVisibilityRank;
	BOOL 		mVisible;
	
	//--------------------------------------------------------------------
	// Shadowing
	//--------------------------------------------------------------------
public:
	void 		updateShadowFaces();
	LLDrawable*	mShadow;
private:
	LLFace* 	mShadow0Facep;
	LLFace* 	mShadow1Facep;
	LLPointer<LLViewerTexture> mShadowImagep;

	//--------------------------------------------------------------------
	// Impostors
	//--------------------------------------------------------------------
public:
	virtual BOOL isImpostor() const;
	BOOL 		shouldImpostor(const U32 rank_factor = 1) const;
	BOOL 		needsImpostorUpdate() const;
	const LLVector3& getImpostorOffset() const;
	const LLVector2& getImpostorDim() const;
	void 		getImpostorValues(LLVector4a* extents, LLVector3& angle, F32& distance) const;
	void 		cacheImpostorValues();
	void 		setImpostorDim(const LLVector2& dim);
	static void	resetImpostors();
	static void updateImpostors();
	LLRenderTarget mImpostor;
	BOOL		mNeedsImpostorUpdate;
	F32SecondsImplicit mLastImpostorUpdateFrameTime;
	const LLVector3*  getLastAnimExtents() const { return mLastAnimExtents; }
	void		setNeedsExtentUpdate(bool val) { mNeedsExtentUpdate = val; }

private:
	LLVector3	mImpostorOffset;
	LLVector2	mImpostorDim;
	BOOL		mNeedsAnimUpdate;
	bool		mNeedsExtentUpdate;
	LLVector3	mImpostorAngle;
	F32			mImpostorDistance;
	F32			mImpostorPixelArea;
	LLVector3	mLastAnimExtents[2];  
	LLVector3	mLastAnimBasePos;

	//--------------------------------------------------------------------
	// Wind rippling in clothes
	//--------------------------------------------------------------------
public:
	LLVector4	mWindVec;
	F32			mRipplePhase;
	BOOL		mBelowWater;
private:
	F32			mWindFreq;
	LLFrameTimer mRippleTimer;
	F32			mRippleTimeLast;
	LLVector3	mRippleAccel;
	LLVector3	mLastVel;

	//--------------------------------------------------------------------
	// Culling
	//--------------------------------------------------------------------
public:
	static void	cullAvatarsByPixelArea();
	BOOL		isCulled() const { return mCulled; }
private:
	BOOL		mCulled;

	//--------------------------------------------------------------------
	// Freeze counter
	//--------------------------------------------------------------------
public:
	static void updateFreezeCounter(S32 counter = 0);
private:
	static S32  sFreezeCounter;

	//--------------------------------------------------------------------
	// Constants
	//--------------------------------------------------------------------
public:
	virtual LLViewerTexture::EBoostLevel 	getAvatarBoostLevel() const { return LLGLTexture::BOOST_AVATAR; }
	virtual LLViewerTexture::EBoostLevel 	getAvatarBakedBoostLevel() const { return LLGLTexture::BOOST_AVATAR_BAKED; }
	virtual S32 						getTexImageSize() const;
	/*virtual*/ S32						getTexImageArea() const { return getTexImageSize()*getTexImageSize(); }

/**                    Rendering
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    TEXTURES
 **/

	//--------------------------------------------------------------------
	// Loading status
	//--------------------------------------------------------------------
public:
	virtual BOOL    isTextureDefined(LLAvatarAppearanceDefines::ETextureIndex type, U32 index = 0) const;
	virtual BOOL	isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type, U32 index = 0) const;
	virtual BOOL	isTextureVisible(LLAvatarAppearanceDefines::ETextureIndex type, LLViewerWearable *wearable) const;

	BOOL			isFullyBaked();
	static BOOL		areAllNearbyInstancesBaked(S32& grey_avatars);
	static void		getNearbyRezzedStats(std::vector<S32>& counts);
	static std::string rezStatusToString(S32 status);

	//--------------------------------------------------------------------
	// Baked textures
	//--------------------------------------------------------------------
public:
	/*virtual*/ LLTexLayerSet*	createTexLayerSet(); // Return LLViewerTexLayerSet
	void			releaseComponentTextures(); // ! BACKWARDS COMPATIBILITY !
protected:
	static void		onBakedTextureMasksLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	static void		onInitialBakedTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	static void		onBakedTextureLoaded(BOOL success, LLViewerFetchedTexture *src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata);
	virtual void	removeMissingBakedTextures();
	void			useBakedTexture(const LLUUID& id);
	LLViewerTexLayerSet*  getTexLayerSet(const U32 index) const { return dynamic_cast<LLViewerTexLayerSet*>(mBakedTextureDatas[index].mTexLayerSet);	}


	LLLoadedCallbackEntry::source_callback_list_t mCallbackTextureList ; 
	BOOL mLoadedCallbacksPaused;
	uuid_set_t	mTextureIDs;
	//--------------------------------------------------------------------
	// Local Textures
	//--------------------------------------------------------------------
protected:
	virtual void	setLocalTexture(LLAvatarAppearanceDefines::ETextureIndex type, LLViewerTexture* tex, BOOL baked_version_exits, U32 index = 0);
	virtual void	addLocalTextureStats(LLAvatarAppearanceDefines::ETextureIndex type, LLViewerFetchedTexture* imagep, F32 texel_area_ratio, BOOL rendered, BOOL covered_by_baked);
	// MULTI-WEARABLE: make self-only?
	virtual void	setBakedReady(LLAvatarAppearanceDefines::ETextureIndex type, BOOL baked_version_exists, U32 index = 0);

	//--------------------------------------------------------------------
	// Texture accessors
	//--------------------------------------------------------------------
private:
	virtual	void				setImage(const U8 te, LLViewerTexture *imagep, const U32 index); 
	virtual LLViewerTexture*	getImage(const U8 te, const U32 index) const;
	const std::string 			getImageURL(const U8 te, const LLUUID &uuid);

	virtual const LLTextureEntry* getTexEntry(const U8 te_num) const;
	virtual void setTexEntry(const U8 index, const LLTextureEntry &te);

	void checkTextureLoading() ;
	//--------------------------------------------------------------------
	// Layers
	//--------------------------------------------------------------------
protected:
	void			deleteLayerSetCaches(bool clearAll = true);
	void			addBakedTextureStats(LLViewerFetchedTexture* imagep, F32 pixel_area, F32 texel_area_ratio, S32 boost_level);

	//--------------------------------------------------------------------
	// Composites
	//--------------------------------------------------------------------
public:
	virtual void	invalidateComposite(LLTexLayerSet* layerset, BOOL upload_result);
	virtual void	invalidateAll();
	virtual void	setCompositeUpdatesEnabled(bool b) {}
	virtual void 	setCompositeUpdatesEnabled(U32 index, bool b) {}
	virtual bool 	isCompositeUpdateEnabled(U32 index) { return false; }

	//--------------------------------------------------------------------
	// Static texture/mesh/baked dictionary
	//--------------------------------------------------------------------
public:
	static BOOL 	isIndexLocalTexture(LLAvatarAppearanceDefines::ETextureIndex i);
	static BOOL 	isIndexBakedTexture(LLAvatarAppearanceDefines::ETextureIndex i);
private:
	static const LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary *getDictionary() { return sAvatarDictionary; }
	static LLAvatarAppearanceDefines::LLAvatarAppearanceDictionary* sAvatarDictionary;

	//--------------------------------------------------------------------
	// Messaging
	//--------------------------------------------------------------------
public:
	void 			onFirstTEMessageReceived();
private:
	BOOL			mFirstTEMessageReceived;
	BOOL			mFirstAppearanceMessageReceived;
	
/**                    Textures
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    MESHES
 **/

public:
	void			debugColorizeSubMeshes(U32 i, const LLColor4& color);
	virtual void 	updateMeshTextures();
	void 			updateSexDependentLayerSets(bool upload_bake = false);
	virtual void	dirtyMesh(); // Dirty the avatar mesh
	void 			updateMeshData();
	void			updateMeshVisibility();
	LLViewerTexture*		getBakedTexture(const U8 te);

protected:
	void 			releaseMeshData();
	virtual void restoreMeshData();
private:
	virtual void	dirtyMesh(S32 priority); // Dirty the avatar mesh, with priority
	LLViewerJoint*	getViewerJoint(S32 idx);
	S32 			mDirtyMesh; // 0 -- not dirty, 1 -- morphed, 2 -- LOD
	BOOL			mMeshTexturesDirty;

	//--------------------------------------------------------------------
	// Destroy invisible mesh
	//--------------------------------------------------------------------
protected:
	BOOL			mMeshValid;
	LLFrameTimer	mMeshInvisibleTime;

/**                    Meshes
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    APPEARANCE
 **/

   LLPointer<LLAppearanceMessageContents> 	mLastProcessedAppearance;

public:
	void 			parseAppearanceMessage(LLMessageSystem* mesgsys, LLAppearanceMessageContents& msg);
	void 			processAvatarAppearance(LLMessageSystem* mesgsys);
    void            applyParsedAppearanceMessage(LLAppearanceMessageContents& contents, bool slam_params);
	void 			hideSkirt();
	void			startAppearanceAnimation();
	
	//--------------------------------------------------------------------
	// Appearance morphing
	//--------------------------------------------------------------------
public:
	BOOL			getIsAppearanceAnimating() const { return mAppearanceAnimating; }

	// True if we are computing our appearance via local compositing
	// instead of baked textures, as for example during wearable
	// editing or when waiting for a subsequent server rebake.
	/*virtual*/ BOOL	isUsingLocalAppearance() const { return mUseLocalAppearance; }

	// True if this avatar should fetch its baked textures via the new
	// appearance mechanism.
	BOOL				isUsingServerBakes() const;
	void 				setIsUsingServerBakes(BOOL newval);


	// True if we are currently in appearance editing mode. Often but
	// not always the same as isUsingLocalAppearance().
	/*virtual*/ BOOL	isEditingAppearance() const { return mIsEditingAppearance; }

	// FIXME review isUsingLocalAppearance uses, some should be isEditing instead.

private:
	BOOL			mAppearanceAnimating;
	LLFrameTimer	mAppearanceMorphTimer;
	F32				mLastAppearanceBlendTime;
	BOOL			mIsEditingAppearance; // flag for if we're actively in appearance editing mode
	BOOL			mUseLocalAppearance; // flag for if we're using a local composite
	BOOL			mUseServerBakes; // flag for if baked textures should be fetched from baking service (false if they're temporary uploads)

	//--------------------------------------------------------------------
	// Visibility
	//--------------------------------------------------------------------
public:
	BOOL			isVisible() const;
	virtual bool    shouldRenderRigged() const;
	void			setVisibilityRank(U32 rank);
	U32				getVisibilityRank()  const { return mVisibilityRank; } // unused
	static S32 		sNumVisibleAvatars; // Number of instances of this class
/**                    Appearance
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    WEARABLES
 **/

	//--------------------------------------------------------------------
	// Attachments
	//--------------------------------------------------------------------
public:
	void 				clampAttachmentPositions();
	virtual const LLViewerJointAttachment* attachObject(LLViewerObject *viewer_object);
	virtual BOOL 		detachObject(LLViewerObject *viewer_object);
	static bool		    getRiggedMeshID( LLViewerObject* pVO, LLUUID& mesh_id );
	void				cleanupAttachedMesh( LLViewerObject* pVO );
	static LLVOAvatar*  findAvatarFromAttachment(LLViewerObject* obj);
	/*virtual*/ BOOL	isWearingWearableType(LLWearableType::EType type ) const;
	LLViewerObject *	findAttachmentByID( const LLUUID & target_id ) const;
	LLViewerJointAttachment* getTargetAttachmentPoint(LLViewerObject* viewer_object);

protected:
	void 				lazyAttach();
	void				rebuildRiggedAttachments( void );

	//--------------------------------------------------------------------
	// Map of attachment points, by ID
	//--------------------------------------------------------------------
public:
	S32 				getAttachmentCount(); // Warning: order(N) not order(1) // currently used only by -self
	//typedef std::map<S32, LLViewerJointAttachment*> attachment_map_t;
	typedef LLSortedVector<S32, LLViewerJointAttachment*> attachment_map_t;
	attachment_map_t 								mAttachmentPoints;
	std::vector<LLPointer<LLViewerObject> > 		mPendingAttachment;
	std::vector<std::pair<LLViewerObject*,LLViewerJointAttachment*> >	mAttachedObjectsVector;	//A vector of all current attachments for fast iteration.

	//--------------------------------------------------------------------
	// HUD functions
	//--------------------------------------------------------------------
public:
	BOOL 				hasHUDAttachment() const;
	LLBBox 				getHUDBBox() const;
	void 				resetHUDAttachments();
	BOOL				canAttachMoreObjects(U32 n=1) const;
    U32					getMaxAnimatedObjectAttachments() const;
    BOOL				canAttachMoreAnimatedObjects(U32 n=1) const;
protected:
	U32					getNumAttachments() const; // O(N), not O(1)
	U32					getNumAnimatedObjectAttachments() const; // O(N), not O(1)
	//--------------------------------------------------------------------
	// Old/nonstandard/Agent-only functions
	//--------------------------------------------------------------------
public:
	BOOL 			isWearingAttachment( const LLUUID& inv_item_id );
	LLViewerObject* getWornAttachment( const LLUUID& inv_item_id );


/**                    Wearables
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    ACTIONS
 **/

	//--------------------------------------------------------------------
	// Animations
	//--------------------------------------------------------------------
public:
	BOOL 			isAnyAnimationSignaled(const LLUUID *anim_array, const S32 num_anims) const;
	void 			processAnimationStateChanges();
protected:
	BOOL 			processSingleAnimationStateChange(const LLUUID &anim_id, BOOL start);
	void 			resetAnimations();
private:
	LLTimer			mAnimTimer;
	F32				mTimeLast;	

	//--------------------------------------------------------------------
	// Animation state data
	//--------------------------------------------------------------------
public:
	typedef std::map<LLUUID, S32>::iterator AnimIterator;
	std::map<LLUUID, S32> 					mSignaledAnimations; // requested state of Animation name/value
	std::map<LLUUID, S32> 					mPlayingAnimations; // current state of Animation name/value

	typedef std::multimap<LLUUID, LLUUID> 	AnimationSourceMap;
	typedef AnimationSourceMap::iterator 	AnimSourceIterator;
	AnimationSourceMap 						mAnimationSources; // object ids that triggered anim ids

	//--------------------------------------------------------------------
	// Chat
	//--------------------------------------------------------------------
public:
	void			addChat(const LLChat& chat);
	void	   		clearChat();
	void			startTyping() { mTyping = TRUE; mTypingTimer.reset(); mIdleTimer.reset();}
	void			stopTyping() { mTyping = FALSE; mIdleTimer.reset();}
private:
	BOOL			mVisibleChat;
	bool			mVisibleTyping;

	//--------------------------------------------------------------------
	// Lip synch morphs
	//--------------------------------------------------------------------
private:
	bool 		   	mLipSyncActive; // we're morphing for lip sync
	LLVisualParam* 	mOohMorph; // cached pointers morphs for lip sync
	LLVisualParam* 	mAahMorph; // cached pointers morphs for lip sync

	//--------------------------------------------------------------------
	// Flight
	//--------------------------------------------------------------------
public:
	BOOL			mInAir;
	LLFrameTimer	mTimeInAir;

/**                    Actions
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    PHYSICS
 **/

private:
	F32 		mSpeedAccum; // measures speed (for diagnostics mostly).
	BOOL 		mTurning; // controls hysteresis on avatar rotation
	F32			mSpeed; // misc. animation repeated state

	//--------------------------------------------------------------------
	// Dimensions
	//--------------------------------------------------------------------
public:
	void 		resolveHeightGlobal(const LLVector3d &inPos, LLVector3d &outPos, LLVector3 &outNorm);
	void 		resolveHeightAgent(const LLVector3 &inPos, LLVector3 &outPos, LLVector3 &outNorm);
	void 		resolveRayCollisionAgent(const LLVector3d start_pt, const LLVector3d end_pt, LLVector3d &out_pos, LLVector3 &out_norm);
	void 		slamPosition(); // Slam position to transmitted position (for teleport);
protected:

	//--------------------------------------------------------------------
	// Material being stepped on
	//--------------------------------------------------------------------
private:
	BOOL		mStepOnLand;
	U8			mStepMaterial;
	LLVector3	mStepObjectVelocity;

public:
	bool		mHasPhysicsParameters; //If we receive no physics params from this av then reset and skip llphysicsmotion calculations.
/**                    Physics
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    HIERARCHY
 **/

public:
	/*virtual*/ BOOL 	setParent(LLViewerObject* parent);
	/*virtual*/ void 	addChild(LLViewerObject *childp);
	/*virtual*/ void 	removeChild(LLViewerObject *childp);

	//--------------------------------------------------------------------
	// Sitting
	//--------------------------------------------------------------------
public:
	void			sitDown(BOOL bSitting);
	BOOL			isSitting() const {return mIsSitting;}
	void 			sitOnObject(LLViewerObject *sit_object);
	void 			getOffObject();
private:
	// set this property only with LLVOAvatar::sitDown method
	BOOL 			mIsSitting;
	// position backup in case of missing data
	LLVector3		mLastRootPos;

/**                    Hierarchy
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    NAME
 **/

public:
	std::string		getFullname() const; // Returns "FirstName LastName"
	std::string		avString() const; // Frequently used string in log messages "Avatar '<full name'"
protected:
	static void		getAnimLabels(std::vector<std::string>* labels);
	static void		getAnimNames(std::vector<std::string>* names);
private:
	std::string		mNameString;		// UTF-8 title + name + status
	std::string  	mTitle;
	bool	  		mNameAway;
	bool	  		mNameBusy;
	bool	  		mNameMute;
	bool      		mNameAppearance;
	bool			mNameFriend;
	bool			mNameCloud;
	bool			mNameLangolier;
	F32				mNameAlpha;
	BOOL      		mRenderGroupTitles;

	//--------------------------------------------------------------------
	// Display the name (then optionally fade it out)
	//--------------------------------------------------------------------
public:
	LLFrameTimer	mChatTimer;
	LLPointer<LLHUDNameTag> mNameText;
private:
	LLFrameTimer	mTimeVisible;
	std::deque<LLChat> mChats;
	BOOL			mTyping;
public:
	BOOL isTyping(){ return mTyping; }
	LLFrameTimer	mTypingTimer;

/**                    Name
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SOUNDS
 **/

	//--------------------------------------------------------------------
	// Voice visualizer
	//--------------------------------------------------------------------
public:
	// Responsible for detecting the user's voice signal (and when the
	// user speaks, it puts a voice symbol over the avatar's head) and gesticulations
	LLPointer<LLVoiceVisualizer>  mVoiceVisualizer;
	int					mCurrentGesticulationLevel;

	//--------------------------------------------------------------------
	// Step sound
	//--------------------------------------------------------------------
protected:
	const LLUUID& 		getStepSound() const;
private:
	// Global table of sound ids per material, and the ground
	const static LLUUID	sStepSounds[LL_MCODE_END];
	const static LLUUID	sStepSoundOnLand;

	//--------------------------------------------------------------------
	// Foot step state (for generating sounds)
	//--------------------------------------------------------------------
public:
	void 				setFootPlane(const LLVector4 &plane) { mFootPlane = plane; }
	LLVector4			mFootPlane;
private:
	BOOL				mWasOnGroundLeft;
	BOOL				mWasOnGroundRight;

/**                    Sounds
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    DIAGNOSTICS
 **/
	
	//--------------------------------------------------------------------
	// General
	//--------------------------------------------------------------------
public:
	void				getSortedJointNames(S32 joint_type, std::vector<std::string>& result) const;
	static void			dumpArchetypeXML_header(LLAPRFile& file, std::string const& archetype_name = "???");
	static void			dumpArchetypeXML_footer(LLAPRFile& file);
	void				dumpArchetypeXML(const std::string& prefix, bool group_by_wearables = false);
	void				dumpArchetypeXML_cont(std::string const& fullpath, bool group_by_wearables);
	void 				dumpAppearanceMsgParams( const std::string& dump_prefix,
												 const LLAppearanceMessageContents& contents);
	static void			dumpBakedStatus();
	const std::string 	getBakedStatusForPrintout() const;
	void				dumpAvatarTEs(const std::string& context) const;

	static F32 			sUnbakedTime; // Total seconds with >=1 unbaked avatars
	static F32 			sUnbakedUpdateTime; // Last time stats were updated (to prevent multiple updates per frame) 
	static F32 			sGreyTime; // Total seconds with >=1 grey avatars	
	static F32 			sGreyUpdateTime; // Last time stats were updated (to prevent multiple updates per frame) 
protected:
	S32					getUnbakedPixelAreaRank();
	BOOL				mHasGrey;
private:
	F32					mMinPixelArea;
	F32					mMaxPixelArea;
	F32					mAdjustedPixelArea;
	std::string  		mDebugText;
	std::string			mBakedTextureDebugText;


	//--------------------------------------------------------------------
	// Avatar Rez Metrics
	//--------------------------------------------------------------------
public:
	void 			debugAvatarRezTime(std::string notification_name, std::string comment = "");
	F32				debugGetExistenceTimeElapsedF32() const { return mDebugExistenceTimer.getElapsedTimeF32(); }

protected:
	LLFrameTimer	mRuthDebugTimer; // For tracking how long it takes for av to rez
	LLFrameTimer	mDebugExistenceTimer; // Debugging for how long the avatar has been in memory.
	LLFrameTimer	mLastAppearanceMessageTimer; // Time since last appearance message received.

	//--------------------------------------------------------------------
	// COF monitoring
	//--------------------------------------------------------------------

public:
	// COF version of last viewer-initiated appearance update request. For non-self avs, this will remain at default.
	S32 mLastUpdateRequestCOFVersion;

	// COF version of last appearance message received for this av.
	S32 mLastUpdateReceivedCOFVersion;

/**                    Diagnostics
 **                                                                            **
 *******************************************************************************/

/********************************************************************************
 **                                                                            **
 **                    SUPPORT CLASSES
 **/

protected: // Shared with LLVOAvatarSelf


/**                    Support classes
 **                                                                            **
 *******************************************************************************/

public:
	typedef std::array<F32, LL_MAX_JOINTS_PER_MESH_OBJECT * 12> rigged_matrix_array_t;
	typedef std::vector<std::pair<LLUUID, std::pair<U32, rigged_matrix_array_t> > > rigged_transformation_cache_t;
	rigged_transformation_cache_t& getRiggedMatrixCache()
	{
		return mRiggedMatrixDataCache;
	}
	void clearRiggedMatrixCache()
	{
		mRiggedMatrixDataCache.clear();
	}
private:
	rigged_transformation_cache_t mRiggedMatrixDataCache;

// <edit>

//Avatar idle timer
private:
	std::string		getIdleTime(bool is_away, bool is_busy, bool is_appearance);
public:
	LLFrameTimer 	mIdleTimer;
private:
	S32				mIdleMinute;

	LLTimer			mComplexityTimer;

//CCS Nametag
public:
	void setNameFromChat(const std::string &text);
	void clearNameFromChat();
private:
	void			idleCCSUpdateAttachmentText(bool render_name);
	LLFrameTimer	mCCSUpdateAttachmentTimer;
	std::string		mCCSAttachmentText;
	bool			mCCSChatTextOverride;
	std::string		mCCSChatText;

// </edit>
}; // LLVOAvatar
extern const F32 SELF_ADDITIONAL_PRI;
extern const S32 MAX_TEXTURE_VIRTUAL_SIZE_RESET_INTERVAL;

extern const F32 MAX_HOVER_Z;
extern const F32 MIN_HOVER_Z;

constexpr U32 NUM_ATTACHMENT_GROUPS = 24;

void dump_sequential_xml(const std::string outprefix, const LLSD& content);

#endif // LL_VOAVATAR_H

