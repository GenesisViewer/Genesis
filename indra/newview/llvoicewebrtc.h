#ifndef LL_VOICE_WEBRTC_H
#define LL_VOICE_WEBRTC_H

#include "llvoiceclient.h"
#include "llcororesponder.h"
// WebRTC Includes
#include "llwebrtc.h"
#include "llmutelist.h"
class LLVoiceWebRTCConnection;
class LLAvatarName;
typedef std::shared_ptr<LLVoiceWebRTCConnection> connectionPtr_t;
class LLWebRTCVoiceClient :	public LLSingleton<LLWebRTCVoiceClient>,
							virtual public LLVoiceModuleInterface,
							virtual public LLVoiceEffectInterface,
							public LLMuteListObserver,
                            public llwebrtc::LLWebRTCDevicesObserver,
                            public llwebrtc::LLWebRTCLogCallback
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
	 ///////////////////
    /// @name Logging
    /// @{
    void LogMessage(llwebrtc::LLWebRTCLogCallback::LogLevel level, const std::string& message) override;
    //@}
	/////////////////////
	/// @name Tuning
	//@{
	virtual void tuningStart();
	virtual void tuningStop();
	virtual bool inTuningMode();

	virtual void tuningSetMicVolume(float volume);
	virtual void tuningSetSpeakerVolume(float volume);
    float getAudioLevel();
	virtual float tuningGetEnergy(void);
	virtual void updateDefaultBoostLevel(float volume);
	virtual void setNonFriendsVoiceAttenuation(float volume);
	virtual float getNonFriendsVoiceAttenuation(){
		return mNonFriendsVoiceAttenuation;
	};
	//@}

	/////////////////////
	/// @name Devices
	//@{
	// This returns true when it's safe to bring up the "device settings" dialog in the prefs.
	// i.e. when the daemon is running and connected, and the device lists are populated.
	virtual bool deviceSettingsAvailable();
	bool deviceSettingsUpdated() ;  //return if the list has been updated and never fetched,  only to be called from the voicepanel.
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
    bool inSpatialChannel();
	bool inOrJoiningChannel(const std::string &channelID);
	 bool inEstateChannel();
	virtual void setNonSpatialChannel(const std::string &uri,
									  const std::string &credentials);

	virtual void setSpatialChannel(const std::string &uri,
								   const std::string &credentials);
    bool setSpatialChannel(const LLSD &channelInfo);
	virtual void leaveNonSpatialChannel();

	virtual void leaveChannel(void);
	void processChannels(bool process) override;
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
	void OnConnectionEstablished(const std::string& channelID, const LLUUID& regionID);
    void OnConnectionShutDown(const std::string &channelID, const LLUUID &regionID);
    void OnConnectionFailure(const std::string &channelID,
        const LLUUID &regionID,
        LLVoiceClientStatusObserver::EStatusType status_type = LLVoiceClientStatusObserver::ERROR_UNKNOWN);
    void updatePosition(void); // update the internal position state
    void sendPositionUpdate(bool force); // send the position to the voice server.
    void updateOwnVolume();


 	//////////////////
    /// @name LLMuteListObserver
    //@{
    void onChange() override;
    void onChangeDetailed(const LLMute& ) override;
    //@}

	//////////////////////////////
	/// @name Status notification
	//@{
	virtual void addObserver(LLVoiceClientStatusObserver* observer);
	virtual void removeObserver(LLVoiceClientStatusObserver* observer);
	virtual void addObserver(LLFriendObserver* observer);
	virtual void removeObserver(LLFriendObserver* observer);
	virtual void addObserver(LLVoiceClientParticipantObserver* observer);
	virtual void removeObserver(LLVoiceClientParticipantObserver* observer);

	typedef std::set<LLVoiceClientStatusObserver*> status_observer_set_t;
    status_observer_set_t mStatusObservers;

	void notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status);

	typedef std::set<LLVoiceClientParticipantObserver*> observer_set_t;
    observer_set_t mParticipantObservers;

    void notifyParticipantObservers();
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


	LLSD getAudioSessionChannelInfo();
	
	//////////////////////////////
    /// @name Devices change notification
    //  LLWebRTCDevicesObserver
    //@{
    void OnDevicesChanged(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                          const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices) override;
    //@}
    void OnDevicesChangedImpl(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                              const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices);

    struct participantState
    {
    public:
        participantState(const LLUUID& agent_id, const LLUUID& region);

        bool isAvatar();

        std::string mURI;
        LLUUID mAvatarID;
        std::string mDisplayName;
        LLFrameTimer mSpeakingTimeout;
        F32 mLevel; // the current audio level of the participant
        F32 mVolume; // the gain applied to the participant
        bool mIsSpeaking;
        bool mIsModeratorMuted;
		bool mVoiceEnabled;
		bool mOnMuteList;
        LLUUID mRegion;
    };
    typedef std::shared_ptr<participantState> participantStatePtr_t;

    participantStatePtr_t findParticipantByID(const std::string &channelID, const LLUUID &id);
    participantStatePtr_t addParticipantByID(const std::string& channelID, const LLUUID &id, const LLUUID& region);
    void removeParticipantByID(const std::string& channelID, const LLUUID &id, const LLUUID& region);
	   
