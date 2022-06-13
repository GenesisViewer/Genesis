/** 
 * @file llappviewer.cpp
 * @brief The LLAppViewer class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
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

#include "llappviewer.h"

#include "hippogridmanager.h"
#include "hippolimits.h"

#include "llversioninfo.h"
#include "llfeaturemanager.h"
#include "lluictrlfactory.h"
#include "lltexteditor.h"
#include "llerrorcontrol.h"
#include "lleventtimer.h"
#include "llviewertexturelist.h"
#include "llgroupmgr.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llagentwearables.h"
#include "llimprocessing.h"
#include "llwindow.h"
#include "llviewerstats.h"
#include "llmarketplacefunctions.h"
#include "llmarketplacenotifications.h"
#include "llmeshrepository.h"
#include "llmodaldialog.h"
#include "llpumpio.h"
#include "llmimetypes.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llfocusmgr.h"
#include "llviewerjoystick.h"
#include "llares.h" 
#include "llcurl.h"
#include "llcalc.h"
#include "llviewerwindow.h"
#include "llviewerdisplay.h"
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewermediafocus.h"
#include "llviewermessage.h"
#include "llviewerobjectlist.h"
#include "llworldmap.h"
#include "llmutelist.h"
#include "llurldispatcher.h"
#include "llurlhistory.h"
#include "statemachine/aifilepicker.h"
#include "llfirstuse.h"
#include "llrender.h"
#include "llvector4a.h"
#include "llvoicechannel.h"
#include "llvoavatarself.h"
#include "llurlmatch.h"
#include "llprogressview.h"
#include "llvocache.h"
#include "llvopartgroup.h"
// [SL:KB] - Patch: Appearance-Misc | Checked: 2013-02-12 (Catznip-3.4)
#include "llappearancemgr.h"
// [/SL:KB]
#include "llfloaterteleporthistory.h"
#include "llweb.h"
#include "llsecondlifeurls.h"
#include "llavatarrenderinfoaccountant.h"
#include "llskinningutil.h"

// Linden library includes
#include "llavatarnamecache.h"
#include "lldiriterator.h"
#include "llexperiencecache.h"
#include "llimagej2c.h"
#include "llmemory.h"
#include "llprimitive.h"
#include "llurlaction.h"
#include "llurlentry.h"
#include "llvolumemgr.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include <boost/bind.hpp>

#if LL_WINDOWS
	#include "llwindebug.h"
#endif

#if LL_WINDOWS
#	include <share.h> // For _SH_DENYWR in initMarkerFile
#else
#   include <sys/file.h> // For initMarkerFile support
#endif

#include "llnotify.h"
#include "llviewerkeyboard.h"
#include "lllfsthread.h"
#include "llworkerthread.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llimageworker.h"

// <edit>
#include "aicurleasyrequeststatemachine.h"
#include "aihttptimeoutpolicy.h"
// </edit>
// The files below handle dependencies from cleanup.
#include "llkeyframemotion.h"
#include "llworldmap.h"
#include "llhudmanager.h"
#include "lltoolmgr.h"
#include "llassetstorage.h"
#include "llpolymesh.h"
#include "llaudioengine.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "lltracker.h"
#include "llviewerparcelmgr.h"
#include "llworldmapview.h"
#include "llpostprocess.h"
#include "llwlparammanager.h"

#include "lldebugview.h"
#include "llconsole.h"
#include "llcontainerview.h"
#include "llhoverview.h"

#include "llsdserialize.h"

#include "llworld.h"
#include "llhudeffecttrail.h"
#include "llvectorperfoptions.h"
#include "llwatchdog.h"

// Included so that constants/settings might be initialized
// in save_settings_to_globals()
#include "aosystem.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "floaterlocalassetbrowse.h"
#include "llsurface.h"
#include "llvotree.h"
#include "llfolderview.h"
#include "lltoolbar.h"
#include "llframestats.h"
#include "llagentpilot.h"
#include "llvovolume.h"
#include "llflexibleobject.h" 
#include "llvosurfacepatch.h"
#include "llcommandlineparser.h"
#include "llfloatermemleak.h"
#include "llfloatersnapshot.h"
#include "llfloaterinventory.h"

// includes for idle() idleShutdown()
#include "llviewercontrol.h"
#include "lleventnotifier.h"
#include "llcallbacklist.h"
#include "pipeline.h"
#include "llgesturemgr.h"
#include "llsky.h"
#include "llvlmanager.h"
#include "llviewercamera.h"
#include "lldrawpoolbump.h"
#include "llvieweraudio.h"
#include "llimview.h"
#include "llviewerthrottle.h"
#include "llparcel.h"
#include "llviewerassetstats.h"
#include "NACLantispam.h"

#include "llmainlooprepeater.h"

// [RLVa:KB]
#include "rlvhandler.h"
// [/RLVa:KB]

// *FIX: These extern globals should be cleaned up.
// The globals either represent state/config/resource-storage of either 
// this app, or another 'component' of the viewer. App globals should be 
// moved into the app class, where as the other globals should be 
// moved out of here.
// If a global symbol reference seems valid, it will be included
// via header files above.

//----------------------------------------------------------------------------
// llviewernetwork.h
#include "llviewernetwork.h"

#include <random>

#ifdef USE_CRASHPAD
#pragma warning(disable:4265)

#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <client/prune_crash_reports.h>
#include <client/settings.h>
#include <client/annotation.h>

#include <fmt/format.h>

#include "llnotificationsutil.h"
#include "llversioninfo.h"


template <size_t SIZE, crashpad::Annotation::Type T = crashpad::Annotation::Type::kString>
struct crashpad_annotation : public crashpad::Annotation {
	std::array<char, SIZE> buffer;
	crashpad_annotation(const char* name) : crashpad::Annotation(T, name, buffer.data())
	{}
	void set(const std::string& src) {
		//LL_INFOS() << name() << ": " << src.c_str() << LL_ENDL;
		const size_t min_size = llmin(SIZE, src.size());
		memcpy(buffer.data(), src.data(), min_size);
		buffer.data()[SIZE - 1] = '\0';
		SetSize(min_size);
	}
};
#define DEFINE_CRASHPAD_ANNOTATION(name, len) \
static crashpad_annotation<len> g_crashpad_annotation_##name##_buffer("sentry[tags]["#name"]");
#define DEFINE_CRASHPAD_ANNOTATION_EXTRA(name, len) \
static crashpad_annotation<len> g_crashpad_annotation_##name##_buffer("sentry[extra]["#name"]");
#define SET_CRASHPAD_ANNOTATION_VALUE(name, value) \
g_crashpad_annotation_##name##_buffer.set(value);
#else
#define SET_CRASHPAD_ANNOTATION_VALUE(name, value)
#define DEFINE_CRASHPAD_ANNOTATION_EXTRA(name, len)
#define DEFINE_CRASHPAD_ANNOTATION(name, len)
#endif

DEFINE_CRASHPAD_ANNOTATION_EXTRA(fatal_message, 512);
DEFINE_CRASHPAD_ANNOTATION(grid_name, 64);
DEFINE_CRASHPAD_ANNOTATION(region_name, 64);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(cpu_string, 128);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(gpu_string, 128);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(gl_version, 128);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(session_duration, 32);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(startup_state, 32);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(memory_sys, 32);
DEFINE_CRASHPAD_ANNOTATION_EXTRA(memory_alloc, 32);

////// Windows-specific includes to the bottom - nasty defines in these pollute the preprocessor
//
//----------------------------------------------------------------------------
// viewer.cpp - these are only used in viewer, should be easily moved.

#if LL_DARWIN
extern void init_apple_menu(const char* product);
#endif // LL_DARWIN

extern BOOL gRandomizeFramerate;
extern BOOL gPeriodicSlowFrame;
extern BOOL gDebugGL;

extern void startEngineThread(void);
extern void stopEngineThread(void);

////////////////////////////////////////////////////////////
// All from the last globals push...
const F32 DEFAULT_AFK_TIMEOUT = 5.f * 60.f; // time with no input before user flagged as Away From Keyboard

F32 gSimLastTime; // Used in LLAppViewer::init and send_stats()
F32 gSimFrames;

BOOL gShowObjectUpdates = FALSE;

S32 gLastExecDuration = -1; // (<0 indicates unknown)

BOOL gAcceptTOS = FALSE;
BOOL gAcceptCriticalMessage = FALSE;

eLastExecEvent gLastExecEvent = LAST_EXEC_NORMAL;

LLSD gDebugInfo;

U32	gFrameCount = 0;
U32 gForegroundFrameCount = 0; // number of frames that app window was in foreground
LLPumpIO* gServicePump = NULL;

BOOL gPacificDaylightTime = FALSE;

U64MicrosecondsImplicit gFrameTime = 0;
F32SecondsImplicit gFrameTimeSeconds = 0.f;
F32SecondsImplicit gFrameIntervalSeconds = 0.f;
F32 gFPSClamped = 10.f;						// Pretend we start at target rate.
F32 gFrameDTClamped = 0.f;					// Time between adjacent checks to network for packets
U64MicrosecondsImplicit	gStartTime = 0; // gStartTime is "private", used only to calculate gFrameTimeSeconds
U32 gFrameStalls = 0;
const F64 FRAME_STALL_THRESHOLD = 1.0;

LLTimer gRenderStartTime;
LLFrameTimer gForegroundTime;
LLTimer gLogoutTimer;
static const F32 LOGOUT_REQUEST_TIME = 6.f;  // this will be cut short by the LogoutReply msg.
F32 gLogoutMaxTime = LOGOUT_REQUEST_TIME;

BOOL				gDisconnected = FALSE;

// used to restore texture state after a mode switch
LLFrameTimer	gRestoreGLTimer;
BOOL			gRestoreGL = FALSE;
BOOL				gUseWireframe = FALSE;

// VFS globals - see llappviewer.h
LLVFS* gStaticVFS = NULL;

LLMemoryInfo gSysMemory;
U64Bytes gMemoryAllocated(0); // updated in display_stats() in llviewerdisplay.cpp

std::string gLastVersionChannel;

LLVector3			gWindVec(3.0, 3.0, 0.0);
LLVector3			gRelativeWindVec(0.0, 0.0, 0.0);

U32		gPacketsIn = 0;

BOOL				gPrintMessagesThisFrame = FALSE;

BOOL gRandomizeFramerate = FALSE;
BOOL gPeriodicSlowFrame = FALSE;

BOOL gCrashOnStartup = FALSE;
BOOL gLLErrorActivated = FALSE;
BOOL gLogoutInProgress = FALSE;

////////////////////////////////////////////////////////////
// Internal globals... that should be removed.
static std::string gArgs;
const int MAX_MARKER_LENGTH = 1024;
const std::string MARKER_FILE_NAME("Singularity.exec_marker");
const std::string START_MARKER_FILE_NAME("Singularity.start_marker");;
const std::string ERROR_MARKER_FILE_NAME("Singularity.error_marker");
const std::string LLERROR_MARKER_FILE_NAME("Singularity.llerror_marker");
const std::string LOGOUT_MARKER_FILE_NAME("Singularity.logout_marker");
const std::string LOG_FILE("Singularity.log");
extern const std::string OLD_LOG_FILE("Singularity.old");
static BOOL gDoDisconnect = FALSE;
static std::string gLaunchFileOnQuit;

// Used on Win32 for other apps to identify our window (eg, win_setup)
const char* const VIEWER_WINDOW_CLASSNAME = "Second Life"; // Don't change

//-- LLDeferredTaskList ------------------------------------------------------

/**
 * A list of deferred tasks.
 *
 * We sometimes need to defer execution of some code until the viewer gets idle,
 * e.g. removing an inventory item from within notifyObservers() may not work out.
 *
 * Tasks added to this list will be executed in the next LLAppViewer::idle() iteration.
 * All tasks are executed only once.
 */
class LLDeferredTaskList: public LLSingleton<LLDeferredTaskList>
{
	LOG_CLASS(LLDeferredTaskList);

	friend class LLAppViewer;
	typedef boost::signals2::signal<void()> signal_t;

	void addTask(const signal_t::slot_type& cb)
	{
		mSignal.connect(cb);
	}

	void run()
	{
		if (!mSignal.empty())
		{
			mSignal();
			mSignal.disconnect_all_slots();
		}
	}

	signal_t mSignal;
};

//----------------------------------------------------------------------------

// List of entries from strings.xml to always replace
static std::set<std::string> default_trans_args;
void init_default_trans_args()
{
	default_trans_args.insert("SECOND_LIFE"); // World
	default_trans_args.insert("APP_NAME");
	default_trans_args.insert("SHORT_APP_NAME");
	default_trans_args.insert("CAPITALIZED_APP_NAME");
	default_trans_args.insert("APP_SITE");
	default_trans_args.insert("SECOND_LIFE_GRID");
	default_trans_args.insert("SUPPORT_SITE");
	default_trans_args.insert("CURRENCY");
	default_trans_args.insert("GRID_OWNER");
}

//----------------------------------------------------------------------------
// File scope definitons
const char *VFS_DATA_FILE_BASE = "data.db2.x.";
const char *VFS_INDEX_FILE_BASE = "index.db2.x.";

std::string gWindowTitle;

std::string gLoginPage;
std::vector<std::string> gLoginURIs;
static std::string gHelperURI;

LLAppViewer::LLUpdaterInfo *LLAppViewer::sUpdaterInfo = NULL ;

//----------------------------------------------------------------------------
// Metrics logging control constants
//----------------------------------------------------------------------------
static const F32 METRICS_INTERVAL_DEFAULT = 600.0;
static const F32 METRICS_INTERVAL_QA = 30.0;
static F32 app_metrics_interval = METRICS_INTERVAL_DEFAULT;
static bool app_metrics_qa_mode = false;

void idle_afk_check()
{
	static const LLCachedControl<bool> allow_idk_afk("AllowIdleAFK");
	// check idle timers
	static const LLCachedControl<F32> afk_timeout("AFKTimeout",0.f);
	//if (allow_idk_afk && (gAwayTriggerTimer.getElapsedTimeF32() > gSavedSettings.getF32("AFKTimeout")))
// [RLVa:KB] - Checked: 2009-10-19 (RLVa-1.1.0g) | Added: RLVa-1.1.0g
#ifdef RLV_EXTENSION_CMD_ALLOWIDLE
	if ( (allow_idk_afk || gRlvHandler.hasBehaviour(RLV_BHVR_ALLOWIDLE)) &&
		 (gAwayTriggerTimer.getElapsedTimeF32() > afk_timeout) && (afk_timeout > 0))
#else
	if (allow_idk_afk && (gAwayTriggerTimer.getElapsedTimeF32() > afk_timeout) && (afk_timeout > 0))
#endif // RLV_EXTENSION_CMD_ALLOWIDLE
// [/RLVa:KB]
	{
		gAgent.setAFK();
	}
}


// A callback set in LLAppViewer::init()
static void ui_audio_callback(const LLUUID& uuid)
{
	if (gAudiop)
	{
		gAudiop->triggerSound(uuid, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI);
	}
}

// Use these strictly for things that are constructed at startup,
// or for things that are performance critical.  JC
static void settings_to_globals()
{
	LLBUTTON_H_PAD		= gSavedSettings.getS32("ButtonHPad");
	LLBUTTON_V_PAD		= gSavedSettings.getS32("ButtonVPad");
	BTN_HEIGHT_SMALL	= gSavedSettings.getS32("ButtonHeightSmall");
	BTN_HEIGHT			= gSavedSettings.getS32("ButtonHeight");

	extern S32 MENU_BAR_HEIGHT;
	MENU_BAR_HEIGHT		= gSavedSettings.getS32("MenuBarHeight");
	extern S32 STATUS_BAR_HEIGHT;
	STATUS_BAR_HEIGHT	= gSavedSettings.getS32("StatusBarHeight");

	LLCOMBOBOX_HEIGHT	= BTN_HEIGHT - 2;
	LLCOMBOBOX_WIDTH	= 128;

	LLSurface::setTextureSize(gSavedSettings.getU32("RegionTextureSize"));
	
	LLRender::sGLCoreProfile = LLGLSLShader::sNoFixedFunction = gSavedSettings.getBOOL("RenderGLCoreProfile");

	LLImageGL::sGlobalUseAnisotropic	= gSavedSettings.getBOOL("RenderAnisotropic");
	LLImageGL::sCompressTextures		= gSavedSettings.getBOOL("RenderCompressTextures");
	LLVOVolume::sLODFactor				= gSavedSettings.getF32("RenderVolumeLODFactor");
	LLVOVolume::sDistanceFactor			= 1.f-LLVOVolume::sLODFactor * 0.1f;
	LLVolumeImplFlexible::sUpdateFactor = gSavedSettings.getF32("RenderFlexTimeFactor");
	LLVOTree::sTreeFactor				= gSavedSettings.getF32("RenderTreeLODFactor");
	LLVOAvatar::sLODFactor				= gSavedSettings.getF32("RenderAvatarLODFactor");
	LLVOAvatar::sPhysicsLODFactor		= gSavedSettings.getF32("RenderAvatarPhysicsLODFactor");
	LLVOAvatar::sMaxVisible				= gSavedSettings.getS32("RenderAvatarMaxVisible");
	LLVOAvatar::sVisibleInFirstPerson	= gSavedSettings.getBOOL("FirstPersonAvatarVisible");
	// clamp auto-open time to some minimum usable value
	LLFolderView::sAutoOpenTime			= llmax(0.25f, gSavedSettings.getF32("FolderAutoOpenDelay"));
	LLToolBar::sInventoryAutoOpenTime	= gSavedSettings.getF32("InventoryAutoOpenDelay");
	LLSelectMgr::sRectSelectInclusive	= gSavedSettings.getBOOL("RectangleSelectInclusive");
	LLSelectMgr::sRenderHiddenSelections = gSavedSettings.getBOOL("RenderHiddenSelections");
	LLSelectMgr::sRenderLightRadius = gSavedSettings.getBOOL("RenderLightRadius");

	gFrameStats.setTrackStats(gSavedSettings.getBOOL("StatsSessionTrackFrameStats"));
	gAgentPilot.mNumRuns		= gSavedSettings.getS32("StatsNumRuns");
	gAgentPilot.mQuitAfterRuns	= gSavedSettings.getBOOL("StatsQuitAfterRuns");
	gAgent.setHideGroupTitle(gSavedSettings.getBOOL("RenderHideGroupTitle"));
		
	gDebugWindowProc = gSavedSettings.getBOOL("DebugWindowProc");
	gShowObjectUpdates = gSavedSettings.getBOOL("ShowObjectUpdates");
	LLWorldMapView::sMapScale =  llmax(.1f,gSavedSettings.getF32("MapScale"));
	LLHoverView::sShowHoverTips = gSavedSettings.getBOOL("ShowHoverTips");
}

static void settings_modify()
{
	bool can_defer = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred");
	LLRenderTarget::sUseFBO				= gSavedSettings.getBOOL("RenderUseFBO") || (gSavedSettings.getBOOL("RenderDeferred") && can_defer);
	LLVOAvatar::sUseImpostors			= gSavedSettings.getBOOL("RenderUseImpostors");
	LLVOSurfacePatch::sLODFactor		= gSavedSettings.getF32("RenderTerrainLODFactor");
	LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor; //square lod factor to get exponential range of [1,4]
	gDebugGL = gSavedSettings.getBOOL("RenderDebugGL") || gDebugSession;
	gDebugPipeline = gSavedSettings.getBOOL("RenderDebugPipeline");
	gAuditTexture = gSavedSettings.getBOOL("AuditTexture");
}

//virtual
bool LLAppViewer::initSLURLHandler()
{
	// does nothing unless subclassed
	return false;
}

//virtual
bool LLAppViewer::sendURLToOtherInstance(const std::string& url)
{
	// does nothing unless subclassed
	return false;
}

//----------------------------------------------------------------------------
// LLAppViewer definition

// Static members.
// The single viewer app.
LLAppViewer* LLAppViewer::sInstance = NULL;

const std::string LLAppViewer::sGlobalSettingsName = "Global"; 
const std::string LLAppViewer::sPerAccountSettingsName = "PerAccount"; 

LLTextureCache* LLAppViewer::sTextureCache = NULL; 
LLImageDecodeThread* LLAppViewer::sImageDecodeThread = NULL; 
LLTextureFetch* LLAppViewer::sTextureFetch = NULL; 

LLAppViewer::LLAppViewer() : 
	mMarkerFile(),
	mLogoutMarkerFile(),
	mReportedCrash(false),
	mNumSessions(0),
	mPurgeCache(false),
	mPurgeOnExit(false),
	mSecondInstance(false),
	mSavedFinalSnapshot(false),
	mQuitRequested(false),
	mLogoutRequestSent(false),
	mMainloopTimeout(NULL),
	mAgentRegionLastAlive(false)
{
	if(nullptr != sInstance)
	{
		LL_ERRS() << "Oh no! An instance of LLAppViewer already exists! LLAppViewer is sort of like a singleton." << LL_ENDL;
	}

	// Need to do this initialization before we do anything else, since anything
	// that touches files should really go through the lldir API
	{
		std::string newview_path;
		const auto& exe_dir = gDirUtilp->getExecutableDir();
		auto build_dir_pos = exe_dir.rfind("build-");
		if (build_dir_pos != std::string::npos)
		{
			// ...we're in a dev checkout
			newview_path = gDirUtilp->add(gDirUtilp->add(exe_dir.substr(0, build_dir_pos), "indra"), "newview");
			if (LLFile::isdir(newview_path))
				LL_INFOS() << "Running in dev checkout with newview " << newview_path << LL_ENDL;
			else newview_path.clear();
		}
		//Mely : Change the settings dir
		//gDirUtilp->initAppDirs("SecondLife", newview_path);
		gDirUtilp->initAppDirs("Genesis-Eve", newview_path);
	}
	//
	// IMPORTANT! Do NOT put anything that will write
	// into the log files during normal startup until AFTER
	// we run the "program crashed last time" error handler below.
	//
	sInstance = this;


	initLoggingAndGetLastDuration();

	processMarkerFiles();
	//
	// OK to write stuff to logs now, we've now crash reported if necessary
	//
#if !defined(USE_CRASHPAD)
	// write dump files to a per-run dump directory to avoid multiple viewer issues.
	std::string logdir = gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "");

	setDebugFileNames(logdir);
#endif
}


LLAppViewer::~LLAppViewer()
{
	destroyMainloopTimeout();

	// If we got to this destructor somehow, the app didn't hang.
	removeMarkerFiles();
}

class LLUITranslationBridge : public LLTranslationBridge
{
public:
	std::string getString(const std::string &xml_desc) override
	{
		return LLTrans::getString(xml_desc);
	}
};

