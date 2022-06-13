/**
 * @file llvoicevivox.h
 * @brief Declaration of LLDiamondwareVoiceClient class which is the interface to the voice client process.
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
#ifndef LL_VOICE_VIVOX_H
#define LL_VOICE_VIVOX_H



#include "llvoiceclient.h"

class LLAvatarName;


class LLVivoxVoiceClient :	public LLSingleton<LLVivoxVoiceClient>,
							virtual public LLVoiceModuleInterface,
							virtual public LLVoiceEffectInterface
{
	LOG_CLASS(LLVivoxVoiceClient);
public:
	LLVivoxVoiceClient();
	virtual ~LLVivoxVoiceClient();


	/// @name LLVoiceModuleInterface virtual implementations
	///  @see LLVoiceModuleInterface
	//@{
	virtual void init(LLPumpIO *pump);	// Call this once at application startup (creates connector)
	virtual void terminate();	// Call this to clean up during shutdown

	virtual const LLVoiceVersionInfo& getVersion();

	virtual void updateSettings(); // call after loading settings and whenever they change

	// Returns true if vivox has successfully logged in and is not in error state
	virtual bool isVoiceWorking() const;

	/////////////////////
	/// @name Tuning
	//@{
	virtual void tuningStart();
	virtual void tuningStop();
	virtual bool inTuningMode();

	virtual void tuningSetMicVolume(float volume);
	virtual void tuningSetSpeakerVolume(float volume);
	virtual float tuningGetEnergy(void);
	//@}

	/////////////////////
	/// @name Devices
	//@{
	// This returns true when it's safe to bring up the "device settings" dialog in the prefs.
	// i.e. when the daemon is running and connected, and the device lists are populated.
	virtual bool deviceSettingsAvailable();

	// Requery the vivox daemon for the current list of input/output devices.
	// If you pass true for clearCurrentList, deviceSettingsAvailable() will be false until the query has completed
	// (use this if you want to know when it's done).
	// If you pass false, you'll have no way to know when the query finishes, but the device lists will not appear empty in the interim.
	virtual void refreshDeviceLists(bool clearCurrentList = true);

	virtual void setCaptureDevice(const std::string& name);
	virtual void setRenderDevice(const std::string& name);

	virtual LLVoiceDeviceList& getCaptureDevices();
	virtual LLVoiceDeviceList& getRenderDevices();
	//@}

	virtual void getParticipantList(uuid_set_t &participants);
	virtual bool isParticipant(const LLUUID& speaker_id);

	// Send a text message to the specified user, initiating the session if necessary.
	virtual BOOL sendTextMessage(const LLUUID& participant_id, const std::string& message);

	// close any existing text IM session with the specified user
	virtual void endUserIMSession(const LLUUID &uuid);

	// Returns true if calling back the session URI after the session has closed is possible.
	// Currently this will be false only for PSTN P2P calls.
	// NOTE: this will return true if the session can't be found.
	virtual BOOL isSessionCallBackPossible(const LLUUID &session_id);

	// Returns true if the session can accept text IM's.
	// Currently this will be false only for PSTN P2P calls.
	// NOTE: this will return true if the session can't be found.
	virtual BOOL isSessionTextIMPossible(const LLUUID &session_id);


	////////////////////////////
	/// @name Channel stuff
	//@{
	// returns true iff the user is currently in a proximal (local spatial) channel.
	// Note that gestures should only fire if this returns true.
	virtual bool inProximalChannel();

	virtual void setNonSpatialChannel(const std::string &uri,
									  const std::string &credentials);

	virtual void setSpatialChannel(const std::string &uri,
								   const std::string &credentials);

	virtual void leaveNonSpatialChannel();

	virtual void leaveChannel(void);

	// Returns the URI of the current channel, or an empty string if not currently in a channel.
	// NOTE that it will return an empty string if it's in the process of joining a channel.
	virtual std::string getCurrentChannel();
	//@}


	//////////////////////////
	/// @name invitations
	//@{
	// start a voice channel with the specified user
	virtual void callUser(const LLUUID &uuid);
	virtual bool isValidChannel(std::string &channelHandle);
	virtual bool answerInvite(std::string &channelHandle);
	virtual void declineInvite(std::string &channelHandle);
	//@}

	/////////////////////////
	/// @name Volume/gain
	//@{
	virtual void setVoiceVolume(F32 volume);
	virtual void setMicGain(F32 volume);
	//@}

	/////////////////////////
	/// @name enable disable voice and features
	//@{
	virtual bool voiceEnabled();
	virtual void setVoiceEnabled(bool enabled);
	virtual BOOL lipSyncEnabled();
	virtual void setLipSyncEnabled(BOOL enabled);
	virtual void setMuteMic(bool muted);		// Set the mute state of the local mic.
	//@}

	//////////////////////////
	/// @name nearby speaker accessors
	//@{
	virtual BOOL getVoiceEnabled(const LLUUID& id);		// true if we've received data for this avatar
	virtual std::string getDisplayName(const LLUUID& id);
	virtual BOOL isParticipantAvatar(const LLUUID &id);
	virtual BOOL getIsSpeaking(const LLUUID& id);
	virtual BOOL getIsModeratorMuted(const LLUUID& id);
	virtual F32 getCurrentPower(const LLUUID& id);		// "power" is related to "amplitude" in a defined way.  I'm just not sure what the formula is...
	virtual BOOL getOnMuteList(const LLUUID& id);
	virtual F32 getUserVolume(const LLUUID& id);
	virtual void setUserVolume(const LLUUID& id, F32 volume); // set's volume for specified agent, from 0-1 (where .5 is nominal)
	//@}

	// authorize the user
	virtual void userAuthorized(const std::string& user_id,
								const LLUUID &agentID);

	//////////////////////////////
	/// @name Status notification
	//@{
	virtual void addObserver(LLVoiceClientStatusObserver* observer);
	virtual void removeObserver(LLVoiceClientStatusObserver* observer);
	virtual void addObserver(LLFriendObserver* observer);
	virtual void removeObserver(LLFriendObserver* observer);
	virtual void addObserver(LLVoiceClientParticipantObserver* observer);
	virtual void removeObserver(LLVoiceClientParticipantObserver* observer);
	//@}

	virtual std::string sipURIFromID(const LLUUID &id);
	//@}

	/// @name LLVoiceEffectInterface virtual implementations
	///  @see LLVoiceEffectInterface
	//@{

	//////////////////////////
	/// @name Accessors
	//@{
	virtual bool setVoiceEffect(const LLUUID& id);
	virtual const LLUUID getVoiceEffect();
	virtual LLSD getVoiceEffectProperties(const LLUUID& id);

	virtual void refreshVoiceEffectLists(bool clear_lists);
	virtual const voice_effect_list_t& getVoiceEffectList() const;
	virtual const voice_effect_list_t& getVoiceEffectTemplateList() const;
	//@}

	//////////////////////////////
	/// @name Status notification
	//@{
	virtual void addObserver(LLVoiceEffectObserver* observer);
	virtual void removeObserver(LLVoiceEffectObserver* observer);
	//@}

	//////////////////////////////
	/// @name Effect preview buffer
	//@{
	virtual void enablePreviewBuffer(bool enable);
	virtual void recordPreviewBuffer();
	virtual void playPreviewBuffer(const LLUUID& effect_id = LLUUID::null);
	virtual void stopPreviewBuffer();

	virtual bool isPreviewRecording();
	virtual bool isPreviewPlaying();
	//@}

	//@}


protected:
	//////////////////////
	// Vivox Specific definitions

	friend class LLVivoxVoiceAccountProvisionResponder;
	friend class LLVivoxVoiceClientMuteListObserver;
	friend class LLVivoxVoiceClientFriendsObserver;

	enum streamState
	{
		streamStateUnknown = 0,
		streamStateIdle = 1,
		streamStateConnected = 2,
		streamStateRinging = 3,
	};
	struct participantState
	{
	public:
		// Singu Note: Extended the ctor.
		participantState(const std::string &uri, const LLUUID& id, bool isAv);

		bool updateMuteState();	// true if mute state has changed
		bool isAvatar();

		std::string mURI;
		LLUUID mAvatarID;
		std::string mAccountName;
		std::string mDisplayName;
		LLFrameTimer mSpeakingTimeout;
		F32	mLastSpokeTimestamp;
		F32 mPower;
		F32 mVolume;
		std::string mGroupID;
		int mUserVolume;
		bool mPTT;
		bool mIsSpeaking;
		bool mIsModeratorMuted;
		bool mOnMuteList;		// true if this avatar is on the user's mute list (and should be muted)
		bool mVolumeSet;		// true if incoming volume messages should not change the volume
		bool mVolumeDirty;		// true if this participant needs a volume command sent (either mOnMuteList or mUserVolume has changed)
		bool mAvatarIDValid;
		bool mIsSelf;
	};
	
	// Singu Note: participantList has replaced both participantMap and participantUUIDMap.
	typedef std::vector<participantState> participantList;

	struct sessionState
	{
	public:
		sessionState();
		~sessionState();

		participantState *addParticipant(const std::string &uri);
		// Note: after removeParticipant returns, the participant* that was passed to it will have been deleted.
		// Take care not to use the pointer again after that.
		// Singu Note: removeParticipant now internally finds the entry. More efficient. Other option is to add a find procedure that returns an iterator.
		void removeParticipant(const std::string& uri);
		void removeAllParticipants();

		participantState *findParticipant(const std::string &uri);
		participantState *findParticipantByID(const LLUUID& id);

		bool isCallBackPossible();
		bool isTextIMPossible();

		std::string mHandle;
		std::string mGroupHandle;
		std::string mSIPURI;
		std::string mAlias;
		std::string mName;
		std::string mAlternateSIPURI;
		std::string mHash;			// Channel password
		std::string mErrorStatusString;
		std::queue<std::string> mTextMsgQueue;

		LLUUID		mIMSessionID;
		LLUUID		mCallerID;
		int			mErrorStatusCode;
		int			mMediaStreamState;
		int			mTextStreamState;
		bool		mCreateInProgress;	// True if a Session.Create has been sent for this session and no response has been received yet.
		bool		mMediaConnectInProgress;	// True if a Session.MediaConnect has been sent for this session and no response has been received yet.
		bool		mVoiceInvitePending;	// True if a voice invite is pending for this session (usually waiting on a name lookup)
		bool		mTextInvitePending;		// True if a text invite is pending for this session (usually waiting on a name lookup)
		bool		mSynthesizedCallerID;	// True if the caller ID is a hash of the SIP URI -- this means we shouldn't do a name lookup.
		bool		mIsChannel;	// True for both group and spatial channels (false for p2p, PSTN)
		bool		mIsSpatial;	// True for spatial channels
		bool		mIsP2P;
		bool		mIncoming;
		bool		mVoiceEnabled;
		bool		mReconnect;	// Whether we should try to reconnect to this session if it's dropped

		// Set to true when the volume/mute state of someone in the participant list changes.
		// The code will have to walk the list to find the changed participant(s).
		bool		mVolumeDirty;
		bool		mMuteDirty;

		bool		mParticipantsChanged;
		// Singu Note: mParticipantList has replaced both mParticipantsByURI and mParticipantsByUUID.
		participantList mParticipantList;

		LLUUID		mVoiceFontID;
	};

	// internal state for a simple state machine.  This is used to deal with the asynchronous nature of some of the messages.
	// Note: if you change this list, please make corresponding changes to LLVivoxVoiceClient::state2string().
	enum state
	{
		stateDisableCleanup,
		stateDisabled,				// Voice is turned off.
		stateStart,					// Class is initialized, socket is created
		stateDaemonLaunched,		// Daemon has been launched
		stateConnecting,			// connect() call has been issued
		stateConnected,				// connection to the daemon has been made, send some initial setup commands.
		stateIdle,					// socket is connected, ready for messaging
		stateMicTuningStart,
		stateMicTuningRunning,
		stateMicTuningStop,
		stateCaptureBufferPaused,
		stateCaptureBufferRecStart,
		stateCaptureBufferRecording,
		stateCaptureBufferPlayStart,
		stateCaptureBufferPlaying,
		stateConnectorStart,		// connector needs to be started
		stateConnectorStarting,		// waiting for connector handle
		stateConnectorStarted,		// connector handle received
		stateLoginRetry,			// need to retry login (failed due to changing password)
		stateLoginRetryWait,		// waiting for retry timer
		stateNeedsLogin,			// send login request
		stateLoggingIn,				// waiting for account handle
		stateLoggedIn,				// account handle received
		stateVoiceFontsWait,		// Awaiting the list of voice fonts
		stateVoiceFontsReceived,	// List of voice fonts received
		stateCreatingSessionGroup,	// Creating the main session group
		stateNoChannel,				// Need to join a channel
		stateRetrievingParcelVoiceInfo,    // waiting for parcel voice info request to return with spatial credentials
		stateJoiningSession,		// waiting for session handle
		stateSessionJoined,			// session handle received
		stateRunning,				// in session, steady state
		stateLeavingSession,		// waiting for terminate session response
		stateSessionTerminated,		// waiting for terminate session response

		stateLoggingOut,			// waiting for logout response
		stateLoggedOut,				// logout response received
		stateConnectorStopping,		// waiting for connector stop
		stateConnectorStopped,		// connector stop received

		// We go to this state if the login fails because the account needs to be provisioned.

		// error states.  No way to recover from these yet.
		stateConnectorFailed,
		stateConnectorFailedWaiting,
		stateLoginFailed,
		stateLoginFailedWaiting,
		stateJoinSessionFailed,
		stateJoinSessionFailedWaiting,

		stateJail					// Go here when all else has failed.  Nothing will be retried, we're done.
	};

	typedef std::map<std::string, sessionState*> sessionMap;



	///////////////////////////////////////////////////////
	// Private Member Functions
	//////////////////////////////////////////////////////

	//////////////////////////////
	/// @name TVC/Server management and communication
	//@{
	// Call this if the connection to the daemon terminates unexpectedly.  It will attempt to reset everything and relaunch.
	void daemonDied();

	// Call this if we're just giving up on voice (can't provision an account, etc.).  It will clean up and go away.
	void giveUp();

	// write to the tvc
	bool writeString(const std::string &str);

	void connectorCreate();
	void connectorShutdown();
	void closeSocket(void);

	void requestVoiceAccountProvision(S32 retries = 3);
	void login(
			   const std::string& account_name,
			   const std::string& password,
			   const std::string& voice_sip_uri_hostname,
			   const std::string& voice_account_server_uri);
	void loginSendMessage();
	void logout();
	void logoutSendMessage();


	//@}

	//------------------------------------
	// tuning

	void tuningRenderStartSendMessage(const std::string& name, bool loop);
	void tuningRenderStopSendMessage();

	void tuningCaptureStartSendMessage(int duration);
	void tuningCaptureStopSendMessage();

	//----------------------------------
	// devices
	void clearCaptureDevices();
	void addCaptureDevice(const std::string& name);
	void clearRenderDevices();
	void addRenderDevice(const std::string& name);
	void buildSetAudioDevices(std::ostringstream &stream);

	void getCaptureDevicesSendMessage();
	void getRenderDevicesSendMessage();

	// local audio updates
	void buildLocalAudioUpdates(std::ostringstream &stream);


	/////////////////////////////
	// Response/Event handlers
	void connectorCreateResponse(int statusCode, std::string &statusString, std::string &connectorHandle, std::string &versionID);
	void loginResponse(int statusCode, std::string &statusString, std::string &accountHandle, int numberOfAliases);
	void sessionCreateResponse(std::string &requestId, int statusCode, std::string &statusString, std::string &sessionHandle);
	void sessionGroupAddSessionResponse(std::string &requestId, int statusCode, std::string &statusString, std::string &sessionHandle);
	void sessionConnectResponse(std::string &requestId, int statusCode, std::string &statusString);
	void logoutResponse(int statusCode, std::string &statusString);
	void connectorShutdownResponse(int statusCode, std::string &statusString);

	void accountLoginStateChangeEvent(std::string &accountHandle, int statusCode, std::string &statusString, int state);
	void mediaCompletionEvent(std::string &sessionGroupHandle, std::string &mediaCompletionType);
	void mediaStreamUpdatedEvent(std::string &sessionHandle, std::string &sessionGroupHandle, int statusCode, std::string &statusString, int state, bool incoming);
	void textStreamUpdatedEvent(std::string &sessionHandle, std::string &sessionGroupHandle, bool enabled, int state, bool incoming);
	void sessionAddedEvent(std::string &uriString, std::string &alias, std::string &sessionHandle, std::string &sessionGroupHandle, bool isChannel, bool incoming, std::string &nameString, std::string &applicationString);
	void sessionGroupAddedEvent(std::string &sessionGroupHandle);
	void sessionRemovedEvent(std::string &sessionHandle, std::string &sessionGroupHandle);
	void participantAddedEvent(std::string &sessionHandle, std::string &sessionGroupHandle, std::string &uriString, std::string &alias, std::string &nameString, std::string &displayNameString, int participantType);
	void participantRemovedEvent(std::string &sessionHandle, std::string &sessionGroupHandle, std::string &uriString, std::string &alias, std::string &nameString);
	void participantUpdatedEvent(std::string &sessionHandle, std::string &sessionGroupHandle, std::string &uriString, std::string &alias, bool isModeratorMuted, bool isSpeaking, int volume, F32 energy);
	void auxAudioPropertiesEvent(F32 energy);
	void messageEvent(std::string &sessionHandle, std::string &uriString, std::string &alias, std::string &messageHeader, std::string &messageBody, std::string &applicationString);
	void sessionNotificationEvent(std::string &sessionHandle, std::string &uriString, std::string &notificationType);

	void muteListChanged();

	/////////////////////////////
	// Sending updates of current state
	void updatePosition(void);
	void setCameraPosition(const LLVector3d &position, const LLVector3 &velocity, const LLMatrix3 &rot);
	void setAvatarPosition(const LLVector3d &position, const LLVector3 &velocity, const LLMatrix3 &rot);
	bool channelFromRegion(LLViewerRegion *region, std::string &name);

	void setEarLocation(S32 loc);


	/////////////////////////////
	// Accessors for data related to nearby speakers

	// MBW -- XXX -- Not sure how to get this data out of the TVC
	BOOL getUsingPTT(const LLUUID& id);
	std::string getGroupID(const LLUUID& id);		// group ID if the user is in group chat (empty string if not applicable)

	/////////////////////////////
	BOOL getAreaVoiceDisabled();		// returns true if the area the avatar is in is speech-disabled.
										// Use this to determine whether to show a "no speech" icon in the menu bar.


	/////////////////////////////
	// Recording controls
	void recordingLoopStart(int seconds = 3600, int deltaFramesPerControlFrame = 200);
	void recordingLoopSave(const std::string& filename);
	void recordingStop();

	// Playback controls
	void filePlaybackStart(const std::string& filename);
	void filePlaybackStop();
	void filePlaybackSetPaused(bool paused);
	void filePlaybackSetMode(bool vox = false, float speed = 1.0f);

	participantState *findParticipantByID(const LLUUID& id);


	////////////////////////////////////////
	// voice sessions.
	typedef std::set<sessionState*> sessionSet;

	typedef sessionSet::iterator sessionIterator;
	sessionIterator sessionsBegin(void);
	sessionIterator sessionsEnd(void);

	sessionState *findSession(const std::string &handle);
	sessionState *findSessionBeingCreatedByURI(const std::string &uri);
	sessionState *findSession(const LLUUID &participant_id);
	sessionState *findSessionByCreateID(const std::string &create_id);

	sessionState *addSession(const std::string &uri, const std::string &handle = LLStringUtil::null);
	void setSessionHandle(sessionState *session, const std::string &handle = LLStringUtil::null);
	void setSessionURI(sessionState *session, const std::string &uri);
	void deleteSession(sessionState *session);
	void deleteAllSessions(void);

	void verifySessionState(void);

	void joinedAudioSession(sessionState *session);
	void leftAudioSession(sessionState *session);

	// This is called in several places where the session _may_ need to be deleted.
	// It contains logic for whether to delete the session or keep it around.
	void reapSession(sessionState *session);

	// Returns true if the session seems to indicate we've moved to a region on a different voice server
	bool sessionNeedsRelog(sessionState *session);


	//////////////////////////////////////
	// buddy list stuff, needed for SLIM later
	struct buddyListEntry
	{
		buddyListEntry(const std::string &uri);
		std::string mURI;
		std::string mDisplayName;
		LLUUID	mUUID;
		bool mOnlineSL;
		bool mOnlineSLim;
		bool mCanSeeMeOnline;
		bool mHasBlockListEntry;
		bool mHasAutoAcceptListEntry;
		bool mNameResolved;
		bool mInSLFriends;
		bool mInVivoxBuddies;
	};

	typedef std::map<std::string, buddyListEntry*> buddyListMap;

	/////////////////////////////
	// session control messages

	void accountListBlockRulesSendMessage();
	void accountListAutoAcceptRulesSendMessage();

	void sessionGroupCreateSendMessage();
	void sessionCreateSendMessage(sessionState *session, bool startAudio = true, bool startText = false);
	void sessionGroupAddSessionSendMessage(sessionState *session, bool startAudio = true, bool startText = false);
	void sessionMediaConnectSendMessage(sessionState *session);		// just joins the audio session
	void sessionTextConnectSendMessage(sessionState *session);		// just joins the text session
	void sessionTerminateSendMessage(sessionState *session);
	void sessionGroupTerminateSendMessage(sessionState *session);
	void sessionMediaDisconnectSendMessage(sessionState *session);
	void sessionTextDisconnectSendMessage(sessionState *session);



	// Pokes the state machine to leave the audio session next time around.
	void sessionTerminate();

	// Pokes the state machine to shut down the connector and restart it.
	void requestRelog();

	// Does the actual work to get out of the audio session
	void leaveAudioSession();

	// notifies the voice client that we've received parcel voice info
	bool parcelVoiceInfoReceived(state requesting_state);

	friend class LLVivoxVoiceClientCapResponder;


	void lookupName(const LLUUID &id);
	void onAvatarNameCache(const LLUUID& id, const LLAvatarName& av_name);
	void avatarNameResolved(const LLUUID &id, const std::string &name);
	boost::signals2::connection mAvatarNameCacheConnection;

	/////////////////////////////
	// Voice fonts

	void addVoiceFont(const S32 id,
					  const std::string &name,
					  const std::string &description,
					  const LLDate &expiration_date,
					  bool  has_expired,
					  const S32 font_type,
					  const S32 font_status,
					  const bool template_font = false);
	void accountGetSessionFontsResponse(int statusCode, const std::string &statusString);
	void accountGetTemplateFontsResponse(int statusCode, const std::string &statusString);

private:
	LLVoiceVersionInfo mVoiceVersion;

	/// Clean up objects created during a voice session.
	void cleanUp();

	state mState;
	bool mSessionTerminateRequested;
	bool mRelogRequested;
	// Number of times (in a row) "stateJoiningSession" case for spatial channel is reached in stateMachine().
	// The larger it is the greater is possibility there is a problem with connection to voice server.
	// Introduced while fixing EXT-4313.
	int mSpatialJoiningNum;

	void setState(state inState);
	state getState(void)  { return mState; };
	std::string state2string(state inState);

	void stateMachine();
	static void idle(void *user_data);

	LLHost mDaemonHost;
	LLSocket::ptr_t mSocket;
	bool mConnected;

	// We should kill the voice daemon in case of connection alert
	bool mTerminateDaemon;

	LLPumpIO *mPump;
	friend class LLVivoxProtocolParser;

	std::string mAccountName;
	std::string mAccountPassword;
	std::string mAccountDisplayName;

	bool mTuningMode;
	float mTuningEnergy;
	std::string mTuningAudioFile;
	int mTuningMicVolume;
	bool mTuningMicVolumeDirty;
	int mTuningSpeakerVolume;
	bool mTuningSpeakerVolumeDirty;
	state mTuningExitState;					// state to return to when we leave tuning mode.

	std::string mSpatialSessionURI;
	std::string mSpatialSessionCredentials;

	std::string mMainSessionGroupHandle; // handle of the "main" session group.

	std::string mChannelName;			// Name of the channel to be looked up
	bool mAreaVoiceDisabled;
	sessionState *mAudioSession;		// Session state for the current audio session
	bool mAudioSessionChanged;			// set to true when the above pointer gets changed, so observers can be notified.

	sessionState *mNextAudioSession;	// Session state for the audio session we're trying to join

//		std::string mSessionURI;			// URI of the session we're in.
//		std::string mSessionHandle;		// returned by ?

	S32 mCurrentParcelLocalID;			// Used to detect parcel boundary crossings
	std::string mCurrentRegionName;		// Used to detect parcel boundary crossings

	std::string mConnectorHandle;	// returned by "Create Connector" message
	std::string mAccountHandle;		// returned by login message
	int 		mNumberOfAliases;
	U32 mCommandCookie;

	std::string mVoiceAccountServerURI;
	std::string mVoiceSIPURIHostName;

	int mLoginRetryCount;

	sessionMap mSessionsByHandle;				// Active sessions, indexed by session handle.  Sessions which are being initiated may not be in this map.
	sessionSet mSessions;						// All sessions, not indexed.  This is the canonical session list.

	bool mBuddyListMapPopulated;
	bool mBlockRulesListReceived;
	bool mAutoAcceptRulesListReceived;
	buddyListMap mBuddyListMap;

	LLVoiceDeviceList mCaptureDevices;
	LLVoiceDeviceList mRenderDevices;

	std::string mCaptureDevice;
	std::string mRenderDevice;
	bool mCaptureDeviceDirty;
	bool mRenderDeviceDirty;

	bool mIsInitialized;


	bool checkParcelChanged(bool update = false);
	// This should be called when the code detects we have changed parcels.
	// It initiates the call to the server that gets the parcel channel.
	bool requestParcelVoiceInfo();

	void switchChannel(std::string uri = std::string(), bool spatial = true, bool no_reconnect = false, bool is_p2p = false, std::string hash = "");
	void joinSession(sessionState *session);

	std::string nameFromAvatar(LLVOAvatar *avatar);
	std::string nameFromID(const LLUUID &id);
	bool IDFromName(const std::string name, LLUUID &uuid);
	std::string displayNameFromAvatar(LLVOAvatar *avatar);
	std::string sipURIFromAvatar(LLVOAvatar *avatar);
	std::string sipURIFromName(std::string &name);

	// Returns the name portion of the SIP URI if the string looks vaguely like a SIP URI, or an empty string if not.
	std::string nameFromsipURI(const std::string &uri);

	bool inSpatialChannel(void);
	std::string getAudioSessionURI();
	std::string getAudioSessionHandle();

	void sendPositionalUpdate(void);

	void buildSetCaptureDevice(std::ostringstream &stream);
	void buildSetRenderDevice(std::ostringstream &stream);


	void sendFriendsListUpdates();

	// start a text IM session with the specified user
	// This will be asynchronous, the session may be established at a future time.
	sessionState* startUserIMSession(const LLUUID& uuid);
	void sendQueuedTextMessages(sessionState *session);

	void enforceTether(void);

	bool		mSpatialCoordsDirty;

	LLVector3d	mCameraPosition;
	LLVector3d	mCameraRequestedPosition;
	LLVector3	mCameraVelocity;
	LLMatrix3	mCameraRot;

	LLVector3d	mAvatarPosition;
	LLVector3	mAvatarVelocity;
	LLMatrix3	mAvatarRot;

	bool		mMuteMic;
	bool		mMuteMicDirty;

	// Set to true when the friends list is known to have changed.
	bool		mFriendsListDirty;

	enum
	{
		earLocCamera = 0,		// ear at camera
		earLocAvatar,			// ear at avatar
		earLocMixed,			// ear at avatar location/camera direction
		earLocSpeaker			// ear at speaker, speakers not affected by position
	};

	S32			mEarLocation;

	bool		mSpeakerVolumeDirty;
	bool		mSpeakerMuteDirty;
	int			mSpeakerVolume;

	int			mMicVolume;
	bool		mMicVolumeDirty;

	bool		mVoiceEnabled;
	bool		mWriteInProgress;
	std::string mWriteString;
	size_t		mWriteOffset;

	LLTimer		mUpdateTimer;

	BOOL		mLipSyncEnabled;

	typedef std::set<LLVoiceClientParticipantObserver*> observer_set_t;
	observer_set_t mParticipantObservers;

	void notifyParticipantObservers();

	typedef std::set<LLVoiceClientStatusObserver*> status_observer_set_t;
	status_observer_set_t mStatusObservers;

	void notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status);

	typedef std::set<LLFriendObserver*> friend_observer_set_t;
	friend_observer_set_t mFriendObservers;
	void notifyFriendObservers();

	// Voice Fonts

	void expireVoiceFonts();
	void deleteVoiceFont(const LLUUID& id);
	void deleteAllVoiceFonts();
	void deleteVoiceFontTemplates();

	S32 getVoiceFontIndex(const LLUUID& id) const;
	S32 getVoiceFontTemplateIndex(const LLUUID& id) const;

	void accountGetSessionFontsSendMessage();
	void accountGetTemplateFontsSendMessage();
	void sessionSetVoiceFontSendMessage(sessionState *session);

	void notifyVoiceFontObservers();

	typedef enum e_voice_font_type
	{
		VOICE_FONT_TYPE_NONE = 0,
		VOICE_FONT_TYPE_ROOT = 1,
		VOICE_FONT_TYPE_USER = 2,
		VOICE_FONT_TYPE_UNKNOWN
	} EVoiceFontType;

	typedef enum e_voice_font_status
	{
		VOICE_FONT_STATUS_NONE = 0,
		VOICE_FONT_STATUS_FREE = 1,
		VOICE_FONT_STATUS_NOT_FREE = 2,
		VOICE_FONT_STATUS_UNKNOWN
	} EVoiceFontStatus;

	struct voiceFontEntry
	{
		voiceFontEntry(LLUUID& id);
		~voiceFontEntry();

		LLUUID		mID;
		S32			mFontIndex;
		std::string mName;
		LLDate		mExpirationDate;
		S32			mFontType;
		S32			mFontStatus;
		bool		mIsNew;

		LLFrameTimer	mExpiryTimer;
		LLFrameTimer	mExpiryWarningTimer;
	};

	bool mVoiceFontsReceived;
	bool mVoiceFontsNew;
	bool mVoiceFontListDirty;
	voice_effect_list_t	mVoiceFontList;
	voice_effect_list_t	mVoiceFontTemplateList;

	typedef std::map<const LLUUID, voiceFontEntry*> voice_font_map_t;
	voice_font_map_t	mVoiceFontMap;
	voice_font_map_t	mVoiceFontTemplateMap;

	typedef std::set<LLVoiceEffectObserver*> voice_font_observer_set_t;
	voice_font_observer_set_t mVoiceFontObservers;

	LLFrameTimer	mVoiceFontExpiryTimer;


	// Audio capture buffer

	void captureBufferRecordStartSendMessage();
	void captureBufferRecordStopSendMessage();
	void captureBufferPlayStartSendMessage(const LLUUID& voice_font_id = LLUUID::null);
	void captureBufferPlayStopSendMessage();

	bool mCaptureBufferMode;		// Disconnected from voice channels while using the capture buffer.
	bool mCaptureBufferRecording;	// A voice sample is being captured.
	bool mCaptureBufferRecorded;	// A voice sample is captured in the buffer ready to play.
	bool mCaptureBufferPlaying;		// A voice sample is being played.
	bool mShutdownComplete;

	LLTimer	mCaptureTimer;
	LLUUID mPreviewVoiceFont;
	LLUUID mPreviewVoiceFontLast;
	S32 mPlayRequestCount;
};

/**
 * @class LLVivoxProtocolParser
 * @brief This class helps construct new LLIOPipe specializations
 * @see LLIOPipe
 *
 * THOROUGH_DESCRIPTION
 */