protected:
    typedef std::map<const LLUUID, participantStatePtr_t> participantUUIDMap;

    class sessionState
    {
    public:
        typedef std::shared_ptr<sessionState> ptr_t;
        typedef std::weak_ptr<sessionState> wptr_t;

        typedef std::function<void(const ptr_t &)> sessionFunc_t;

        static void addSession(const std::string &channelID, ptr_t& session);
        virtual ~sessionState();

        participantStatePtr_t addParticipant(const LLUUID& agent_id, const LLUUID& region);
        void removeParticipant(const participantStatePtr_t &participant);
        void removeAllParticipants(const LLUUID& region = LLUUID());

        participantStatePtr_t findParticipantByID(const LLUUID& id);

        static ptr_t matchSessionByChannelID(const std::string& channel_id);

        void shutdownAllConnections();
        void revive();

        static void processSessionStates();

        virtual bool processConnectionStates();

        virtual void sendData(const std::string &data);

        void setMuteMic(bool muted);
        void setSpeakerVolume(F32 volume);
        void setUserVolume(const LLUUID& id, F32 volume);

        void setUserMute(const LLUUID& id, bool mute);

	

        static void for_each(sessionFunc_t func);

        static void reapEmptySessions();
        static void clearSessions();

        bool isEmpty() { return mWebRTCConnections.empty(); }

        virtual bool isSpatial() = 0;
        virtual bool isEstate()  = 0;
        virtual bool isCallbackPossible() = 0;

        std::string mHandle;
        std::string mChannelID;
        std::string mName;

        bool    mMuted;          // this session is muted.
        F32     mSpeakerVolume;  // volume for this session.

        bool        mShuttingDown;

        participantUUIDMap mParticipantsByUUID;

        static bool hasSession(const std::string &sessionID)
        { return sSessions.find(sessionID) != sSessions.end(); }

       bool mHangupOnLastLeave;  // notify observers after the session becomes empty.
       bool mNotifyOnFirstJoin;  // notify observers when the first peer joins.
	   
    protected:
        sessionState();
        std::list<connectionPtr_t> mWebRTCConnections;

    private:

        static std::map<std::string, ptr_t> sSessions;  // canonical list of outstanding sessions.

        static void for_eachPredicate(const std::pair<std::string,
                                      LLWebRTCVoiceClient::sessionState::wptr_t> &a,
                                      sessionFunc_t func);
    };

    typedef std::shared_ptr<sessionState> sessionStatePtr_t;
    typedef std::map<std::string, sessionStatePtr_t> sessionMap;   

    sessionStatePtr_t findP2PSession(const LLUUID &agent_id);
    sessionStatePtr_t addSession(const std::string &channel_id, sessionState::ptr_t session);
    void deleteSession(const sessionStatePtr_t &session);
    
	class estateSessionState : public sessionState
    {
      public:
        estateSessionState();
        bool processConnectionStates() override;

        bool isSpatial() override { return true; }
        bool isEstate() override { return true; }
        bool isCallbackPossible() override { return false; }
    };

    class parcelSessionState : public sessionState
    {
      public:
        parcelSessionState(const std::string& channelID, S32 parcel_local_id);

        bool isSpatial() override { return true; }
        bool isEstate() override { return false; }
        bool isCallbackPossible() override { return false; }
    };

    class adhocSessionState : public sessionState
    {
    public:
        adhocSessionState(const std::string &channelID,
            const std::string& credentials,
            bool notify_on_first_join,
            bool hangup_on_last_leave);

        bool isSpatial() override { return false; }
        bool isEstate() override { return false; }

        // only p2p-type adhoc sessions allow callback
        bool isCallbackPossible() override { return mNotifyOnFirstJoin && mHangupOnLastLeave; }

        // don't send spatial data to adhoc sessions.
        void sendData(const std::string &data) override { }

    protected:
        std::string mCredentials;
    };