void load_default_bindings(bool zqsd)
{
	gViewerKeyboard.unloadBindings();
	const std::string key_bindings_file(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, zqsd ? "keysZQSD.xml" : "keys.xml"));
	if (!gDirUtilp->fileExists(key_bindings_file) || !gViewerKeyboard.loadBindingsXML(key_bindings_file))
	{
		const std::string key_bindings_file(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, zqsd ? "keysZQSD.ini" : "keys.ini"));
		if (!gViewerKeyboard.loadBindings(key_bindings_file))
		{
			LL_ERRS("InitInfo") << "Unable to open " << key_bindings_file << LL_ENDL;
		}
	}
	// Load Custom bindings (override defaults)
	std::string custom_keys(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "custom_keys.xml"));
	if (!gDirUtilp->fileExists(custom_keys) || !gViewerKeyboard.loadBindingsXML(custom_keys))
	{
		custom_keys = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "custom_keys.ini");
		if (gDirUtilp->fileExists(custom_keys))
			gViewerKeyboard.loadBindings(custom_keys);
	}
}

/* Singu unused
namespace {
// With Xcode 6, _exit() is too magical to use with boost::bind(), so provide
// this little helper function.
void fast_exit(int rc)
{
	_exit(rc);
}
}
*/

#ifdef USE_CRASHPAD
base::FilePath databasePath()
{
	// Cache directory that will store crashpad information and minidumps
	std::string crashpad_path = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "crashpad");
	return base::FilePath(ll_convert_string_to_wide(crashpad_path));
}

static void handleCrashSubmitBehaviorChanged(LLControlVariable*, const LLSD& val)
{
	if (auto db = crashpad::CrashReportDatabase::Initialize(databasePath()))
	{
		if (auto settings = db->GetSettings())
		{
			settings->SetUploadsEnabled(val.asBoolean());
		}
	}
}

static void configureCrashUploads()
{
	auto database = databasePath();
	auto db = crashpad::CrashReportDatabase::InitializeWithoutCreating(database);
	if (!db) return;
	auto settings = db->GetSettings();
	if (!settings) return;
	auto control = gSavedSettings.getControl("CrashSubmitBehavior");
	control->getSignal()->connect(handleCrashSubmitBehaviorChanged);
	if (control->get().asInteger() == -1)
	{
		LLNotificationsUtil::add("SubmitCrashReports", LLSD(), LLSD(), [control](const LLSD& p, const LLSD& f) {
			control->set(!LLNotificationsUtil::getSelectedOption(p, f));
			});
	}

	if (!settings->SetUploadsEnabled(control->get().asInteger() == 1))
	{
		LL_WARNS() << "Failed to set enable upload of crash database." << LL_ENDL;
	}
}
#endif

void LLAppViewer::initCrashReporting()
{
#ifdef USE_CRASHPAD
	// Path to the out-of-process handler executable
	std::string handler_path = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "crashpad_handler.exe");
	if (!gDirUtilp->fileExists(handler_path))
	{
		LL_ERRS() << "Failed to initialize crashpad due to missing handler executable." << LL_ENDL;
		return;
	}
	base::FilePath handler(ll_convert_string_to_wide(handler_path));

	auto database = databasePath();

	// URL used to submit minidumps to
	std::string url(CRASHPAD_URL);

	// Optional annotations passed via --annotations to the handler
	std::map<std::string, std::string> annotations;

#if 0
	unsigned char node_id[6];
	if (LLUUID::getNodeID(node_id) > 0)
	{
		char md5str[MD5HEX_STR_SIZE] = { 0 };
		LLMD5 hashed_unique_id;
		hashed_unique_id.update(node_id, 6);
		hashed_unique_id.finalize();
		hashed_unique_id.hex_digest((char*)md5str);
		annotations.emplace("sentry[contexts][app][device_app_hash]", std::string(md5str));
	}
#endif

	annotations.emplace("sentry[contexts][app][app_name]", LLVersionInfo::getChannel());
	annotations.emplace("sentry[contexts][app][app_version]", LLVersionInfo::getVersion());
	annotations.emplace("sentry[contexts][app][app_build]", LLVersionInfo::getChannelAndVersion());

	annotations.emplace("sentry[release]", LLVersionInfo::getChannelAndVersion());

	annotations.emplace("sentry[tags][second_instance]", fmt::to_string(isSecondInstance()));
	//annotations.emplace("sentry[tags][bitness]", fmt::to_string(ADDRESS_SIZE));
	annotations.emplace("sentry[tags][bitness]",
#if defined(_WIN64) || defined(__x86_64__)
		"64"
#else
		"32"
#endif
	);

	// Optional arguments to pass to the handler
	std::vector<std::string> arguments;
	arguments.push_back("--no-rate-limit");
	arguments.push_back("--monitor-self");

	if (isSecondInstance())
	{
		arguments.push_back("--no-periodic-tasks");
	}
	else
	{
		auto db = crashpad::CrashReportDatabase::Initialize(database);
		if (db == nullptr)
		{
			LL_WARNS() << "Failed to initialize crashpad database at path: " << wstring_to_utf8str(database.value()) << LL_ENDL;
			return;
		}

		auto prune_condition = crashpad::PruneCondition::GetDefault();
		if (prune_condition != nullptr)
		{
			auto ret = crashpad::PruneCrashReportDatabase(db.get(), prune_condition.get());
			LL_INFOS() << "Pruned " << ret << " reports from the crashpad database." << LL_ENDL;
		}
	}

	crashpad::CrashpadClient client;
	bool success = client.StartHandler(
		handler,
		database,
		database,
		url,
		annotations,
		arguments,
		/* restartable */ true,
		/* asynchronous_start */ false
	);
	if (success)
		LL_INFOS() << "Crashpad init success" << LL_ENDL;
	else
		LL_WARNS() << "FAILED TO INITIALIZE CRASHPAD" << LL_ENDL;
#endif
}

bool LLAppViewer::init()
{
#ifdef USE_CRASHPAD
	initCrashReporting();
#endif

	writeDebugInfo();

	setupErrorHandling();

	{
		auto fn = boost::bind<bool>([](const LLSD& stateInfo) -> bool {
			SET_CRASHPAD_ANNOTATION_VALUE(startup_state, stateInfo["str"].asString());
			return false;
			}, _1);
		LLStartUp::getStateEventPump().listen<::LLEventListener>("LLAppViewer", fn);
	}

	//
	// Start of the application
	//
	LLFastTimer::reset();
	
	// initialize LLWearableType translation bridge.
	// Memory will be cleaned up in ::cleanupClass()
	LLTranslationBridge::ptr_t trans = std::make_shared<LLUITranslationBridge>();
	LLWearableType::initClass(trans);

	// <edit>
	// We can call this early.
	LLFrameTimer::global_initialization();
	// </edit>

	// initialize SSE options
	LLVector4a::initClass();

	//initialize particle index pool
	LLVOPartGroup::initClass();

	// set skin search path to default, will be overridden later
	// this allows simple skinned file lookups to work
	gDirUtilp->setSkinFolder("default", "en-us");

//	initLoggingAndGetLastDuration();

	// <edit>
	// Curl must be initialized before any thread is running.
	AICurlInterface::initCurl();

	//
	// OK to write stuff to logs now, we've now crash reported if necessary
	//
	init_default_trans_args();
	
	if (!initConfiguration())
		return false;

	LL_INFOS("InitInfo") << "Configuration initialized." << LL_ENDL ;

	// initialize skinning util
	LLSkinningUtil::initClass();

	//set the max heap size.
	initMaxHeapSize() ;

	LLPrivateMemoryPoolManager::initClass((BOOL)gSavedSettings.getBOOL("MemoryPrivatePoolEnabled"), (U32)gSavedSettings.getU32("MemoryPrivatePoolSize")) ;

	AIEngine::setMaxCount(gSavedSettings.getU32("StateMachineMaxTime"));

	{
		AIHTTPTimeoutPolicy policy_tmp(
			"CurlTimeout* Debug Settings",
			gSavedSettings.getU32("CurlTimeoutDNSLookup"),
			gSavedSettings.getU32("CurlTimeoutConnect"),
			gSavedSettings.getU32("CurlTimeoutReplyDelay"),
			gSavedSettings.getU32("CurlTimeoutLowSpeedTime"),
			gSavedSettings.getU32("CurlTimeoutLowSpeedLimit"),
			gSavedSettings.getU32("CurlTimeoutMaxTransaction"),
			gSavedSettings.getU32("CurlTimeoutMaxTotalDelay")
			);
		AIHTTPTimeoutPolicy::setDefaultCurlTimeout(policy_tmp);
	}

	mAlloc.setProfilingEnabled(gSavedSettings.getBOOL("MemProfiling"));

	{
		if (gSavedSettings.getBOOL("QAModeMetrics"))
		{
			app_metrics_qa_mode = true;
			app_metrics_interval = METRICS_INTERVAL_QA;
		}
		LLViewerAssetStatsFF::init();
	}

	initThreads();
	LL_INFOS("InitInfo") << "Threads initialized." << LL_ENDL ;

	// Load art UUID information, don't require these strings to be declared in code.
	for(auto& colors_base_filename : gDirUtilp->findSkinnedFilenames(LLDir::SKINBASE, "colors_base.xml", LLDir::ALL_SKINS))
	{
		LL_DEBUGS("InitInfo") << "Loading colors from " << colors_base_filename << LL_ENDL;
		gColors.loadFromFileLegacy(colors_base_filename, FALSE, TYPE_COL4U);
	}
	// Load overrides from user colors file
	for (auto& colors_base_filename : gDirUtilp->findSkinnedFilenames(LLDir::SKINBASE, "colors.xml", LLDir::ALL_SKINS))
	{
		gColors.loadFromFileLegacy(colors_base_filename, FALSE, TYPE_COL4U);
		LL_DEBUGS("InitInfo") << "Loading user colors from " << colors_base_filename << LL_ENDL;
		if (gColors.loadFromFileLegacy(colors_base_filename, FALSE, TYPE_COL4U) == 0)
		{
			LL_DEBUGS("InitInfo") << "Cannot load user colors from " << colors_base_filename << LL_ENDL;
		}
	}

	// Widget construction depends on LLUI being initialized
	LLUI::initClass(&gSavedSettings,
		&gSavedPerAccountSettings,
		&gSavedSettings,
		&gColors,
		LLUIImageList::getInstance(),
		ui_audio_callback,
		&LLUI::getScaleFactor());
	LL_INFOS("InitInfo") << "UI initialized." << LL_ENDL ;

	// NOW LLUI::getLanguage() should work. gDirUtilp must know the language
	// for this session ASAP so all the file-loading commands that follow,
	// that use findSkinnedFilenames(), will include the localized files.
	gDirUtilp->setSkinFolder(gDirUtilp->getSkinFolder(), LLUI::getLanguage());
	
	LLUICtrlFactory::getInstance()->setupPaths(); // update paths with correct language set

	// Setup LLTrans after LLUI::initClass has been called.
	LLTrans::parseStrings("strings.xml", default_trans_args);

	// Setup notifications after LLUI::initClass() has been called.
	LLNotifications::instance().createDefaultChannels();
	LL_INFOS("InitInfo") << "Notifications initialized." << LL_ENDL ;

#ifdef USE_CRASHPAD
	// Now that we have Settings and Notifications, we can configure crash uploads
	configureCrashUploads();
#endif

	writeSystemInfo();

	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////
	// *FIX: The following code isn't grouped into functions yet.

	//
	// Various introspection concerning the libs we're using - particularly
		// the libs involved in getting to a full login screen.
	//
	LL_INFOS("InitInfo") << "J2C Engine is: " << LLImageJ2C::getEngineInfo() << LL_ENDL;
	LL_INFOS("InitInfo") << "libcurl version is: " << LLCurl::getVersionString() << LL_ENDL;

	/////////////////////////////////////////////////
	// OS-specific login dialogs
	/////////////////////////////////////////////////

#if TEST_CACHED_CONTROL
	test_cached_control();
#endif

	// track number of times that app has run
	mNumSessions = gSavedSettings.getS32("NumSessions");
	mNumSessions++;
	gSavedSettings.setS32("NumSessions", mNumSessions);

	gSavedSettings.setString("HelpLastVisitedURL",gSavedSettings.getString("HelpHomeURL"));

	if (gSavedSettings.getBOOL("VerboseLogs"))
	{
		LLError::setPrintLocation(true);
	}
	
	LLWeb::initClass();			  // do this after LLUI

	// Provide the text fields with callbacks for opening Urls
	LLUrlAction::setOpenURLCallback(boost::bind(&LLWeb::loadURL, _1, LLStringUtil::null, LLStringUtil::null));
	LLUrlAction::setOpenURLInternalCallback(boost::bind(&LLWeb::loadURLInternal, _1, LLStringUtil::null, LLStringUtil::null));
	LLUrlAction::setOpenURLExternalCallback(boost::bind(&LLWeb::loadURLExternal, _1, true, LLStringUtil::null));
	LLUrlAction::setExecuteSLURLCallback(&LLURLDispatcher::dispatchFromTextEditor);

	LL_INFOS("InitInfo") << "UI initialization is done." << LL_ENDL ;

	// Load translations for tooltips
	LLFloater::initClass();

	/////////////////////////////////////////////////

	LLToolMgr::getInstance(); // Initialize tool manager if not already instantiated
		
	/////////////////////////////////////////////////
	//
	// Load settings files
	//
	//
	LLGroupMgr::parseRoleActions("role_actions.xml");

	LLAgent::parseTeleportMessages("teleport_strings.xml");

	// load MIME type -> media impl mappings
	std::string mime_types_name;
#if LL_DARWIN
	mime_types_name = "mime_types_mac.xml";
#elif LL_LINUX
	mime_types_name = "mime_types_linux.xml";
#else
	mime_types_name = "mime_types_windows.xml";
#endif
	LLMIMETypes::parseMIMETypes( mime_types_name ); 

	// Copy settings to globals. *TODO: Remove or move to appropriate class initializers
	settings_to_globals();
	// Setup settings listeners
	settings_setup_listeners();
	// Modify settings based on system configuration and compile options
	settings_modify();
	// Work around for a crash bug when changing OpenGL settings
	LLFontFreetype::sOpenGLcrashOnRestart = (getenv("LL_OPENGL_RESTART_CRASH_BUG") != NULL);

	// Find partition serial number (Windows) or hardware serial (Mac)
	mSerialNumber = generateSerialNumber();

	// do any necessary set-up for accepting incoming SLURLs from apps
	initSLURLHandler();

	if(false == initHardwareTest())
	{
		// Early out from user choice.
		return false;
	}
	LL_INFOS("InitInfo") << "Hardware test initialization done." << LL_ENDL ;
	
	// Always fetch the Ethernet MAC address, needed both for login
	// and password load.
	LLUUID::getNodeID(gMACAddress);

	// Prepare for out-of-memory situations, during which we will crash on
	// purpose and save a dump.
#if LL_WINDOWS && LL_RELEASE_FOR_DOWNLOAD && LL_USE_SMARTHEAP
	MemSetErrorHandler(first_mem_error_handler);
#endif // LL_WINDOWS && LL_RELEASE_FOR_DOWNLOAD && LL_USE_SMARTHEAP

	// *Note: this is where gViewerStats used to be created.

	//
	// Initialize the VFS, and gracefully handle initialization errors
	//

	if (!initCache())
	{
		LL_WARNS("InitInfo") << "Failed to init cache" << LL_ENDL;
		std::ostringstream msg;
		msg << LLTrans::getString("MBUnableToAccessFile");
		OSMessageBox(msg.str(),LLStringUtil::null,OSMB_OK);
		return true;
	}
	LL_INFOS("InitInfo") << "Cache initialization is done." << LL_ENDL ;

	// Initialize the repeater service.
	LLMainLoopRepeater::instance().start();

	//
	// Initialize the window
	//
	gGLActive = TRUE;
	initWindow();
	LL_INFOS("InitInfo") << "Window is initialized." << LL_ENDL ;

	// initWindow also initializes the Feature List, so now we can initialize this global.
	LLCubeMap::sUseCubeMaps = LLFeatureManager::getInstance()->isFeatureAvailable("RenderCubeMap");

	// call all self-registered classes
	LLInitClassList::instance().fireCallbacks();

	LLFolderViewItem::initClass(); // SJB: Needs to happen after initWindow(), not sure why but related to fonts
		
	gGLManager.getGLInfo(gDebugInfo);
	gGLManager.printGLInfoString();

	writeDebugInfo();

	// Load Default bindings
	load_default_bindings(gSavedSettings.getBOOL("LiruUseZQSDKeys"));

	// If we don't have the right GL requirements, exit.
	if (!gGLManager.mHasRequirements)
	{	
		// can't use an alert here since we're exiting and
		// all hell breaks lose.
		std::string msg = LLNotificationTemplates::instance().getGlobalString("UnsupportedGLRequirements");
		LLStringUtil::format(msg,LLTrans::getDefaultArgs());
		OSMessageBox(
			msg,
			LLStringUtil::null,
			OSMB_OK);
		return false;
	}

	// Without SSE2 support we will crash almost immediately, warn here.
	if (!gSysCPU.hasSSE2())
	{
		// can't use an alert here since we're exiting and
		// all hell breaks lose.
		std::string msg = LLNotificationTemplates::instance().getGlobalString("UnsupportedCPUSSE2");
		LLStringUtil::format(msg,LLTrans::getDefaultArgs());
		OSMessageBox(
			msg,
			LLStringUtil::null,
			OSMB_OK);
		return false;
	}

	// alert the user if they are using unsupported hardware
	if(!gSavedSettings.getBOOL("AlertedUnsupportedHardware"))
	{
		bool unsupported = false;
		LLSD args;
		std::string minSpecs;
		
		// get cpu data from xml
		std::stringstream minCPUString(LLNotificationTemplates::instance().getGlobalString("UnsupportedCPUAmount"));
		S32 minCPU = 0;
		minCPUString >> minCPU;

		// get RAM data from XML
		std::stringstream minRAMString(LLNotificationTemplates::instance().getGlobalString("UnsupportedRAMAmount"));
		U64Bytes minRAM;
		minRAMString >> minRAM;

		if(!LLFeatureManager::getInstance()->isGPUSupported() && LLFeatureManager::getInstance()->getGPUClass() != GPU_CLASS_UNKNOWN)
		{
			minSpecs += LLNotificationTemplates::instance().getGlobalString("UnsupportedGPU");
			minSpecs += "\n";
			unsupported = true;
		}
		if(gSysCPU.getMHz() < minCPU)
		{
			minSpecs += LLNotificationTemplates::instance().getGlobalString("UnsupportedCPU");
			minSpecs += "\n";
			unsupported = true;
		}
		if(gSysMemory.getPhysicalMemoryKB() < minRAM)
		{
			minSpecs += LLNotificationTemplates::instance().getGlobalString("UnsupportedRAM");
			minSpecs += "\n";
			unsupported = true;
		}

		if (LLFeatureManager::getInstance()->getGPUClass() == GPU_CLASS_UNKNOWN)
		{
			LLNotificationsUtil::add("UnknownGPU");
		} 
			
		if(unsupported)
		{
			if(!gSavedSettings.controlExists("WarnUnsupportedHardware") 
				|| gSavedSettings.getBOOL("WarnUnsupportedHardware"))
			{
				args["MINSPECS"] = minSpecs;
				LLNotificationsUtil::add("UnsupportedHardware", args );
			}

		}
	}

	// save the graphics card
	gDebugInfo["GraphicsCard"] = LLFeatureManager::getInstance()->getGPUString();

	writeDebugInfo();

	// Save the current version to the prefs file
	gSavedSettings.setString("LastRunVersion",
							 LLVersionInfo::getChannelAndVersion());

	gSimLastTime = gRenderStartTime.getElapsedTimeF32();
	gSimFrames = (F32)gFrameCount;

	LLViewerJoystick::getInstance()->init(false);

	// Finish windlight initialization.
	LLWLParamManager::instance().initHack();
	// Use prefered Environment.
	LLEnvManagerNew::instance().usePrefs();

	gGLActive = FALSE;
	LLViewerMedia::initClass();
	LL_INFOS("InitInfo") << "Viewer media initialized." << LL_ENDL ;

	writeDebugInfo();
	return true;
}

