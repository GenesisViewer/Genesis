#ifndef LL_VOICE_WEBRTC_H
#define LL_VOICE_WEBRTC_H


#include "llcororesponder.h"
#include "llvoiceclient.h"
#include "llvoicechannel.h"

// WebRTC Includes
#include <llwebrtc.h>
class LLAvatarName;
class LLVoiceWebRTCConnection;
typedef boost::shared_ptr<LLVoiceWebRTCConnection> connectionPtr_t;
class LLVoiceWebRTCConnection :
    public llwebrtc::LLWebRTCSignalingObserver,
    public llwebrtc::LLWebRTCDataObserver
{
public:
    LLVoiceWebRTCConnection(const LLUUID &regionID, const std::string &channelID);

    virtual ~LLVoiceWebRTCConnection() = 0;
	 //////////////////////////////
    /// @name Signaling notification
    //  LLWebRTCSignalingObserver
    //@{
    void OnIceGatheringState(EIceGatheringState state) override;
    void OnIceCandidate(const llwebrtc::LLWebRTCIceCandidate &candidate) override;
    void OnOfferAvailable(const std::string &sdp) override;
    void OnRenegotiationNeeded() override;
    void OnAudioEstablished(llwebrtc::LLWebRTCAudioInterface *audio_interface) override;
    //@}

    /////////////////////////
    /// @name Data Notification
    /// LLWebRTCDataObserver
    //@{
    void OnDataReceived(const std::string &data, bool binary) override;
    void OnDataChannelReady(llwebrtc::LLWebRTCDataInterface *data_interface) override;
	
	void OnDataReceivedImpl(const std::string &data, bool binary);
	bool connectionStateMachine();
	void processIceUpdates();
	void processIceUpdatesCoro();
	virtual void setMuteMic(bool muted);
    virtual void setMicGain(F32 volume);
    virtual void setSpeakerVolume(F32 volume);
	virtual bool isSpatial() = 0;
	void OnVoiceConnectionRequestSuccess(const LLCoroResponder& responder);
	LLUUID getRegionID() { return mRegionID; }
	void sendJoin();
	void sendData(const std::string &data);
    void shutDown()
    {
        mShutDown = true;
    }
protected:
    typedef enum e_voice_connection_state
    {
        VOICE_STATE_ERROR                  = 0x0,
        VOICE_STATE_START_SESSION          = 0x1,
        VOICE_STATE_WAIT_FOR_SESSION_START = 0x2,
        VOICE_STATE_REQUEST_CONNECTION     = 0x4,
        VOICE_STATE_CONNECTION_WAIT        = 0x8,
        VOICE_STATE_SESSION_ESTABLISHED    = 0x10,
        VOICE_STATE_WAIT_FOR_DATA_CHANNEL  = 0x20,
        VOICE_STATE_SESSION_UP             = 0x40,
        VOICE_STATE_SESSION_RETRY          = 0x80,
        VOICE_STATE_DISCONNECT             = 0x100,
        VOICE_STATE_WAIT_FOR_EXIT          = 0x200,
        VOICE_STATE_SESSION_EXIT           = 0x400,
        VOICE_STATE_SESSION_STOPPING       = 0x780
    } EVoiceConnectionState;

    EVoiceConnectionState mVoiceConnectionState;
    //LL::WorkQueue::weak_t mMainQueue;

    void                  setVoiceConnectionState(EVoiceConnectionState new_voice_connection_state)
    {
        if (new_voice_connection_state & VOICE_STATE_SESSION_STOPPING)
        {
            // the new state is shutdown or restart.
            mVoiceConnectionState = new_voice_connection_state;
            return;
        }
        if (mVoiceConnectionState & VOICE_STATE_SESSION_STOPPING)
        {
            // we're currently shutting down or restarting, so ignore any
            // state changes.
            return;
        }

        mVoiceConnectionState = new_voice_connection_state;
    }
    EVoiceConnectionState getVoiceConnectionState()
    {
        return mVoiceConnectionState;
    }

    virtual void requestVoiceConnection() = 0;
    void requestVoiceConnectionCoro() { requestVoiceConnection(); }
	
    void breakVoiceConnectionCoro();

    LLVoiceClientStatusObserver::EStatusType mCurrentStatus;

    LLUUID mRegionID;
    LLUUID mViewerSession;
    std::string mChannelID;

    std::string mChannelSDP;
    std::string mRemoteChannelSDP;

    bool mMuted;
    F32  mMicGain;
    F32  mSpeakerVolume;

    bool mShutDown;
    S32  mOutstandingRequests;

    S32  mRetryWaitPeriod; // number of UPDATE_THROTTLE_SECONDS we've
                           // waited since our last attempt to connect.
    F32  mRetryWaitSecs;   // number of seconds to wait before next retry



    std::vector<llwebrtc::LLWebRTCIceCandidate> mIceCandidates;
    bool                                        mIceCompleted;

    llwebrtc::LLWebRTCPeerConnectionInterface *mWebRTCPeerConnectionInterface;
    llwebrtc::LLWebRTCAudioInterface *mWebRTCAudioInterface;
    llwebrtc::LLWebRTCDataInterface  *mWebRTCDataInterface;
};
class LLVoiceWebRTCSpatialConnection :
    public LLVoiceWebRTCConnection
{
  public:
    LLVoiceWebRTCSpatialConnection(const LLUUID &regionID, S32 parcelLocalID, const std::string &channelID);

    virtual ~LLVoiceWebRTCSpatialConnection();

    void setMuteMic(bool muted) override;

    bool isSpatial() override { return true; }


protected:

    void requestVoiceConnection() override;

    S32    mParcelLocalID;
};
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
	static bool isShuttingDown() { return sShuttingDown; }
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
	typedef std::set<LLVoiceClientParticipantObserver*> observer_set_t;
	observer_set_t mParticipantObservers;

	

    void notifyParticipantObservers();
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
	std::set<LLUUID> mNeighboringRegions; // includes current region
	std::set<LLUUID> getNeighboringRegions() { return mNeighboringRegions; }

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
	void OnConnectionEstablished(const std::string& channelID, const LLUUID& regionID);
    void OnConnectionShutDown(const std::string &channelID, const LLUUID &regionID);
    void OnConnectionFailure(const std::string &channelID,
        const LLUUID &regionID,
        LLVoiceClientStatusObserver::EStatusType status_type = LLVoiceClientStatusObserver::ERROR_UNKNOWN);
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
	
	//----------------------------------
    // devices
    void clearCaptureDevices();
    void addCaptureDevice(const LLVoiceDevice& device);

    void clearRenderDevices();
    void addRenderDevice(const LLVoiceDevice& device);

    void setDevicesListUpdated(bool state);
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
	void sendPositionUpdate(bool force);

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
		participantState(const LLUUID& agent_id);
		
		bool updateMuteState();	// true if mute state has changed
		bool isAvatar();

		std::string mURI;
		LLUUID mAvatarID;
		std::string mAccountName;
		std::string mDisplayName;
		LLFrameTimer mSpeakingTimeout;
		F32	mLastSpokeTimestamp;
		F32 mPower;
		F32 mLevel; // the current audio level of the participant
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
	typedef boost::shared_ptr<participantState> participantStatePtr_t;
	typedef std::map<const LLUUID, participantStatePtr_t> participantUUIDMap;

	

    boost::signals2::connection mAvatarNameCacheConnection;

	class sessionState
	{
	public:
		sessionState();
		~sessionState();
		
		typedef boost::shared_ptr<sessionState> ptr_t;
		typedef boost::weak_ptr<sessionState> wptr_t;
		typedef boost::function<void(const ptr_t &)> sessionFunc_t;
		participantStatePtr_t addParticipant(const LLUUID& agent_id);
		// Note: after removeParticipant returns, the participant* that was passed to it will have been deleted.
		// Take care not to use the pointer again after that.
		// Singu Note: removeParticipant now internally finds the entry. More efficient. Other option is to add a find procedure that returns an iterator.
		void removeParticipant(const std::string& uri);
		void removeAllParticipants();

		participantState *findParticipant(const std::string &uri);
		participantState *findParticipantByID(const LLUUID& id);
		static ptr_t matchSessionByChannelID(const std::string& channel_id);
		static void addSession(const std::string &channelID, ptr_t& session);
		virtual bool isSpatial() = 0;
		virtual bool isEstate()  = 0;
		virtual bool isCallbackPossible() = 0;
		static void for_each(sessionFunc_t func);
		bool isTextIMPossible();
		void setMuteMic(bool muted);
        void setMicGain(F32 volume);
        void setSpeakerVolume(F32 volume);
		void shutdownAllConnections();
		void revive();
		static void processSessionStates();
		virtual bool processConnectionStates();
		virtual void sendData(const std::string &data);
		std::string mHandle;
		std::string mChannelID;
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

	   bool mHangupOnLastLeave;  // notify observers after the session becomes empty.
       bool mNotifyOnFirstJoin;  // notify observers when the first peer joins.
	   bool    mMuted;          // this session is muted.
        F32     mMicGain;        // gain for this session.
        F32     mSpeakerVolume;  // volume for this session.

		bool        mShuttingDown;
		participantUUIDMap mParticipantsByUUID;
	protected:
		std::list<connectionPtr_t> mWebRTCConnections;
	private:
		static std::map<std::string, ptr_t> mSessions;  // canonical list of outstanding sessions.
		static void for_eachPredicate(const std::pair<std::string,
                                      LLWebRTCVoiceClient::sessionState::wptr_t> &a,
                                      sessionFunc_t func);
		 
	};
	class estateSessionState : public sessionState
	{
		public:
		estateSessionState();
		bool processConnectionStates() override;

		bool isSpatial() override { return true; }
		bool isEstate() override { return true; }
		bool isCallbackPossible() override { return false; }
	};
