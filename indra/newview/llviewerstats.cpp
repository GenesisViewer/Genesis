/** 
 * @file llviewerstats.cpp
 * @brief LLViewerStats class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llviewerstats.h"
#include "llviewerthrottle.h"

#include "message.h"
#include "lltimer.h"

#include "llappviewer.h"

#include "pipeline.h" 
#include "lltexturefetch.h" 
#include "llviewerobjectlist.h" 
#include "llviewertexturelist.h" 
#include "lltexlayer.h"
#include "llsurface.h"
#include "llvlmanager.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llfloaterdirectory.h"
#include "llversioninfo.h"
#include "llfloatertools.h"
#include "lldebugview.h"
#include "llfasttimerview.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llviewerwindow.h"		// *TODO: remove, only used for width/height
#include "llworld.h"
#include "llfeaturemanager.h"
#include "llviewernetwork.h"
#include "llmeshrepository.h" //for LLMeshRepository::sBytesReceived
#include "sgmemstat.h"
#include "llviewertexlayer.h"

class AIHTTPTimeoutPolicy;
extern AIHTTPTimeoutPolicy viewerStatsResponder_timeout;

namespace AICurlInterface {
size_t getHTTPBandwidth(void);
}

class StatAttributes
{
public:
	StatAttributes(const char* name,
				   const BOOL enabled,
				   const BOOL is_timer)
		: mName(name),
		  mEnabled(enabled),
		  mIsTimer(is_timer)
	{
	}
	
	std::string mName;
	BOOL mEnabled;
	BOOL mIsTimer;
};

const StatAttributes STAT_INFO[LLViewerStats::ST_COUNT] =
{
	// ST_VERSION
	StatAttributes("Version", TRUE, FALSE),
	// ST_AVATAR_EDIT_SECONDS
	StatAttributes("Seconds in Edit Appearance", FALSE, TRUE),
	// ST_TOOLBOX_SECONDS
	StatAttributes("Seconds using Toolbox", FALSE, TRUE),
	// ST_CHAT_COUNT
	StatAttributes("Chat messages sent", FALSE, FALSE),
	// ST_IM_COUNT
	StatAttributes("IMs sent", FALSE, FALSE),
	// ST_FULLSCREEN_BOOL
	StatAttributes("Fullscreen mode", FALSE, FALSE),
	// ST_RELEASE_COUNT
	StatAttributes("Object release count", FALSE, FALSE),
	// ST_CREATE_COUNT
	StatAttributes("Object create count", FALSE, FALSE),
	// ST_REZ_COUNT
	StatAttributes("Object rez count", FALSE, FALSE),
	// ST_FPS_10_SECONDS
	StatAttributes("Seconds below 10 FPS", FALSE, TRUE),
	// ST_FPS_2_SECONDS
	StatAttributes("Seconds below 2 FPS", FALSE, TRUE),
	// ST_MOUSELOOK_SECONDS
	StatAttributes("Seconds in Mouselook", FALSE, TRUE),
	// ST_FLY_COUNT
	StatAttributes("Fly count", FALSE, FALSE),
	// ST_TELEPORT_COUNT
	StatAttributes("Teleport count", FALSE, FALSE),
	// ST_OBJECT_DELETE_COUNT
	StatAttributes("Objects deleted", FALSE, FALSE),
	// ST_SNAPSHOT_COUNT
	StatAttributes("Snapshots taken", FALSE, FALSE),
	// ST_UPLOAD_SOUND_COUNT
	StatAttributes("Sounds uploaded", FALSE, FALSE),
	// ST_UPLOAD_TEXTURE_COUNT
	StatAttributes("Textures uploaded", FALSE, FALSE),
	// ST_EDIT_TEXTURE_COUNT
	StatAttributes("Changes to textures on objects", FALSE, FALSE),
	// ST_KILLED_COUNT
	StatAttributes("Number of times killed", FALSE, FALSE),
	// ST_FRAMETIME_JITTER
	StatAttributes("Average delta between sucessive frame times", FALSE, FALSE),
	// ST_FRAMETIME_SLEW
	StatAttributes("Average delta between frame time and mean", FALSE, FALSE),
	// ST_INVENTORY_TOO_LONG
	StatAttributes("Inventory took too long to load", FALSE, FALSE),
	// ST_WEARABLES_TOO_LONG
	StatAttributes("Wearables took too long to load", FALSE, FALSE),
	// ST_LOGIN_SECONDS
	StatAttributes("Time between LoginRequest and LoginReply", FALSE, FALSE),
	// ST_LOGIN_TIMEOUT_COUNT
	StatAttributes("Number of login attempts that timed out", FALSE, FALSE),
	// ST_HAS_BAD_TIMER
	StatAttributes("Known bad timer if != 0.0", FALSE, FALSE),
	// ST_DOWNLOAD_FAILED
	StatAttributes("Number of times LLAssetStorage::getAssetData() has failed", FALSE, FALSE),
	// ST_LSL_SAVE_COUNT
	StatAttributes("Number of times user has saved a script", FALSE, FALSE),
	// ST_UPLOAD_ANIM_COUNT
	StatAttributes("Animations uploaded", FALSE, FALSE),
	// ST_FPS_8_SECONDS
	StatAttributes("Seconds below 8 FPS", FALSE, TRUE),
	// ST_SIM_FPS_20_SECONDS
	StatAttributes("Seconds with sim FPS below 20", FALSE, TRUE),
	// ST_PHYS_FPS_20_SECONDS
	StatAttributes("Seconds with physics FPS below 20", FALSE, TRUE),
	// ST_LOSS_05_SECONDS
	StatAttributes("Seconds with packet loss > 5%", FALSE, TRUE),
	// ST_FPS_DROP_50_RATIO
	StatAttributes("Ratio of frames 2x longer than previous", FALSE, FALSE),
	// ST_ENABLE_VBO
	StatAttributes("Vertex Buffers Enabled", TRUE, FALSE),
	// ST_DELTA_BANDWIDTH
	StatAttributes("Increase/Decrease in bandwidth based on packet loss", FALSE, FALSE),
	// ST_MAX_BANDWIDTH
	StatAttributes("Max bandwidth setting", FALSE, FALSE),
	// ST_LIGHTING_DETAIL
	StatAttributes("Lighting Detail", FALSE, FALSE),
	// ST_VISIBLE_AVATARS
	StatAttributes("Visible Avatars", FALSE, FALSE),
	// ST_SHADER_OJECTS
	StatAttributes("Object Shaders", FALSE, FALSE),
	// ST_SHADER_ENVIRONMENT
	StatAttributes("Environment Shaders", FALSE, FALSE),
	// ST_VISIBLE_DRAW_DIST
	StatAttributes("Draw Distance", FALSE, FALSE),
	// ST_VISIBLE_CHAT_BUBBLES
	StatAttributes("Chat Bubbles Enabled", FALSE, FALSE),
	// ST_SHADER_AVATAR
	StatAttributes("Avatar Shaders", FALSE, FALSE),
	// ST_FRAME_SECS
	StatAttributes("FRAME_SECS", FALSE, FALSE),
	// ST_UPDATE_SECS
	StatAttributes("UPDATE_SECS", FALSE, FALSE),
	// ST_NETWORK_SECS
	StatAttributes("NETWORK_SECS", FALSE, FALSE),
	// ST_IMAGE_SECS
	StatAttributes("IMAGE_SECS", FALSE, FALSE),
	// ST_REBUILD_SECS
	StatAttributes("REBUILD_SECS", FALSE, FALSE),
	// ST_RENDER_SECS
	StatAttributes("RENDER_SECS", FALSE, FALSE),
	// ST_CROSSING_AVG
	StatAttributes("CROSSING_AVG", FALSE, FALSE),
	// ST_CROSSING_MAX
	StatAttributes("CROSSING_MAX", FALSE, FALSE),
	// ST_LIBXUL_WIDGET_USED
	StatAttributes("LibXUL Widget used", FALSE, FALSE), // Unused
	// ST_WINDOW_WIDTH
	StatAttributes("Window width", FALSE, FALSE),
	// ST_WINDOW_HEIGHT
	StatAttributes("Window height", FALSE, FALSE),
	// ST_TEX_BAKES
	StatAttributes("Texture Bakes", FALSE, FALSE),
	// ST_TEX_REBAKES
	StatAttributes("Texture Rebakes", FALSE, FALSE)

};

LLViewerStats::LLViewerStats() :
	mKBitStat("kbitstat"),
	mLayersKBitStat("layerskbitstat"),
	mObjectKBitStat("objectkbitstat"),
	mAssetKBitStat("assetkbitstat"),
	mHTTPTextureKBitStat("httptexturekbitstat"),
	mUDPTextureKBitStat("udptexturekbitstat"),
	mMallocStat("mallocstat"),
	mVFSPendingOperations("vfspendingoperations"),
	mObjectsDrawnStat("objectsdrawnstat"),
	mObjectsCulledStat("objectsculledstat"),
	mObjectsTestedStat("objectstestedstat"),
	mObjectsComparedStat("objectscomparedstat"),
	mObjectsOccludedStat("objectsoccludedstat"),
	mFPSStat("fpsstat"),
	mPacketsInStat("packetsinstat"),
	mPacketsLostStat("packetsloststat"),
	mPacketsOutStat("packetsoutstat"),
	mPacketsLostPercentStat("packetslostpercentstat", 64),
	mTexturePacketsStat("texturepacketsstat"),
	mActualInKBitStat("actualinkbitstat"),
	mActualOutKBitStat("actualoutkbitstat"),
	mTrianglesDrawnStat("trianglesdrawnstat"),
	mSimTimeDilation("simtimedilation"),
	mSimFPS("simfps"),
	mSimPhysicsFPS("simphysicsfps"),
	mSimAgentUPS("simagentups"),
	mSimScriptEPS("simscripteps"),
	mSimFrameMsec("simframemsec"),
	mSimNetMsec("simnetmsec"),
	mSimSimOtherMsec("simsimothermsec"),
	mSimSimPhysicsMsec("simsimphysicsmsec"),
	mSimSimPhysicsStepMsec("simsimphysicsstepmsec"),
	mSimSimPhysicsShapeUpdateMsec("simsimphysicsshapeupdatemsec"),
	mSimSimPhysicsOtherMsec("simsimphysicsothermsec"),
	mSimSimAIStepMsec("simsimaistepmsec"),
	mSimSimSkippedSilhouetteSteps("simsimskippedsilhouettesteps"),
	mSimSimPctSteppedCharacters("simsimpctsteppedcharacters"),
	mSimAgentMsec("simagentmsec"),
	mSimImagesMsec("simimagesmsec"),
	mSimScriptMsec("simscriptmsec"),
	mSimSpareMsec("simsparemsec"),
	mSimSleepMsec("simsleepmsec"),
	mSimPumpIOMsec("simpumpiomsec"),
	mSimMainAgents("simmainagents"),
	mSimChildAgents("simchildagents"),
	mSimObjects("simobjects"),
	mSimActiveObjects("simactiveobjects"),
	mSimActiveScripts("simactivescripts"),
	mSimPctScriptsRun("simpctscriptsrun"),
	mSimInPPS("siminpps"),
	mSimOutPPS("simoutpps"),
	mSimPendingDownloads("simpendingdownloads"),
	mSimPendingUploads("simpendinguploads"),
	mSimPendingLocalUploads("simpendinglocaluploads"),
	mSimTotalUnackedBytes("simtotalunackedbytes"),
	mPhysicsPinnedTasks("physicspinnedtasks"),
	mPhysicsLODTasks("physicslodtasks"),
	mPhysicsMemoryAllocated("physicsmemoryallocated"),
	mSimPingStat("simpingstat"),
	mNumImagesStat("numimagesstat", 32, TRUE),
	mNumRawImagesStat("numrawimagesstat", 32, TRUE),
	mGLTexMemStat("gltexmemstat", 32, TRUE),
	mGLBoundMemStat("glboundmemstat", 32, TRUE),
	mRawMemStat("rawmemstat", 32, TRUE),
	mFormattedMemStat("formattedmemstat", 32, TRUE),
	mNumObjectsStat("numobjectsstat"),
	mNumActiveObjectsStat("numactiveobjectsstat"),
	mNumNewObjectsStat("numnewobjectsstat"),
	mNumSizeCulledStat("numsizeculledstat"),
	mNumVisCulledStat("numvisculledstat"),
	mLastTimeDiff(0.0)
{
	for (S32 i = 0; i < ST_COUNT; i++)
	{
		mStats[i] = 0.0;
	}
	
	if (LLTimer::knownBadTimer())
	{
		mStats[ST_HAS_BAD_TIMER] = 1.0;
	}	
	
	mAgentPositionSnaps.reset();
}

LLViewerStats::~LLViewerStats()
{
}

void LLViewerStats::resetStats()
{
	LLViewerStats& stats = LLViewerStats::instance();
	stats.mKBitStat.reset();
	stats.mLayersKBitStat.reset();
	stats.mObjectKBitStat.reset();
	stats.mHTTPTextureKBitStat.reset();
	stats.mUDPTextureKBitStat.reset();
	stats.mVFSPendingOperations.reset();
	stats.mAssetKBitStat.reset();
	stats.mPacketsInStat.reset();
	stats.mPacketsLostStat.reset();
	stats.mPacketsOutStat.reset();
	stats.mFPSStat.reset();
	stats.mTexturePacketsStat.reset();
	stats.mAgentPositionSnaps.reset();
}


F64 LLViewerStats::getStat(EStatType type) const
{
	return mStats[type];
}

F64 LLViewerStats::setStat(EStatType type, F64 value)
{
	mStats[type] = value;
	return mStats[type];
}

F64 LLViewerStats::incStat(EStatType type, F64 value)
{
	mStats[type] += value;
	return mStats[type];
}

void LLViewerStats::updateFrameStats(const F64 time_diff)
{
	if (mPacketsLostPercentStat.getCurrent() > 5.0)
	{
		incStat(ST_LOSS_05_SECONDS, time_diff);
	}
	
	if (mSimFPS.getCurrent() < 20.f && mSimFPS.getCurrent() > 0.f)
	{
		incStat(ST_SIM_FPS_20_SECONDS, time_diff);
	}
	
	if (mSimPhysicsFPS.getCurrent() < 20.f && mSimPhysicsFPS.getCurrent() > 0.f)
	{
		incStat(ST_PHYS_FPS_20_SECONDS, time_diff);
	}
		
	if (time_diff >= 0.5)
	{
		incStat(ST_FPS_2_SECONDS, time_diff);
	}
	if (time_diff >= 0.125)
	{
		incStat(ST_FPS_8_SECONDS, time_diff);
	}
	if (time_diff >= 0.1)
	{
		incStat(ST_FPS_10_SECONDS, time_diff);
	}

	if (gFrameCount && mLastTimeDiff > 0.0)
	{
		// new "stutter" meter
		setStat(ST_FPS_DROP_50_RATIO,
				(getStat(ST_FPS_DROP_50_RATIO) * (F64)(gFrameCount - 1) + 
				 (time_diff >= 2.0 * mLastTimeDiff ? 1.0 : 0.0)) / gFrameCount);
			

		// old stats that were never really used
		setStat(ST_FRAMETIME_JITTER,
				(getStat(ST_FRAMETIME_JITTER) * (gFrameCount - 1) + 
				 fabs(mLastTimeDiff - time_diff) / mLastTimeDiff) / gFrameCount);
			
		F32 average_frametime = gRenderStartTime.getElapsedTimeF32() / (F32)gFrameCount;
		setStat(ST_FRAMETIME_SLEW,
				(getStat(ST_FRAMETIME_SLEW) * (gFrameCount - 1) + 
				 fabs(average_frametime - time_diff) / average_frametime) / gFrameCount);

		F32 max_bandwidth = gViewerThrottle.getMaxBandwidth();
		F32 delta_bandwidth = gViewerThrottle.getCurrentBandwidth() - max_bandwidth;
		setStat(ST_DELTA_BANDWIDTH, delta_bandwidth / 1024.f);

		setStat(ST_MAX_BANDWIDTH, max_bandwidth / 1024.f);
		
	}
	
	mLastTimeDiff = time_diff;

}

void LLViewerStats::addToMessage(LLSD &body) const
{
	LLSD &misc = body["misc"];
	
	for (S32 i = 0; i < ST_COUNT; i++)
	{
		if (STAT_INFO[i].mEnabled)
		{
			// TODO: send timer value so dataserver can normalize
			misc[STAT_INFO[i].mName] = mStats[i];
			LL_INFOS() << "STAT: " << STAT_INFO[i].mName << ": " << mStats[i]
					<< LL_ENDL;
		}
	}
	
	body["AgentPositionSnaps"] = mAgentPositionSnaps.getData();
	LL_INFOS() << "STAT: AgentPositionSnaps: Mean = " << mAgentPositionSnaps.getMean() << "; StdDev = " << mAgentPositionSnaps.getStdDev() 
			<< "; Count = " << mAgentPositionSnaps.getCount() << LL_ENDL;
}

// static
// const std::string LLViewerStats::statTypeToText(EStatType type)
// {
// 	if (type >= 0 && type < ST_COUNT)
// 	{
// 		return STAT_INFO[type].mName;
// 	}
// 	else
// 	{
// 		return "Unknown statistic";
// 	}
// }

// *NOTE:Mani The following methods used to exist in viewer.cpp
// Moving them here, but not merging them into LLViewerStats yet.
void reset_statistics()
{
	if (LLSurface::sTextureUpdateTime)
	{
		LLSurface::sTexelsUpdatedPerSecStat.addValue(0.001f*(LLSurface::sTexelsUpdated / LLSurface::sTextureUpdateTime));
		LLSurface::sTexelsUpdated = 0;
		LLSurface::sTextureUpdateTime = 0.f;
	}
}


void output_statistics(void*)
{
	S64 global_raw_memory;
	{
		global_raw_memory = *AIAccess<S64>(LLImageRaw::sGlobalRawMemory);
	}
	LL_INFOS() << "Number of orphans: " << gObjectList.getOrphanCount() << LL_ENDL;
	LL_INFOS() << "Number of dead objects: " << gObjectList.mNumDeadObjects << LL_ENDL;
	LL_INFOS() << "Num images: " << gTextureList.getNumImages() << LL_ENDL;
	LL_INFOS() << "Texture usage: " << LLImageGL::sGlobalTextureMemory.value() << LL_ENDL;
	LL_INFOS() << "Texture working set: " << LLImageGL::sBoundTextureMemory.value() << LL_ENDL;
	LL_INFOS() << "Raw usage: " << global_raw_memory << LL_ENDL;
	LL_INFOS() << "Formatted usage: " << LLImageFormatted::sGlobalFormattedMemory << LL_ENDL;
	LL_INFOS() << "Zombie Viewer Objects: " << LLViewerObject::getNumZombieObjects() << LL_ENDL;
	LL_INFOS() << "Number of lights: " << gPipeline.getLightCount() << LL_ENDL;

	LL_INFOS() << "Memory Usage:" << LL_ENDL;
	LL_INFOS() << "--------------------------------" << LL_ENDL;
	LL_INFOS() << "Pipeline:" << LL_ENDL;
	LL_INFOS() << LL_ENDL;

#if LL_SMARTHEAP
	LL_INFOS() << "--------------------------------" << LL_ENDL;
	{
		LL_INFOS() << "sizeof(LLVOVolume) = " << sizeof(LLVOVolume) << LL_ENDL;

		U32 total_pool_size = 0;
		U32 total_used_size = 0;
		MEM_POOL_INFO pool_info;
		MEM_POOL_STATUS pool_status;
		U32 pool_num = 0;
		for(pool_status = MemPoolFirst( &pool_info, 1 ); 
			pool_status != MEM_POOL_END; 
			pool_status = MemPoolNext( &pool_info, 1 ) )
		{
			LL_INFOS() << "Pool #" << pool_num << LL_ENDL;
			if( MEM_POOL_OK != pool_status )
			{
				LL_WARNS() << "Pool not ok" << LL_ENDL;
				continue;
			}

			LL_INFOS() << "Pool blockSizeFS " << pool_info.blockSizeFS
				<< " pageSize " << pool_info.pageSize
				<< LL_ENDL;

			U32 pool_count = MemPoolCount(pool_info.pool);
			LL_INFOS() << "Blocks " << pool_count << LL_ENDL;

			U32 pool_size = MemPoolSize( pool_info.pool );
			if( pool_size == MEM_ERROR_RET )
			{
				LL_INFOS() << "MemPoolSize() failed (" << pool_num << ")" << LL_ENDL;
			}
			else
			{
				LL_INFOS() << "MemPool Size " << pool_size / 1024 << "K" << LL_ENDL;
			}

			total_pool_size += pool_size;

			if( !MemPoolLock( pool_info.pool ) )
			{
				LL_INFOS() << "MemPoolLock failed (" << pool_num << ") " << LL_ENDL;
				continue;
			}

			U32 used_size = 0; 
			MEM_POOL_ENTRY entry;
			entry.entry = NULL;
			while( MemPoolWalk( pool_info.pool, &entry ) == MEM_POOL_OK )
			{
				if( entry.isInUse )
				{
					used_size += entry.size;
				}
			}

			MemPoolUnlock( pool_info.pool );

			LL_INFOS() << "MemPool Used " << used_size/1024 << "K" << LL_ENDL;
			total_used_size += used_size;
			pool_num++;
		}
		
		LL_INFOS() << "Total Pool Size " << total_pool_size/1024 << "K" << LL_ENDL;
		LL_INFOS() << "Total Used Size " << total_used_size/1024 << "K" << LL_ENDL;

	}
#endif

	LL_INFOS() << "--------------------------------" << LL_ENDL;
	LL_INFOS() << "Avatar Memory (partly overlaps with above stats):" << LL_ENDL;
	LLTexLayerStaticImageList::getInstance()->dumpByteCount();
	LLViewerTexLayerSetBuffer::dumpTotalByteCount();
	LLVOAvatarSelf::dumpTotalLocalTextureByteCount();
	LLTexLayerParamAlpha::dumpCacheByteCount();
	LLVOAvatar::dumpBakedStatus();

	LL_INFOS() << LL_ENDL;

	LL_INFOS() << "Object counts:" << LL_ENDL;
	S32 i;
	S32 obj_counts[256];
//	S32 app_angles[256];
	for (i = 0; i < 256; i++)
	{
		obj_counts[i] = 0;
	}
	for (i = 0; i < gObjectList.getNumObjects(); i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
			obj_counts[objectp->getPCode()]++;
		}
	}
	for (i = 0; i < 256; i++)
	{
		if (obj_counts[i])
		{
			LL_INFOS() << LLPrimitive::pCodeToString(i) << ":" << obj_counts[i] << LL_ENDL;
		}
	}
}


U32		gTotalLandIn = 0, 
		gTotalLandOut = 0,
		gTotalWaterIn = 0, 
		gTotalWaterOut = 0;

F32		gAveLandCompression = 0.f, 
		gAveWaterCompression = 0.f,
		gBestLandCompression = 1.f,
		gBestWaterCompression = 1.f,
		gWorstLandCompression = 0.f, 
		gWorstWaterCompression = 0.f;

U32Bytes				gTotalWorldData, 
								gTotalObjectData, 
								gTotalTextureData;
U32								gSimPingCount = 0;
U32Bits				gObjectData;
F32Milliseconds		gAvgSimPing(0.f);
U32Bytes     gTotalTextureBytesPerBoostLevel[LLGLTexture::MAX_GL_IMAGE_CATEGORY] = {U32Bytes(0)};

extern U32  gVisCompared;
extern U32  gVisTested;

LLFrameTimer gTextureTimer;

void update_statistics()
{
	gTotalWorldData += U32Bytes(gVLManager.getTotalBytes());
	gTotalObjectData += gObjectData;

	LLViewerStats& stats = LLViewerStats::instance();

	// make sure we have a valid time delta for this frame
	if (gFrameIntervalSeconds > 0.f)
	{
		if (gAgentCamera.getCameraMode() == CAMERA_MODE_MOUSELOOK)
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_MOUSELOOK_SECONDS, gFrameIntervalSeconds);
		}
		else if (gAgentCamera.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_AVATAR_EDIT_SECONDS, gFrameIntervalSeconds);
		}
		else if (gFloaterTools && gFloaterTools->getVisible())
		{
			LLViewerStats::getInstance()->incStat(LLViewerStats::ST_TOOLBOX_SECONDS, gFrameIntervalSeconds);
		}
	}
	stats.setStat(LLViewerStats::ST_ENABLE_VBO, (F64)gSavedSettings.getBOOL("RenderVBOEnable"));
	stats.setStat(LLViewerStats::ST_LIGHTING_DETAIL, (F64)gPipeline.isLocalLightingEnabled());
	stats.setStat(LLViewerStats::ST_DRAW_DIST, (F64)gSavedSettings.getF32("RenderFarClip"));
	stats.setStat(LLViewerStats::ST_CHAT_BUBBLES, (F64)gSavedSettings.getBOOL("UseChatBubbles"));
#if 0 // 1.9.2
	LLViewerStats::getInstance()->setStat(LLViewerStats::ST_SHADER_OBJECTS, (F64)gSavedSettings.getS32("VertexShaderLevelObject"));
	LLViewerStats::getInstance()->setStat(LLViewerStats::ST_SHADER_AVATAR, (F64)gSavedSettings.getBOOL("VertexShaderLevelAvatar"));
	LLViewerStats::getInstance()->setStat(LLViewerStats::ST_SHADER_ENVIRONMENT, (F64)gSavedSettings.getBOOL("VertexShaderLevelEnvironment"));
#endif
	stats.setStat(LLViewerStats::ST_FRAME_SECS, gDebugView->mFastTimerView->getTime("Frame"));
	F64 idle_secs = gDebugView->mFastTimerView->getTime("Idle");
	F64 network_secs = gDebugView->mFastTimerView->getTime("Network");
	stats.setStat(LLViewerStats::ST_UPDATE_SECS, idle_secs - network_secs);
	stats.setStat(LLViewerStats::ST_NETWORK_SECS, network_secs);
	stats.setStat(LLViewerStats::ST_IMAGE_SECS, gDebugView->mFastTimerView->getTime("Update Images"));
	stats.setStat(LLViewerStats::ST_REBUILD_SECS, gDebugView->mFastTimerView->getTime("Sort Draw State"));
	stats.setStat(LLViewerStats::ST_RENDER_SECS, gDebugView->mFastTimerView->getTime("Geometry"));
		
	LLCircuitData *cdp = gMessageSystem->mCircuitInfo.findCircuit(gAgent.getRegion()->getHost());
	if (cdp)
	{
		stats.mSimPingStat.addValue(cdp->getPingDelay().value());
		gAvgSimPing = ((gAvgSimPing * gSimPingCount) + cdp->getPingDelay()) / (gSimPingCount + 1);
		gSimPingCount++;
	}
	else
	{
		stats.mSimPingStat.addValue(10000);
	}

	stats.mFPSStat.addValue(1);
	F64Bits layer_bits = F64Bits(gVLManager.getLandBits() + gVLManager.getWindBits() + gVLManager.getCloudBits());
	stats.mLayersKBitStat.addValue((F32)layer_bits.valueInUnits<LLUnits::Kilobits>());
	stats.mObjectKBitStat.addValue(gObjectData.valueInUnits<LLUnits::Kilobits>());
	stats.mVFSPendingOperations.addValue(LLVFile::getVFSThread()->getPending());
	stats.mAssetKBitStat.addValue(gTransferManager.getTransferBitsIn(LLTCT_ASSET) / 1024);
	gTransferManager.resetTransferBitsIn(LLTCT_ASSET);

	if (LLAppViewer::getTextureFetch()->getNumRequests() == 0)
	{
		gTextureTimer.pause();
	}
	else
	{
		gTextureTimer.unpause();
	}
	
	{
		static F32 visible_avatar_frames = 0.f;
		static F32 avg_visible_avatars = 0;
		F32 visible_avatars = (F32)LLVOAvatar::sNumVisibleAvatars;
		if (visible_avatars > 0.f)
		{
			visible_avatar_frames = 1.f;
			avg_visible_avatars = (avg_visible_avatars * (F32)(visible_avatar_frames - 1.f) + visible_avatars) / visible_avatar_frames;
		}
		stats.setStat(LLViewerStats::ST_VISIBLE_AVATARS, (F64)avg_visible_avatars);
	}
	LLWorld::getInstance()->updateNetStats();
	LLWorld::getInstance()->requestCacheMisses();
	
	// Reset all of these values.
	gVLManager.resetBitCounts();
	gObjectData = (U32Bytes)0;
//	gDecodedBits = 0;

	// Only update texture stats periodically so that they are less noisy
	{
		static const F32 texture_stats_freq = 0.25f;
		static LLFrameTimer texture_stats_timer;
		if (texture_stats_timer.getElapsedTimeF32() >= texture_stats_freq)
		{
			stats.mHTTPTextureKBitStat.addValue(AICurlInterface::getHTTPBandwidth()/125.f);
			stats.mUDPTextureKBitStat.addValue(LLViewerTextureList::sTextureBits.valueInUnits<LLUnits::Kilobits>());
			stats.mTexturePacketsStat.addValue(LLViewerTextureList::sTexturePackets);
			gTotalTextureData += U32Bits(LLViewerTextureList::sTextureBits);
			LLViewerTextureList::sTextureBits = U32Bits(0);
			LLViewerTextureList::sTexturePackets = 0;
			texture_stats_timer.reset();
		}
	}

	{
		static const F32 mem_stats_freq = 10.f;
		static LLFrameTimer mem_stats_timer;
		if (mem_stats_timer.getElapsedTimeF32() >= mem_stats_freq)
		{
			stats.mMallocStat.addValue(SGMemStat::getMalloc()/1024.f/1024.f);
			mem_stats_timer.reset();
		}
	}
}

class ViewerStatsResponder : public LLHTTPClient::ResponderWithResult
{
public:
    ViewerStatsResponder() { }

    /*virtual*/ void httpFailure(void)
    {
		LL_INFOS() << "ViewerStatsResponder::error " << mStatus << " "
				<< mReason << LL_ENDL;
    }

    /*virtual*/ void httpSuccess(void)
    {
		LL_INFOS() << "ViewerStatsResponder::result" << LL_ENDL;
	}

	/*virtual*/ AIHTTPTimeoutPolicy const& getHTTPTimeoutPolicy(void) const { return viewerStatsResponder_timeout; }
	/*virtual*/ char const* getName(void) const { return "ViewerStatsResponder"; }
};