void LLAppViewer::initMaxHeapSize()
{
	//set the max heap size.
	//here is some info regarding to the max heap size:
	//------------------------------------------------------------------------------------------
	// OS       | setting | SL address bits | max manageable memory space | max heap size
	// Win 32   | default | 32-bit          | 2GB                         | < 1.7GB
	// Win 32   | /3G     | 32-bit          | 3GB                         | < 1.7GB or 2.7GB
	// Win 64   | default | 32-bit          | 2GB                         | < 1.7GB
	// Win 64   | LAA     | 32-bit          | 3GB                         | < 3.7GB
	//Linux 32  | default | 32-bit          | 3GB                         | < 2.7GB
	//Linux 32  |HUGEMEM  | 32-bit          | 4GB                         | < 3.7GB
	//64-bit OS |default  | 32-bit          | 4GB                         | < 3.7GB
	//64-bit OS |default  | 64-bit          | N/A (> 4GB)                 | N/A (> 4GB)
	//------------------------------------------------------------------------------------------
	//currently SL is built under 32-bit setting, we set its max heap size no more than 1.6 GB.

	//F32 max_heap_size_gb = llmin(1.6f, (F32)gSavedSettings.getF32("MaxHeapSize")) ;
	F32Gigabytes max_heap_size_gb = (F32Gigabytes)gSavedSettings.getF32("MaxHeapSize") ;

	//This is all a bunch of CRAP. We run LAA on windows. 64bit windows supports LAA out of the box. 32bit does not, unless PAE is on.
#if LL_WINDOWS
	//http://msdn.microsoft.com/en-us/library/windows/desktop/ms684139%28v=vs.85%29.aspx
	//Easier than GetNativeSystemInfo.
	BOOL bWow64Process = FALSE;
	typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
	LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
	
	HKEY hKey;
	 
	if(fnIsWow64Process && fnIsWow64Process(GetCurrentProcess(), &bWow64Process) && bWow64Process)
	{
		max_heap_size_gb = F32Gigabytes(3.7f);
	}
	else if(ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management"), &hKey))
	{
		DWORD dwSize = sizeof(DWORD);
		DWORD dwResult = 0;
		if(ERROR_SUCCESS == RegQueryValueEx(hKey, TEXT("PhysicalAddressExtension"), NULL, NULL, (LPBYTE)&dwResult, &dwSize))
		{
			if(dwResult)
				max_heap_size_gb = F32Gigabytes(3.7f);
		}
		RegCloseKey(hKey);
	}
#endif

	BOOL enable_mem_failure_prevention = gSavedSettings.getBOOL("MemoryFailurePreventionEnabled") ;

	LLMemory::initMaxHeapSizeGB(max_heap_size_gb, enable_mem_failure_prevention) ;
}

void LLAppViewer::checkMemory()
{
	const static F32 MEMORY_CHECK_INTERVAL = 1.0f ; //second
	//const static F32 MAX_QUIT_WAIT_TIME = 30.0f ; //seconds
	//static F32 force_quit_timer = MAX_QUIT_WAIT_TIME + MEMORY_CHECK_INTERVAL ;

	if(!gGLManager.mDebugGPU)
	{
		return ;
	}

	if(MEMORY_CHECK_INTERVAL > mMemCheckTimer.getElapsedTimeF32())
	{
		return ;
	}
	mMemCheckTimer.reset() ;

		//update the availability of memory
		LLMemory::updateMemoryInfo() ;

	bool is_low = LLMemory::isMemoryPoolLow() ;

	LLPipeline::throttleNewMemoryAllocation(is_low) ;		
	
	if(is_low)
	{
		LLMemory::logMemoryInfo() ;
	}
}

static LLTrace::BlockTimerStatHandle FTM_MESSAGES("System Messages");
static LLTrace::BlockTimerStatHandle FTM_SLEEP("Sleep");
static LLTrace::BlockTimerStatHandle FTM_YIELD("Yield");

static LLTrace::BlockTimerStatHandle FTM_TEXTURE_CACHE("Texture Cache");
static LLTrace::BlockTimerStatHandle FTM_DECODE("Image Decode");
static LLTrace::BlockTimerStatHandle FTM_VFS("VFS Thread");
static LLTrace::BlockTimerStatHandle FTM_LFS("LFS Thread");
static LLTrace::BlockTimerStatHandle FTM_PAUSE_THREADS("Pause Threads");
static LLTrace::BlockTimerStatHandle FTM_IDLE("Idle");
static LLTrace::BlockTimerStatHandle FTM_PUMP("Pump");
static LLTrace::BlockTimerStatHandle FTM_PUMP_ARES("Ares");
static LLTrace::BlockTimerStatHandle FTM_PUMP_SERVICE("Service");
static LLTrace::BlockTimerStatHandle FTM_SERVICE_CALLBACK("Callback");
static LLTrace::BlockTimerStatHandle FTM_AGENT_AUTOPILOT("Autopilot");
static LLTrace::BlockTimerStatHandle FTM_AGENT_UPDATE("Update");
static LLTrace::BlockTimerStatHandle FTM_STATEMACHINE("State Machine");

bool LLAppViewer::mainLoop()
{
	mMainloopTimeout = new LLWatchdogTimeout();
	// *FIX:Mani - Make this a setting, once new settings exist in this branch.
	
	//-------------------------------------------
	// Run main loop until time to quit
	//-------------------------------------------

	// Create IO Pump to use for HTTP Requests.
	gServicePump = new LLPumpIO;
	LLCurl::setCAFile(gDirUtilp->getCAFile());
	
	// Note: this is where gLocalSpeakerMgr and gActiveSpeakerMgr used to be instantiated.

	LLVoiceChannel::initClass();
	LLVoiceClient::getInstance()->init(gServicePump);
				
	LLTimer frameTimer,idleTimer,periodicRenderingTimer;
	LLTimer debugTime;
	LLFrameTimer memCheckTimer;
	LLViewerJoystick* joystick(LLViewerJoystick::getInstance());
	joystick->setNeedsReset(true);

	LLEventPump& mainloop(LLEventPumps::instance().obtain("mainloop"));

	// merge grid info from web site, if newer.
	if (gSavedSettings.getBOOL("CheckForGridUpdates"))
		gHippoGridManager->parseUrl();

	// As we do not (yet) send data on the mainloop LLEventPump that varies
	// with each frame, no need to instantiate a new LLSD event object each
	// time. Obviously, if that changes, just instantiate the LLSD at the
	// point of posting.
	LLSD newFrame;

	BOOL restore_rendering_masks = FALSE;

	// Handle messages
	try
	{
		while (!LLApp::isExiting())
		{
			LLFastTimer::nextFrame(); // Should be outside of any timer instances

			//clear call stack records
			LL_CLEAR_CALLSTACKS();

#ifdef USE_CRASHPAD
			// Not event based. Update per frame
			SET_CRASHPAD_ANNOTATION_VALUE(session_duration, fmt::to_string(LLFrameTimer::getElapsedSeconds()));
			SET_CRASHPAD_ANNOTATION_VALUE(memory_alloc, fmt::to_string((LLMemory::getCurrentRSS() >> 10)/1000.f));
#endif

			//check memory availability information
			checkMemory() ;
		
			// Check if we need to restore rendering masks.
			if (restore_rendering_masks)
			{
				gPipeline.popRenderDebugFeatureMask();
				gPipeline.popRenderTypeMask();
			}
			// Check if we need to temporarily enable rendering.
			static const LLCachedControl<F32> periodic_rendering("ForcePeriodicRenderingTime", 0.f);
			if (periodic_rendering > F_APPROXIMATELY_ZERO && periodicRenderingTimer.getElapsedTimeF64() > periodic_rendering)
			{
				periodicRenderingTimer.reset();
				restore_rendering_masks = TRUE;
				gPipeline.pushRenderTypeMask();
				gPipeline.pushRenderDebugFeatureMask();
				gPipeline.setAllRenderTypes();
				gPipeline.setAllRenderDebugFeatures();
			}
			else
			{
				restore_rendering_masks = FALSE;
			}

			pingMainloopTimeout("Main:MiscNativeWindowEvents");

			if (gViewerWindow)
			{
				LL_RECORD_BLOCK_TIME(FTM_MESSAGES);
				gViewerWindow->getWindow()->processMiscNativeEvents();
			}
		
			pingMainloopTimeout("Main:GatherInput");
			
			if (gViewerWindow)
			{
				LL_RECORD_BLOCK_TIME(FTM_MESSAGES);
				if (!restoreErrorTrap())
				{
					LL_WARNS() << " Someone took over my signal/exception handler (post messagehandling)!" << LL_ENDL;
				}

				gViewerWindow->getWindow()->gatherInput();
			}

#if 1 && !LL_RELEASE_FOR_DOWNLOAD
			// once per second debug info
			if (debugTime.getElapsedTimeF32() > 1.f)
			{
				debugTime.reset();
			}
			
#endif
			//memory leaking simulation
			if (auto mem_leak_instance = LLFloaterMemLeak::getInstance())
			{
				mem_leak_instance->idle();
			}

			// canonical per-frame event
			mainloop.post(newFrame);

			if (!LLApp::isExiting())
			{
				pingMainloopTimeout("Main:JoystickKeyboard");
				
				// Scan keyboard for movement keys.  Command keys and typing
				// are handled by windows callbacks.  Don't do this until we're
				// done initializing.  JC
				if (gViewerWindow->getWindow()->getVisible()
					&& gViewerWindow->getActive()
					&& !gViewerWindow->getWindow()->getMinimized()
					&& LLStartUp::getStartupState() == STATE_STARTED
					&& !gViewerWindow->getShowProgress()
					&& !gFocusMgr.focusLocked())
				{
					joystick->scanJoystick();
					gKeyboard->scanKeyboard();
					if (gAgent.isCrouching())
						gAgent.moveUp(-1);
				}

				// Update state based on messages, user input, object idle.
				{
					pauseMainloopTimeout(); // *TODO: Remove. Messages shouldn't be stalling for 20+ seconds!
					
					LL_RECORD_BLOCK_TIME(FTM_IDLE);
					// <edit> bad_alloc!! </edit>
					idle();

					if (gAres != NULL && gAres->isInitialized())
					{
						pingMainloopTimeout("Main:ServicePump");				
						LL_RECORD_BLOCK_TIME(FTM_PUMP);
						{
							LL_RECORD_BLOCK_TIME(FTM_PUMP_ARES);
							gAres->process();
						}
						{
							LL_RECORD_BLOCK_TIME(FTM_PUMP_SERVICE);
							// this pump is necessary to make the login screen show up
							gServicePump->pump();

							{
								LL_RECORD_BLOCK_TIME(FTM_SERVICE_CALLBACK);
								gServicePump->callback();
							}
						}
					}
					
					resumeMainloopTimeout();
				}
 
				if (gDoDisconnect && (LLStartUp::getStartupState() == STATE_STARTED))
				{
					pauseMainloopTimeout();
					saveFinalSnapshot();
					disconnectViewer();
					resumeMainloopTimeout();
				}

				// Render scene.
				if (!LLApp::isExiting())
				{
					pingMainloopTimeout("Main:Display");
					gGLActive = TRUE;
					display();

					pingMainloopTimeout("Main:Snapshot");
					LLFloaterSnapshot::update(); // take snapshots
					gGLActive = FALSE;
				}
			}

			pingMainloopTimeout("Main:Sleep");
			
			pauseMainloopTimeout();

			// Sleep and run background threads
			{
				LL_RECORD_BLOCK_TIME(FTM_SLEEP);
				static const LLCachedControl<bool> run_multiple_threads("RunMultipleThreads",false);
				static const LLCachedControl<S32> yield_time("YieldTime", -1);
				// yield some time to the os based on command line option
				if(yield_time >= 0)
				{
					LL_RECORD_BLOCK_TIME(FTM_YIELD);
					ms_sleep(yield_time);
				}

				// yield cooperatively when not running as foreground window
				if (   gNoRender
					   || (gViewerWindow && !gViewerWindow->getWindow()->getVisible())
						|| (!gFocusMgr.getAppHasFocus() && !AIFilePicker::activePicker) )
				{
					// Sleep if we're not rendering, or the window is minimized.
					static LLCachedControl<S32> s_bacground_yeild_time(gSavedSettings, "BackgroundYieldTime", 40);
					S32 milliseconds_to_sleep = llclamp((S32)s_bacground_yeild_time, 0, 1000);
					// don't sleep when BackgroundYieldTime set to 0, since this will still yield to other threads
					// of equal priority on Windows
					if (milliseconds_to_sleep > 0)
					{
						//Prevent sleeping too long while in the login process (tends to cause stalling)
						if(LLStartUp::getStartupState() >= STATE_LOGIN_AUTH_INIT && LLStartUp::getStartupState() < STATE_STARTED)
							milliseconds_to_sleep = llmin(milliseconds_to_sleep,250);
						ms_sleep(milliseconds_to_sleep);
						// also pause worker threads during this wait period
						LLAppViewer::getTextureCache()->pause();
						LLAppViewer::getImageDecodeThread()->pause();
					}
				}
				
				if (gRandomizeFramerate)
				{
					ms_sleep(rand() % 200);
				}

				if (gPeriodicSlowFrame
					&& (gFrameCount % 10 == 0))
				{
					LL_INFOS() << "Periodic slow frame - sleeping 500 ms" << LL_ENDL;
					ms_sleep(500);
				}

				const F64 max_idle_time = run_multiple_threads ? 0.0 : llmin(.005*10.0*gFrameIntervalSeconds.value(), 0.005); // 50ms/second, no more than 5ms/frame
				idleTimer.reset();
				while(1)
				{
					S32 work_pending = 0;
					S32 io_pending = 0;
					{
						LL_RECORD_BLOCK_TIME(FTM_TEXTURE_CACHE);
						work_pending += LLAppViewer::getTextureCache()->update(1); // unpauses the texture cache thread
					}
					{
						LL_RECORD_BLOCK_TIME(FTM_DECODE);
						work_pending += LLAppViewer::getImageDecodeThread()->update(1); // unpauses the image thread
					}
					{
						LL_RECORD_BLOCK_TIME(FTM_DECODE);
						work_pending += LLAppViewer::getTextureFetch()->update(1); // unpauses the texture fetch thread
					}

					{
						LL_RECORD_BLOCK_TIME(FTM_VFS);
						io_pending += LLVFSThread::updateClass(1);
					}
					{
						LL_RECORD_BLOCK_TIME(FTM_LFS);
						io_pending += LLLFSThread::updateClass(1);
					}

					if (io_pending > 1000)
					{
						ms_sleep(llmin(io_pending/100,100)); // give the vfs some time to catch up
					}

					F64 idle_time = idleTimer.getElapsedTimeF64();
					if (!work_pending || idle_time >= max_idle_time)
					{
						break;
					}
				}
				if ((LLStartUp::getStartupState() >= STATE_CLEANUP) &&
					(frameTimer.getElapsedTimeF64() > FRAME_STALL_THRESHOLD))
				{
					gFrameStalls++;
				}
				frameTimer.reset();

				 // Prevent the worker threads from running while rendering.
				// if (LLThread::processorCount()==1) //pause() should only be required when on a single processor client...
				if (run_multiple_threads == FALSE)
				{
					LLAppViewer::getTextureCache()->pause();
					LLAppViewer::getImageDecodeThread()->pause();
					// LLAppViewer::getTextureFetch()->pause(); // Don't pause the fetch (IO) thread
				}
				//LLVFSThread::sLocal->pause(); // Prevent the VFS thread from running while rendering.

				resumeMainloopTimeout();
	
				pingMainloopTimeout("Main:End");
			}
		}

		// Save snapshot for next time, if we made it through initialization
		if (STATE_STARTED == LLStartUp::getStartupState())
			saveFinalSnapshot();
	}
	catch(std::bad_alloc)
	{
		LLMemory::logMemoryInfo(TRUE);

		//stop memory leaking simulation
		if (auto mem_leak_instance = LLFloaterMemLeak::getInstance())
			mem_leak_instance->stop();

		//output possible call stacks to log file.
		LLError::LLCallStacks::print();

		LL_ERRS() << "Bad memory allocation in LLAppViewer::mainLoop()!" << LL_ENDL;
	}
	
	delete gServicePump;

	destroyMainloopTimeout();

	LL_INFOS() << "Exiting main_loop" << LL_ENDL;

	return true;
}

void LLAppViewer::flushVFSIO()
{
	while (true)
	{
		S32 pending = LLVFSThread::updateClass(0);
		pending += LLLFSThread::updateClass(0);
		if (!pending)
		{
			break;
		}
		LL_INFOS() << "Waiting for pending IO to finish: " << pending << LL_ENDL;
		ms_sleep(100);
	}
}

extern void cleanup_pose_stand(void);

bool LLAppViewer::cleanup()
{
	//ditch LLVOAvatarSelf instance
	gAgentAvatarp = nullptr;


	// remove any old breakpad minidump files from the log directory
	if (! isError())
	{
		std::string logdir = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "");
		gDirUtilp->deleteFilesInDir(logdir, "*-*-*-*-*.dmp");
	}

	cleanup_pose_stand();

	//flag all elements as needing to be destroyed immediately
	// to ensure shutdown order
	LLMortician::setZealous(TRUE);

	LLVoiceClient::getInstance()->terminate();
	
	disconnectViewer();

	LL_INFOS() << "Viewer disconnected" << LL_ENDL;

	display_cleanup(); 

	release_start_screen(); // just in case

	LLError::logToFixedBuffer(nullptr); // stop the fixed buffer recorder

	LL_INFOS() << "Cleaning Up" << LL_ENDL;

	// shut down mesh streamer
	gMeshRepo.shutdown();

	// Must clean up texture references before viewer window is destroyed.
	if(LLHUDManager::instanceExists())
	{
		LLHUDManager::getInstance()->updateEffects();
		LLHUDObject::updateAll();
		LLHUDManager::getInstance()->cleanupEffects();
		LLHUDObject::cleanupHUDObjects();
		LL_INFOS() << "HUD Objects cleaned up" << LL_ENDL;
	}

	// End TransferManager before deleting systems it depends on (Audio, VFS, AssetStorage)
#if 0 // this seems to get us stuck in an infinite loop...
	gTransferManager.cleanup();
#endif
	
	// Note: this is where gWorldMap used to be deleted.

	// Note: this is where gHUDManager used to be deleted.
	if(LLHUDManager::instanceExists())
	{
		LLHUDManager::getInstance()->shutdownClass();
	}

	delete gAssetStorage;
	gAssetStorage = nullptr;

	LLPolyMesh::freeAllMeshes();

	LLStartUp::cleanupNameCache();

	// Note: this is where gLocalSpeakerMgr and gActiveSpeakerMgr used to be deleted.

	if (LLWorldMap::instanceExists())
	{
		LLWorldMap::getInstance()->reset(); // release any images
	}

	LLCalc::cleanUp();

	AOSystem::deleteSingleton();

	LL_INFOS() << "Global stuff deleted" << LL_ENDL;

	// Note: this is where LLFeatureManager::getInstance()-> used to be deleted.

	// Patch up settings for next time
	// Must do this before we delete the viewer window,
	// such that we can suck rectangle information out of
	// it.
	cleanupSavedSettings();
	LL_INFOS() << "Settings patched up" << LL_ENDL;

	// <edit> moving this to below.
	/*
	// </edit>
	// delete some of the files left around in the cache.
	removeCacheFiles("*.wav");
	removeCacheFiles("*.tmp");
	removeCacheFiles("*.lso");
	removeCacheFiles("*.out");
	removeCacheFiles("*.dsf");
	removeCacheFiles("*.bodypart");
	removeCacheFiles("*.clothing");
	
	// <edit>
	LL_INFOS() << "Cache files removed" << LL_ENDL;
	*/
	// </edit>

	void cleanup_menus();
	cleanup_menus();

	// Wait for any pending VFS IO
	flushVFSIO();
	LL_INFOS() << "Shutting down Views" << LL_ENDL;

	// Destroy the UI
	if( gViewerWindow)
		gViewerWindow->shutdownViews();

	LL_INFOS() << "Cleaning up Inventory" << LL_ENDL;
	
	// Cleanup Inventory after the UI since it will delete any remaining observers
	// (Deleted observers should have already removed themselves)
	gInventory.cleanupInventory();

	LL_INFOS() << "Cleaning up Selections" << LL_ENDL;
	
	// Clean up selection managers after UI is destroyed, as UI may be observing them.
	// Clean up before GL is shut down because we might be holding on to objects with texture references
	LLSelectMgr::cleanupGlobals();
	
	// Clean up before shutdownGL.
	LLPostProcess::cleanupClass();

	LL_INFOS() << "Shutting down OpenGL" << LL_ENDL;

	// Shut down OpenGL
	if( gViewerWindow)
	{
		gViewerWindow->shutdownGL();
	
		// Destroy window, and make sure we're not fullscreen
		// This may generate window reshape and activation events.
		// Therefore must do this before destroying the message system.
		delete gViewerWindow;
		gViewerWindow = nullptr;
		LL_INFOS() << "ViewerWindow deleted" << LL_ENDL;
	}

	LL_INFOS() << "Cleaning up Keyboard & Joystick" << LL_ENDL;
	
	// viewer UI relies on keyboard so keep it aound until viewer UI isa gone
	delete gKeyboard;
	gKeyboard = nullptr;

	
	LL_INFOS() << "Cleaning up Objects" << LL_ENDL;
	
	LLViewerObject::cleanupVOClasses();

	LLAvatarAppearance::cleanupClass();

	LLTracker::cleanupInstance();
	
	// *FIX: This is handled in LLAppViewerWin32::cleanup().
	// I'm keeping the comment to remember its order in cleanup,
	// in case of unforseen dependency.
	//#if LL_WINDOWS
	//	gDXHardware.cleanup();
	//#endif // LL_WINDOWS

	LLVolumeMgr* volume_manager = LLPrimitive::getVolumeManager();
	if (!volume_manager->cleanup())
	{
		LL_WARNS() << "Remaining references in the volume manager!" << LL_ENDL;
	}
	LLPrimitive::cleanupVolumeManager();

	LL_INFOS() << "Additional Cleanup..." << LL_ENDL;
	
	LLViewerParcelMgr::cleanupGlobals();

	// *Note: this is where gViewerStats used to be deleted.

	//end_messaging_system();

	LLFollowCamMgr::cleanupClass();
	//LLVolumeMgr::cleanupClass();
	LLPrimitive::cleanupVolumeManager();
	LLWorldMapView::cleanupClass();
	LLFolderViewItem::cleanupClass();
	LLUI::cleanupClass();
	
	//
	// Shut down the VFS's AFTER the decode manager cleans up (since it cleans up vfiles).
	// Also after viewerwindow is deleted, since it may have image pointers (which have vfiles)
	// Also after shutting down the messaging system since it has VFS dependencies

	//
	LL_INFOS() << "Cleaning up VFS" << LL_ENDL;
	LLVFile::cleanupClass();

	LL_INFOS() << "Saving Data" << LL_ENDL;

	// Quitting with "Remember Password" turned off should always stomp your
	// saved password, whether or not you successfully logged in.  JC
	if (!gSavedSettings.getBOOL("RememberPassword"))
	{
		LLStartUp::deletePasswordFromDisk();
	}
	
	// Store the time of our current logoff
	gSavedPerAccountSettings.setU32("LastLogoff", time_corrected());

	// Must do this after all panels have been deleted because panels that have persistent rects
	// save their rects on delete.
	gSavedSettings.saveToFile(gSavedSettings.getString("ClientSettingsFile"), TRUE);	

	// PerAccountSettingsFile should be empty if no user has been logged on.
	// *FIX:Mani This should get really saved in a "logoff" mode. 
	if (gSavedSettings.getString("PerAccountSettingsFile").empty())
	{
		LL_INFOS() << "Not saving per-account settings; don't know the account name yet." << LL_ENDL;
	}
	else
	{
		gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"), TRUE);
		LL_INFOS() << "Saved settings" << LL_ENDL;
	}

	// Save URL history file
	LLURLHistory::saveFile("url_history.xml");

	// Save file- and dirpicker {context, default paths} map.
	AIFilePicker::saveFile("filepicker_contexts.xml");

	LLFloaterTeleportHistory::saveFile("teleport_history.xml");

	LocalAssetBrowser::deleteSingleton(); // <edit/>

	// save mute list. gMuteList used to also be deleted here too.
	LLMuteList::getInstance()->cache(gAgent.getID());


	if (mPurgeOnExit)
	{
		LL_INFOS() << "Purging all cache files on exit" << LL_ENDL;
		gDirUtilp->deleteFilesInDir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE,""),"*.*");
	}

	// <edit> moved this stuff from above to make it conditional here...
	if(!mSecondInstance)
	{
		removeCacheFiles("*.wav");
		removeCacheFiles("*.tmp");
		removeCacheFiles("*.lso");
		removeCacheFiles("*.out");
		removeCacheFiles("*.dsf");
		removeCacheFiles("*.bodypart");
		removeCacheFiles("*.clothing");
		LL_INFOS() << "Cache files removed" << LL_ENDL;
	}
	else
	{
		LL_INFOS() << "Not removing cache files. Other viewer instance detected." << LL_ENDL;
	}
	// </edit>

	writeDebugInfo();

	// Stop the plugin read thread if it's running.
	LLPluginProcessParent::setUseReadThread(false);
	// Stop curl responder call backs.
	AICurlInterface::shutdownCurl();

	LL_INFOS() << "Shutting down Threads" << LL_ENDL;

	// Let threads finish
	LLTimer idleTimer;
	idleTimer.reset();
	const F64 max_idle_time = 5.f; // 5 seconds
	while(true)
	{
		S32 pending = 0;
		pending += LLAppViewer::getTextureCache()->update(1); // unpauses the worker thread
		pending += LLAppViewer::getImageDecodeThread()->update(1); // unpauses the image thread
		pending += LLAppViewer::getTextureFetch()->update(1); // unpauses the texture fetch thread
		pending += LLVFSThread::updateClass(0);
		pending += LLLFSThread::updateClass(0);
		if (!pending)
		{
			break ; //done
		}
		else if (idleTimer.getElapsedTimeF64() >= max_idle_time)
		{
			LL_WARNS() << "Quitting with pending background tasks." << LL_ENDL;
			break;
		}
	}
	
	// Delete workers first
	// shotdown all worker threads before deleting them in case of co-dependencies
	sTextureFetch->shutdown();
	sTextureCache->shutdown();
	sImageDecodeThread->shutdown();

	sTextureFetch->shutDownTextureCacheThread();
	sTextureFetch->shutDownImageDecodeThread();
	delete sTextureCache;
    sTextureCache = nullptr;
	delete sTextureFetch;
    sTextureFetch = nullptr;
	delete sImageDecodeThread;
    sImageDecodeThread = nullptr;



	LL_INFOS() << "Cleaning up Media and Textures" << LL_ENDL;

	//Note:
	//LLViewerMedia::cleanupClass() has to be put before gTextureList.shutdown()
	//because some new image might be generated during cleaning up media. --bao
	LLViewerMedia::cleanupClass();
	LLViewerParcelMedia::cleanupClass();
	gTextureList.shutdown(); // shutdown again in case a callback added something
	LLUIImageList::getInstance()->cleanUp();
	
	// This should eventually be done in LLAppViewer
	LLImage::cleanupClass();
	LLVFSThread::cleanupClass();
	LLLFSThread::cleanupClass();

	LL_INFOS() << "VFS Thread finished" << LL_ENDL;