private:
	llwebrtc::LLWebRTCDeviceInterface *mWebRTCDeviceInterface;
	LLVoiceVersionInfo mVoiceVersion;
	

    LLVector3d   mListenerPosition;
    LLVector3d   mListenerRequestedPosition;
    LLVector3    mListenerVelocity;
    LLQuaternion mListenerRot;

    LLVector3d   mAvatarPosition;
    LLVector3    mAvatarVelocity;
    
	std::string mAccountName;
	std::string mAccountPassword;
	std::string mAccountDisplayName;
	sessionState *mAudioSession;		// Session state for the current audio session
	bool mAudioSessionChanged;			// set to true when the above pointer gets changed, so observers can be notified.
	bool  mVoiceEnabled;
	bool mTuningMode;
	enum
    {
        earLocCamera = 0,        // ear at camera
        earLocAvatar,            // ear at avatar
        earLocMixed              // ear at avatar location/camera direction
    };
    S32   mEarLocation;	
	bool		mSpeakerVolumeDirty;
	bool		mSpeakerMuteDirty;
	
	bool mSpatialCoordsDirty;
	// These variables can last longer than WebRTC in coroutines so we need them as static
    static bool sShuttingDown;	

	sessionState *mNextAudioSession;	// Session state for the audio session we're trying to join

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

	std::string mCaptureDevice;
	std::string mRenderDevice;
	bool mCaptureDeviceDirty;
	bool mRenderDeviceDirty;
	bool mIsInitialized;
	// We should kill the voice daemon in case of connection alert
	bool mTerminateDaemon;
	int mTuningMicVolume;
	bool mTuningMicVolumeDirty;
	float mTuningEnergy;
	float mNonFriendsVoiceAttenuation;
	int mTuningSpeakerVolume;
	bool mTuningSpeakerVolumeDirty;

	int			mMicVolume;
	bool		mMicVolumeDirty;
	float mSpeakerVolume;

    
	bool mConnected;
	std::string mChannelName;			// Name of the channel to be looked up
	std::string mSpatialSessionURI;
	std::string mSpatialSessionCredentials;
	bool mAreaVoiceDisabled;
	void setState(state inState);
	state getState(void)  { return mState; };
	std::string state2string(state inState);

	// Pokes the state machine to leave the audio session next time around.
	void sessionTerminate();
	void switchChannel(std::string uri = std::string(), bool spatial = true, bool no_reconnect = false, bool is_p2p = false, std::string hash = "");
	
	void deleteSession(sessionState *session);
	// This is called in several places where the session _may_ need to be deleted.
	// It contains logic for whether to delete the session or keep it around.
	void reapSession(sessionState *session);
	void joinSession(sessionState *session);
	void verifySessionState(void);
	void setSessionHandle(sessionState *session, const std::string &handle = LLStringUtil::null);
	void setSessionURI(sessionState *session, const std::string &uri);
	participantState *findParticipantByID(const LLUUID& id);

	////////////////////////////////////////
	// voice sessions.
	typedef std::set<sessionState*> sessionSet;
	typedef std::map<std::string, sessionState*> sessionMap;
	typedef sessionSet::iterator sessionIterator;
	sessionIterator sessionsBegin(void);
	sessionIterator sessionsEnd(void);
	sessionState *findSession(const std::string &handle);
	sessionState *findSessionBeingCreatedByURI(const std::string &uri);
	sessionState *findSession(const LLUUID &participant_id);
	sessionState *findSessionByCreateID(const std::string &create_id);
	sessionMap mSessionsByHandle;				// Active sessions, indexed by session handle.  Sessions which are being initiated may not be in this map.
	sessionSet mSessions;						// All sessions, not indexed.  This is the canonical session list.


	typedef std::set<LLVoiceEffectObserver*> voice_font_observer_set_t;
	voice_font_observer_set_t mVoiceFontObservers;

	void accountGetSessionFontsSendMessage();
	void accountGetTemplateFontsSendMessage();
	bool mVoiceFontsReceived;
	bool mVoiceFontsNew;
	bool mVoiceFontListDirty;
	voice_effect_list_t	mVoiceFontList;
	voice_effect_list_t	mVoiceFontTemplateList;

	bool		mMuteMic;
	bool		mMuteMicDirty;
	F32     mMicGain;        // gain for this session.
	bool mLipSyncEnabled;
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


	LLVoiceDeviceList mCaptureDevices;
    LLVoiceDeviceList mRenderDevices;
	bool mDevicesListUpdated;            // set to true when the device list has been updated
                                        // and false when the panelvoicedevicesettings has queried for an update status.

	typedef std::set<LLVoiceClientStatusObserver*> status_observer_set_t;
	status_observer_set_t mStatusObservers;

	void notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status);					
	typedef std::set<LLFriendObserver*> friend_observer_set_t;
	friend_observer_set_t mFriendObservers;
	void notifyFriendObservers();
	bool inSpatialChannel(void);
	std::string getAudioSessionURI();					

	void stateMachine();
	static void idle(void *user_data);

	/////////////////////////////
	// Sending updates of current state
	void updatePosition(void);
    void setListenerPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot);
    void setAvatarPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot);

	LLVector3d getListenerPosition() { return mListenerPosition; }
    LLVector3d getSpeakerPosition() { return mAvatarPosition; }



	LLQuaternion mAvatarRot;

	bool inEstateChannel();
	typedef boost::shared_ptr<sessionState> sessionStatePtr_t;
	sessionStatePtr_t mSession;    // Session state for the current session

    sessionStatePtr_t mNextSession;    // Session state for the session we're trying to join

	bool startEstateSession();
	/////////////////////////////
    // Accessors for data related to nearby speakers

    /////////////////////////////
    sessionStatePtr_t findP2PSession(const LLUUID &agent_id);
	sessionStatePtr_t addSession(const std::string &channel_id, sessionState::ptr_t session);

	static void predSendData(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const std::string& spatial_data);

	void enforceTether();
	void updateNeighboringRegions();

	void lookupName(const LLUUID &id);
    void onAvatarNameCache(const LLUUID& id, const LLAvatarName& av_name);
    void avatarNameResolved(const LLUUID &id, const std::string &name);
    static void predAvatarNameResolution(const LLWebRTCVoiceClient::sessionStatePtr_t &session, LLUUID id, std::string name);
};

#endif //LL_VOICE_WEBRTC_H