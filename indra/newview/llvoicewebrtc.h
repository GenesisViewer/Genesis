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
private:
	llwebrtc::LLWebRTCDeviceInterface *mWebRTCDeviceInterface;
	LLVoiceVersionInfo mVoiceVersion;
	bool  mVoiceEnabled;

    S32   mEarLocation;	
	bool mSpatialCoordsDirty;
	// These variables can last longer than WebRTC in coroutines so we need them as static
    static bool sShuttingDown;	
};

#endif //LL_VOICE_WEBRTC_H