#ifndef LL_RELEASE_FOR_DOWNLOAD
	LL_INFOS() << "Auditing VFS" << LL_ENDL;
	if(gVFS)
	{
		gVFS->audit();
	}
#endif

	LL_INFOS() << "Misc Cleanup" << LL_ENDL;
	
	// For safety, the LLVFS has to be deleted *after* LLVFSThread. This should be cleaned up.
	// (LLVFS doesn't know about LLVFSThread so can't kill pending requests) -Steve
	delete gStaticVFS;
	gStaticVFS = nullptr;
	delete gVFS;
	gVFS = nullptr;

	LLWatchdog::getInstance()->cleanup();

	LL_INFOS() << "Shutting down message system" << LL_ENDL;
	end_messaging_system();
	LL_INFOS() << "Message system deleted." << LL_ENDL;

	LLApp::stopErrorThread();			// The following call is not thread-safe. Have to stop all threads.
	stopEngineThread();
	AICurlInterface::cleanupCurl();

	// Cleanup settings last in case other classes reference them.
	gSavedSettings.cleanup();
	gColors.cleanup();
	LLViewerAssetStatsFF::cleanup();
		
	// If we're exiting to launch an URL, do that here so the screen
	// is at the right resolution before we launch IE.
	if (!gLaunchFileOnQuit.empty())
	{
		LL_INFOS() << "Launch file on quit." << LL_ENDL;
#if LL_WINDOWS
		// Indicate an application is starting.
		SetCursor(LoadCursor(nullptr, IDC_WAIT));
#endif

		// HACK: Attempt to wait until the screen res. switch is complete.
		ms_sleep(1000);

		LLWeb::loadURLExternal( gLaunchFileOnQuit );
		LL_INFOS() << "File launched." << LL_ENDL;
	}

	LLWearableType::cleanupClass();

	LLMainLoopRepeater::instance().stop();

	//release all private memory pools.
	LLPrivateMemoryPoolManager::destroyClass() ;

	ll_close_fail_log();

	LLError::LLCallStacks::cleanup();

	removeMarkerFiles();

	MEM_TRACK_RELEASE

	LL_INFOS() << "Goodbye!" << LL_ENDL;

	// return 0;
	return true;
}

// A callback for LL_ERRS() to call during the watchdog error.
void watchdog_llerrs_callback(const std::string &error_string)
{
	gLLErrorActivated = true;

#ifdef LL_WINDOWS
	RaiseException(0,0,0,nullptr);
#else
	raise(SIGQUIT);
#endif
}

// A callback for the watchdog to call.
void watchdog_killer_callback()
{
	LLError::setFatalFunction(watchdog_llerrs_callback);
	LL_ERRS() << "Watchdog killer event" << LL_ENDL;
}

bool LLAppViewer::initThreads()
{
	static const bool enable_threads = true;

	bool use_watchdog = gSavedSettings.getBOOL("WatchdogEnabled");
	bool send_reports = gSavedSettings.getS32("CrashSubmitBehavior") == 1;
	if(use_watchdog && send_reports)
	{
		LLWatchdog::getInstance()->init(watchdog_killer_callback);
	}

	// State machine thread.
	startEngineThread();

	AICurlInterface::startCurlThread(&gSavedSettings);

	LLImage::initClass();
	
	LLVFSThread::initClass(enable_threads && false);
	LLLFSThread::initClass(enable_threads && false);

	// Image decoding
	LLAppViewer::sImageDecodeThread = new LLImageDecodeThread(enable_threads && true);
	LLAppViewer::sTextureCache = new LLTextureCache(enable_threads && true);
	LLAppViewer::sTextureFetch = new LLTextureFetch(LLAppViewer::getTextureCache(),
													sImageDecodeThread,
													enable_threads && true,
													app_metrics_qa_mode);	


	// Mesh streaming and caching
	gMeshRepo.init();

	// *FIX: no error handling here!
	return true;
}

void errorCallback(const std::string &error_string)
{
	static std::string last_message;
	if(last_message != error_string)
	{
		U32 response = OSMessageBox(error_string, LLTrans::getString("MBFatalError"), OSMB_YESNO);
		if (response == OSBTN_NO)
		{
			last_message = error_string;
			return;
		}

		//Set the ErrorActivated global so we know to create a marker file
		gLLErrorActivated = true;

		gDebugInfo["FatalMessage"] = error_string;
		// We're not already crashing -- we simply *intend* to crash. Since we
		// haven't actually trashed anything yet, we can afford to write the whole
		// static info file.
		LLAppViewer::instance()->writeDebugInfo();

		LLError::crashAndLoop(error_string);
	}
}

void LLAppViewer::initLoggingAndGetLastDuration()
{
	//
	// Set up logging defaults for the viewer
	//
	LLError::initForApplication(
				gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "")
								);
	LLError::setFatalFunction(errorCallback);
	//LLError::setTimeFunction(getRuntime);
	
	initLoggingInternal();
}
void LLAppViewer::initLoggingInternal()
{
	// Remove the last ".old" log file.
	std::string old_log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
								 OLD_LOG_FILE);
	LLFile::remove(old_log_file);

	// Get name of the log file
	std::string log_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
								 LOG_FILE);
 	/*
	 * Before touching any log files, compute the duration of the last run
	 * by comparing the ctime of the previous start marker file with the ctime
	 * of the last log file.
	 */
	std::string start_marker_file_name = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, START_MARKER_FILE_NAME);
	llstat start_marker_stat;
	llstat log_file_stat;
	std::ostringstream duration_log_stream; // can't log yet, so save any message for when we can below
	int start_stat_result = LLFile::stat(start_marker_file_name, &start_marker_stat);
	int log_stat_result = LLFile::stat(log_file, &log_file_stat);
	if ( 0 == start_stat_result && 0 == log_stat_result )
	{
		int elapsed_seconds = log_file_stat.st_ctime - start_marker_stat.st_ctime;
		// only report a last run time if the last viewer was the same version
		// because this stat will be counted against this version
		if ( markerIsSameVersion(start_marker_file_name) )
		{
			gLastExecDuration = elapsed_seconds;
		}
		else
		{
			duration_log_stream << "start marker from some other version; duration is not reported";
			gLastExecDuration = -1;
		}
	}
	else
	{
		// at least one of the LLFile::stat calls failed, so we can't compute the run time
		duration_log_stream << "duration stat failure; start: "<< start_stat_result << " log: " << log_stat_result;
		gLastExecDuration = -1; // unknown
	}
	std::string duration_log_msg(duration_log_stream.str());

	// Create a new start marker file for comparison with log file time for the next run
	LLAPRFile start_marker_file ;
	start_marker_file.open(start_marker_file_name, LL_APR_WB);
	if (start_marker_file.getFileHandle())
	{
		recordMarkerVersion(start_marker_file);
		start_marker_file.close();
	}

	// Rename current log file to ".old"
	LLFile::rename(log_file, old_log_file);

	// Set the log file to SecondLife.log
	LLError::logToFile(log_file);
	if (!duration_log_msg.empty())
	{
		LL_WARNS("MarkerFile") << duration_log_msg << LL_ENDL;
	}
}

bool LLAppViewer::loadSettingsFromDirectory(AIReadAccess<settings_map_type> const& settings_r,
											std::string const& location_key,
											bool set_defaults)
{	
	// Find and vet the location key.
	if(!mSettingsLocationList.has(location_key))
	{
		LL_ERRS() << "Requested unknown location: " << location_key << LL_ENDL;
		return false;
	}

	LLSD location = mSettingsLocationList.get(location_key);

	if(!location.has("PathIndex"))
	{
		LL_ERRS() << "Settings location is missing PathIndex value. Settings cannot be loaded." << LL_ENDL;
		return false;
	}
	ELLPath path_index = (ELLPath)(location.get("PathIndex").asInteger());
	if(path_index <= LL_PATH_NONE || path_index >= LL_PATH_LAST)
	{
		LL_ERRS() << "Out of range path index in app_settings/settings_files.xml" << LL_ENDL;
		return false;
	}

	// Iterate through the locations list of files.
	LLSD files = location.get("Files");
	for(LLSD::map_iterator itr = files.beginMap(); itr != files.endMap(); ++itr)
	{
		std::string const settings_group = (*itr).first;
		settings_map_type::const_iterator const settings_group_iter = settings_r->find(settings_group);

		LL_INFOS() << "Attempting to load settings for the group " << settings_group 
				<< " - from location " << location_key << LL_ENDL;

		if(settings_group_iter == settings_r->end())
		{
			LL_WARNS() << "No matching settings group for name " << settings_group << LL_ENDL;
			continue;
		}

		LLSD file = (*itr).second;

		std::string full_settings_path;
		if(file.has("NameFromSetting"))
		{
			std::string custom_name_setting = file.get("NameFromSetting");
			// *NOTE: Regardless of the group currently being lodaed,
			// this setting is always read from the Global settings.
			LLControlGroup const* control_group = settings_r->find(sGlobalSettingsName)->second;
			if(control_group->controlExists(custom_name_setting))
			{
				full_settings_path = control_group->getString(custom_name_setting);
			}
		}

		if(full_settings_path.empty())
		{
			std::string file_name = file.get("Name");
			full_settings_path = gDirUtilp->getExpandedFilename(path_index, file_name);
		}

		int requirement = 0;
		if(file.has("Requirement"))
		{
			requirement = file.get("Requirement").asInteger();
		}
		
		if(!settings_group_iter->second->loadFromFile(full_settings_path, set_defaults))
		{
			if(requirement == 1)
			{
				LL_WARNS() << "Error: Cannot load required settings file from: " 
						<< full_settings_path << LL_ENDL;
				return false;
			}
			else
			{
				LL_WARNS() << "Cannot load " << full_settings_path << " - No settings found." << LL_ENDL;
			}
		}
		else
		{
			LL_INFOS() << "Loaded settings file " << full_settings_path << LL_ENDL;
		}
	}

	return true;
}

std::string LLAppViewer::getSettingsFilename(const std::string& location_key,
											 const std::string& file)
{
	if(mSettingsLocationList.has(location_key))
	{
		LLSD location = mSettingsLocationList.get(location_key);
		if(location.has("Files"))
		{
			LLSD files = location.get("Files");
			if(files.has(file) && files[file].has("Name"))
			{
				return files.get(file).get("Name").asString();
			}
		}
	}
	return std::string();
}

bool LLAppViewer::initConfiguration()
{	
	// Grab and hold write locks for the entire duration of this function.
	AIWriteAccess<settings_map_type> settings_w(gSettings);
	settings_map_type& settings(*settings_w);

	//Set up internal pointers	
	settings[sGlobalSettingsName] = &gSavedSettings;
	settings[sPerAccountSettingsName] = &gSavedPerAccountSettings;

	//Load settings files list
	std::string settings_file_list = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "settings_files.xml");
	LLControlGroup settings_control("groups");
	LL_INFOS() << "Loading settings file list" << settings_file_list << LL_ENDL;
	if (0 == settings_control.loadFromFile(settings_file_list))
	{
		OSMessageBox("Cannot load default configuration file " + settings_file_list + " The installation may be corrupted.",
			LLStringUtil::null,OSMB_OK);
		return false;
	}

	mSettingsLocationList = settings_control.getLLSD("Locations");
		
	// The settings and command line parsing have a fragile
	// order-of-operation:
	// - load defaults from app_settings
	// - set procedural settings values
	// - read command line settings
	// - selectively apply settings needed to load user settings.
	// - load overrides from user_settings 
	// - apply command line settings (to override the overrides)
	// - load per account settings (happens in llstartup
	
	// - load defaults
	bool set_defaults = true;
	if(!loadSettingsFromDirectory(settings_w, "Default", set_defaults))
	{
		OSMessageBox(
			"Unable to load default settings file. The installation may be corrupted.",
			LLStringUtil::null,OSMB_OK);
		return false;
	}

	LLUICtrlFactory::getInstance()->setupPaths(); // setup paths for LLTrans based on settings files only
	LLTrans::parseStrings("strings.xml", default_trans_args);

	//COA vars in gSavedSettings will be linked to gSavedPerAccountSettings entries that will be created if not present.
	//Signals will be shared between linked vars.
	gSavedSettings.connectCOAVars(gSavedPerAccountSettings);

	// - set procedural settings 
	// Note: can't use LL_PATH_PER_SL_ACCOUNT for any of these since we haven't logged in yet
	gSavedSettings.setString("ClientSettingsFile", 
		gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, getSettingsFilename("Default", "Global")));

	gSavedSettings.setString("VersionChannelName", LLVersionInfo::getChannel());

#if 0 //#ifndef	LL_RELEASE_FOR_DOWNLOAD
	// provide developer build only overrides for these control variables that are not
	// persisted to settings.xml
	LLControlVariable* c = gSavedSettings.getControl("ShowConsoleWindow");
	if (c)
	{
		c->setValue(true, false);
	}
	c = gSavedSettings.getControl("AllowMultipleViewers");
	if (c)
	{
		c->setValue(true, false);
	}
#endif

	// *FIX:Mani - Set default to disabling watchdog mainloop 
	// timeout for mac and linux. There is no call stack info 
	// on these platform to help debug.
#ifndef	LL_RELEASE_FOR_DOWNLOAD
	gSavedSettings.setBOOL("QAMode", TRUE );
	gSavedSettings.setBOOL("WatchdogEnabled", 0);
#endif

#ifndef LL_WINDOWS
	gSavedSettings.setBOOL("WatchdogEnabled", FALSE);
#endif

	// These are warnings that appear on the first experience of that condition.
	// They are already set in the settings_default.xml file, but still need to be added to LLFirstUse
	// for disable/reset ability
	LLFirstUse::addConfigVariable("FirstBalanceIncrease");
	LLFirstUse::addConfigVariable("FirstBalanceDecrease");
	LLFirstUse::addConfigVariable("FirstSit");
	LLFirstUse::addConfigVariable("FirstMap");
	LLFirstUse::addConfigVariable("FirstGoTo");
	LLFirstUse::addConfigVariable("FirstBuild");
	LLFirstUse::addConfigVariable("FirstLeftClickNoHit");
	LLFirstUse::addConfigVariable("FirstTeleport");
	LLFirstUse::addConfigVariable("FirstOverrideKeys");
	LLFirstUse::addConfigVariable("FirstAttach");
	LLFirstUse::addConfigVariable("FirstAO");
	LLFirstUse::addConfigVariable("FirstAppearance");
	LLFirstUse::addConfigVariable("FirstInventory");
	LLFirstUse::addConfigVariable("FirstSandbox");
	LLFirstUse::addConfigVariable("FirstFlexible");
	LLFirstUse::addConfigVariable("FirstDebugMenus");
	LLFirstUse::addConfigVariable("FirstStreamingMusic");
	LLFirstUse::addConfigVariable("FirstStreamingVideo");
	LLFirstUse::addConfigVariable("FirstSculptedPrim");
	LLFirstUse::addConfigVariable("FirstVoice");
	LLFirstUse::addConfigVariable("FirstMedia");
	
// [RLVa:KB] - Checked: RLVa-1.0.3a (2009-09-10) | Added: RLVa-1.0.3a
	//LLFirstUse::addConfigVariable(RLV_SETTING_FIRSTUSE_DETACH);
	//LLFirstUse::addConfigVariable(RLV_SETTING_FIRSTUSE_ENABLEWEAR);
	//LLFirstUse::addConfigVariable(RLV_SETTING_FIRSTUSE_FARTOUCH);
// [/RLVa:KB]

	// - read command line settings.
	LLControlGroupCLP clp;
	std::string	cmd_line_config	= gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
														  "cmd_line.xml");

	clp.configure(cmd_line_config, &gSavedSettings);

	if(!initParseCommandLine(clp))
	{
		LL_WARNS() << "Error parsing command line options.	Command	Line options ignored."  << LL_ENDL;
		
		LL_INFOS() << "Command	line usage:\n" << clp << LL_ENDL;

		std::ostringstream msg;
		msg << LLTrans::getString("MBCmdLineError") << clp.getErrorMessage();
		OSMessageBox(msg.str(),LLStringUtil::null,OSMB_OK);
		return false;
	}
	
	// - selectively apply settings 

	// <singu> Portability Mode!
	if (clp.hasOption("portable"))
	{
		const std::string log = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, LOG_FILE);
		LL_INFOS() << "Attempting to use portable settings and cache!" << LL_ENDL;
		gDirUtilp->makePortable();
		initLoggingInternal(); // Switch to portable log file
		LL_INFOS() << "Portable viewer configuration initialized!" << LL_ENDL;
		LLFile::remove(log);
		LL_INFOS() << "Cleaned up local log file to keep this computer untouched." << LL_ENDL;
	}
	// </singu>

	// If the user has specified a alternate settings file name.
	// Load	it now before loading the user_settings/settings.xml
	if(clp.hasOption("settings"))
	{
		std::string	user_settings_filename = 
			gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, 
										   clp.getOption("settings")[0]);		
		gSavedSettings.setString("ClientSettingsFile", user_settings_filename);
		LL_INFOS("Settings")	<< "Using command line specified settings filename: "
			<< user_settings_filename << LL_ENDL;
	}
	else
	{
		std::string channel(LLVersionInfo::getChannel());
		LLStringUtil::toLower(channel);
		switch (LLVersionInfo::getViewerMaturity())
		{
		default:
		case LLVersionInfo::TEST_VIEWER:
		case LLVersionInfo::PROJECT_VIEWER:
		case LLVersionInfo::ALPHA_VIEWER:
		case LLVersionInfo::BETA_VIEWER:
		{
			channel.erase(std::remove_if(channel.begin(), channel.end(), isspace), channel.end());
			break;
		}
		case LLVersionInfo::RELEASE_VIEWER:
			size_t pos = channel.find(' ');
			if (pos != channel.npos)
				channel.erase(pos, channel.npos);
			break;
		}
		//Mely : do not use channel here but Genesis
		//don't want to change the channel right now, cause I don't know the impact with Linden Labs
		//std::string settings_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
		//	llformat("settings_%s.xml", channel.c_str()));
		std::string settings_filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
			llformat("settings_%s.xml", "Genesis"));	
		gSavedSettings.setString("ClientSettingsFile", settings_filename);
		LL_INFOS() << "Using default settings filename from channel: "
			<< settings_filename << LL_ENDL;
	}

	// - load overrides from user_settings 
	loadSettingsFromDirectory(settings_w, "User");

	// - apply command line settings 
	clp.notify(); 

	// Register the core crash option as soon as we can
	// if we want gdb post-mortem on cores we need to be up and running
	// ASAP or we might miss init issue etc.
	if(clp.hasOption("disablecrashlogger"))
	{
		LL_WARNS() << "Crashes will be handled by system, stack trace logs and crash logger are both disabled" <<LL_ENDL;
		disableCrashlogger();
	}

	// Handle initialization from settings.
	// Start up the debugging console before handling other options.
	if (gSavedSettings.getBOOL("ShowConsoleWindow"))
	{
		initConsole();
	}

	if(clp.hasOption("help"))
	{
		std::ostringstream msg;
		msg << LLTrans::getString("MBCmdLineUsg") << "\n" << clp;
		LL_INFOS() << msg.str() << LL_ENDL;

		OSMessageBox(
			msg.str(),
			LLStringUtil::null,
			OSMB_OK);

		return false;
	}

	if(clp.hasOption("set"))
	{
		const LLCommandLineParser::token_vector_t& set_values = clp.getOption("set");
		if(0x1 & set_values.size())
		{
			LL_WARNS() << "Invalid '--set' parameter count." << LL_ENDL;
		}
		else
		{
			LLCommandLineParser::token_vector_t::const_iterator itr = set_values.begin();
			for(; itr != set_values.end(); ++itr)
			{
				const std::string& name = *itr;
				const std::string& value = *(++itr);
				LLControlVariable* c = settings[sGlobalSettingsName]->getControl(name);
				if(c)
				{
					c->setValue(value, false);
				}
				else
				{
					LL_WARNS() << "'--set' specified with unknown setting: '"
						<< name << "'." << LL_ENDL;
				}
			}
		}
	}

	if (!gHippoGridManager)
	{
		gHippoGridManager = new HippoGridManager();
		gHippoGridManager->init();
	}
	if (!gHippoLimits)
	{
		gHippoLimits = new HippoLimits();
	}

	// If we have specified crash on startup, set the global so we'll trigger the crash at the right time
	if(clp.hasOption("crashonstartup"))
	{
		gCrashOnStartup = TRUE;
	}
	
	if (clp.hasOption("debugsession"))
	{
		gDebugSession = TRUE;
		gDebugGL = TRUE;

		ll_init_fail_log(gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "test_failures.log"));
	}

	// Handle slurl use. NOTE: Don't let SL-55321 reappear.

	// *FIX: This init code should be made more robust to prevent 
	// the issue SL-55321 from returning. One thought is to allow 
	// only select options to be set from command line when a slurl 
	// is specified. More work on the settings system is needed to 
	// achieve this. For now...

	// *NOTE:Mani The command line parser parses tokens and is 
	// setup to bail after parsing the '--url' option or the 
	// first option specified without a '--option' flag (or
	// any other option that uses the 'last_option' setting - 
	// see LLControlGroupCLP::configure())

	// What can happen is that someone can use IE (or potentially 
	// other browsers) and do the rough equivalent of command 
	// injection and steal passwords. Phoenix. SL-55321
	if(clp.hasOption("url"))
	{
		LLStartUp::setStartSLURL(LLSLURL(clp.getOption("url")[0]));
		if(LLStartUp::getStartSLURL().getType() == LLSLURL::LOCATION) 
		{  
			gHippoGridManager->setCurrentGrid(LLStartUp::getStartSLURL().getGrid());
		}  
	}
	else if(clp.hasOption("slurl"))
	{
		LLSLURL start_slurl(clp.getOption("slurl")[0]);
		LLStartUp::setStartSLURL(start_slurl);
	}

	const LLControlVariable* skinfolder = gSavedSettings.getControl("SkinCurrent");
	if(skinfolder && LLStringUtil::null != skinfolder->getValue().asString())
	{   
		// Examining "Language" may not suffice -- see LLUI::getLanguage()
		// logic. Unfortunately LLUI::getLanguage() doesn't yet do us much
		// good because we haven't yet called LLUI::initClass().
		gDirUtilp->setSkinFolder(skinfolder->getValue().asString(),
								 gSavedSettings.getString("Language"));
	}

