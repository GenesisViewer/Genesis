/** 
 * @file llappviewer.h
 * @brief The LLAppViewer class declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLAPPVIEWER_H
#define LL_LLAPPVIEWER_H

#include "llallocator.h"
#include "llsys.h"			// for LLOSInfo
#include "llviewercontrol.h"	// settings_map_type

class LLCommandLineParser;
class LLTextureCache;
class LLImageDecodeThread;
class LLTextureFetch;
class LLWatchdogTimeout;

class LLAppViewer : public LLApp
{
public:
	LLAppViewer();
	virtual ~LLAppViewer();

    /**
     * @brief Access to the LLAppViewer singleton.
     * 
     * The LLAppViewer singleton is created in main()/WinMain().
     * So don't use it in pre-entry (static initialization) code.
     */
    static LLAppViewer* instance() {return sInstance; } 

	//
	// Main application logic
	//
	virtual bool init();			// Override to do application initialization
	virtual bool cleanup();			// Override to do application cleanup
	virtual bool mainLoop(); // Override for the application main loop.  Needs to at least gracefully notice the QUITTING state and exit.

	// Application control
	void flushVFSIO(); // waits for vfs transfers to complete
	void forceQuit(); // Puts the viewer into 'shutting down without error' mode.
	void fastQuit(S32 error_code = 0); // Shuts down the viewer immediately after sending a logout message
	void requestQuit(); // Request a quit. A kinder, gentler quit.
	void userQuit(); // The users asks to quit. Confirm, then requestQuit()
    void earlyExit(const std::string& name, 
				   const LLSD& substitutions = LLSD()); // Display an error dialog and forcibly quit.
    void abortQuit();  // Called to abort a quit request.

    bool quitRequested() { return mQuitRequested; }
    bool logoutRequestSent() { return mLogoutRequestSent; }
	bool isSecondInstance() { return mSecondInstance; }

	void writeDebugInfo(bool isStatic=true);

	const LLOSInfo& getOSInfo() const { return mSysOSInfo; }

	// Report true if under the control of a debugger. A null-op default.
	virtual bool beingDebugged() { return false; } 

	virtual bool restoreErrorTrap() = 0; // Require platform specific override to reset error handling mechanism.
	                                     // return false if the error trap needed restoration.
	void initCrashReporting(); // What to do with crash report?
	static void handleViewerCrash(); // Hey! The viewer crashed. Do this, soon.
    
	// Thread accessors
	static LLTextureCache* getTextureCache() { return sTextureCache; }
	static LLImageDecodeThread* getImageDecodeThread() { return sImageDecodeThread; }
	static LLTextureFetch* getTextureFetch() { return sTextureFetch; }

	static U32 getTextureCacheVersion() ;
	static U32 getObjectCacheVersion() ;

	const std::string& getSerialNumber() { return mSerialNumber; }
	
	bool getPurgeCache() const { return mPurgeCache; }
	
	std::string getSecondLifeTitle() const; // The Second Life title.
	const std::string& getWindowTitle() const; // The window display name.

    void forceDisconnect(const std::string& msg); // Force disconnection, with a message to the user.
    void badNetworkHandler(); // Cause a crash state due to bad network packet.

	bool hasSavedFinalSnapshot() { return mSavedFinalSnapshot; }
	void saveFinalSnapshot(); 

    void loadNameCache();
    void saveNameCache();

	void removeMarkerFiles();
	
	void removeDumpDir();
    // LLAppViewer testing helpers.
    // *NOTE: These will potentially crash the viewer. Only for debugging.
    virtual void forceErrorLLError();
    virtual void forceErrorBreakpoint();
    virtual void forceErrorBadMemoryAccess();
    virtual void forceErrorInfiniteLoop();
    virtual void forceErrorSoftwareException();
    virtual void forceErrorDriverCrash();

	// *NOTE: There are currently 2 settings files: 
	// "Global", and "PerAccount"
	// The list is found in app_settings/settings_files.xml
	// but since they are used explicitly in code,
	// the follow consts should also do the trick.
	static const std::string sGlobalSettingsName; 
	static const std::string sPerAccountSettingsName; 

	// Load settings from the location specified by loction_key.
	// Key availale and rules for loading, are specified in 
	// 'app_settings/settings_files.xml'
	bool loadSettingsFromDirectory(AIReadAccess<settings_map_type> const& settings_r,
	                               std::string const& location_key,
	                               bool set_defaults = false);

	std::string getSettingsFilename(std::string const& location_key,
	                                std::string const& file);

	// For thread debugging. 
	// llstartup needs to control init.
	// llworld, send_agent_pause() also controls pause/resume.
	void initMainloopTimeout(const std::string& state, F32 secs = -1.0f);
	void destroyMainloopTimeout();
	void pauseMainloopTimeout();
	void resumeMainloopTimeout(const std::string& state = "", F32 secs = -1.0f);
	void pingMainloopTimeout(const std::string& state, F32 secs = -1.0f);

	// Handle the 'login completed' event.
	// *NOTE:Mani Fix this for login abstraction!!
	void handleLoginComplete();

    LLAllocator & getAllocator() { return mAlloc; }

	// On LoginCompleted callback
	typedef boost::signals2::signal<void (void)> login_completed_signal_t;
	login_completed_signal_t mOnLoginCompleted;
	boost::signals2::connection setOnLoginCompletedCallback( const login_completed_signal_t::slot_type& cb ) { return mOnLoginCompleted.connect(cb); } 

	void addOnIdleCallback(const boost::function<void()>& cb); // add a callback to fire (once) when idle

	void purgeCache(); // Clear the local cache. 

	// Metrics policy helper statics.
	static void metricsUpdateRegion(U64 region_handle);
	static void metricsSend(bool enable_reporting);