class LLVivoxProtocolParser : public LLIOPipe
{
	LOG_CLASS(LLVivoxProtocolParser);
public:
	LLVivoxProtocolParser();
	virtual ~LLVivoxProtocolParser();

protected:
	/* @name LLIOPipe virtual implementations
	 */
	//@{
	/**
	 * @brief Process the data in buffer
	 */
	virtual EStatus process_impl(
								 const LLChannelDescriptors& channels,
								 buffer_ptr_t& buffer,
								 bool& eos,
								 LLSD& context,
								 LLPumpIO* pump);
	//@}

	std::string		mInput;

	// Expat control members
	XML_Parser		parser;
	int				responseDepth;
	bool			ignoringTags;
	bool			isEvent;
	int				ignoreDepth;

	// Members for processing responses. The values are transient and only valid within a call to processResponse().
	bool			squelchDebugOutput;
	int				returnCode;
	int				statusCode;
	std::string		statusString;
	std::string		requestId;
	std::string		actionString;
	std::string		connectorHandle;
	std::string		versionID;
	std::string		accountHandle;
	std::string		sessionHandle;
	std::string		sessionGroupHandle;
	std::string		alias;
	std::string		applicationString;

	// Members for processing events. The values are transient and only valid within a call to processResponse().
	std::string		eventTypeString;
	int				state;
	std::string		uriString;
	bool			isChannel;
	bool			incoming;
	bool			enabled;
	std::string		nameString;
	std::string		audioMediaString;
	std::string		deviceString;
	std::string		displayNameString;
	int				participantType;
	bool			isLocallyMuted;
	bool			isModeratorMuted;
	bool			isSpeaking;
	int				volume;
	F32				energy;
	std::string		messageHeader;
	std::string		messageBody;
	std::string		notificationType;
	bool			hasText;
	bool			hasAudio;
	bool			hasVideo;
	bool			terminated;
	std::string		blockMask;
	std::string		presenceOnly;
	std::string		autoAcceptMask;
	std::string		autoAddAsBuddy;
	int				numberOfAliases;
	std::string		subscriptionHandle;
	std::string		subscriptionType;
	S32				id;
	std::string		descriptionString;
	LLDate			expirationDate;
	bool			hasExpired;
	S32				fontType;
	S32				fontStatus;
	std::string		mediaCompletionType;

	// Members for processing text between tags
	std::string		textBuffer;
	bool			accumulateText;

	void			reset();

	void			processResponse(std::string tag);

	static void XMLCALL ExpatStartTag(void *data, const char *el, const char **attr);
	static void XMLCALL ExpatEndTag(void *data, const char *el);
	static void XMLCALL ExpatCharHandler(void *data, const XML_Char *s, int len);

	void			StartTag(const char *tag, const char **attr);
	void			EndTag(const char *tag);
	void			CharData(const char *buffer, int length);
	LLDate			expiryTimeStampToLLDate(const std::string& vivox_ts);
};


#endif //LL_VIVOX_VOICE_CLIENT_H