#if LL_DARWIN
	// Initialize apple menubar and various callbacks
	init_apple_menu(LLTrans::getString("APP_NAME").c_str());
#endif // LL_DARWIN

	// Display splash screen.  Must be after above check for previous
	// crash as this dialog is always frontmost.
	std::string splash_msg;
	LLStringUtil::format_map_t args;
	splash_msg = LLTrans::getString("StartupLoading", args);
	LLSplashScreen::show();
	LLSplashScreen::update(splash_msg);

	//LLVolumeMgr::initClass();
	LLVolumeMgr* volume_manager = new LLVolumeMgr();
	volume_manager->useMutex();	// LLApp and LLMutex magic must be manually enabled
	LLPrimitive::setVolumeManager(volume_manager);

	// Note: this is where we used to initialize gFeatureManagerp.

	gStartTime = totalTime();

	//
	// Set the name of the window
	//
	gWindowTitle = LLTrans::getString("APP_NAME") + llformat(" (%d) ", LLVersionInfo::getBuild())
#if LL_DEBUG
		+ "[DEBUG] "
#endif
		+ gArgs;
	LLStringUtil::truncate(gWindowTitle, 255);

	//RN: if we received a URL, hand it off to the existing instance.
	// don't call anotherInstanceRunning() when doing URL handoff, as
	// it relies on checking a marker file which will not work when running
	// out of different directories

	if (LLStartUp::getStartSLURL().isValid() &&
		(gSavedSettings.getBOOL("SLURLPassToOtherInstance")))
	{
		if (sendURLToOtherInstance(LLStartUp::getStartSLURL().getSLURLString()))
		{
			// successfully handed off URL to existing instance, exit
			return false;
		}
	}

	
	//
	// Check for another instance of the app running
	//
	if (mSecondInstance && !gSavedSettings.getBOOL("AllowMultipleViewers"))
	{
		OSMessageBox(
			LLTrans::getString("MBAlreadyRunning"),
			LLStringUtil::null,
			OSMB_OK);
		return false;
	}

	if (mSecondInstance)
	{
		// This is the second instance of SL. Turn off voice support,
		// but make sure the setting is *not* persisted.
		LLControlVariable* disable_voice = gSavedSettings.getControl("CmdLineDisableVoice");
		if(disable_voice && !gSavedSettings.getBOOL("VoiceMultiInstance"))
		{
			const BOOL DO_NOT_PERSIST = FALSE;
			disable_voice->setValue(LLSD(TRUE), DO_NOT_PERSIST);
		}
	}

	// need to do this here - need to have initialized global settings first
	std::string nextLoginLocation = gSavedSettings.getString( "NextLoginLocation" );
	if ( !nextLoginLocation.empty() )
	{
		LL_DEBUGS("AppInit")<<"set start from NextLoginLocation: "<<nextLoginLocation<<LL_ENDL;
		LLStartUp::setStartSLURL(LLSLURL(nextLoginLocation));
	}

	gLastRunVersion = gSavedSettings.getString("LastRunVersion");

	return true; // Config was successful.
}

bool LLAppViewer::initWindow()
{
	LL_INFOS("AppInit") << "Initializing window..." << LL_ENDL;

	// store setting in a global for easy access and modification
	gNoRender = gSavedSettings.getBOOL("DisableRendering");

	// Hide the splash screen
	LLSplashScreen::hide();

	// always start windowed
	BOOL ignorePixelDepth = gSavedSettings.getBOOL("IgnorePixelDepth");
	gViewerWindow = new LLViewerWindow(gWindowTitle, "Second Life",
		gSavedSettings.getS32("WindowX"), gSavedSettings.getS32("WindowY"),
		gSavedSettings.getS32("WindowWidth"), gSavedSettings.getS32("WindowHeight"),
		FALSE, ignorePixelDepth);
		
	if (gSavedSettings.getBOOL("FullScreen"))
	{
		gViewerWindow->toggleFullscreen(FALSE);
			// request to go full screen... which will be delayed until login
	}
	
	if (gSavedSettings.getBOOL("WindowMaximized"))
	{
		gViewerWindow->getWindow()->maximize();
		gViewerWindow->getWindow()->setNativeAspectRatio(gSavedSettings.getF32("FullScreenAspectRatio"));
	}

	if (!gNoRender)
	{
		//
		// Initialize GL stuff
		//

		// Set this flag in case we crash while initializing GL
		gSavedSettings.setBOOL("RenderInitError", TRUE);
		gSavedSettings.saveToFile( gSavedSettings.getString("ClientSettingsFile"), TRUE );

		gPipeline.init();
		LL_INFOS("AppInit") << "gPipeline Initialized" << LL_ENDL;
		stop_glerror();

		gGL.restoreVertexBuffers();
		gViewerWindow->initGLDefaults();

		gSavedSettings.setBOOL("RenderInitError", FALSE);
		gSavedSettings.saveToFile( gSavedSettings.getString("ClientSettingsFile"), TRUE );
	}

	//If we have a startup crash, it's usually near GL initialization, so simulate that.
	if(gCrashOnStartup)
	{
		LLAppViewer::instance()->forceErrorLLError();
	}

	LLUI::sWindow = gViewerWindow->getWindow();

	// Show watch cursor
	gViewerWindow->setCursor(UI_CURSOR_WAIT);

	// Finish view initialization
	gViewerWindow->initBase();

	// show viewer window
	//gViewerWindow->getWindow()->show();

	LL_INFOS("AppInit") << "Window initialization done." << LL_ENDL;

	return true;
}

void LLAppViewer::writeDebugInfo(bool isStatic)
{
#if !defined(USE_CRASHPAD)
	//Try to do the minimum when writing data during a crash.
	std::string* debug_filename;
	debug_filename = ( isStatic
		? getStaticDebugFile()
		: getDynamicDebugFile() );
	
    LL_INFOS() << "Writing debug file " << *debug_filename << LL_ENDL;
    llofstream out_file(debug_filename->c_str());
	
	isStatic ?  LLSDSerialize::toPrettyXML(gDebugInfo, out_file)
			 :  LLSDSerialize::toPrettyXML(gDebugInfo["Dynamic"], out_file);
#else
	SET_CRASHPAD_ANNOTATION_VALUE(fatal_message, gDebugInfo["FatalMessage"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(grid_name, gDebugInfo["GridName"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(region_name, gDebugInfo["CurrentRegion"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(cpu_string, gDebugInfo["CPUInfo"]["CPUString"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(gpu_string, gDebugInfo["GraphicsCard"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(gl_version, gDebugInfo["GLInfo"]["GLVersion"].asString());
	SET_CRASHPAD_ANNOTATION_VALUE(session_duration, fmt::to_string(LLFrameTimer::getElapsedSeconds()));
	SET_CRASHPAD_ANNOTATION_VALUE(memory_alloc, fmt::to_string((LLMemory::getCurrentRSS() >> 10) / 1000.f));
	SET_CRASHPAD_ANNOTATION_VALUE(memory_sys, fmt::to_string(gDebugInfo["RAMInfo"]["Physical"].asInteger() * 0.001f));
#endif
}

void LLAppViewer::cleanupSavedSettings()
{
	gSavedSettings.setBOOL("MouseSun", FALSE);

	gSavedSettings.setBOOL("UseEnergy", TRUE);				// force toggle to turn off, since sends message to simulator

	gSavedSettings.setBOOL("DebugWindowProc", gDebugWindowProc);
		
	gSavedSettings.setBOOL("ShowObjectUpdates", gShowObjectUpdates);
	
	if (!gNoRender && gDebugView)
	{
		gSavedSettings.setBOOL("ShowDebugConsole", gDebugView->mDebugConsolep->getVisible());
	}

	// save window position if not fullscreen
	// as we don't track it in callbacks
	BOOL fullscreen = gViewerWindow->getWindow()->getFullscreen();
	BOOL maximized = gViewerWindow->getWindow()->getMaximized();
	if (!fullscreen && !maximized)
	{
		LLCoordScreen window_pos;

		if (gViewerWindow->getWindow()->getPosition(&window_pos))
		{
			gSavedSettings.setS32("WindowX", window_pos.mX);
			gSavedSettings.setS32("WindowY", window_pos.mY);
		}
	}

	gSavedSettings.setF32("MapScale", LLWorldMapView::sMapScale );
	gSavedSettings.setBOOL("ShowHoverTips", LLHoverView::sShowHoverTips);

	// Some things are cached in LLAgent.
	if (gAgentCamera.isInitialized())
	{
		gSavedSettings.setF32("RenderFarClip", gAgentCamera.mDrawDistance);
	}

	LLSpeakerVolumeStorage::deleteSingleton();
}

void LLAppViewer::removeCacheFiles(const std::string& file_mask)
{
	gDirUtilp->deleteFilesInDir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE, ""), file_mask);
}

void LLAppViewer::writeSystemInfo()
{

	if (! gDebugInfo.has("Dynamic") )
		gDebugInfo["Dynamic"] = LLSD::emptyMap();
	
	gDebugInfo["SLLog"] = LLError::logFileName();

	gDebugInfo["ClientInfo"]["Name"] = LLVersionInfo::getChannel();
	gDebugInfo["ClientInfo"]["MajorVersion"] = LLVersionInfo::getMajor();
	gDebugInfo["ClientInfo"]["MinorVersion"] = LLVersionInfo::getMinor();
	gDebugInfo["ClientInfo"]["PatchVersion"] = LLVersionInfo::getPatch();
	gDebugInfo["ClientInfo"]["BuildVersion"] = LLVersionInfo::getBuild();
	gDebugInfo["ClientInfo"]["AddressSize"] =
#if defined(_WIN64) || defined(__x86_64__)
		"64";
#else
		"32";
#endif

	gDebugInfo["CAFilename"] = gDirUtilp->getCAFile();

	gDebugInfo["CPUInfo"]["CPUString"] = gSysCPU.getCPUString();
	gDebugInfo["CPUInfo"]["CPUFamily"] = gSysCPU.getFamily();
	gDebugInfo["CPUInfo"]["CPUMhz"] = gSysCPU.getMHz();
	gDebugInfo["CPUInfo"]["CPUAltivec"] = gSysCPU.hasAltivec();
	gDebugInfo["CPUInfo"]["CPUSSE"] = gSysCPU.hasSSE();
	gDebugInfo["CPUInfo"]["CPUSSE2"] = gSysCPU.hasSSE2();
	
	gDebugInfo["RAMInfo"]["Physical"] = LLSD::Integer(gSysMemory.getPhysicalMemoryKB().value());
	gDebugInfo["RAMInfo"]["Allocated"] = LLSD::Integer(gMemoryAllocated.valueInUnits<LLUnits::Kilobytes>());
	gDebugInfo["OSInfo"] = getOSInfo().getOSStringSimple();

	// The user is not logged on yet, but record the current grid choice login url
	// which may have been the intended grid.
	gDebugInfo["GridName"] = LLViewerLogin::getInstance()->getGridLabel();

	// *FIX:Mani - move this down in llappviewerwin32
#ifdef LL_WINDOWS
	DWORD thread_id = GetCurrentThreadId();
	gDebugInfo["MainloopThreadID"] = (S32)thread_id;
#endif

	// "CrashNotHandled" is set here, while things are running well,
	// in case of a freeze. If there is a freeze, the crash logger will be launched
	// and can read this value from the debug_info.log.
	// If the crash is handled by LLAppViewer::handleViewerCrash, ie not a freeze,
	// then the value of "CrashNotHandled" will be set to true.
	gDebugInfo["CrashNotHandled"] = (LLSD::Boolean)true;

	// Insert crash host url (url to post crash log to) if configured. This insures
	// that the crash report will go to the proper location in the case of a 
	// prior freeze.
	std::string crashHostUrl = gSavedSettings.get<std::string>("CrashHostUrl");
	if (!crashHostUrl.empty())
	{
		gDebugInfo["CrashHostUrl"] = crashHostUrl;
	}
	
	// Dump some debugging info
	LL_INFOS("SystemInfo") << "Application: " << LLTrans::getString("APP_NAME") << LL_ENDL;
	LL_INFOS("SystemInfo") << "Version: " << LLVersionInfo::getChannelAndVersion() << LL_ENDL;

	// Dump the local time and time zone
	time_t now;
	time(&now);
	char tbuffer[256];		/* Flawfinder: ignore */
	strftime(tbuffer, 256, "%Y-%m-%dT%H:%M:%S %Z", localtime(&now));
	LL_INFOS("SystemInfo") << "Local time: " << tbuffer << LL_ENDL;

	// query some system information
	LL_INFOS("SystemInfo") << "CPU info:\n" << gSysCPU << LL_ENDL;
	LL_INFOS("SystemInfo") << "Memory info:\n" << gSysMemory << LL_ENDL;
	LL_INFOS("SystemInfo") << "OS: " << getOSInfo().getOSStringSimple() << LL_ENDL;
	LL_INFOS("SystemInfo") << "OS info: " << getOSInfo() << LL_ENDL;

	LL_INFOS("SystemInfo") << "Timers: " << LLFastTimer::sClockType << LL_ENDL;

	writeDebugInfo(); // Save out debug_info.log early, in case of crash.
}

void LLAppViewer::handleViewerCrash()
{
	LL_INFOS("CRASHREPORT") << "Handle viewer crash entry." << LL_ENDL;

	LL_INFOS("CRASHREPORT") << "Last render pool type: " << LLPipeline::sCurRenderPoolType << LL_ENDL ;

	LLMemory::logMemoryInfo(true) ;

	//print out recorded call stacks if there are any.
	LLError::LLCallStacks::print();

	LLAppViewer* pApp = LLAppViewer::instance();
	if (pApp->beingDebugged())
	{
		// This will drop us into the debugger.
		abort();
	}

	if (LLApp::isCrashloggerDisabled())
	{
		abort();
	}

	// Returns whether a dialog was shown.
	// Only do the logic in here once
	if (pApp->mReportedCrash)
	{
		return;
	}
	pApp->mReportedCrash = TRUE;

	// Insert crash host url (url to post crash log to) if configured.
	std::string crashHostUrl = gSavedSettings.get<std::string>("CrashHostUrl");
	if (!crashHostUrl.empty())
	{
		gDebugInfo["Dynamic"]["CrashHostUrl"] = crashHostUrl;
	}
	
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if ( parcel && parcel->getMusicURL()[0])
	{
		gDebugInfo["Dynamic"]["ParcelMusicURL"] = parcel->getMusicURL();
	}	
	if ( parcel && parcel->getMediaURL()[0])
	{
		gDebugInfo["Dynamic"]["ParcelMediaURL"] = parcel->getMediaURL();
	}

	gDebugInfo["SettingsFilename"] = gSavedSettings.getString("ClientSettingsFile");
	gDebugInfo["CAFilename"] = gDirUtilp->getCAFile();
	gDebugInfo["ViewerExePath"] = gDirUtilp->getExecutablePathAndName();
	gDebugInfo["CurrentPath"] = gDirUtilp->getCurPath();
	gDebugInfo["Dynamic"]["SessionLength"] = F32(LLFrameTimer::getElapsedSeconds());
	gDebugInfo["Dynamic"]["RAMInfo"]["Allocated"] = (LLSD::Integer) LLMemory::getCurrentRSS() >> 10;
	gDebugInfo["StartupState"] = LLStartUp::getStartupStateString();
	gDebugInfo["FirstLogin"] = (LLSD::Boolean) gAgent.isFirstLogin();
	gDebugInfo["FirstRunThisInstall"] = gSavedSettings.getBOOL("FirstRunThisInstall");

	if(gLogoutInProgress)
	{
		gDebugInfo["Dynamic"]["LastExecEvent"] = LAST_EXEC_LOGOUT_CRASH;
	}
	else
	{
		gDebugInfo["Dynamic"]["LastExecEvent"] = gLLErrorActivated ? LAST_EXEC_LLERROR_CRASH : LAST_EXEC_OTHER_CRASH;
	}

	if(gAgent.getRegion())
	{
		gDebugInfo["Dynamic"]["CurrentSimHost"] = gAgent.getRegionHost().getHostName();
		gDebugInfo["Dynamic"]["CurrentRegion"] = gAgent.getRegion()->getName();
		
		const LLVector3& loc = gAgent.getPositionAgent();
		gDebugInfo["Dynamic"]["CurrentLocationX"] = loc.mV[0];
		gDebugInfo["Dynamic"]["CurrentLocationY"] = loc.mV[1];
		gDebugInfo["Dynamic"]["CurrentLocationZ"] = loc.mV[2];
	}

	if(LLAppViewer::instance()->mMainloopTimeout)
	{
		gDebugInfo["Dynamic"]["MainloopTimeoutState"] = LLAppViewer::instance()->mMainloopTimeout->getState();
	}
	
	// The crash is being handled here so set this value to false.
	// Otherwise the crash logger will think this crash was a freeze.
	gDebugInfo["Dynamic"]["CrashNotHandled"] = (LLSD::Boolean)false;
	
	//Write out the crash status file
	//Use marker file style setup, as that's the simplest, especially since
	//we're already in a crash situation	
	if (gDirUtilp)
	{
		std::string crash_marker_file_name = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,
																			gLLErrorActivated
																			? LLERROR_MARKER_FILE_NAME
																			: ERROR_MARKER_FILE_NAME);
		LLAPRFile crash_marker_file ;
		crash_marker_file.open(crash_marker_file_name, LL_APR_WB);
		if (crash_marker_file.getFileHandle())
		{
			LL_INFOS("MarkerFile") << "Created crash marker file " << crash_marker_file_name << LL_ENDL;
			recordMarkerVersion(crash_marker_file);
		}
		else
		{
			LL_WARNS("MarkerFile") << "Cannot create error marker file " << crash_marker_file_name << LL_ENDL;
		}		
	}
	else
	{
		LL_WARNS("MarkerFile") << "No gDirUtilp with which to create error marker file name" << LL_ENDL;
	}
    LL_WARNS("CRASHREPORT") << "no minidump file? ah yeah, boi!" << LL_ENDL;

	gDebugInfo["Dynamic"]["CrashType"]="crash";
	
	if (gMessageSystem && gDirUtilp)
	{
		std::string filename;
		filename = gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "stats.log");
        LL_DEBUGS("CRASHREPORT") << "recording stats " << filename << LL_ENDL;
		llofstream file(filename.c_str(), std::ios_base::binary);
		if(file.good())
		{
			gMessageSystem->summarizeLogs(file);
			file.close();
		}
        else
        {
            LL_WARNS("CRASHREPORT") << "problem recording stats" << LL_ENDL;
        }
	}

	if (gMessageSystem)
	{
		gMessageSystem->getCircuitInfo(gDebugInfo["CircuitInfo"]);
		gMessageSystem->stopLogging();
	}

	if (LLWorld::instanceExists()) LLWorld::getInstance()->getInfo(gDebugInfo["Dynamic"]);

	// Close the debug file
	pApp->writeDebugInfo(false);  //false answers the isStatic question with the least overhead.
}

// static
void LLAppViewer::recordMarkerVersion(LLAPRFile& marker_file)
{
	std::string marker_version(LLVersionInfo::getChannelAndVersion());
	if ( marker_version.length() > MAX_MARKER_LENGTH )
	{
		LL_WARNS_ONCE("MarkerFile") << "Version length ("<< marker_version.length()<< ")"
									<< " greater than maximum (" << MAX_MARKER_LENGTH << ")"
									<< ": marker matching may be incorrect"
									<< LL_ENDL;
	}

	// record the viewer version in the marker file
	marker_file.write(marker_version.data(), marker_version.length());
}

bool LLAppViewer::markerIsSameVersion(const std::string& marker_name) const
{
	bool sameVersion = false;

	std::string my_version(LLVersionInfo::getChannelAndVersion());
	char marker_version[MAX_MARKER_LENGTH];

    LLAPRFile marker_file;
	marker_file.open(marker_name, LL_APR_RB);
	if (marker_file.getFileHandle())
	{
		S32 marker_version_length = marker_file.read(marker_version, sizeof(marker_version));
		std::string marker_string(marker_version, marker_version_length);
		if ( 0 == my_version.compare( 0, my_version.length(), marker_version, 0, marker_version_length ) )
		{
			sameVersion = true;
		}
		LL_DEBUGS("MarkerFile") << "Compare markers for '" << marker_name << "': "
								<< "\n   mine '" << my_version    << "'"
								<< "\n marker '" << marker_string << "'"
								<< "\n " << ( sameVersion ? "same" : "different" ) << " version"
								<< LL_ENDL;
		marker_file.close();
	}
	return sameVersion;
}

void LLAppViewer::processMarkerFiles()
{
	//We've got 4 things to test for here
	// - Other Process Running (SecondLife.exec_marker present, locked)
	// - Freeze (SecondLife.exec_marker present, not locked)
	// - LLError Crash (SecondLife.llerror_marker present)
	// - Other Crash (SecondLife.error_marker present)
	// These checks should also remove these files for the last 2 cases if they currently exist

	bool marker_is_same_version = true;
	// first, look for the marker created at startup and deleted on a clean exit
	mMarkerFileName = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,MARKER_FILE_NAME);
	if (LLAPRFile::isExist(mMarkerFileName, LL_APR_RB))
	{
		// File exists...
		// first, read it to see if it was created by the same version (we need this later)
		marker_is_same_version = markerIsSameVersion(mMarkerFileName);

		// now test to see if this file is locked by a running process (try to open for write)
		LL_DEBUGS("MarkerFile") << "Checking exec marker file for lock..." << LL_ENDL;
		mMarkerFile.open(mMarkerFileName, LL_APR_WB);
		apr_file_t* fMarker = mMarkerFile.getFileHandle() ;
		if (!fMarker)
		{
			LL_INFOS("MarkerFile") << "Exec marker file open failed - assume it is locked." << LL_ENDL;
			mSecondInstance = true; // lock means that instance is running.
		}
		else
		{
			// We were able to open it, now try to lock it ourselves...
			if (apr_file_lock(fMarker, APR_FLOCK_NONBLOCK | APR_FLOCK_EXCLUSIVE) != APR_SUCCESS)
			{
				LL_WARNS_ONCE("MarkerFile") << "Locking exec marker failed." << LL_ENDL;
				mSecondInstance = true; // lost a race? be conservative
			}
			else
			{
				// No other instances; we've locked this file now, so record our version; delete on quit.
				recordMarkerVersion(mMarkerFile);
				LL_DEBUGS("MarkerFile") << "Exec marker file existed but was not locked; rewritten." << LL_ENDL;
			}
		}

		if (mSecondInstance)
		{
			LL_INFOS("MarkerFile") << "Exec marker '"<< mMarkerFileName << "' owned by another instance" << LL_ENDL;
		}
		else if (marker_is_same_version)
		{
			// the file existed, is ours, and matched our version, so we can report on what it says
			LL_INFOS("MarkerFile") << "Exec marker '"<< mMarkerFileName << "' found; last exec FROZE" << LL_ENDL;
			gLastExecEvent = LAST_EXEC_FROZE;

		}
		else
		{
			LL_INFOS("MarkerFile") << "Exec marker '"<< mMarkerFileName << "' found, but versions did not match" << LL_ENDL;
		}
	}
	else // marker did not exist... last exec (if any) did not freeze
	{
		// Create the marker file for this execution & lock it; it will be deleted on a clean exit
		apr_status_t s;
		s = mMarkerFile.open(mMarkerFileName, LL_APR_WB, TRUE);

		if (s == APR_SUCCESS && mMarkerFile.getFileHandle())
		{
			LL_DEBUGS("MarkerFile") << "Exec marker file '"<< mMarkerFileName << "' created." << LL_ENDL;
			if (APR_SUCCESS == apr_file_lock(mMarkerFile.getFileHandle(), APR_FLOCK_NONBLOCK | APR_FLOCK_EXCLUSIVE))
			{
				recordMarkerVersion(mMarkerFile);
				LL_DEBUGS("MarkerFile") << "Exec marker file locked." << LL_ENDL;
			}
			else
			{
				LL_WARNS("MarkerFile") << "Exec marker file cannot be locked." << LL_ENDL;
			}
		}
		else
		{
			LL_WARNS("MarkerFile") << "Failed to create exec marker file '"<< mMarkerFileName << "'." << LL_ENDL;
		}
	}

	// now check for cases in which the exec marker may have been cleaned up by crash handlers

	// check for any last exec event report based on whether or not it happened during logout
	// (the logout marker is created when logout begins)
	std::string logout_marker_file =  gDirUtilp->getExpandedFilename(LL_PATH_LOGS, LOGOUT_MARKER_FILE_NAME);
	if(LLAPRFile::isExist(logout_marker_file, LL_APR_RB))
	{
		if (markerIsSameVersion(logout_marker_file))
		{
			gLastExecEvent = LAST_EXEC_LOGOUT_FROZE;
			LL_INFOS("MarkerFile") << "Logout crash marker '"<< logout_marker_file << "', changing LastExecEvent to LOGOUT_FROZE" << LL_ENDL;
		}
		else
		{
			LL_INFOS("MarkerFile") << "Logout crash marker '"<< logout_marker_file << "' found, but versions did not match" << LL_ENDL;
		}
		LLAPRFile::remove(logout_marker_file);
	}
	// further refine based on whether or not a marker created during an llerr crash is found
	std::string llerror_marker_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, LLERROR_MARKER_FILE_NAME);
	if(LLAPRFile::isExist(llerror_marker_file, LL_APR_RB))
	{
		if (markerIsSameVersion(llerror_marker_file))
		{
			if ( gLastExecEvent == LAST_EXEC_LOGOUT_FROZE )
			{
				gLastExecEvent = LAST_EXEC_LOGOUT_CRASH;
				LL_INFOS("MarkerFile") << "LLError marker '"<< llerror_marker_file << "' crashed, setting LastExecEvent to LOGOUT_CRASH" << LL_ENDL;
			}
			else
			{
				gLastExecEvent = LAST_EXEC_LLERROR_CRASH;
				LL_INFOS("MarkerFile") << "LLError marker '"<< llerror_marker_file << "' crashed, setting LastExecEvent to LLERROR_CRASH" << LL_ENDL;
			}
		}
		else
		{
			LL_INFOS("MarkerFile") << "LLError marker '"<< llerror_marker_file << "' found, but versions did not match" << LL_ENDL;
		}
		LLAPRFile::remove(llerror_marker_file);
	}
	// and last refine based on whether or not a marker created during a non-llerr crash is found
	std::string error_marker_file = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, ERROR_MARKER_FILE_NAME);
	if(LLAPRFile::isExist(error_marker_file, LL_APR_RB))
	{
		if (markerIsSameVersion(error_marker_file))
		{
			if (gLastExecEvent == LAST_EXEC_LOGOUT_FROZE)
			{
				gLastExecEvent = LAST_EXEC_LOGOUT_CRASH;
				LL_INFOS("MarkerFile") << "Error marker '"<< error_marker_file << "' crashed, setting LastExecEvent to LOGOUT_CRASH" << LL_ENDL;
			}
			else
			{
				gLastExecEvent = LAST_EXEC_OTHER_CRASH;
				LL_INFOS("MarkerFile") << "Error marker '"<< error_marker_file << "' crashed, setting LastExecEvent to " << gLastExecEvent << LL_ENDL;
			}
		}
		else
		{
			LL_INFOS("MarkerFile") << "Error marker '"<< error_marker_file << "' marker found, but versions did not match" << LL_ENDL;
		}
		LLAPRFile::remove(error_marker_file);
	}
}

void LLAppViewer::removeMarkerFiles()
{
	if (!mSecondInstance)
	{
		if (mMarkerFile.getFileHandle())
		{
			mMarkerFile.close() ;
			LLAPRFile::remove( mMarkerFileName );
			LL_DEBUGS("MarkerFile") << "removed exec marker '"<<mMarkerFileName<<"'"<< LL_ENDL;
		}
		else
		{
			LL_WARNS("MarkerFile") << "marker '"<<mMarkerFileName<<"' not open"<< LL_ENDL;
 		}

		if (mLogoutMarkerFile.getFileHandle())
		{
			mLogoutMarkerFile.close();
			LLAPRFile::remove( mLogoutMarkerFileName );
			LL_DEBUGS("MarkerFile") << "removed logout marker '"<<mLogoutMarkerFileName<<"'"<< LL_ENDL;
		}
		else
		{
			LL_WARNS("MarkerFile") << "logout marker '"<<mLogoutMarkerFileName<<"' not open"<< LL_ENDL;
		}
	}
	else
	{
		LL_WARNS("MarkerFile") << "leaving markers because this is a second instance" << LL_ENDL;
	}
}

void LLAppViewer::removeDumpDir()
{
#if !defined(USE_CRASHPAD)
	//Call this routine only on clean exit.  Crash reporter will clean up
	//its locking table for us.
	std::string dump_dir = gDirUtilp->getExpandedFilename(LL_PATH_DUMP, "");
	gDirUtilp->deleteDirAndContents(dump_dir);
#endif
}

void LLAppViewer::forceQuit()
{ 
	LLApp::setQuitting(); 
}

//TODO: remove
void LLAppViewer::fastQuit(S32 error_code)
{
	// finish pending transfers
	flushVFSIO();
	// let sim know we're logging out
	sendLogoutRequest();
	// flush network buffers by shutting down messaging system
	end_messaging_system();
	// figure out the error code
	S32 final_error_code = error_code ? error_code : (S32)isError();
	// this isn't a crash	
	removeMarkerFiles();
	// get outta here
	_exit(final_error_code);	
}

void LLAppViewer::requestQuit()
{
	LL_INFOS() << "requestQuit" << LL_ENDL;

	LLViewerRegion* region = gAgent.getRegion();
	
	if( (LLStartUp::getStartupState() < STATE_STARTED) || !region )
	{
		// If we have a region, make some attempt to send a logout request first.
		// This prevents the halfway-logged-in avatar from hanging around inworld for a couple minutes.
		if(region)
		{
			sendLogoutRequest();
		}
		
		// Quit immediately
		forceQuit();
		return;
	}

	// Try to send metrics back to the grid
	metricsSend(!gDisconnected);

	// Try to send last batch of avatar rez metrics.
	if (!gDisconnected && isAgentAvatarValid())
	{
		gAgentAvatarp->updateAvatarRezMetrics(true); // force a last packet to be sent.
	}
	
	LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral*)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());
	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();
	effectp->markDead() ;//remove it.

	// Attempt to close all floaters that might be
	// editing things.
	if (gFloaterView)
	{
		// application is quitting
		gFloaterView->closeAllChildren(true);
	}

	send_stats();

	gLogoutTimer.reset();
	mQuitRequested = true;
}

static bool finish_quit(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLAppViewer::instance()->requestQuit();
	}
	return false;
}
static LLNotificationFunctorRegistration finish_quit_reg("ConfirmQuit", finish_quit);