protected:
	virtual bool initWindow(); // Initialize the viewer's window.
	virtual void initLoggingAndGetLastDuration(); // Initialize log files, logging system
	void initLoggingInternal();
	virtual void initConsole() {}; // Initialize OS level debugging console.
	virtual bool initHardwareTest() { return true; } // A false result indicates the app should quit.
	virtual bool initSLURLHandler();
	virtual bool sendURLToOtherInstance(const std::string& url);

	virtual bool initParseCommandLine(LLCommandLineParser& clp) 
        { return true; } // Allow platforms to specify the command line args.

	virtual std::string generateSerialNumber() = 0; // Platforms specific classes generate this.


private:

	void initMaxHeapSize();
	bool initThreads(); // Initialize viewer threads, return false on failure.
	bool initConfiguration(); // Initialize settings from the command line/config file.
	bool initCache(); // Initialize local client cache.
	void checkMemory() ;

	// We have switched locations of both Mac and Windows cache, make sure
	// files migrate and old cache is cleared out.
	void migrateCacheDirectory();

	void cleanupSavedSettings(); // Sets some config data to current or default values during cleanup.
	void removeCacheFiles(const std::string& filemask); // Deletes cached files the match the given wildcard.

	void writeSystemInfo(); // Write system info to "debug_info.log"

	void processMarkerFiles(); 
	static void recordMarkerVersion(LLAPRFile& marker_file);
	bool markerIsSameVersion(const std::string& marker_name) const;
    
    void idle(); 
    void idleShutdown();
	// update avatar SLID and display name caches
	void idleNameCache();
    void idleNetwork();
	void idleAudio();
	void shutdownAudio();

    void sendLogoutRequest();
    void disconnectViewer();

	// *FIX: the app viewer class should be some sort of singleton, no?
	// Perhaps its child class is the singleton and this should be an abstract base.
	static LLAppViewer* sInstance; 

    bool mSecondInstance; // Is this a second instance of the app?

	std::string mMarkerFileName;
	LLAPRFile mMarkerFile; // A file created to indicate the app is running.

	std::string mLogoutMarkerFileName;
	LLAPRFile mLogoutMarkerFile; // A file created to indicate the app is running.
	
	LLOSInfo mSysOSInfo; 
	bool mReportedCrash;

	// Thread objects.
	static LLTextureCache* sTextureCache; 
	static LLImageDecodeThread* sImageDecodeThread; 
	static LLTextureFetch* sTextureFetch;

	S32 mNumSessions;

	std::string mSerialNumber;
	bool mPurgeCache;
    bool mPurgeOnExit;

	bool mSavedFinalSnapshot;

    bool mQuitRequested;				// User wants to quit, may have modified documents open.
    bool mLogoutRequestSent;			// Disconnect message sent to simulator, no longer safe to send messages to the sim.
	LLSD mSettingsLocationList;

	LLWatchdogTimeout* mMainloopTimeout;

	// for tracking viewer<->region circuit death
	bool mAgentRegionLastAlive;
	LLUUID mAgentRegionLastID;

    LLAllocator mAlloc;

	LLFrameTimer mMemCheckTimer;
	
