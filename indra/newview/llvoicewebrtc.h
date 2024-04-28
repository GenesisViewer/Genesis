#ifndef LL_VOICE_WEBRTC_H
#define LL_VOICE_WEBRTC_H



#include "llvoiceclient.h"
// WebRTC Includes
#include <llwebrtc.h>
class LLAvatarName;


class LLWebRTCVoiceClient :	public LLSingleton<LLWebRTCVoiceClient>,
							virtual public LLVoiceModuleInterface,
							virtual public LLVoiceEffectInterface,
							public llwebrtc::LLWebRTCDevicesObserver
{
LOG_CLASS(LLWebRTCVoiceClient);
public:
	LLWebRTCVoiceClient();
	virtual ~LLWebRTCVoiceClient();

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
	virtual void updateDefaultBoostLevel(float volume);
	virtual void setNonFriendsVoiceAttenuation(float volume);
	virtual float getNonFriendsVoiceAttenuation(){
		return 0;
	};

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
	//////////////////////////////
    /// @name Devices change notification
    //  LLWebRTCDevicesObserver
    //@{
    void OnDevicesChanged(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                          const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices) override;
    //@}
    void OnDevicesChangedImpl(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                              const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices);
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
	void setEarLocation(S32 loc);

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
		bool isBuddy;
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
		bool		mNonFriendsAttenuationLevelDirty;

		bool		mParticipantsChanged;
		// Singu Note: mParticipantList has replaced both mParticipantsByURI and mParticipantsByUUID.
		participantList mParticipantList;

		LLUUID		mVoiceFontID;
	};
private:
	llwebrtc::LLWebRTCDeviceInterface *mWebRTCDeviceInterface;
	LLVoiceVersionInfo mVoiceVersion;

	sessionState *mAudioSession;		// Session state for the current audio session
	bool  mVoiceEnabled;
	bool mTuningMode;
    S32   mEarLocation;	
	bool mSpatialCoordsDirty;
	// These variables can last longer than WebRTC in coroutines so we need them as static
    static bool sShuttingDown;	


	// Number of times (in a row) "stateJoiningSession" case for spatial channel is reached in stateMachine().
	// The larger it is the greater is possibility there is a problem with connection to voice server.
	// Introduced while fixing EXT-4313.
	int mSpatialJoiningNum;
	bool mSessionTerminateRequested;
	bool mRelogRequested;
	state mState;

	bool mCaptureBufferMode;		// Disconnected from voice channels while using the capture buffer.
	bool mCaptureBufferRecording;	// A voice sample is being captured.
	bool mCaptureBufferRecorded;	// A voice sample is captured in the buffer ready to play.
	bool mCaptureBufferPlaying;		// A voice sample is being played.

	LLUUID mPreviewVoiceFont;
	LLUUID mPreviewVoiceFontLast;

	int mTuningMicVolume;
	bool mTuningMicVolumeDirty;
	float mTuningEnergy;
	int mTuningSpeakerVolume;
	bool mTuningSpeakerVolumeDirty;
	void setState(state inState);
	state getState(void)  { return mState; };
	std::string state2string(state inState);

	// Pokes the state machine to leave the audio session next time around.
	void sessionTerminate();

	participantState *findParticipantByID(const LLUUID& id);




	typedef std::set<LLVoiceEffectObserver*> voice_font_observer_set_t;
	voice_font_observer_set_t mVoiceFontObservers;

	void accountGetSessionFontsSendMessage();
	void accountGetTemplateFontsSendMessage();
	bool mVoiceFontsReceived;
	bool mVoiceFontsNew;
	bool mVoiceFontListDirty;
	voice_effect_list_t	mVoiceFontList;
	voice_effect_list_t	mVoiceFontTemplateList;

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
	typedef std::map<const LLUUID, voiceFontEntry*> voice_font_map_t;
	voice_font_map_t	mVoiceFontMap;
	voice_font_map_t	mVoiceFontTemplateMap;
	
	void deleteAllVoiceFonts();
	void deleteVoiceFontTemplates();
};

#endif //LL_VOICE_WEBRTC_H