void LLAppViewer::userQuit()
{
	if (gDisconnected || gViewerWindow->getProgressView()->getVisible())
	{
		requestQuit();
	}
	else
	{
		LLNotificationsUtil::add("ConfirmQuit");
	}
}

static bool finish_early_exit(const LLSD& notification, const LLSD& response)
{
	LLAppViewer::instance()->forceQuit();
	return false;
}

void LLAppViewer::earlyExit(const std::string& name, const LLSD& substitutions)
{
	LL_WARNS() << "app_early_exit: " << name << LL_ENDL;
	gDoDisconnect = TRUE;
	LLNotificationsUtil::add(name, substitutions, LLSD(), finish_early_exit);
}


void LLAppViewer::abortQuit()
{
	LL_INFOS() << "abortQuit()" << LL_ENDL;
	mQuitRequested = false;
}

void LLAppViewer::migrateCacheDirectory()
{
#if LL_WINDOWS || LL_DARWIN
	// NOTE: (Nyx) as of 1.21, cache for mac is moving to /library/caches/SecondLife from
	// /library/application support/SecondLife/cache This should clear/delete the old dir.

	// As of 1.23 the Windows cache moved from
	//   C:\Documents and Settings\James\Application Support\SecondLife\cache
	// to
	//   C:\Documents and Settings\James\Local Settings\Application Support\SecondLife
	//
	// The Windows Vista equivalent is from
	//   C:\Users\James\AppData\Roaming\SecondLife\cache
	// to
	//   C:\Users\James\AppData\Local\SecondLife
	//
	// Note the absence of \cache on the second path.  James.

	// Only do this once per fresh install of this version.
	if (gSavedSettings.getBOOL("MigrateCacheDirectory"))
	{
		gSavedSettings.setBOOL("MigrateCacheDirectory", FALSE);

		std::string old_cache_dir = gDirUtilp->add(gDirUtilp->getOSUserAppDir(), "cache");
		std::string new_cache_dir = gDirUtilp->getCacheDir(true);

		if (gDirUtilp->fileExists(old_cache_dir))
		{
			LL_INFOS() << "Migrating cache from " << old_cache_dir << " to " << new_cache_dir << LL_ENDL;

			// Migrate inventory cache to avoid pain to inventory database after mass update
			S32 file_count = 0;
			std::string file_name;
			std::string mask = "*.*";

			LLDirIterator iter(old_cache_dir, mask);
			while (iter.next(file_name))
			{
				if (file_name == "." || file_name == "..") continue;
				std::string source_path = gDirUtilp->add(old_cache_dir, file_name);
				std::string dest_path = gDirUtilp->add(new_cache_dir, file_name);
				if (!LLFile::rename(source_path, dest_path))
				{
					file_count++;
				}
			}
			LL_INFOS() << "Moved " << file_count << " files" << LL_ENDL;

			// Nuke the old cache
			gDirUtilp->setCacheDir(old_cache_dir);
			purgeCache();
			gDirUtilp->setCacheDir(new_cache_dir);

#if LL_DARWIN
			// Clean up Mac files not deleted by removing *.*
			std::string ds_store = old_cache_dir + "/.DS_Store";
			if (gDirUtilp->fileExists(ds_store))
			{
				LLFile::remove(ds_store);
			}
#endif
			if (LLFile::rmdir(old_cache_dir) != 0)
			{
				LL_WARNS() << "could not delete old cache directory " << old_cache_dir << LL_ENDL;
			}
		}
	}
#endif // LL_WINDOWS || LL_DARWIN
}

void dumpVFSCaches()
{
	LL_INFOS() << "======= Static VFS ========" << LL_ENDL;
	gStaticVFS->listFiles();
#if LL_WINDOWS
	LL_INFOS() << "======= Dumping static VFS to StaticVFSDump ========" << LL_ENDL;
	WCHAR w_str[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, w_str);
	S32 res = LLFile::mkdir("StaticVFSDump");
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			LL_WARNS() << "Couldn't create dir StaticVFSDump" << LL_ENDL;
		}
	}
	SetCurrentDirectory(utf8str_to_utf16str("StaticVFSDump").c_str());
	gStaticVFS->dumpFiles();
	SetCurrentDirectory(w_str);
#endif

	LL_INFOS() << "========= Dynamic VFS ====" << LL_ENDL;
	gVFS->listFiles();
#if LL_WINDOWS
	LL_INFOS() << "========= Dumping dynamic VFS to VFSDump ====" << LL_ENDL;
	res = LLFile::mkdir("VFSDump");
	if (res == -1)
	{
		if (errno != EEXIST)
		{
			LL_WARNS() << "Couldn't create dir VFSDump" << LL_ENDL;
		}
	}
	SetCurrentDirectory(utf8str_to_utf16str("VFSDump").c_str());
	gVFS->dumpFiles();
	SetCurrentDirectory(w_str);
#endif
}

//static
U32 LLAppViewer::getTextureCacheVersion()
{
	//viewer texture cache version, change if the texture cache format changes.
	static const U32 TEXTURE_CACHE_VERSION = 8;

	return TEXTURE_CACHE_VERSION ;
}

//static
U32 LLAppViewer::getObjectCacheVersion() 
{
	// Viewer object cache version, change if object update
	// format changes. JC
	const U32 INDRA_OBJECT_CACHE_VERSION = 14;

	return INDRA_OBJECT_CACHE_VERSION;
}

bool LLAppViewer::initCache()
{
	mPurgeCache = false;
	BOOL read_only = mSecondInstance ? TRUE : FALSE;
	LLAppViewer::getTextureCache()->setReadOnly(read_only) ;
	LLVOCache::getInstance()->setReadOnly(read_only);

	bool texture_cache_mismatch = false;
	if (gSavedSettings.getS32("LocalCacheVersion") != LLAppViewer::getTextureCacheVersion())
	{
		texture_cache_mismatch = true;
		if(!read_only)
		{
			gSavedSettings.setS32("LocalCacheVersion", LLAppViewer::getTextureCacheVersion());
		}
	}

	if (!read_only)
	{
		// Purge cache if user requested it
		if (gSavedSettings.getBOOL("PurgeCacheOnStartup") ||
			gSavedSettings.getBOOL("PurgeCacheOnNextStartup"))
		{
            LL_INFOS("AppCache") << "Startup cache purge requested: " << (gSavedSettings.getBOOL("PurgeCacheOnStartup") ? "ALWAYS" : "ONCE") << LL_ENDL;
			gSavedSettings.setBOOL("PurgeCacheOnNextStartup", false);
			mPurgeCache = true;
			// STORM-1141 force purgeAllTextures to get called to prevent a crash here. -brad
			// Bullshit, mPurgeCache already causes the same and doing it twice just leads to loads of warnings. --Aleric
			//texture_cache_mismatch = true;
		}
	
		// We have moved the location of the cache directory over time.
		migrateCacheDirectory();

		// Setup and verify the cache location
		std::string cache_location = gSavedSettings.getString("CacheLocation");
		std::string new_cache_location = gSavedSettings.getString("NewCacheLocation");
		if (new_cache_location != cache_location)
		{
            LL_INFOS("AppCache") << "Cache location changed, cache needs purging" << LL_ENDL;
			gDirUtilp->setCacheDir(gSavedSettings.getString("CacheLocation"));
			purgeCache(); // purge old cache
			gSavedSettings.setString("CacheLocation", new_cache_location);
		}
	}
	
	if (!gDirUtilp->setCacheDir(gSavedSettings.getString("CacheLocation")))
	{
		LL_WARNS("AppCache") << "Unable to set cache location" << LL_ENDL;
		gSavedSettings.setString("CacheLocation", "");
		// Keep NewCacheLocation equal to CacheLocation so we won't try to erase the cache the next time the viewer is run.
		gSavedSettings.setString("NewCacheLocation", "");
	}
	
	if (mPurgeCache && !read_only)
	{
		LLSplashScreen::update(LLTrans::getString("StartupClearingCache"));
		purgeCache();
		// <edit>
		texture_cache_mismatch = false;
		// </edit>
	}

    {
        std::random_device rnddev;
        std::mt19937 rng(rnddev());
        std::uniform_int_distribution<> dist(0, 4);

        LLSplashScreen::update(LLTrans::getString(
            llformat("StartupInitializingTextureCache%d", dist(rng))));
    }

	// Init the texture cache
	// Allocate 80% of the cache size for textures
	const U64Bytes MIN_CACHE_SIZE = U32Megabytes(64);
	const U64Bytes MAX_CACHE_SIZE = U32Megabytes(9984);
	const U64Bytes MAX_VFS_SIZE = U32Gigabytes(1);

	U64Bytes cache_size = U32Megabytes(gSavedSettings.getU32("CacheSize"));
	cache_size = llclamp(cache_size, MIN_CACHE_SIZE, MAX_CACHE_SIZE);

	U64Bytes texture_cache_size = ((cache_size * 8) / 10);
	U64Bytes vfs_size = U64Bytes(cache_size) - U64Bytes(texture_cache_size);

	if (vfs_size > MAX_VFS_SIZE)
	{
		// Give the texture cache more space, since the VFS can't be bigger than 1GB.
		// This happens when the user's CacheSize setting is greater than 5GB.
		vfs_size = MAX_VFS_SIZE;
		texture_cache_size = cache_size - MAX_VFS_SIZE;
	}

	U64Bytes extra(LLAppViewer::getTextureCache()->initCache(LL_PATH_CACHE, texture_cache_size, texture_cache_mismatch));
	texture_cache_size -= extra;

	LLVOCache::getInstance()->initCache(LL_PATH_CACHE, gSavedSettings.getU32("CacheNumberOfRegionsForObjects"), getObjectCacheVersion()) ;

	LLSplashScreen::update(LLTrans::getString("StartupInitializingVFS"));
	
	// Init the VFS
	vfs_size = llmin(vfs_size + extra, MAX_VFS_SIZE);
	vfs_size = U32Megabytes(vfs_size + U32Bytes(1048575)); // make sure it is MB aligned
	U32Megabytes old_vfs_size(gSavedSettings.getU32("VFSOldSize"));
	bool resize_vfs = (vfs_size != old_vfs_size);
	if (resize_vfs)
	{
		gSavedSettings.setU32("VFSOldSize", U32Megabytes(vfs_size));
	}
	LL_INFOS("AppCache") << "VFS CACHE SIZE: " << U32Megabytes(vfs_size) << LL_ENDL;
	
	// This has to happen BEFORE starting the vfs
	// time_t	ltime;
	srand(time(nullptr));		// Flawfinder: ignore
	U32 old_salt = gSavedSettings.getU32("VFSSalt");
	U32 new_salt;
	std::string old_vfs_data_file;
	std::string old_vfs_index_file;
	std::string new_vfs_data_file;
	std::string new_vfs_index_file;
	std::string static_vfs_index_file;
	std::string static_vfs_data_file;

	if (gSavedSettings.getBOOL("AllowMultipleViewers"))
	{
		// don't mess with renaming the VFS in this case
		new_salt = old_salt;
	}
	else
	{
		do
		{
			new_salt = rand();
		} while(new_salt == old_salt);
	}

	old_vfs_data_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, VFS_DATA_FILE_BASE) + llformat("%u",old_salt);

	// make sure this file exists
	llstat s;
	S32 stat_result = LLFile::stat(old_vfs_data_file, &s);
	if (stat_result)
	{
		// doesn't exist, look for a data file
		std::string mask;
		mask = VFS_DATA_FILE_BASE;
		mask += "*";

		std::string dir;
		dir = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");

		std::string found_file;
		LLDirIterator iter(dir, mask);
		if (iter.next(found_file))
		{
			old_vfs_data_file = gDirUtilp->add(dir, found_file);

			size_t start_pos = found_file.find_last_of('.');
			if (start_pos != std::string::npos && start_pos != 0)
			{
				sscanf(found_file.substr(start_pos+1).c_str(), "%d", &old_salt);
			}
			LL_DEBUGS("AppCache") << "Default vfs data file not present, found: " << old_vfs_data_file << " Old salt: " << old_salt << LL_ENDL;
		}
	}

	old_vfs_index_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, VFS_INDEX_FILE_BASE) + llformat("%u",old_salt);

	stat_result = LLFile::stat(old_vfs_index_file, &s);
	if (stat_result)
	{
		// We've got a bad/missing index file, nukem!
		LL_WARNS("AppCache") << "Bad or missing vfx index file " << old_vfs_index_file << LL_ENDL;
		LL_WARNS("AppCache") << "Removing old vfs data file " << old_vfs_data_file << LL_ENDL;
		LLFile::remove(old_vfs_data_file);
		LLFile::remove(old_vfs_index_file);
		
		// Just in case, nuke any other old cache files in the directory.
		std::string dir;
		dir = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");

		std::string mask;
		mask = VFS_DATA_FILE_BASE;
		mask += "*";

		gDirUtilp->deleteFilesInDir(dir, mask);

		mask = VFS_INDEX_FILE_BASE;
		mask += "*";

		gDirUtilp->deleteFilesInDir(dir, mask);
	}

	new_vfs_data_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, VFS_DATA_FILE_BASE) + llformat("%u", new_salt);
	new_vfs_index_file = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, VFS_INDEX_FILE_BASE) + llformat("%u", new_salt);

	static_vfs_data_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "static_data.db2");
	static_vfs_index_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "static_index.db2");

	if (resize_vfs)
	{
		LL_DEBUGS("AppCache") << "Removing old vfs and re-sizing" << LL_ENDL;
		
		LLFile::remove(old_vfs_data_file);
		LLFile::remove(old_vfs_index_file);
	}
	else if (old_salt != new_salt)
	{
		// move the vfs files to a new name before opening
		LL_DEBUGS("AppCache") << "Renaming " << old_vfs_data_file << " to " << new_vfs_data_file << LL_ENDL;
		LL_DEBUGS("AppCache") << "Renaming " << old_vfs_index_file << " to " << new_vfs_index_file << LL_ENDL;
		LLFile::rename(old_vfs_data_file, new_vfs_data_file);
		LLFile::rename(old_vfs_index_file, new_vfs_index_file);
	}

	// Startup the VFS...
	gSavedSettings.setU32("VFSSalt", new_salt);

	// Don't remove VFS after viewer crashes.  If user has corrupt data, they can reinstall. JC
	gVFS = LLVFS::createLLVFS(new_vfs_index_file, new_vfs_data_file, false, U32Bytes(vfs_size), false);
	if (!gVFS)
	{
		return false;
	}

	gStaticVFS = LLVFS::createLLVFS(static_vfs_index_file, static_vfs_data_file, true, 0, false);
	if (!gStaticVFS)
	{
		return false;
	}

	BOOL success = gVFS->isValid() && gStaticVFS->isValid();
	if (!success)
	{
		return false;
	}
	else
	{
		LLVFile::initClass();

#ifndef LL_RELEASE_FOR_DOWNLOAD
		if (gSavedSettings.getBOOL("DumpVFSCaches"))
		{
			dumpVFSCaches();
		}
#endif

		return true;
	}
}