public:
	//some information for updater
	typedef struct
	{
		std::string mUpdateExePath;
		std::ostringstream mParams;
	}LLUpdaterInfo ;
	static LLUpdaterInfo *sUpdaterInfo ;
};

// consts from viewer.h
const S32 AGENT_UPDATES_PER_SECOND  = 10;
const S32 AGENT_FORCE_UPDATES_PER_SECOND  = 1;

// Globals with external linkage. From viewer.h
// *NOTE:Mani - These will be removed as the Viewer App Cleanup project continues.
//
// "// llstartup" indicates that llstartup is the only client for this global.

extern LLSD gDebugInfo;
extern BOOL	gShowObjectUpdates;

extern BOOL gAcceptTOS;
extern BOOL gAcceptCriticalMessage;

typedef enum 
{
	LAST_EXEC_NORMAL = 0,
	LAST_EXEC_FROZE,
	LAST_EXEC_LLERROR_CRASH,
	LAST_EXEC_OTHER_CRASH,
	LAST_EXEC_LOGOUT_FROZE,
	LAST_EXEC_LOGOUT_CRASH
} eLastExecEvent;

extern eLastExecEvent gLastExecEvent; // llstartup

extern U32 gFrameCount;
extern U32 gForegroundFrameCount;

extern LLPumpIO* gServicePump;

// Is the Pacific time zone (aka server time zone)
// currently in daylight savings time?
extern BOOL gPacificDaylightTime;
extern U64MicrosecondsImplicit	gStartTime;
extern U64MicrosecondsImplicit	gFrameTime;					// The timestamp of the most-recently-processed frame
extern F32SecondsImplicit		gFrameTimeSeconds;			// Loses msec precision after ~4.5 hours...
extern F32SecondsImplicit		gFrameIntervalSeconds;		// Elapsed time between current and previous gFrameTimeSeconds
extern F32		gFPSClamped;				// Frames per second, smoothed, weighted toward last frame
extern F32		gFrameDTClamped;

extern U32 		gFrameStalls;

extern LLTimer gRenderStartTime;
extern LLFrameTimer gForegroundTime;

extern F32 gLogoutMaxTime;
extern LLTimer gLogoutTimer;

extern F32 gSimLastTime; 
extern F32 gSimFrames;

extern BOOL		gDisconnected;

extern LLFrameTimer	gRestoreGLTimer;
extern BOOL			gRestoreGL;
extern BOOL		gUseWireframe;

// VFS globals - gVFS is for general use
// gStaticVFS is read-only and is shipped w/ the viewer
// it has pre-cache data like the UI .TGAs
extern LLVFS	*gStaticVFS;

extern LLMemoryInfo gSysMemory;
extern U64Bytes gMemoryAllocated;

extern std::string gLastVersionChannel;

extern LLVector3 gWindVec;
extern LLVector3 gRelativeWindVec;
extern U32	gPacketsIn;
extern BOOL gPrintMessagesThisFrame;

extern LLUUID gSunTextureID;
extern LLUUID gMoonTextureID;

extern BOOL gRandomizeFramerate;
extern BOOL gPeriodicSlowFrame;

#endif // LL_LLAPPVIEWER_H