private:

    LLVoiceVersionInfo mVoiceVersion;

    float mNonFriendsVoiceAttenuation;  

    bool mHidden;
    bool mTuningMode;
    float mTuningMicGain;
    int mTuningSpeakerVolume;


    bool mSpatialCoordsDirty;

    bool mMuteMic;
	LLVector3d   mListenerPosition;
    LLVector3d   mListenerRequestedPosition;
    LLVector3    mListenerVelocity;
    LLQuaternion mListenerRot;
	LLVector3d	mAvatarPosition;
	LLVector3	mAvatarVelocity;
	LLQuaternion	mAvatarRot;
	void enforceTether();

	std::set<LLUUID> mNeighboringRegions; // includes current region
	void updateNeighboringRegions();
    std::set<LLUUID> getNeighboringRegions() { return mNeighboringRegions; }

	LLVector3d getListenerPosition() { return mListenerPosition; }
    LLVector3d getSpeakerPosition() { return mAvatarPosition; }

	enum
	{
		earLocCamera = 0,		// ear at camera
		earLocAvatar,			// ear at avatar
		earLocMixed,			// ear at avatar location/camera direction
		earLocSpeaker			// ear at speaker, speakers not affected by position
	};
    int mEarLocation;
    float mMicGain;

    bool mVoiceEnabled;
	
    bool mProcessChannels;
	void lookupName(const LLUUID &id);
    void onAvatarNameCache(const LLUUID& id, const LLAvatarName& av_name);
    void avatarNameResolved(const LLUUID &id, const std::string &name);
    static void predAvatarNameResolution(const LLWebRTCVoiceClient::sessionStatePtr_t &session, LLUUID id, std::string name);
    boost::signals2::connection mAvatarNameCacheConnection;
    bool mIsInTuningMode;
    bool mIsProcessingChannels;
    bool mIsCoroutineActive;
    LLPumpIO * mWebRTCPump;

	BOOL		mLipSyncEnabled;

    llwebrtc::LLWebRTCDeviceInterface *mWebRTCDeviceInterface;

    static bool sShuttingDown;

    float mSpeakerVolume;
    void setEarLocation(S32 loc);
    LLVoiceDeviceList mCaptureDevices;
    LLVoiceDeviceList mRenderDevices;
    bool mDevicesListUpdated;

	static void predSendData(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const std::string& spatial_data);
    static void predSetSpeakerVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, F32 volume);
	static void predSetMuteMic(const LLWebRTCVoiceClient::sessionStatePtr_t &session, bool mute);
    static void predSetUserVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const LLUUID& id, F32 volume);
	static void predShutdownSession(const LLWebRTCVoiceClient::sessionStatePtr_t &session);
	static void predUpdateOwnVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, F32 audio_level);
	static void predSetUserMute(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const LLUUID& id, bool mute);
	/////////////////////////////
    // Sending updates of current state
    void setListenerPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot);
    void setAvatarPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot);

    //----------------------------------
    // devices
    void clearCaptureDevices();
    void addCaptureDevice(const LLVoiceDevice& device);

    void clearRenderDevices();
    void addRenderDevice(const LLVoiceDevice& device);
    void setDevicesListUpdated(bool state);

    sessionStatePtr_t mSession;    // Session state for the current session

    sessionStatePtr_t mNextSession;    // Session state for the session we're trying to join

    bool startEstateSession();
    bool startParcelSession(const std::string& channelID, S32 parcelID);
    bool startAdHocSession(const LLSD &channelInfo, bool notify_on_first_join, bool hangup_on_last_leave);

    void setNonSpatialChannel(const LLSD& channelInfo, bool notify_on_first_join, bool hangup_on_last_leave);

	// Coroutine support methods
    //---
    void voiceConnectionCoro();

    //---
    /// Clean up objects created during a voice session.
    void cleanUp();
};