void LLAppViewer::addOnIdleCallback(const boost::function<void()>& cb)
{
	LLDeferredTaskList::instance().addTask(cb);
}

void LLAppViewer::purgeCache()
{
	LL_INFOS("AppCache") << "Purging Cache and Texture Cache..." << LL_ENDL;
	LLAppViewer::getTextureCache()->purgeCache(LL_PATH_CACHE);
	LLVOCache::getInstance()->removeCache(LL_PATH_CACHE);
	std::string mask = "*.*";
	gDirUtilp->deleteFilesInDir(gDirUtilp->getExpandedFilename(LL_PATH_CACHE, ""), mask);
}

std::string LLAppViewer::getSecondLifeTitle() const
{
	return LLTrans::getString("APP_NAME");
}

const std::string& LLAppViewer::getWindowTitle() const 
{
	return gWindowTitle;
}

// Callback from a dialog indicating user was logged out.  
bool finish_disconnect(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (1 == option)
	{
		LLAppViewer::instance()->forceQuit();
	}
	return false;
}

// Callback from an early disconnect dialog, force an exit
bool finish_forced_disconnect(const LLSD& notification, const LLSD& response)
{
	LLAppViewer::instance()->forceQuit();
	return false;
}


void LLAppViewer::forceDisconnect(const std::string& mesg)
{
	if (gDoDisconnect)
	{
		// Already popped up one of these dialogs, don't
		// do this again.
		return;
	}
	
	// *TODO: Translate the message if possible
	std::string big_reason = LLAgent::sTeleportErrorMessages[mesg];
	if (big_reason.empty())
	{
		big_reason = mesg;
	}

	LLSD args;
	gDoDisconnect = TRUE;

	if (LLStartUp::getStartupState() < STATE_STARTED)
	{
		// Tell users what happened
		args["ERROR_MESSAGE"] = big_reason;
		LLNotificationsUtil::add("ErrorMessage", args, LLSD(), &finish_forced_disconnect);
	}
	else
	{
		args["MESSAGE"] = big_reason;
		LLNotificationsUtil::add("YouHaveBeenLoggedOut", args, LLSD(), &finish_disconnect );
	}
}

void LLAppViewer::badNetworkHandler()
{
	// Dump the packet
	gMessageSystem->dumpPacketToLog();

	// Flush all of our caches on exit in the case of disconnect due to
	// invalid packets.

	mPurgeOnExit = TRUE;

	std::ostringstream message;
	message <<
		"The viewer has detected mangled network data indicative\n"
		"of a bad upstream network connection or an incomplete\n"
		"local installation of " << LLAppViewer::instance()->getSecondLifeTitle() << ". \n"
		" \n"
		"Try uninstalling and reinstalling to see if this resolves \n"
		"the issue. \n"
		" \n"
		"If the problem continues, please report the issue at: \n"
		"http://www.singularityviewer.org";

	if (!gHippoGridManager->getCurrentGrid()->getSupportUrl().empty())
	{
		message << "\n\nOr visit the grid support page at: \n" 
				<< gHippoGridManager->getCurrentGrid()->getSupportUrl();
	}

	forceDisconnect(message.str());
}

// This routine may get called more than once during the shutdown process.
// This can happen because we need to get the screenshot before the window
// is destroyed.
void LLAppViewer::saveFinalSnapshot()
{
	if (!mSavedFinalSnapshot && !gNoRender)
	{
		gSavedSettings.setVector3d("FocusPosOnLogout", gAgentCamera.calcFocusPositionTargetGlobal());
		gSavedSettings.setVector3d("CameraPosOnLogout", gAgentCamera.calcCameraPositionTargetGlobal());
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
		gAgentCamera.changeCameraToThirdPerson( FALSE );	// don't animate, need immediate switch
		gSavedSettings.setBOOL("ShowParcelOwners", FALSE);
		idle();

		std::string snap_filename = gDirUtilp->getLindenUserDir(true);
		if (!snap_filename.empty())
		{
			snap_filename += gDirUtilp->getDirDelimiter();
			snap_filename += SCREEN_LAST_FILENAME;
			// use full pixel dimensions of viewer window (not post-scale dimensions)
			gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(), FALSE, TRUE);
			mSavedFinalSnapshot = TRUE;
		}
	}
}

void LLAppViewer::loadNameCache()
{
	// display names cache
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "avatar_name_cache.xml");
	LL_INFOS("AvNameCache") << filename << LL_ENDL;
	llifstream name_cache_stream(filename.c_str());
	if(name_cache_stream.is_open())
	{
		LLAvatarNameCache::importFile(name_cache_stream);
	}

	if (!gCacheName) return;

	std::string name_cache;
	name_cache = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "name.cache");
	llifstream cache_file(name_cache.c_str());
	if(cache_file.is_open())
	{
		if(gCacheName->importFile(cache_file)) return;
	}
}

void LLAppViewer::saveNameCache()
{
	// display names cache
	std::string filename =
		gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "avatar_name_cache.xml");
	llofstream name_cache_stream(filename.c_str());
	if(name_cache_stream.is_open())
	{
		LLAvatarNameCache::exportFile(name_cache_stream);
	}

    // real names cache
	if (gCacheName)
    {
		std::string name_cache;
		name_cache = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "name.cache");
        llofstream cache_file(name_cache.c_str());
		if(cache_file.is_open())
		{
			gCacheName->exportFile(cache_file);
		}
	}
}


/*!	@brief		This class is an LLFrameTimer that can be created with
				an elapsed time that starts counting up from the given value
				rather than 0.0.
				
				Otherwise it behaves the same way as LLFrameTimer.
*/
class LLFrameStatsTimer : public LLFrameTimer
{
public:
	LLFrameStatsTimer(F64 elapsed_already = 0.0)
		: LLFrameTimer()
		{
			mStartTime -= elapsed_already;
		}
};

static LLTrace::BlockTimerStatHandle FTM_AUDIO_UPDATE("Update Audio");
static LLTrace::BlockTimerStatHandle FTM_CLEANUP("Cleanup");
static LLTrace::BlockTimerStatHandle FTM_CLEANUP_DRAWABLES("Drawables");
static LLTrace::BlockTimerStatHandle FTM_CLEANUP_OBJECTS("Objects");
static LLTrace::BlockTimerStatHandle FTM_IDLE_CB("Idle Callbacks");
static LLTrace::BlockTimerStatHandle FTM_LOD_UPDATE("Update LOD");
static LLTrace::BlockTimerStatHandle FTM_OBJECTLIST_UPDATE("Update Objectlist");
static LLTrace::BlockTimerStatHandle FTM_REGION_UPDATE("Update Region");
static LLTrace::BlockTimerStatHandle FTM_WORLD_UPDATE("Update World");
static LLTrace::BlockTimerStatHandle FTM_NETWORK("Network");
static LLTrace::BlockTimerStatHandle FTM_AGENT_NETWORK("Agent Network");
static LLTrace::BlockTimerStatHandle FTM_VLMANAGER("VL Manager");

///////////////////////////////////////////////////////
// idle()
//
// Called every time the window is not doing anything.
// Receive packets, update statistics, and schedule a redisplay.
///////////////////////////////////////////////////////
void LLAppViewer::idle()
{
//LAZY_FT is just temporary.
#define LAZY_FT(str) static LLTrace::BlockTimerStatHandle ftm(str); LL_RECORD_BLOCK_TIME(ftm)
	pingMainloopTimeout("Main:Idle");

	// Update frame timers
	static LLTimer idle_timer;

	{
		LAZY_FT("updateFrameTimeAndCount");
		LLFrameTimer::updateFrameTimeAndCount();
	}
	{
		LAZY_FT("LLEventTimer::updateClass");
		LLEventTimer::updateClass();
	}
	{
		LAZY_FT("LLSmoothInterpolation::updateInterpolants");
		LLSmoothInterpolation::updateInterpolants();
	}
	{
		LAZY_FT("LLMortician::updateClass");
		LLMortician::updateClass();
	}
	F32 dt_raw;
	{
		LAZY_FT("UpdateGlobalTimers");
		dt_raw = idle_timer.getElapsedTimeAndResetF32();

		// Cap out-of-control frame times
		// Too low because in menus, swapping, debugger, etc.
		// Too high because idle called with no objects in view, etc.
		const F32 MIN_FRAME_RATE = 1.f;
		const F32 MAX_FRAME_RATE = 200.f;

		F32 frame_rate_clamped = 1.f / dt_raw;
		frame_rate_clamped = llclamp(frame_rate_clamped, MIN_FRAME_RATE, MAX_FRAME_RATE);
		gFrameDTClamped = 1.f / frame_rate_clamped;

		// Global frame timer
		// Smoothly weight toward current frame
		gFPSClamped = (frame_rate_clamped + (4.f * gFPSClamped)) / 5.f;

		static LLCachedControl<F32> qas(gSavedSettings, "QuitAfterSeconds");
		if (qas > 0.f)
		{
			if (gRenderStartTime.getElapsedTimeF32() > qas)
			{
				LL_INFOS() << "Quitting after " << qas << " seconds. See setting \"QuitAfterSeconds\"." << LL_ENDL;
				LLAppViewer::instance()->forceQuit();
			}
		}
	}

	//////////////////////////////////////
	//
	// Run state machines
	//

	{
		LL_RECORD_BLOCK_TIME(FTM_STATEMACHINE);
		gMainThreadEngine.mainloop();
	}

	// Must wait until both have avatar object and mute list, so poll
	// here.
	{
		LAZY_FT("request_initial_instant_messages");
		LLIMProcessing::requestOfflineMessages();
	}

	///////////////////////////////////
	//
	// Special case idle if still starting up
	//
	{
		LAZY_FT("idle_startup");
		if (LLStartUp::getStartupState() < STATE_STARTED)
		{
			// Skip rest if idle startup returns false (essentially, no world yet)
			gGLActive = TRUE;
			if (!idle_startup())
			{
				gGLActive = FALSE;
				return;
			}
			gGLActive = FALSE;
		}
	}


	F32 yaw = 0.f;				// radians

	if (!gDisconnected)
	{
		LL_RECORD_BLOCK_TIME(FTM_NETWORK);
		// Update spaceserver timeinfo
		LLWorld::getInstance()->setSpaceTimeUSec(LLWorld::getInstance()->getSpaceTimeUSec() + (U32)(dt_raw * SEC_TO_MICROSEC));


		//////////////////////////////////////
		//
		// Update simulator agent state
		//

		static LLCachedControl<bool> rotateRight(gSavedSettings, "RotateRight");
		if (rotateRight)
		{
			gAgent.moveYaw(-1.f);
		}

		{
			LL_RECORD_BLOCK_TIME(FTM_AGENT_AUTOPILOT);
			// Handle automatic walking towards points
			gAgentPilot.updateTarget();
			gAgent.autoPilot(&yaw);
		}

		static LLFrameTimer agent_update_timer;
		static U32 				last_control_flags;

		//	When appropriate, update agent location to the simulator.
		F32 agent_update_time = agent_update_timer.getElapsedTimeF32();
		BOOL flags_changed = gAgent.controlFlagsDirty()
							 || (last_control_flags != gAgent.getControlFlags());

		if (flags_changed || (agent_update_time > (1.0f / (F32)AGENT_UPDATES_PER_SECOND)))
		{
			LL_RECORD_BLOCK_TIME(FTM_AGENT_UPDATE);
			// Send avatar and camera info
			last_control_flags = gAgent.getControlFlags();
			send_agent_update(TRUE);
			agent_update_timer.reset();
		}
	}

	//////////////////////////////////////
	//
	// Manage statistics
	//
	//
	{
		LAZY_FT("Frame Stats");
		// Initialize the viewer_stats_timer with an already elapsed time
		// of SEND_STATS_PERIOD so that the initial stats report will
		// be sent immediately.
		static LLFrameStatsTimer viewer_stats_timer(SEND_STATS_PERIOD);
		reset_statistics();

		// Update session stats every large chunk of time
		// *FIX: (?) SAMANTHA
		if (viewer_stats_timer.getElapsedTimeF32() >= SEND_STATS_PERIOD && !gDisconnected)
		{
			LL_INFOS() << "Transmitting sessions stats" << LL_ENDL;
			send_stats();
			viewer_stats_timer.reset();
		}

		// Print the object debugging stats
		static LLFrameTimer object_debug_timer;
		if (object_debug_timer.getElapsedTimeF32() > 5.f)
		{
			object_debug_timer.reset();
			if (gObjectList.mNumDeadObjectUpdates)
			{
				LL_INFOS() << "Dead object updates: " << gObjectList.mNumDeadObjectUpdates << LL_ENDL;
				gObjectList.mNumDeadObjectUpdates = 0;
			}
			if (gObjectList.mNumUnknownKills)
			{
				LL_INFOS() << "Kills on unknown objects: " << gObjectList.mNumUnknownKills << LL_ENDL;
				gObjectList.mNumUnknownKills = 0;
			}
			if (gObjectList.mNumUnknownUpdates)
			{
				LL_INFOS() << "Unknown object updates: " << gObjectList.mNumUnknownUpdates << LL_ENDL;
				gObjectList.mNumUnknownUpdates = 0;
			}

			// ViewerMetrics FPS piggy-backing on the debug timer.
			// The 5-second interval is nice for this purpose.  If the object debug
			// bit moves or is disabled, please give this a suitable home.
			LLViewerAssetStatsFF::record_fps_main(gFPSClamped);
		}
		gFrameStats.addFrameData();
	}

	if (!gDisconnected)
	{
		LL_RECORD_BLOCK_TIME(FTM_NETWORK);

		////////////////////////////////////////////////
		//
		// Network processing
		//
		// NOTE: Starting at this point, we may still have pointers to "dead" objects
		// floating throughout the various object lists.
		//
		idleNameCache();
		if (gAgent.getRegion()) LLExperienceCache::instance().idleCoro();

		gFrameStats.start(LLFrameStats::IDLE_NETWORK);
		stop_glerror();
		idleNetwork();
		stop_glerror();

		gFrameStats.start(LLFrameStats::AGENT_MISC);

		// Check for away from keyboard, kick idle agents.
		idle_afk_check();

		//  Update statistics for this frame
		update_statistics();
	}

	////////////////////////////////////////
	//
	// Handle the regular UI idle callbacks as well as
	// hover callbacks
	//
	{
		LL_RECORD_BLOCK_TIME(FTM_IDLE_CB);

		// Do event notifications if necessary.  Yes, we may want to move this elsewhere.
		gEventNotifier.update();

		gIdleCallbacks.callFunctions();
		gInventory.idleNotifyObservers();
		if (auto antispam = NACLAntiSpamRegistry::getIfExists()) antispam->idle();
	}

	// Metrics logging (LLViewerAssetStats, etc.)
	{
		static LLTimer report_interval;

		// *TODO:  Add configuration controls for this
		F32 seconds = report_interval.getElapsedTimeF32();
		if (seconds >= app_metrics_interval)
		{
			LAZY_FT("metricsSend");
			metricsSend(!gDisconnected);
			report_interval.reset();
		}
	}

	if (gDisconnected)
	{
		return;
	}

	static const LLCachedControl<bool> hide_tp_screen("AscentDisableTeleportScreens", false);
	LLAgent::ETeleportState tp_state = gAgent.getTeleportState();
	if (!hide_tp_screen && tp_state != LLAgent::TELEPORT_NONE && tp_state != LLAgent::TELEPORT_LOCAL && tp_state != LLAgent::TELEPORT_PENDING)
	{
		return;
	}

	gViewerWindow->updateUI();

	//updateUI may have created sounds (clicks, etc). Call idleAudio to dispatch asap.
	idleAudio();

	///////////////////////////////////////
	// Agent and camera movement
	//
	LLCoordGL current_mouse = gViewerWindow->getCurrentMouse();

	{
		// After agent and camera moved, figure out if we need to
		// deselect objects.
		LAZY_FT("deselectAllIfTooFar");
		LLSelectMgr::getInstance()->deselectAllIfTooFar();

	}

	{
		// Handle pending gesture processing
		static LLTrace::BlockTimerStatHandle ftm("Agent Position");
		LL_RECORD_BLOCK_TIME(ftm);
		LLGestureMgr::instance().update();

		gAgent.updateAgentPosition(gFrameDTClamped, yaw, current_mouse.mX, current_mouse.mY);
	}

	{
		LL_RECORD_BLOCK_TIME(FTM_OBJECTLIST_UPDATE);
		gFrameStats.start(LLFrameStats::OBJECT_UPDATE);

		if (!(logoutRequestSent() && hasSavedFinalSnapshot()))
		{
			gObjectList.update(gAgent, *LLWorld::getInstance());
		}
	}

	//////////////////////////////////////
	//
	// Deletes objects...
	// Has to be done after doing idleUpdates (which can kill objects)
	//

	{
		gFrameStats.start(LLFrameStats::CLEAN_DEAD);
		LL_RECORD_BLOCK_TIME(FTM_CLEANUP);
		{
			LL_RECORD_BLOCK_TIME(FTM_CLEANUP_OBJECTS);
			gObjectList.cleanDeadObjects();
		}
		{
			LL_RECORD_BLOCK_TIME(FTM_CLEANUP_DRAWABLES);
			LLDrawable::cleanupDeadDrawables();
		}
	}

	//
	// After this point, in theory we should never see a dead object
	// in the various object/drawable lists.
	//

	//////////////////////////////////////
	//
	// Update/send HUD effects
	//
	// At this point, HUD effects may clean up some references to
	// dead objects.
	//

	{
		gFrameStats.start(LLFrameStats::UPDATE_EFFECTS);
		static LLTrace::BlockTimerStatHandle ftm("HUD Effects");
		LL_RECORD_BLOCK_TIME(ftm);
		LLSelectMgr::getInstance()->updateEffects();
		LLHUDManager::getInstance()->cleanupEffects();
		LLHUDManager::getInstance()->sendEffects();
	}

	stop_glerror();

	////////////////////////////////////////
	//
	// Unpack layer data that we've received
	//

	{
		LL_RECORD_BLOCK_TIME(FTM_NETWORK);
		gVLManager.unpackData();
	}

	/////////////////////////
	//
	// Update surfaces, and surface textures as well.
	//

	{
		LAZY_FT("updateVisibilities");
		LLWorld::getInstance()->updateVisibilities();
	}
	{
		const F32 max_region_update_time = .001f; // 1ms
		LL_RECORD_BLOCK_TIME(FTM_REGION_UPDATE);
		LLWorld::getInstance()->updateRegions(max_region_update_time);
	}

	/////////////////////////
	//
	// Update weather effects
	//
	if (!gNoRender)
	{
		LAZY_FT("Weather");
#if ENABLE_CLASSIC_CLOUDS
		LLWorld::getInstance()->updateClouds(gFrameDTClamped);
#endif
		gSky.propagateHeavenlyBodies(gFrameDTClamped);				// moves sun, moon, and planets

		// Update wind vector 
		LLVector3 wind_position_region;
		static LLVector3 average_wind;

		LLViewerRegion *regionp;
		regionp = LLWorld::getInstance()->resolveRegionGlobal(wind_position_region, gAgent.getPositionGlobal());	// puts agent's local coords into wind_position	
		if (regionp)
		{
			gWindVec = regionp->mWind.getVelocity(wind_position_region);

			// Compute average wind and use to drive motion of water

			average_wind = regionp->mWind.getAverage();
#if ENABLE_CLASSIC_CLOUDS
			F32 cloud_density = regionp->mCloudLayer.getDensityRegion(wind_position_region);
			gSky.setCloudDensityAtAgent(cloud_density);
#endif
			gSky.setWind(average_wind);
			//LLVOWater::setWind(average_wind);
		}
		else
		{
			gWindVec.setVec(0.0f, 0.0f, 0.0f);
		}
	}
	stop_glerror();

	//////////////////////////////////////
	//
	// Sort and cull in the new renderer are moved to pipeline.cpp
	// Here, particles are updated and drawables are moved.
	//

	if (!gNoRender)
	{
		LL_RECORD_BLOCK_TIME(FTM_WORLD_UPDATE);
		gFrameStats.start(LLFrameStats::UPDATE_MOVE);
		gPipeline.updateMove();

		gFrameStats.start(LLFrameStats::UPDATE_PARTICLES);
		LLWorld::getInstance()->updateParticles();
	}
	stop_glerror();

	{
		LAZY_FT("Move*");
		if (LLViewerJoystick::getInstance()->getOverrideCamera())
		{
			LLViewerJoystick::getInstance()->moveFlycam();
		}
		else
		{
			if (LLToolMgr::getInstance()->inBuildMode())
			{
				LLViewerJoystick::getInstance()->moveObjects();
			}

			gAgentCamera.updateCamera();
		}
	}

	// update media focus
	{
		LAZY_FT("Media Focus");
		LLViewerMediaFocus::getInstance()->update();
	}

	// Update marketplace
	{
		LAZY_FT("MPII::update");
		LLMarketplaceInventoryImporter::update();
	}
	{
		LAZY_FT("MPIN::update");
		LLMarketplaceInventoryNotifications::update();
	}

	// objects and camera should be in sync, do LOD calculations now
	{
		LL_RECORD_BLOCK_TIME(FTM_LOD_UPDATE);
		gObjectList.updateApparentAngles(gAgent);
	}

	// Update AV render info
	LLAvatarRenderInfoAccountant::idle();

	// Execute deferred tasks.
	{
		LAZY_FT("DeferredTaskRun");
		LLDeferredTaskList::instance().run();
	}
	
	// Handle shutdown process, for example, 
	// wait for floaters to close, send quit message,
	// forcibly quit if it has taken too long
	if (mQuitRequested)
	{
		gGLActive = TRUE;
		idleShutdown();
	}

	stop_glerror();
}