/*
 * The sim-side LLSD is in newsim/llagentinfo.cpp:forwardViewerStats.
 *
 * There's also a compatibility shim for the old fixed-format sim
 * stats in newsim/llagentinfo.cpp:processViewerStats.
 *
 * If you move stats around here, make the corresponding changes in
 * those locations, too.
 */
void send_stats()
{
	// IW 9/23/02 I elected not to move this into LLViewerStats
	// because it depends on too many viewer.cpp globals.
	// Someday we may want to merge all our stats into a central place
	// but that day is not today.

	// Only send stats if the agent is connected to a region.
	if (!gAgent.getRegion() || gNoRender)
	{
		return;
	}

	LLSD body;
	std::string url = gAgent.getRegion()->getCapability("ViewerStats");

	if (url.empty()) {
		LL_WARNS() << "Could not get ViewerStats capability" << LL_ENDL;
		return;
	}
	
	body["session_id"] = gAgentSessionID;
	
	LLSD &agent = body["agent"];
	
	time_t ltime;
	time(&ltime);
	F32 run_time = F32(LLFrameTimer::getElapsedSeconds());

	agent["start_time"] = S32(ltime - S32(run_time));

	// The first stat set must have a 0 run time if it doesn't actually
	// contain useful data in terms of FPS, etc.  We use half the
	// SEND_STATS_PERIOD seconds as the point at which these statistics become
	// valid.  Data warehouse uses a 0 value here to easily discard these
	// records with non-useful FPS values etc.
	if (run_time < (SEND_STATS_PERIOD / 2))
	{
		agent["run_time"] = 0.0f;
	}
	else
	{
		agent["run_time"] = run_time;
	}

	// send fps only for time app spends in foreground
	agent["fps"] = (F32)gForegroundFrameCount / gForegroundTime.getElapsedTimeF32();
	agent["version"] = LLVersionInfo::getChannelAndVersion();
	std::string language = LLUI::getLanguage();
	agent["language"] = language;
	
	agent["sim_fps"] = ((F32) gFrameCount - gSimFrames) /
		(F32) (gRenderStartTime.getElapsedTimeF32() - gSimLastTime);

	gSimLastTime = gRenderStartTime.getElapsedTimeF32();
	gSimFrames   = (F32) gFrameCount;

	agent["agents_in_view"] = LLVOAvatar::sNumVisibleAvatars;
	agent["ping"] = gAvgSimPing.value();
	agent["meters_traveled"] = gAgent.getDistanceTraveled();
	agent["regions_visited"] = gAgent.getRegionsVisited();
	agent["mem_use"] = LLMemory::getCurrentRSS() / 1024.0;

	LLSD &system = body["system"];
	
	system["ram"] = (S32) gSysMemory.getPhysicalMemoryKB().value();
	system["os"] = LLAppViewer::instance()->getOSInfo().getOSStringSimple();
	system["cpu"] = gSysCPU.getCPUString();
	unsigned char MACAddress[MAC_ADDRESS_BYTES];
	LLUUID::getNodeID(MACAddress);
	std::string macAddressString = llformat("%02x-%02x-%02x-%02x-%02x-%02x",
											MACAddress[0],MACAddress[1],MACAddress[2],
											MACAddress[3],MACAddress[4],MACAddress[5]);
	system["mac_address"] = macAddressString;
	system["serial_number"] = LLAppViewer::instance()->getSerialNumber();
	std::string gpu_desc = llformat(
		"%-6s Class %d ",
		gGLManager.mGLVendorShort.substr(0,6).c_str(),
		(S32)LLFeatureManager::getInstance()->getGPUClass())
		+ LLFeatureManager::getInstance()->getGPUString();

	system["gpu"] = gpu_desc;
	system["gpu_class"] = (S32)LLFeatureManager::getInstance()->getGPUClass();
	system["gpu_vendor"] = gGLManager.mGLVendorShort;
	system["gpu_version"] = gGLManager.mDriverVersionVendorString;
	system["opengl_version"] = gGLManager.mGLVersionString;

	LLSD &download = body["downloads"];

	download["world_kbytes"] = F64Kilobytes(gTotalWorldData).value();
	download["object_kbytes"] = F64Kilobytes(gTotalObjectData).value();
	download["texture_kbytes"] = F64Kilobytes(gTotalTextureData).value();
	download["mesh_kbytes"] = LLMeshRepository::sBytesReceived/1024.0;

	LLSD &in = body["stats"]["net"]["in"];

	in["kbytes"] = gMessageSystem->mTotalBytesIn / 1024.0;
	in["packets"] = (S32) gMessageSystem->mPacketsIn;
	in["compressed_packets"] = (S32) gMessageSystem->mCompressedPacketsIn;
	in["savings"] = (gMessageSystem->mUncompressedBytesIn -
					 gMessageSystem->mCompressedBytesIn) / 1024.0;
	
	LLSD &out = body["stats"]["net"]["out"];
	
	out["kbytes"] = gMessageSystem->mTotalBytesOut / 1024.0;
	out["packets"] = (S32) gMessageSystem->mPacketsOut;
	out["compressed_packets"] = (S32) gMessageSystem->mCompressedPacketsOut;
	out["savings"] = (gMessageSystem->mUncompressedBytesOut -
					  gMessageSystem->mCompressedBytesOut) / 1024.0;

	LLSD &fail = body["stats"]["failures"];

	fail["send_packet"] = (S32) gMessageSystem->mSendPacketFailureCount;
	fail["dropped"] = (S32) gMessageSystem->mDroppedPackets;
	fail["resent"] = (S32) gMessageSystem->mResentPackets;
	fail["failed_resends"] = (S32) gMessageSystem->mFailedResendPackets;
	fail["off_circuit"] = (S32) gMessageSystem->mOffCircuitPackets;
	fail["invalid"] = (S32) gMessageSystem->mInvalidOnCircuitPackets;

	// Misc stats, two strings and two ints
	// These are not expecticed to persist across multiple releases
	// Comment any changes with your name and the expected release revision
	// If the current revision is recent, ping the previous author before overriding
	LLSD &misc = body["stats"]["misc"];

	// Screen size so the UI team can figure out how big the widgets
	// appear and use a "typical" size for end user tests.

	S32 window_width = gViewerWindow->getWindowWidthRaw();
	S32 window_height = gViewerWindow->getWindowHeightRaw();
	S32 window_size = (window_width * window_height) / 1024;
	misc["string_1"] = llformat("%d", window_size);
	misc["string_2"] = llformat("Texture Time: %.2f, Total Time: %.2f", gTextureTimer.getElapsedTimeF32(), gFrameTimeSeconds);

// 	misc["int_1"] = LLSD::Integer(gSavedSettings.getU32("RenderQualityPerformance")); // Steve: 1.21
// 	misc["int_2"] = LLSD::Integer(gFrameStalls); // Steve: 1.21

	F32 unbaked_time = LLVOAvatar::sUnbakedTime * 1000.f / gFrameTimeSeconds;
	misc["int_1"] = LLSD::Integer(unbaked_time); // Steve: 1.22
	F32 grey_time = LLVOAvatar::sGreyTime * 1000.f / gFrameTimeSeconds;
	misc["int_2"] = LLSD::Integer(grey_time); // Steve: 1.22

	LL_INFOS() << "Misc Stats: int_1: " << misc["int_1"] << " int_2: " << misc["int_2"] << LL_ENDL;
	LL_INFOS() << "Misc Stats: string_1: " << misc["string_1"] << " string_2: " << misc["string_2"] << LL_ENDL;

	const S32 namesys = gSavedSettings.getS32("PhoenixNameSystem");
	body["DisplayNamesEnabled"] = namesys > 0 && namesys < 4;
	body["DisplayNamesShowUsername"] = namesys == 1 || namesys == 3;
	
	body["MinimalSkin"] = false;
	
	LLViewerStats::getInstance()->addToMessage(body);

	LL_INFOS("LogViewerStatsPacket") << "Sending viewer statistics: " << body << LL_ENDL;
	LLHTTPClient::post(url, body, new ViewerStatsResponder());

}