class LLVoiceWebRTCConnection :
    public llwebrtc::LLWebRTCSignalingObserver,
    public llwebrtc::LLWebRTCDataObserver,
    public std::enable_shared_from_this<LLVoiceWebRTCConnection>
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
    void OnPeerConnectionClosed() override;
    void OnAudioEstablished(llwebrtc::LLWebRTCAudioInterface *audio_interface) override;
    //@}

    /////////////////////////
    /// @name Data Notification
    /// LLWebRTCDataObserver
    //@{
    void OnDataReceived(const std::string &data, bool binary) override;
    void OnDataChannelReady(llwebrtc::LLWebRTCDataInterface *data_interface) override;
    //@}

    void OnDataReceivedImpl(const std::string &data, bool binary);

    void sendJoin();
    void sendData(const std::string &data);

    void processIceUpdates();

    static void processIceUpdatesCoro(connectionPtr_t connection);

    virtual void setMuteMic(bool muted);
    virtual void setSpeakerVolume(F32 volume);

    void setUserVolume(const LLUUID& id, F32 volume);
    void setUserMute(const LLUUID& id, bool mute);

    bool connectionStateMachine();

    virtual bool isSpatial() { return false; }

    LLUUID getRegionID() { return mRegionID; }

    void shutDown()
    {
        mShutDown = true;
    }

    void OnVoiceConnectionRequestSuccess(const LLSD &body);

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
        VOICE_STATE_WAIT_FOR_CLOSE         = 0x800,
        VOICE_STATE_CLOSED                 = 0x1000,
        VOICE_STATE_SESSION_STOPPING       = 0x1F80
    } EVoiceConnectionState;

    EVoiceConnectionState mVoiceConnectionState;
    

    void setVoiceConnectionState(EVoiceConnectionState new_voice_connection_state)
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
    static void requestVoiceConnectionCoro(connectionPtr_t connection) { connection->requestVoiceConnection(); }

    static void breakVoiceConnectionCoro(connectionPtr_t connection);

    LLVoiceClientStatusObserver::EStatusType mCurrentStatus;

    LLUUID mRegionID;
    bool   mPrimary;
    LLUUID mViewerSession;
    std::string mChannelID;

    std::string mChannelSDP;
    std::string mRemoteChannelSDP;

    bool mMuted;
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

class LLVoiceWebRTCAdHocConnection : public LLVoiceWebRTCConnection
{
  public:
    LLVoiceWebRTCAdHocConnection(const LLUUID &regionID, const std::string &channelID, const std::string& credentials);

    virtual ~LLVoiceWebRTCAdHocConnection();

    bool isSpatial() override { return false; }

  protected:
    void requestVoiceConnection() override;

    std::string mCredentials;
};
#endif //LL_VOICE_WEBRTC_H