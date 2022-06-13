/** 
 * @file pipeline.cpp
 * @brief Rendering pipeline.
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

#include "llviewerprecompiledheaders.h"

#include "pipeline.h"

// library includes
#include "llaudioengine.h" // For MAX_BUFFERS for debugging.
#include "imageids.h"
#include "llerror.h"
#include "llviewercontrol.h"
#include "llfasttimer.h"
#include "llfontgl.h"
#include "llmemory.h"
#include "llnamevalue.h"
#include "llpointer.h"
#include "llprimitive.h"
#include "llvolume.h"
#include "material_codes.h"
#include "timing.h"
#include "v3color.h"
#include "llui.h" 
#include "llglheaders.h"
#include "llrender.h"
#include "llwindow.h"
#include "llpostprocess.h"

// newview includes
#include "llagent.h"
#include "llagentcamera.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolground.h"
#include "lldrawpoolbump.h"
#include "lldrawpooltree.h"
#include "lldrawpoolwater.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "llfloatertelehub.h"
#include "llframestats.h"
#include "llgldbg.h"
#include "llhudmanager.h"
#include "llhudnametag.h"
#include "llhudtext.h"
#include "lllightconstants.h"
#include "llmeshrepository.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "lltracker.h"
#include "lltool.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewermediafocus.h"
#include "llviewertexturelist.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h" // for audio debugging.
#include "llviewerstats.h"
#include "llviewerwindow.h" // For getSpinAxis
#include "llvoavatar.h"
#include "llvoground.h"
#include "llvosky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llvosurfacepatch.h"
#include "llvowater.h"
#include "llvotree.h"
#include "llvopartgroup.h"
#include "llworld.h"
#include "llcubemap.h"
#include "lldebugmessagebox.h"
#include "llviewershadermgr.h"
#include "llviewerjoystick.h"
#include "llviewerdisplay.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llspatialpartition.h"
#include "llmutelist.h"
#include "llfloatertools.h"
#include "llpanelface.h"

// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

void check_stack_depth(S32 stack_depth)
{
	if (gDebugGL || gDebugSession)
	{
		GLint depth;
		glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &depth);
		if (depth != stack_depth)
		{
			if (gDebugSession)
			{
				ll_fail("GL matrix stack corrupted.");
			}
			else
			{
				LL_ERRS() << "GL matrix stack corrupted!" << LL_ENDL;
			}
		}
	}
}

#ifdef _DEBUG
// Debug indices is disabled for now for debug performance - djs 4/24/02
//#define DEBUG_INDICES
#else
//#define DEBUG_INDICES
#endif

bool gShiftFrame = false;

const F32 BACKLIGHT_DAY_MAGNITUDE_AVATAR = 0.2f;
const F32 BACKLIGHT_NIGHT_MAGNITUDE_AVATAR = 0.1f;
const F32 BACKLIGHT_DAY_MAGNITUDE_OBJECT = 0.1f;
const F32 BACKLIGHT_NIGHT_MAGNITUDE_OBJECT = 0.08f;
const S32 MAX_ACTIVE_OBJECT_QUIET_FRAMES = 40;
const S32 MAX_OFFSCREEN_GEOMETRY_CHANGES_PER_FRAME = 10;
const U32 NOISE_MAP_RES = 256;
const U32 AUX_VB_MASK = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1;
// Max number of occluders to search for. JC
const S32 MAX_OCCLUDER_COUNT = 2;

enum {
	LIGHT_MODE_NORMAL,
	LIGHT_MODE_EDIT,
	LIGHT_MODE_BACKLIGHT,
	LIGHT_MODE_PREVIEW
};


extern S32 gBoxFrame;
//extern BOOL gHideSelectedObjects;
extern BOOL gDisplaySwapBuffers;
extern BOOL gDebugGL;

// hack counter for rendering a fixed number of frames after toggling
// fullscreen to work around DEV-5361
static S32 sDelayedVBOEnable = 0;

BOOL	gAvatarBacklight = FALSE;

BOOL	gDebugPipeline = FALSE;
LLPipeline gPipeline;
const LLMatrix4a* gGLLastMatrix = NULL;

LLTrace::BlockTimerStatHandle FTM_RENDER_GEOMETRY("Geometry");
LLTrace::BlockTimerStatHandle FTM_RENDER_GRASS("Grass");
LLTrace::BlockTimerStatHandle FTM_RENDER_OCCLUSION("Occlusion");
LLTrace::BlockTimerStatHandle FTM_RENDER_SHINY("Shiny");
LLTrace::BlockTimerStatHandle FTM_RENDER_SIMPLE("Simple");
LLTrace::BlockTimerStatHandle FTM_RENDER_TERRAIN("Terrain");
LLTrace::BlockTimerStatHandle FTM_RENDER_TREES("Trees");
LLTrace::BlockTimerStatHandle FTM_RENDER_UI("UI");
LLTrace::BlockTimerStatHandle FTM_RENDER_WATER("Water");
LLTrace::BlockTimerStatHandle FTM_RENDER_WL_SKY("Windlight Sky");
LLTrace::BlockTimerStatHandle FTM_RENDER_ALPHA("Alpha Objects");
LLTrace::BlockTimerStatHandle FTM_RENDER_CHARACTERS("Avatars");
LLTrace::BlockTimerStatHandle FTM_RENDER_BUMP("Bump");
LLTrace::BlockTimerStatHandle FTM_RENDER_MATERIALS("Materials");
LLTrace::BlockTimerStatHandle FTM_RENDER_FULLBRIGHT("Fullbright");
LLTrace::BlockTimerStatHandle FTM_RENDER_GLOW("Glow");
LLTrace::BlockTimerStatHandle FTM_GEO_UPDATE("Geo Update");
LLTrace::BlockTimerStatHandle FTM_PIPELINE_CREATE("Pipeline Create");
LLTrace::BlockTimerStatHandle FTM_POOLRENDER("RenderPool");
LLTrace::BlockTimerStatHandle FTM_POOLS("Pools");
LLTrace::BlockTimerStatHandle FTM_DEFERRED_POOLRENDER("RenderPool (Deferred)");
LLTrace::BlockTimerStatHandle FTM_DEFERRED_POOLS("Pools (Deferred)");
LLTrace::BlockTimerStatHandle FTM_POST_DEFERRED_POOLRENDER("RenderPool (Post)");
LLTrace::BlockTimerStatHandle FTM_POST_DEFERRED_POOLS("Pools (Post)");
LLTrace::BlockTimerStatHandle FTM_RENDER_BLOOM_FBO("First FBO");
LLTrace::BlockTimerStatHandle FTM_STATESORT("Sort Draw State");
LLTrace::BlockTimerStatHandle FTM_PIPELINE("Pipeline");
LLTrace::BlockTimerStatHandle FTM_CLIENT_COPY("Client Copy");
LLTrace::BlockTimerStatHandle FTM_RENDER_DEFERRED("Deferred Shading");


static LLTrace::BlockTimerStatHandle FTM_STATESORT_DRAWABLE("Sort Drawables");
static LLTrace::BlockTimerStatHandle FTM_STATESORT_POSTSORT("Post Sort");

static LLStaticHashedString sDelta("delta");
static LLStaticHashedString sDistFactor("dist_factor");

//----------------------------------------
std::string gPoolNames[] = 
{
	// Correspond to LLDrawpool enum render type
	"NONE",
	"POOL_GROUND",
	"POOL_TERRAIN",
	"POOL_SIMPLE",
	"POOL_FULLBRIGHT",
	"POOL_BUMP",
	"POOL_MATERIALS",
	"POOL_TREE", // Singu Note: Before sky for zcull.
	"POOL_ALPHA_MASK",
	"POOL_FULLBRIGHT_ALPHA_MASK",
	"POOL_SKY",
	"POOL_WL_SKY",
	"POOL_GRASS",
	"POOL_AVATAR",
	"POOL_VOIDWATER",
	"POOL_WATER",
	"POOL_GLOW",
	"POOL_ALPHA",
	"POOL_INVALID_OUCH_CATASTROPHE_ERROR"
};

void drawBox(const LLVector3& c, const LLVector3& r);
void drawBoxOutline(const LLVector3& pos, const LLVector3& size);
U32 nhpo2(U32 v);
LLVertexBuffer* ll_create_cube_vb(U32 type_mask, U32 usage);

const LLMatrix4a& glh_get_current_modelview()
{
	return gGLModelView;
}

const LLMatrix4a& glh_get_current_projection()
{
	return gGLProjection;
}

inline const LLMatrix4a& glh_get_last_modelview()
{
	return gGLLastModelView;
}

inline const LLMatrix4a& glh_get_last_projection()
{
	return gGLLastProjection;
}

void glh_set_current_modelview(const LLMatrix4a& mat)
{
	gGLModelView = mat;
}

void glh_set_current_projection(const LLMatrix4a& mat)
{
	gGLProjection = mat;
}

inline void glh_set_last_modelview(const LLMatrix4a& mat)
{
	gGLLastModelView = mat;
}

void glh_set_last_projection(const LLMatrix4a& mat)
{
	gGLLastProjection = mat;
}

void display_update_camera(bool tiling=false);
//----------------------------------------

S32		LLPipeline::sCompiles = 0;

BOOL	LLPipeline::sPickAvatar = TRUE;
BOOL	LLPipeline::sDynamicLOD = TRUE;
BOOL	LLPipeline::sShowHUDAttachments = TRUE;
BOOL	LLPipeline::sRenderMOAPBeacons = FALSE;
BOOL	LLPipeline::sRenderPhysicalBeacons = TRUE;
BOOL	LLPipeline::sRenderScriptedBeacons = FALSE;
BOOL	LLPipeline::sRenderScriptedTouchBeacons = TRUE;
BOOL	LLPipeline::sRenderParticleBeacons = FALSE;
BOOL	LLPipeline::sRenderSoundBeacons = FALSE;
BOOL	LLPipeline::sRenderBeacons = FALSE;
BOOL	LLPipeline::sRenderHighlight = TRUE;
LLRender::eTexIndex LLPipeline::sRenderHighlightTextureChannel = LLRender::DIFFUSE_MAP;
BOOL	LLPipeline::sForceOldBakedUpload = FALSE;
S32		LLPipeline::sUseOcclusion = 0;
BOOL	LLPipeline::sDelayVBUpdate = FALSE;
BOOL	LLPipeline::sAutoMaskAlphaDeferred = TRUE;
BOOL	LLPipeline::sAutoMaskAlphaNonDeferred = FALSE;
BOOL	LLPipeline::sRenderBump = TRUE;
BOOL	LLPipeline::sNoAlpha = FALSE;
BOOL	LLPipeline::sUseFarClip = TRUE;
BOOL	LLPipeline::sShadowRender = FALSE;
BOOL	LLPipeline::sSkipUpdate = FALSE;
BOOL	LLPipeline::sWaterReflections = FALSE;
BOOL	LLPipeline::sRenderGlow = FALSE;
BOOL	LLPipeline::sReflectionRender = FALSE;
BOOL	LLPipeline::sImpostorRender = FALSE;
BOOL	LLPipeline::sImpostorRenderAlphaDepthPass = FALSE;
BOOL	LLPipeline::sUnderWaterRender = FALSE;
BOOL	LLPipeline::sTextureBindTest = FALSE;
BOOL	LLPipeline::sRenderFrameTest = FALSE;
BOOL	LLPipeline::sRenderAttachedLights = TRUE;
BOOL	LLPipeline::sRenderAttachedParticles = TRUE;
BOOL	LLPipeline::sRenderDeferred = FALSE;
BOOL    LLPipeline::sMemAllocationThrottled = FALSE;
S32		LLPipeline::sVisibleLightCount = 0;
F32		LLPipeline::sMinRenderSize = 0.f;
BOOL	LLPipeline::sRenderingHUDs = FALSE;


static LLCullResult* sCull = NULL;

static const U32 gl_cube_face[] = 
{
	GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB,
};

void validate_framebuffer_object();


bool addDeferredAttachments(LLRenderTarget& target)
{
	//static const LLCachedControl<bool> SHPrecisionDeferredNormals("SHPrecisionDeferredNormals",false); //DEAD
	return target.addColorAttachment(GL_SRGB8_ALPHA8) && //specular
			target.addColorAttachment(GL_RGB10_A2); //normal+z
}

LLPipeline::LLPipeline() :
	mBackfaceCull(FALSE),
	mBatchCount(0),
	mMatrixOpCount(0),
	mTextureMatrixOps(0),
	mMaxBatchSize(0),
	mMinBatchSize(0),
	mMeanBatchSize(0),
	mTrianglesDrawn(0),
	mNumVisibleNodes(0),
	mInitialized(FALSE),
	mTransformFeedbackPrimitives(0),
	mRenderDebugFeatureMask(0),
	mRenderDebugMask(0),
	mOldRenderDebugMask(0),
	mMeshDirtyQueryObject(0),
	mGroupQ1Locked(false),
	mGroupQ2Locked(false),
	mResetVertexBuffers(false),
	mInRenderPass(false),
	mLastRebuildPool(NULL),
	mAlphaPool(NULL),
	mSkyPool(NULL),
	mTerrainPool(NULL),
	mWaterPool(NULL),
	mGroundPool(NULL),
	mSimplePool(NULL),
	mGrassPool(NULL),
	mAlphaMaskPool(NULL),
	mFullbrightAlphaMaskPool(NULL),
	mFullbrightPool(NULL),
	mGlowPool(NULL),
	mBumpPool(NULL),
	mMaterialsPool(NULL),
	mWLSkyPool(NULL),
	mLightMask(0),
	mLightMode(LIGHT_MODE_NORMAL)
{}

void LLPipeline::init()
{
	refreshCachedSettings();

	mInitialized = true;
	
	stop_glerror();

	//create render pass pools
	getPool(LLDrawPool::POOL_ALPHA);
	getPool(LLDrawPool::POOL_SIMPLE);
	getPool(LLDrawPool::POOL_ALPHA_MASK);
	getPool(LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK);
	getPool(LLDrawPool::POOL_GRASS);
	getPool(LLDrawPool::POOL_FULLBRIGHT);
	getPool(LLDrawPool::POOL_BUMP);
	getPool(LLDrawPool::POOL_MATERIALS);
	getPool(LLDrawPool::POOL_GLOW);

	LLViewerStats::getInstance()->mTrianglesDrawnStat.reset();
	resetFrameStats();

	if (gSavedSettings.getBOOL("DisableAllRenderFeatures"))
	{
		clearAllRenderDebugFeatures();
	}
	else
	{
		setAllRenderDebugFeatures(); // By default, all debugging features on
	}
	clearAllRenderDebugDisplays(); // All debug displays off

	if (gSavedSettings.getBOOL("DisableAllRenderTypes"))
	{
		clearAllRenderTypes();
	}
	else
	{
		setAllRenderTypes(); // By default, all rendering types start enabled
		// Don't turn on ground when this is set
		// Mac Books with intel 950s need this
		if(!gSavedSettings.getBOOL("RenderGround"))
		{
			toggleRenderType(RENDER_TYPE_GROUND);
		}
	}

	// make sure RenderPerformanceTest persists (hackity hack hack)
	// disables non-object rendering (UI, sky, water, etc)
	if (gSavedSettings.getBOOL("RenderPerformanceTest"))
	{
		gSavedSettings.setBOOL("RenderPerformanceTest", FALSE);
		gSavedSettings.setBOOL("RenderPerformanceTest", TRUE);
	}

	mOldRenderDebugMask = mRenderDebugMask;

	mBackfaceCull = TRUE;

	stop_glerror();

	// Enable features

	LLViewerShaderMgr::instance()->setShaders();

	stop_glerror();


	for (U32 i = 0; i < 2; ++i)
	{
		mSpotLightFade[i] = 1.f;
	}

	updateLocalLightingEnabled();
	gSavedSettings.getControl("RenderAutoMaskAlphaDeferred")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	gSavedSettings.getControl("RenderAutoMaskAlphaNonDeferred")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	gSavedSettings.getControl("RenderUseFarClip")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	gSavedSettings.getControl("RenderAvatarMaxVisible")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	//gSavedSettings.getControl("RenderDelayVBUpdate")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	gSavedSettings.getControl("UseOcclusion")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	//gSavedSettings.getControl("VertexShaderEnable")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));	//Already registered to handleSetShaderChanged
	//gSavedSettings.getControl("RenderDeferred")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));	//Already registered to handleSetShaderChanged
	gSavedSettings.getControl("RenderFSAASamples")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));
	//gSavedSettings.getControl("RenderAvatarVP")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings));	//Already registered to handleSetShaderChanged
	//gSavedSettings.getControl("WindLightUseAtmosShaders")->getCommitSignal()->connect(boost::bind(&LLPipeline::refreshCachedSettings)); //Already registered to handleSetShaderChanged

	gGL.init();
}

LLPipeline::~LLPipeline()
{

}

void LLPipeline::cleanup()
{
	assertInitialized();

	mGroupQ1.clear() ;
	mGroupQ2.clear() ;

	for(pool_set_t::iterator iter = mPools.begin();
		iter != mPools.end(); )
	{
		pool_set_t::iterator curiter = iter++;
		LLDrawPool* poolp = *curiter;
		if (poolp->isFacePool())
		{
			LLFacePool* face_pool = (LLFacePool*) poolp;
			if (face_pool->mReferences.empty())
			{
				mPools.erase(curiter);
				removeFromQuickLookup( poolp );
				delete poolp;
			}
		}
		else
		{
			mPools.erase(curiter);
			removeFromQuickLookup( poolp );
			delete poolp;
		}
	}
	
	if (!mTerrainPools.empty())
	{
		LL_WARNS() << "Terrain Pools not cleaned up" << LL_ENDL;
	}
	if (!mTreePools.empty())
	{
		LL_WARNS() << "Tree Pools not cleaned up" << LL_ENDL;
	}
		
	delete mAlphaPool;
	mAlphaPool = NULL;
	delete mSkyPool;
	mSkyPool = NULL;
	delete mTerrainPool;
	mTerrainPool = NULL;
	delete mWaterPool;
	mWaterPool = NULL;
	delete mGroundPool;
	mGroundPool = NULL;
	delete mSimplePool;
	mSimplePool = NULL;
	delete mFullbrightPool;
	mFullbrightPool = NULL;
	delete mGlowPool;
	mGlowPool = NULL;
	delete mBumpPool;
	mBumpPool = NULL;
	// don't delete wl sky pool it was handled above in the for loop
	//delete mWLSkyPool;
	mWLSkyPool = NULL;

	releaseGLBuffers();

	mFaceSelectImagep = NULL;

	mMovedBridge.clear();

	mInitialized = FALSE;

	mAuxScreenRectVB = NULL;

	mCubeVB = NULL;
}

//============================================================================

void LLPipeline::destroyGL() 
{
	stop_glerror();
	unloadShaders();
	mHighlightFaces.clear();
	
	resetDrawOrders();

	releaseVertexBuffers();

	releaseGLBuffers();

	if (LLVertexBuffer::sEnableVBOs)
	{
		// render 30 frames after switching to work around DEV-5361
		if(!LLRenderTarget::sUseFBO)
		{
			sDelayedVBOEnable = 30;
			LLVertexBuffer::sEnableVBOs = FALSE;
		}

	}
	if (mMeshDirtyQueryObject)
	{
		glDeleteQueriesARB(1, &mMeshDirtyQueryObject);
		mMeshDirtyQueryObject = 0;
	}
}

static LLTrace::BlockTimerStatHandle FTM_RESIZE_SCREEN_TEXTURE("Resize Screen Texture");

//static
void LLPipeline::throttleNewMemoryAllocation(BOOL disable)
{
	if(sMemAllocationThrottled != disable)
	{
		sMemAllocationThrottled = disable ;

		if(sMemAllocationThrottled)
		{
			//send out notification
			LLNotification::Params params("LowMemory");
			LLNotifications::instance().add(params);

			//release some memory.
		}
	}
}

void LLPipeline::resizeScreenTexture()
{
	LL_RECORD_BLOCK_TIME(FTM_RESIZE_SCREEN_TEXTURE);
	if (LLGLSLShader::sNoFixedFunction && assertInitialized())
	{
		GLuint resX = gViewerWindow->getWorldViewWidthRaw();
		GLuint resY = gViewerWindow->getWorldViewHeightRaw();
	
// [RLVa:KB] - Checked: 2014-02-23 (RLVa-1.4.10)
		U32 resMod = gSavedSettings.getU32("RenderResolutionDivisor"), resAdjustedX = resX, resAdjustedY = resY;
		if ( (resMod > 1) && (resMod < resX) && (resMod < resY) )
		{
			resAdjustedX /= resMod;
			resAdjustedY /= resMod;
		}

		if ( (resAdjustedX != mScreen.getWidth()) || (resAdjustedY != mScreen.getHeight()) )
// [/RLVa:KB]
//		if ((resX != mScreen.getWidth()) || (resY != mScreen.getHeight()))
		{
			releaseScreenBuffers();
		if (!allocateScreenBuffer(resX,resY))
			{
#if PROBABLE_FALSE_DISABLES_OF_ALM_HERE
				//FAILSAFE: screen buffer allocation failed, disable deferred rendering if it's enabled
			//NOTE: if the session closes successfully after this call, deferred rendering will be 
			// disabled on future sessions
			if (LLPipeline::sRenderDeferred)
			{
				gSavedSettings.setBOOL("RenderDeferred", FALSE);
				LLPipeline::refreshCachedSettings();

				}
#endif
			}
		}
	}
}

void LLPipeline::allocatePhysicsBuffer()
{
	GLuint resX = gViewerWindow->getWorldViewWidthRaw();
	GLuint resY = gViewerWindow->getWorldViewHeightRaw();

	if (mPhysicsDisplay.getWidth() != resX || mPhysicsDisplay.getHeight() != resY)
	{
		mPhysicsDisplay.allocate(resX, resY, GL_RGBA, TRUE, FALSE, LLTexUnit::TT_TEXTURE, FALSE);
	}
}

bool LLPipeline::allocateScreenBuffer(U32 resX, U32 resY)
{
	refreshCachedSettings();
	
	bool save_settings = sRenderDeferred;
	if (save_settings)
	{
		// Set this flag in case we crash while resizing window or allocating space for deferred rendering targets
		gSavedSettings.setBOOL("RenderInitError", TRUE);
		gSavedSettings.saveToFile( gSavedSettings.getString("ClientSettingsFile"), TRUE );
	}

	eFBOStatus ret = doAllocateScreenBuffer(resX, resY);

	if (save_settings)
	{
		// don't disable shaders on next session
		gSavedSettings.setBOOL("RenderInitError", FALSE);
		gSavedSettings.saveToFile( gSavedSettings.getString("ClientSettingsFile"), TRUE );
	}
	
	if (ret == FBO_FAILURE)
	{ //FAILSAFE: screen buffer allocation failed, disable deferred rendering if it's enabled
		//NOTE: if the session closes successfully after this call, deferred rendering will be 
		// disabled on future sessions
		if (LLPipeline::sRenderDeferred)
		{
			gSavedSettings.setBOOL("RenderDeferred", FALSE);
			LLPipeline::refreshCachedSettings();
		}
	}

	return ret == FBO_SUCCESS_FULLRES;
}


LLPipeline::eFBOStatus LLPipeline::doAllocateScreenBuffer(U32 resX, U32 resY)
{
	refreshCachedSettings();
	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	U32 samples = RenderFSAASamples.get() - RenderFSAASamples.get() % 2;	//Must be multipe of 2.

	//try to allocate screen buffers at requested resolution and samples
	// - on failure, shrink number of samples and try again
	// - if not multisampled, shrink resolution and try again (favor X resolution over Y)
	// Make sure to call "releaseScreenBuffers" after each failure to cleanup the partially loaded state
	eFBOStatus ret = FBO_SUCCESS_FULLRES;
	if (!allocateScreenBuffer(resX, resY, samples))
	{
		//failed to allocate at requested specification, return false
		ret = FBO_FAILURE;

		releaseScreenBuffers();
		//reduce number of samples 
		while (samples > 0)
		{
			samples /= 2;
			if (allocateScreenBuffer(resX, resY, samples))
			{ //success
				return FBO_SUCCESS_LOWRES;
			}
			releaseScreenBuffers();
		}

		samples = 0;

		//reduce resolution
		while (resY > 0 && resX > 0)
		{
			resY /= 2;
			if (allocateScreenBuffer(resX, resY, samples))
			{
				return FBO_SUCCESS_LOWRES;
			}
			releaseScreenBuffers();

			resX /= 2;
			if (allocateScreenBuffer(resX, resY, samples))
			{
				return FBO_SUCCESS_LOWRES;
			}
			releaseScreenBuffers();
		}

		LL_WARNS() << "Unable to allocate screen buffer at any resolution!" << LL_ENDL;
	}

	return ret;
}

bool LLPipeline::allocateScreenBuffer(U32 resX, U32 resY, U32 samples)
{
	mAuxScreenRectVB = NULL;

	refreshCachedSettings();
	U32 res_mod = gSavedSettings.getU32("RenderResolutionDivisor");
	if (res_mod > 1 && res_mod < resX && res_mod < resY)
	{
		resX /= res_mod;
		resY /= res_mod;
	}

	mSampleBuffer.release();
	mScreen.release();
	mFinalScreen.release();

	mDeferredDownsampledDepth.release();

	if (LLPipeline::sRenderDeferred)
	{
		static const LLCachedControl<S32> shadow_detail("RenderShadowDetail",0);
		static const LLCachedControl<bool> ssao ("RenderDeferredSSAO",false);
		static const LLCachedControl<bool> RenderDepthOfField("RenderDepthOfField",false);
		static const LLCachedControl<F32> RenderShadowResolutionScale("RenderShadowResolutionScale",1.0f);
		static const LLCachedControl<F32> RenderSSAOResolutionScale("SHRenderSSAOResolutionScale",.5f);

		const U32 occlusion_divisor = 3;

		//allocate deferred rendering color buffers
		if (!mDeferredScreen.allocate(resX, resY, GL_SRGB8_ALPHA8, TRUE, TRUE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		if (!mDeferredDepth.allocate(resX, resY, 0, TRUE, FALSE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		if (!addDeferredAttachments(mDeferredScreen)) return false;
		
		GLuint screenFormat = GL_RGBA16;
		if (gGLManager.mIsATI)
		{
			screenFormat = GL_RGBA12;
		}

		if (gGLManager.mGLVersion < 4.f && gGLManager.mIsNVIDIA)
		{
			screenFormat = GL_RGBA16F_ARB;
		}
        
		if (!mScreen.allocate(resX, resY, screenFormat, FALSE, FALSE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		if (!mFinalScreen.allocate(resX, resY, GL_RGBA, FALSE, FALSE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		if (samples > 0)
		{
			if (!mFXAABuffer.allocate(resX, resY, GL_RGBA, FALSE, FALSE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		}
		else
		{
			mFXAABuffer.release();
		}
		
		if (shadow_detail > 0 || ssao || RenderDepthOfField || samples > 0)
		{ //only need mDeferredLight for shadows OR ssao OR dof OR fxaa
			if (!mDeferredLight.allocate(resX, resY, GL_RGBA, FALSE, FALSE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
			if(ssao)
			{
				F32 scale = llclamp(RenderSSAOResolutionScale.get(),.01f,1.f);
				if( scale < 1.f && !mDeferredDownsampledDepth.allocate(llceil(F32(resX)*scale), llceil(F32(resY)*scale), 0, TRUE, FALSE, LLTexUnit::TT_TEXTURE, FALSE) ) return false;
			}
		}
		else
		{
			mDeferredLight.release();
		}

		F32 scale = RenderShadowResolutionScale;

		if (shadow_detail > 0)
		{ //allocate 4 sun shadow maps
			U32 sun_shadow_map_width = ((U32(resX*scale)+1)&~1); // must be even to avoid a stripe in the horizontal shadow blur
			for (U32 i = 0; i < 4; i++)
			{
				if (!mShadow[i].allocate(sun_shadow_map_width,U32(resY*scale), 0, TRUE, FALSE, LLTexUnit::TT_TEXTURE)) return false;
			}
		}
		else
		{
			for (U32 i = 0; i < 4; i++)
			{
				mShadow[i].release();
			}
		}

		U32 width = (U32) (resX*scale);
		U32 height = width;

		if (shadow_detail > 1)
		{ //allocate two spot shadow maps
			U32 spot_shadow_map_width = width;
			for (U32 i = 4; i < 6; i++)
			{
				if (!mShadow[i].allocate(spot_shadow_map_width, height, 0, TRUE, FALSE)) return false;
			}
		}
		else
		{
			for (U32 i = 4; i < 6; i++)
			{
				mShadow[i].release();
			}
		}

		//HACK make screenbuffer allocations start failing after 30 seconds
		if (gSavedSettings.getBOOL("SimulateFBOFailure"))
		{
			return false;
		}
	}
	else
	{
		mDeferredLight.release();
				
		for (U32 i = 0; i < 6; i++)
		{
			mShadow[i].release();
		}
		mFXAABuffer.release();
		mScreen.release();
		mFinalScreen.release();
		mDeferredScreen.release(); //make sure to release any render targets that share a depth buffer with mDeferredScreen first
		mDeferredDepth.release();
		mDeferredDownsampledDepth.release();
						
		if (!mScreen.allocate(resX, resY, GL_RGBA, TRUE, TRUE, LLTexUnit::TT_TEXTURE, FALSE)) return false;
		if(samples > 1 && mScreen.getFBO())
		{
			if(mSampleBuffer.allocate(resX,resY,GL_RGBA,TRUE,TRUE,LLTexUnit::TT_TEXTURE,FALSE,samples))
				mScreen.setSampleBuffer(&mSampleBuffer);
			else
			{
				mSampleBuffer.release();
				return false;
			}
		}
		
	}
	
	if (LLPipeline::sRenderDeferred)
	{ //share depth buffer between deferred targets
		mDeferredScreen.shareDepthBuffer(mScreen);
		mDeferredScreen.shareDepthBuffer(mFinalScreen);
		//mDeferredScreen.shareDepthBuffer(mDeferredLight);
		/*for (U32 i = 0; i < 3; i++)
		{ //share stencil buffer with screen space lightmap to stencil out sky
			if (mDeferredLight[i].getTexture(0))
			{
				mDeferredScreen.shareDepthBuffer(mDeferredLight[i]);
			}
		}*/
	}

	gGL.getTexUnit(0)->disable();

	stop_glerror();

	return true;
}

bool LLPipeline::isRenderDeferredCapable()
{
	return gGLManager.mHasFramebufferObject &&
		LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
		LLFeatureManager::getInstance()->isFeatureAvailable("RenderAvatarVP") &&			//Hardware Skinning. Deferred forces RenderAvatarVP to true
		LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") &&		//Basic Shaders
		LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders") &&	//Atmospheric Shaders
		LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable");
}

bool LLPipeline::isRenderDeferredDesired()
{
	return isRenderDeferredCapable() &&
		gSavedSettings.getBOOL("RenderDeferred") &&
		gSavedSettings.getBOOL("VertexShaderEnable") &&
		gSavedSettings.getBOOL("WindLightUseAtmosShaders");
}

//static
void LLPipeline::updateRenderDeferred()
{
	bool deferred = (bool(LLRenderTarget::sUseFBO &&
					 LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&	 
					 LLPipeline::sRenderBump &&
					 isRenderDeferredDesired())) &&
					!gUseWireframe;

	sRenderDeferred = deferred;	
	if (deferred)
	{ //must render glow when rendering deferred since post effect pass is needed to present any lighting at all
		sRenderGlow = true;
	}
}


//static
void LLPipeline::refreshCachedSettings()
{
	LLRenderTarget::sUseFBO = gSavedSettings.getBOOL("RenderUseFBO") || LLPipeline::sRenderDeferred;
	LLPipeline::sAutoMaskAlphaDeferred = gSavedSettings.getBOOL("RenderAutoMaskAlphaDeferred");
	LLPipeline::sAutoMaskAlphaNonDeferred = gSavedSettings.getBOOL("RenderAutoMaskAlphaNonDeferred");
	LLPipeline::sUseFarClip = gSavedSettings.getBOOL("RenderUseFarClip");
	LLVOAvatar::sMaxVisible = (U32)gSavedSettings.getS32("RenderAvatarMaxVisible");
	//LLPipeline::sDelayVBUpdate = gSavedSettings.getBOOL("RenderDelayVBUpdate");
	gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
	gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");
	gOctreeReserveCapacity = llmin(gSavedSettings.getU32("OctreeReserveNodeCapacity"), U32(512));
	LLPipeline::sDynamicLOD = gSavedSettings.getBOOL("RenderDynamicLOD");
	LLPipeline::sRenderBump = gSavedSettings.getBOOL("RenderObjectBump");
	LLVertexBuffer::sUseStreamDraw = gSavedSettings.getBOOL("ShyotlRenderUseStreamVBO");
	LLVertexBuffer::sEnableVBOs = gSavedSettings.getBOOL("RenderVBOEnable");
	LLVertexBuffer::sUseVAO = gSavedSettings.getBOOL("RenderUseVAO") && gSavedSettings.getBOOL("VertexShaderEnable"); //Temporary workaround for vaos being broken when shaders are off
	LLVertexBuffer::sDisableVBOMapping = LLVertexBuffer::sEnableVBOs;// && gSavedSettings.getBOOL("RenderVBOMappingDisable") ; //Temporary workaround for vbo mapping being straight up broken
	LLVertexBuffer::sPreferStreamDraw = gSavedSettings.getBOOL("RenderPreferStreamDraw");
	LLPipeline::sRenderAttachedLights = gSavedSettings.getBOOL("RenderAttachedLights");
	LLPipeline::sRenderAttachedParticles = gSavedSettings.getBOOL("RenderAttachedParticles");
	LLPipeline::sTextureBindTest = gSavedSettings.getBOOL("RenderDebugTextureBind");
	
	LLPipeline::sUseOcclusion = 
			(!gUseWireframe
			&& LLGLSLShader::sNoFixedFunction
			&& LLFeatureManager::getInstance()->isFeatureAvailable("UseOcclusion") 
			&& gSavedSettings.getBOOL("UseOcclusion") 
			&& gGLManager.mHasOcclusionQuery) ? 2 : 0;
}

void LLPipeline::releaseOcclusionBuffers()
{
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
		iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				part->resetVertexBuffers();
			}
		}
	}
}
void LLPipeline::releaseVertexBuffers()
{
	mCubeVB = NULL;
	mAuxScreenRectVB = NULL;

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
		iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				part->resetVertexBuffers();
			}
		}
	}

	resetDrawOrders();

	gSky.resetVertexBuffers();

	LLVOPartGroup::destroyGL();

	if (LLPostProcess::instanceExists())
		LLPostProcess::getInstance()->destroyGL();

	LLVOPartGroup::destroyGL();

	//delete all name pool caches
	LLGLNamePool::cleanupPools();

	gGL.resetVertexBuffers();

	LLVertexBuffer::cleanupClass();

	if (LLVertexBuffer::sGLCount > 0)
	{
		LL_WARNS() << "VBO wipe failed -- " << LLVertexBuffer::sGLCount << " buffers remaining. " << LLVertexBuffer::sCount << LL_ENDL;
	}

	LLVertexBuffer::unbind();
}

void LLPipeline::releaseGLBuffers()
{
	assertInitialized();
	
	mNoiseMap.reset();

	releaseLUTBuffers();

	mWaterRef.release();
	mWaterDis.release();
	
	for (U32 i = 0; i < 2; i++)
	{
		mGlow[i].release();
	}

	releaseScreenBuffers();

	gBumpImageList.destroyGL();
	LLVOAvatar::resetImpostors();

	if(LLPostProcess::instanceExists())
		LLPostProcess::getInstance()->destroyGL();
}

void LLPipeline::releaseLUTBuffers()
{
	mLightFunc.reset();
}

void LLPipeline::releaseScreenBuffers()
{
	mScreen.release();
	mFinalScreen.release();
	mFXAABuffer.release();
	mPhysicsDisplay.release();
	mDeferredScreen.release();
	mDeferredDepth.release();
	mDeferredDownsampledDepth.release();
	mDeferredLight.release();
		
	for (U32 i = 0; i < 6; i++)
	{
		mShadow[i].release();
	}
}


void LLPipeline::createGLBuffers()
{
	stop_glerror();
	assertInitialized();

	bool materials_in_water = false;

#if MATERIALS_IN_REFLECTIONS
	materials_in_water = gSavedSettings.getS32("RenderWaterMaterials");
#endif

	if (LLPipeline::sWaterReflections)
	{ //water reflection texture
		U32 res = (U32) llmax(gSavedSettings.getS32("RenderWaterRefResolution"), 512);
			
		// Set up SRGB targets if we're doing deferred-path reflection rendering
		//
		if (LLPipeline::sRenderDeferred && materials_in_water)
		{
			mWaterRef.allocate(res,res,GL_SRGB8_ALPHA8,TRUE,FALSE);
			//always use FBO for mWaterDis so it can be used for avatar texture bakes
			mWaterDis.allocate(res,res,GL_SRGB8_ALPHA8,TRUE,FALSE,LLTexUnit::TT_TEXTURE, true);
		}
		else
		{
			mWaterRef.allocate(res,res,GL_RGBA,TRUE,FALSE);
			//always use FBO for mWaterDis so it can be used for avatar texture bakes
			mWaterDis.allocate(res,res,GL_RGBA,TRUE,FALSE,LLTexUnit::TT_TEXTURE, true);
		}
	}


	stop_glerror();

	GLuint resX = gViewerWindow->getWorldViewWidthRaw();
	GLuint resY = gViewerWindow->getWorldViewHeightRaw();
	


	if (LLPipeline::sRenderGlow)
	{ //screen space glow buffers
		const U32 glow_res = llmax(1, 
			llmin(512, 1 << gSavedSettings.getS32("RenderGlowResolutionPow")));

		glClearColor(0,0,0,0);
		gGL.setColorMask(true, true);
		for (U32 i = 0; i < 2; i++)
		{
			if(mGlow[i].allocate(512,glow_res,GL_RGBA,FALSE,FALSE))
			{
				mGlow[i].bindTarget();
				mGlow[i].clear();
				mGlow[i].flush();
			}
		}


		allocateScreenBuffer(resX,resY);

	}

	if (sRenderDeferred)
	{
		if (!mNoiseMap)
		{
			std::array<LLVector3, NOISE_MAP_RES * NOISE_MAP_RES> noise;

			F32 scaler = gSavedSettings.getF32("RenderDeferredNoise")/100.f;
			for (auto& val : noise)
			{
				val = LLVector3(ll_frand()-0.5f, ll_frand()-0.5f, 0.f);
				val.normVec();
				val.mV[2] = ll_frand()*scaler+1.f-scaler/2.f;
			}

			mNoiseMap = LLImageGL::createTextureName();
			
			gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap->getTexName());
			LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_RGB16F_ARB, NOISE_MAP_RES, NOISE_MAP_RES, GL_RGB, GL_FLOAT, noise.data());
			gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}

		createLUTBuffers();
	}

	gBumpImageList.restoreGL();
}

void LLPipeline::createLUTBuffers()
{
	if (sRenderDeferred)
	{
		if (!mLightFunc)
		{
			U32 lightResX = gSavedSettings.getU32("RenderSpecularResX");
			U32 lightResY = gSavedSettings.getU32("RenderSpecularResY");
			//U8* ls = new U8[lightResX*lightResY];
			F32* ls = new F32[lightResX*lightResY];
			//static const LLCachedControl<F32> specExp("RenderSpecularExponent");
            // Calculate the (normalized) Blinn-Phong specular lookup texture.
			for (U32 y = 0; y < lightResY; ++y)
			{
				for (U32 x = 0; x < lightResX; ++x)
				{
					ls[y*lightResX+x] = 0;
					F32 sa = (F32) x/(lightResX-1);
					F32 spec = (F32) y/(lightResY-1);
					F32 n = spec * spec * 368;//specExp;
					
					// Nothing special here.  Just your typical blinn-phong term.
					spec = powf(sa, n);
					
					// Apply our normalization function.
					// Note: This is the full equation that applies the full normalization curve, not an approximation.
					// This is fine, given we only need to create our LUT once per buffer initialization.
					// The only trade off is we have a really low dynamic range.
					// This means we have to account for things not being able to exceed 0 to 1 in our shaders.
					spec *= (((n + 2) * (n + 4)) / (8 * F_PI * (powf(2, -n/2) + n)));
					
					// Always sample at a 1.0/2.2 curve.
					// This "Gamma corrects" our specular term, boosting our lower exponent reflections.
					ls[y*lightResX+x] = spec;
					
					// Easy fix for our dynamic range problem: divide by 6 here, multiply by 6 in our shaders.
					// This allows for our specular term to exceed a value of 1 in our shaders.
					// This is something that can be important for energy conserving specular models where higher exponents can result in highlights that exceed a range of 0 to 1.
					// Technically, we could just use an R16F texture, but driver support for R16F textures can be somewhat spotty at times.
					// This works remarkably well for higher specular exponents, though banding can sometimes be seen on lower exponents.
					// Combined with a bit of noise and trilinear filtering, the banding is hardly noticable.
					//ls[y*lightResX+x] = (U8)(llclamp(spec * (1.f / 6), 0.f, 1.f) * 255);
				}
			}
			
			U32 pix_format = GL_R16F;
#if LL_DARWIN
			// Need to work around limited precision with 10.6.8 and older drivers
			//
			pix_format = GL_R32F;
#endif
			mLightFunc = LLImageGL::createTextureName();
			gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc->getTexName());
			LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, pix_format, lightResX, lightResY, GL_RED, GL_FLOAT, ls);
			//LLImageGL::setManualImage(LLTexUnit::getInternalType(LLTexUnit::TT_TEXTURE), 0, GL_UNSIGNED_BYTE, lightResX, lightResY, GL_RED, GL_UNSIGNED_BYTE, ls, false);
			gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			
			delete [] ls;
		}
	}
}


void LLPipeline::restoreGL() 
{
	assertInitialized();

	LLViewerShaderMgr::instance()->setShaders();

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				part->restoreGL();
			}
		}
	}

	updateLocalLightingEnabled(); //Default all gl light parameters. Fixes light brightness problems on fullscren toggle
}

BOOL LLPipeline::canUseWindLightShaders() const
{
	return (gWLSkyProgram.mProgramObject != 0 &&
			LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_WINDLIGHT) > 1);
}

BOOL LLPipeline::canUseWindLightShadersOnObjects() const
{
	return (canUseWindLightShaders() 
		&& LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT) > 0);
}

BOOL LLPipeline::canUseAntiAliasing() const
{
	return TRUE;
}

void LLPipeline::unloadShaders()
{
	LLViewerShaderMgr::instance()->unloadShaders();
}

void LLPipeline::assertInitializedDoError()
{
	LL_ERRS() << "LLPipeline used when uninitialized." << LL_ENDL;
}

//============================================================================

void LLPipeline::enableShadows(const BOOL enable_shadows)
{
	//should probably do something here to wrangle shadows....	
}

void LLPipeline::updateLocalLightingEnabled()
{
	refreshCachedSettings();

	static const LLCachedControl<bool> render_local_lights("RenderLocalLights", true);
	//Bugfix: If setshaders was called with RenderLocalLights off then enabling RenderLocalLights later will not work. Reloading shaders fixes this.
	if (render_local_lights != mLightingEnabled)
	{
		mLightingEnabled = render_local_lights;
		if (LLGLSLShader::sNoFixedFunction)
			LLViewerShaderMgr::instance()->setShaders();
	}
}

class LLOctreeDirtyTexture : public OctreeTraveler
{
public:
	const std::set<LLViewerFetchedTexture*>& mTextures;

	LLOctreeDirtyTexture(const std::set<LLViewerFetchedTexture*>& textures) : mTextures(textures) { }

	virtual void visit(const OctreeNode* node)
	{
		LLSpatialGroup* group = (LLSpatialGroup*) node->getListener(0);

		if (!group->hasState(LLSpatialGroup::GEOM_DIRTY) && !group->isEmpty())
		{
			for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
			{
				for (LLSpatialGroup::drawmap_elem_t::iterator j = i->second.begin(); j != i->second.end(); ++j) 
				{
					LLDrawInfo* params = *j;
					LLViewerFetchedTexture* tex = LLViewerTextureManager::staticCastToFetchedTexture(params->mTexture);
					if (tex && mTextures.find(tex) != mTextures.end())
					{ 
						group->setState(LLSpatialGroup::GEOM_DIRTY);
					}
				}
			}
		}

		for (LLSpatialGroup::bridge_list_t::iterator i = group->mBridgeList.begin(); i != group->mBridgeList.end(); ++i)
		{
			LLSpatialBridge* bridge = *i;
			traverse(bridge->mOctree);
		}
	}
};

// Called when a texture changes # of channels (causes faces to move to alpha pool)
void LLPipeline::dirtyPoolObjectTextures(const std::set<LLViewerFetchedTexture*>& textures)
{
	assertInitialized();

	// *TODO: This is inefficient and causes frame spikes; need a better way to do this
	//        Most of the time is spent in dirty.traverse.

	for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
	{
		LLDrawPool *poolp = *iter;
		if (poolp->isFacePool())
		{
			((LLFacePool*) poolp)->dirtyTextures(textures);
		}
	}
	
	LLOctreeDirtyTexture dirty(textures);
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				dirty.traverse(part->mOctree);
			}
		}
	}
}

LLDrawPool *LLPipeline::findPool(const U32 type, LLViewerTexture *tex0)
{
	assertInitialized();

	LLDrawPool *poolp = NULL;
	switch( type )
	{
	case LLDrawPool::POOL_SIMPLE:
		poolp = mSimplePool;
		break;

	case LLDrawPool::POOL_GRASS:
		poolp = mGrassPool;
		break;

	case LLDrawPool::POOL_ALPHA_MASK:
		poolp = mAlphaMaskPool;
		break;

	case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
		poolp = mFullbrightAlphaMaskPool;
		break;

	case LLDrawPool::POOL_FULLBRIGHT:
		poolp = mFullbrightPool;
		break;

	case LLDrawPool::POOL_GLOW:
		poolp = mGlowPool;
		break;

	case LLDrawPool::POOL_TREE:
		poolp = get_if_there(mTreePools, (uintptr_t)tex0, (LLDrawPool*)0 );
		break;

	case LLDrawPool::POOL_TERRAIN:
		poolp = get_if_there(mTerrainPools, (uintptr_t)tex0, (LLDrawPool*)0 );
		break;

	case LLDrawPool::POOL_BUMP:
		poolp = mBumpPool;
		break;
	case LLDrawPool::POOL_MATERIALS:
		poolp = mMaterialsPool;
		break;
	case LLDrawPool::POOL_ALPHA:
		poolp = mAlphaPool;
		break;

	case LLDrawPool::POOL_AVATAR:
		break; // Do nothing

	case LLDrawPool::POOL_SKY:
		poolp = mSkyPool;
		break;

	case LLDrawPool::POOL_WATER:
		poolp = mWaterPool;
		break;

	case LLDrawPool::POOL_GROUND:
		poolp = mGroundPool;
		break;

	case LLDrawPool::POOL_WL_SKY:
		poolp = mWLSkyPool;
		break;

	default:
		llassert(0);
		LL_ERRS() << "Invalid Pool Type in  LLPipeline::findPool() type=" << type << LL_ENDL;
		break;
	}

	return poolp;
}


LLDrawPool *LLPipeline::getPool(const U32 type,	LLViewerTexture *tex0)
{
	LLDrawPool *poolp = findPool(type, tex0);
	if (poolp)
	{
		return poolp;
	}


	LLDrawPool *new_poolp = LLDrawPool::createPool(type, tex0);
	addPool( new_poolp );

	return new_poolp;
}


// static
LLDrawPool* LLPipeline::getPoolFromTE(const LLTextureEntry* te, LLViewerTexture* imagep)
{
	U32 type = getPoolTypeFromTE(te, imagep);
	return gPipeline.getPool(type, imagep);
}

//static 
U32 LLPipeline::getPoolTypeFromTE(const LLTextureEntry* te, LLViewerTexture* imagep)
{
	if (!te || !imagep)
	{
		return 0;
	}
		
	LLMaterial* mat = te->getMaterialParams().get();

	bool color_alpha = te->getColor().mV[3] < 0.999f;
	bool alpha = color_alpha;
	if (imagep)
	{
		alpha = alpha || (imagep->getComponents() == 4 && imagep->getType() != LLViewerTexture::MEDIA_TEXTURE) || (imagep->getComponents() == 2);
	}

	if (alpha && mat)
	{
		switch (mat->getDiffuseAlphaMode())
		{
			case LLMaterial::DIFFUSE_ALPHA_MODE_BLEND:
				alpha = true; // Material's alpha mode is set to blend.  Toss it into the alpha draw pool.
				break;
			case LLMaterial::DIFFUSE_ALPHA_MODE_NONE: //alpha mode set to none, never go to alpha pool
			case LLMaterial::DIFFUSE_ALPHA_MODE_EMISSIVE: //alpha mode set to emissive, never go to alpha pool
				alpha = color_alpha;
				break;
			default: //alpha mode set to "mask", go to alpha pool if fullbright
				alpha = color_alpha; // Material's alpha mode is set to mask, or default.  Toss it into the opaque material draw pool.
				break;
		}
	}
	
	static const LLCachedControl<bool> alt_batching("SHAltBatching",true);

	if(!alt_batching)
	{
	if (alpha)
	{
		return LLDrawPool::POOL_ALPHA;
	}
	else if ((te->getBumpmap() || te->getShiny()) && (!mat || mat->getNormalID().isNull()))
	{
		return LLDrawPool::POOL_BUMP;
	}
	else if (mat && !alpha)
	{
		return LLDrawPool::POOL_MATERIALS;
	}
	else
	{
		return LLDrawPool::POOL_SIMPLE;
	}
	}
	else
	{
	static const LLCachedControl<bool> sh_fullbright_deferred("SHFullbrightDeferred",true);

	//Bump goes into bump pool unless using deferred and there's a normal map that takes precedence.
	bool legacy_bump = (!LLPipeline::sRenderDeferred || !mat || mat->getNormalID().isNull()) && LLPipeline::sRenderBump && te->getBumpmap() && te->getBumpmap() < 18;
	if (alpha)
	{
		return LLDrawPool::POOL_ALPHA;
	}
	else if (mat && mat->getDiffuseAlphaMode() == LLMaterial::DIFFUSE_ALPHA_MODE_MASK)
	{
		if(!LLPipeline::sRenderDeferred || legacy_bump)
		{
			return te->getFullbright() ? LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK : LLDrawPool::POOL_ALPHA_MASK;
		}
		else if(te->getFullbright() && !mat->getEnvironmentIntensity() && !te->getShiny())
		{
			return LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK;
		}
		return LLDrawPool::POOL_MATERIALS;
	}
	else if (legacy_bump)
	{
		return LLDrawPool::POOL_BUMP;	
	}
	else if(LLPipeline::sRenderDeferred && mat)
	{
		if(te->getFullbright() && !mat->getEnvironmentIntensity() && !te->getShiny())
		{
			return sh_fullbright_deferred ? LLDrawPool::POOL_FULLBRIGHT : LLDrawPool::POOL_SIMPLE;
		}
		return LLDrawPool::POOL_MATERIALS;
	}
	else if((sh_fullbright_deferred || !LLPipeline::sRenderDeferred) && te->getFullbright())
	{
		return (LLPipeline::sRenderBump && te->getShiny()) ? LLDrawPool::POOL_BUMP : LLDrawPool::POOL_FULLBRIGHT;
	}
	else if (!LLPipeline::sRenderDeferred && LLPipeline::sRenderBump && te->getShiny())
	{
		return LLDrawPool::POOL_BUMP;	//Shiny goes into bump pool when not using deferred rendering.
	}
	else
	{
		return LLDrawPool::POOL_SIMPLE;
	}
	}
}


void LLPipeline::addPool(LLDrawPool *new_poolp)
{
	assertInitialized();
	mPools.insert(new_poolp);
	addToQuickLookup( new_poolp );
}

void LLPipeline::allocDrawable(LLViewerObject *vobj)
{
	if(!vobj)
	{
		LL_ERRS() << "Null object passed to allocDrawable!" << LL_ENDL;
	}

	LLDrawable *drawable = new LLDrawable(vobj);
	vobj->mDrawable = drawable;
	
	//encompass completely sheared objects by taking 
	//the most extreme point possible (<1,1,0.5>)
	drawable->setRadius(LLVector3(1,1,0.5f).scaleVec(vobj->getScale()).length());
	if (vobj->isOrphaned())
	{
		drawable->setState(LLDrawable::FORCE_INVISIBLE);
	}
	drawable->updateXform(TRUE);
}


static LLTrace::BlockTimerStatHandle FTM_UNLINK("Unlink");
static LLTrace::BlockTimerStatHandle FTM_REMOVE_FROM_MOVE_LIST("Movelist");
static LLTrace::BlockTimerStatHandle FTM_REMOVE_FROM_SPATIAL_PARTITION("Spatial Partition");
static LLTrace::BlockTimerStatHandle FTM_REMOVE_FROM_LIGHT_SET("Light Set");
//static LLTrace::BlockTimerStatHandle FTM_REMOVE_FROM_HIGHLIGHT_SET("Highlight Set");

void LLPipeline::unlinkDrawable(LLDrawable *drawable)
{
	LL_RECORD_BLOCK_TIME(FTM_UNLINK);

	assertInitialized();

	LLPointer<LLDrawable> drawablep = drawable; // make sure this doesn't get deleted before we are done
	
	// Based on flags, remove the drawable from the queues that it's on.
	if (drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		LL_RECORD_BLOCK_TIME(FTM_REMOVE_FROM_MOVE_LIST);
		LLDrawable::drawable_vector_t::iterator iter = std::find(mMovedList.begin(), mMovedList.end(), drawablep);
		if (iter != mMovedList.end())
		{
			mMovedList.erase(iter);
		}
	}

	if (drawablep->getSpatialGroup())
	{
		LL_RECORD_BLOCK_TIME(FTM_REMOVE_FROM_SPATIAL_PARTITION);
		if (!drawablep->getSpatialGroup()->getSpatialPartition()->remove(drawablep, drawablep->getSpatialGroup()))
		{
#ifdef LL_RELEASE_FOR_DOWNLOAD
			LL_WARNS() << "Couldn't remove object from spatial group!" << LL_ENDL;
#else
			LL_ERRS() << "Couldn't remove object from spatial group!" << LL_ENDL;
#endif
		}
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_REMOVE_FROM_LIGHT_SET);
		mLights.erase(drawablep);

		for (light_set_t::iterator iter = mNearbyLights.begin();
					iter != mNearbyLights.end(); iter++)
		{
			if (iter->drawable == drawablep)
			{
				mNearbyLights.erase(iter);
				break;
			}
		}
	}

	for (U32 i = 0; i < 2; ++i)
	{
		if (mShadowSpotLight[i] == drawablep)
		{
			mShadowSpotLight[i] = NULL;
		}

		if (mTargetShadowSpotLight[i] == drawablep)
		{
			mTargetShadowSpotLight[i] = NULL;
		}
	}


}

//static
void LLPipeline::removeMutedAVsLights(LLVOAvatar* muted_avatar)
{
	LL_RECORD_BLOCK_TIME(FTM_REMOVE_FROM_LIGHT_SET);
	for (light_set_t::iterator iter = gPipeline.mNearbyLights.begin();
		 iter != gPipeline.mNearbyLights.end();)
	{
		if (iter->drawable->getVObj()->isAttachment() && iter->drawable->getVObj()->getAvatar() == muted_avatar)
		{
			gPipeline.mLights.erase(iter->drawable);
			gPipeline.mNearbyLights.erase(iter++);
		}
		else ++iter;
	}
}

U32 LLPipeline::addObject(LLViewerObject *vobj)
{
	llassert_always(vobj);
	if (gNoRender)
	{
		return 0;
	}

	static const LLCachedControl<bool> render_delay_creation("RenderDelayCreation",false);
	if (!vobj->isAvatar() && render_delay_creation)
	{
		mCreateQ.push_back(vobj);
	}
	else
	{
		createObject(vobj);
	}

	return 1;
}

void LLPipeline::createObjects(F32 max_dtime)
{
	LL_RECORD_BLOCK_TIME(FTM_PIPELINE_CREATE);

	LLTimer update_timer;

	while (!mCreateQ.empty() && update_timer.getElapsedTimeF32() < max_dtime)
	{
		LLViewerObject* vobj = mCreateQ.front();
		if (!vobj->isDead())
		{
			createObject(vobj);
		}
		mCreateQ.pop_front();
	}
	
	//for (LLViewerObject::vobj_list_t::iterator iter = mCreateQ.begin(); iter != mCreateQ.end(); ++iter)
	//{
	//	createObject(*iter);
	//}

	//mCreateQ.clear();
}

void LLPipeline::createObject(LLViewerObject* vobj)
{
	LLDrawable* drawablep = vobj->mDrawable;

	if (!drawablep)
	{
		drawablep = vobj->createDrawable(this);
	}
	else
	{
		LL_ERRS() << "Redundant drawable creation!" << LL_ENDL;
	}
		
	llassert(drawablep);

	if (vobj->getParent())
	{
		vobj->setDrawableParent(((LLViewerObject*)vobj->getParent())->mDrawable); // LLPipeline::addObject 1
	}
	else
	{
		vobj->setDrawableParent(NULL); // LLPipeline::addObject 2
	}

	markRebuild(drawablep, LLDrawable::REBUILD_ALL, TRUE);

	static const LLCachedControl<bool> render_animate_res("RenderAnimateRes",false);
	if (drawablep->getVOVolume() && render_animate_res)
	{
		// fun animated res
		drawablep->updateXform(TRUE);
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
		drawablep->setScale(LLVector3(0,0,0));
		drawablep->makeActive();
	}
}


void LLPipeline::resetFrameStats()
{
	assertInitialized();

	sCompiles = 0;

	LLViewerStats::getInstance()->mTrianglesDrawnStat.addValue(mTrianglesDrawn/1000.f);

	if (mBatchCount > 0)
	{
		mMeanBatchSize = gPipeline.mTrianglesDrawn/gPipeline.mBatchCount;
	}
	mTrianglesDrawn = 0;

	if (mOldRenderDebugMask != mRenderDebugMask)
	{
		gObjectList.clearDebugText();
		mOldRenderDebugMask = mRenderDebugMask;
	}
		
}

//external functions for asynchronous updating
void LLPipeline::updateMoveDampedAsync(LLDrawable* drawablep)
{
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time)
	{
		return;
	}
	if (!drawablep)
	{
		LL_ERRS() << "updateMove called with NULL drawablep" << LL_ENDL;
		return;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	assertInitialized();

	// update drawable now
	drawablep->clearState(LLDrawable::MOVE_UNDAMPED); // force to DAMPED
	drawablep->updateMove(); // returns done
	drawablep->setState(LLDrawable::EARLY_MOVE); // flag says we already did an undamped move this frame
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.push_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMoveNormalAsync(LLDrawable* drawablep)
{
	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time)
	{
		return;
	}
	if (!drawablep)
	{
		LL_ERRS() << "updateMove called with NULL drawablep" << LL_ENDL;
		return;
	}
	if (drawablep->isState(LLDrawable::EARLY_MOVE))
	{
		return;
	}

	assertInitialized();

	// update drawable now
	drawablep->setState(LLDrawable::MOVE_UNDAMPED); // force to UNDAMPED
	drawablep->updateMove();
	drawablep->setState(LLDrawable::EARLY_MOVE); // flag says we already did an undamped move this frame
	// Put on move list so that EARLY_MOVE gets cleared
	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		mMovedList.push_back(drawablep);
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
}

void LLPipeline::updateMovedList(LLDrawable::drawable_vector_t& moved_list)
{
	for (LLDrawable::drawable_vector_t::iterator iter = moved_list.begin();
		 iter != moved_list.end(); )
	{
		LLDrawable::drawable_vector_t::iterator curiter = iter++;
		LLDrawable *drawablep = *curiter;
		BOOL done = TRUE;
		if (!drawablep->isDead() && (!drawablep->isState(LLDrawable::EARLY_MOVE)))
		{
			done = drawablep->updateMove();
		}
		drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED);
		if (done)
		{
			if (drawablep->isRoot())
			{
				drawablep->makeStatic();
			}
			drawablep->clearState(LLDrawable::ON_MOVE_LIST);
			if (drawablep->isState(LLDrawable::ANIMATED_CHILD))
			{ //will likely not receive any future world matrix updates
				// -- this keeps attachments from getting stuck in space and falling off your avatar
				drawablep->clearState(LLDrawable::ANIMATED_CHILD);
				markRebuild(drawablep, LLDrawable::REBUILD_VOLUME, TRUE);
				if (drawablep->getVObj())
				{
					drawablep->getVObj()->dirtySpatialGroup(TRUE);
				}
			}
			iter = moved_list.erase(curiter);
		}
	}
}

static LLTrace::BlockTimerStatHandle FTM_OCTREE_BALANCE("Balance Octree");
static LLTrace::BlockTimerStatHandle FTM_UPDATE_MOVE("Update Move");

void LLPipeline::updateMove()
{
	LL_RECORD_BLOCK_TIME(FTM_UPDATE_MOVE);

	static const LLCachedControl<bool> freeze_time("FreezeTime",false);
	if (freeze_time)
	{
		return;
	}

	assertInitialized();

	{
		static LLTrace::BlockTimerStatHandle ftm("Retexture");
		LL_RECORD_BLOCK_TIME(ftm);

		for (LLDrawable::drawable_set_t::iterator iter = mRetexturedList.begin();
			 iter != mRetexturedList.end(); ++iter)
		{
			LLDrawable* drawablep = *iter;
			if (drawablep && !drawablep->isDead())
			{
				drawablep->updateTexture();
			}
		}
		mRetexturedList.clear();
	}

	{
		static LLTrace::BlockTimerStatHandle ftm("Moved List");
		LL_RECORD_BLOCK_TIME(ftm);
		updateMovedList(mMovedList);
	}

	//balance octrees
	{
		LL_RECORD_BLOCK_TIME(FTM_OCTREE_BALANCE);

		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				if (part)
				{
					part->mOctree->balance();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
// Culling and occlusion testing
/////////////////////////////////////////////////////////////////////////////

//static
F32 LLPipeline::calcPixelArea(LLVector3 center, LLVector3 size, LLCamera &camera)
{
	LLVector3 lookAt = center - camera.getOrigin();
	F32 dist = lookAt.length();

	//ramp down distance for nearby objects
	//shrink dist by dist/16.
	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	//get area of circle around node
	F32 app_angle = atanf(size.length()/dist);
	F32 radius = app_angle*LLDrawable::sCurPixelAngle;
	return radius*radius * F_PI;
}

//static
F32 LLPipeline::calcPixelArea(const LLVector4a& center, const LLVector4a& size, LLCamera &camera)
{
	LLVector4a origin;
	origin.load3(camera.getOrigin().mV);

	LLVector4a lookAt;
	lookAt.setSub(center, origin);
	F32 dist = lookAt.getLength3().getF32();

	//ramp down distance for nearby objects
	//shrink dist by dist/16.
	if (dist < 16.f)
	{
		dist /= 16.f;
		dist *= dist;
		dist *= 16.f;
	}

	//get area of circle around node
	F32 app_angle = atanf(size.getLength3().getF32()/dist);
	F32 radius = app_angle*LLDrawable::sCurPixelAngle;
	return radius*radius * F_PI;
}

void LLPipeline::grabReferences(LLCullResult& result)
{
	sCull = &result;
}

void LLPipeline::clearReferences()
{
	sCull = NULL;
	mGroupSaveQ1.clear();
}

void check_references(LLSpatialGroup* group, LLDrawable* drawable)
{
	for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
	{
		if (drawable == (LLDrawable*)(*i)->getDrawable())
		{
			LL_ERRS() << "LLDrawable deleted while actively reference by LLPipeline." << LL_ENDL;
		}
	}
}

void check_references(LLDrawable* drawable, LLFace* face)
{
	for (S32 i = 0; i < drawable->getNumFaces(); ++i)
	{
		if (drawable->getFace(i) == face)
		{
			LL_ERRS() << "LLFace deleted while actively referenced by LLPipeline." << LL_ENDL;
		}
	}
}

void check_references(LLSpatialGroup* group, LLFace* face)
{
	for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
	{
		LLDrawable* drawable = (LLDrawable*)(*i)->getDrawable();
		if(drawable)
		{
		check_references(drawable, face);
	}			
}
}

void LLPipeline::checkReferences(LLFace* face)
{
#if 0
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, face);
		}

		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList(); iter != sCull->endVisibleList(); ++iter)
		{
			LLDrawable* drawable = *iter;
			check_references(drawable, face);
		}
	}
#endif
}

void LLPipeline::checkReferences(LLDrawable* drawable)
{
#if 0
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, drawable);
		}

		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList(); iter != sCull->endVisibleList(); ++iter)
		{
			if (drawable == *iter)
			{
				LL_ERRS() << "LLDrawable deleted while actively referenced by LLPipeline." << LL_ENDL;
			}
		}
	}
#endif
}

void check_references(LLSpatialGroup* group, LLDrawInfo* draw_info)
{
	for (LLSpatialGroup::draw_map_t::iterator i = group->mDrawMap.begin(); i != group->mDrawMap.end(); ++i)
	{
		LLSpatialGroup::drawmap_elem_t& draw_vec = i->second;
		for (LLSpatialGroup::drawmap_elem_t::iterator j = draw_vec.begin(); j != draw_vec.end(); ++j)
		{
			LLDrawInfo* params = *j;
			if (params == draw_info)
			{
				LL_ERRS() << "LLDrawInfo deleted while actively referenced by LLPipeline." << LL_ENDL;
			}
		}
	}
}


void LLPipeline::checkReferences(LLDrawInfo* draw_info)
{
#if 0
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			check_references(group, draw_info);
		}
	}
#endif
}

void LLPipeline::checkReferences(LLSpatialGroup* group)
{
#if 0
	if (sCull)
	{
		for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
		{
			if (group == *iter)
			{
				LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
			}
		}

		for (LLCullResult::sg_iterator iter = sCull->beginAlphaGroups(); iter != sCull->endAlphaGroups(); ++iter)
		{
			if (group == *iter)
			{
				LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
			}
		}

		for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
		{
			if (group == *iter)
			{
				LL_ERRS() << "LLSpatialGroup deleted while actively referenced by LLPipeline." << LL_ENDL;
			}
		}
	}
#endif
}


BOOL LLPipeline::visibleObjectsInFrustum(LLCamera& camera)
{
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;

		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				if (hasRenderType(part->mDrawableType))
				{
					if (part->visibleObjectsInFrustum(camera))
					{
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}

BOOL LLPipeline::getVisibleExtents(LLCamera& camera, LLVector3& min, LLVector3& max)
{
	const F32 X = 65536.f;

	min = LLVector3(X,X,X);
	max = LLVector3(-X,-X,-X);

	LLViewerCamera::eCameraID saved_camera_id = LLViewerCamera::sCurCameraID;
	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

	BOOL res = TRUE;

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;

		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				if (hasRenderType(part->mDrawableType))
				{
					if (!part->getVisibleExtents(camera, min, max))
					{
						res = FALSE;
					}
				}
			}
		}
	}

	LLViewerCamera::sCurCameraID = saved_camera_id;

	return res;
}

static LLTrace::BlockTimerStatHandle FTM_CULL("Object Culling");

void LLPipeline::updateCull(LLCamera& camera, LLCullResult& result, S32 water_clip, LLPlane* planep)
{
	LL_RECORD_BLOCK_TIME(FTM_CULL);

	grabReferences(result);

	sCull->clear();

	BOOL to_texture =	LLPipeline::sUseOcclusion > 1 &&
						!hasRenderType(LLPipeline::RENDER_TYPE_HUD) && 
						LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
						LLGLSLShader::sNoFixedFunction;

	if (to_texture)
	{
		mScreen.bindTarget();
	}

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(false, false);
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(glh_get_last_projection());
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_last_modelview());

	LLGLDisable<GL_BLEND> blend;
	LLGLDisable<GL_ALPHA_TEST> test;
	LLGLDisable<GL_STENCIL_TEST> stencil;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);


	//setup a clip plane in projection matrix for reflection renders (prevents flickering from occlusion culling)
	LLViewerRegion* region = gAgent.getRegion();
	LLPlane plane;

	if (planep)
	{
		plane = *planep;
	}
	else 
	{
		if (region)
		{
			LLVector3 pnorm;
			F32 height = region->getWaterHeight();
			if (water_clip < 0)
			{ //camera is above water, clip plane points up
				pnorm.setVec(0,0,1);
				plane.setVec(pnorm, -height);
			}
			else if (water_clip > 0)
			{	//camera is below water, clip plane points down
				pnorm = LLVector3(0,0,-1);
				plane.setVec(pnorm, height);
			}
		}
	}
	
	const LLMatrix4a& modelview = glh_get_last_modelview();
	const LLMatrix4a& proj = glh_get_last_projection();
	LLGLUserClipPlane clip(plane, modelview, proj, water_clip != 0 && LLPipeline::sReflectionRender);

	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	bool bound_shader = false;
	if (LLGLSLShader::sNoFixedFunction && LLGLSLShader::sCurBoundShader == 0)
	{ //if no shader is currently bound, use the occlusion shader instead of fixed function if we can
		// (shadow render uses a special shader that clamps to clip planes)
		bound_shader = true;
		gOcclusionCubeProgram.bind();
	}

	if (sUseOcclusion > 1)
	{
		if (mCubeVB.isNull())
		{ //cube VB will be used for issuing occlusion queries
			mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX, GL_STATIC_DRAW_ARB);
		}
		mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
	}
	
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		if (water_clip != 0)
		{
			LLPlane plane(LLVector3(0,0, (F32) -water_clip), (F32) water_clip*region->getWaterHeight());
			camera.setUserClipPlane(plane);
		}
		else
		{
			camera.disableUserClipPlane();
		}

		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				if (hasRenderType(part->mDrawableType))
				{
					part->cull(camera);
				}
			}
		}
	}

	if (bound_shader)
	{
		gOcclusionCubeProgram.unbind();
	}

	camera.disableUserClipPlane();

	if (hasRenderType(LLPipeline::RENDER_TYPE_SKY) && 
		gSky.mVOSkyp.notNull() && 
		gSky.mVOSkyp->mDrawable.notNull())
	{
		gSky.mVOSkyp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOSkyp->mDrawable);
		gSky.updateCull();
		stop_glerror();
	}

	if (hasRenderType(LLPipeline::RENDER_TYPE_GROUND) && 
		!gPipeline.canUseWindLightShaders() &&
		gSky.mVOGroundp.notNull() && 
		gSky.mVOGroundp->mDrawable.notNull() &&
		!LLPipeline::sWaterReflections)
	{
		gSky.mVOGroundp->mDrawable->setVisible(camera);
		sCull->pushDrawable(gSky.mVOGroundp->mDrawable);
	}
	
	
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	if (sUseOcclusion > 1)
	{
		gGL.setColorMask(true, false);
	}

	if (to_texture)
	{
		mScreen.flush();
	}
}

void LLPipeline::markNotCulled(LLSpatialGroup* group, LLCamera& camera)
{
	if (group->isEmpty())
	{ 
		return;
	}
	
	group->setVisible();

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		group->updateDistance(camera);
	}
	
	const F32 MINIMUM_PIXEL_AREA = 16.f;

	if (group->mPixelArea < MINIMUM_PIXEL_AREA)
	{
		return;
	}

	const LLVector4a* bounds = group->getBounds();
	if (sMinRenderSize > 0.f && 
			llmax(llmax(bounds[1][0], bounds[1][1]), bounds[1][2]) < sMinRenderSize)
	{
		return;
	}

	assertInitialized();
	
	if (!group->getSpatialPartition()->mRenderByGroup)
	{ //render by drawable
		sCull->pushDrawableGroup(group);
	}
	else
	{   //render by group
		sCull->pushVisibleGroup(group);
	}

	mNumVisibleNodes++;
}

void LLPipeline::markOccluder(LLSpatialGroup* group)
{
	if (sUseOcclusion > 1 && group && !group->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION))
	{
		LLSpatialGroup* parent = group->getParent();

		if (!parent || !parent->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{ //only mark top most occluders as active occlusion
			sCull->pushOcclusionGroup(group);
			group->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
				
			if (parent && 
				!parent->isOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION) &&
				parent->getElementCount() == 0 &&
				parent->needsUpdate())
			{
				sCull->pushOcclusionGroup(group);
				parent->setOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
			}
		}
	}
}

void LLPipeline::downsampleDepthBuffer(LLRenderTarget& source, LLRenderTarget& dest, LLRenderTarget* scratch_space)
{
	LLGLSLShader* last_shader = LLGLSLShader::sCurBoundShaderPtr;

	LLGLSLShader* shader = NULL;

	if (scratch_space)
	{
		scratch_space->copyContents(source, 
									0, 0, source.getWidth(), source.getHeight(), 
									0, 0, scratch_space->getWidth(), scratch_space->getHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	dest.bindTarget();
	dest.clear(GL_DEPTH_BUFFER_BIT);
	
	shader = &gDownsampleDepthProgram;
	shader->bind();
	shader->uniform2f(sDelta, 1.f/source.getWidth(), 1.f/source.getHeight());

	gGL.getTexUnit(0)->bind(scratch_space ? scratch_space : &source, TRUE);

	{
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
		drawFullScreenRect();
	}
	
	dest.flush();
	
	if (last_shader)
	{
		last_shader->bind();
	}
	else
	{
		shader->unbind();
	}
}

void LLPipeline::doOcclusion(LLCamera& camera, LLRenderTarget& source, LLRenderTarget& dest, LLRenderTarget* scratch_space)
{
	downsampleDepthBuffer(source, dest, scratch_space);
	dest.bindTarget();
	doOcclusion(camera);
	dest.flush();
}

void LLPipeline::doOcclusion(LLCamera& camera)
{
	if (LLGLSLShader::sNoFixedFunction && LLPipeline::sUseOcclusion > 1 && !LLSpatialPartition::sTeleportRequested && sCull->hasOcclusionGroups())
	{
		LLVertexBuffer::unbind();

		if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_OCCLUSION))
		{
			gGL.setColorMask(true, false, false, false);
		}
		else
		{
			gGL.setColorMask(false, false);
		}
		LLGLDisable<GL_BLEND> blend;
		LLGLDisable<GL_ALPHA_TEST> test;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);

		LLGLDisable<GL_CULL_FACE> cull;

		
		bool bind_shader = LLGLSLShader::sNoFixedFunction && LLGLSLShader::sCurBoundShader == 0;
		if (bind_shader)
		{
			if (LLPipeline::sShadowRender)
			{
				gDeferredShadowCubeProgram.bind();
			}
			else
			{
				gOcclusionCubeProgram.bind();
			}
		}

		if (mCubeVB.isNull())
		{ //cube VB will be used for issuing occlusion queries
			mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX, GL_STATIC_DRAW_ARB);
		}
		mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

		for (LLCullResult::sg_iterator iter = sCull->beginOcclusionGroups(); iter != sCull->endOcclusionGroups(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			group->doOcclusion(&camera);
			group->clearOcclusionState(LLSpatialGroup::ACTIVE_OCCLUSION);
		}
	
		if (bind_shader)
		{
			if (LLPipeline::sShadowRender)
			{
				gDeferredShadowCubeProgram.unbind();
			}
			else
			{
				gOcclusionCubeProgram.unbind();
			}
		}

		gGL.setColorMask(true, false);
	}
}
	
BOOL LLPipeline::updateDrawableGeom(LLDrawable* drawablep, BOOL priority)
{
	BOOL update_complete = drawablep->updateGeometry(priority);
	if (update_complete && assertInitialized())
	{
		drawablep->setState(LLDrawable::BUILT);
	}
	return update_complete;
}

static LLTrace::BlockTimerStatHandle FTM_SEED_VBO_POOLS("Seed VBO Pool");

static LLTrace::BlockTimerStatHandle FTM_UPDATE_GL("Update GL");

void LLPipeline::updateGL()
{
	{
		LL_RECORD_BLOCK_TIME(FTM_UPDATE_GL);
		while (!LLGLUpdate::sGLQ.empty())
		{
			LLGLUpdate* glu = LLGLUpdate::sGLQ.front();
			glu->updateGL();
			glu->mInQ = FALSE;
			LLGLUpdate::sGLQ.pop_front();
		}
	}

	{ //seed VBO Pools
		LL_RECORD_BLOCK_TIME(FTM_SEED_VBO_POOLS);
		LLVertexBuffer::seedPools();
	}
}

void LLPipeline::clearRebuildGroups()
{
	LLSpatialGroup::sg_vector_t	hudGroups;

	mGroupQ1Locked = true;
	// Iterate through all drawables on the priority build queue,
	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ1.begin();
		 iter != mGroupQ1.end(); ++iter)
	{
		LLSpatialGroup* group = *iter;

		// If the group contains HUD objects, save the group
		if (group->isHUDGroup())
		{
			hudGroups.push_back(group);
		}
		// Else, no HUD objects so clear the build state
		else
		{
			group->clearState(LLSpatialGroup::IN_BUILD_Q1);
		}
	}

	// Clear the group
	//mGroupQ1.clear();	//Assign already clears...

	// Copy the saved HUD groups back in
	mGroupQ1.assign(hudGroups.begin(), hudGroups.end());
	mGroupQ1Locked = false;

	// Clear the HUD groups
	hudGroups.clear();

	mGroupQ2Locked = true;
	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ2.begin();
		 iter != mGroupQ2.end(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		if (group == nullptr) {
			LL_WARNS() << "Null spatial group in Pipeline::mGroupQ2." << LL_ENDL;
		}

		// If the group contains HUD objects, save the group
		if (group->isHUDGroup())
		{
			hudGroups.push_back(group);
		}
		// Else, no HUD objects so clear the build state
		else
		{
			group->clearState(LLSpatialGroup::IN_BUILD_Q2);
		}
	}	

	// Clear the group
	//mGroupQ2.clear(); //Assign already clears...

	// Copy the saved HUD groups back in
	mGroupQ2.assign(hudGroups.begin(), hudGroups.end());
	mGroupQ2Locked = false;
}

void LLPipeline::clearRebuildDrawables()
{
	// Clear all drawables on the priority build queue,
	for (LLDrawable::drawable_list_t::iterator iter = mBuildQ1.begin();
		 iter != mBuildQ1.end(); ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (drawablep && !drawablep->isDead())
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
			drawablep->clearState(LLDrawable::IN_REBUILD_Q1);
		}
	}
	mBuildQ1.clear();

	// clear drawables on the non-priority build queue
	for (LLDrawable::drawable_list_t::iterator iter = mBuildQ2.begin();
		 iter != mBuildQ2.end(); ++iter)
	{
		LLDrawable* drawablep = *iter;
		if (!drawablep->isDead())
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
		}
	}	
	mBuildQ2.clear();
	
	//clear all moving bridges
	for (LLDrawable::drawable_vector_t::iterator iter = mMovedBridge.begin();
		 iter != mMovedBridge.end(); ++iter)
	{
		LLDrawable *drawablep = *iter;
		drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED | LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD);
	}
	mMovedBridge.clear();

	//clear all moving drawables
	for (LLDrawable::drawable_vector_t::iterator iter = mMovedList.begin();
		 iter != mMovedList.end(); ++iter)
	{
		LLDrawable *drawablep = *iter;
		drawablep->clearState(LLDrawable::EARLY_MOVE | LLDrawable::MOVE_UNDAMPED | LLDrawable::ON_MOVE_LIST | LLDrawable::ANIMATED_CHILD);
	}
	mMovedList.clear();
}

static LLTrace::BlockTimerStatHandle FTM_REBUILD_PRIORITY_GROUPS("Rebuild Priority Groups");

void LLPipeline::rebuildPriorityGroups()
{
	LL_RECORD_BLOCK_TIME(FTM_REBUILD_PRIORITY_GROUPS);
	LLTimer update_timer;

	assertInitialized();

	gMeshRepo.notifyLoadedMeshes();

	mGroupQ1Locked = true;
	// Iterate through all drawables on the priority build queue,
	for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ1.begin();
		 iter != mGroupQ1.end(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		group->rebuildGeom();
		group->clearState(LLSpatialGroup::IN_BUILD_Q1);
	}

	mGroupSaveQ1 = mGroupQ1;
	mGroupQ1.clear();
	mGroupQ1Locked = false;

}

static LLTrace::BlockTimerStatHandle FTM_REBUILD_GROUPS("Rebuild Groups");

void LLPipeline::rebuildGroups()
{
	if (mGroupQ2.empty())
	{
		return;
	}

	LL_RECORD_BLOCK_TIME(FTM_REBUILD_GROUPS);
	mGroupQ2Locked = true;
	// Iterate through some drawables on the non-priority build queue
	S32 size = (S32) mGroupQ2.size();
	S32 min_count = llclamp((S32) ((F32) (size * size)/4096*0.25f), 1, size);
			
	S32 count = 0;
	
	std::sort(mGroupQ2.begin(), mGroupQ2.end(), LLSpatialGroup::CompareUpdateUrgency());

	LLSpatialGroup::sg_vector_t::iterator iter;
	LLSpatialGroup::sg_vector_t::iterator last_iter = mGroupQ2.begin();

	for (iter = mGroupQ2.begin();
		 iter != mGroupQ2.end() && count <= min_count; ++iter)
	{
		LLSpatialGroup* group = *iter;
		last_iter = iter;

		if (!group->isDead())
		{
			group->rebuildGeom();
			
			if (group->getSpatialPartition()->mRenderByGroup)
			{
				count++;
			}
		}

		group->clearState(LLSpatialGroup::IN_BUILD_Q2);
	}	

	mGroupQ2.erase(mGroupQ2.begin(), ++last_iter);

	mGroupQ2Locked = false;

	updateMovedList(mMovedBridge);
}

void LLPipeline::updateGeom(F32 max_dtime, LLCamera& camera)
{
	LLTimer update_timer;
	LLPointer<LLDrawable> drawablep;

	LL_RECORD_BLOCK_TIME(FTM_GEO_UPDATE);

	assertInitialized();

	if (sDelayedVBOEnable > 0)
	{
		if (--sDelayedVBOEnable <= 0)
		{
			resetVertexBuffers();
			LLVertexBuffer::sEnableVBOs = TRUE;
		}
	}

	// notify various object types to reset internal cost metrics, etc.
	// for now, only LLVOVolume does this to throttle LOD changes
	LLVOVolume::preUpdateGeom();

	// Iterate through all drawables on the priority build queue,
	for (LLDrawable::drawable_list_t::iterator iter = mBuildQ1.begin();
		 iter != mBuildQ1.end();)
	{
		LLDrawable::drawable_list_t::iterator curiter = iter++;
		LLDrawable* drawablep = *curiter;
		if (drawablep && !drawablep->isDead())
		{
			if (drawablep->isState(LLDrawable::IN_REBUILD_Q2))
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
				LLDrawable::drawable_list_t::iterator find = std::find(mBuildQ2.begin(), mBuildQ2.end(), drawablep);
				if (find != mBuildQ2.end())
				{
					mBuildQ2.erase(find);
				}
			}

			if (drawablep->isUnload())
			{
				drawablep->unload();
				drawablep->clearState(LLDrawable::FOR_UNLOAD);
			}
			if (updateDrawableGeom(drawablep, TRUE))
			{
				drawablep->clearState(LLDrawable::IN_REBUILD_Q1);
				mBuildQ1.erase(curiter);
			}
		}
		else
		{
			mBuildQ1.erase(curiter);
		}
	}
		
	// Iterate through some drawables on the non-priority build queue
	S32 min_count = 16;
	S32 size = (S32) mBuildQ2.size();
	if (size > 1024)
	{
		min_count = llclamp((S32) (size * (F32) size/4096), 16, size);
	}
		
	S32 count = 0;
	
	max_dtime = llmax(update_timer.getElapsedTimeF32()+0.001f, F32SecondsImplicit(max_dtime));
	LLSpatialGroup* last_group = NULL;
	LLSpatialBridge* last_bridge = NULL;

	for (LLDrawable::drawable_list_t::iterator iter = mBuildQ2.begin();
		 iter != mBuildQ2.end(); )
	{
		LLDrawable::drawable_list_t::iterator curiter = iter++;
		LLDrawable* drawablep = *curiter;

		LLSpatialBridge* bridge = drawablep->isRoot() ? drawablep->getSpatialBridge() :
									drawablep->getParent()->getSpatialBridge();

		if (drawablep->getSpatialGroup() != last_group && 
			(!last_bridge || bridge != last_bridge) &&
			(update_timer.getElapsedTimeF32() >= max_dtime) && count > min_count)
		{
			break;
		}

		//make sure updates don't stop in the middle of a spatial group
		//to avoid thrashing (objects are enqueued by group)
		last_group = drawablep->getSpatialGroup();
		last_bridge = bridge;

		BOOL update_complete = TRUE;
		if (!drawablep->isDead())
		{
			update_complete = updateDrawableGeom(drawablep, FALSE);
			count++;
		}
		if (update_complete)
		{
			drawablep->clearState(LLDrawable::IN_REBUILD_Q2);
			mBuildQ2.erase(curiter);
		}
	}	

	updateMovedList(mMovedBridge);
	calcNearbyLights(camera);
	mLightMode = LIGHT_MODE_NORMAL;
	setupHWLights();
}

void LLPipeline::markVisible(LLDrawable *drawablep, LLCamera& camera)
{
	if(drawablep && !drawablep->isDead())
	{
		if (drawablep->isSpatialBridge())
		{
			const LLDrawable* root = ((LLSpatialBridge*) drawablep)->mDrawable;
			llassert(root); // trying to catch a bad assumption
			if (root && //  // this test may not be needed, see above
					root->getVObj()->isAttachment())
			{
				LLDrawable* rootparent = root->getParent();
				static const LLCachedControl<bool> draw_orphans("ShyotlDrawOrphanAttachments",false);

				if (rootparent) // this IS sometimes NULL
				{
					LLViewerObject *vobj = rootparent->getVObj();
					llassert(vobj); // trying to catch a bad assumption
					if (vobj) // this test may not be needed, see above
					{
						const LLVOAvatar* av = vobj->asAvatar();
						if (av && av->isImpostor() )
						{
							return;
						}
						else if(!draw_orphans && (!av || av->isDead()))
							return;
					}
				}
				else if(!draw_orphans)
					return;
			}
			sCull->pushBridge((LLSpatialBridge*) drawablep);
		}
		else
		{
			sCull->pushDrawable(drawablep);
		}

		drawablep->setVisible(camera);
	}
}

void LLPipeline::markMoved(LLDrawable *drawablep, BOOL damped_motion)
{
	if (!drawablep)
	{
		//LL_ERRS() << "Sending null drawable to moved list!" << LL_ENDL;
		return;
	}
	
	if (drawablep->isDead())
	{
		LL_WARNS() << "Marking NULL or dead drawable moved!" << LL_ENDL;
		return;
	}
	
	if (drawablep->getParent()) 
	{
		//ensure that parent drawables are moved first
		markMoved(drawablep->getParent(), damped_motion);
	}

	assertInitialized();

	if (!drawablep->isState(LLDrawable::ON_MOVE_LIST))
	{
		if (drawablep->isSpatialBridge())
		{
			mMovedBridge.push_back(drawablep);
		}
		else
		{
			mMovedList.push_back(drawablep);
		}
		drawablep->setState(LLDrawable::ON_MOVE_LIST);
	}
	if (damped_motion == FALSE)
	{
		drawablep->setState(LLDrawable::MOVE_UNDAMPED); // UNDAMPED trumps DAMPED
	}
	else if (drawablep->isState(LLDrawable::MOVE_UNDAMPED))
	{
		drawablep->clearState(LLDrawable::MOVE_UNDAMPED);
	}
}

void LLPipeline::markShift(LLDrawable *drawablep)
{
	if (!drawablep || drawablep->isDead())
	{
		return;
	}

	assertInitialized();

	if (!drawablep->isState(LLDrawable::ON_SHIFT_LIST))
	{
		drawablep->getVObj()->setChanged(LLXform::SHIFTED | LLXform::SILHOUETTE);
		if (drawablep->getParent()) 
		{
			markShift(drawablep->getParent());
		}
		mShiftList.push_back(drawablep);
		drawablep->setState(LLDrawable::ON_SHIFT_LIST);
	}
}

static LLTrace::BlockTimerStatHandle FTM_SHIFT_DRAWABLE("Shift Drawable");
static LLTrace::BlockTimerStatHandle FTM_SHIFT_OCTREE("Shift Octree");
static LLTrace::BlockTimerStatHandle FTM_SHIFT_HUD("Shift HUD");

void LLPipeline::shiftObjects(const LLVector3 &offset)
{
	assertInitialized();

	gGL.syncContextState();
	glClear(GL_DEPTH_BUFFER_BIT);
	gDepthDirty = TRUE;
		
	LLVector4a offseta;
	offseta.load3(offset.mV);

	{
		LL_RECORD_BLOCK_TIME(FTM_SHIFT_DRAWABLE);
		for (LLDrawable::drawable_vector_t::iterator iter = mShiftList.begin();
			 iter != mShiftList.end(); iter++)
		{
			LLDrawable *drawablep = *iter;
			if (drawablep->isDead())
			{
				continue;
			}	
			drawablep->shiftPos(offseta);	
			drawablep->clearState(LLDrawable::ON_SHIFT_LIST);
		}
		mShiftList.resize(0);
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_SHIFT_OCTREE);
		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
				iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				if (part)
				{
					part->shift(offseta);
				}
			}
		}
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_SHIFT_HUD);
		LLHUDText::shiftAll(offset);
		LLHUDNameTag::shiftAll(offset);
	}
	display_update_camera();
}

void LLPipeline::markTextured(LLDrawable *drawablep)
{
	if (drawablep && !drawablep->isDead() && assertInitialized())
	{
		mRetexturedList.insert(drawablep);
	}
}

void LLPipeline::markGLRebuild(LLGLUpdate* glu)
{
	if (glu && !glu->mInQ)
	{
		LLGLUpdate::sGLQ.push_back(glu);
		glu->mInQ = TRUE;
	}
}

void LLPipeline::markPartitionMove(LLDrawable* drawable)
{
	if (!drawable->isState(LLDrawable::PARTITION_MOVE) && 
		!drawable->getPositionGroup().equals3(LLVector4a::getZero()))
	{
		drawable->setState(LLDrawable::PARTITION_MOVE);
		mPartitionQ.push_back(drawable);
	}
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_PARTITIONQ("PartitionQ");
void LLPipeline::processPartitionQ()
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_PARTITIONQ);
	for (LLDrawable::drawable_list_t::iterator iter = mPartitionQ.begin(); iter != mPartitionQ.end(); ++iter)
	{
		LLDrawable* drawable = *iter;
		if (!drawable->isDead())
		{
			drawable->updateBinRadius();
			drawable->movePartition();
		}
		drawable->clearState(LLDrawable::PARTITION_MOVE);
	}

	mPartitionQ.clear();
}

void LLPipeline::markMeshDirty(LLSpatialGroup* group)
{
	mMeshDirtyGroup.push_back(group);
}

void LLPipeline::markRebuild(LLSpatialGroup* group, BOOL priority)
{
	if (group && !group->isDead() && group->getSpatialPartition())
	{
		if (group->getSpatialPartition()->mPartitionType == LLViewerRegion::PARTITION_HUD)
		{
			priority = TRUE;
		}

		if (priority)
		{
			if (!group->hasState(LLSpatialGroup::IN_BUILD_Q1))
			{
				llassert_always(!mGroupQ1Locked);

				mGroupQ1.push_back(group);
				group->setState(LLSpatialGroup::IN_BUILD_Q1);

				if (group->hasState(LLSpatialGroup::IN_BUILD_Q2))
				{
					LLSpatialGroup::sg_vector_t::iterator iter = std::find(mGroupQ2.begin(), mGroupQ2.end(), group);
					if (iter != mGroupQ2.end())
					{
						mGroupQ2.erase(iter);
					}
					group->clearState(LLSpatialGroup::IN_BUILD_Q2);
				}
			}
		}
		else if (!group->hasState(LLSpatialGroup::IN_BUILD_Q2 | LLSpatialGroup::IN_BUILD_Q1))
		{
			llassert_always(!mGroupQ2Locked);
			//LL_ERRS() << "Non-priority updates not yet supported!" << LL_ENDL;
			/*if (std::find(mGroupQ2.begin(), mGroupQ2.end(), group) != mGroupQ2.end())
			{
				LL_ERRS() << "WTF?" << LL_ENDL;
			}*/
			mGroupQ2.push_back(group);
			group->setState(LLSpatialGroup::IN_BUILD_Q2);

		}
	}
}

void LLPipeline::markRebuild(LLDrawable *drawablep, LLDrawable::EDrawableFlags flag, BOOL priority)
{
	if (drawablep && !drawablep->isDead() && assertInitialized())
	{
		if (!drawablep->isState(LLDrawable::BUILT))
		{
			priority = TRUE;
		}
		if (priority)
		{
			if (!drawablep->isState(LLDrawable::IN_REBUILD_Q1))
			{
				mBuildQ1.push_back(drawablep);
				drawablep->setState(LLDrawable::IN_REBUILD_Q1); // mark drawable as being in priority queue
			}
		}
		else if (!drawablep->isState(LLDrawable::IN_REBUILD_Q2))
		{
			mBuildQ2.push_back(drawablep);
			drawablep->setState(LLDrawable::IN_REBUILD_Q2); // need flag here because it is just a list
		}
		if (flag & (LLDrawable::REBUILD_VOLUME | LLDrawable::REBUILD_POSITION))
		{
			drawablep->getVObj()->setChanged(LLXform::SILHOUETTE);
		}
		drawablep->setState(flag);
	}
}

static LLTrace::BlockTimerStatHandle FTM_RESET_DRAWORDER("Reset Draw Order");

void LLPipeline::stateSort(LLCamera& camera, LLCullResult &result)
{
	if (hasAnyRenderType(LLPipeline::RENDER_TYPE_AVATAR,
					  LLPipeline::RENDER_TYPE_GROUND,
					  LLPipeline::RENDER_TYPE_TERRAIN,
					  LLPipeline::RENDER_TYPE_TREE,
					  LLPipeline::RENDER_TYPE_SKY,
					  LLPipeline::RENDER_TYPE_VOIDWATER,
					  LLPipeline::RENDER_TYPE_WATER,
					  LLPipeline::END_RENDER_TYPES))
	{
		//clear faces from face pools
		LL_RECORD_BLOCK_TIME(FTM_RESET_DRAWORDER);
		gPipeline.resetDrawOrders();
	}

	LL_RECORD_BLOCK_TIME(FTM_STATESORT);

	//LLVertexBuffer::unbind();

	grabReferences(result);
	for (LLCullResult::sg_iterator iter = sCull->beginDrawableGroups(); iter != sCull->endDrawableGroups(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		group->checkOcclusion();
		if (sUseOcclusion > 1 && group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(group);
		}
		else
		{
			group->setVisible();
			OctreeGuard guard(group->getOctreeNode());
			for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
			{
				markVisible((LLDrawable*)(*i)->getDrawable(), camera);
			}

			if (!sDelayVBUpdate)
			{ //rebuild mesh as soon as we know it's visible
				group->rebuildMesh();
			}
		}
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		LLSpatialGroup* last_group = NULL;
		for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge(); i != sCull->endVisibleBridge(); ++i)
		{
			LLCullResult::bridge_iterator cur_iter = i;
			LLSpatialBridge* bridge = *cur_iter;
			LLSpatialGroup* group = bridge->getSpatialGroup();

			if (last_group == NULL)
			{
				last_group = group;
			}

			if (!bridge->isDead() && group && !group->isOcclusionState(LLSpatialGroup::OCCLUDED))
			{
				stateSort(bridge, camera);
			}

			if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
				last_group != group && last_group->changeLOD())
			{
				last_group->mLastUpdateDistance = last_group->mDistance;
			}

			last_group = group;
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD &&
			last_group && last_group->changeLOD())
		{
			last_group->mLastUpdateDistance = last_group->mDistance;
		}
	}

	for (LLCullResult::sg_iterator iter = sCull->beginVisibleGroups(); iter != sCull->endVisibleGroups(); ++iter)
	{
		LLSpatialGroup* group = *iter;
		group->checkOcclusion();
		if (sUseOcclusion > 1 && group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			markOccluder(group);
		}
		else
		{
			group->setVisible();
			stateSort(group, camera);

			if (!sDelayVBUpdate)
			{ //rebuild mesh as soon as we know it's visible
				group->rebuildMesh();
			}
		}
	}
	
	{
		LL_RECORD_BLOCK_TIME(FTM_STATESORT_DRAWABLE);
		for (LLCullResult::drawable_iterator iter = sCull->beginVisibleList();
			 iter != sCull->endVisibleList(); ++iter)
		{
			LLDrawable *drawablep = *iter;
			if (!drawablep->isDead())
			{
				stateSort(drawablep, camera);
			}
		}
	}
		
	postSort(camera);	
}

void LLPipeline::stateSort(LLSpatialGroup* group, LLCamera& camera)
{
	if (!sSkipUpdate && group->changeLOD())
	{
		OctreeGuard guard(group->getOctreeNode());
		for (LLSpatialGroup::element_iter i = group->getDataBegin(); i != group->getDataEnd(); ++i)
		{
			LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
			stateSort(drawablep, camera);
		}

		if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
		{ //avoid redundant stateSort calls
			group->mLastUpdateDistance = group->mDistance;
		}
	}

}

void LLPipeline::stateSort(LLSpatialBridge* bridge, LLCamera& camera)
{
	if (/*!sShadowRender && */!sSkipUpdate && bridge->getSpatialGroup()->changeLOD())
	{
		bool force_update = false;
		bridge->updateDistance(camera, force_update);
	}
}

void LLPipeline::stateSort(LLDrawable* drawablep, LLCamera& camera)
{
	if (!drawablep
		|| drawablep->isDead() 
		|| !hasRenderType(drawablep->getRenderType()))
	{
		return;
	}
	
	if (LLSelectMgr::getInstance()->mHideSelectedObjects)
	{
//		if (drawablep->getVObj().notNull() &&
//			drawablep->getVObj()->isSelected())
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.2.1f
		const LLViewerObject* pObj = drawablep->getVObj();
		if ( (pObj) && (pObj->isSelected()) && 
			 ( (!rlv_handler_t::isEnabled()) || 
			   ( ((!pObj->isHUDAttachment()) || (!gRlvAttachmentLocks.isLockedAttachment(pObj->getRootEdit()))) && 
			     (gRlvHandler.canEdit(pObj)) ) ) )
// [/RLVa:KB]
		{
			return;
		}
	}

	if (drawablep->isAvatar())
	{ //don't draw avatars beyond render distance or if we don't have a spatial group.
		if ((drawablep->getSpatialGroup() == NULL) || 
			(drawablep->getSpatialGroup()->mDistance > LLVOAvatar::sRenderDistance))
		{
			return;
		}

		LLVOAvatar* avatarp = (LLVOAvatar*) drawablep->getVObj().get();
		if (!avatarp->isVisible())
		{
			return;
		}
	}

	assertInitialized();

	if (hasRenderType(drawablep->mRenderType))
	{
		if (!drawablep->isState(LLDrawable::INVISIBLE|LLDrawable::FORCE_INVISIBLE))
		{
			drawablep->setVisible(camera, NULL, FALSE);
		}
	}

	if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
	{
		//llassert_always(drawablep->isVisible());
		/*LLSpatialGroup* group = drawablep->getSpatialGroup();
		if (!group || group->changeLOD())
		{
			if (drawablep->isVisible() && !sSkipUpdate)*/
			if(!sSkipUpdate)
			{
				if (!drawablep->isActive())
				{
					bool force_update = false;
					drawablep->updateDistance(camera, force_update);
				}
				else if (drawablep->isAvatar())
				{
					bool force_update = false;
					drawablep->updateDistance(camera, force_update); // calls vobj->updateLOD() which calls LLVOAvatar::updateVisibility()
				}
			}
		//}
	}

	if(!drawablep->getVOVolume())
	{
		for (LLDrawable::face_list_t::iterator iter = drawablep->mFaces.begin();
				iter != drawablep->mFaces.end(); iter++)
		{
			LLFace* facep = *iter;

			if (facep->hasGeometry())
			{
				if (facep->getPool())
				{
					facep->getPool()->enqueue(facep);
				}
				else
				{
					break;
				}
			}
		}
	}
}


void forAllDrawables(LLCullResult::sg_iterator begin, 
					 LLCullResult::sg_iterator end,
					 void (*func)(LLDrawable*))
{
	for (LLCullResult::sg_iterator i = begin; i != end; ++i)
	{
		LLSpatialGroup* group = (*i).get();
		OctreeGuard guard(group->getOctreeNode());
		for (LLSpatialGroup::element_iter j = group->getDataBegin(); j != group->getDataEnd(); ++j)
		{
			func((LLDrawable*)(*j)->getDrawable());	
		}
	}
}

void LLPipeline::forAllVisibleDrawables(void (*func)(LLDrawable*))
{
	forAllDrawables(sCull->beginDrawableGroups(), sCull->endDrawableGroups(), func);
	forAllDrawables(sCull->beginVisibleGroups(), sCull->endVisibleGroups(), func);
}

//function for creating scripted beacons
void renderScriptedBeacons(LLDrawable* drawablep)
{
	LLViewerObject *vobj = drawablep->getVObj();
	if (vobj 
		&& !vobj->isAvatar() 
		&& !vobj->getParent()
		&& vobj->flagScripted())
	{
		if (gPipeline.sRenderBeacons)
		{
			static const LLCachedControl<S32> debug_beacon_line_width("DebugBeaconLineWidth",1);
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 0.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), debug_beacon_line_width);
		}

		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep) 
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderScriptedTouchBeacons(LLDrawable* drawablep)
{
	LLViewerObject *vobj = drawablep->getVObj();
	if (vobj 
		&& !vobj->isAvatar() 
		&& !vobj->getParent()
		&& vobj->flagScripted()
		&& vobj->flagHandleTouch())
	{
		if (gPipeline.sRenderBeacons)
		{
			static const LLCachedControl<S32> debug_beacon_line_width("DebugBeaconLineWidth",1);
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 0.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), debug_beacon_line_width);
		}

		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderPhysicalBeacons(LLDrawable* drawablep)
{
	LLViewerObject *vobj = drawablep->getVObj();
	if (vobj 
		&& !vobj->isAvatar() 
		//&& !vobj->getParent()
		&& vobj->flagUsePhysics())
	{
		if (gPipeline.sRenderBeacons)
		{
			static const LLCachedControl<S32> DebugBeaconLineWidth("DebugBeaconLineWidth",1);
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(0.f, 1.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), DebugBeaconLineWidth);
		}

		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderMOAPBeacons(LLDrawable* drawablep)
{
	LLViewerObject *vobj = drawablep->getVObj();

	if(!vobj || vobj->isAvatar())
		return;

	BOOL beacon=FALSE;
	U8 tecount=vobj->getNumTEs();
	for(int x=0;x<tecount;x++)
	{
		if(vobj->getTE(x)->hasMedia())
		{
			beacon=TRUE;
			break;
		}
	}
	if(beacon==TRUE)
	{
		if (gPipeline.sRenderBeacons)
		{
			static const LLCachedControl<S32> DebugBeaconLineWidth("DebugBeaconLineWidth",1);
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", LLColor4(1.f, 1.f, 1.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), DebugBeaconLineWidth);
		}

		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
			}
		}
	}
}
}

void renderParticleBeacons(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject *vobj = drawablep->getVObj();
	if (vobj 
		&& vobj->isParticleSource())
	{
		if (gPipeline.sRenderBeacons)
		{
			LLColor4 light_blue(0.5f, 0.5f, 1.f, 0.5f);
			static const LLCachedControl<S32> DebugBeaconLineWidth("DebugBeaconLineWidth",1);
			gObjectList.addDebugBeacon(vobj->getPositionAgent(), "", light_blue, LLColor4(1.f, 1.f, 1.f, 0.5f), DebugBeaconLineWidth);
		}

		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void renderSoundHighlights(LLDrawable* drawablep)
{
	// Look for attachments, objects, etc.
	LLViewerObject *vobj = drawablep->getVObj();
	if (vobj && vobj->isAudioSource())
	{
		if (gPipeline.sRenderHighlight)
		{
			S32 face_id;
			S32 count = drawablep->getNumFaces();
			for (face_id = 0; face_id < count; face_id++)
			{
				LLFace * facep = drawablep->getFace(face_id);
				if (facep)
				{
					gPipeline.mHighlightFaces.push_back(facep);
				}
			}
		}
	}
}

void updateParticleActivity(LLDrawable *drawablep);

void LLPipeline::postSort(LLCamera& camera)
{
	LL_RECORD_BLOCK_TIME(FTM_STATESORT_POSTSORT);

	assertInitialized();

	LL_PUSH_CALLSTACKS();
	//rebuild drawable geometry
	for (LLCullResult::sg_iterator i = sCull->beginDrawableGroups(); i != sCull->endDrawableGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (!sUseOcclusion || 
			!group->isOcclusionState(LLSpatialGroup::OCCLUDED))
		{
			group->rebuildGeom();
		}
	}
	LL_PUSH_CALLSTACKS();
	//rebuild groups
	sCull->assertDrawMapsEmpty();

	rebuildPriorityGroups();
	LL_PUSH_CALLSTACKS();


	//build render map
	for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups(); i != sCull->endVisibleGroups(); ++i)
	{
		LLSpatialGroup* group = *i;

		static LLCachedControl<F32> RenderAutoHideSurfaceAreaLimit("RenderAutoHideSurfaceAreaLimit", 0.f);
		if (sUseOcclusion && 
			group->isOcclusionState(LLSpatialGroup::OCCLUDED) ||
			(RenderAutoHideSurfaceAreaLimit > 0.f && 
			group->mSurfaceArea > RenderAutoHideSurfaceAreaLimit*llmax(group->mObjectBoxSize, 10.f)))
		{
			continue;
		}

		if (group->hasState(LLSpatialGroup::NEW_DRAWINFO) && group->hasState(LLSpatialGroup::GEOM_DIRTY))
		{ //no way this group is going to be drawable without a rebuild
			group->rebuildGeom();
		}

		for (LLSpatialGroup::draw_map_t::iterator j = group->mDrawMap.begin(); j != group->mDrawMap.end(); ++j)
		{
			LLSpatialGroup::drawmap_elem_t& src_vec = j->second;	
			if (!hasRenderType(j->first))
			{
				continue;
			}
			
			for (LLSpatialGroup::drawmap_elem_t::iterator k = src_vec.begin(); k != src_vec.end(); ++k)
			{
				if (sMinRenderSize > 0.f)
				{
					LLVector4a bounds;
					bounds.setSub((*k)->mExtents[1],(*k)->mExtents[0]);

					if (llmax(llmax(bounds[0], bounds[1]), bounds[2]) > sMinRenderSize)
					{
						sCull->pushDrawInfo(j->first, *k);
					}
				}
				else
				{
					sCull->pushDrawInfo(j->first, *k);
				}
			}
		}

		if (hasRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA))
		{
		LLSpatialGroup::draw_map_t::iterator alpha = group->mDrawMap.find(LLRenderPass::PASS_ALPHA);
		
			if (alpha != group->mDrawMap.end())
			{ //store alpha groups for sorting
				LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();
				if (LLViewerCamera::sCurCameraID == LLViewerCamera::CAMERA_WORLD)
				{
					if (bridge)
					{
						LLCamera trans_camera = bridge->transformCamera(camera);
						group->updateDistance(trans_camera);
					}
					else
					{
						group->updateDistance(camera);
					}
				}
		
				if (hasRenderType(LLDrawPool::POOL_ALPHA))
				{
					sCull->pushAlphaGroup(group);
				}
			}
		}
	}
	
	//flush particle VB
	LLVOPartGroup::sVB->flush();

	/*bool use_transform_feedback = gTransformPositionProgram.mProgramObject && !mMeshDirtyGroup.empty();

	if (use_transform_feedback)
	{ //place a query around potential transform feedback code for synchronization
		mTransformFeedbackPrimitives = 0;

		if (!mMeshDirtyQueryObject)
		{
			glGenQueriesARB(1, &mMeshDirtyQueryObject);
		}

		
		glBeginQueryARB(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, mMeshDirtyQueryObject);
	}*/

	//pack vertex buffers for groups that chose to delay their updates
	for (LLSpatialGroup::sg_vector_t::iterator iter = mMeshDirtyGroup.begin(); iter != mMeshDirtyGroup.end(); ++iter)
	{
		(*iter)->rebuildMesh();
	}

	/*if (use_transform_feedback)
	}*/
	
	mMeshDirtyGroup.clear();

	if (!sShadowRender)
	{
		std::sort(sCull->beginAlphaGroups(), sCull->endAlphaGroups(), LLSpatialGroup::CompareDepthGreater());
	}

	LL_PUSH_CALLSTACKS();

	forAllVisibleDrawables(updateParticleActivity);	//for llfloateravatarlist

	// only render if the flag is set. The flag is only set if we are in edit mode or the toggle is set in the menus
	static const LLCachedControl<bool> beacons_visible("BeaconsVisible", false);
	if (beacons_visible && !sShadowRender)
	{
		if (sRenderScriptedTouchBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedTouchBeacons);
		}
		else
		if (sRenderScriptedBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderScriptedBeacons);
		}

		if (sRenderPhysicalBeacons)
		{
			// Only show the beacon on the root object.
			forAllVisibleDrawables(renderPhysicalBeacons);
		}

		if(sRenderMOAPBeacons)
		{
			forAllVisibleDrawables(renderMOAPBeacons);
		}

		if (sRenderParticleBeacons)
		{
			forAllVisibleDrawables(renderParticleBeacons);
		}

		// If god mode, also show audio cues
		if (sRenderSoundBeacons && gAudiop)
		{
			// Walk all sound sources and render out beacons for them. Note, this isn't done in the ForAllVisibleDrawables function, because some are not visible.
			LLAudioEngine::source_map::iterator iter;
			for (iter = gAudiop->mAllSources.begin(); iter != gAudiop->mAllSources.end(); ++iter)
			{
				LLAudioSource *sourcep = iter->second;

				LLVector3d pos_global = sourcep->getPositionGlobal();
				LLVector3 pos = gAgent.getPosAgentFromGlobal(pos_global);
				if (gPipeline.sRenderBeacons)
				{
					//<NewShinyStuff>
					LLAudioChannel* channel = sourcep->getChannel();
					bool const is_playing = channel && channel->isPlaying();
					S32 width = 2;
					LLColor4 color = LLColor4(0.f, 0.f, 1.f, 0.5f);				// Blue: Not playing and not muted.
					if (is_playing)
					{
						static const LLCachedControl<S32> debug_beacon_line_width("DebugBeaconLineWidth",1);
					  	llassert(!sourcep->isMuted());
						F32 gain = sourcep->getGain() * channel->getSecondaryGain();
						if (gain == 0.f)
						{
						  color = LLColor4(1.f, 0.f, 0.f, 0.5f);				// Red: Playing with gain == 0. This sucks up CPU, these should be muted.
						}
						else if (gain == 1.f)
						{
						  color = LLColor4(0.f, 1.f, 0.f, 0.5f);				// Green: Playing with gain == 1.
						  width = debug_beacon_line_width;
						}
						else
						{
						  color = LLColor4(1.f, 1.f, 0.f, 0.5f);				// Yellow: Playing with 0 < gain < 1.
						  width = 1 + gain * (debug_beacon_line_width - 1);
						}
					}
					else if (sourcep->isMuted())
						color = LLColor4(0.f, 1.f, 1.f, 0.5f);					// Cyan: Muted sound source.
					gObjectList.addDebugBeacon(pos, "", color, LLColor4(1.f, 1.f, 1.f, 0.5f), width);
					//</NewShinyStuff>
					//gObjectList.addDebugBeacon(pos, "", LLColor4(1.f, 1.f, 0.f, 0.5f), LLColor4(1.f, 1.f, 1.f, 0.5f), debug_beacon_line_width);
				}
			}
			// now deal with highlights for all those seeable sound sources
			forAllVisibleDrawables(renderSoundHighlights);
		}
	}
	LL_PUSH_CALLSTACKS();
	// If managing your telehub, draw beacons at telehub and currently selected spawnpoint.
	if (LLFloaterTelehub::renderBeacons())
	{
		LLFloaterTelehub::addBeacons();
	}

	if (!sShadowRender)
	{
		mSelectedFaces.clear();
		
		LLPipeline::setRenderHighlightTextureChannel(gFloaterTools->getPanelFace()->getTextureChannelToEdit());

		// Draw face highlights for selected faces.
		if (LLSelectMgr::getInstance()->getTEMode())
		{
			struct f : public LLSelectedTEFunctor
			{
				virtual bool apply(LLViewerObject* object, S32 te)
				{
					if (object->mDrawable)
					{
						LLFace * facep = object->mDrawable->getFace(te);
						if (facep)
						{
							gPipeline.mSelectedFaces.push_back(facep);
						}
					}
					return true;
				}
			} func;
			LLSelectMgr::getInstance()->getSelection()->applyToTEs(&func);
		}
	}

	//LLSpatialGroup::sNoDelete = FALSE;
	LL_PUSH_CALLSTACKS();
}


void render_hud_elements()
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_UI);

	LLGLDisable<GL_FOG> fog;
	LLGLSUIDefault gls_ui;

	LLGLEnable<GL_STENCIL_TEST> stencil;
	glStencilFunc(GL_ALWAYS, 255, 0xFFFFFFFF);
	glStencilMask(0xFFFFFFFF);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	
	gGL.color4f(1,1,1,1);

	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	if (!LLPipeline::sReflectionRender && gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
	{
		static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
		LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);
		gViewerWindow->renderSelections(FALSE, FALSE, FALSE); // For HUD version in render_ui_3d()
	
		// Draw the tracking overlays
		LLTracker::render3D();
		
		// Show the property lines
		LLWorld::getInstance()->renderPropertyLines();
		LLViewerParcelMgr::getInstance()->render();
		LLViewerParcelMgr::getInstance()->renderParcelCollision();
	
		// Render name tags.
		LLHUDObject::renderAll();
	}
	else if (gForceRenderLandFence)
	{
		// This is only set when not rendering the UI, for parcel snapshots
		LLViewerParcelMgr::getInstance()->render();
	}
	else if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{
		LLHUDText::renderAllHUD();
	}

	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.unbind();
	}
	gGL.flush();
}

// Singu Note: Created to avoid redundant code.
void renderSelectedFaces(LLGLSLShader& shader, std::vector<LLFace*> &selected_faces, LLViewerTexture* tex, LLColor4 color, LLRender::eTexIndex channel, LLRender::eTexIndex active_channel = LLRender::NUM_TEXTURE_CHANNELS)
{
	if ((LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0))
	{
		shader.bind();
	}

	bool active = active_channel == LLRender::NUM_TEXTURE_CHANNELS || channel == active_channel;
	
	if(!active)	//Draw 'faded out' overlay for selected faces that aren't applicable to current channel being edited.
		color.mV[3] *= .5f;

	for (std::vector<LLFace*>::iterator it = selected_faces.begin(); it != selected_faces.end(); ++it)
	{
		LLFace *facep = *it;
		if (!facep || facep->getDrawable()->isDead())
		{
			LL_ERRS() << "Bad face on selection" << LL_ENDL;
			return;
		}

		const LLTextureEntry* te = facep->getTextureEntry();
		LLMaterial* mat = te ? te->getMaterialParams().get() : NULL;

		if(channel == LLRender::DIFFUSE_MAP)
		{
			if(active || !mat || 
				(active_channel == LLRender::NORMAL_MAP && mat->getNormalID().isNull()) || 
				(active_channel == LLRender::SPECULAR_MAP && mat->getSpecularID().isNull()))
				facep->renderSelected(tex, color);
		}
		else if(channel == LLRender::NORMAL_MAP)
		{
			if(mat && mat->getNormalID().notNull())
				facep->renderSelected(tex, color);
		}
		else if(channel == LLRender::SPECULAR_MAP)
		{
			if(mat && mat->getSpecularID().notNull())
				facep->renderSelected(tex, color);
		}
	}

	if ((LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_INTERFACE) > 0))
	{
		shader.unbind();
	}
}
void LLPipeline::renderHighlights()
{
	assertInitialized();

	if(!hasRenderDebugFeatureMask(RENDER_DEBUG_FEATURE_SELECTED))
		return;	//Nothing to draw...

	// Setup
	if ( !mFaceSelectImagep)
	{
		mFaceSelectImagep = LLViewerTextureManager::getFetchedTexture(IMG_FACE_SELECT);
	}

	// Make sure the selection image gets downloaded and decoded
	mFaceSelectImagep->addTextureStats((F32)MAX_IMAGE_AREA);

	// Draw 3D UI elements here (before we clear the Z buffer in POOL_HUD)
	// Render highlighted faces.
	LLGLSPipelineAlpha gls_pipeline_alpha;
	LLGLEnable<GL_COLOR_MATERIAL> color_mat;
	LLGLState<GL_LIGHTING> light_state;
	gPipeline.disableLights(light_state);

	// Singu Note: Logic here changed, and behavior changed as well. Always draw overlays of some nature over all selected faces.
	//  Faces that wont undergo any change if the current active channel is edited should have a 'faded' overlay

	// Temporary. If not deferred, then texcoord1 and texcoord2 are probably absent from face vbo, which LLFace::renderSelected piggybacks.
	//  Force to standard diffuse mode. Workaround would probably be to generate new face vbo data on the fly if required texcoord data is absent.
	LLRender::eTexIndex active_channel = LLPipeline::sRenderDeferred ? sRenderHighlightTextureChannel : LLRender::NUM_TEXTURE_CHANNELS;

	// Default diffuse mapping
	renderSelectedFaces(gHighlightProgram, mSelectedFaces, mFaceSelectImagep, LLColor4(1.f,1.f,1.f,.5f), LLRender::DIFFUSE_MAP, active_channel);
	
	// Paint 'em red!
	renderSelectedFaces(gHighlightProgram, mHighlightFaces, LLViewerTexture::sNullImagep, LLColor4(1.f,0.f,0.f,.5f), LLRender::DIFFUSE_MAP);

	// Normal mapping
	if(active_channel == LLRender::NORMAL_MAP)
		renderSelectedFaces(gHighlightNormalProgram, mSelectedFaces, mFaceSelectImagep, LLColor4(1.f, .5f, .5f, .5f), LLRender::NORMAL_MAP);

	// Specular mapping
	if(active_channel == LLRender::SPECULAR_MAP)
		renderSelectedFaces(gHighlightSpecularProgram, mSelectedFaces, mFaceSelectImagep, LLColor4(0.f, .3f, 1.f, .8f), LLRender::SPECULAR_MAP);

	mHighlightFaces.clear();
}

//debug use
U32 LLPipeline::sCurRenderPoolType = 0 ;

extern void check_blend_funcs();
void LLPipeline::renderGeom(LLCamera& camera, BOOL forceVBOUpdate)
{
	LL_RECORD_BLOCK_TIME(FTM_RENDER_GEOMETRY);

	assertInitialized();

	LLMatrix4a saved_modelview;
	LLMatrix4a saved_projection;

	//HACK: preserve/restore matrices around HUD render
	if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{
		saved_modelview = glh_get_current_modelview();
		saved_projection = glh_get_current_projection();
	}

	///////////////////////////////////////////
	//
	// Sync and verify GL state
	//
	//

	stop_glerror();
	gFrameStats.start(LLFrameStats::RENDER_SYNC);

	LLVertexBuffer::unbind();

	// Do verification of GL state
	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();
	LLGLStateValidator::checkClientArrays();
	if (mRenderDebugMask & RENDER_DEBUG_VERIFY)
	{
		if (!verify())
		{
			LL_ERRS() << "Pipeline verification failed!" << LL_ENDL;
		}
	}

	LLAppViewer::instance()->pingMainloopTimeout("Pipeline:ForceVBO");
	
	gFrameStats.start(LLFrameStats::RENDER_GEOM);

	// Initialize lots of GL state to "safe" values
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.matrixMode(LLRender::MM_TEXTURE);
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	LLGLSPipeline gls_pipeline;
	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);

	LLGLEnable<GL_COLOR_MATERIAL> gls_color_material;
				
	// Toggle backface culling for debugging
	LLGLEnable<GL_CULL_FACE> cull_face(mBackfaceCull);

	// Set fog only if not using shaders and is underwater render.
	BOOL use_fog = hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_FOG);
	LLGLEnable<GL_FOG> fog_enable(use_fog && sUnderWaterRender && !LLGLSLShader::sNoFixedFunction);
	gSky.updateFog(camera.getFar());

	gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sDefaultImagep);
	LLViewerFetchedTexture::sDefaultImagep->setAddressMode(LLTexUnit::TAM_WRAP);
	
	//////////////////////////////////////////////
	//
	// Actually render all of the geometry
	//
	//	
	stop_glerror();
	
	LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderDrawPools");

	for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
	{
		LLDrawPool *poolp = *iter;
		if (hasRenderType(poolp->getType()))
		{
			poolp->prerender();
		}
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_POOLS);
		
		// HACK: don't calculate local lights if we're rendering the HUD!
		//    Removing this check will cause bad flickering when there are 
		//    HUD elements being rendered AND the user is in flycam mode  -nyx
		if (!gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			updateHWLightMode(LIGHT_MODE_NORMAL);
			llassert(LLGLState<GL_LIGHTING>::checkEnabled());
		}

		BOOL occlude = sUseOcclusion > 1;
		U32 cur_type = 0;

		pool_set_t::iterator iter1 = mPools.begin();
		while ( iter1 != mPools.end() )
		{
			LLDrawPool *poolp = *iter1;
			
			cur_type = poolp->getType();

			//debug use
			sCurRenderPoolType = cur_type ;

			if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
			{
				occlude = FALSE;
				gGLLastMatrix = NULL;
				gGL.loadMatrix(glh_get_current_modelview());
				LLGLSLShader::bindNoShader();
				doOcclusion(camera);
			}

			pool_set_t::iterator iter2 = iter1;
			if (hasRenderType(poolp->getType()) && poolp->getNumPasses() > 0)
			{
				LL_RECORD_BLOCK_TIME(FTM_POOLRENDER);

				gGLLastMatrix = NULL;
				gGL.loadMatrix(glh_get_current_modelview());
			
				for( S32 i = 0; i < poolp->getNumPasses(); i++ )
				{
					LLVertexBuffer::unbind();
					if(gDebugGL)check_blend_funcs();
					mInRenderPass = true;
					poolp->beginRenderPass(i);
					for (iter2 = iter1; iter2 != mPools.end(); iter2++)
					{
						LLDrawPool *p = *iter2;
						if (p->getType() != cur_type)
						{
							break;
						}
						p->render(i);
					}
					poolp->endRenderPass(i);
					clearRenderPassStates();
					if(gDebugGL)check_blend_funcs();
					LLVertexBuffer::unbind();
					if (gDebugGL)
					{
						std::string msg = llformat("%s pass %d", gPoolNames[cur_type].c_str(), i);
						LLGLStateValidator::checkStates(msg);
						//LLGLStateValidator::checkTextureChannels(msg);
						//LLGLStateValidator::checkClientArrays(msg);
					}
				}
			}
			else
			{
				// Skip all pools of this type
				for (iter2 = iter1; iter2 != mPools.end(); iter2++)
				{
					LLDrawPool *p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
				}
			}
			iter1 = iter2;
			stop_glerror();
		}
	
	LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderDrawPoolsEnd");

	LLVertexBuffer::unbind();
		
		gGLLastMatrix = NULL;
		gGL.loadMatrix(glh_get_current_modelview());

		if (occlude)
		{
			occlude = FALSE;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(glh_get_current_modelview());
			LLGLSLShader::bindNoShader();
			doOcclusion(camera);
		}
	}

	LLVertexBuffer::unbind();
	LLGLStateValidator::checkStates();
	//LLGLStateValidator::checkTextureChannels();
	//LLGLStateValidator::checkClientArrays();

	

	//stop_glerror();
		
	//LLGLStateValidator::checkStates();
	//LLGLStateValidator::checkTextureChannels();
	//LLGLStateValidator::checkClientArrays();
	if (!LLPipeline::sImpostorRender)
	{
	
		LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderHighlights");

		if (!sReflectionRender)
		{
			renderHighlights();
		}

		// Contains a list of the faces of objects that are physical or
		// have touch-handlers.
		mHighlightFaces.clear();

		LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderDebug");
	
		renderDebug();

		LLVertexBuffer::unbind();
	
		if (!LLPipeline::sReflectionRender && !LLPipeline::sRenderDeferred)
		{
			if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
			{
				// Render debugging beacons.
				gObjectList.renderObjectBeacons();
				gObjectList.resetObjectBeacons();
			}
			else
			{
				// Make sure particle effects disappear
				LLHUDObject::renderAllForTimer();
			}
		}
		else
		{
			// Make sure particle effects disappear
			LLHUDObject::renderAllForTimer();
		}

		LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderGeomEnd");

		//HACK: preserve/restore matrices around HUD render
		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			glh_set_current_modelview(saved_modelview);
			glh_set_current_projection(saved_projection);
		}
	}

	LLVertexBuffer::unbind();

	LLGLStateValidator::checkStates();
	//LLGLStateValidator::checkTextureChannels();
	//LLGLStateValidator::checkClientArrays();
}

void LLPipeline::renderGeomDeferred(LLCamera& camera)
{
	LLAppViewer::instance()->pingMainloopTimeout("Pipeline:RenderGeomDeferred");
	LL_RECORD_BLOCK_TIME(FTM_RENDER_GEOMETRY);

	LL_RECORD_BLOCK_TIME(FTM_DEFERRED_POOLS);

	LLGLEnable<GL_CULL_FACE> cull;

	LLGLEnable<GL_STENCIL_TEST> stencil;
	glStencilFunc(GL_ALWAYS, 1, 0xFFFFFFFF);
	stop_glerror();
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	stop_glerror();

	for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
	{
		LLDrawPool *poolp = *iter;
		if (hasRenderType(poolp->getType()))
		{
			poolp->prerender();
		}
	}

	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);

	LLVertexBuffer::unbind();

	//LLGLStateValidator::checkStates();
	//LLGLStateValidator::checkTextureChannels();
	//LLGLStateValidator::checkClientArrays();

	U32 cur_type = 0;

	gGL.setColorMask(true, true);
	
	pool_set_t::iterator iter1 = mPools.begin();

	while ( iter1 != mPools.end() )
	{
		LLDrawPool *poolp = *iter1;
		
		cur_type = poolp->getType();

		pool_set_t::iterator iter2 = iter1;
		if (hasRenderType(poolp->getType()) && poolp->getNumDeferredPasses() > 0)
		{
			LL_RECORD_BLOCK_TIME(FTM_DEFERRED_POOLRENDER);

			gGLLastMatrix = NULL;
			gGL.loadMatrix(glh_get_current_modelview());
		
			for( S32 i = 0; i < poolp->getNumDeferredPasses(); i++ )
			{
				LLVertexBuffer::unbind();
				mInRenderPass = true;
				poolp->beginDeferredPass(i);
				for (iter2 = iter1; iter2 != mPools.end(); iter2++)
				{
					LLDrawPool *p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
										
					p->renderDeferred(i);
				}
				poolp->endDeferredPass(i);
				clearRenderPassStates();
				LLVertexBuffer::unbind();

				if (gDebugGL || gDebugPipeline)
				{
					LLGLStateValidator::checkStates();
					//LLGLStateValidator::checkTextureChannels();
					//LLGLStateValidator::checkClientArrays();
				}
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != mPools.end(); iter2++)
			{
				LLDrawPool *p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
		stop_glerror();
	}

	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_current_modelview());

	gGL.setColorMask(true, false);
}

void LLPipeline::renderGeomPostDeferred(LLCamera& camera, bool do_occlusion)
{
	LL_RECORD_BLOCK_TIME(FTM_POST_DEFERRED_POOLS);
	U32 cur_type = 0;

	LLGLEnable<GL_CULL_FACE> cull;

	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);

	updateHWLightMode(LIGHT_MODE_NORMAL);

	gGL.setColorMask(true, false);

	pool_set_t::iterator iter1 = mPools.begin();
	BOOL occlude = LLPipeline::sUseOcclusion > 1 && do_occlusion;

	while ( iter1 != mPools.end() )
	{
		LLDrawPool *poolp = *iter1;
		
		cur_type = poolp->getType();

		if (occlude && cur_type >= LLDrawPool::POOL_GRASS)
		{
			occlude = FALSE;
			gGLLastMatrix = NULL;
			gGL.loadMatrix(glh_get_current_modelview());
			LLGLSLShader::bindNoShader();
			doOcclusion(camera);
			gGL.setColorMask(true, false);
		}

		pool_set_t::iterator iter2 = iter1;
		if (hasRenderType(poolp->getType()) && poolp->getNumPostDeferredPasses() > 0)
		{
			LL_RECORD_BLOCK_TIME(FTM_POST_DEFERRED_POOLRENDER);

			gGLLastMatrix = NULL;
			gGL.loadMatrix(glh_get_current_modelview());
		
			for( S32 i = 0; i < poolp->getNumPostDeferredPasses(); i++ )
			{
				LLVertexBuffer::unbind();
				mInRenderPass = true;
				poolp->beginPostDeferredPass(i);
				for (iter2 = iter1; iter2 != mPools.end(); iter2++)
				{
					LLDrawPool *p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
										
					p->renderPostDeferred(i);
				}
				poolp->endPostDeferredPass(i);
				clearRenderPassStates();
				LLVertexBuffer::unbind();

				if (gDebugGL || gDebugPipeline)
				{
					LLGLStateValidator::checkStates();
				}
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != mPools.end(); iter2++)
			{
				LLDrawPool *p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
		stop_glerror();
	}

	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_current_modelview());

	if (occlude)
	{
		occlude = FALSE;
		gGLLastMatrix = NULL;
		gGL.loadMatrix(glh_get_current_modelview());
		LLGLSLShader::bindNoShader();
		doOcclusion(camera);
		gGLLastMatrix = NULL;
		gGL.loadMatrix(glh_get_current_modelview());
	}
}

void LLPipeline::renderGeomShadow(LLCamera& camera)
{
	U32 cur_type = 0;
	
	LLGLEnable<GL_CULL_FACE> cull;

	LLVertexBuffer::unbind();

	pool_set_t::iterator iter1 = mPools.begin();
	
	while ( iter1 != mPools.end() )
	{
		LLDrawPool *poolp = *iter1;
		
		cur_type = poolp->getType();

		pool_set_t::iterator iter2 = iter1;
		if (hasRenderType(poolp->getType()) && poolp->getNumShadowPasses() > 0)
		{
			poolp->prerender() ;

			gGLLastMatrix = NULL;
			gGL.loadMatrix(glh_get_current_modelview());
		
			for( S32 i = 0; i < poolp->getNumShadowPasses(); i++ )
			{
				LLVertexBuffer::unbind();
				mInRenderPass = true;
				poolp->beginShadowPass(i);
				for (iter2 = iter1; iter2 != mPools.end(); iter2++)
				{
					LLDrawPool *p = *iter2;
					if (p->getType() != cur_type)
					{
						break;
					}
										
					p->renderShadow(i);
				}
				poolp->endShadowPass(i);
				clearRenderPassStates();
				LLVertexBuffer::unbind();

				LLGLStateValidator::checkStates();
			}
		}
		else
		{
			// Skip all pools of this type
			for (iter2 = iter1; iter2 != mPools.end(); iter2++)
			{
				LLDrawPool *p = *iter2;
				if (p->getType() != cur_type)
				{
					break;
				}
			}
		}
		iter1 = iter2;
		stop_glerror();
	}

	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_current_modelview());
}


void LLPipeline::addTrianglesDrawn(S32 index_count, U32 render_type)
{
	assertInitialized();
	S32 count = 0;
	if (render_type == LLRender::TRIANGLE_STRIP)
	{
		count = index_count-2;
	}
	else
	{
		count = index_count/3;
	}

	mTrianglesDrawn += count;
	mBatchCount++;
	mMaxBatchSize = llmax(mMaxBatchSize, count);
	mMinBatchSize = llmin(mMinBatchSize, count);

	if (LLPipeline::sRenderFrameTest)
	{
		gViewerWindow->getWindow()->swapBuffers();
		ms_sleep(16);
	}
}

void LLPipeline::renderPhysicsDisplay()
{
	if (!hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
	{
		return;
	}

	allocatePhysicsBuffer();

	gGL.flush();
	mPhysicsDisplay.bindTarget();
	glClearColor(0,0,0,1);
	gGL.setColorMask(true, true);
	mPhysicsDisplay.clear();
	glClearColor(0,0,0,0);

	gGL.setColorMask(true, false);

	if (LLGLSLShader::sNoFixedFunction)
	{
		gDebugProgram.bind();
	}

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				if (hasRenderType(part->mDrawableType))
				{
					part->renderPhysicsShapes();
				}
			}
		}
	}

	gGL.flush();

	if (LLGLSLShader::sNoFixedFunction)
	{
		gDebugProgram.unbind();
	}

	mPhysicsDisplay.flush();
}


void LLPipeline::renderDebug()
{
	assertInitialized();


	gGL.color4f(1,1,1,1);

	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_current_modelview());
	gGL.setColorMask(true, false);
	bool hud_only = hasRenderType(LLPipeline::RENDER_TYPE_HUD);

	if (!hud_only && !mDebugBlips.empty())
	{ //render debug blips
		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.bind();
		}

		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep, true);

		gGL.setPointSize(8.f);
		LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);

		gGL.begin(LLRender::POINTS);
		for (std::list<DebugBlip>::iterator iter = mDebugBlips.begin(); iter != mDebugBlips.end(); )
		{
			DebugBlip& blip = *iter;

			blip.mAge += gFrameIntervalSeconds;
			if (blip.mAge > 2.f)
			{
				mDebugBlips.erase(iter++);
			}
			else
			{
				iter++;
			}

			blip.mPosition.mV[2] += gFrameIntervalSeconds*2.f;

			gGL.color4fv(blip.mColor.mV);
			gGL.vertex3fv(blip.mPosition.mV);
		}
		gGL.end();
		gGL.setPointSize(1.f);

		if (LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.unbind();
		}

	}

	if(!mRenderDebugMask)
		return;

	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_LEQUAL);

	// Debug stuff.
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;
		for (U32 i = 0; i < LLViewerRegion::NUM_PARTITIONS; i++)
		{
			LLSpatialPartition* part = region->getSpatialPartition(i);
			if (part)
			{
				if ( (hud_only && (part->mDrawableType == RENDER_TYPE_HUD || part->mDrawableType == RENDER_TYPE_HUD_PARTICLES)) ||
					 (!hud_only && hasRenderType(part->mDrawableType)) )
				{
					part->renderDebug();
				}
			}
		}
	}

	for (LLCullResult::bridge_iterator i = sCull->beginVisibleBridge(); i != sCull->endVisibleBridge(); ++i)
	{
		LLSpatialBridge* bridge = *i;
		if (!bridge->isDead() && hasRenderType(bridge->mDrawableType))
		{
			gGL.pushMatrix();
			gGL.multMatrix(bridge->mDrawable->getRenderMatrix());
			bridge->renderDebug();
			gGL.popMatrix();
		}
	}

	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.bind();
	}

	if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
	{
		LLVertexBuffer::unbind();

		LLGLEnable<GL_BLEND> blend;
		LLGLDepthTest depth(TRUE, FALSE);
		LLGLDisable<GL_CULL_FACE> cull;

		gGL.color4f(1,1,1,1);
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				
		F32 a = 0.1f;

		F32 col[] =
		{
			1,0,0,a,
			0,1,0,a,
			0,0,1,a,
			1,0,1,a,

			1,1,0,a,
			0,1,1,a,
			1,1,1,a,
			1,0,1,a,
		};

		for (U32 i = 0; i < 8; i++)
		{
			LLVector3* frust = mShadowCamera[i].mAgentFrustum;

			if (i > 3)
			{ //render shadow frusta as volumes
				if (mShadowFrustPoints[i-4].empty())
			{
					continue;
				}

				gGL.color4fv(col+(i-4)*4);
			
				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
				gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[5].mV);
				gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[6].mV);
				gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[7].mV);
				gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
				gGL.end();


				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3fv(frust[0].mV);
				gGL.vertex3fv(frust[1].mV);
				gGL.vertex3fv(frust[3].mV);
				gGL.vertex3fv(frust[2].mV);
				gGL.end();

				gGL.begin(LLRender::TRIANGLE_STRIP);
				gGL.vertex3fv(frust[4].mV);
				gGL.vertex3fv(frust[5].mV);
				gGL.vertex3fv(frust[7].mV);
				gGL.vertex3fv(frust[6].mV);
				gGL.end();
			}


			if (i < 4)
			{
				
				//if (i == 0 || !mShadowFrustPoints[i].empty())
				{
					//render visible point cloud
					gGL.setPointSize(8.f);
					gGL.begin(LLRender::POINTS);
					
					F32* c = col+i*4;
					gGL.color3fv(c);

					for (U32 j = 0; j < mShadowFrustPoints[i].size(); ++j)
						{
							gGL.vertex3fv(mShadowFrustPoints[i][j].mV);

						}
					gGL.end();

					gGL.setPointSize(1.f);

					LLVector3* ext = mShadowExtents[i];
					LLVector3 pos = (ext[0]+ext[1])*0.5f;
					LLVector3 size = (ext[1]-ext[0])*0.5f;
					drawBoxOutline(pos, size);

					//render camera frustum splits as outlines
					gGL.begin(LLRender::LINES);
					gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[1].mV);
					gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[2].mV);
					gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[3].mV);
					gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[0].mV);
					gGL.vertex3fv(frust[4].mV); gGL.vertex3fv(frust[5].mV);
					gGL.vertex3fv(frust[5].mV); gGL.vertex3fv(frust[6].mV);
					gGL.vertex3fv(frust[6].mV); gGL.vertex3fv(frust[7].mV);
					gGL.vertex3fv(frust[7].mV); gGL.vertex3fv(frust[4].mV);
					gGL.vertex3fv(frust[0].mV); gGL.vertex3fv(frust[4].mV);
					gGL.vertex3fv(frust[1].mV); gGL.vertex3fv(frust[5].mV);
					gGL.vertex3fv(frust[2].mV); gGL.vertex3fv(frust[6].mV);
					gGL.vertex3fv(frust[3].mV); gGL.vertex3fv(frust[7].mV);
					gGL.end();
				}
			}

			/*gGL.flush();
			glLineWidth(16-i*2);
			for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
					iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
			{
				LLViewerRegion* region = *iter;
				for (U32 j = 0; j < LLViewerRegion::NUM_PARTITIONS; j++)
				{
					LLSpatialPartition* part = region->getSpatialPartition(j);
					if (part)
					{
						if (hasRenderType(part->mDrawableType))
						{
							part->renderIntersectingBBoxes(&mShadowCamera[i]);
						}
					}
				}
			}
			gGL.flush();
			glLineWidth(1.f);*/
		}
	}

	if (mRenderDebugMask & RENDER_DEBUG_WIND_VECTORS)
	{
		gAgent.getRegion()->mWind.renderVectors();
	}
	
	if (mRenderDebugMask & RENDER_DEBUG_COMPOSITION)
	{
		// Debug composition layers
		F32 x, y;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if (gAgent.getRegion())
		{
			gGL.begin(LLRender::POINTS);
			// Draw the composition layer for the region that I'm in.
			for (x = 0; x <= 260; x++)
			{
				for (y = 0; y <= 260; y++)
				{
					if ((x > 255) || (y > 255))
					{
						gGL.color4f(1.f, 0.f, 0.f, 1.f);
					}
					else
					{
						gGL.color4f(0.f, 0.f, 1.f, 1.f);
					}
					F32 z = gAgent.getRegion()->getCompositionXY((S32)x, (S32)y);
					z *= 5.f;
					z += 50.f;
					gGL.vertex3f(x, y, z);
				}
			}
			gGL.end();
		}
	}

	if (mRenderDebugMask & LLPipeline::RENDER_DEBUG_BUILD_QUEUE)
	{
		U32 count = 0;
		U32 size = mGroupQ2.size();
		LLColor4 col;

		LLVertexBuffer::unbind();
		LLGLEnable<GL_BLEND> blend;
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
		LLGLDepthTest depth(GL_TRUE, GL_FALSE);
		gGL.getTexUnit(0)->bind(LLViewerFetchedTexture::sWhiteImagep);
		
		gGL.pushMatrix();
		gGL.loadMatrix(glh_get_current_modelview());
		gGLLastMatrix = NULL;

		for (LLSpatialGroup::sg_vector_t::iterator iter = mGroupQ2.begin(); iter != mGroupQ2.end(); ++iter)
		{
			LLSpatialGroup* group = *iter;
			if (group->isDead())
			{
				continue;
			}

			LLSpatialBridge* bridge = group->getSpatialPartition()->asBridge();

			if (bridge && (!bridge->mDrawable || bridge->mDrawable->isDead()))
			{
				continue;
			}

			if (bridge)
			{
				gGL.pushMatrix();
				gGL.multMatrix(bridge->mDrawable->getRenderMatrix());
			}

			F32 alpha = llclamp((F32) (size-count)/size, 0.f, 1.f);

			
			LLVector2 c(1.f-alpha, alpha);
			c.normVec();

			
			++count;
			col.set(c.mV[0], c.mV[1], 0, alpha*0.5f+0.5f);
			group->drawObjectBox(col);

			if (bridge)
			{
				gGL.popMatrix();
			}
		}

		gGL.popMatrix();
	}

	gGL.flush();
	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.unbind();
	}
}

static LLTrace::BlockTimerStatHandle FTM_REBUILD_POOLS("Rebuild Pools");

void LLPipeline::rebuildPools()
{
	LL_RECORD_BLOCK_TIME(FTM_REBUILD_POOLS);

	assertInitialized();

	S32 max_count = mPools.size();
	pool_set_t::iterator iter1 = mPools.upper_bound(mLastRebuildPool);
	while(max_count > 0 && mPools.size() > 0) // && num_rebuilds < MAX_REBUILDS)
	{
		if (iter1 == mPools.end())
		{
			iter1 = mPools.begin();
		}
		LLDrawPool* poolp = *iter1;

		if (poolp->isDead())
		{
			mPools.erase(iter1++);
			removeFromQuickLookup( poolp );
			if (poolp == mLastRebuildPool)
			{
				mLastRebuildPool = NULL;
			}
			delete poolp;
		}
		else
		{
			mLastRebuildPool = poolp;
			iter1++;
		}
		max_count--;
	}
}

void LLPipeline::addToQuickLookup( LLDrawPool* new_poolp )
{
	assertInitialized();

	switch( new_poolp->getType() )
	{
	case LLDrawPool::POOL_SIMPLE:
		if (mSimplePool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate simple pool." << LL_ENDL;
		}
		else
		{
			mSimplePool = (LLRenderPass*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_ALPHA_MASK:
		if (mAlphaMaskPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate alpha mask pool." << LL_ENDL;
			break;
		}
		else
		{
			mAlphaMaskPool = (LLRenderPass*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
		if (mFullbrightAlphaMaskPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate alpha mask pool." << LL_ENDL;
			break;
		}
		else
		{
			mFullbrightAlphaMaskPool = (LLRenderPass*) new_poolp;
		}
		break;
		
	case LLDrawPool::POOL_GRASS:
		if (mGrassPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate grass pool." << LL_ENDL;
		}
		else
		{
			mGrassPool = (LLRenderPass*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_FULLBRIGHT:
		if (mFullbrightPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate simple pool." << LL_ENDL;
		}
		else
		{
			mFullbrightPool = (LLRenderPass*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_GLOW:
		if (mGlowPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate glow pool." << LL_ENDL;
		}
		else
		{
			mGlowPool = (LLRenderPass*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_TREE:
		mTreePools[ uintptr_t(new_poolp->getTexture()) ] = new_poolp ;
		break;
 
	case LLDrawPool::POOL_TERRAIN:
		mTerrainPools[ uintptr_t(new_poolp->getTexture()) ] = new_poolp ;
		break;

	case LLDrawPool::POOL_BUMP:
		if (mBumpPool)
		{
			llassert(0);
			LL_WARNS() << "Ignoring duplicate bump pool." << LL_ENDL;
		}
		else
		{
			mBumpPool = new_poolp;
		}
		break;
	case LLDrawPool::POOL_MATERIALS:
		if (mMaterialsPool)
		{
			llassert(0);
			LL_WARNS() << "Ignorning duplicate materials pool." << LL_ENDL;
		}
		else
		{
			mMaterialsPool = new_poolp;
		}
		break;
	case LLDrawPool::POOL_ALPHA:
		if( mAlphaPool )
		{
			llassert(0);
			LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Alpha pool" << LL_ENDL;
		}
		else
		{
			mAlphaPool = (LLDrawPoolAlpha*) new_poolp;
		}
		break;

	case LLDrawPool::POOL_AVATAR:
		break; // Do nothing

	case LLDrawPool::POOL_SKY:
		if( mSkyPool )
		{
			llassert(0);
			LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Sky pool" << LL_ENDL;
		}
		else
		{
			mSkyPool = new_poolp;
		}
		break;
	
	case LLDrawPool::POOL_WATER:
		if( mWaterPool )
		{
			llassert(0);
			LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Water pool" << LL_ENDL;
		}
		else
		{
			mWaterPool = new_poolp;
		}
		break;

	case LLDrawPool::POOL_GROUND:
		if( mGroundPool )
		{
			llassert(0);
			LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate Ground Pool" << LL_ENDL;
		}
		else
		{ 
			mGroundPool = new_poolp;
		}
		break;

	case LLDrawPool::POOL_WL_SKY:
		if( mWLSkyPool )
		{
			llassert(0);
			LL_WARNS() << "LLPipeline::addPool(): Ignoring duplicate WLSky Pool" << LL_ENDL;
		}
		else
		{ 
			mWLSkyPool = new_poolp;
		}
		break;

	default:
		llassert(0);
		LL_WARNS() << "Invalid Pool Type in  LLPipeline::addPool()" << LL_ENDL;
		break;
	}
}

void LLPipeline::removePool( LLDrawPool* poolp )
{
	assertInitialized();
	removeFromQuickLookup(poolp);
	mPools.erase(poolp);
	delete poolp;
}

void LLPipeline::removeFromQuickLookup( LLDrawPool* poolp )
{
	assertInitialized();
	switch( poolp->getType() )
	{
	case LLDrawPool::POOL_SIMPLE:
		llassert(mSimplePool == poolp);
		mSimplePool = NULL;
		break;

	case LLDrawPool::POOL_ALPHA_MASK:
		llassert(mAlphaMaskPool == poolp);
		mAlphaMaskPool = NULL;
		break;

	case LLDrawPool::POOL_FULLBRIGHT_ALPHA_MASK:
		llassert(mFullbrightAlphaMaskPool == poolp);
		mFullbrightAlphaMaskPool = NULL;
		break;

	case LLDrawPool::POOL_GRASS:
		llassert(mGrassPool == poolp);
		mGrassPool = NULL;
		break;

	case LLDrawPool::POOL_FULLBRIGHT:
		llassert(mFullbrightPool == poolp);
		mFullbrightPool = NULL;
		break;

	case LLDrawPool::POOL_WL_SKY:
		llassert(mWLSkyPool == poolp);
		mWLSkyPool = NULL;
		break;

	case LLDrawPool::POOL_GLOW:
		llassert(mGlowPool == poolp);
		mGlowPool = NULL;
		break;

	case LLDrawPool::POOL_TREE:
		#ifdef _DEBUG
			{
				BOOL found = mTreePools.erase( (uintptr_t)poolp->getTexture() );
				llassert( found );
			}
		#else
			mTreePools.erase( (uintptr_t)poolp->getTexture() );
		#endif
		break;

	case LLDrawPool::POOL_TERRAIN:
		#ifdef _DEBUG
			{
				BOOL found = mTerrainPools.erase( (uintptr_t)poolp->getTexture() );
				llassert( found );
			}
		#else
			mTerrainPools.erase( (uintptr_t)poolp->getTexture() );
		#endif
		break;

	case LLDrawPool::POOL_BUMP:
		llassert( poolp == mBumpPool );
		mBumpPool = NULL;
		break;
			
	case LLDrawPool::POOL_MATERIALS:
		llassert(poolp == mMaterialsPool);
		mMaterialsPool = NULL;
		break;
			
	case LLDrawPool::POOL_ALPHA:
		llassert( poolp == mAlphaPool );
		mAlphaPool = NULL;
		break;

	case LLDrawPool::POOL_AVATAR:
		break; // Do nothing

	case LLDrawPool::POOL_SKY:
		llassert( poolp == mSkyPool );
		mSkyPool = NULL;
		break;

	case LLDrawPool::POOL_WATER:
		llassert( poolp == mWaterPool );
		mWaterPool = NULL;
		break;

	case LLDrawPool::POOL_GROUND:
		llassert( poolp == mGroundPool );
		mGroundPool = NULL;
		break;

	default:
		llassert(0);
		LL_WARNS() << "Invalid Pool Type in  LLPipeline::removeFromQuickLookup() type=" << poolp->getType() << LL_ENDL;
		break;
	}
}

void LLPipeline::resetDrawOrders()
{
	assertInitialized();
	// Iterate through all of the draw pools and rebuild them.
	for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
	{
		LLDrawPool *poolp = *iter;
		poolp->resetDrawOrders();
	}
}

//============================================================================
// Once-per-frame setup of hardware lights,
// including sun/moon, avatar backlight, and up to 6 local lights

U8 LLPipeline::setupFeatureLights(U8 cur_count)
{
	//assertInitialized();

	if (mLightMode == LIGHT_MODE_EDIT)
	{
		LLColor4 diffuse(1.f, 1.f, 1.f, 0.f);
		LLVector4a light_pos_cam(-8.f, 0.25f, 10.f, 0.f);  // w==0 => directional light
		LLMatrix4a camera_rot = LLViewerCamera::getInstance()->getModelview();
		camera_rot.extractRotation_affine();
		camera_rot.invert();
		LLVector4a light_pos;
		
		camera_rot.rotate(light_pos_cam,light_pos);
		
		light_pos.normalize3fast();

		LLLightState* light = gGL.getLight(cur_count++);

		light->setDiffuse(diffuse);
		light->setSpecular(LLColor4::black);
		light->setPosition(LLVector4(light_pos.getF32ptr()));
		light->setConstantAttenuation(1.f);
		light->setLinearAttenuation(0.f);
		light->setQuadraticAttenuation(0.f);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}
	else if (mLightMode == LIGHT_MODE_BACKLIGHT) // Always true (unless overridden in a devs .ini)
	{
		LLVector3 opposite_pos = -1.f * mSunDir;
		LLVector3 orthog_light_pos = mSunDir % LLVector3::z_axis;
		LLVector4 backlight_pos = LLVector4(lerp(opposite_pos, orthog_light_pos, 0.3f), 0.0f);
		backlight_pos.normalize();
			
		LLColor4 light_diffuse = mSunDiffuse;
		LLColor4 backlight_diffuse(1.f - light_diffuse.mV[VRED], 1.f - light_diffuse.mV[VGREEN], 1.f - light_diffuse.mV[VBLUE], 1.f);
		F32 max_component = 0.001f;
		for (S32 i = 0; i < 3; i++)
		{
			if (backlight_diffuse.mV[i] > max_component)
			{
				max_component = backlight_diffuse.mV[i];
			}
		}
		F32 backlight_mag;
		if (gSky.getSunDirection().mV[2] >= LLSky::NIGHTTIME_ELEVATION_COS)
		{
			backlight_mag = BACKLIGHT_DAY_MAGNITUDE_OBJECT;
		}
		else
		{
			backlight_mag = BACKLIGHT_NIGHT_MAGNITUDE_OBJECT;
		}
		backlight_diffuse *= backlight_mag / max_component;

		LLLightState* light = gGL.getLight(cur_count++);

		light->setPosition(backlight_pos);
		light->setDiffuse(backlight_diffuse);
		light->setSpecular(LLColor4::black);
		light->setConstantAttenuation(1.f);
		light->setLinearAttenuation(0.f);
		light->setQuadraticAttenuation(0.f);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}
	else if (mLightMode == LIGHT_MODE_PREVIEW)
	{
		static LLCachedControl<LLColor4> PreviewAmbientColor("PreviewAmbientColor");

		LLColor4 ambient = PreviewAmbientColor;
		gGL.setAmbientLightColor(ambient);

		static LLCachedControl<LLColor4> PreviewDiffuse0("PreviewDiffuse0");
		static LLCachedControl<LLColor4> PreviewSpecular0("PreviewSpecular0");
		static LLCachedControl<LLColor4> PreviewDiffuse1("PreviewDiffuse1");
		static LLCachedControl<LLColor4> PreviewSpecular1("PreviewSpecular1");
		static LLCachedControl<LLColor4> PreviewDiffuse2("PreviewDiffuse2");
		static LLCachedControl<LLColor4> PreviewSpecular2("PreviewSpecular2");
		static LLCachedControl<LLVector3> PreviewDirection0("PreviewDirection0");
		static LLCachedControl<LLVector3> PreviewDirection1("PreviewDirection1");
		static LLCachedControl<LLVector3> PreviewDirection2("PreviewDirection2");

		LLColor4 diffuse0 = PreviewDiffuse0;
		LLColor4 specular0 = PreviewSpecular0;
		LLColor4 diffuse1 = PreviewDiffuse1;
		LLColor4 specular1 = PreviewSpecular1;
		LLColor4 diffuse2 = PreviewDiffuse2;
		LLColor4 specular2 = PreviewSpecular2;

		LLVector3 dir0 = PreviewDirection0;
		LLVector3 dir1 = PreviewDirection1;
		LLVector3 dir2 = PreviewDirection2;

		dir0.normVec();
		dir1.normVec();
		dir2.normVec();

		LLVector4 light_pos(dir0, 0.0f);

		LLLightState* light = gGL.getLight(cur_count++);

		light->enable();
		light->setPosition(light_pos);
		light->setDiffuse(diffuse0);
		light->setSpecular(specular0);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);

		light_pos = LLVector4(dir1, 0.f);

		light = gGL.getLight(cur_count++);
		light->enable();
		light->setPosition(light_pos);
		light->setDiffuse(diffuse1);
		light->setSpecular(specular1);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);

		light_pos = LLVector4(dir2, 0.f);
		light = gGL.getLight(cur_count++);
		light->enable();
		light->setPosition(light_pos);
		light->setDiffuse(diffuse2);
		light->setSpecular(specular2);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}
	return cur_count;
}

static F32 calc_light_dist(LLVOVolume* light, const LLVector3& cam_pos, F32 max_dist)
{
	F32 inten = light->getLightIntensity();
	if (inten < .001f)
	{
		return max_dist;
	}
	F32 radius = light->getLightRadius();
	BOOL selected = light->isSelected();
	LLVector3 dpos = light->getRenderPosition() - cam_pos;
	F32 dist2 = dpos.lengthSquared();
	if (!selected && dist2 > (max_dist + radius)*(max_dist + radius))
	{
		return max_dist;
	}
	F32 dist = (F32) sqrt(dist2);
	dist *= 1.f / inten;
	dist -= radius;
	if (selected)
	{
		dist -= 10000.f; // selected lights get highest priority
	}
	if (light->mDrawable.notNull() && light->mDrawable->isState(LLDrawable::ACTIVE))
	{
		// moving lights get a little higher priority (too much causes artifacts)
		dist -= light->getLightRadius()*0.25f;
	}
	return dist;
}

//Default all gl light parameters. Used upon restoreGL. Fixes brightness problems on fullscren toggle
void LLPipeline::resetLocalLights()
{
	LLGLEnable<GL_LIGHTING> enable;
	for (S32 i = 0; i < LLRender::NUM_LIGHTS; ++i)
	{
		LLLightState *pLight = gGL.getLight(i);
		pLight->enable();
		pLight->setConstantAttenuation(0.f);
		pLight->setDiffuse(LLColor4::black);
		pLight->setLinearAttenuation(0.f);
		pLight->setPosition(LLVector4(0.f,0.f,1.f,0.f));
		pLight->setQuadraticAttenuation(0.f);
		pLight->setSpecular(LLColor4::black);
		pLight->setSpotCutoff(0.f);
		pLight->setSpotDirection(LLVector3(0.f,0.f,-1.f));
		pLight->setSpotExponent(0.f);
		pLight->disable();
	}
}

void LLPipeline::calcNearbyLights(LLCamera& camera)
{
	assertInitialized();

	if (LLPipeline::sReflectionRender)
	{
		return;
	}

	if (isLocalLightingEnabled())
	{
		// mNearbyLight (and all light_set_t's) are sorted such that
		// begin() == the closest light and rbegin() == the farthest light
		const S32 MAX_LOCAL_LIGHTS = 7;
// 		LLVector3 cam_pos = gAgentCamera.getCameraPositionAgent();
		LLVector3 cam_pos = LLViewerJoystick::getInstance()->getOverrideCamera() ?
						camera.getOrigin() : 
						gAgent.getPositionAgent();

		F32 max_dist = LIGHT_MAX_RADIUS * 4.f; // ignore enitrely lights > 4 * max light rad
		
		// UPDATE THE EXISTING NEARBY LIGHTS
		light_set_t cur_nearby_lights;
		for (light_set_t::iterator iter = mNearbyLights.begin();
			iter != mNearbyLights.end(); iter++)
		{
			const Light* light = &(*iter);
			LLDrawable* drawable = light->drawable;
			LLVOVolume* volight = drawable->getVOVolume();
			if (!volight || !drawable->isState(LLDrawable::LIGHT))
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (light->fade <= -LIGHT_FADE_TIME)
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}
			if (!sRenderAttachedLights && volight && volight->isAttachment())
			{
				drawable->clearState(LLDrawable::NEARBY_LIGHT);
				continue;
			}

			F32 dist = calc_light_dist(volight, cam_pos, max_dist);
			cur_nearby_lights.insert(Light(drawable, dist, light->fade));
		}
		mNearbyLights = cur_nearby_lights;
				
		// FIND NEW LIGHTS THAT ARE IN RANGE
		light_set_t new_nearby_lights;
		for (LLDrawable::drawable_set_t::iterator iter = mLights.begin();
			 iter != mLights.end(); ++iter)
		{
			LLDrawable* drawable = *iter;
			LLVOVolume* light = drawable->getVOVolume();
			if (!light || drawable->isState(LLDrawable::NEARBY_LIGHT))
			{
				continue;
			}
			if (light->isHUDAttachment())
			{
				continue; // no lighting from HUD objects
			}
			F32 dist = calc_light_dist(light, cam_pos, max_dist);
			if (dist >= max_dist)
			{
				continue;
			}
			if (!sRenderAttachedLights && light && light->isAttachment())
			{
				continue;
			}
			new_nearby_lights.insert(Light(drawable, dist, 0.f));
			if (new_nearby_lights.size() > (U32)MAX_LOCAL_LIGHTS)
			{
				new_nearby_lights.erase(--new_nearby_lights.end());
				const Light& last = *new_nearby_lights.rbegin();
				max_dist = last.dist;
			}
		}

		// INSERT ANY NEW LIGHTS
		for (light_set_t::iterator iter = new_nearby_lights.begin();
			 iter != new_nearby_lights.end(); iter++)
		{
			const Light* light = &(*iter);
			if (mNearbyLights.size() < (U32)MAX_LOCAL_LIGHTS)
			{
				mNearbyLights.insert(*light);
				((LLDrawable*) light->drawable)->setState(LLDrawable::NEARBY_LIGHT);
			}
			else
			{
				// crazy cast so that we can overwrite the fade value
				// even though gcc enforces sets as const
				// (fade value doesn't affect sort so this is safe)
				Light* farthest_light = (const_cast<Light*>(&(*(mNearbyLights.rbegin()))));
				if (light->dist < farthest_light->dist)
				{
					if (farthest_light->fade >= 0.f)
					{
						farthest_light->fade = -gFrameIntervalSeconds;
					}
				}
				else
				{
					break; // none of the other lights are closer
				}
			}
		}
	}
	gatherLocalLights();
}


void LLPipeline::gatherLocalLights()
{
	mLocalLights.clear();
	if (isLocalLightingEnabled())
	{
		for (light_set_t::iterator iter = mNearbyLights.begin();
			 iter != mNearbyLights.end(); ++iter)
		{
			LLDrawable* drawable = iter->drawable;
			LLVOVolume* light = drawable->getVOVolume();
			if (!light)
			{
				continue;
			}

			LLColor4  light_color = light->getLightColor();
			light_color.mV[3] = 0.0f;

			F32 fade = iter->fade;
			if (fade < LIGHT_FADE_TIME)
			{
				// fade in/out light
				if (fade >= 0.f)
				{
					fade = fade / LIGHT_FADE_TIME;
					((Light*) (&(*iter)))->fade += gFrameIntervalSeconds;
				}
				else
				{
					fade = 1.f + fade / LIGHT_FADE_TIME;
					((Light*) (&(*iter)))->fade -= gFrameIntervalSeconds;
				}
				fade = llclamp(fade,0.f,1.f);
				light_color *= fade;
			}

			LLVector3 light_pos(light->getRenderPosition());
			LLVector4 light_pos_gl(light_pos, 1.0f);
	
			F32 light_radius = llmax(light->getLightRadius(), 0.001f);

			F32 x = (3.f * (1.f + light->getLightFalloff())); // why this magic?  probably trying to match a historic behavior.
			float linatten = x / (light_radius); // % of brightness at radius

			LLLightStateData light_state = LLLightStateData();
			light_state.mPosition = light_pos_gl;
			light_state.mDiffuse = light_color;
			light_state.mConstantAtten = 0.f;

			if (sRenderDeferred)
			{
				light_state.mLinearAtten = light_radius * 1.5f;
				light_state.mQuadraticAtten = light->getLightFalloff()*0.5f + 1.f;
			}
			else
			{
				light_state.mLinearAtten = linatten;
			}
			
			static const LLCachedControl<bool> RenderSpotLightsInNondeferred("RenderSpotLightsInNondeferred",false);
			if (light->isLightSpotlight() // directional (spot-)light
			    && (LLPipeline::sRenderDeferred || RenderSpotLightsInNondeferred)) // these are only rendered as GL spotlights if we're in deferred rendering mode *or* the setting forces them on
			{
				LLQuaternion quat = light->getRenderRotation();
				LLVector3 at_axis(0,0,-1); // this matches deferred rendering's object light direction
				at_axis *= quat;

				light_state.mSpotDirection = at_axis;
				light_state.mSpotCutoff = 90.f;
				light_state.mSpotExponent = 2.f;
				light_state.mSpecular = LLColor4::transparent;
			}
			else // omnidirectional (point) light
			{
				// we use specular.w = 1.0 as a cheap hack for the shaders to know that this is omnidirectional rather than a spotlight
				light_state.mSpecular = LLColor4::black;
			}
			mLocalLights.push_back(light_state);
		}
	}
}

LLVector3 getSunDir()
{
	if (gSky.getSunDirection().mV[2] >= LLSky::NIGHTTIME_ELEVATION_COS)
	{
		return gSky.getSunDirection();
	}
	else
	{
		return gSky.getMoonDirection();
	}
}


extern U32 sLightMask;
void LLPipeline::setupHWLights()
{
	static LLCachedControl<U32> LightMask("LightMask", 0xFFFFFFFF);
	sLightMask = LightMask;

	// Ambient
	if (mLightMode == LIGHT_MODE_PREVIEW)
	{
		static LLCachedControl<LLColor4> PreviewAmbientColor("PreviewAmbientColor");
		LLColor4 ambient = PreviewAmbientColor;
		gGL.setAmbientLightColor(ambient);
	}
	else if (!LLGLSLShader::sNoFixedFunction)
	{
		LLColor4 ambient = gSky.getTotalAmbientColor();
		gGL.setAmbientLightColor(ambient);
	}

	U8 cur_light = 0;
	// Light 0 = Sun or Moon (All objects)
	{
		if (gSky.getSunDirection().mV[2] >= LLSky::NIGHTTIME_ELEVATION_COS)
		{
			mSunDir.setVec(gSky.getSunDirection());
			mSunDiffuse.setVec(gSky.getSunDiffuseColor());
		}
		else
		{
			mSunDir.setVec(gSky.getMoonDirection());
			mSunDiffuse.setVec(gSky.getMoonDiffuseColor());
		}

		F32 max_color = llmax(mSunDiffuse.mV[0], mSunDiffuse.mV[1], mSunDiffuse.mV[2]);
		if (max_color > 1.f)
		{
			mSunDiffuse *= 1.f/max_color;
		}
		mSunDiffuse.clamp();

		LLVector4 light_pos(mSunDir, 0.0f);
		LLColor4 light_diffuse = mSunDiffuse;

		LLLightState* light = gGL.getLight(cur_light++);
		light->setPosition(light_pos);
		light->setDiffuse(light_diffuse);
		light->setSpecular(LLColor4::black);
		light->setConstantAttenuation(1.f);
		light->setLinearAttenuation(0.f);
		light->setQuadraticAttenuation(0.f);
		light->setSpotExponent(0.f);
		light->setSpotCutoff(180.f);
	}
	
	// Edit mode lights
	cur_light = setupFeatureLights(cur_light);
	
	// Nearby lights
	if (isLocalLightingEnabled())
	{
		for (auto& local_light : mLocalLights)
		{
			gGL.getLight(cur_light++)->setState(local_light);
			if (cur_light >= LLRender::NUM_LIGHTS)
				break;
		}
	}
	for (S32 i = cur_light; i < LLRender::NUM_LIGHTS; ++i)
	{
		//gGL.getLight(cur_light++)->setState(LLLightStateData());
		gGL.getLight(i)->disable();
	}
	/*for (S32 i = 0; i < LLRender::NUM_LIGHTS; ++i)
	{
		gGL.getLight(i)->disable();
	}
	mLightMask = 0;*/
	mHWLightCount = cur_light;
}


void LLPipeline::updateHWLightMode(U8 mode)
{
	if (mLightMode != mode)
	{
		mLightMode = mode;
		setupHWLights();
	}
}

void LLPipeline::enableLights(U32 mask, LLGLState<GL_LIGHTING>& light_state, const LLColor4* color)
{
	if (mLightMask != mask)
	{
		stop_glerror();
		if (!mLightMask)
		{
			light_state.enable();
		}
		if (mask)
		{
			stop_glerror();
			for (S32 i=0; i < LLRender::NUM_LIGHTS; i++)
			{
				LLLightState* light = gGL.getLight(i);
				if (i < mHWLightCount && mask & (1<<i))
				{
					light->enable();
				}
				else
				{
					light->disable();
				}
			}
			stop_glerror();
		}
		else
		{
			light_state.disable();
		}
		mLightMask = mask;
		stop_glerror();
	}
	LLColor4 ambient = color ? *color : gSky.getTotalAmbientColor();
	gGL.setAmbientLightColor(ambient);
}

void LLPipeline::enableLightsStatic(LLGLState<GL_LIGHTING>& light_state)
{
	updateHWLightMode(LIGHT_MODE_NORMAL);
	enableLights(0xFFFFFFFF, light_state);
}

void LLPipeline::enableLightsDynamic(LLGLState<GL_LIGHTING>& light_state)
{
	assertInitialized();
	
	if (isAgentAvatarValid() && !isLocalLightingEnabled())
	{
		if (gAgentAvatarp->mSpecialRenderMode == 0) // normal
		{
			gPipeline.enableLightsAvatar(light_state);
			return;
		}
		else if (gAgentAvatarp->mSpecialRenderMode >= 1)  // anim preview
		{
			gPipeline.enableLightsAvatarEdit(light_state, LLColor4(0.7f, 0.6f, 0.3f, 1.f));
			return;
		}
	}

	updateHWLightMode(LIGHT_MODE_NORMAL);
	enableLights(0xFFFFFFFF, light_state);
}

void LLPipeline::enableLightsAvatar(LLGLState<GL_LIGHTING>& light_state)
{
	updateHWLightMode(gAvatarBacklight ? LIGHT_MODE_BACKLIGHT : LIGHT_MODE_NORMAL);  // Avatar backlight only, set ambient
	enableLights(0xFFFFFFFF, light_state);
}

void LLPipeline::enableLightsPreview(LLGLState<GL_LIGHTING>& light_state)
{
	updateHWLightMode(LIGHT_MODE_PREVIEW);
	enableLights(0xFFFFFFFF, light_state);
}
void LLPipeline::enableLightsAvatarEdit(LLGLState<GL_LIGHTING>& light_state, const LLColor4& color)
{
	updateHWLightMode(LIGHT_MODE_EDIT);
	enableLights(0xFFFFFFFF, light_state, &color);
}

void LLPipeline::enableLightsFullbright(LLGLState<GL_LIGHTING>& light_state)
{
	enableLights(0, light_state);
}

void LLPipeline::disableLights(LLGLState<GL_LIGHTING>& light_state)
{
	enableLights(0, light_state); // no lighting (full bright)
}

//============================================================================

class LLMenuItemGL;
class LLInvFVBridge;
struct cat_folder_pair;
class LLVOBranch;
class LLVOLeaf;

void LLPipeline::findReferences(LLDrawable *drawablep)
{
	assertInitialized();
	if (mLights.find(drawablep) != mLights.end())
	{
		LL_INFOS() << "In mLights" << LL_ENDL;
	}
	if (std::find(mMovedList.begin(), mMovedList.end(), drawablep) != mMovedList.end())
	{
		LL_INFOS() << "In mMovedList" << LL_ENDL;
	}
	if (std::find(mShiftList.begin(), mShiftList.end(), drawablep) != mShiftList.end())
	{
		LL_INFOS() << "In mShiftList" << LL_ENDL;
	}
	if (mRetexturedList.find(drawablep) != mRetexturedList.end())
	{
		LL_INFOS() << "In mRetexturedList" << LL_ENDL;
	}
	
	if (std::find(mBuildQ1.begin(), mBuildQ1.end(), drawablep) != mBuildQ1.end())
	{
		LL_INFOS() << "In mBuildQ1" << LL_ENDL;
	}
	if (std::find(mBuildQ2.begin(), mBuildQ2.end(), drawablep) != mBuildQ2.end())
	{
		LL_INFOS() << "In mBuildQ2" << LL_ENDL;
	}

	S32 count;
	
	count = gObjectList.findReferences(drawablep);
	if (count)
	{
		LL_INFOS() << "In other drawables: " << count << " references" << LL_ENDL;
	}
}

BOOL LLPipeline::verify()
{
	BOOL ok = assertInitialized();
	if (ok) 
	{
		for (pool_set_t::iterator iter = mPools.begin(); iter != mPools.end(); ++iter)
		{
			LLDrawPool *poolp = *iter;
			if (!poolp->verify())
			{
				ok = FALSE;
			}
		}
	}

	if (!ok)
	{
		LL_WARNS() << "Pipeline verify failed!" << LL_ENDL;
	}
	return ok;
}

//////////////////////////////
//
// Collision detection
//
//

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	A method to compute a ray-AABB intersection.
 *	Original code by Andrew Woo, from "Graphics Gems", Academic Press, 1990
 *	Optimized code by Pierre Terdiman, 2000 (~20-30% faster on my Celeron 500)
 *	Epsilon value added by Klaus Hartmann. (discarding it saves a few cycles only)
 *
 *	Hence this version is faster as well as more robust than the original one.
 *
 *	Should work provided:
 *	1) the integer representation of 0.0f is 0x00000000
 *	2) the sign bit of the float is the most significant one
 *
 *	Report bugs: p.terdiman@codercorner.com
 *
 *	\param		aabb		[in] the axis-aligned bounding box
 *	\param		origin		[in] ray origin
 *	\param		dir			[in] ray direction
 *	\param		coord		[out] impact coordinates
 *	\return		true if ray intersects AABB
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//#define RAYAABB_EPSILON 0.00001f
#define IR(x)	((U32&)x)

bool LLRayAABB(const LLVector3 &center, const LLVector3 &size, const LLVector3& origin, const LLVector3& dir, LLVector3 &coord, F32 epsilon)
{
	BOOL Inside = TRUE;
	LLVector3 MinB = center - size;
	LLVector3 MaxB = center + size;
	LLVector3 MaxT;
	MaxT.mV[VX]=MaxT.mV[VY]=MaxT.mV[VZ]=-1.0f;

	// Find candidate planes.
	for(U32 i=0;i<3;i++)
	{
		if(origin.mV[i] < MinB.mV[i])
		{
			coord.mV[i]	= MinB.mV[i];
			Inside		= FALSE;

			// Calculate T distances to candidate planes
			if(IR(dir.mV[i]))	MaxT.mV[i] = (MinB.mV[i] - origin.mV[i]) / dir.mV[i];
		}
		else if(origin.mV[i] > MaxB.mV[i])
		{
			coord.mV[i]	= MaxB.mV[i];
			Inside		= FALSE;

			// Calculate T distances to candidate planes
			if(IR(dir.mV[i]))	MaxT.mV[i] = (MaxB.mV[i] - origin.mV[i]) / dir.mV[i];
		}
	}

	// Ray origin inside bounding box
	if(Inside)
	{
		coord = origin;
		return true;
	}

	// Get largest of the maxT's for final choice of intersection
	U32 WhichPlane = 0;
	if(MaxT.mV[1] > MaxT.mV[WhichPlane])	WhichPlane = 1;
	if(MaxT.mV[2] > MaxT.mV[WhichPlane])	WhichPlane = 2;

	// Check final candidate actually inside box
	if(IR(MaxT.mV[WhichPlane])&0x80000000) return false;

	for(U32 i=0;i<3;i++)
	{
		if(i!=WhichPlane)
		{
			coord.mV[i] = origin.mV[i] + MaxT.mV[WhichPlane] * dir.mV[i];
			if (epsilon > 0)
			{
				if(coord.mV[i] < MinB.mV[i] - epsilon || coord.mV[i] > MaxB.mV[i] + epsilon)	return false;
			}
			else
			{
				if(coord.mV[i] < MinB.mV[i] || coord.mV[i] > MaxB.mV[i])	return false;
			}
		}
	}
	return true;	// ray hits box
}

//////////////////////////////
//
// Macros, functions, and inline methods from other classes
//
//

void LLPipeline::setLight(LLDrawable *drawablep, BOOL is_light)
{
	if (drawablep && assertInitialized())
	{
		if (is_light)
		{
			mLights.insert(drawablep);
			drawablep->setState(LLDrawable::LIGHT);
		}
		else
		{
			drawablep->clearState(LLDrawable::LIGHT);
			mLights.erase(drawablep);
		}
	}
}

//static
void LLPipeline::toggleRenderType(U32 type)
{
	gPipeline.mRenderTypeEnabled[type] = !gPipeline.mRenderTypeEnabled[type];
	if (type == LLPipeline::RENDER_TYPE_WATER)
	{
		gPipeline.mRenderTypeEnabled[LLPipeline::RENDER_TYPE_VOIDWATER] = !gPipeline.mRenderTypeEnabled[LLPipeline::RENDER_TYPE_VOIDWATER];
	}
}

//static
void LLPipeline::toggleRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	U32 bit = (1<<type);
	if (gPipeline.hasRenderType(type))
	{
		LL_INFOS() << "Toggling render type mask " << std::hex << bit << " off" << std::dec << LL_ENDL;
	}
	else
	{
		LL_INFOS() << "Toggling render type mask " << std::hex << bit << " on" << std::dec << LL_ENDL;
	}
	gPipeline.toggleRenderType(type);
}

//static
BOOL LLPipeline::hasRenderTypeControl(void* data)
{
	U32 type = (U32)(intptr_t)data;
	return gPipeline.hasRenderType(type);
}

// Allows UI items labeled "Hide foo" instead of "Show foo"
//static
BOOL LLPipeline::toggleRenderTypeControlNegated(void* data)
{
	S32 type = (S32)(intptr_t)data;
	return !gPipeline.hasRenderType(type);
}

//static
BOOL LLPipeline::hasRenderPairedTypeControl(void* data)
{
	U64 typeflags = *(U64*)data;
	for(U8 i = 1 ;i < NUM_RENDER_TYPES; ++i)
	{
		if( typeflags & (1ULL<<i) )
		{
			if( gPipeline.hasRenderType(i) )
				return true;
		}
	}
	return false;
}

//static
void LLPipeline::toggleRenderPairedTypeControl(void *data)
{
	bool on = !!hasRenderPairedTypeControl(data);
	U64 typeflags = *(U64*)data;
	for(U8 i = 1 ;i < NUM_RENDER_TYPES; ++i)
	{
		if( typeflags & (1ULL<<i))
		{
			LL_INFOS() << "Toggling render type mask " << std::hex << (1ULL<<i) << (on ? " off" : " on") << std::dec << LL_ENDL;
			gPipeline.mRenderTypeEnabled[i]=!on;
		}
	}	
}

//static
void LLPipeline::toggleRenderDebug(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	if (gPipeline.hasRenderDebugMask(bit))
	{
		LL_INFOS() << "Toggling render debug mask " << std::hex << bit << " off" << std::dec << LL_ENDL;
	}
	else
	{
		LL_INFOS() << "Toggling render debug mask " << std::hex << bit << " on" << std::dec << LL_ENDL;
	}
	gPipeline.mRenderDebugMask ^= bit;
}


//static
BOOL LLPipeline::toggleRenderDebugControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugMask(bit);
}

//static
void LLPipeline::toggleRenderDebugFeature(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	gPipeline.mRenderDebugFeatureMask ^= bit;
}


//static
BOOL LLPipeline::toggleRenderDebugFeatureControl(void* data)
{
	U32 bit = (U32)(intptr_t)data;
	return gPipeline.hasRenderDebugFeatureMask(bit);
}

void LLPipeline::setRenderDebugFeatureControl(U32 bit, bool value)
{
	if (value)
	{
		gPipeline.mRenderDebugFeatureMask |= bit;
	}
	else
	{
		gPipeline.mRenderDebugFeatureMask &= ~bit;
	}
}

void LLPipeline::pushRenderDebugFeatureMask()
{
	mRenderDebugFeatureStack.push(mRenderDebugFeatureMask);
}

void LLPipeline::popRenderDebugFeatureMask()
{
	if (mRenderDebugFeatureStack.empty())
	{
		LL_ERRS() << "Depleted render feature stack." << LL_ENDL;
	}

	mRenderDebugFeatureMask = mRenderDebugFeatureStack.top();
	mRenderDebugFeatureStack.pop();
}

// static
void LLPipeline::setRenderScriptedBeacons(BOOL val)
{
	sRenderScriptedBeacons = val;
}

// static
void LLPipeline::toggleRenderScriptedBeacons(void*)
{
	sRenderScriptedBeacons = !sRenderScriptedBeacons;
}

// static
BOOL LLPipeline::getRenderScriptedBeacons(void*)
{
	return sRenderScriptedBeacons;
}

// static
void LLPipeline::setRenderScriptedTouchBeacons(BOOL val)
{
	sRenderScriptedTouchBeacons = val;
}

// static
void LLPipeline::toggleRenderScriptedTouchBeacons(void*)
{
	sRenderScriptedTouchBeacons = !sRenderScriptedTouchBeacons;
}

// static
BOOL LLPipeline::getRenderScriptedTouchBeacons(void*)
{
	return sRenderScriptedTouchBeacons;
}

// static
void LLPipeline::setRenderMOAPBeacons(BOOL val)
{
	sRenderMOAPBeacons = val;
}

// static
void LLPipeline::toggleRenderMOAPBeacons(void*)
{
	sRenderMOAPBeacons = !sRenderMOAPBeacons;
}

// static
BOOL LLPipeline::getRenderMOAPBeacons(void*)
{
	return sRenderMOAPBeacons;
}

// static
void LLPipeline::setRenderPhysicalBeacons(BOOL val)
{
	sRenderPhysicalBeacons = val;
}

// static
void LLPipeline::toggleRenderPhysicalBeacons(void*)
{
	sRenderPhysicalBeacons = !sRenderPhysicalBeacons;
}

// static
BOOL LLPipeline::getRenderPhysicalBeacons(void*)
{
	return sRenderPhysicalBeacons;
}

// static
void LLPipeline::setRenderParticleBeacons(BOOL val)
{
	sRenderParticleBeacons = val;
}

// static
void LLPipeline::toggleRenderParticleBeacons(void*)
{
	sRenderParticleBeacons = !sRenderParticleBeacons;
}

// static
BOOL LLPipeline::getRenderParticleBeacons(void*)
{
	return sRenderParticleBeacons;
}

// static
void LLPipeline::setRenderSoundBeacons(BOOL val)
{
	sRenderSoundBeacons = val;
}

// static
void LLPipeline::toggleRenderSoundBeacons(void*)
{
	sRenderSoundBeacons = !sRenderSoundBeacons;
}

// static
BOOL LLPipeline::getRenderSoundBeacons(void*)
{
	return sRenderSoundBeacons;
}

// static
void LLPipeline::setRenderBeacons(BOOL val)
{
	sRenderBeacons = val;
}

// static
void LLPipeline::toggleRenderBeacons(void*)
{
	sRenderBeacons = !sRenderBeacons;
}

// static
BOOL LLPipeline::getRenderBeacons(void*)
{
	return sRenderBeacons;
}

// static
void LLPipeline::setRenderHighlights(BOOL val)
{
	sRenderHighlight = val;
}

// static
void LLPipeline::toggleRenderHighlights(void*)
{
	sRenderHighlight = !sRenderHighlight;
}

// static
BOOL LLPipeline::getRenderHighlights(void*)
{
	return sRenderHighlight;
}

// static
void LLPipeline::setRenderHighlightTextureChannel(LLRender::eTexIndex channel)
{
	sRenderHighlightTextureChannel = channel;
}

LLVOPartGroup* LLPipeline::lineSegmentIntersectParticle(const LLVector4a& start, const LLVector4a& end, LLVector4a* intersection,
														S32* face_hit)
{
	LLVector4a local_end = end;

	LLVector4a position;
	position.clear();

	LLDrawable* drawable = NULL;

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;

		LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_PARTICLE);
		if (part && hasRenderType(part->mDrawableType))
		{
			LLDrawable* hit = part->lineSegmentIntersect(start, local_end, TRUE, FALSE, face_hit, &position, NULL, NULL, NULL);
			if (hit)
			{
				drawable = hit;
				local_end = position;						
			}
		}
	}

	LLVOPartGroup* ret = NULL;
	if (drawable)
	{
		//make sure we're returning an LLVOPartGroup
		llassert(drawable->getVObj()->getPCode() == LLViewerObject::LL_VO_PART_GROUP);
		ret = (LLVOPartGroup*) drawable->getVObj().get();
	}
		
	if (intersection)
	{
		*intersection = position;
	}

	return ret;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInWorld(const LLVector4a& start, const LLVector4a& end,
														BOOL pick_transparent,												
														bool pick_rigged,
														S32* face_hit,
														LLVector4a* intersection,         // return the intersection point
														LLVector2* tex_coord,            // return the texture coordinates of the intersection point
														LLVector4a* normal,               // return the surface normal at the intersection point
														LLVector4a* tangent             // return the surface tangent at the intersection point
	)
{
	LLDrawable* drawable = NULL;

	LLVector4a local_end = end;

	LLVector4a position;
	position.clear();

	sPickAvatar = FALSE; //LLToolMgr::getInstance()->inBuildMode() ? FALSE : TRUE;
	
	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;

		for (U32 j = 0; j < LLViewerRegion::NUM_PARTITIONS; j++)
		{
			if ((j == LLViewerRegion::PARTITION_VOLUME) || 
				(j == LLViewerRegion::PARTITION_ATTACHMENT) ||
				(j == LLViewerRegion::PARTITION_BRIDGE) || 
				(j == LLViewerRegion::PARTITION_TERRAIN) ||
				(j == LLViewerRegion::PARTITION_TREE) ||
				(j == LLViewerRegion::PARTITION_GRASS))  // only check these partitions for now
			{
				LLSpatialPartition* part = region->getSpatialPartition(j);
				if (part && hasRenderType(part->mDrawableType))
				{
					LLDrawable* hit = part->lineSegmentIntersect(start, local_end, pick_transparent, pick_rigged, face_hit, &position, tex_coord, normal, tangent);
					if (hit)
					{
						drawable = hit;
						local_end = position;						
					}
				}
			}
		}
	}
	
	if (!sPickAvatar)
	{
		//save hit info in case we need to restore
		//due to attachment override
		LLVector4a local_normal;
		LLVector4a local_tangent;
		LLVector2 local_texcoord;
		S32 local_face_hit = -1;

		if (face_hit)
		{ 
			local_face_hit = *face_hit;
		}
		if (tex_coord)
		{
			local_texcoord = *tex_coord;
		}
		if (tangent)
		{
			local_tangent = *tangent;
		}
		else
		{
			local_tangent.clear();
		}
		if (normal)
		{
			local_normal = *normal;
		}
		else
		{
			local_normal.clear();
		}
				
		const F32 ATTACHMENT_OVERRIDE_DIST = 0.1f;

		//check against avatars
		sPickAvatar = TRUE;
		for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
				iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
		{
			LLViewerRegion* region = *iter;

			LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_ATTACHMENT);
			if (part && hasRenderType(part->mDrawableType))
			{
				LLDrawable* hit = part->lineSegmentIntersect(start, local_end, pick_transparent, pick_rigged, face_hit, &position, tex_coord, normal, tangent);
				if (hit)
				{
					LLVector4a delta;
					delta.setSub(position, local_end);

					if (!drawable || 
						!drawable->getVObj()->isAttachment() ||
						delta.getLength3().getF32() > ATTACHMENT_OVERRIDE_DIST)
					{ //avatar overrides if previously hit drawable is not an attachment or 
					  //attachment is far enough away from detected intersection
						drawable = hit;
						local_end = position;						
					}
					else
					{ //prioritize attachments over avatars
						position = local_end;

						if (face_hit)
						{
							*face_hit = local_face_hit;
						}
						if (tex_coord)
						{
							*tex_coord = local_texcoord;
						}
						if (tangent)
						{
							*tangent = local_tangent;
						}
						if (normal)
						{
							*normal = local_normal;
						}
					}
				}
			}
		}
	}

	//check all avatar nametags (silly, isn't it?)
	for (std::vector< LLCharacter* >::iterator iter = LLCharacter::sInstances.begin();
		iter != LLCharacter::sInstances.end();
		++iter)
	{
		LLVOAvatar* av = (LLVOAvatar*) *iter;
		if (av->mNameText.notNull()
			&& av->mNameText->lineSegmentIntersect(start, local_end, position))
		{
			drawable = av->mDrawable;
			local_end = position;
		}
	}

	if (intersection)
	{
		*intersection = position;
	}

	return drawable ? drawable->getVObj().get() : NULL;
}

LLViewerObject* LLPipeline::lineSegmentIntersectInHUD(const LLVector4a& start, const LLVector4a& end,
													  BOOL pick_transparent,													
													  S32* face_hit,
													  LLVector4a* intersection,         // return the intersection point
													  LLVector2* tex_coord,            // return the texture coordinates of the intersection point
													  LLVector4a* normal,               // return the surface normal at the intersection point
													  LLVector4a* tangent				// return the surface tangent at the intersection point
	)
{
	LLDrawable* drawable = NULL;

	for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin(); 
			iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
	{
		LLViewerRegion* region = *iter;

		BOOL toggle = FALSE;
		if (!hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
			toggle = TRUE;
		}

		LLSpatialPartition* part = region->getSpatialPartition(LLViewerRegion::PARTITION_HUD);
		if (part)
		{
			LLDrawable* hit = part->lineSegmentIntersect(start, end, pick_transparent, FALSE, face_hit, intersection, tex_coord, normal, tangent);
			if (hit)
			{
				drawable = hit;
			}
		}

		if (toggle)
		{
			toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}
	}
	return drawable ? drawable->getVObj().get() : NULL;
}

LLSpatialPartition* LLPipeline::getSpatialPartition(LLViewerObject* vobj)
{
	if (vobj)
	{
		LLViewerRegion* region = vobj->getRegion();
		if (region)
		{
			return region->getSpatialPartition(vobj->getPartitionType());
		}
	}
	return NULL;
}

void LLPipeline::resetVertexBuffers(LLDrawable* drawable)
{
	if (!drawable)
	{
		return;
	}

	for (S32 i = 0; i < drawable->getNumFaces(); i++)
	{
		LLFace* facep = drawable->getFace(i);
		if (facep)
		{
			facep->clearVertexBuffer();
		}
	}
}

void LLPipeline::resetVertexBuffers()
{
	// Only set to true if pipeline has been initialized (No vbo's to reset, otherwise)
	//if (mInitialized)
	mResetVertexBuffers = true;
}

static LLTrace::BlockTimerStatHandle FTM_RESET_VB("Reset VB");

void LLPipeline::doResetVertexBuffers(bool forced)
{
	if ( !mResetVertexBuffers)
	{
		return;
	}
	if(!forced && LLSpatialPartition::sTeleportRequested)
	{
		if(gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
		{
			return; //wait for teleporting to finish
		}
		else
		{
			//teleporting aborted
			LLSpatialPartition::sTeleportRequested = FALSE;
			mResetVertexBuffers = false;
			return;
		}
	}

	LL_RECORD_BLOCK_TIME(FTM_RESET_VB);
	mResetVertexBuffers = false;
	releaseVertexBuffers();
	if(LLSpatialPartition::sTeleportRequested)
	{
		LLSpatialPartition::sTeleportRequested = FALSE;

		LLWorld::getInstance()->clearAllVisibleObjects();
		clearRebuildDrawables();
	}

	refreshCachedSettings();

	LLVertexBuffer::initClass(LLVertexBuffer::sEnableVBOs, LLVertexBuffer::sDisableVBOMapping);

	LLVOPartGroup::restoreGL();

	gGL.restoreVertexBuffers();
}

void LLPipeline::renderObjects(U32 type, U32 mask, BOOL texture, BOOL batch_texture)
{
	assertInitialized();
	gGL.loadMatrix(glh_get_current_modelview());
	gGLLastMatrix = NULL;
	mSimplePool->pushBatches(type, mask, texture, batch_texture);
	gGL.loadMatrix(glh_get_current_modelview());
	gGLLastMatrix = NULL;		
}

void LLPipeline::renderMaskedObjects(U32 type, U32 mask, BOOL texture, BOOL batch_texture)
{
	assertInitialized();
	gGL.loadMatrix(glh_get_current_modelview());
	gGLLastMatrix = NULL;
	mAlphaMaskPool->pushMaskBatches(type, mask, texture, batch_texture);
	gGL.loadMatrix(glh_get_current_modelview());
	gGLLastMatrix = NULL;		
}


void apply_cube_face_rotation(U32 face)
{
	static const LLMatrix4a x_90 = gGL.genRot( 90.f, 1.f, 0.f, 0.f );
	static const LLMatrix4a y_90 = gGL.genRot( 90.f, 0.f, 1.f, 0.f );
	static const LLMatrix4a x_90_neg = gGL.genRot( -90.f, 1.f, 0.f, 0.f );
	static const LLMatrix4a y_90_neg = gGL.genRot( -90.f, 0.f, 1.f, 0.f );

	static const LLMatrix4a x_180 = gGL.genRot( 180.f, 1.f, 0.f, 0.f );
	static const LLMatrix4a y_180 = gGL.genRot( 180.f, 0.f, 1.f, 0.f );
	static const LLMatrix4a z_180 = gGL.genRot( 180.f, 0.f, 0.f, 1.f );

	switch (face)
	{
		case 0: 

			gGL.rotatef(y_90);
			gGL.rotatef(x_180);
		break;
		case 2: 
			gGL.rotatef(x_90_neg);
		break;
		case 4:
			gGL.rotatef(y_180);
			gGL.rotatef(z_180);
		break;
		case 1: 
			gGL.rotatef(y_90_neg);
			gGL.rotatef(x_180);
		break;
		case 3:
			gGL.rotatef(x_90);
		break;
		case 5: 
			gGL.rotatef(z_180);
		break;
	}
}

void validate_framebuffer_object()
{                                                           
	GLenum status;                                            
	status = glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT); 
	switch(status) 
	{                                          
		case GL_FRAMEBUFFER_COMPLETE:                       
			//framebuffer OK, no error.
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			// frame buffer not OK: probably means unsupported depth buffer format
			LL_ERRS() << "Framebuffer Incomplete Missing Attachment." << LL_ENDL;
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:	//May not work on mac. Remove/ifdef if that's the case, for now. GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS missing from glext.h.
			// frame buffer not OK: probably means unsupported depth buffer format
			LL_ERRS() << "Framebuffer Incomplete Dimensions." << LL_ENDL;
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			// frame buffer not OK: probably means unsupported depth buffer format
			LL_ERRS() << "Framebuffer Incomplete Attachment." << LL_ENDL;
			break; 
		case GL_FRAMEBUFFER_UNSUPPORTED:                    
			/* choose different formats */                        
			LL_ERRS() << "Framebuffer unsupported." << LL_ENDL;
			break;                                                
		default:                                                
			LL_ERRS() << "Unknown framebuffer status." << LL_ENDL;
			break;
	}
}

void LLPipeline::bindScreenToTexture() 
{
	
}

static LLTrace::BlockTimerStatHandle FTM_RENDER_BLOOM("Bloom");
void LLPipeline::renderBloom(BOOL for_snapshot, F32 zoom_factor, int subfield, bool tiling)
{
	if (!(LLGLSLShader::sNoFixedFunction &&
		sRenderGlow))
	{
		return;
	}
	
	static const LLCachedControl<U32> RenderResolutionDivisor("RenderResolutionDivisor",1);
	static const LLCachedControl<F32> RenderGlowMinLuminance("RenderGlowMinLuminance",2.5);
	static const LLCachedControl<F32> RenderGlowMaxExtractAlpha("RenderGlowMaxExtractAlpha",0.065f);
	static const LLCachedControl<F32> RenderGlowWarmthAmount("RenderGlowWarmthAmount",0.0f);
	static const LLCachedControl<LLVector3> RenderGlowLumWeights("RenderGlowLumWeights",LLVector3(.299f,.587f,.114f));
	static const LLCachedControl<LLVector3> RenderGlowWarmthWeights("RenderGlowWarmthWeights",LLVector3(1.f,.5f,.7f));
	static const LLCachedControl<S32> RenderGlowResolutionPow("RenderGlowResolutionPow",9);
	static const LLCachedControl<S32> RenderGlowIterations("RenderGlowIterations",2);
	static const LLCachedControl<F32> RenderGlowWidth("RenderGlowWidth",1.3f);
	static const LLCachedControl<F32> RenderGlowStrength("RenderGlowStrength",.35f);
	static const LLCachedControl<bool> RenderDepthOfField("RenderDepthOfField",false);
	static const LLCachedControl<F32> CameraFocusTransitionTime("CameraFocusTransitionTime",.5f);
	static const LLCachedControl<F32> CameraFNumber("CameraFNumber",9.f);
	static const LLCachedControl<F32> CameraFocalLength("CameraFocalLength",50.f);
	static const LLCachedControl<F32> CameraFieldOfView("CameraFieldOfView",60.f);
	static const LLCachedControl<F32> CameraMaxCoF("CameraMaxCoF",10.0f);
	static const LLCachedControl<F32> CameraDoFResScale("CameraDoFResScale",.7f);
	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
			
	LLVertexBuffer::unbind();
	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();

	assertInitialized();

	if (gUseWireframe)
	{
		gGL.setPolygonMode(LLRender::PF_FRONT_AND_BACK, LLRender::PM_FILL);
	}

	//U32 res_mod = RenderResolutionDivisor;//.get();	

	/*if (res_mod > 1)
	{
		tc2 /= (F32) res_mod;
	}*/

	LL_RECORD_BLOCK_TIME(FTM_RENDER_BLOOM);
	gGL.color4f(1,1,1,1);
	LLGLDepthTest depth(GL_FALSE);
	LLGLDisable<GL_BLEND> blend;
	LLGLDisable<GL_CULL_FACE> cull;
	
	LLGLState<GL_LIGHTING> light_state;
	enableLightsFullbright(light_state);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	LLGLDisable<GL_ALPHA_TEST> test;

	gGL.setColorMask(true, true);
	glClearColor(0,0,0,0);

	//static LLRender::Context sOldContext = LLRender::Context();
	//const LLRender::Context& newContext = gGL.getContextSnapshot();
	//if (memcmp(&sOldContext, &newContext, sizeof(LLRender::Context)) != 0)
	//{
		//newContext.printDiff(sOldContext);
	//}
	//sOldContext = newContext;

	if (tiling && !LLPipeline::sRenderDeferred) //Need to coax this into working with deferred now that tiling is back.
	{
		gGlowCombineProgram.bind();

		gGL.getTexUnit(0)->bind(&mGlow[1]);
		{
			//LLGLEnable stencil(GL_STENCIL_TEST);
			//glStencilFunc(GL_NOTEQUAL, 255, 0xFFFFFFFF);
			//glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
			//LLGLDisable<GL_BLEND> blend;

			// If the snapshot is constructed from tiles, calculate which
			// tile we're in.
			const S32 num_horizontal_tiles = llceil(zoom_factor);
			const LLVector2 tile(subfield % num_horizontal_tiles,
								 (S32)(subfield / num_horizontal_tiles));
			llassert(zoom_factor > 0.0); // Non-zero, non-negative.
			const F32 tile_size = 1.0/zoom_factor;
			
			LLVector2 tc1 = tile*tile_size; // Top left texture coordinates
			LLVector2 tc2 = (tile+LLVector2(1,1))*tile_size; // Bottom right texture coordinates
			
			LLGLEnable<GL_BLEND> blend;
			gGL.setSceneBlendType(LLRender::BT_ADD);
						
			LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(AUX_VB_MASK, 0);
			buff->allocateBuffer(4, 0, true);
			LLStrider<LLVector3> vert;
			LLStrider<LLVector2> texcoord0, texcoord1;
			buff->getVertexStrider(vert);
			buff->getTexCoord0Strider(texcoord0);
			buff->getTexCoord1Strider(texcoord1);

			vert[0].set(-1.f,-1.f,0.f);
			vert[1].set(-1.f,1.f,0.f);
			vert[2].set(1.f,-1.f,0.f);
			vert[3].set(1.f,1.f,0.f);

			//Texcoord 0 is actually for texture 1, which is unbound and thus all components = 0,0,0,0. Just zero out the texcoords.
			texcoord0[0] = texcoord0[1] = texcoord0[2] = texcoord0[3] = LLVector2::zero;

			texcoord1[0].set(tc1.mV[0], tc1.mV[1]);
			texcoord1[1].set(tc1.mV[0], tc2.mV[1]);
			texcoord1[2].set(tc2.mV[0], tc1.mV[1]);
			texcoord1[3].set(tc2.mV[0], tc2.mV[1]);

			buff->setBuffer(AUX_VB_MASK);
			buff->drawArrays(LLRender::TRIANGLE_STRIP, 0, 4);

			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}

		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		gGlowCombineProgram.unbind();

		gGL.flush();
		return;
	}
	
	{
		{
			LL_RECORD_BLOCK_TIME(FTM_RENDER_BLOOM_FBO);
			mGlow[1].bindTarget();
			mGlow[1].clear();
		}
		
		gGlowExtractProgram.bind();	
		F32 minLum = llmax((F32) RenderGlowMinLuminance/*.get()*/, 0.0f);
		F32 maxAlpha = RenderGlowMaxExtractAlpha;		
		F32 warmthAmount = RenderGlowWarmthAmount;	
		LLVector3 lumWeights = RenderGlowLumWeights;//.get();
		LLVector3 warmthWeights = RenderGlowWarmthWeights;//.get();


		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MIN_LUMINANCE, minLum);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_MAX_EXTRACT_ALPHA, maxAlpha);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_LUM_WEIGHTS, lumWeights.mV[0], lumWeights.mV[1], lumWeights.mV[2]);
		gGlowExtractProgram.uniform3f(LLShaderMgr::GLOW_WARMTH_WEIGHTS, warmthWeights.mV[0], warmthWeights.mV[1], warmthWeights.mV[2]);
		gGlowExtractProgram.uniform1f(LLShaderMgr::GLOW_WARMTH_AMOUNT, warmthAmount);
		LLGLEnable<GL_BLEND> blend_on;
		LLGLEnable<GL_ALPHA_TEST> test;
		
		gGL.setSceneBlendType(LLRender::BT_ADD_WITH_ALPHA);

		LLRenderTarget& render_target = LLPipeline::sRenderDeferred ? mFinalScreen : mScreen;
		render_target.bindTexture(0, 0);
		
		gGL.color4f(1,1,1,1);
		LLGLState<GL_LIGHTING> light_state;
		gPipeline.enableLightsFullbright(light_state);

		drawFullScreenRect();
		
		gGL.getTexUnit(0)->unbind(render_target.getUsage());

		mGlow[1].flush();
	}

	// power of two between 1 and 1024
	U32 glowResPow = RenderGlowResolutionPow;
	const U32 glow_res = llmax(1, 
		llmin(1024, 1 << glowResPow));

	S32 kernel = RenderGlowIterations*2;
	F32 delta = RenderGlowWidth * zoom_factor / glow_res;
	// Use half the glow width if we have the res set to less than 9 so that it looks
	// almost the same in either case.
	if (glowResPow < 9)
	{
		delta *= 0.5f;
	}
	F32 strength = RenderGlowStrength;

	gGlowProgram.bind();
	gGlowProgram.uniform1f(LLShaderMgr::GLOW_STRENGTH, strength);

	for (S32 i = 0; i < kernel; i++)
	{
		{
			LL_RECORD_BLOCK_TIME(FTM_RENDER_BLOOM_FBO);
			mGlow[i%2].bindTarget();
			mGlow[i%2].clear();
		}
			
		gGL.getTexUnit(0)->bind(&mGlow[(i+1)%2]);

		if (i%2 == 0)
		{
			gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, delta, 0);
		}
		else
		{
			gGlowProgram.uniform2f(LLShaderMgr::GLOW_DELTA, 0, delta);
		}

		drawFullScreenRect();
		
		mGlow[i%2].flush();
	}

	gGlowProgram.unbind();

	/*if (LLRenderTarget::sUseFBO)
	{
		LL_RECORD_BLOCK_TIME(FTM_RENDER_BLOOM_FBO);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}*/

	gGLViewport = gViewerWindow->getWorldViewRectRaw();
	gGL.setViewport(gGLViewport);

	gGL.flush();
	
	LLVertexBuffer::unbind();

	if (LLPipeline::sRenderDeferred)
	{

		bool dof_enabled = !LLViewerCamera::getInstance()->cameraUnderWater() &&
							!LLToolMgr::getInstance()->inBuildMode() &&
							RenderDepthOfField;


		bool multisample = RenderFSAASamples > 1 && mFXAABuffer.isComplete();

		gViewerWindow->setup3DViewport();
				
		if (dof_enabled)
		{
			LLGLSLShader* shader = &gDeferredPostProgram;
			LLGLDisable<GL_BLEND> blend;

			//depth of field focal plane calculations
			static F32 current_distance = 16.f;
			static F32 start_distance = 16.f;
			static F32 transition_time = 1.f;

			LLVector3 focus_point;

			LLViewerObject* obj = LLViewerMediaFocus::getInstance()->getFocusedObject();
			if (obj && obj->mDrawable && obj->isSelected())
			{ //focus on selected media object
				S32 face_idx = LLViewerMediaFocus::getInstance()->getFocusedFace();
				if (obj && obj->mDrawable)
				{
					LLFace* face = obj->mDrawable->getFace(face_idx);
					if (face)
					{
						focus_point = face->getPositionAgent();
					}
				}
			}
		
			if (focus_point.isExactlyZero())
			{
				if (LLViewerJoystick::getInstance()->getOverrideCamera())
				{ //focus on point under cursor
					focus_point.set(gDebugRaycastIntersection.getF32ptr());
				}
				else if (gAgentCamera.cameraMouselook())
				{ //focus on point under mouselook crosshairs
					LLVector4a result;
					result.clear();

					gViewerWindow->cursorIntersect(-1, -1, 512.f, NULL, -1, FALSE, FALSE,
													NULL,
													&result);

					focus_point.set(result.getF32ptr());
				}
				else if(gAgent.getRegion())
				{
					//focus on alt-zoom target
					focus_point = LLVector3(gAgentCamera.getFocusGlobal()-gAgent.getRegion()->getOriginGlobal());
				}
			}

			LLVector3 eye = LLViewerCamera::getInstance()->getOrigin();
			F32 target_distance = 16.f;
			if (!focus_point.isExactlyZero())
			{
				target_distance = LLViewerCamera::getInstance()->getAtAxis() * (focus_point-eye);
			}

			if (transition_time >= 1.f &&
				fabsf(current_distance-target_distance)/current_distance > 0.01f)
			{ //large shift happened, interpolate smoothly to new target distance
				transition_time = 0.f;
				start_distance = current_distance;
			}
			else if (transition_time < 1.f)
			{ //currently in a transition, continue interpolating		
				transition_time += 1.f/CameraFocusTransitionTime*gFrameIntervalSeconds;
				transition_time = llmin(transition_time, 1.f);

				F32 t = cosf(transition_time*F_PI+F_PI)*0.5f+0.5f;
				current_distance = start_distance + (target_distance-start_distance)*t;
			}
			else
			{ //small or no change, just snap to target distance
				current_distance = target_distance;
			}

			//convert to mm
			F32 subject_distance = current_distance*1000.f;
			F32 fnumber = CameraFNumber;
			F32 default_focal_length = CameraFocalLength;

			F32 fov = LLViewerCamera::getInstance()->getView();
		
			const F32 default_fov = CameraFieldOfView * F_PI/180.f;
			//const F32 default_aspect_ratio = gSavedSettings.getF32("CameraAspectRatio");
		
			//F32 aspect_ratio = (F32) mScreen.getWidth()/(F32)mScreen.getHeight();
		
			F32 dv = 2.f*default_focal_length * tanf(default_fov/2.f);
			//F32 dh = 2.f*default_focal_length * tanf(default_fov*default_aspect_ratio/2.f);

			F32 focal_length = dv/(2*tanf(fov/2.f));
		 
			//F32 tan_pixel_angle = tanf(LLDrawable::sCurPixelAngle);
	
			// from wikipedia -- c = |s2-s1|/s2 * f^2/(N(S1-f))
			// where	 N = fnumber
			//			 s2 = dot distance
			//			 s1 = subject distance
			//			 f = focal length
			//	

			F32 blur_constant = focal_length*focal_length/(fnumber*(subject_distance-focal_length));
			blur_constant /= 1000.f; //convert to meters for shader
			F32 magnification = focal_length/(subject_distance-focal_length);

			{ //build diffuse+bloom+CoF
				mDeferredLight.bindTarget();
				shader = &gDeferredCoFProgram;

				bindDeferredShader(*shader, &mFinalScreen);

				shader->uniform1f(LLShaderMgr::DOF_FOCAL_DISTANCE, -subject_distance/1000.f);
				shader->uniform1f(LLShaderMgr::DOF_BLUR_CONSTANT, blur_constant);
				shader->uniform1f(LLShaderMgr::DOF_TAN_PIXEL_ANGLE, tanf(1.f/LLDrawable::sCurPixelAngle));
				shader->uniform1f(LLShaderMgr::DOF_MAGNIFICATION, magnification);
				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);

				drawFullScreenRect();

				unbindDeferredShader(*shader, &mFinalScreen);
				mDeferredLight.flush();
			}

			U32 dof_width = (U32) (mScreen.getWidth()*CameraDoFResScale);
			U32 dof_height = (U32) (mScreen.getHeight()*CameraDoFResScale);
			
			{ //perform DoF sampling at half-res (preserve alpha channel)
				mFinalScreen.bindTarget();
				gGL.setViewport(0,0, dof_width, dof_height);
				gGL.setColorMask(true, false);

				shader = &gDeferredPostProgram;
				bindDeferredShader(*shader, &mDeferredLight);

				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);

				drawFullScreenRect();

				unbindDeferredShader(*shader, &mDeferredLight);
				mFinalScreen.flush();
				gGL.setColorMask(true, true);
			}
	
			{ //combine result based on alpha
				if (multisample)
				{
					mScreen.bindTarget();
					gGL.setViewport(0, 0, mDeferredScreen.getWidth(), mDeferredScreen.getHeight());
				}
				else
				{
					gGLViewport = gViewerWindow->getWorldViewRectRaw();
					gGL.setViewport(gGLViewport);
				}

				shader = &gDeferredDoFCombineProgram;
				bindDeferredShader(*shader, &mFinalScreen);
				
				S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
				if (channel > -1)
				{
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
				}
				
				if (!LLViewerCamera::getInstance()->cameraUnderWater())
				{
					shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 2.2f);
				} else {
					shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 1.0f);
				}

				shader->uniform1f(LLShaderMgr::DOF_MAX_COF, CameraMaxCoF);
				shader->uniform1f(LLShaderMgr::DOF_RES_SCALE, CameraDoFResScale);
				shader->uniform1f(LLShaderMgr::DOF_WIDTH, CameraDoFResScale - CameraDoFResScale / dof_width);
				shader->uniform1f(LLShaderMgr::DOF_HEIGHT, CameraDoFResScale - CameraDoFResScale / dof_height);

				drawFullScreenRect();

				if (channel > -1)
				{
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
				}

				unbindDeferredShader(*shader, &mFinalScreen);

				if (multisample)
				{
					mScreen.flush();
				}
			}
		}
		else
		{
			if (multisample)
			{
				mDeferredLight.bindTarget();
			}
			LLGLSLShader* shader = &gDeferredPostNoDoFProgram;
			
			bindDeferredShader(*shader, &mFinalScreen);
							
			S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
			if (channel > -1)
			{
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			}
			
			if (!LLViewerCamera::getInstance()->cameraUnderWater())
			{
				shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 2.2f);
			} else {
				shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 1.0f);
			}

			drawFullScreenRect();

			if (channel > -1)
			{
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			}

			unbindDeferredShader(*shader, &mFinalScreen);

			if (multisample)
			{
				mDeferredLight.flush();
			}
		}

		if (multisample)
		{
			//bake out texture2D with RGBL for FXAA shader
			mFXAABuffer.bindTarget();
			
			LLRenderTarget& render_target = dof_enabled ? mScreen : mDeferredLight;
			S32 width = render_target.getWidth();
			S32 height = render_target.getHeight();
			gGL.setViewport(0, 0, width, height);

			LLGLSLShader* shader = &gGlowCombineFXAAProgram;

			bindDeferredShader(*shader, &render_target);

			S32 channel = shader->enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, render_target.getUsage());
			if (channel > -1)
			{
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			}
						
			drawFullScreenRect();

			if (channel > -1)
			{
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			}

			unbindDeferredShader(*shader, &render_target);
			
			mFXAABuffer.flush();

			shader = &gFXAAProgram;
			shader->bind();

			channel = shader->enableTexture(LLShaderMgr::DIFFUSE_MAP, mFXAABuffer.getUsage());
			if (channel > -1)
			{
				mFXAABuffer.bindTexture(0, channel);
				gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			}
			
			gGLViewport = gViewerWindow->getWorldViewRectRaw();
			gGL.setViewport(gGLViewport);

			shader->uniform2f(LLShaderMgr::FXAA_RCP_SCREEN_RES, 1.f/width, 1.f/height);
			shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT, -0.5f/width, -0.5f/height, 0.5f/width, 0.5f/height);
			shader->uniform4f(LLShaderMgr::FXAA_RCP_FRAME_OPT2, -2.f/width, -2.f/height, 2.f/width, 2.f/height);
			
			drawFullScreenRect();

			shader->unbind();
		}
	}
	else
	{
		/*if (res_mod > 1)
		{
			tc2 /= (F32) res_mod;
		}*/

		LLGLDisable<GL_BLEND> blend;

		if (LLGLSLShader::sNoFixedFunction)
		{
			gGlowCombineProgram.bind();
		}
		else
		{
			//tex unit 0
			gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_COLOR);
			//tex unit 1
			gGL.getTexUnit(1)->setTextureColorBlend(LLTexUnit::TBO_ADD, LLTexUnit::TBS_TEX_COLOR, LLTexUnit::TBS_PREV_COLOR);
		}
		
		gGL.getTexUnit(0)->bind(&mGlow[1]);
		gGL.getTexUnit(1)->bind(&mScreen);
		
		LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);
		
		drawFullScreenRect();
		
		if (LLGLSLShader::sNoFixedFunction)
		{
			gGlowCombineProgram.unbind();
		}
		else
		{
			gGL.getTexUnit(1)->disable();
			gGL.getTexUnit(1)->setTextureBlendType(LLTexUnit::TB_MULT);

			gGL.getTexUnit(0)->activate();
			gGL.getTexUnit(0)->setTextureBlendType(LLTexUnit::TB_MULT);
		}
		
	}

	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	if (hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
	{
		if (LLGLSLShader::sNoFixedFunction)
		{
			gSplatTextureRectProgram.bind();
		}

		gGL.setColorMask(true, false);

		LLGLEnable<GL_BLEND> blend;
		gGL.color4f(1,1,1,0.75f);

		gGL.getTexUnit(0)->bind(&mPhysicsDisplay);

		gGL.begin(LLRender::TRIANGLES);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2f(-1.f, -1.f);
		gGL.texCoord2f(0.f,2.f);
		gGL.vertex2f(-1.f,  3.f);
		gGL.texCoord2f(2.f, 0.f);
		gGL.vertex2f( 3.f, -1.f);
		gGL.end();
		gGL.flush();

		if (LLGLSLShader::sNoFixedFunction)
		{
			gSplatTextureRectProgram.unbind();
		}
	}

	
	if (mScreen.getFBO())
	{ //copy depth buffer from mScreen to framebuffer
		LLRenderTarget::copyContentsToFramebuffer(mScreen, 0, 0, mScreen.getWidth(), mScreen.getHeight(), 
			0, 0, mScreen.getWidth(), mScreen.getHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}
	

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	LLVertexBuffer::unbind();

	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();

}

static LLTrace::BlockTimerStatHandle FTM_BIND_DEFERRED("Bind Deferred");

void LLPipeline::bindDeferredShader(LLGLSLShader& shader, LLRenderTarget* diffuse_source, LLRenderTarget* light_source)
{
	if (shader.mShaderClass != LLViewerShaderMgr::SHADER_DEFERRED && shader.mShaderClass != LLViewerShaderMgr::SHADER_INTERFACE)
	{
		shader.bind();
		return;
	}
	LL_RECORD_BLOCK_TIME(FTM_BIND_DEFERRED);

	static const LLCachedControl<F32> RenderDeferredSunWash("RenderDeferredSunWash",.5f);
	static const LLCachedControl<F32> RenderShadowNoise("RenderShadowNoise",-.0001f);
	static const LLCachedControl<F32> RenderShadowBlurSize("RenderShadowBlurSize",.7f);
	static const LLCachedControl<F32> RenderSSAOScale("RenderSSAOScale",500);
	static const LLCachedControl<U32> RenderSSAOMaxScale("RenderSSAOMaxScale",200);
	static const LLCachedControl<F32> RenderSSAOFactor("RenderSSAOFactor",.3f);
	static const LLCachedControl<LLVector3> RenderSSAOEffect("RenderSSAOEffect",LLVector3(.4f,1.f,0.f));
	static const LLCachedControl<F32> RenderDeferredAlphaSoften("RenderDeferredAlphaSoften",.75f);
	static const LLCachedControl<F32> RenderShadowOffsetError("RenderShadowOffsetError",0.f);
	static const LLCachedControl<F32> RenderShadowBiasError("RenderShadowBiasError",0.f);
	static const LLCachedControl<F32> RenderShadowOffset("RenderShadowOffset",.1f);
	static const LLCachedControl<F32> RenderShadowBias("RenderShadowBias",0.f);
	static const LLCachedControl<F32> RenderSpotShadowOffset("RenderSpotShadowOffset",.4f);
	static const LLCachedControl<F32> RenderSpotShadowBias("RenderSpotShadowBias",0.f);
	static const LLCachedControl<F32> RenderEdgeDepthCutoff("RenderEdgeDepthCutoff",.01f);
	static const LLCachedControl<F32> RenderEdgeNormCutoff("RenderEdgeNormCutoff",.25f);
	static const LLCachedControl<F32> RenderSSAOResolutionScale("SHRenderSSAOResolutionScale",.5f);

	diffuse_source = diffuse_source ? diffuse_source : &mDeferredScreen;
	light_source = light_source ? light_source : &mDeferredLight;

	shader.bind();
	S32 channel = 0;
	channel = shader.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, diffuse_source->getUsage());
	if (channel > -1)
	{
		diffuse_source->bindTexture(0, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_SPECULAR, mDeferredScreen.getUsage());
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(1, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NORMAL, mDeferredScreen.getUsage());
	if (channel > -1)
	{
		mDeferredScreen.bindTexture(2, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	S32 channel2 = shader.enableTexture(LLShaderMgr::DEFERRED_DOWNSAMPLED_DEPTH, mDeferredDepth.getUsage());
	channel = shader.enableTexture(LLShaderMgr::DEFERRED_DEPTH, mDeferredDepth.getUsage());
	if (channel > -1 || channel2 >= -1)
	{
		if(channel > -1)
			gGL.getTexUnit(channel)->bind(&mDeferredDepth, TRUE);
		if(channel2 > -1)
		{
			F32 scale = llclamp(RenderSSAOResolutionScale.get(),.01f,1.f);
			if(scale < 1.f)
				gGL.getTexUnit(channel2)->bind(&mDeferredDownsampledDepth, TRUE);
			else
				gGL.getTexUnit(channel2)->bind(&mDeferredDepth, TRUE);	//Bind full res depth instead, as downsampling is disabled if scale == 1.f
		}

		//gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		stop_glerror();
		
		//glTexParameteri(LLTexUnit::getInternalType(mDeferredDepth.getUsage()), GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);		
		//glTexParameteri(LLTexUnit::getInternalType(mDeferredDepth.getUsage()), GL_DEPTH_TEXTURE_MODE_ARB, GL_ALPHA);		

		stop_glerror();

		LLMatrix4a inv_proj = glh_get_current_projection();
		inv_proj.invert();
		
		shader.uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX, 1, FALSE, inv_proj.getF32ptr());
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_NOISE);
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseMap->getTexName());
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);
	if (channel > -1)
	{
		gGL.getTexUnit(channel)->bindManual(LLTexUnit::TT_TEXTURE, mLightFunc->getTexName());
	}

	stop_glerror();

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_LIGHT, light_source->getUsage());
	if (channel > -1)
	{
		light_source->bindTexture(0, channel);
		gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	channel = shader.enableTexture(LLShaderMgr::DEFERRED_BLOOM);
	if (channel > -1)
	{
		mGlow[1].bindTexture(0, channel);
	}

	stop_glerror();

	for (U32 i = 0; i < 4; i++)
	{
		channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0+i);
		stop_glerror();
		if (channel > -1)
		{
			stop_glerror();
			gGL.getTexUnit(channel)->bind(&mShadow[i], TRUE);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
			stop_glerror();
		}
	}

	for (U32 i = 4; i < 6; i++)
	{
		channel = shader.enableTexture(LLShaderMgr::DEFERRED_SHADOW0+i);
		stop_glerror();
		if (channel > -1)
		{
			stop_glerror();
			gGL.getTexUnit(channel)->bind(&mShadow[i], TRUE);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			gGL.getTexUnit(channel)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();
			
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_COMPARE_R_TO_TEXTURE_ARB);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC_ARB, GL_LEQUAL);
			stop_glerror();
		}
	}

	stop_glerror();

	if(shader.getUniformLocation(LLShaderMgr::DEFERRED_SHADOW_MATRIX) >= 0)
	{
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_SHADOW_MATRIX, 6, FALSE, mSunShadowMatrix[0].getF32ptr());

		stop_glerror();
	}

	channel = shader.enableTexture(LLShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
	if (channel > -1)
	{
		LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
		if (cube_map)
		{
			cube_map->enable(channel);
			cube_map->bind();
			const F32* m = glh_get_current_modelview().getF32ptr();
						
			F32 mat[] = { m[0], m[1], m[2],
						  m[4], m[5], m[6],
						  m[8], m[9], m[10] };
		
			shader.uniformMatrix3fv(LLShaderMgr::DEFERRED_ENV_MAT, 1, TRUE, mat);
		}
	}

	shader.uniform4fv(LLShaderMgr::DEFERRED_SHADOW_CLIP, 1, mSunClipPlanes.mV);
	shader.uniform1f(LLShaderMgr::DEFERRED_SUN_WASH, RenderDeferredSunWash);
	shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_NOISE, RenderShadowNoise);
	shader.uniform1f(LLShaderMgr::DEFERRED_BLUR_SIZE, RenderShadowBlurSize);

	shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_RADIUS, RenderSSAOScale);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_MAX_RADIUS, RenderSSAOMaxScale);

	F32 ssao_factor = RenderSSAOFactor;
	shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR, ssao_factor);
	shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_FACTOR_INV, 1.f/ssao_factor);

	LLVector3 ssao_effect = RenderSSAOEffect;
	shader.uniform1f(LLShaderMgr::DEFERRED_SSAO_EFFECT, ssao_effect[0]);

	shader.uniform2f(LLShaderMgr::DEFERRED_KERN_SCALE, 1.f / mDeferredScreen.getWidth(), 1.f / mDeferredScreen.getHeight());
	shader.uniform2f(LLShaderMgr::DEFERRED_NOISE_SCALE, mDeferredScreen.getWidth() / NOISE_MAP_RES, mDeferredScreen.getHeight() / NOISE_MAP_RES);

	//F32 shadow_offset_error = 1.f + RenderShadowOffsetError * fabsf(LLViewerCamera::getInstance()->getOrigin().mV[2]);
	F32 shadow_bias_error = RenderShadowBiasError * fabsf(LLViewerCamera::getInstance()->getOrigin().mV[2])/3000.f;

	shader.uniform1f(LLShaderMgr::DEFERRED_NEAR_CLIP, LLViewerCamera::getInstance()->getNear()*2.f);
	shader.uniform1f (LLShaderMgr::DEFERRED_SHADOW_OFFSET, RenderShadowOffset); //*shadow_offset_error);
	shader.uniform1f(LLShaderMgr::DEFERRED_SHADOW_BIAS, RenderShadowBias+shadow_bias_error);
	shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_OFFSET, RenderSpotShadowOffset);
	shader.uniform1f(LLShaderMgr::DEFERRED_SPOT_SHADOW_BIAS, RenderSpotShadowBias);	

	shader.uniform3fv(LLShaderMgr::DEFERRED_SUN_DIR, 1, mTransformedSunDir.getF32ptr());
	shader.uniform2f(LLShaderMgr::DEFERRED_SHADOW_RES, mShadow[0].getWidth(), mShadow[0].getHeight());
	shader.uniform2f(LLShaderMgr::DEFERRED_PROJ_SHADOW_RES, mShadow[4].getWidth(), mShadow[4].getHeight());
	shader.uniform1f(LLShaderMgr::DEFERRED_DEPTH_CUTOFF, RenderEdgeDepthCutoff);
	shader.uniform1f(LLShaderMgr::DEFERRED_NORM_CUTOFF, RenderEdgeNormCutoff);
	

	if (shader.getUniformLocation(LLShaderMgr::DEFERRED_NORM_MATRIX) >= 0)
	{
		LLMatrix4a norm_mat = glh_get_current_modelview();
		norm_mat.invert();
		norm_mat.transpose();
		shader.uniformMatrix4fv(LLShaderMgr::DEFERRED_NORM_MATRIX, 1, FALSE, norm_mat.getF32ptr());
	}
}

static LLTrace::BlockTimerStatHandle FTM_GI_TRACE("Trace");
static LLTrace::BlockTimerStatHandle FTM_GI_GATHER("Gather");
static LLTrace::BlockTimerStatHandle FTM_SUN_SHADOW("Shadow Map");
static LLTrace::BlockTimerStatHandle FTM_SOFTEN_SHADOW("Shadow Soften");
static LLTrace::BlockTimerStatHandle FTM_EDGE_DETECTION("Find Edges");
static LLTrace::BlockTimerStatHandle FTM_LOCAL_LIGHTS("Local Lights");
static LLTrace::BlockTimerStatHandle FTM_ATMOSPHERICS("Atmospherics");
static LLTrace::BlockTimerStatHandle FTM_FULLSCREEN_LIGHTS("Fullscreen Lights");
static LLTrace::BlockTimerStatHandle FTM_PROJECTORS("Projectors");
static LLTrace::BlockTimerStatHandle FTM_POST("Post");


void LLPipeline::renderDeferredLighting()
{
	if (!sCull)
	{
		return;
	}

	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	static const LLCachedControl<bool> RenderDeferredSSAO("RenderDeferredSSAO",false);
	static const LLCachedControl<F32> RenderSSAOResolutionScale("SHRenderSSAOResolutionScale",.5f);
	static const LLCachedControl<S32> RenderShadowDetail("RenderShadowDetail",0);
	static const LLCachedControl<LLVector3> RenderShadowGaussian("RenderShadowGaussian",LLVector3(3.f,2.f,0.f));
	static const LLCachedControl<F32> RenderShadowBlurSize("RenderShadowBlurSize",1.4f);
	static const LLCachedControl<F32> RenderShadowBlurDistFactor("RenderShadowBlurDistFactor",.1f);
	static const LLCachedControl<bool> RenderDeferredAtmospheric("RenderDeferredAtmospheric",false);
	static const LLCachedControl<bool> RenderLocalLights("RenderLocalLights",false);

	{
		LL_RECORD_BLOCK_TIME(FTM_RENDER_DEFERRED);

		LLViewerCamera* camera = LLViewerCamera::getInstance();
		{
			LLGLDepthTest depth(GL_TRUE);
			mDeferredDepth.copyContents(mDeferredScreen, 0, 0, mDeferredScreen.getWidth(), mDeferredScreen.getHeight(),
							0, 0, mDeferredDepth.getWidth(), mDeferredDepth.getHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);	
		}

		LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);

		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		//ati doesn't seem to love actually using the stencil buffer on FBO's
		LLGLDisable<GL_STENCIL_TEST> stencil;
		//glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
		//glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		gGL.setColorMask(true, true);
		
		//draw a cube around every light
		LLVertexBuffer::unbind();

		LLGLEnable<GL_CULL_FACE> cull;
		LLGLEnable<GL_BLEND> blend;

		{
			mTransformedSunDir.load3(getSunDir().mV);
			glh_get_current_modelview().rotate(mTransformedSunDir,mTransformedSunDir);
		}

		llassert_always(LLGLState<GL_LIGHTING>::checkEnabled());

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		if (RenderDeferredSSAO)
		{
			F32 ssao_scale = llclamp(RenderSSAOResolutionScale.get(),.01f,1.f);

			LLGLDisable<GL_BLEND> blend;

			//Downsample with fullscreen quad. GL_NEAREST
			if(ssao_scale < 1.f)
			{
				mDeferredDownsampledDepth.bindTarget();
				mDeferredDownsampledDepth.clear(GL_DEPTH_BUFFER_BIT);
				bindDeferredShader(gDeferredDownsampleDepthNearestProgram);
				{
					LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
					drawFullScreenRect();
				}
				mDeferredDownsampledDepth.flush();
				unbindDeferredShader(gDeferredDownsampleDepthNearestProgram);
			}

			//Run SSAO
			{
				mScreen.bindTarget();
				glClearColor(1,1,1,1);
				mScreen.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0,0,0,0);
				bindDeferredShader(gDeferredSSAOProgram);
				if(ssao_scale < 1.f)
				{
					gGL.setViewport(0,0,mDeferredDownsampledDepth.getWidth(),mDeferredDownsampledDepth.getHeight());
				}
				{
					LLGLDepthTest depth(GL_FALSE);
					drawFullScreenRect();
				}
				mScreen.flush();
				unbindDeferredShader(gDeferredSSAOProgram);
			}
		}

		if (RenderDeferredSSAO || RenderShadowDetail > 0)
		{
			mDeferredLight.bindTarget();
			{ //paint shadow/SSAO light map (direct lighting lightmap)
				LL_RECORD_BLOCK_TIME(FTM_SUN_SHADOW);
				bindDeferredShader(gDeferredSunProgram);
				glClearColor(1,1,1,1);
				mDeferredLight.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0,0,0,0);

				F32 ssao_scale = llclamp(RenderSSAOResolutionScale.get(), .01f, 1.f);
				gDeferredSunProgram.uniform2f(LLShaderMgr::DEFERRED_SSAO_SCALE, ssao_scale, ssao_scale);

				//Enable bilinear filtering, as the screen tex resolution may not match current framebuffer resolution. Eg, half-res SSAO
				// diffuse map should only be found if the sun shader is the SSAO variant.
				S32 channel = gDeferredSunProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
				if (channel > -1)
				{
					mScreen.bindTexture(0,channel);
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
				}
				
				{
					LLGLDisable<GL_BLEND> blend;
					LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
					drawFullScreenRect();
				}

				if (channel > -1)
				{
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
				}
				
				unbindDeferredShader(gDeferredSunProgram);
			}
			mDeferredLight.flush();
		}

		static const LLCachedControl<bool> SHAlwaysSoftenShadows("SHAlwaysSoftenShadows",true);
		if (RenderDeferredSSAO || (RenderShadowDetail > 0 && SHAlwaysSoftenShadows))
		{ //soften direct lighting lightmap
			LL_RECORD_BLOCK_TIME(FTM_SOFTEN_SHADOW);
			//blur lightmap
			mScreen.bindTarget();
			glClearColor(1,1,1,1);
			mScreen.clear(GL_COLOR_BUFFER_BIT);
			glClearColor(0,0,0,0);
			
			bindDeferredShader(gDeferredBlurLightProgram);
			LLVector3 go = RenderShadowGaussian;
			const U32 kern_length = 4;
			F32 blur_size = RenderShadowBlurSize;
			F32 dist_factor = RenderShadowBlurDistFactor;

			gDeferredBlurLightProgram.uniform2f(sDelta, (blur_size * (kern_length / 2.f - 0.5f)) / mScreen.getWidth(), 0.f);
			gDeferredBlurLightProgram.uniform1f(sDistFactor, dist_factor);
		
			{
				LLGLDisable<GL_BLEND> blend;
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				drawFullScreenRect();
			}
			
			mScreen.flush();
			unbindDeferredShader(gDeferredBlurLightProgram);

			bindDeferredShader(gDeferredBlurLightProgram, &mScreen, &mScreen);
			mDeferredLight.bindTarget();

			gDeferredBlurLightProgram.uniform2f(sDelta, 0.f, (blur_size * (kern_length / 2.f - 0.5f)) / mScreen.getHeight());

			{
				LLGLDisable<GL_BLEND> blend;
				LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
				drawFullScreenRect();
			}
			mDeferredLight.flush();
			unbindDeferredShader(gDeferredBlurLightProgram, &mScreen, &mScreen);
		}

		stop_glerror();
		gGL.popMatrix();
		stop_glerror();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		stop_glerror();
		gGL.popMatrix();
		stop_glerror();

		mScreen.bindTarget();
		// clear color buffer here - zeroing alpha (glow) is important or it will accumulate against sky
		glClearColor(0,0,0,0);
		mScreen.clear(GL_COLOR_BUFFER_BIT);
		
		if (RenderDeferredAtmospheric)
		{ //apply sunlight contribution 
			LL_RECORD_BLOCK_TIME(FTM_ATMOSPHERICS);
			bindDeferredShader(LLPipeline::sUnderWaterRender ? gDeferredSoftenWaterProgram : gDeferredSoftenProgram);	
			{
				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable<GL_BLEND> blend;
				LLGLDisable<GL_ALPHA_TEST> test;

				//full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				drawFullScreenRect();

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}

			unbindDeferredShader(LLPipeline::sUnderWaterRender ? gDeferredSoftenWaterProgram : gDeferredSoftenProgram);
		}

		{ //render non-deferred geometry (fullbright, alpha, etc)
			LLGLDisable<GL_BLEND> blend;
			LLGLDisable<GL_STENCIL_TEST> stencil;
			gGL.setSceneBlendType(LLRender::BT_ALPHA);

			gPipeline.pushRenderTypeMask();
			
			gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY,
#if ENABLE_CLASSIC_CLOUDS
									LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS,
#endif
									LLPipeline::RENDER_TYPE_WL_CLOUDS,
									LLPipeline::RENDER_TYPE_WL_SKY,
									LLPipeline::END_RENDER_TYPES);
								
			
			renderGeomPostDeferred(*LLViewerCamera::getInstance(), false);
			gPipeline.popRenderTypeMask();
		}

		BOOL render_local = RenderLocalLights;
				
		if (render_local)
		{
			gGL.setSceneBlendType(LLRender::BT_ADD);
			std::list<LLVector4> fullscreen_lights;
			LLDrawable::drawable_list_t spot_lights;
			LLDrawable::drawable_list_t fullscreen_spot_lights;

			for (U32 i = 0; i < 2; i++)
			{
				mTargetShadowSpotLight[i] = NULL;
			}

			std::list<LLVector4> light_colors;

			LLVertexBuffer::unbind();

			{
				bindDeferredShader(gDeferredLightProgram);
				
				if (mCubeVB.isNull())
				{
					mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX, GL_STATIC_DRAW_ARB);
				}

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				for (LLDrawable::drawable_set_t::iterator iter = mLights.begin(); iter != mLights.end(); ++iter)
				{
					LLDrawable* drawablep = *iter;
					
					LLVOVolume* volume = drawablep->getVOVolume();
					if (!volume)
					{
						continue;
					}

					if (volume->isAttachment())
					{
						if (!sRenderAttachedLights)
						{
							continue;
						}
					}

					const LLViewerObject *vobj = drawablep->getVObj();
					if(vobj && vobj->getAvatar()
						&& (vobj->getAvatar()->isTooComplex()))
					{
						continue;
					}

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius()*1.5f;

					LLColor3 col = volume->getLightColor();

					if (col.magVecSquared() < 0.001f)
					{
						continue;
					}

					if (s <= 0.001f)
					{
						continue;
					}

					LLVector4a sa;
					sa.splat(s);
					if (camera->AABBInFrustumNoFarClip(center, sa) == 0)
					{
						continue;
					}

					sVisibleLightCount++;
									
					const auto& camera_origin = camera->getOrigin();
					if (camera_origin.mV[0] > c[0] + s + 0.2f ||
						camera_origin.mV[0] < c[0] - s - 0.2f ||
						camera_origin.mV[1] > c[1] + s + 0.2f ||
						camera_origin.mV[1] < c[1] - s - 0.2f ||
						camera_origin.mV[2] > c[2] + s + 0.2f ||
						camera_origin.mV[2] < c[2] - s - 0.2f)
					{ //draw box if camera is outside box
						if (render_local)
						{
							if (volume->isLightSpotlight())
							{
								drawablep->getVOVolume()->updateSpotLightPriority();
								spot_lights.push_back(drawablep);
								continue;
							}
							
							LL_RECORD_BLOCK_TIME(FTM_LOCAL_LIGHTS);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
							
							mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
							stop_glerror();
						}
					}
					else
					{	
						if (volume->isLightSpotlight())
						{
							drawablep->getVOVolume()->updateSpotLightPriority();
							fullscreen_spot_lights.push_back(drawablep);
							continue;
						}
						
						glh_get_current_modelview().affineTransform(center,center);
						
						LLVector4 tc(center.getF32ptr());
						tc.mV[VW] = s;
						fullscreen_lights.push_back(tc);
						light_colors.push_back(LLVector4(col.mV[0], col.mV[1], col.mV[2], volume->getLightFalloff()*0.5f));
					}
				}
				unbindDeferredShader(gDeferredLightProgram);
			}

			if (!spot_lights.empty())
			{
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				bindDeferredShader(gDeferredSpotLightProgram);

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				gDeferredSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::drawable_list_t::iterator iter = spot_lights.begin(); iter != spot_lights.end(); ++iter)
				{
					LL_RECORD_BLOCK_TIME(FTM_PROJECTORS);
					LLDrawable* drawablep = *iter;

					LLVOVolume* volume = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius()*1.5f;

					sVisibleLightCount++;

					setupSpotLight(gDeferredSpotLightProgram, drawablep);
					
					LLColor3 col = volume->getLightColor();
					
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
										
					mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
				}
				gDeferredSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(gDeferredSpotLightProgram);
			}

			{
				LLGLDepthTest depth(GL_FALSE);

				//full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				U32 count = 0;

				const U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
				LLVector4 light[max_count];
				LLVector4 col[max_count];

				F32 far_z = 0.f;

				while (!fullscreen_lights.empty())
				{
					LL_RECORD_BLOCK_TIME(FTM_FULLSCREEN_LIGHTS);
					light[count] = fullscreen_lights.front();
					fullscreen_lights.pop_front();
					col[count] = light_colors.front();
					light_colors.pop_front();
					
					/*col[count].mV[0] = powf(col[count].mV[0], 2.2f);
					col[count].mV[1] = powf(col[count].mV[1], 2.2f);
					col[count].mV[2] = powf(col[count].mV[2], 2.2f);*/
					
					far_z = llmin(light[count].mV[2]-light[count].mV[3], far_z);
					//col[count] = pow4fsrgb(col[count], 2.2f);
					count++;
					if (count == max_count || fullscreen_lights.empty())
					{
						U32 idx = count-1;
						bindDeferredShader(gDeferredMultiLightProgram[idx]);
						gDeferredMultiLightProgram[idx].uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT, count);
						gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT, count, (GLfloat*) light);
						gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT_COL, count, (GLfloat*) col);
						gDeferredMultiLightProgram[idx].uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z, far_z);
						far_z = 0.f;
						count = 0; 
						drawFullScreenRect();
						unbindDeferredShader(gDeferredMultiLightProgram[idx]);
					}
				}
				
				bindDeferredShader(gDeferredMultiSpotLightProgram);

				gDeferredMultiSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::drawable_list_t::iterator iter = fullscreen_spot_lights.begin(); iter != fullscreen_spot_lights.end(); ++iter)
				{
					LL_RECORD_BLOCK_TIME(FTM_PROJECTORS);
					LLDrawable* drawablep = *iter;
					
					LLVOVolume* volume = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					F32 s = volume->getLightRadius()*1.5f;

					sVisibleLightCount++;

					glh_get_current_modelview().affineTransform(center,center);
					
					setupSpotLight(gDeferredMultiSpotLightProgram, drawablep);

					LLColor3 col = volume->getLightColor();
					
					/*col.mV[0] = powf(col.mV[0], 2.2f);
					col.mV[1] = powf(col.mV[1], 2.2f);
					col.mV[2] = powf(col.mV[2], 2.2f);*/
					
					gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, center.getF32ptr());
					gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
					drawFullScreenRect();
				}

				gDeferredMultiSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(gDeferredMultiSpotLightProgram);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
		}

		gGL.setColorMask(true, true);
	}

	mScreen.flush();

	//gamma correct lighting
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);

		mFinalScreen.bindTarget();
		// Apply gamma correction to the frame here.
		gDeferredPostGammaCorrectProgram.bind();
		S32 channel = gDeferredPostGammaCorrectProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
		if (channel > -1)
		{
			mScreen.bindTexture(0,channel);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}
		
		drawFullScreenRect();
		
		gGL.getTexUnit(channel)->unbind(mScreen.getUsage());
		gDeferredPostGammaCorrectProgram.unbind();
		mFinalScreen.flush();
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();	

	mFinalScreen.bindTarget();

	{ //render non-deferred geometry (alpha, fullbright, glow)
		LLGLDisable<GL_BLEND> blend;
		LLGLDisable<GL_STENCIL_TEST> stencil;

		pushRenderTypeMask();
		andRenderTypeMask(LLPipeline::RENDER_TYPE_ALPHA,
						 LLPipeline::RENDER_TYPE_FULLBRIGHT,
						 LLPipeline::RENDER_TYPE_VOLUME,
						 LLPipeline::RENDER_TYPE_GLOW,
						 LLPipeline::RENDER_TYPE_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_SIMPLE,
						 LLPipeline::RENDER_TYPE_PASS_ALPHA,
						 LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_PASS_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_POST_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						 LLPipeline::RENDER_TYPE_PASS_GLOW,
						 LLPipeline::RENDER_TYPE_PASS_GRASS,
						 LLPipeline::RENDER_TYPE_PASS_SHINY,
						 LLPipeline::RENDER_TYPE_AVATAR,
						 LLPipeline::RENDER_TYPE_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						 END_RENDER_TYPES);
		
		renderGeomPostDeferred(*LLViewerCamera::getInstance());
		popRenderTypeMask();
	}

	{
		//render highlights, etc.
		renderHighlights();
		mHighlightFaces.clear();

		renderDebug();

		LLVertexBuffer::unbind();

		if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
		{
			// Render debugging beacons.
			gObjectList.renderObjectBeacons();
			gObjectList.resetObjectBeacons();
		}
	}

	mFinalScreen.flush();
						
}

void LLPipeline::renderDeferredLightingToRT(LLRenderTarget* target)
{
	if (!sCull)
	{
		return;
	}

	static const LLCachedControl<U32> RenderFSAASamples("RenderFSAASamples",0);
	static const LLCachedControl<bool> RenderDeferredSSAO("RenderDeferredSSAO",false);
	static const LLCachedControl<F32> RenderSSAOResolutionScale("SHRenderSSAOResolutionScale",.5f);
	static const LLCachedControl<S32> RenderShadowDetail("RenderShadowDetail",0);
	static const LLCachedControl<LLVector3> RenderShadowGaussian("RenderShadowGaussian",LLVector3(3.f,2.f,0.f));
	static const LLCachedControl<F32> RenderShadowBlurSize("RenderShadowBlurSize",1.4f);
	static const LLCachedControl<F32> RenderShadowBlurDistFactor("RenderShadowBlurDistFactor",.1f);
	static const LLCachedControl<bool> RenderDeferredAtmospheric("RenderDeferredAtmospheric",false);
	static const LLCachedControl<bool> RenderLocalLights("RenderLocalLights",false);

	{
		LL_RECORD_BLOCK_TIME(FTM_RENDER_DEFERRED);

		LLViewerCamera* camera = LLViewerCamera::getInstance();
		{
			LLGLDepthTest depth(GL_TRUE);
			mDeferredDepth.copyContents(mDeferredScreen, 0, 0, mDeferredScreen.getWidth(), mDeferredScreen.getHeight(),
							0, 0, mDeferredDepth.getWidth(), mDeferredDepth.getHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);	
		}

		LLGLEnable<GL_MULTISAMPLE_ARB> multisample(RenderFSAASamples > 0);

		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		//ati doesn't seem to love actually using the stencil buffer on FBO's
		LLGLDisable<GL_STENCIL_TEST> stencil;
		//glStencilFunc(GL_EQUAL, 1, 0xFFFFFFFF);
		//glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

		gGL.setColorMask(true, true);
		
		//draw a cube around every light
		LLVertexBuffer::unbind();

		LLGLEnable<GL_CULL_FACE> cull;
		LLGLEnable<GL_BLEND> blend;

		{
			mTransformedSunDir.load3(getSunDir().mV);
			glh_get_current_modelview().rotate(mTransformedSunDir,mTransformedSunDir);
		}

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		if (RenderDeferredSSAO)
		{
			F32 ssao_scale = llclamp(RenderSSAOResolutionScale.get(),.01f,1.f);

			LLGLDisable<GL_BLEND> blend;

			//Downsample with fullscreen quad. GL_NEAREST
			if(ssao_scale < 1.f)
			{
				mDeferredDownsampledDepth.bindTarget();
				mDeferredDownsampledDepth.clear(GL_DEPTH_BUFFER_BIT);
				bindDeferredShader(gDeferredDownsampleDepthNearestProgram);
				{
					LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
					drawFullScreenRect();
				}
				mDeferredDownsampledDepth.flush();
				unbindDeferredShader(gDeferredDownsampleDepthNearestProgram);
			}

			//Run SSAO
			{
				mScreen.bindTarget();
				glClearColor(1,1,1,1);
				mScreen.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0,0,0,0);
				bindDeferredShader(gDeferredSSAOProgram);
				if(ssao_scale < 1.f)
				{
					gGL.setViewport(0,0,mDeferredDownsampledDepth.getWidth(),mDeferredDownsampledDepth.getHeight());	
				}
				{
					LLGLDepthTest depth(GL_FALSE);
					drawFullScreenRect();
				}
				mScreen.flush();
				unbindDeferredShader(gDeferredSSAOProgram);
			}
		}

		if (RenderDeferredSSAO || RenderShadowDetail > 0)
		{
			mDeferredLight.bindTarget();
			{ //paint shadow/SSAO light map (direct lighting lightmap)
				LL_RECORD_BLOCK_TIME(FTM_SUN_SHADOW);
				bindDeferredShader(gDeferredSunProgram);

				F32 ssao_scale = llclamp(RenderSSAOResolutionScale.get(), .01f, 1.f);
				gDeferredSunProgram.uniform2f(LLShaderMgr::DEFERRED_SSAO_SCALE, ssao_scale, ssao_scale);

				glClearColor(1,1,1,1);
				mDeferredLight.clear(GL_COLOR_BUFFER_BIT);
				glClearColor(0,0,0,0);
				
				//Enable bilinear filtering, as the screen tex resolution may not match current framebuffer resolution. Eg, half-res SSAO
				// diffuse map should only be found if the sun shader is the SSAO variant.
				S32 channel = gDeferredSunProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
				if (channel > -1)
				{
					mScreen.bindTexture(0,channel);
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
				}
				
				{
					LLGLDisable<GL_BLEND> blend;
					LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_ALWAYS);
					drawFullScreenRect();
				}

				if (channel > -1)
				{
					gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
				}
				
				unbindDeferredShader(gDeferredSunProgram);
			}
			mDeferredLight.flush();
		}
				
		stop_glerror();
		gGL.popMatrix();
		stop_glerror();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		stop_glerror();
		gGL.popMatrix();
		stop_glerror();

		target->bindTarget();

		//clear color buffer here - zeroing alpha (glow) is important or it will accumulate against sky
		glClearColor(0,0,0,0);
		target->clear(GL_COLOR_BUFFER_BIT);
		
		if (RenderDeferredAtmospheric)
		{ //apply sunlight contribution 
			LL_RECORD_BLOCK_TIME(FTM_ATMOSPHERICS);
			bindDeferredShader(gDeferredSoftenProgram);	
			{
				LLGLDepthTest depth(GL_FALSE);
				LLGLDisable<GL_BLEND> blend;
				LLGLDisable<GL_ALPHA_TEST> test;

				//full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				drawFullScreenRect();

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}

			unbindDeferredShader(gDeferredSoftenProgram);
		}

		{ //render non-deferred geometry (fullbright, alpha, etc)
			LLGLDisable<GL_BLEND> blend;
			LLGLDisable<GL_STENCIL_TEST> stencil;
			gGL.setSceneBlendType(LLRender::BT_ALPHA);

			gPipeline.pushRenderTypeMask();
			
			gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY,
#if ENABLE_CLASSIC_CLOUDS
										LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS,
#endif
										LLPipeline::RENDER_TYPE_WL_CLOUDS,
										LLPipeline::RENDER_TYPE_WL_SKY,
										LLPipeline::END_RENDER_TYPES);
								
			
			renderGeomPostDeferred(*LLViewerCamera::getInstance(), false);
			gPipeline.popRenderTypeMask();
		}

		BOOL render_local = RenderLocalLights;
				
		if (render_local)
		{
			gGL.setSceneBlendType(LLRender::BT_ADD);
			std::list<LLVector4> fullscreen_lights;
			LLDrawable::drawable_list_t spot_lights;
			LLDrawable::drawable_list_t fullscreen_spot_lights;

			for (U32 i = 0; i < 2; i++)
			{
				mTargetShadowSpotLight[i] = NULL;
			}

			std::list<LLVector4> light_colors;

			LLVertexBuffer::unbind();

			{
				bindDeferredShader(gDeferredLightProgram);
				
				if (mCubeVB.isNull())
				{
					mCubeVB = ll_create_cube_vb(LLVertexBuffer::MAP_VERTEX, GL_STATIC_DRAW_ARB);
				}

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
				
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				for (LLDrawable::drawable_set_t::iterator iter = mLights.begin(); iter != mLights.end(); ++iter)
				{
					LLDrawable* drawablep = *iter;
					
					LLVOVolume* volume = drawablep->getVOVolume();
					if (!volume)
					{
						continue;
					}

					if (volume->isAttachment())
					{
						if (!sRenderAttachedLights)
						{
							continue;
						}
					}


					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius()*1.5f;

					LLColor3 col = volume->getLightColor();
					
					if (col.magVecSquared() < 0.001f)
					{
						continue;
					}

					if (s <= 0.001f)
					{
						continue;
					}

					LLVector4a sa;
					sa.splat(s);
					if (camera->AABBInFrustumNoFarClip(center, sa) == 0)
					{
						continue;
					}

					sVisibleLightCount++;
									
					const auto& camera_origin = camera->getOrigin();
					if (camera_origin.mV[0] > c[0] + s + 0.2f ||
						camera_origin.mV[0] < c[0] - s - 0.2f ||
						camera_origin.mV[1] > c[1] + s + 0.2f ||
						camera_origin.mV[1] < c[1] - s - 0.2f ||
						camera_origin.mV[2] > c[2] + s + 0.2f ||
						camera_origin.mV[2] < c[2] - s - 0.2f)
					{ //draw box if camera is outside box
						if (render_local)
						{
							if (volume->isLightSpotlight())
							{
								drawablep->getVOVolume()->updateSpotLightPriority();
								spot_lights.push_back(drawablep);
								continue;
							}
							
							/*col.mV[0] = powf(col.mV[0], 2.2f);
							col.mV[1] = powf(col.mV[1], 2.2f);
							col.mV[2] = powf(col.mV[2], 2.2f);*/
							
							LL_RECORD_BLOCK_TIME(FTM_LOCAL_LIGHTS);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
							gDeferredLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
							gDeferredLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
							
							mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
							stop_glerror();
						}
					}
					else
					{	
						if (volume->isLightSpotlight())
						{
							drawablep->getVOVolume()->updateSpotLightPriority();
							fullscreen_spot_lights.push_back(drawablep);
							continue;
						}

						glh_get_current_modelview().affineTransform(center,center);
						
						LLVector4 tc(center.getF32ptr());
						tc.mV[VW] = s;
						fullscreen_lights.push_back(tc);
						light_colors.push_back(LLVector4(col.mV[0], col.mV[1], col.mV[2], volume->getLightFalloff()*0.5f));
					}
				}
				unbindDeferredShader(gDeferredLightProgram);
			}

			if (!spot_lights.empty())
			{
				LLGLDepthTest depth(GL_TRUE, GL_FALSE);
				bindDeferredShader(gDeferredSpotLightProgram);

				mCubeVB->setBuffer(LLVertexBuffer::MAP_VERTEX);

				gDeferredSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::drawable_list_t::iterator iter = spot_lights.begin(); iter != spot_lights.end(); ++iter)
				{
					LL_RECORD_BLOCK_TIME(FTM_PROJECTORS);
					LLDrawable* drawablep = *iter;

					LLVOVolume* volume = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					const F32* c = center.getF32ptr();
					F32 s = volume->getLightRadius()*1.5f;

					sVisibleLightCount++;

					setupSpotLight(gDeferredSpotLightProgram, drawablep);
					
					LLColor3 col = volume->getLightColor();
					/*col.mV[0] = powf(col.mV[0], 2.2f);
					col.mV[1] = powf(col.mV[1], 2.2f);
					col.mV[2] = powf(col.mV[2], 2.2f);*/
					
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, c);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					gDeferredSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					gDeferredSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
										
					mCubeVB->drawRange(LLRender::TRIANGLE_FAN, 0, 7, 8, get_box_fan_indices(camera, center));
				}
				gDeferredSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(gDeferredSpotLightProgram);
			}

			{
				LLGLDepthTest depth(GL_FALSE);

				//full screen blit
				gGL.pushMatrix();
				gGL.loadIdentity();
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.pushMatrix();
				gGL.loadIdentity();

				U32 count = 0;

				const U32 max_count = LL_DEFERRED_MULTI_LIGHT_COUNT;
				LLVector4 light[max_count];
				LLVector4 col[max_count];

				F32 far_z = 0.f;

				while (!fullscreen_lights.empty())
				{
					LL_RECORD_BLOCK_TIME(FTM_FULLSCREEN_LIGHTS);
					light[count] = fullscreen_lights.front();
					fullscreen_lights.pop_front();
					col[count] = light_colors.front();
					light_colors.pop_front();
					
					/*col[count].mV[0] = powf(col[count].mV[0], 2.2f);
					col[count].mV[1] = powf(col[count].mV[1], 2.2f);
					col[count].mV[2] = powf(col[count].mV[2], 2.2f);*/
					
					far_z = llmin(light[count].mV[2]-light[count].mV[3], far_z);
					//col[count] = pow4fsrgb(col[count], 2.2f);
					count++;
					if (count == max_count || fullscreen_lights.empty())
					{
						U32 idx = count-1;
						bindDeferredShader(gDeferredMultiLightProgram[idx]);
						gDeferredMultiLightProgram[idx].uniform1i(LLShaderMgr::MULTI_LIGHT_COUNT, count);
						gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT, count, (GLfloat*) light);
						gDeferredMultiLightProgram[idx].uniform4fv(LLShaderMgr::MULTI_LIGHT_COL, count, (GLfloat*) col);
						gDeferredMultiLightProgram[idx].uniform1f(LLShaderMgr::MULTI_LIGHT_FAR_Z, far_z);
						far_z = 0.f;
						count = 0; 
						drawFullScreenRect();
					}
				}
				
				unbindDeferredShader(gDeferredMultiLightProgram[0]);

				bindDeferredShader(gDeferredMultiSpotLightProgram);

				gDeferredMultiSpotLightProgram.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

				for (LLDrawable::drawable_list_t::iterator iter = fullscreen_spot_lights.begin(); iter != fullscreen_spot_lights.end(); ++iter)
				{
					LL_RECORD_BLOCK_TIME(FTM_PROJECTORS);
					LLDrawable* drawablep = *iter;
					
					LLVOVolume* volume = drawablep->getVOVolume();

					LLVector4a center;
					center.load3(drawablep->getPositionAgent().mV);
					F32 s = volume->getLightRadius()*1.5f;

					sVisibleLightCount++;

					glh_get_current_modelview().affineTransform(center,center);
					
					setupSpotLight(gDeferredMultiSpotLightProgram, drawablep);

					LLColor3 col = volume->getLightColor();
					
					/*col.mV[0] = powf(col.mV[0], 2.2f);
					col.mV[1] = powf(col.mV[1], 2.2f);
					col.mV[2] = powf(col.mV[2], 2.2f);*/
					
					gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::LIGHT_CENTER, 1, center.getF32ptr());
					gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_SIZE, s);
					gDeferredMultiSpotLightProgram.uniform3fv(LLShaderMgr::DIFFUSE_COLOR, 1, col.mV);
					gDeferredMultiSpotLightProgram.uniform1f(LLShaderMgr::LIGHT_FALLOFF, volume->getLightFalloff()*0.5f);
					drawFullScreenRect();
				}

				gDeferredMultiSpotLightProgram.disableTexture(LLShaderMgr::DEFERRED_PROJECTION);
				unbindDeferredShader(gDeferredMultiSpotLightProgram);

				gGL.popMatrix();
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.popMatrix();
			}
		}

		gGL.setColorMask(true, true);
	}

	mScreen.flush();

	//gamma correct lighting
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);

		mFinalScreen.bindTarget();
		// Apply gamma correction to the frame here.
		gDeferredPostGammaCorrectProgram.bind();
		S32 channel = 0;
		channel = gDeferredPostGammaCorrectProgram.enableTexture(LLShaderMgr::DEFERRED_DIFFUSE, mScreen.getUsage());
		if (channel > -1)
		{
			mScreen.bindTexture(0,channel);
			gGL.getTexUnit(channel)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
		}
		
		drawFullScreenRect();
		
		gGL.getTexUnit(channel)->unbind(mScreen.getUsage());
		gDeferredPostGammaCorrectProgram.unbind();
		mFinalScreen.flush();
	}

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();	

	mFinalScreen.bindTarget();

	{ //render non-deferred geometry (alpha, fullbright, glow)
		LLGLDisable<GL_BLEND> blend;
		LLGLDisable<GL_STENCIL_TEST> stencil;

		pushRenderTypeMask();
		andRenderTypeMask(LLPipeline::RENDER_TYPE_ALPHA,
						 LLPipeline::RENDER_TYPE_FULLBRIGHT,
						 LLPipeline::RENDER_TYPE_VOLUME,
						 LLPipeline::RENDER_TYPE_GLOW,
						 LLPipeline::RENDER_TYPE_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_SIMPLE,
						 LLPipeline::RENDER_TYPE_PASS_ALPHA,
						 LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_PASS_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_POST_BUMP,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
						 LLPipeline::RENDER_TYPE_PASS_GLOW,
						 LLPipeline::RENDER_TYPE_PASS_GRASS,
						 LLPipeline::RENDER_TYPE_PASS_SHINY,
						 LLPipeline::RENDER_TYPE_AVATAR,
						 LLPipeline::RENDER_TYPE_ALPHA_MASK,
						 LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
						 END_RENDER_TYPES);
		
		renderGeomPostDeferred(*LLViewerCamera::getInstance());
		popRenderTypeMask();
	}

					
}

void LLPipeline::setupSpotLight(LLGLSLShader& shader, LLDrawable* drawablep)
{
	//construct frustum
	LLVOVolume* volume = drawablep->getVOVolume();
	LLVector3 params = volume->getSpotLightParams();

	F32 fov = params.mV[0];
	F32 focus = params.mV[1];

	LLVector3 pos = drawablep->getPositionAgent();
	LLQuaternion quat = volume->getRenderRotation();
	LLVector3 scale = volume->getScale();
	
	//get near clip plane
	LLVector3 at_axis(0,0,-scale.mV[2]*0.5f);
	at_axis *= quat;

	LLVector3 np = pos+at_axis;
	at_axis.normVec();

	//get origin that has given fov for plane np, at_axis, and given scale
	F32 dist = (scale.mV[1]*0.5f)/tanf(fov*0.5f);

	LLVector3 origin = np - at_axis*dist;

	//matrix from volume space to agent space
	LLMatrix4 light_mat_(quat, LLVector4(origin,1.f));

	LLMatrix4a light_mat;
	light_mat.loadu(light_mat_.mMatrix[0]);
	LLMatrix4a light_to_screen;
	light_to_screen.setMul(glh_get_current_modelview(),light_mat);
	LLMatrix4a screen_to_light = light_to_screen;
	screen_to_light.invert();

	F32 s = volume->getLightRadius()*1.5f;
	F32 near_clip = dist;
	F32 width = scale.mV[VX];
	F32 height = scale.mV[VY];
	F32 far_clip = s+dist-scale.mV[VZ];

	F32 fovy = fov * RAD_TO_DEG;
	F32 aspect = width/height;

	LLVector4a p1(0, 0, -(near_clip+0.01f));
	LLVector4a p2(0, 0, -(near_clip+1.f));

	LLVector4a screen_origin(LLVector4a::getZero());

	light_to_screen.affineTransform(p1,p1);
	light_to_screen.affineTransform(p2,p2);
	light_to_screen.affineTransform(screen_origin,screen_origin);

	LLVector4a n;
	n.setSub(p2,p1);
	n.normalize3fast();

	F32 proj_range = far_clip - near_clip;
	LLMatrix4a light_proj = gGL.genPersp(fovy, aspect, near_clip, far_clip);
	light_proj.setMul(gGL.genNDCtoWC(),light_proj);
	screen_to_light.setMul(light_proj,screen_to_light);

	shader.uniformMatrix4fv(LLShaderMgr::PROJECTOR_MATRIX, 1, FALSE, screen_to_light.getF32ptr());
	shader.uniform1f(LLShaderMgr::PROJECTOR_NEAR, near_clip);
	shader.uniform3fv(LLShaderMgr::PROJECTOR_P, 1, p1.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_N, 1, n.getF32ptr());
	shader.uniform3fv(LLShaderMgr::PROJECTOR_ORIGIN, 1, screen_origin.getF32ptr());
	shader.uniform1f(LLShaderMgr::PROJECTOR_RANGE, proj_range);
	shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIANCE, params.mV[2]);
	S32 s_idx = -1;

	for (U32 i = 0; i < 2; i++)
	{
		if (mShadowSpotLight[i] == drawablep)
		{
			s_idx = i;
		}
	}

	shader.uniform1i(LLShaderMgr::PROJECTOR_SHADOW_INDEX, s_idx);

	if (s_idx >= 0)
	{
		shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f-mSpotLightFade[s_idx]);
	}
	else
	{
		shader.uniform1f(LLShaderMgr::PROJECTOR_SHADOW_FADE, 1.f);
	}

	{
		LLDrawable* potential = drawablep;
		//determine if this is a good light for casting shadows
		F32 m_pri = volume->getSpotLightPriority();

		for (U32 i = 0; i < 2; i++)
		{
			F32 pri = 0.f;

			if (mTargetShadowSpotLight[i].notNull())
			{
				pri = mTargetShadowSpotLight[i]->getVOVolume()->getSpotLightPriority();			
			}

			if (m_pri > pri)
			{
				LLDrawable* temp = mTargetShadowSpotLight[i];
				mTargetShadowSpotLight[i] = potential;
				potential = temp;
				m_pri = pri;
			}
		}
	}

	LLViewerTexture* img = volume->getLightTexture();

	if (img == NULL)
	{
		img = LLViewerFetchedTexture::sWhiteImagep;
	}

	S32 channel = shader.enableTexture(LLShaderMgr::DEFERRED_PROJECTION);

	if (channel > -1)
	{
		if (img)
		{
			gGL.getTexUnit(channel)->bind(img);

			F32 lod_range = logf(img->getWidth())/logf(2.f);

			shader.uniform1f(LLShaderMgr::PROJECTOR_FOCUS, focus);
			shader.uniform1f(LLShaderMgr::PROJECTOR_LOD, lod_range);
			shader.uniform1f(LLShaderMgr::PROJECTOR_AMBIENT_LOD, llclamp((proj_range-focus)/proj_range*lod_range, 0.f, 1.f));
		}
	}
		
}

void LLPipeline::unbindDeferredShader(LLGLSLShader &shader, LLRenderTarget* diffuse_source, LLRenderTarget* light_source)
{
	if (shader.mShaderClass != LLViewerShaderMgr::SHADER_DEFERRED && shader.mShaderClass != LLViewerShaderMgr::SHADER_INTERFACE)
	{
		shader.unbind();
		return;
	}

	diffuse_source = diffuse_source ? diffuse_source : &mDeferredScreen;
	light_source = light_source ? light_source : &mDeferredLight;

	stop_glerror();
	shader.disableTexture(LLShaderMgr::DEFERRED_NORMAL, mDeferredScreen.getUsage());
	shader.disableTexture(LLShaderMgr::DEFERRED_DIFFUSE, diffuse_source->getUsage());
	shader.disableTexture(LLShaderMgr::DEFERRED_SPECULAR, mDeferredScreen.getUsage());
	shader.disableTexture(LLShaderMgr::DEFERRED_DEPTH, mDeferredDepth.getUsage());
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHT, light_source->getUsage());
	shader.disableTexture(LLShaderMgr::DIFFUSE_MAP);
	shader.disableTexture(LLShaderMgr::DEFERRED_BLOOM);

	for (U32 i = 0; i < 4; i++)
	{
		if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0+i) > -1)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
		}
	}

	for (U32 i = 4; i < 6; i++)
	{
		if (shader.disableTexture(LLShaderMgr::DEFERRED_SHADOW0+i) > -1)
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE_ARB, GL_NONE);
		}
	}

	shader.disableTexture(LLShaderMgr::DEFERRED_NOISE);
	shader.disableTexture(LLShaderMgr::DEFERRED_LIGHTFUNC);

	S32 channel = shader.disableTexture(LLShaderMgr::ENVIRONMENT_MAP, LLTexUnit::TT_CUBE_MAP);
	if (channel > -1)
	{
		LLCubeMap* cube_map = gSky.mVOSkyp ? gSky.mVOSkyp->getCubeMap() : NULL;
		if (cube_map)
		{
			cube_map->disable();
		}
	}
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->activate();
	shader.unbind();
}

inline float sgn(float a)
{
    if (a > 0.0F) return (1.0F);
    if (a < 0.0F) return (-1.0F);
    return (0.0F);
}

void LLPipeline::generateWaterReflection(LLCamera& camera_in)
{
	static const LLCachedControl<bool> render_transparent_water("RenderTransparentWater",false);
	if ((render_transparent_water || LLPipeline::sRenderDeferred) && LLPipeline::sWaterReflections && assertInitialized() && LLDrawPoolWater::sNeedsReflectionUpdate)
	{
		BOOL skip_avatar_update = FALSE;
		if (!isAgentAvatarValid() || gAgentCamera.getCameraAnimating() || gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK || !LLVOAvatar::sVisibleInFirstPerson)
		{
			skip_avatar_update = TRUE;
		}

		if (!skip_avatar_update)
		{
			gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
		}
		LLVertexBuffer::unbind();

		LLGLStateValidator::checkStates();
		LLGLStateValidator::checkTextureChannels();
		LLGLStateValidator::checkClientArrays();

		LLCamera camera = camera_in;
		camera.setFar(camera.getFar()*0.87654321f);
		LLPipeline::sReflectionRender = TRUE;
		
		gPipeline.pushRenderTypeMask();

		const LLMatrix4a projection = glh_get_current_projection();

		stop_glerror();
		LLPlane plane;

		F32 height = gAgent.getRegion()->getWaterHeight(); 
		F32 to_clip = fabsf(camera.getOrigin().mV[2]-height);
		F32 pad = -to_clip*0.05f; //amount to "pad" clip plane by

		//plane params
		LLVector3 pnorm;
		F32 pd;

		S32 water_clip = 0;
		if (!LLViewerCamera::getInstance()->cameraUnderWater())
		{ //camera is above water, clip plane points up
			pnorm.setVec(0,0,1);
			pd = -height;
			plane.setVec(pnorm, pd);
			water_clip = -1;
		}
		else
		{	//camera is below water, clip plane points down
			pnorm = LLVector3(0,0,-1);
			pd = height;
			plane.setVec(pnorm, pd);
			water_clip = 1;
		}

		bool materials_in_water = false;

#if MATERIALS_IN_REFLECTIONS
		materials_in_water = gSavedSettings.getS32("RenderWaterMaterials");
#endif

		if (!LLViewerCamera::getInstance()->cameraUnderWater())
		{	//generate planar reflection map

			//disable occlusion culling for reflection map for now
			S32 occlusion = LLPipeline::sUseOcclusion;
			LLPipeline::sUseOcclusion = 0;
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			glClearColor(0,0,0,0);
			mWaterRef.bindTarget();
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER0;
			gGL.setColorMask(true, true);
			mWaterRef.clear();
			gGL.setColorMask(true, false);

			mWaterRef.getViewport(gGLViewport);

			stop_glerror();

			gGL.pushMatrix();

			const LLMatrix4a saved_modelview = glh_get_current_modelview();

			LLMatrix4a mat;
			mat.setIdentity();
			mat.getRow<2>().negate();
			mat.setTranslate_affine(LLVector3(0.f,0.f,height*2.f));
			mat.setMul(saved_modelview,mat);

			glh_set_current_modelview(mat);
			gGL.loadMatrix(mat);

			LLViewerCamera::updateFrustumPlanes(camera, FALSE, TRUE);

			LLMatrix4a inv_mat = mat;
			inv_mat.invert();

			LLVector4a origin;
			origin.clear();
			inv_mat.affineTransform(origin,origin);

			camera.setOrigin(origin.getF32ptr());

			glCullFace(GL_FRONT);

			static LLCullResult ref_result;

			if (LLDrawPoolWater::sNeedsReflectionUpdate)
			{
				//initial sky pass (no user clip plane)
				{ //mask out everything but the sky
					gPipeline.pushRenderTypeMask();
					gPipeline.andRenderTypeMask(LLPipeline::RENDER_TYPE_SKY,
												LLPipeline::RENDER_TYPE_WL_SKY,
												LLPipeline::RENDER_TYPE_WL_CLOUDS,
												LLPipeline::END_RENDER_TYPES);

					static LLCullResult result;
					updateCull(camera, result);
					stateSort(camera, result);

					if (LLPipeline::sRenderDeferred && materials_in_water)
					{
						mWaterRef.flush();

						gPipeline.grabReferences(result);
						gPipeline.mDeferredScreen.bindTarget();
						gGL.setColorMask(true, true);						
						glClearColor(0,0,0,0);
						gPipeline.mDeferredScreen.clear();

						renderGeomDeferred(camera);						
					}
					else
					{
						renderGeom(camera, TRUE);
					}					

					gPipeline.popRenderTypeMask();
				}

				gGL.setColorMask(true, false);
				
				static const LLCachedControl<S32> detail("RenderReflectionDetail",0);
				if (detail > 0)
				{ //mask out selected geometry based on reflection detail
					gPipeline.pushRenderTypeMask();
					if (detail < 4)
					{
#if ENABLE_CLASSIC_CLOUDS
						clearRenderTypeMask(LLPipeline::RENDER_TYPE_PARTICLES, LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS, END_RENDER_TYPES);
#else
						clearRenderTypeMask(LLPipeline::RENDER_TYPE_PARTICLES, END_RENDER_TYPES);
#endif
						if (detail < 3)
						{
							clearRenderTypeMask(LLPipeline::RENDER_TYPE_AVATAR, END_RENDER_TYPES);
							if (detail < 2)
							{
								clearRenderTypeMask(LLPipeline::RENDER_TYPE_VOLUME, END_RENDER_TYPES);
							}
						}
					}

					clearRenderTypeMask(LLPipeline::RENDER_TYPE_WATER,
									LLPipeline::RENDER_TYPE_VOIDWATER,
									LLPipeline::RENDER_TYPE_GROUND,
									LLPipeline::RENDER_TYPE_SKY,
									LLPipeline::RENDER_TYPE_WL_CLOUDS,
									LLPipeline::END_RENDER_TYPES);
					static const LLCachedControl<bool> skip_distortion_updates("SkipReflectOcclusionUpdates",false);
					LLPipeline::sSkipUpdate = skip_distortion_updates;
					LLGLUserClipPlane clip_plane(plane, mat, projection);
					LLGLDisable<GL_CULL_FACE> cull;
					updateCull(camera, ref_result, -water_clip, &plane);
					stateSort(camera, ref_result);

					if (LLDrawPoolWater::sNeedsDistortionUpdate)
					{
						if (detail > 0)
						{
							gPipeline.grabReferences(ref_result);
							LLGLUserClipPlane clip_plane(plane, mat, projection);

							if (LLPipeline::sRenderDeferred && materials_in_water)
							{							
								renderGeomDeferred(camera);
							}
							else
							{
								renderGeom(camera);
							}
						}
					}
					
					if (LLPipeline::sRenderDeferred && materials_in_water)
					{
						gPipeline.mDeferredScreen.flush();
						renderDeferredLightingToRT(&mWaterRef);
					}

					LLPipeline::sSkipUpdate = FALSE;
					gPipeline.popRenderTypeMask();
				}
			}
			glCullFace(GL_BACK);
			gGL.popMatrix();
			mWaterRef.flush();
			glh_set_current_modelview(saved_modelview);
			LLPipeline::sUseOcclusion = occlusion;
		}

		camera.setOrigin(camera_in.getOrigin());
		//render distortion map
		static BOOL last_update = TRUE;
		if (last_update)
		{
			camera.setFar(camera_in.getFar());
			clearRenderTypeMask(LLPipeline::RENDER_TYPE_WATER,
								LLPipeline::RENDER_TYPE_VOIDWATER,
								LLPipeline::RENDER_TYPE_GROUND,
								END_RENDER_TYPES);	
			stop_glerror();

			LLPipeline::sUnderWaterRender = LLViewerCamera::getInstance()->cameraUnderWater() ? FALSE : TRUE;

			if (LLPipeline::sUnderWaterRender)
			{
				clearRenderTypeMask(LLPipeline::RENDER_TYPE_GROUND,
									LLPipeline::RENDER_TYPE_SKY,
#if ENABLE_CLASSIC_CLOUDS
									LLPipeline::RENDER_TYPE_CLASSIC_CLOUDS,
#endif
									LLPipeline::RENDER_TYPE_WL_CLOUDS,
									LLPipeline::RENDER_TYPE_WL_SKY,
									END_RENDER_TYPES);		
			}
			LLViewerCamera::updateFrustumPlanes(camera);

			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			LLColor4& col = LLDrawPoolWater::sWaterFogColor;
			glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			mWaterDis.bindTarget();
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WATER1;
			mWaterDis.getViewport(gGLViewport);
			
			if (!LLPipeline::sUnderWaterRender || LLDrawPoolWater::sNeedsReflectionUpdate)
			{
				//clip out geometry on the same side of water as the camera
				LLPlane plane(-pnorm, -(pd+pad));

				LLGLUserClipPlane clip_plane(plane, glh_get_current_modelview(), projection);
				static LLCullResult result;
				updateCull(camera, result, water_clip, &plane);
				stateSort(camera, result);

				gGL.setColorMask(true, true);
				mWaterDis.clear();
				

				gGL.setColorMask(true, false);

				
				if (LLPipeline::sRenderDeferred && materials_in_water)
				{										
					mWaterDis.flush();
					gPipeline.mDeferredScreen.bindTarget();
					gGL.setColorMask(true, true);
					glClearColor(0,0,0,0);
					gPipeline.mDeferredScreen.clear();
					gPipeline.grabReferences(result);
					renderGeomDeferred(camera);					
				}
				else
				{
				renderGeom(camera);
				}

				if (LLPipeline::sRenderDeferred && materials_in_water)
				{
					gPipeline.mDeferredScreen.flush();
					renderDeferredLightingToRT(&mWaterDis);
				}
			}

			mWaterDis.flush();
			LLPipeline::sUnderWaterRender = FALSE;
			
		}
		last_update = LLDrawPoolWater::sNeedsReflectionUpdate && LLDrawPoolWater::sNeedsDistortionUpdate;

		LLPipeline::sReflectionRender = FALSE;

		if (!LLRenderTarget::sUseFBO)
		{
			gGL.syncContextState();
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		glClearColor(0.f, 0.f, 0.f, 0.f);
		gViewerWindow->setup3DViewport();
		gPipeline.popRenderTypeMask();
		LLDrawPoolWater::sNeedsReflectionUpdate = FALSE;
		LLDrawPoolWater::sNeedsDistortionUpdate = FALSE;
		LLPlane npnorm(-pnorm, -pd);
		LLViewerCamera::getInstance()->setUserClipPlane(npnorm);
		
		LLGLStateValidator::checkStates();

		if (!skip_avatar_update)
		{
			gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
		}

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
	}
}

static LLTrace::BlockTimerStatHandle FTM_SHADOW_RENDER("Render Shadows");
static LLTrace::BlockTimerStatHandle FTM_SHADOW_ALPHA("Alpha Shadow");
static LLTrace::BlockTimerStatHandle FTM_SHADOW_SIMPLE("Simple Shadow");

void LLPipeline::renderShadow(const LLMatrix4a& view, const LLMatrix4a& proj, LLCamera& shadow_cam, LLCullResult &result, BOOL use_shader, BOOL use_occlusion, U32 target_width)
{
	LL_RECORD_BLOCK_TIME(FTM_SHADOW_RENDER);

	//clip out geometry on the same side of water as the camera
	S32 occlude = LLPipeline::sUseOcclusion;
	if (!use_occlusion)
	{
		LLPipeline::sUseOcclusion = 0;
	}
	LLPipeline::sShadowRender = TRUE;
	
	U32 types[] = { 
		LLRenderPass::PASS_SIMPLE, 
		LLRenderPass::PASS_FULLBRIGHT, 
		LLRenderPass::PASS_SHINY, 
		LLRenderPass::PASS_BUMP, 
		LLRenderPass::PASS_FULLBRIGHT_SHINY ,
		LLRenderPass::PASS_MATERIAL,
		LLRenderPass::PASS_MATERIAL_ALPHA_EMISSIVE,
		LLRenderPass::PASS_SPECMAP,
		LLRenderPass::PASS_SPECMAP_EMISSIVE,
		LLRenderPass::PASS_NORMMAP,
		LLRenderPass::PASS_NORMMAP_EMISSIVE,
		LLRenderPass::PASS_NORMSPEC,
		LLRenderPass::PASS_NORMSPEC_EMISSIVE,
	};

	LLGLEnable<GL_CULL_FACE> cull;

	if (use_shader)
	{
		gDeferredShadowCubeProgram.bind();
	}

	updateCull(shadow_cam, result);

	stateSort(shadow_cam, result);
	
	
	//generate shadow map
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(proj);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadMatrix(view);	//Why was glh_get_current_modelview() used instead of view?

	stop_glerror();
	gGLLastMatrix = NULL;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	
	stop_glerror();
	
	LLVertexBuffer::unbind();

	{
		if (!use_shader)
		{ //occlusion program is general purpose depth-only no-textures
			gOcclusionProgram.bind();
		}
		else
		{
			gDeferredShadowProgram.bind();
		}

		gGL.diffuseColor4f(1,1,1,1);
		gGL.setColorMask(false, false);
	
		LL_RECORD_BLOCK_TIME(FTM_SHADOW_SIMPLE);
		gGL.getTexUnit(0)->disable();
		for (U32 i = 0; i < sizeof(types)/sizeof(U32); ++i)
		{
			renderObjects(types[i], LLVertexBuffer::MAP_VERTEX, FALSE);
		}
		gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);
		if (!use_shader)
		{
			gOcclusionProgram.unbind();
		}
	}
	
	if (use_shader)
	{
		gDeferredShadowProgram.unbind();
		renderGeomShadow(shadow_cam);
		gDeferredShadowProgram.bind();
	}
	else
	{
		renderGeomShadow(shadow_cam);
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_SHADOW_ALPHA);
		gDeferredShadowAlphaMaskProgram.bind();
		gDeferredShadowAlphaMaskProgram.uniform1f(LLShaderMgr::DEFERRED_SHADOW_TARGET_WIDTH, (float)target_width);

		U32 mask =	LLVertexBuffer::MAP_VERTEX | 
					LLVertexBuffer::MAP_TEXCOORD0 | 
					LLVertexBuffer::MAP_COLOR | 
					LLVertexBuffer::MAP_TEXTURE_INDEX;

		renderMaskedObjects(LLRenderPass::PASS_ALPHA_MASK, mask, TRUE, TRUE);
		renderMaskedObjects(LLRenderPass::PASS_FULLBRIGHT_ALPHA_MASK, mask, TRUE, TRUE);
		gDeferredShadowAlphaMaskProgram.setMinimumAlpha(0.598f);
		renderObjects(LLRenderPass::PASS_ALPHA, mask, TRUE, TRUE);

		mask = mask & ~LLVertexBuffer::MAP_TEXTURE_INDEX;

		gDeferredTreeShadowProgram.bind();
		renderMaskedObjects(LLRenderPass::PASS_NORMSPEC_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_MATERIAL_ALPHA_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_SPECMAP_MASK, mask);
		renderMaskedObjects(LLRenderPass::PASS_NORMMAP_MASK, mask);
		
		gDeferredTreeShadowProgram.setMinimumAlpha(0.598f);
		renderObjects(LLRenderPass::PASS_GRASS, LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0, TRUE);
	}

	//glCullFace(GL_BACK);

	gDeferredShadowCubeProgram.bind();
	gGLLastMatrix = NULL;
	gGL.loadMatrix(glh_get_current_modelview());

	//LLRenderTarget& occlusion_source = mShadow[LLViewerCamera::sCurCameraID-1];

	doOcclusion(shadow_cam);

	if (use_shader)
	{
		gDeferredShadowProgram.unbind();
	}
	
	gGL.setColorMask(true, true);
			
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();
	gGLLastMatrix = NULL;

	LLPipeline::sUseOcclusion = occlude;
	LLPipeline::sShadowRender = FALSE;
}

static LLTrace::BlockTimerStatHandle FTM_VISIBLE_CLOUD("Visible Cloud");
BOOL LLPipeline::getVisiblePointCloud(LLCamera& camera, LLVector3& min, LLVector3& max, std::vector<LLVector3>& fp, LLVector3 light_dir)
{
	LL_RECORD_BLOCK_TIME(FTM_VISIBLE_CLOUD);
	//get point cloud of intersection of frust and min, max

	if (getVisibleExtents(camera, min, max))
	{
		return FALSE;
	}

	//get set of planes on bounding box
	LLPlane bp[] = { 
		LLPlane(min, LLVector3(-1,0,0)),
		LLPlane(min, LLVector3(0,-1,0)),
		LLPlane(min, LLVector3(0,0,-1)),
		LLPlane(max, LLVector3(1,0,0)),
		LLPlane(max, LLVector3(0,1,0)),
		LLPlane(max, LLVector3(0,0,1))};
	
	//potential points
	std::vector<LLVector3> pp;

	//add corners of AABB
	pp.push_back(LLVector3(min.mV[0], min.mV[1], min.mV[2]));
	pp.push_back(LLVector3(max.mV[0], min.mV[1], min.mV[2]));
	pp.push_back(LLVector3(min.mV[0], max.mV[1], min.mV[2]));
	pp.push_back(LLVector3(max.mV[0], max.mV[1], min.mV[2]));
	pp.push_back(LLVector3(min.mV[0], min.mV[1], max.mV[2]));
	pp.push_back(LLVector3(max.mV[0], min.mV[1], max.mV[2]));
	pp.push_back(LLVector3(min.mV[0], max.mV[1], max.mV[2]));
	pp.push_back(LLVector3(max.mV[0], max.mV[1], max.mV[2]));

	//add corners of camera frustum
	for (U32 i = 0; i < LLCamera::AGENT_FRUSTRUM_NUM; i++)
	{
		pp.push_back(camera.mAgentFrustum[i]);
	}


	//bounding box line segments
	U32 bs[] = 
			{
		0,1,
		1,3,
		3,2,
		2,0,

		4,5,
		5,7,
		7,6,
		6,4,

		0,4,
		1,5,
		3,7,
		2,6
	};

	for (U32 i = 0; i < 12; i++)
	{ //for each line segment in bounding box
		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; j++) 
		{ //for each plane in camera frustum
			const LLPlane& cp = camera.getAgentPlane(j);
			const LLVector3& v1 = pp[bs[i*2+0]];
			const LLVector3& v2 = pp[bs[i*2+1]];
			LLVector3 n;
			cp.getVector3(n);

			LLVector3 line = v1-v2;

			F32 d1 = line*n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2/d1;

			if (t > 0.f && t < 1.f)
			{
				LLVector3 intersect = v2+line*t;
				pp.push_back(intersect);
			}
		}
	}
			
	//camera frustum line segments
	const U32 fs[] =
	{
		0,1,
		1,2,
		2,3,
		3,0,

		4,5,
		5,6,
		6,7,
		7,4,
	
		0,4,
		1,5,
		2,6,
		3,7	
	};

	for (U32 i = 0; i < 12; i++)
	{
		for (U32 j = 0; j < 6; ++j)
		{
			const LLVector3& v1 = pp[fs[i*2+0]+8];
			const LLVector3& v2 = pp[fs[i*2+1]+8];
			const LLPlane& cp = bp[j];
			LLVector3 n;
			cp.getVector3(n);

			LLVector3 line = v1-v2;

			F32 d1 = line*n;
			F32 d2 = -cp.dist(v2);

			F32 t = d2/d1;

			if (t > 0.f && t < 1.f)
			{
				LLVector3 intersect = v2+line*t;
				pp.push_back(intersect);
			}	
		}
	}

	LLVector3 ext[] = { min-LLVector3(0.05f,0.05f,0.05f),
		max+LLVector3(0.05f,0.05f,0.05f) };

	for (U32 i = 0; i < pp.size(); ++i)
	{
		bool found = true;

		const F32* p = pp[i].mV;
			
		for (U32 j = 0; j < 3; ++j)
		{
			if (p[j] < ext[0].mV[j] ||
				p[j] > ext[1].mV[j])
			{
				found = false;
				break;
			}
		}
				
		for (U32 j = 0; j < LLCamera::AGENT_PLANE_NO_USER_CLIP_NUM; ++j)
		{
			const LLPlane& cp = camera.getAgentPlane(j);
			F32 dist = cp.dist(pp[i]);
			if (dist > 0.05f) //point is above some plane, not contained
					{
				found = false;
				break;
						}
					}

					if (found)
					{
			fp.push_back(pp[i]);
		}
	}
	
	if (fp.empty())
	{
		return FALSE;
	}
	
	return TRUE;
}


static LLTrace::BlockTimerStatHandle FTM_GEN_SUN_SHADOW("Gen Sun Shadow");

void LLPipeline::generateSunShadow(LLCamera& camera)
{
	static const LLCachedControl<S32> RenderShadowDetail("RenderShadowDetail",0);
	static const LLCachedControl<LLVector3> RenderShadowClipPlanes("RenderShadowClipPlanes",LLVector3(1.f,12.f,32.f));
	static const LLCachedControl<LLVector3> RenderShadowOrthoClipPlanes("RenderShadowOrthoClipPlanes",LLVector3(4.f,8.f,24.f));
	static const LLCachedControl<F32> RenderFarClip("RenderFarClip");
	static const LLCachedControl<LLVector3> RenderShadowNearDist("RenderShadowNearDist");
	static const LLCachedControl<LLVector3> RenderShadowSplitExponent("RenderShadowSplitExponent",LLVector3(3.f,3.f,2.f));
	static const LLCachedControl<F32> RenderShadowFOVCutoff("RenderShadowFOVCutoff",1.1f);
	static const LLCachedControl<F32> RenderShadowErrorCutoff("RenderShadowErrorCutoff",5.f);
	static const LLCachedControl<bool> CameraOffset("CameraOffset",false);
	
	if (!sRenderDeferred || RenderShadowDetail <= 0)
	{
		return;
	}

	LL_RECORD_BLOCK_TIME(FTM_GEN_SUN_SHADOW);

	BOOL skip_avatar_update = FALSE;
	if (!isAgentAvatarValid() || gAgentCamera.getCameraAnimating() || gAgentCamera.getCameraMode() != CAMERA_MODE_MOUSELOOK || !LLVOAvatar::sVisibleInFirstPerson)
	{

		skip_avatar_update = TRUE;
	}

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}

	LLMatrix4a last_modelview = glh_get_last_modelview();
	LLMatrix4a last_projection = glh_get_last_projection();

	pushRenderTypeMask();
	andRenderTypeMask(LLPipeline::RENDER_TYPE_SIMPLE,
					LLPipeline::RENDER_TYPE_ALPHA,
					LLPipeline::RENDER_TYPE_GRASS,
					LLPipeline::RENDER_TYPE_FULLBRIGHT,
					LLPipeline::RENDER_TYPE_BUMP,
					LLPipeline::RENDER_TYPE_VOLUME,
					LLPipeline::RENDER_TYPE_AVATAR,
					LLPipeline::RENDER_TYPE_TREE, 
					LLPipeline::RENDER_TYPE_TERRAIN,
					LLPipeline::RENDER_TYPE_WATER,
					LLPipeline::RENDER_TYPE_VOIDWATER,
					LLPipeline::RENDER_TYPE_PASS_ALPHA,
					LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
					LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
					LLPipeline::RENDER_TYPE_PASS_GRASS,
					LLPipeline::RENDER_TYPE_PASS_SIMPLE,
					LLPipeline::RENDER_TYPE_PASS_BUMP,
					LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
					LLPipeline::RENDER_TYPE_PASS_SHINY,
					LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
					LLPipeline::RENDER_TYPE_PASS_MATERIAL,
					LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA,
					LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_MASK,
					LLPipeline::RENDER_TYPE_PASS_MATERIAL_ALPHA_EMISSIVE,
					LLPipeline::RENDER_TYPE_PASS_SPECMAP,
					LLPipeline::RENDER_TYPE_PASS_SPECMAP_BLEND,
					LLPipeline::RENDER_TYPE_PASS_SPECMAP_MASK,
					LLPipeline::RENDER_TYPE_PASS_SPECMAP_EMISSIVE,
					LLPipeline::RENDER_TYPE_PASS_NORMMAP,
					LLPipeline::RENDER_TYPE_PASS_NORMMAP_BLEND,
					LLPipeline::RENDER_TYPE_PASS_NORMMAP_MASK,
					LLPipeline::RENDER_TYPE_PASS_NORMMAP_EMISSIVE,
					LLPipeline::RENDER_TYPE_PASS_NORMSPEC,
					LLPipeline::RENDER_TYPE_PASS_NORMSPEC_BLEND,
					LLPipeline::RENDER_TYPE_PASS_NORMSPEC_MASK,
					LLPipeline::RENDER_TYPE_PASS_NORMSPEC_EMISSIVE,
					END_RENDER_TYPES);

	gGL.setColorMask(false, false);

	//get sun view matrix
	
	//store current projection/modelview matrix
	const LLMatrix4a saved_proj = glh_get_current_projection();
	const LLMatrix4a saved_view = glh_get_current_modelview();
	LLMatrix4a inv_view(saved_view);
	inv_view.invert();

	LLMatrix4a view[6];
	LLMatrix4a proj[6];

	//clip contains parallel split distances for 3 splits
	LLVector3 clip = RenderShadowClipPlanes;

	//F32 slope_threshold = gSavedSettings.getF32("RenderShadowSlopeThreshold");

	//far clip on last split is minimum of camera view distance and 128
	mSunClipPlanes = LLVector4(clip, clip.mV[2] * clip.mV[2]/clip.mV[1]);

	//currently used for amount to extrude frusta corners for constructing shadow frusta
	//LLVector3 n = RenderShadowNearDist;
	//F32 nearDist[] = { n.mV[0], n.mV[1], n.mV[2], n.mV[2] };

	//put together a universal "near clip" plane for shadow frusta
	LLVector3 sunDir = getSunDir();
	LLPlane shadow_near_clip;
	{
		;
		LLVector3 p = gAgent.getPositionAgent();
		p += sunDir * RenderFarClip*2.f;
		shadow_near_clip.setVec(p, sunDir);
	}

	LLVector3 lightDir = -sunDir;
	lightDir.normVec();

	//create light space camera matrix
	
	LLVector3 at = lightDir;

	LLVector3 up = camera.getAtAxis();

	if (fabsf(up*lightDir) > 0.75f)
	{
		up = camera.getUpAxis();
	}

	/*LLVector3 left = up%at;
	up = at%left;*/

	up.normVec();
	at.normVec();
	
	
	LLCamera main_camera = camera;
	
	F32 near_clip = 0.f;
	{
		//get visible point cloud
		std::vector<LLVector3> fp;

		main_camera.calcAgentFrustumPlanes(main_camera.mAgentFrustum);
		
		LLVector3 min,max;
		getVisiblePointCloud(main_camera,min,max,fp);

		if (fp.empty())
		{
			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowCamera[0] = main_camera;
				mShadowExtents[0][0] = min;
				mShadowExtents[0][1] = max;

				mShadowFrustPoints[0].clear();
				mShadowFrustPoints[1].clear();
				mShadowFrustPoints[2].clear();
				mShadowFrustPoints[3].clear();
			}
			popRenderTypeMask();

			if (!skip_avatar_update)
			{
				gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
			}

			return;
		}

		//get good split distances for frustum
		for (U32 i = 0; i < fp.size(); ++i)
		{
			LLVector4a v;
			v.load3(fp[i].mV);
			saved_view.affineTransform(v,v);
			fp[i].setVec(v.getF32ptr());
		}

		min = fp[0];
		max = fp[0];

		//get camera space bounding box
		for (U32 i = 1; i < fp.size(); ++i)
		{
			update_min_max(min, max, fp[i]);
		}

		near_clip = -max.mV[2];
		F32 far_clip = -min.mV[2]*2.f;

		//far_clip = llmin(far_clip, 128.f);
		far_clip = llmin(far_clip, camera.getFar());

		F32 range = far_clip-near_clip;

		LLVector3 split_exp = RenderShadowSplitExponent;

		F32 da = 1.f-llmax( fabsf(lightDir*up), fabsf(lightDir*camera.getLeftAxis()) );
		
		da = powf(da, split_exp.mV[2]);

		F32 sxp = split_exp.mV[1] + (split_exp.mV[0]-split_exp.mV[1])*da;
		
		for (U32 i = 0; i < 4; ++i)
		{
			F32 x = (F32)(i+1)/4.f;
			x = powf(x, sxp);
			mSunClipPlanes.mV[i] = near_clip+range*x;
		}

		mSunClipPlanes.mV[0] *= 1.25f; //bump back first split for transition padding
	}

	// convenience array of 4 near clip plane distances
	F32 dist[] = { near_clip, mSunClipPlanes.mV[0], mSunClipPlanes.mV[1], mSunClipPlanes.mV[2], mSunClipPlanes.mV[3] };

	if (mSunDiffuse == LLColor4::black)
	{ //sun diffuse is totally black, shadows don't matter
		LLGLDepthTest depth(GL_TRUE);

		for (S32 j = 0; j < 4; j++)
		{
			mShadow[j].bindTarget();
			mShadow[j].clear();
			mShadow[j].flush();
		}
	}
	else
	{
		for (S32 j = 0; j < 4; j++)
		{
			if (!hasRenderDebugMask(RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowFrustPoints[j].clear();
			}

			LLViewerCamera::sCurCameraID = LLViewerCamera::eCameraID(LLViewerCamera::CAMERA_SHADOW0+j);

			//restore render matrices
			glh_set_current_modelview(saved_view);
			glh_set_current_projection(saved_proj);

			LLVector3 eye = camera.getOrigin();

			//camera used for shadow cull/render
			LLCamera shadow_cam;
		
			//create world space camera frustum for this split
			shadow_cam = camera;
			shadow_cam.setFar(16.f);
	
			LLViewerCamera::updateFrustumPlanes(shadow_cam, FALSE, FALSE, TRUE);

			LLVector3* frust = shadow_cam.mAgentFrustum;

			LLVector3 pn = shadow_cam.getAtAxis();
		
			LLVector3 min, max;

			//construct 8 corners of split frustum section
			for (U32 i = 0; i < 4; i++)
			{
				LLVector3 delta = frust[i+4]-eye;
				delta += (frust[i+4]-frust[(i+2)%4+4])*0.05f;
				delta.normVec();
				F32 dp = delta*pn;
				frust[i] = eye + (delta*dist[j]*0.75f)/dp;
				frust[i+4] = eye + (delta*dist[j+1]*1.25f)/dp;
			}
						
			shadow_cam.calcAgentFrustumPlanes(frust);
			shadow_cam.mFrustumCornerDist = 0.f;
		
			if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowCamera[j] = shadow_cam;
			}

			std::vector<LLVector3> fp;

			if (!gPipeline.getVisiblePointCloud(shadow_cam, min, max, fp, lightDir))
			{
				//no possible shadow receivers
				if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
				{
					mShadowExtents[j][0] = LLVector3();
					mShadowExtents[j][1] = LLVector3();
					mShadowCamera[j+4] = shadow_cam;
				}

				mShadow[j].bindTarget();
				{
					LLGLDepthTest depth(GL_TRUE);
					mShadow[j].clear();
				}
				mShadow[j].flush();

				continue;
			}

			if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
			{
				mShadowExtents[j][0] = min;
				mShadowExtents[j][1] = max;
				mShadowFrustPoints[j] = fp;
			}
				

			//find a good origin for shadow projection
			LLVector3 origin;

			//get a temporary view projection
			view[j] = gGL.genLook(camera.getOrigin(), lightDir, -up);

			std::vector<LLVector3> wpf;

			for (U32 i = 0; i < fp.size(); i++)
			{
				LLVector4a p;
				p.load3(fp[i].mV);
				view[j].affineTransform(p,p);
				wpf.push_back(LLVector3(p.getF32ptr()));
			}

			min = wpf[0];
			max = wpf[0];

			for (U32 i = 0; i < fp.size(); ++i)
			{ //get AABB in camera space
				update_min_max(min, max, wpf[i]);
			}

			// Construct a perspective transform with perspective along y-axis that contains
			// points in wpf
			//Known:
			// - far clip plane
			// - near clip plane
			// - points in frustum
			//Find:
			// - origin

			//get some "interesting" points of reference
			LLVector3 center = (min+max)*0.5f;
			LLVector3 size = (max-min)*0.5f;
			LLVector3 near_center = center;
			near_center.mV[1] += size.mV[1]*2.f;
		
		
			//put all points in wpf in quadrant 0, reletive to center of min/max
			//get the best fit line using least squares
			F32 bfm = 0.f;
			F32 bfb = 0.f;

			for (U32 i = 0; i < wpf.size(); ++i)
			{
				wpf[i] -= center;
				wpf[i].mV[0] = fabsf(wpf[i].mV[0]);
				wpf[i].mV[2] = fabsf(wpf[i].mV[2]);
			}

			if (!wpf.empty())
			{ 
				F32 sx = 0.f;
				F32 sx2 = 0.f;
				F32 sy = 0.f;
				F32 sxy = 0.f;
			
				for (U32 i = 0; i < wpf.size(); ++i)
				{		
					sx += wpf[i].mV[0];
					sx2 += wpf[i].mV[0]*wpf[i].mV[0];
					sy += wpf[i].mV[1];
					sxy += wpf[i].mV[0]*wpf[i].mV[1]; 
				}

				bfm = (sy*sx-wpf.size()*sxy)/(sx*sx-wpf.size()*sx2);
				bfb = (sx*sxy-sy*sx2)/(sx*sx-bfm*sx2);
			}
		
			{
				// best fit line is y=bfm*x+bfb
		
				//find point that is furthest to the right of line
				F32 off_x = -1.f;
				LLVector3 lp;

				for (U32 i = 0; i < wpf.size(); ++i)
				{
					//y = bfm*x+bfb
					//x = (y-bfb)/bfm
					F32 lx = (wpf[i].mV[1]-bfb)/bfm;

					lx = wpf[i].mV[0]-lx;
				
					if (off_x < lx)
					{
						off_x = lx;
						lp = wpf[i];
					}
				}

				//get line with slope bfm through lp
				// bfb = y-bfm*x
				bfb = lp.mV[1]-bfm*lp.mV[0];

				//calculate error
				F32 shadow_error = 0.f;

				for (U32 i = 0; i < wpf.size(); ++i)
				{
					F32 lx = (wpf[i].mV[1]-bfb)/bfm;
					shadow_error += fabsf(wpf[i].mV[0]-lx);
				}

				shadow_error /= wpf.size();
				shadow_error /= size.mV[0];

				if (shadow_error > RenderShadowErrorCutoff)
				{ //just use ortho projection
					origin.clearVec();
					proj[j] = gGL.genOrtho(min.mV[0], max.mV[0], min.mV[1], max.mV[1], -max.mV[2], -min.mV[2]);
				}
				else
				{
					//origin is where line x = 0;
					origin.setVec(0,bfb,0);

					F32 fovz = 1.f;
					F32 fovx = 1.f;
				
					LLVector3 zp;
					LLVector3 xp;

					for (U32 i = 0; i < wpf.size(); ++i)
					{
						LLVector3 atz = wpf[i]-origin;
						atz.mV[0] = 0.f;
						atz.normVec();
						if (fovz > -atz.mV[1])
						{
							zp = wpf[i];
							fovz = -atz.mV[1];
						}
					
						LLVector3 atx = wpf[i]-origin;
						atx.mV[2] = 0.f;
						atx.normVec();
						if (fovx > -atx.mV[1])
						{
							fovx = -atx.mV[1];
							xp = wpf[i];
						}
					}

					fovx = acos(fovx);
					fovz = acos(fovz);

					F32 cutoff = llmin((F32) RenderShadowFOVCutoff, 1.4f);
				
					if (fovx < cutoff && fovz > cutoff)
					{
						//x is a good fit, but z is too big, move away from zp enough so that fovz matches cutoff
						F32 d = zp.mV[2]/tan(cutoff);
						F32 ny = zp.mV[1] + fabsf(d);

						origin.mV[1] = ny;

						fovz = 1.f;
						fovx = 1.f;

						for (U32 i = 0; i < wpf.size(); ++i)
						{
							LLVector3 atz = wpf[i]-origin;
							atz.mV[0] = 0.f;
							atz.normVec();
							fovz = llmin(fovz, -atz.mV[1]);

							LLVector3 atx = wpf[i]-origin;
							atx.mV[2] = 0.f;
							atx.normVec();
							fovx = llmin(fovx, -atx.mV[1]);
						}

						fovx = acos(fovx);
						fovz = acos(fovz);

					}

				
					origin += center;
			
					F32 ynear = -(max.mV[1]-origin.mV[1]);
					F32 yfar = -(min.mV[1]-origin.mV[1]);
				
					if (ynear < 0.1f) //keep a sensible near clip plane
					{
						F32 diff = 0.1f-ynear;
						origin.mV[1] += diff;
						ynear += diff;
						yfar += diff;
					}
	
					if (fovx > cutoff)
					{ //just use ortho projection
						origin.clearVec();
						proj[j] = gGL.genOrtho(min.mV[0], max.mV[0], min.mV[1], max.mV[1], -max.mV[2], -min.mV[2]);
					}
					else
					{
						//get perspective projection
						view[j].invert();
						LLVector4a origin_agent;
						origin_agent.load3(origin.mV);

						//translate view to origin
						view[j].affineTransform(origin_agent,origin_agent);

						eye = LLVector3(origin_agent.getF32ptr());

						view[j] = gGL.genLook(LLVector3(origin_agent.getF32ptr()), lightDir, -up);

						F32 fx = 1.f/tanf(fovx);
						F32 fz = 1.f/tanf(fovz);

						proj[j].setRow<0>(LLVector4a(	-fx,	0.f,							0.f));
						proj[j].setRow<1>(LLVector4a(	0.f,	(yfar+ynear)/(ynear-yfar),		0.f,	-1.f));
						proj[j].setRow<2>(LLVector4a(	0.f,	0.f,							-fz));
						proj[j].setRow<3>(LLVector4a(	0.f,	(2.f*yfar*ynear)/(ynear-yfar),	0.f));
					}
				}
			}

			//shadow_cam.setFar(128.f);
			shadow_cam.setOriginAndLookAt(eye, up, center);

			shadow_cam.setOrigin(0,0,0);

			glh_set_current_modelview(view[j]);
			glh_set_current_projection(proj[j]);

			LLViewerCamera::updateFrustumPlanes(shadow_cam, FALSE, FALSE, TRUE);

			//shadow_cam.ignoreAgentFrustumPlane(LLCamera::AGENT_PLANE_NEAR);
			shadow_cam.getAgentPlane(LLCamera::AGENT_PLANE_NEAR).set(shadow_near_clip);

			glh_set_current_modelview(view[j]);
			glh_set_current_projection(proj[j]);

			glh_set_last_modelview(mShadowModelview[j]);
			glh_set_last_projection(mShadowProjection[j]);

			mShadowModelview[j] = view[j];
			mShadowProjection[j] = proj[j];


			mSunShadowMatrix[j].setMul(gGL.genNDCtoWC(),proj[j]);
			mSunShadowMatrix[j].mul_affine(view[j]);
			mSunShadowMatrix[j].mul_affine(inv_view);
		
			stop_glerror();

			mShadow[j].bindTarget();
			mShadow[j].getViewport(gGLViewport);
			mShadow[j].clear();
		
			U32 target_width = mShadow[j].getWidth();

			{
				static LLCullResult result[4];

				//LLGLEnable enable(GL_DEPTH_CLAMP_NV);
				renderShadow(view[j], proj[j], shadow_cam, result[j], TRUE, TRUE, target_width);
			}

			mShadow[j].flush();
 
			if (!gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA))
			{
				LLViewerCamera::updateFrustumPlanes(shadow_cam, FALSE, FALSE, TRUE);
				mShadowCamera[j+4] = shadow_cam;
			}
		}
	}

	
	//hack to disable projector shadows 
	bool gen_shadow = RenderShadowDetail > 1;

	if (gen_shadow)
	{
		F32 fade_amt = gFrameIntervalSeconds * llmax(LLViewerCamera::getInstance()->getVelocityStat()->getCurrentPerSec(), 1.f);

		//update shadow targets
		for (U32 i = 0; i < 2; i++)
		{ //for each current shadow
			LLViewerCamera::sCurCameraID = LLViewerCamera::eCameraID(LLViewerCamera::CAMERA_SHADOW4 + i);

			if (mShadowSpotLight[i].notNull() && 
				(mShadowSpotLight[i] == mTargetShadowSpotLight[0] ||
				mShadowSpotLight[i] == mTargetShadowSpotLight[1]))
			{ //keep this spotlight
				mSpotLightFade[i] = llmin(mSpotLightFade[i]+fade_amt, 1.f);
			}
			else
			{ //fade out this light
				mSpotLightFade[i] = llmax(mSpotLightFade[i]-fade_amt, 0.f);
				
				if (mSpotLightFade[i] == 0.f || mShadowSpotLight[i].isNull())
				{ //faded out, grab one of the pending spots (whichever one isn't already taken)
					if (mTargetShadowSpotLight[0] != mShadowSpotLight[(i+1)%2])
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[0];
					}
					else
					{
						mShadowSpotLight[i] = mTargetShadowSpotLight[1];
					}
				}
			}
		}

		for (S32 i = 0; i < 2; i++)
		{
			glh_set_current_modelview(saved_view);
			glh_set_current_projection(saved_proj);

			if (mShadowSpotLight[i].isNull())
			{
				continue;
			}

			LLVOVolume* volume = mShadowSpotLight[i]->getVOVolume();

			if (!volume)
			{
				mShadowSpotLight[i] = NULL;
				continue;
			}

			LLDrawable* drawable = mShadowSpotLight[i];

			LLVector3 params = volume->getSpotLightParams();
			F32 fov = params.mV[0];

			//get agent->light space matrix (modelview)
			LLVector3 center = drawable->getPositionAgent();
			LLQuaternion quat = volume->getRenderRotation();

			//get near clip plane
			LLVector3 scale = volume->getScale();
			LLVector3 at_axis(0,0,-scale.mV[2]*0.5f);
			at_axis *= quat;

			LLVector3 np = center+at_axis;
			at_axis.normVec();

			//get origin that has given fov for plane np, at_axis, and given scale
			F32 dist = (scale.mV[1]*0.5f)/tanf(fov*0.5f);

			LLVector3 origin = np - at_axis*dist;

			LLMatrix4 mat(quat, LLVector4(origin, 1.f));

			view[i+4].loadu(mat.mMatrix[0]);

			view[i+4].invert();

			//get perspective matrix
			F32 near_clip = dist+0.01f;
			F32 width = scale.mV[VX];
			F32 height = scale.mV[VY];
			F32 far_clip = dist+volume->getLightRadius()*1.5f;

			F32 fovy = fov * RAD_TO_DEG;
			F32 aspect = width/height;

			proj[i+4] = gGL.genPersp(fovy, aspect, near_clip, far_clip);

			glh_set_current_modelview(view[i+4]);
			glh_set_current_projection(proj[i+4]);

			glh_set_last_modelview(mShadowModelview[i+4]);
			glh_set_last_projection(mShadowProjection[i+4]);

			mShadowModelview[i+4] = view[i+4];
			mShadowProjection[i+4] = proj[i+4];

			mSunShadowMatrix[i+4].setMul(gGL.genNDCtoWC(),proj[i+4]);
			mSunShadowMatrix[i+4].mul_affine(view[i+4]);
			mSunShadowMatrix[i+4].mul_affine(inv_view);

			LLCamera shadow_cam = camera;
			shadow_cam.setFar(far_clip);
			shadow_cam.setOrigin(origin);

			LLViewerCamera::updateFrustumPlanes(shadow_cam, FALSE, FALSE, TRUE);

			stop_glerror();

			mShadow[i+4].bindTarget();
			mShadow[i+4].getViewport(gGLViewport);
			mShadow[i+4].clear();

			U32 target_width = mShadow[i+4].getWidth();

			static LLCullResult result[2];

			LLViewerCamera::sCurCameraID = LLViewerCamera::eCameraID(LLViewerCamera::CAMERA_SHADOW0 + i + 4);

			renderShadow(view[i+4], proj[i+4], shadow_cam, result[i], FALSE, FALSE, target_width);

			mShadow[i+4].flush();
 		}
	}
	else
	{ //no spotlight shadows
		mShadowSpotLight[0] = mShadowSpotLight[1] = NULL;
	}


	if (!CameraOffset)
	{
		glh_set_current_modelview(saved_view);
		glh_set_current_projection(saved_proj);
	}
	else
	{
		glh_set_current_modelview(view[1]);
		glh_set_current_projection(proj[1]);
		gGL.loadMatrix(view[1]);
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.loadMatrix(proj[1]);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
	gGL.setColorMask(true, false);

	glh_set_last_modelview(last_modelview);
	glh_set_last_projection(last_projection);

	popRenderTypeMask();

	if (!skip_avatar_update)
	{
		gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
	}
}

void LLPipeline::renderGroups(LLRenderPass* pass, U32 type, U32 mask, BOOL texture)
{
	for (LLCullResult::sg_iterator i = sCull->beginVisibleGroups(); i != sCull->endVisibleGroups(); ++i)
	{
		LLSpatialGroup* group = *i;
		if (!group->isDead() &&
			(!sUseOcclusion || !group->isOcclusionState(LLSpatialGroup::OCCLUDED)) &&
			gPipeline.hasRenderType(group->getSpatialPartition()->mDrawableType) &&
			group->mDrawMap.find(type) != group->mDrawMap.end())
		{
			pass->renderGroup(group,type,mask,texture);
		}
	}
}

static LLTrace::BlockTimerStatHandle FTM_IMPOSTOR_MARK_VISIBLE("Impostor Mark Visible");
static LLTrace::BlockTimerStatHandle FTM_IMPOSTOR_SETUP("Impostor Setup");
static LLTrace::BlockTimerStatHandle FTM_IMPOSTOR_BACKGROUND("Impostor Background");
static LLTrace::BlockTimerStatHandle FTM_IMPOSTOR_ALLOCATE("Impostor Allocate");
static LLTrace::BlockTimerStatHandle FTM_IMPOSTOR_RESIZE("Impostor Resize");

void LLPipeline::generateImpostor(LLVOAvatar* avatar)
{
	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();
	LLGLStateValidator::checkClientArrays();

	static LLCullResult result;
	result.clear();
	grabReferences(result);
	
	if (!avatar || !avatar->mDrawable)
	{
		return;
	}

	assertInitialized();

	bool visually_muted = avatar->isVisuallyMuted();		

	pushRenderTypeMask();
	
	if (visually_muted)
	{
		andRenderTypeMask(LLPipeline::RENDER_TYPE_AVATAR, END_RENDER_TYPES);
	}
	else
	{
		andRenderTypeMask(LLPipeline::RENDER_TYPE_ALPHA,
			LLPipeline::RENDER_TYPE_FULLBRIGHT,
			LLPipeline::RENDER_TYPE_VOLUME,
			LLPipeline::RENDER_TYPE_GLOW,
						LLPipeline::RENDER_TYPE_BUMP,
						LLPipeline::RENDER_TYPE_PASS_SIMPLE,
						LLPipeline::RENDER_TYPE_PASS_ALPHA,
						LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK,
			LLPipeline::RENDER_TYPE_PASS_BUMP,
			LLPipeline::RENDER_TYPE_PASS_POST_BUMP,
						LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT,
						LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK,
						LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY,
			LLPipeline::RENDER_TYPE_PASS_GLOW,
			LLPipeline::RENDER_TYPE_PASS_GRASS,
						LLPipeline::RENDER_TYPE_PASS_SHINY,
			LLPipeline::RENDER_TYPE_AVATAR,
			LLPipeline::RENDER_TYPE_ALPHA_MASK,
			LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK,
			LLPipeline::RENDER_TYPE_SIMPLE,
						END_RENDER_TYPES);
	}
	
	S32 occlusion = sUseOcclusion;
	sUseOcclusion = 0;
	sReflectionRender = sRenderDeferred ? FALSE : TRUE;
	sShadowRender = TRUE;
	sImpostorRender = TRUE;

	LLViewerCamera* viewer_camera = LLViewerCamera::getInstance();

	{
		LL_RECORD_BLOCK_TIME(FTM_IMPOSTOR_MARK_VISIBLE);
		markVisible(avatar->mDrawable, *viewer_camera);
		LLVOAvatar::sUseImpostors = FALSE;

		/*LLVOAvatar::attachment_map_t::iterator iter;
		for (iter = avatar->mAttachmentPoints.begin();
			iter != avatar->mAttachmentPoints.end();
			++iter)
		{
			LLViewerJointAttachment *attachment = iter->second;
			for (LLViewerJointAttachment::attachedobjs_vec_t::iterator attachment_iter = attachment->mAttachedObjects.begin();
				 attachment_iter != attachment->mAttachedObjects.end();
				 ++attachment_iter)
			{
				if (LLViewerObject* attached_object = (*attachment_iter))*/
		std::vector<std::pair<LLViewerObject*,LLViewerJointAttachment*> >::iterator attachment_iter = avatar->mAttachedObjectsVector.begin();
		std::vector<std::pair<LLViewerObject*,LLViewerJointAttachment*> >::iterator end = avatar->mAttachedObjectsVector.end();
		for(;attachment_iter != end;++attachment_iter)
		{{
				if (LLViewerObject* attached_object = attachment_iter->first)
				{
					markVisible(attached_object->mDrawable->getSpatialBridge(), *viewer_camera);
				}
			}
		}
	}

	stateSort(*LLViewerCamera::getInstance(), result);
	
	LLCamera camera = *viewer_camera;
	LLVector2 tdim;
	U32 resY = 0;
	U32 resX = 0;

	{
		LL_RECORD_BLOCK_TIME(FTM_IMPOSTOR_SETUP);
		const LLVector4a* ext = avatar->mDrawable->getSpatialExtents();
		LLVector3 pos(avatar->getRenderPosition()+avatar->getImpostorOffset());

		camera.lookAt(viewer_camera->getOrigin(), pos, viewer_camera->getUpAxis());
	
		LLVector4a half_height;
		half_height.setSub(ext[1], ext[0]);
		half_height.mul(0.5f);

		LLVector4a left;
		left.load3(camera.getLeftAxis().mV);
		left.mul(left);
		llassert(left.dot3(left).getF32() > F_APPROXIMATELY_ZERO);
		left.normalize3fast();

		LLVector4a up;
		up.load3(camera.getUpAxis().mV);
		up.mul(up);
		llassert(up.dot3(up).getF32() > F_APPROXIMATELY_ZERO);
		up.normalize3fast();

		tdim.mV[0] = fabsf(half_height.dot3(left).getF32());
		tdim.mV[1] = fabsf(half_height.dot3(up).getF32());

		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
	
		F32 distance = (pos-camera.getOrigin()).length();
		F32 fov = atanf(tdim.mV[1]/distance)*2.f*RAD_TO_DEG;
		F32 aspect = tdim.mV[0]/tdim.mV[1];
		LLMatrix4a persp = gGL.genPersp(fov, aspect, 1.f, 256.f);
		glh_set_current_projection(persp);
		gGL.loadMatrix(persp);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		LLMatrix4a mat;
		camera.getOpenGLTransform(mat.getF32ptr());

		mat.setMul(OGL_TO_CFR_ROTATION, mat);

		gGL.loadMatrix(mat);

		glh_set_current_modelview(mat);

		glClearColor(0.0f,0.0f,0.0f,0.0f);
		gGL.setColorMask(true, true);
	
		// get the number of pixels per angle
		F32 pa = gViewerWindow->getWindowHeightRaw() / (RAD_TO_DEG * viewer_camera->getView());

		//get resolution based on angle width and height of impostor (double desired resolution to prevent aliasing)
		resY = llmin(nhpo2((U32) (fov*pa)), (U32) 512);
		resX = llmin(nhpo2((U32) (atanf(tdim.mV[0]/distance)*2.f*RAD_TO_DEG*pa)), (U32) 512);

		if (!avatar->mImpostor.isComplete())
		{
			LL_RECORD_BLOCK_TIME(FTM_IMPOSTOR_ALLOCATE);
			

			if (LLPipeline::sRenderDeferred)
			{
				avatar->mImpostor.allocate(resX,resY,GL_SRGB8_ALPHA8,TRUE,FALSE);
				addDeferredAttachments(avatar->mImpostor);
			}
			else
			{
				avatar->mImpostor.allocate(resX,resY,GL_RGBA,TRUE,FALSE);
			}
		
			gGL.getTexUnit(0)->bind(&avatar->mImpostor);
			gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		}
		else if(resX != avatar->mImpostor.getWidth() || resY != avatar->mImpostor.getHeight())
		{
			LL_RECORD_BLOCK_TIME(FTM_IMPOSTOR_RESIZE);
			avatar->mImpostor.resize(resX,resY);
		}

		avatar->mImpostor.bindTarget();
	}

	F32 old_alpha = LLDrawPoolAvatar::sMinimumAlpha;

	if (visually_muted)
	{ //disable alpha masking for muted avatars (get whole skin silhouette)
		LLDrawPoolAvatar::sMinimumAlpha = 0.f;
	}

	if (LLPipeline::sRenderDeferred)
	{
		avatar->mImpostor.clear();
		renderGeomDeferred(camera);

		renderGeomPostDeferred(camera);		

		// Shameless hack time: render it all again,
		// this time writing the depth
		// values we need to generate the alpha mask below
		// while preserving the alpha-sorted color rendering
		// from the previous pass
		//
		sImpostorRenderAlphaDepthPass = true;
		// depth-only here...
		//
		gGL.setColorMask(false,false);
		renderGeomPostDeferred(camera);

		sImpostorRenderAlphaDepthPass = false;

	}
	else
	{
		LLGLEnable<GL_SCISSOR_TEST> scissor;
		gGL.setScissor(0, 0, resX, resY);
		avatar->mImpostor.clear();
		renderGeom(camera);

		// Shameless hack time: render it all again,
		// this time writing the depth
		// values we need to generate the alpha mask below
		// while preserving the alpha-sorted color rendering
		// from the previous pass
		//
		sImpostorRenderAlphaDepthPass = true;

		// depth-only here...
		//
		gGL.setColorMask(false,false);
		renderGeom(camera);

		sImpostorRenderAlphaDepthPass = false;
	}

	LLDrawPoolAvatar::sMinimumAlpha = old_alpha;

	{ //create alpha mask based on depth buffer (grey out if muted)
		LL_RECORD_BLOCK_TIME(FTM_IMPOSTOR_BACKGROUND);
		if (LLPipeline::sRenderDeferred)
		{
			GLuint buff = GL_COLOR_ATTACHMENT0;
			glDrawBuffersARB(1, &buff);
		}

		LLGLDisable<GL_BLEND> blend;

		if (visually_muted)
		{
			gGL.setColorMask(true, true);
		}
		else
		{
			gGL.setColorMask(false, true);
		}
		
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

		gGL.flush();

		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();

		static const F32 clip_plane = 0.99999f;

		if (LLGLSLShader::sNoFixedFunction)
		{
			gDebugProgram.bind();
		}

		gGL.diffuseColor4ub(64,64,64,255);
		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.vertex3f(-1, -1, clip_plane);
		gGL.vertex3f(1, -1, clip_plane);
		gGL.vertex3f(-1, 1, clip_plane);
		gGL.vertex3f(1, 1, clip_plane);
		gGL.end();
		gGL.flush();

		if (LLGLSLShader::sNoFixedFunction)
		{
			gDebugProgram.unbind();
		}

		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
	}

	avatar->mImpostor.flush();

	avatar->setImpostorDim(tdim);

	LLVOAvatar::sUseImpostors = true;
	sUseOcclusion = occlusion;
	sReflectionRender = false;
	sImpostorRender = false;
	sShadowRender = false;
	popRenderTypeMask();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	avatar->mNeedsImpostorUpdate = FALSE;
	avatar->cacheImpostorValues();
	avatar->mLastImpostorUpdateFrameTime = gFrameTimeSeconds;

	LLVertexBuffer::unbind();
	LLGLStateValidator::checkStates();
	LLGLStateValidator::checkTextureChannels();
	LLGLStateValidator::checkClientArrays();
}

BOOL LLPipeline::hasRenderBatches(const U32 type) const
{
	return sCull->hasRenderMap(type);
}

LLCullResult::drawinfo_iterator LLPipeline::beginRenderMap(U32 type) const
{
	return sCull->beginRenderMap(type);
}

LLCullResult::drawinfo_iterator LLPipeline::endRenderMap(U32 type) const
{
	return sCull->endRenderMap(type);
}

LLCullResult::sg_iterator LLPipeline::beginAlphaGroups() const
{
	return sCull->beginAlphaGroups();
}

LLCullResult::sg_iterator LLPipeline::endAlphaGroups() const
{
	return sCull->endAlphaGroups();
}

BOOL LLPipeline::hasRenderType(const U32 type) const
{
    // STORM-365 : LLViewerJointAttachment::setAttachmentVisibility() is setting type to 0 to actually mean "do not render"
    // We then need to test that value here and return FALSE to prevent attachment to render (in mouselook for instance)
    // TODO: reintroduce RENDER_TYPE_NONE in LLRenderTypeMask and initialize its mRenderTypeEnabled[RENDER_TYPE_NONE] to FALSE explicitely
	return (type == 0 ? FALSE : mRenderTypeEnabled[type]);
}

void LLPipeline::setRenderTypeMask(U32 type, ...)
{
	va_list args;

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = TRUE;
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		LL_ERRS() << "Invalid render type." << LL_ENDL;
	}
}

BOOL LLPipeline::hasAnyRenderType(U32 type, ...) const
{
	va_list args;

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type])
		{
			va_end(args);
			return TRUE;
		}
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		LL_ERRS() << "Invalid render type." << LL_ENDL;
	}

	return FALSE;
}

void LLPipeline::pushRenderTypeMask()
{
	std::string cur_mask;
	cur_mask.assign((const char*) mRenderTypeEnabled, sizeof(mRenderTypeEnabled));
	mRenderTypeEnableStack.push(cur_mask);
}

void LLPipeline::popRenderTypeMask()
{
	if (mRenderTypeEnableStack.empty())
	{
		LL_ERRS() << "Depleted render type stack." << LL_ENDL;
	}

	memcpy(mRenderTypeEnabled, mRenderTypeEnableStack.top().data(), sizeof(mRenderTypeEnabled));
	mRenderTypeEnableStack.pop();
}

void LLPipeline::andRenderTypeMask(U32 type, ...)
{
	va_list args;

	BOOL tmp[NUM_RENDER_TYPES];
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		tmp[i] = FALSE;
	}

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		if (mRenderTypeEnabled[type]) 
		{
			tmp[type] = TRUE;
		}

		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		LL_ERRS() << "Invalid render type." << LL_ENDL;
	}

	for (U32 i = 0; i < LLPipeline::NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = tmp[i];
	}

}

void LLPipeline::clearRenderTypeMask(U32 type, ...)
{
	va_list args;

	va_start(args, type);
	while (type < END_RENDER_TYPES)
	{
		mRenderTypeEnabled[type] = FALSE;
		
		type = va_arg(args, U32);
	}
	va_end(args);

	if (type > END_RENDER_TYPES)
	{
		LL_ERRS() << "Invalid render type." << LL_ENDL;
	}
}

void LLPipeline::setAllRenderTypes()
{
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = TRUE;
	}
}

void LLPipeline::clearAllRenderTypes()
{
	for (U32 i = 0; i < NUM_RENDER_TYPES; ++i)
	{
		mRenderTypeEnabled[i] = FALSE;
	}
}

void LLPipeline::addDebugBlip(const LLVector3& position, const LLColor4& color)
{
	DebugBlip blip(position, color);
	mDebugBlips.push_back(blip);
}

/* Singu Note: This is currently only used upstream by code that requires havok
void LLPipeline::hideObject( const LLUUID& id )
{
	LLViewerObject *pVO = gObjectList.findObject( id );

	if ( pVO )
	{
		LLDrawable *pDrawable = pVO->mDrawable;

		if ( pDrawable )
		{
			hideDrawable( pDrawable );
		}
	}
}

void LLPipeline::hideDrawable( LLDrawable *pDrawable )
{
	pDrawable->setState( LLDrawable::FORCE_INVISIBLE );
	markRebuild( pDrawable, LLDrawable::REBUILD_ALL, TRUE );
	//hide the children
	LLViewerObject::const_child_list_t& child_list = pDrawable->getVObj()->getChildren();
	for ( LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		  iter != child_list.end(); iter++ )
	{
		LLViewerObject* child = *iter;
		LLDrawable* drawable = child->mDrawable;
		if ( drawable )
		{
			drawable->setState( LLDrawable::FORCE_INVISIBLE );
			markRebuild( drawable, LLDrawable::REBUILD_ALL, TRUE );
		}
	}
}

void LLPipeline::unhideDrawable( LLDrawable *pDrawable )
{
	pDrawable->clearState( LLDrawable::FORCE_INVISIBLE );
	markRebuild( pDrawable, LLDrawable::REBUILD_ALL, TRUE );
	//restore children
	LLViewerObject::const_child_list_t& child_list = pDrawable->getVObj()->getChildren();
	for ( LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		  iter != child_list.end(); iter++)
	{
		LLViewerObject* child = *iter;
		LLDrawable* drawable = child->mDrawable;
		if ( drawable )
		{
			drawable->clearState( LLDrawable::FORCE_INVISIBLE );
			markRebuild( drawable, LLDrawable::REBUILD_ALL, TRUE );
		}
	}
}

void LLPipeline::restoreHiddenObject( const LLUUID& id )
{
	LLViewerObject *pVO = gObjectList.findObject( id );

	if ( pVO )
	{
		LLDrawable *pDrawable = pVO->mDrawable;
		if ( pDrawable )
		{
			unhideDrawable( pDrawable );
		}
	}
}
*/

void LLPipeline::drawFullScreenRect()
{
	if(mAuxScreenRectVB.isNull())
	{
		mAuxScreenRectVB = new LLVertexBuffer(AUX_VB_MASK, 0);
		mAuxScreenRectVB->allocateBuffer(3, 0, true);
		LLStrider<LLVector3> vert;
		mAuxScreenRectVB->getVertexStrider(vert);

		vert[0].set(-1.f,-1.f,0.f);
		vert[1].set(3.f,-1.f,0.f);
		vert[2].set(-1.f,3.f,0.f);

	}
	mAuxScreenRectVB->setBuffer(LLVertexBuffer::MAP_VERTEX);
	mAuxScreenRectVB->drawArrays(LLRender::TRIANGLES, 0, 3);
}