LLViewerStats::PhaseMap::PhaseMap()
{
}

LLTimer& LLViewerStats::PhaseMap::getPhaseTimer(const std::string& phase_name)
{
	phase_map_t::iterator iter = mPhaseMap.find(phase_name);
	if (iter == mPhaseMap.end())
	{
		LLTimer timer;
		mPhaseMap[phase_name] = timer;
	}
	LLTimer& timer = mPhaseMap[phase_name];
	return timer;
}

void LLViewerStats::PhaseMap::startPhase(const std::string& phase_name)
{
	LLTimer& timer = getPhaseTimer(phase_name);
	LL_DEBUGS() << "startPhase " << phase_name << LL_ENDL;
	timer.start();
}

void LLViewerStats::PhaseMap::stopPhase(const std::string& phase_name)
{
	phase_map_t::iterator iter = mPhaseMap.find(phase_name);
	if (iter != mPhaseMap.end())
	{
		if (iter->second.getStarted())
		{
			// Going from started to paused state - record stats.
			iter->second.stop();
		}
	}
}

bool LLViewerStats::PhaseMap::getPhaseValues(const std::string& phase_name, F32& elapsed, bool& completed)
{
	phase_map_t::iterator iter = mPhaseMap.find(phase_name);
	if (iter != mPhaseMap.end())
	{
		elapsed =  iter->second.getElapsedTimeF32();
		completed = !iter->second.getStarted();
		return true;
	}
	else
	{
		return false;
	}
}
	
void LLViewerStats::PhaseMap::clearPhases()
{
	LL_DEBUGS() << "clearPhases" << LL_ENDL;

	mPhaseMap.clear();
}

LLSD LLViewerStats::PhaseMap::dumpPhases()
{
	LLSD result;
	for (phase_map_t::iterator iter = mPhaseMap.begin(); iter != mPhaseMap.end(); ++iter)
	{
		const std::string& phase_name = iter->first;
		result[phase_name]["completed"] = LLSD::Integer(!(iter->second.getStarted()));
		result[phase_name]["elapsed"] = iter->second.getElapsedTimeF32();
	}
	return result;
}