void LLAppViewer::idleShutdown()
{
	// Wait for all modal alerts to get resolved
	if (LLModalDialog::activeCount() > 0)
	{
		return;
	}

	// close IM interface
	if(gIMMgr)
	{
		// Save group chat ignore list -- MC
		gIMMgr->saveIgnoreGroup();
		gIMMgr->disconnectAllSessions();
	}
	
	// Wait for all floaters to get resolved
	if (gFloaterView
		&& !gFloaterView->allChildrenClosed())
	{
		return;
	}

	static bool saved_snapshot = false;
	if (!saved_snapshot)
	{
		saved_snapshot = true;
		saveFinalSnapshot();
		return;
	}

	const F32 SHUTDOWN_UPLOAD_SAVE_TIME = 5.f;

	S32 pending_uploads = gAssetStorage->getNumPendingUploads();
	if (pending_uploads > 0
		&& gLogoutTimer.getElapsedTimeF32() < SHUTDOWN_UPLOAD_SAVE_TIME
		&& !logoutRequestSent())
	{
		static S32 total_uploads = 0;
		// Sometimes total upload count can change during logout.
		total_uploads = llmax(total_uploads, pending_uploads);
		gViewerWindow->setShowProgress(!gSavedSettings.getBOOL("AscentDisableLogoutScreens"));
		S32 finished_uploads = total_uploads - pending_uploads;
		F32 percent = 100.f * finished_uploads / total_uploads;
		gViewerWindow->setProgressPercent(percent);
		gViewerWindow->setProgressString(LLTrans::getString("SavingSettings"));
		return;
	}

	// All floaters are closed.  Tell server we want to quit.
	if( !logoutRequestSent() )
	{
		sendLogoutRequest();

		// Wait for a LogoutReply message
		gViewerWindow->setShowProgress(!gSavedSettings.getBOOL("AscentDisableLogoutScreens"));
		gViewerWindow->setProgressPercent(100.f);
		gViewerWindow->setProgressString(LLTrans::getString("LoggingOut"));
		return;
	}

	// Make sure that we quit if we haven't received a reply from the server.
	if( logoutRequestSent() 
		&& gLogoutTimer.getElapsedTimeF32() > gLogoutMaxTime )
	{
		forceQuit();
		return;
	}
}

void LLAppViewer::sendLogoutRequest()
{
	if(!mLogoutRequestSent && gMessageSystem)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LogoutRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID() );
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();

		gLogoutTimer.reset();
		gLogoutMaxTime = LOGOUT_REQUEST_TIME;
		mLogoutRequestSent = TRUE;
		
		if(LLVoiceClient::instanceExists())
		{
			LLVoiceClient::getInstance()->leaveChannel();
		}

		//Set internal status variables and marker files
		gLogoutInProgress = TRUE;
		mLogoutMarkerFileName = gDirUtilp->getExpandedFilename(LL_PATH_LOGS,LOGOUT_MARKER_FILE_NAME);

		if (mLogoutMarkerFile.open(mLogoutMarkerFileName, LL_APR_W) == APR_SUCCESS)
		{
			LL_INFOS() << "Created logout marker file " << mLogoutMarkerFileName << LL_ENDL;
			mLogoutMarkerFile.close();
		}
		else
		{
			LL_WARNS() << "Cannot create logout marker file " << mLogoutMarkerFileName << LL_ENDL;
		}		
	}
}

void LLAppViewer::idleNameCache()
{
	// Neither old nor new name cache can function before agent has a region
	LLViewerRegion* region = gAgent.getRegion();
	if (!region) return;

	// deal with any queued name requests and replies.
	gCacheName->processPending();

	// Can't run the new cache until we have the list of capabilities
	// for the agent region, and can therefore decide whether to use
	// display names or fall back to the old name system.
	if (!region->capabilitiesReceived()) return;

	// Agent may have moved to a different region, so need to update cap URL
	// for name lookups.  Can't do this in the cap grant code, as caps are
	// granted to neighbor regions before the main agent gets there.  Can't
	// do it in the move-into-region code because cap not guaranteed to be
	// granted yet, for example on teleport.
	bool had_capability = LLAvatarNameCache::hasNameLookupURL();
	std::string name_lookup_url;
	name_lookup_url.reserve(128); // avoid a memory allocation below
	name_lookup_url = region->getCapability("GetDisplayNames");
	bool have_capability = !name_lookup_url.empty();
	if (have_capability)
	{
		// we have support for display names, use it
		U32 url_size = name_lookup_url.size();
		// capabilities require URLs with slashes before query params:
		// https://<host>:<port>/cap/<uuid>/?ids=<blah>
		// but the caps are granted like:
		// https://<host>:<port>/cap/<uuid>
		if (url_size > 0 && name_lookup_url[url_size-1] != '/')
		{
			name_lookup_url += '/';
		}
		LLAvatarNameCache::setNameLookupURL(name_lookup_url);
	}
	else
	{
		// Display names not available on this region
		LLAvatarNameCache::setNameLookupURL( std::string() );
	}

	// Error recovery - did we change state?
	if (had_capability != have_capability)
	{
		// name tags are persistant on screen, so make sure they refresh
		LLVOAvatar::invalidateNameTags();	//Should this be commented out?
	}

	LLAvatarNameCache::idle();
}

//
// Handle messages, and all message related stuff
//

#define TIME_THROTTLE_MESSAGES

#ifdef TIME_THROTTLE_MESSAGES
#define CHECK_MESSAGES_DEFAULT_MAX_TIME .020f // 50 ms = 50 fps (just for messages!)
static F32 CheckMessagesMaxTime = CHECK_MESSAGES_DEFAULT_MAX_TIME;
#endif

static LLTrace::BlockTimerStatHandle FTM_IDLE_NETWORK("Idle Network");
static LLTrace::BlockTimerStatHandle FTM_MESSAGE_ACKS("Message Acks");
static LLTrace::BlockTimerStatHandle FTM_RETRANSMIT("Retransmit");
static LLTrace::BlockTimerStatHandle FTM_TIMEOUT_CHECK("Timeout Check");
static LLTrace::BlockTimerStatHandle FTM_DYNAMIC_THROTTLE("Dynamic Throttle");
static LLTrace::BlockTimerStatHandle FTM_CHECK_REGION_CIRCUIT("Check Region Circuit");

void LLAppViewer::idleNetwork()
{
	pingMainloopTimeout("idleNetwork");

	gObjectList.mNumNewObjects = 0;
	S32 total_decoded = 0;

	static const LLCachedControl<bool> speedTest(gSavedSettings, "SpeedTest");
	if (!speedTest)
	{
		LL_RECORD_BLOCK_TIME(FTM_IDLE_NETWORK); // decode
		
		LL_PUSH_CALLSTACKS();
		LLTimer check_message_timer;
		//  Read all available packets from network 
		const S64 frame_count = gFrameCount;  // U32->S64
		F32 total_time = 0.0f;

		while (gMessageSystem->checkAllMessages(frame_count, gServicePump))
		{
			if (gDoDisconnect)
			{
				// We're disconnecting, don't process any more messages from the server
				// We're usually disconnecting due to either network corruption or a
				// server going down, so this is OK.
				break;
			}

			total_decoded++;
			gPacketsIn++;

			if (total_decoded > MESSAGE_MAX_PER_FRAME)
			{
				break;
			}

#ifdef TIME_THROTTLE_MESSAGES
			// Prevent slow packets from completely destroying the frame rate.
			// This usually happens due to clumps of avatars taking huge amount
			// of network processing time (which needs to be fixed, but this is
			// a good limit anyway).
			total_time = check_message_timer.getElapsedTimeF32();
			if (total_time >= CheckMessagesMaxTime)
				break;
#endif
		}

		// Handle per-frame message system processing.
		gMessageSystem->processAcks();

#ifdef TIME_THROTTLE_MESSAGES
		if (total_time >= CheckMessagesMaxTime)
		{
			// Increase CheckMessagesMaxTime so that we will eventually catch up
			CheckMessagesMaxTime *= 1.035f; // 3.5% ~= x2 in 20 frames, ~8x in 60 frames
		}
		else
		{
			// Reset CheckMessagesMaxTime to default value
			CheckMessagesMaxTime = CHECK_MESSAGES_DEFAULT_MAX_TIME;
		}
#endif
		


		// we want to clear the control after sending out all necessary agent updates
		gAgent.resetControlFlags();
		
		// Decode enqueued messages...
		S32 remaining_possible_decodes = MESSAGE_MAX_PER_FRAME - total_decoded;

		if( remaining_possible_decodes <= 0 )
		{
			LL_INFOS() << "Maxed out number of messages per frame at " << MESSAGE_MAX_PER_FRAME << LL_ENDL;
		}

		if (gPrintMessagesThisFrame)
		{
			LL_INFOS() << "Decoded " << total_decoded << " msgs this frame!" << LL_ENDL;
			gPrintMessagesThisFrame = FALSE;
		}
	}

	LLViewerStats::getInstance()->mNumNewObjectsStat.addValue(gObjectList.mNumNewObjects);

	// Retransmit unacknowledged packets.
	gXferManager->retransmitUnackedPackets();
	gAssetStorage->checkForTimeouts();
	gViewerThrottle.updateDynamicThrottle();

	// Check that the circuit between the viewer and the agent's current
	// region is still alive
	LLViewerRegion *agent_region = gAgent.getRegion();
	if (agent_region && (LLStartUp::getStartupState()==STATE_STARTED))
	{
		LLUUID this_region_id = agent_region->getRegionID();
		bool this_region_alive = agent_region->isAlive();
		if ((mAgentRegionLastAlive && !this_region_alive) // newly dead
			&& (mAgentRegionLastID == this_region_id)) // same region
		{
			forceDisconnect(LLTrans::getString("AgentLostConnection"));
		}
		mAgentRegionLastID = this_region_id;
		mAgentRegionLastAlive = this_region_alive;
	}
}

void LLAppViewer::idleAudio()
{
	gFrameStats.start(LLFrameStats::AUDIO);
	LL_RECORD_BLOCK_TIME(FTM_AUDIO_UPDATE);
	
	if (gAudiop)
	{
		audio_update_volume();
		audio_update_listener();

		// this line actually commits the changes we've made to source positions, etc.
		const F32 max_audio_decode_time = 0.002f; // 2 ms decode time
		gAudiop->idle(max_audio_decode_time);
	}
}
void LLAppViewer::shutdownAudio()
{
	if (gAudiop)
	{
		// shut down the audio subsystem
		gAudiop->shutdown();

		delete gAudiop;
		gAudiop = NULL;
	}
}

void LLAppViewer::disconnectViewer()
{
	if (gDisconnected)
	{
		return;
	}
	//
	// Cleanup after quitting.
	//	
	// Save snapshot for next time, if we made it through initialization

	LL_INFOS() << "Disconnecting viewer!" << LL_ENDL;

	// Dump our frame statistics
	gFrameStats.dump();

	// Remember if we were flying
	gSavedSettings.setBOOL("FlyingAtExit", gAgent.getFlying() );

	// Un-minimize all windows so they don't get saved minimized
	if (!gNoRender)
	{
		if (gFloaterView)
		{
			gFloaterView->restoreAll();

			std::list<LLFloater*> floaters_to_close;
			for (LLView::child_list_const_iter_t it = gFloaterView->getChildList()->begin();
				it != gFloaterView->getChildList()->end();
				++it)
			{
				// The following names are defined in the 
				// floater_image_preview.xml
				// floater_sound_preview.xml
				// floater_animation_preview.xml
				// files.

				// A more generic mechanism would be nice..
				LLFloater* fl = static_cast<LLFloater*>(*it);
				if (fl
					&& (fl->getName() == "Image Preview"
						|| fl->getName() == "Sound Preview"
						|| fl->getName() == "Animation Preview"
						|| fl->getName() == "perm prefs"
						))
				{
					floaters_to_close.push_back(fl);
				}
			}

			while (!floaters_to_close.empty())
			{
				LLFloater* fl = floaters_to_close.front();
				floaters_to_close.pop_front();
				fl->close();
			}
		}
	}

	if (LLSelectMgr::getInstance())
	{
		LLSelectMgr::getInstance()->deselectAll();
	}

	if (!gNoRender)
	{
		// save inventory if appropriate
		gInventory.cache(gInventory.getRootFolderID(), gAgent.getID());
		if (gInventory.getLibraryRootFolderID().notNull()
			&& gInventory.getLibraryOwnerID().notNull())
		{
			gInventory.cache(
				gInventory.getLibraryRootFolderID(),
				gInventory.getLibraryOwnerID());
		}
	}

	saveNameCache();
	if (LLExperienceCache::instanceExists())
	{
		// TODO: LLExperienceCache::cleanup() logic should be moved to
		// cleanupSingleton().
		LLExperienceCache::instance().cleanup();
	}

	// close inventory interface, close all windows
	LLPanelMainInventory::cleanup();
// [SL:KB] - Patch: Appearance-Misc | Checked: 2013-02-12 (Catznip-3.4)
	// Destroying all objects below will trigger attachment detaching code and attempt to remove the COF links for them
	LLAppearanceMgr::instance().setAttachmentInvLinkEnable(false);
// [/SL:KB]

	gAgentWearables.cleanup();
	gAgentCamera.cleanup();
	// Also writes cached agent settings to gSavedSettings
	gAgent.cleanup();

	// This is where we used to call gObjectList.destroy() and then delete gWorldp.
	// Now we just ask the LLWorld singleton to cleanly shut down.
	if(LLWorld::instanceExists())
	{
		LLWorld::getInstance()->destroyClass();
	}

	// call all self-registered classes
	LLDestroyClassList::instance().fireCallbacks();

	cleanup_xfer_manager();

	shutdownAudio();

	gDisconnected = TRUE;
}

void LLAppViewer::forceErrorLLError()
{
   	LL_ERRS() << "This is a deliberate llerror" << LL_ENDL;
}

void LLAppViewer::forceErrorBreakpoint()
{
   	LL_WARNS() << "Forcing a deliberate breakpoint" << LL_ENDL;
#ifdef LL_WINDOWS
	DebugBreak();
#else
    asm ("int $3");
#endif
	return;
}

void LLAppViewer::forceErrorBadMemoryAccess()
{
   	LL_WARNS() << "Forcing a deliberate bad memory access" << LL_ENDL;
    S32* crash = nullptr;
	*crash = 0xDEADBEEF;
	return;
}

void LLAppViewer::forceErrorInfiniteLoop()
{
   	LL_WARNS() << "Forcing a deliberate infinite loop" << LL_ENDL;
	while(true)
	{
		;
	}
	return;
}
 
void LLAppViewer::forceErrorSoftwareException()
{
   	LL_WARNS() << "Forcing a deliberate exception" << LL_ENDL;
	// *FIX: Any way to insure it won't be handled?
	throw; 
}

void LLAppViewer::forceErrorDriverCrash()
{
   	LL_WARNS() << "Forcing a deliberate driver crash" << LL_ENDL;
	glDeleteTextures(1, nullptr);
}

void LLAppViewer::initMainloopTimeout(const std::string& state, F32 secs)
{
	if(!mMainloopTimeout)
	{
		mMainloopTimeout = new LLWatchdogTimeout();
		resumeMainloopTimeout(state, secs);
	}
}

void LLAppViewer::destroyMainloopTimeout()
{
	if(mMainloopTimeout)
	{
		delete mMainloopTimeout;
		mMainloopTimeout = nullptr;
	}
}

void LLAppViewer::resumeMainloopTimeout(const std::string& state, F32 secs)
{
	if(mMainloopTimeout)
	{
		if(secs < 0.0f)
		{
			static const LLCachedControl<F32> mainloop_timeout_default("MainloopTimeoutDefault",20);
			secs = mainloop_timeout_default;
		}
		
		mMainloopTimeout->setTimeout(secs);
		mMainloopTimeout->start(state);
	}
}

void LLAppViewer::pauseMainloopTimeout()
{
	if(mMainloopTimeout)
	{
		mMainloopTimeout->stop();
	}
}

void LLAppViewer::pingMainloopTimeout(const std::string& state, F32 secs)
{
//	if(!restoreErrorTrap())
//	{
//		LL_WARNS() << "!!!!!!!!!!!!! Its an error trap!!!!" << state << LL_ENDL;
//	}
	
	if(mMainloopTimeout)
	{
		if(secs < 0.0f)
		{
			static const LLCachedControl<F32> mainloop_timeout_default("MainloopTimeoutDefault",20);
			secs = mainloop_timeout_default;
		}

		mMainloopTimeout->setTimeout(secs);
		mMainloopTimeout->ping(state);
	}
}

void LLAppViewer::handleLoginComplete()
{
	initMainloopTimeout("Mainloop Init");

	// Store some data to DebugInfo in case of a freeze.
	gDebugInfo["ClientInfo"]["Name"] = LLVersionInfo::getChannel();

	gDebugInfo["ClientInfo"]["MajorVersion"] = LLVersionInfo::getMajor();
	gDebugInfo["ClientInfo"]["MinorVersion"] = LLVersionInfo::getMinor();
	gDebugInfo["ClientInfo"]["PatchVersion"] = LLVersionInfo::getPatch();
	gDebugInfo["ClientInfo"]["BuildVersion"] = LLVersionInfo::getBuild();

	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if ( parcel && parcel->getMusicURL()[0])
	{
		gDebugInfo["ParcelMusicURL"] = parcel->getMusicURL();
	}	
	if ( parcel && parcel->getMediaURL()[0])
	{
		gDebugInfo["ParcelMediaURL"] = parcel->getMediaURL();
	}
	
	gDebugInfo["SettingsFilename"] = gSavedSettings.getString("ClientSettingsFile");
	gDebugInfo["CAFilename"] = gDirUtilp->getCAFile();
	gDebugInfo["ViewerExePath"] = gDirUtilp->getExecutablePathAndName();
	gDebugInfo["CurrentPath"] = gDirUtilp->getCurPath();

	if(gAgent.getRegion())
	{
		gDebugInfo["CurrentSimHost"] = gAgent.getRegionHost().getHostName();
		gDebugInfo["CurrentRegion"] = gAgent.getRegion()->getName();
	}

	if(LLAppViewer::instance()->mMainloopTimeout)
	{
		gDebugInfo["MainloopTimeoutState"] = LLAppViewer::instance()->mMainloopTimeout->getState();
	}

	mOnLoginCompleted();
	
	// Singu Note: This would usually be registered via mOnLoginCompleted, but that would require code in newview regardless so.. just call directly here.
	LLNotifications::instance().onLoginCompleted();

	// Singu Note: Due to MAINT-4001, we must do this here, it lives in LLSidepanelInventory::updateInbox upstream.
	// Consolidate Received items
	// We shouldn't have to do that but with a client/server system relying on a "well known folder" convention,
	// things can get messy and conventions broken. This call puts everything back together in its right place.
	LLUUID id(gInventory.findCategoryUUIDForType(LLFolderType::FT_INBOX, true));
	if (id.notNull()) gInventory.consolidateForType(id, LLFolderType::FT_INBOX);

	writeDebugInfo();
}


/**
 * LLViewerAssetStats collects data on a per-region (as defined by the agent's
 * location) so we need to tell it about region changes which become a kind of
 * hidden variable/global state in the collectors.  For collectors not running
 * on the main thread, we need to send a message to move the data over safely
 * and cheaply (amortized over a run).
 */
void LLAppViewer::metricsUpdateRegion(U64 region_handle)
{
	if (0 != region_handle)
	{
		LLViewerAssetStatsFF::set_region_main(region_handle);
		if (LLAppViewer::sTextureFetch)
		{
			// Send a region update message into 'thread1' to get the new region.
			LLAppViewer::sTextureFetch->commandSetRegion(region_handle);
		}
		else
		{
			// No 'thread1', a.k.a. TextureFetch, so update directly
			LLViewerAssetStatsFF::set_region_thread1(region_handle);
		}
	}
}


/**
 * Attempts to start a multi-threaded metrics report to be sent back to
 * the grid for consumption.
 */
void LLAppViewer::metricsSend(bool enable_reporting)
{
	if (! gViewerAssetStatsMain)
		return;

	if (LLAppViewer::sTextureFetch)
	{
		LLViewerRegion * regionp = gAgent.getRegion();

		if (enable_reporting && regionp)
		{
			std::string	caps_url = regionp->getCapability("ViewerMetrics");

			// Make a copy of the main stats to send into another thread.
			// Receiving thread takes ownership.
			LLViewerAssetStats * main_stats(new LLViewerAssetStats(*gViewerAssetStatsMain));
			
			// Send a report request into 'thread1' to get the rest of the data
			// and provide some additional parameters while here.
			LLAppViewer::sTextureFetch->commandSendMetrics(caps_url,
														   gAgentSessionID,
														   gAgentID,
														   main_stats);
			main_stats = 0;		// Ownership transferred
		}
		else
		{
			LLAppViewer::sTextureFetch->commandDataBreak();
		}
	}

	// Reset even if we can't report.  Rather than gather up a huge chunk of
	// data, we'll keep to our sampling interval and retain the data
	// resolution in time.
	gViewerAssetStatsMain->reset();
}

