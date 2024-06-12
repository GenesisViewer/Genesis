
#include "llviewerprecompiledheaders.h"
#include "llvoicewebrtc.h"
#include "llcallbacklist.h"
#include "llagent.h"
#include "llappviewer.h"	// for gDisconnected, gDisableVoice
#include "llvoavatarself.h"
#include "llviewercamera.h"
#include "llworld.h"

bool LLWebRTCVoiceClient::sShuttingDown = false;

// Defines the maximum number of times(in a row) "stateJoiningSession" case for spatial channel is reached in stateMachine()
// which is treated as normal. If this number is exceeded we suspect there is a problem with connection
// to voice server (EXT-4313). When voice works correctly, there is from 1 to 15 times. 50 was chosen
// to make sure we don't make mistake when slight connection problems happen- situation when connection to server is
// blocked is VERY rare and it's better to sacrifice response time in this situation for the sake of stability.
const int MAX_NORMAL_JOINING_SPATIAL_NUM = 50;
const F32 SPEAKING_TIMEOUT = 1.f;
static const std::string REPORTED_VOICE_SERVER_TYPE = "Secondlife WebRTC Gateway";
const std::string WEBRTC_VOICE_SERVER_TYPE = "webrtc";
const S32 INVALID_PARCEL_ID = -1;
LLWebRTCVoiceClient::LLWebRTCVoiceClient(){

    mVoiceVersion.serverVersion = "";
    mVoiceVersion.voiceServerType = REPORTED_VOICE_SERVER_TYPE;
    mVoiceVersion.internalVoiceServerType = WEBRTC_VOICE_SERVER_TYPE;
    mVoiceVersion.minorVersion = 0;
    mVoiceVersion.majorVersion = 2;
    mVoiceVersion.mBuildVersion = "";

	// set up state machine
	setState(stateDisabled);

	gIdleCallbacks.addFunction(idle, this);

}
LLWebRTCVoiceClient::~LLWebRTCVoiceClient(){

}
void LLWebRTCVoiceClient::init(LLPumpIO* pump)
{
    // constructor will set up LLVoiceClient::getInstance()
    llwebrtc::init();

    mWebRTCDeviceInterface = llwebrtc::getDeviceInterface();
    mWebRTCDeviceInterface->setDevicesObserver(this);
}

void LLWebRTCVoiceClient::terminate()
{
    if (sShuttingDown)
    {
        return;
    }

    mVoiceEnabled = false;
    llwebrtc::terminate();

    sShuttingDown = true;
}
const LLVoiceVersionInfo& LLWebRTCVoiceClient::getVersion() {
    return mVoiceVersion;
}

void LLWebRTCVoiceClient::updateSettings()
{
	//setVoiceEnabled(gSavedSettings.getBOOL("EnableVoiceChat"));
	setEarLocation(gSavedSettings.getS32("VoiceEarLocation"));

	std::string inputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
	setCaptureDevice(inputDevice);
	std::string outputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");
	setRenderDevice(outputDevice);
	F32 mic_level = gSavedSettings.getF32("AudioLevelMic");
	setMicGain(mic_level);
	setLipSyncEnabled(gSavedSettings.getBOOL("LipSyncEnabled"));
}
// positional functionality.
void LLWebRTCVoiceClient::setEarLocation(S32 loc)
{
    if (mEarLocation != loc)
    {
        LL_DEBUGS("Voice") << "Setting mEarLocation to " << loc << LL_ENDL;

        mEarLocation        = loc;
        mSpatialCoordsDirty = true;
    }
}
void LLWebRTCVoiceClient::sessionTerminate()
{
	mSessionTerminateRequested = true;
}
void LLWebRTCVoiceClient::tuningStart()
{
	mTuningMode = true;
	LL_DEBUGS("Voice") << "Starting tuning" << LL_ENDL;
	if(getState() >= stateNoChannel)
	{
		LL_DEBUGS("Voice") << "no channel" << LL_ENDL;
		sessionTerminate();
	}
}

void LLWebRTCVoiceClient::tuningStop()
{
	mTuningMode = false;
}

bool LLWebRTCVoiceClient::inTuningMode()
{
	bool result = false;
	switch(getState())
	{
	case stateMicTuningRunning:
		result = true;
		break;
	default:
		break;
	}
	return result;
}
void LLWebRTCVoiceClient::setCameraPosition(const LLVector3d &position, const LLVector3 &velocity, const LLMatrix3 &rot)
{
	mCameraRequestedPosition = position;

	if(mCameraVelocity != velocity)
	{
		mCameraVelocity = velocity;
		mSpatialCoordsDirty = true;
	}

	if(mCameraRot != rot)
	{
		mCameraRot = rot;
		mSpatialCoordsDirty = true;
	}
}

void LLWebRTCVoiceClient::setAvatarPosition(const LLVector3d &position, const LLVector3 &velocity, const LLMatrix3 &rot)
{
	if(dist_vec_squared(mAvatarPosition, position) > 0.01)
	{
		mAvatarPosition = position;
		mSpatialCoordsDirty = true;
	}

	if(mAvatarVelocity != velocity)
	{
		mAvatarVelocity = velocity;
		mSpatialCoordsDirty = true;
	}

	if(mAvatarRot != rot)
	{
		mAvatarRot = rot;
		mSpatialCoordsDirty = true;
	}
}
void LLWebRTCVoiceClient::updatePosition(void)
{

	LLViewerRegion *region = gAgent.getRegion();
	if(region && isAgentAvatarValid())
	{
		LLMatrix3 rot;
		LLVector3d pos;

		// TODO: If camera and avatar velocity are actually used by the voice system, we could compute them here...
		// They're currently always set to zero.

		// Send the current camera position to the voice code
		rot.setRows(LLViewerCamera::getInstance()->getAtAxis(), LLViewerCamera::getInstance()->getLeftAxis (),  LLViewerCamera::getInstance()->getUpAxis());
		pos = gAgent.getRegion()->getPosGlobalFromRegion(LLViewerCamera::getInstance()->getOrigin());

		LLWebRTCVoiceClient::getInstance()->setCameraPosition(
															 pos,				// position
															 LLVector3::zero,	// velocity
															 rot);				// rotation matrix

		// Send the current avatar position to the voice code
		rot = gAgentAvatarp->getRootJoint()->getWorldRotation().getMatrix3();
		pos = gAgentAvatarp->getPositionGlobal();

		// TODO: Can we get the head offset from outside the LLVOAvatar?
		//			pos += LLVector3d(mHeadOffset);
		pos += LLVector3d(0.f, 0.f, 1.f);

		LLWebRTCVoiceClient::getInstance()->setAvatarPosition(
															 pos,				// position
															 LLVector3::zero,	// velocity
															 rot);				// rotation matrix
	}
}
void LLWebRTCVoiceClient::idle(void* user_data)
{
	LLWebRTCVoiceClient* self = (LLWebRTCVoiceClient*)user_data;
	self->stateMachine();
}
bool LLWebRTCVoiceClient::inEstateChannel()
{
    return (mSession && mSession->isEstate()) || (mNextSession && mNextSession->isEstate());
}
// Initiated the various types of sessions.
bool LLWebRTCVoiceClient::startEstateSession()
{
	LL_INFOS() << "LLWebRTCVoiceClient::startEstateSession() "  << LL_ENDL;
    leaveChannel();
    mNextSession = addSession("Estate", sessionState::ptr_t(new estateSessionState()));
    return true;
}
void LLWebRTCVoiceClient::stateMachine()
{
	if(gDisconnected)
	{
		// The viewer has been disconnected from the sim.  Disable voice.
		setVoiceEnabled(false);
	}

	if(mVoiceEnabled || (!mIsInitialized && !mTerminateDaemon) )
	{
		updatePosition();
	}
	else if(mTuningMode)
	{
		// Tuning mode is special -- it needs to launch SLVoice even if voice is disabled.
	}
	else
	{
		if((getState() != stateDisabled) && (getState() != stateDisableCleanup))
		{
			// User turned off voice support.  Send the cleanup messages, close the socket, and reset.
			if(!mConnected || mTerminateDaemon)
			{
				// if voice was turned off after the daemon was launched but before we could connect to it, we may need to issue a kill.
				LL_INFOS("Voice") << "Disabling voice before connection to daemon, terminating." << LL_ENDL;
				//killGateway();
				mTerminateDaemon = false;
			}

			// logout();
			// connectorShutdown();
			//TODO LLVoiceWebRTCConnection::breakVoiceConnectionCoro()
			setState(stateDisableCleanup);
		}
	}
	LL_INFOS() << "WEBRTC in state " << state2string(getState()) << LL_ENDL; 
	switch(getState())
	{
		//MARK: stateDisableCleanup
		// case stateDisableCleanup:
		// 	// Clean up and reset everything.
		// 	closeSocket();
		// 	cleanUp();

		// 	mAccountHandle.clear();
		// 	mAccountPassword.clear();
		// 	mVoiceAccountServerURI.clear();

		// 	setState(stateDisabled);
		// break;

		//MARK: stateDisabled
		case stateDisabled:
			if(mTuningMode || ((mVoiceEnabled || !mIsInitialized) && !mAccountName.empty()))
			{
				setState(stateStart);
			}
		break;
		case stateStart:
			if (gSavedSettings.getBOOL("EnableVoiceChat")  && !inEstateChannel()) {
				// estate voice
				
                startEstateSession();
				sessionState::processSessionStates();
			}
		break;

	}
}
std::string LLWebRTCVoiceClient::state2string(LLWebRTCVoiceClient::state inState)
{
	std::string result = "UNKNOWN";

		// Prevent copy-paste errors when updating this list...
#define CASE(x)  case x:  result = #x;  break

	switch(inState)
	{
		CASE(stateDisableCleanup);
		CASE(stateDisabled);
		CASE(stateStart);
		CASE(stateDaemonLaunched);
		CASE(stateConnecting);
		CASE(stateConnected);
		CASE(stateIdle);
		CASE(stateMicTuningStart);
		CASE(stateMicTuningRunning);
		CASE(stateMicTuningStop);
		CASE(stateCaptureBufferPaused);
		CASE(stateCaptureBufferRecStart);
		CASE(stateCaptureBufferRecording);
		CASE(stateCaptureBufferPlayStart);
		CASE(stateCaptureBufferPlaying);
		CASE(stateConnectorStart);
		CASE(stateConnectorStarting);
		CASE(stateConnectorStarted);
		CASE(stateLoginRetry);
		CASE(stateLoginRetryWait);
		CASE(stateNeedsLogin);
		CASE(stateLoggingIn);
		CASE(stateLoggedIn);
		CASE(stateVoiceFontsWait);
		CASE(stateVoiceFontsReceived);
		CASE(stateCreatingSessionGroup);
		CASE(stateNoChannel);
		CASE(stateRetrievingParcelVoiceInfo);
		CASE(stateJoiningSession);
		CASE(stateSessionJoined);
		CASE(stateRunning);
		CASE(stateLeavingSession);
		CASE(stateSessionTerminated);
		CASE(stateLoggingOut);
		CASE(stateLoggedOut);
		CASE(stateConnectorStopping);
		CASE(stateConnectorStopped);
		CASE(stateConnectorFailed);
		CASE(stateConnectorFailedWaiting);
		CASE(stateLoginFailed);
		CASE(stateLoginFailedWaiting);
		CASE(stateJoinSessionFailed);
		CASE(stateJoinSessionFailedWaiting);
		CASE(stateJail);
	}

#undef CASE

	return result;
}
void LLWebRTCVoiceClient::setState(state inState)
{
	LL_DEBUGS("Voice") << "entering state " << state2string(inState) << LL_ENDL;

	mState = inState;
}
bool LLWebRTCVoiceClient::isVoiceWorking() const
{
	//Added stateSessionTerminated state to avoid problems with call in parcels with disabled voice (EXT-4758)
	// Condition with joining spatial num was added to take into account possible problems with connection to voice
	// server(EXT-4313). See bug descriptions and comments for MAX_NORMAL_JOINING_SPATIAL_NUM for more info.
	return (mSpatialJoiningNum < MAX_NORMAL_JOINING_SPATIAL_NUM) && (stateLoggedIn <= mState) && (mState <= stateSessionTerminated);
}
static int scale_mic_volume(float volume)
{
	// incoming volume has the range [0.0 ... 2.0], with 1.0 as the default.
	// Map it to Vivox levels as follows: 0.0 -> 30, 1.0 -> 50, 2.0 -> 70
	return 30 + (int)(volume * 20.0f);
}
void LLWebRTCVoiceClient::tuningSetMicVolume(float volume)
{
	int scaled_volume = scale_mic_volume(volume);

	if(scaled_volume != mTuningMicVolume)
	{
		mTuningMicVolume = scaled_volume;
		mTuningMicVolumeDirty = true;
	}
}
static int scale_speaker_volume(float volume)
{
	// incoming volume has the range [0.0 ... 1.0], with 0.5 as the default.
	// Map it to Vivox levels as follows: 0.0 -> 30, 0.5 -> 50, 1.0 -> 70
	return 30 + (int)(volume * 40.0f);

}
void LLWebRTCVoiceClient::tuningSetSpeakerVolume(float volume)
{
	int scaled_volume = scale_speaker_volume(volume);

	if(scaled_volume != mTuningSpeakerVolume)
	{
		mTuningSpeakerVolume = scaled_volume;
		mTuningSpeakerVolumeDirty = true;
	}
}
float LLWebRTCVoiceClient::tuningGetEnergy(void)
{
	return mTuningEnergy;
}
void LLWebRTCVoiceClient::updateDefaultBoostLevel(float volume) {
	if (LLVoiceClient::getInstance()->getDefaultBoostlevel() == volume)
		return;
	if (!mAudioSession) 
		return;	
	

	//apply the diff to participant but value each participant must be between 0 and 1
	participantList::iterator iter = mAudioSession->mParticipantList.begin();

		
	
	for(; iter != mAudioSession->mParticipantList.end(); iter++)
	{
		participantState *p = &*iter;
		if (p && p->isAvatar()) {
			
			float newVolume=llclamp(volume,0.0f,1.0f);
			LLVoiceClient::getInstance()->setUserVolume(p->mAvatarID,newVolume);
		}
	}
	
}
void LLWebRTCVoiceClient::getParticipantList(uuid_set_t &participants)
{
	if(mAudioSession)
	{
		// Singu Note: mParticipantList has replaced mParticipantsByURI.
		for (participantList::iterator iter = mAudioSession->mParticipantList.begin();
			iter != mAudioSession->mParticipantList.end();
			iter++)
		{
			participants.insert(iter->mAvatarID);
		}
	}
}

void LLWebRTCVoiceClient::userAuthorized(const std::string& user_id, const LLUUID &agentID)
{

	mAccountDisplayName = user_id;

	LL_INFOS("Voice") << "name \"" << mAccountDisplayName << "\" , ID " << agentID << LL_ENDL;

	//mAccountName = nameFromID(agentID);
	mAccountName=agentID.asString();
}
LLWebRTCVoiceClient::participantState* LLWebRTCVoiceClient::sessionState::findParticipantByID(const LLUUID& id)
{
	// Singu Note: mParticipantList has replaced mParticipantsByUUID.
	participantList& vec = mParticipantList;
	participantList::iterator iter = std::find_if(vec.begin(), vec.end(), boost::bind(&participantList::value_type::mAvatarID, _1) == id);

	if(iter != mParticipantList.end())
	{
		return &*iter;
	}

	return NULL;
}
LLWebRTCVoiceClient::participantState* LLWebRTCVoiceClient::findParticipantByID(const LLUUID& id)
{
	return mAudioSession ? mAudioSession->findParticipantByID(id) : NULL;
}
bool LLWebRTCVoiceClient::isParticipant(const LLUUID &speaker_id)
{
	// Singu Note: findParticipantByID does the same thing. If null, returns false.
	return findParticipantByID(speaker_id);
}
bool LLWebRTCVoiceClient::participantState::isAvatar()
{
	return mAvatarIDValid;
}
bool LLWebRTCVoiceClient::isPreviewPlaying()
{
	return (mCaptureBufferMode && mCaptureBufferPlaying);
}
bool LLWebRTCVoiceClient::isPreviewRecording()
{
	return (mCaptureBufferMode && mCaptureBufferRecording);
}
void LLWebRTCVoiceClient::stopPreviewBuffer()
{
	mCaptureBufferRecording = false;
	mCaptureBufferPlaying = false;
}
void LLWebRTCVoiceClient::playPreviewBuffer(const LLUUID& effect_id)
{
	if (!mCaptureBufferMode)
	{
		LL_DEBUGS("Voice") << "Not in voice effect preview mode, no buffer to play." << LL_ENDL;
		mCaptureBufferRecording = false;
		return;
	}

	if (!mCaptureBufferRecorded)
	{
		// Can't play until we have something recorded!
		mCaptureBufferPlaying = false;
		return;
	}

	mPreviewVoiceFont = effect_id;
	mCaptureBufferPlaying = true;
}
void LLWebRTCVoiceClient::enablePreviewBuffer(bool enable)
{
	mCaptureBufferMode = enable;
	if(mCaptureBufferMode && getState() >= stateNoChannel)
	{
		LL_DEBUGS("Voice") << "no channel" << LL_ENDL;
		sessionTerminate();
	}
}
void LLWebRTCVoiceClient::recordPreviewBuffer()
{
	if (!mCaptureBufferMode)
	{
		LL_DEBUGS("Voice") << "Not in voice effect preview mode, cannot start recording." << LL_ENDL;
		mCaptureBufferRecording = false;
		return;
	}

	mCaptureBufferRecording = true;
}
void LLWebRTCVoiceClient::addObserver(LLVoiceEffectObserver* observer)
{
	mVoiceFontObservers.insert(observer);
}

void LLWebRTCVoiceClient::removeObserver(LLVoiceEffectObserver* observer)
{
	mVoiceFontObservers.erase(observer);
}
const voice_effect_list_t& LLWebRTCVoiceClient::getVoiceEffectList() const
{
	return mVoiceFontList;
}

const voice_effect_list_t& LLWebRTCVoiceClient::getVoiceEffectTemplateList() const
{
	return mVoiceFontTemplateList;
}
void LLWebRTCVoiceClient::deleteAllVoiceFonts()
{
	mVoiceFontList.clear();

	voice_font_map_t::iterator iter;
	for (iter = mVoiceFontMap.begin(); iter != mVoiceFontMap.end(); ++iter)
	{
		delete iter->second;
	}
	mVoiceFontMap.clear();
}

void LLWebRTCVoiceClient::deleteVoiceFontTemplates()
{
	mVoiceFontTemplateList.clear();

	voice_font_map_t::iterator iter;
	for (iter = mVoiceFontTemplateMap.begin(); iter != mVoiceFontTemplateMap.end(); ++iter)
	{
		delete iter->second;
	}
	mVoiceFontTemplateMap.clear();
}
void LLWebRTCVoiceClient::refreshVoiceEffectLists(bool clear_lists)
{
	if (clear_lists)
	{
		mVoiceFontsReceived = false;
		deleteAllVoiceFonts();
		deleteVoiceFontTemplates();
	}

	accountGetSessionFontsSendMessage();
	accountGetTemplateFontsSendMessage();
}
void LLWebRTCVoiceClient::accountGetSessionFontsSendMessage()
{
	//TODO
    //WebRTC doesn't support effects, do nothing
}

void LLWebRTCVoiceClient::accountGetTemplateFontsSendMessage()
{
	//TODO
    //WebRTC doesn't support effects, do nothing
	
}
LLWebRTCVoiceClient::voiceFontEntry::~voiceFontEntry()
{
}
const LLUUID LLWebRTCVoiceClient::getVoiceEffect()
{
	return mAudioSession ? mAudioSession->mVoiceFontID : LLUUID::null;
}
LLSD LLWebRTCVoiceClient::getVoiceEffectProperties(const LLUUID& id)
{
	LLSD sd;

	voice_font_map_t::iterator iter = mVoiceFontMap.find(id);
	if (iter != mVoiceFontMap.end())
	{
		sd["template_only"] = false;
	}
	else
	{
		// Voice effect is not in the voice font map, see if there is a template
		iter = mVoiceFontTemplateMap.find(id);
		if (iter == mVoiceFontTemplateMap.end())
		{
			LL_WARNS("Voice") << "Voice effect " << id << "not found." << LL_ENDL;
			return sd;
		}
		sd["template_only"] = true;
	}

	voiceFontEntry *font = iter->second;
	sd["name"] = font->mName;
	sd["expiry_date"] = font->mExpirationDate;
	sd["is_new"] = font->mIsNew;

	return sd;
}
bool LLWebRTCVoiceClient::setVoiceEffect(const LLUUID& id)
{
	// if (!mAudioSession)
	// {
	// 	return false;
	// }

	// if (!id.isNull())
	// {
	// 	if (mVoiceFontMap.empty())
	// 	{
	// 		LL_DEBUGS("Voice") << "Voice fonts not available." << LL_ENDL;
	// 		return false;
	// 	}
	// 	else if (mVoiceFontMap.find(id) == mVoiceFontMap.end())
	// 	{
	// 		LL_DEBUGS("Voice") << "Invalid voice font " << id << LL_ENDL;
	// 		return false;
	// 	}
	// }

	// // *TODO: Check for expired fonts?
	// mAudioSession->mVoiceFontID = id;

	// // *TODO: Separate voice font defaults for spatial chat and IM?
	// gSavedPerAccountSettings.setString("VoiceEffectDefault", id.asString());

	// sessionSetVoiceFontSendMessage(mAudioSession);
	// notifyVoiceFontObservers();

    //TODO
    //WEBRTC has no voice effect

	return true;
}
std::string LLWebRTCVoiceClient::sipURIFromID(const LLUUID& id)
{
    return id.asString();
}

void LLWebRTCVoiceClient::OnDevicesChanged(const llwebrtc::LLWebRTCVoiceDeviceList& render_devices,
                                           const llwebrtc::LLWebRTCVoiceDeviceList& capture_devices)
{
    
    OnDevicesChangedImpl(render_devices, capture_devices);
        
}
void LLWebRTCVoiceClient::OnDevicesChangedImpl(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                                               const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices)
{
    

    std::string inputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
    std::string outputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");

    LL_DEBUGS("Voice") << "Setting devices to-input: '" << inputDevice << "' output: '" << outputDevice << "'" << LL_ENDL;
    clearRenderDevices();
    bool renderDeviceSet = false;
    for (auto &device : render_devices)
    {
        addRenderDevice(LLVoiceDevice(device.mDisplayName, device.mID));
        LL_DEBUGS("Voice") << "Checking render device" << "'" << device.mID << "'" << LL_ENDL;
        if (outputDevice == device.mID)
        {
            renderDeviceSet = true;
        }
    }
    if (!renderDeviceSet)
    {
        setRenderDevice("Default");
    }

    clearCaptureDevices();
    bool captureDeviceSet = false;
    for (auto &device : capture_devices)
    {
        LL_DEBUGS("Voice") << "Checking capture device:'" << device.mID << "'" << LL_ENDL;
 
        addCaptureDevice(LLVoiceDevice(device.mDisplayName, device.mID));
        if (inputDevice == device.mID)
        {
            captureDeviceSet = true;
        }
    }
    if (!captureDeviceSet)
    {
        setCaptureDevice("Default");
    }

    setDevicesListUpdated(true);
}
void LLWebRTCVoiceClient::addCaptureDevice(const LLVoiceDevice& device)
{
	LL_DEBUGS("Voice") << "display: '" << device.display_name << "' device: '" << device.full_name << "'" << LL_ENDL;
    mCaptureDevices.push_back(device);
}
void LLWebRTCVoiceClient::clearCaptureDevices()
{
	LL_DEBUGS("Voice") << "called" << LL_ENDL;
	mCaptureDevices.clear();
}
LLVoiceDeviceList& LLWebRTCVoiceClient::getCaptureDevices()
{
	return mCaptureDevices;
}
LLVoiceDeviceList& LLWebRTCVoiceClient::getRenderDevices()
{
	return mRenderDevices;
}
void LLWebRTCVoiceClient::clearRenderDevices()
{
    LL_DEBUGS("Voice") << "called" << LL_ENDL;
    mRenderDevices.clear();
}

void LLWebRTCVoiceClient::addRenderDevice(const LLVoiceDevice& device)
{
    LL_DEBUGS("Voice") << "display: '" << device.display_name << "' device: '" << device.full_name << "'" << LL_ENDL;
    mRenderDevices.push_back(device);

}
void LLWebRTCVoiceClient::setCaptureDevice(const std::string& name)
{
	if(name == "Default")
	{
		if(!mCaptureDevice.empty())
		{
			mCaptureDevice.clear();
			mCaptureDeviceDirty = true;
		}
	}
	else
	{
		if(mCaptureDevice != name)
		{
			mCaptureDevice = name;
			mCaptureDeviceDirty = true;
		}
	}
}
void LLWebRTCVoiceClient::setRenderDevice(const std::string& name)
{
	if(name == "Default")
	{
		if(!mRenderDevice.empty())
		{
			mRenderDevice.clear();
			mRenderDeviceDirty = true;
		}
	}
	else
	{
		if(mRenderDevice != name)
		{
			mRenderDevice = name;
			mRenderDeviceDirty = true;
		}
	}
	
}
void LLWebRTCVoiceClient::refreshDeviceLists(bool clearCurrentList)
{
	if(clearCurrentList)
	{
		clearCaptureDevices();
		clearRenderDevices();
	}
	mWebRTCDeviceInterface->refreshDevices();
}

bool LLWebRTCVoiceClient::deviceSettingsAvailable()
{
	bool result = true;

	if(!mConnected)
		result = false;

	if(mRenderDevices.empty())
		result = false;

	return result;
}
void LLWebRTCVoiceClient::setDevicesListUpdated(bool state)
{
    mDevicesListUpdated = state;
}
void LLWebRTCVoiceClient::setMicGain(F32 volume)
{
	int scaled_volume = scale_mic_volume(volume);

	if(scaled_volume != mMicVolume)
	{
		mMicVolume = scaled_volume;
		mMicVolumeDirty = true;
	}
}
LLWebRTCVoiceClient::sessionIterator LLWebRTCVoiceClient::sessionsBegin(void)
{
	return mSessions.begin();
}

LLWebRTCVoiceClient::sessionIterator LLWebRTCVoiceClient::sessionsEnd(void)
{
	return mSessions.end();
}


LLWebRTCVoiceClient::sessionState *LLWebRTCVoiceClient::findSession(const std::string &handle)
{
	sessionState *result = NULL;
	sessionMap::iterator iter = mSessionsByHandle.find(handle);
	if(iter != mSessionsByHandle.end())
	{
		result = iter->second;
	}

	return result;
}

LLWebRTCVoiceClient::sessionState *LLWebRTCVoiceClient::findSessionBeingCreatedByURI(const std::string &uri)
{
	sessionState *result = NULL;
	for(sessionIterator iter = sessionsBegin(); iter != sessionsEnd(); iter++)
	{
		sessionState *session = *iter;
		if(session->mCreateInProgress && (session->mSIPURI == uri))
		{
			result = session;
			break;
		}
	}

	return result;
}

LLWebRTCVoiceClient::sessionState *LLWebRTCVoiceClient::findSession(const LLUUID &participant_id)
{
	sessionState *result = NULL;

	for(sessionIterator iter = sessionsBegin(); iter != sessionsEnd(); iter++)
	{
		sessionState *session = *iter;
		if((session->mCallerID == participant_id) || (session->mIMSessionID == participant_id))
		{
			result = session;
			break;
		}
	}

	return result;
}
bool LLWebRTCVoiceClient::isValidChannel(std::string &sessionHandle)
{
	return(findSession(sessionHandle) != NULL);

}
void LLWebRTCVoiceClient::joinSession(sessionState *session)
{
	mNextAudioSession = session;

	if(getState() <= stateNoChannel)
	{
		// We're already set up to join a channel, just needed to fill in the session handle
	}
	else
	{
		// State machine will come around and rejoin if uri/handle is not empty.
		sessionTerminate();
	}
}
bool LLWebRTCVoiceClient::answerInvite(std::string &sessionHandle)
{
	// this is only ever used to answer incoming p2p call invites.

	sessionState *session = findSession(sessionHandle);
	if(session)
	{
		session->mIsSpatial = false;
		session->mReconnect = false;
		session->mIsP2P = true;

		joinSession(session);
		return true;
	}

	return false;
}
void LLWebRTCVoiceClient::declineInvite(std::string &sessionHandle)
{
	//TODO check in a newer version

	// sessionState *session = findSession(sessionHandle);
	// if(session)
	// {
	// 	sessionMediaDisconnectSendMessage(session);
	// }
}
// Returns true if the indicated participant in the current audio session is really an SL avatar.
// Currently this will be false only for PSTN callers into group chats, and PSTN p2p calls.
BOOL LLWebRTCVoiceClient::isParticipantAvatar(const LLUUID &id)
{
	BOOL result = TRUE;
	sessionState *session = findSession(id);

	if(session != NULL)
	{
		// this is a p2p session with the indicated caller, or the session with the specified UUID.
		if(session->mSynthesizedCallerID)
			result = FALSE;
	}
	else
	{
		// Didn't find a matching session -- check the current audio session for a matching participant
		if(mAudioSession != NULL)
		{
			participantState *participant = findParticipantByID(id);
			if(participant != NULL)
			{
				result = participant->isAvatar();
			}
		}
	}

	return result;
}
bool LLWebRTCVoiceClient::inProximalChannel()
{
	bool result = false;

	if((getState() == stateRunning) && !mSessionTerminateRequested)
	{
		result = inSpatialChannel();
	}

	return result;
}
std::string LLWebRTCVoiceClient::getDisplayName(const LLUUID& id)
{
	std::string result;
	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		result = participant->mDisplayName;
	}

	return result;
}
BOOL LLWebRTCVoiceClient::getIsSpeaking(const LLUUID& id)
{
	BOOL result = FALSE;

	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		if (participant->mSpeakingTimeout.getElapsedTimeF32() > SPEAKING_TIMEOUT)
		{
			participant->mIsSpeaking = FALSE;
		}
		result = participant->mIsSpeaking;
	}

	return result;
}
BOOL LLWebRTCVoiceClient::getIsModeratorMuted(const LLUUID& id)
{
	BOOL result = FALSE;

	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		result = participant->mIsModeratorMuted;
	}

	return result;
}
F32 LLWebRTCVoiceClient::getCurrentPower(const LLUUID& id)
{
	F32 result = 0;
	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		result = participant->mPower;
	}

	return result;
}
void LLWebRTCVoiceClient::setMuteMic(bool muted)
{
	if(mMuteMic != muted)
	{
		mMuteMic = muted;
		mMuteMicDirty = true;
	}
}
bool LLWebRTCVoiceClient::voiceEnabled()
{
	static const LLCachedControl<bool> enable_voice_chat("EnableVoiceChat",true);
	static const LLCachedControl<bool> cmdline_disable_voice("CmdLineDisableVoice",false);
	return enable_voice_chat && !cmdline_disable_voice;
}

void LLWebRTCVoiceClient::setLipSyncEnabled(BOOL enabled)
{
	mLipSyncEnabled = enabled;
}

BOOL LLWebRTCVoiceClient::lipSyncEnabled()
{

	if ( mVoiceEnabled && stateDisabled != getState() )
	{
		return mLipSyncEnabled;
	}
	else
	{
		return FALSE;
	}
}

void LLWebRTCVoiceClient::setNonFriendsVoiceAttenuation(float volume) {
	
	if (mNonFriendsVoiceAttenuation == volume)
		return;
	LL_INFOS() << "setting Non Friends Voice Attenuation to " << volume << LL_ENDL;	
	mNonFriendsVoiceAttenuation=volume;
	if (!mAudioSession) return;
	participantList::iterator iter = mAudioSession->mParticipantList.begin();

		
	
	for(; iter != mAudioSession->mParticipantList.end(); iter++)
	{
		participantState *p = &*iter;
		
		if (p) {
			bool isBuddy = LLAvatarTracker::instance().isBuddy(p->mAvatarID);
			if (p->isBuddy != isBuddy) {
				p->isBuddy=isBuddy;
				
				p->mVolumeDirty=true;
				
			}
			if (!p->isBuddy)
				p->mVolumeDirty=true;
			
			
			
			
			
		}


		

	}
	
	mAudioSession->mNonFriendsAttenuationLevelDirty = true;
	
	
}
void LLWebRTCVoiceClient::setVoiceVolume(F32 volume)
{
	int scaled_volume = scale_speaker_volume(volume);

	if(scaled_volume != mSpeakerVolume)
	{
		int min_volume = scale_speaker_volume(0);
		if((scaled_volume == min_volume) || (mSpeakerVolume == min_volume))
		{
			mSpeakerMuteDirty = true;
		}

		mSpeakerVolume = scaled_volume;
		mSpeakerVolumeDirty = true;
	}
}

BOOL LLWebRTCVoiceClient::getOnMuteList(const LLUUID& id)
{
	BOOL result = FALSE;

	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		result = participant->mOnMuteList;
	}

	return result;
}
void LLWebRTCVoiceClient::setUserVolume(const LLUUID& id, F32 volume)
{
	if(mAudioSession)
	{
		participantState *participant = findParticipantByID(id);
		if (participant && !participant->mIsSelf)
		{
			if (!is_approx_equal(volume, LLVoiceClient::getInstance()->getDefaultBoostlevel()))
			{
				// Store this volume setting for future sessions if it has been
				// changed from the default
				LLSpeakerVolumeStorage::getInstance()->storeSpeakerVolume(id, volume);
			}
			else
			{
				// Remove stored volume setting if it is returned to the default
				LLSpeakerVolumeStorage::getInstance()->removeSpeakerVolume(id);
			}

			participant->mVolume = llclamp(volume, LLVoiceClient::VOLUME_MIN, LLVoiceClient::VOLUME_MAX);
			participant->mVolumeDirty = true;
			mAudioSession->mVolumeDirty = true;
		}
	}
}
F32 LLWebRTCVoiceClient::getUserVolume(const LLUUID& id)
{
	// Minimum volume will be returned for users with voice disabled
	F32 result = LLVoiceClient::VOLUME_MIN;

	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		result = participant->mVolume;
		
		// Enable this when debugging voice slider issues.  It's way to spammy even for debug-level logging.
		// LL_DEBUGS("Voice") << "mVolume = " << result <<  " for " << id << LL_ENDL;
	}

	return result;
}
/////////////////////////////
// Accessors for data related to nearby speakers
BOOL LLWebRTCVoiceClient::getVoiceEnabled(const LLUUID& id)
{
	BOOL result = FALSE;
	participantState *participant = findParticipantByID(id);
	if(participant)
	{
		// I'm not sure what the semantics of this should be.
		// For now, if we have any data about the user that came through the chat channel, assume they're voice-enabled.
		result = TRUE;
	}

	return result;
}
void LLWebRTCVoiceClient::setVoiceEnabled(bool enabled)
{
	if (enabled != mVoiceEnabled)
	{
		// TODO: Refactor this so we don't call into LLVoiceChannel, but simply
		// use the status observer
		mVoiceEnabled = enabled;
		LLVoiceClientStatusObserver::EStatusType status;


		if (enabled)
		{
			LLVoiceChannel::getCurrentVoiceChannel()->activate();
			status = LLVoiceClientStatusObserver::STATUS_VOICE_ENABLED;
		}
		else
		{
			// Turning voice off looses your current channel -- this makes sure the UI isn't out of sync when you re-enable it.
			LLVoiceChannel::getCurrentVoiceChannel()->deactivate();
			status = LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED;
		}
		notifyStatusObservers(status);
	}
}
void LLWebRTCVoiceClient::leaveChannel(void)
{
	if(getState() == stateRunning)
	{
		LL_DEBUGS("Voice") << "leaving channel for teleport/logout" << LL_ENDL;
		mChannelName.clear();
		sessionTerminate();
	}
}
std::string LLWebRTCVoiceClient::getCurrentChannel()
{
	std::string result;

	if((getState() == stateRunning) && !mSessionTerminateRequested)
	{
		result = getAudioSessionURI();
	}

	return result;
}
void LLWebRTCVoiceClient::deleteSession(sessionState *session)
{
	// Remove the session from the handle map
	if(!session->mHandle.empty())
	{
		sessionMap::iterator iter = mSessionsByHandle.find(session->mHandle);
		if(iter != mSessionsByHandle.end())
		{
			if(iter->second != session)
			{
				LL_ERRS("Voice") << "Internal error: session mismatch" << LL_ENDL;
			}
			mSessionsByHandle.erase(iter);
		}
	}

	// Remove the session from the URI map
	mSessions.erase(session);

	// At this point, the session should be unhooked from all lists and all state should be consistent.
	verifySessionState();

	// If this is the current audio session, clean up the pointer which will soon be dangling.
	if(mAudioSession == session)
	{
		mAudioSession = NULL;
		mAudioSessionChanged = true;
	}

	// ditto for the next audio session
	if(mNextAudioSession == session)
	{
		mNextAudioSession = NULL;
	}

	// delete the session
	delete session;
}
void LLWebRTCVoiceClient::callUser(const LLUUID &uuid)
{
	std::string userURI = sipURIFromID(uuid);

	switchChannel(userURI, false, true, true);
}
LLWebRTCVoiceClient::sessionStatePtr_t LLWebRTCVoiceClient::findP2PSession(const LLUUID &agent_id)
{
    sessionStatePtr_t result = sessionState::matchSessionByChannelID(agent_id.asString());
    if (result && !result->isSpatial())
    {
        return result;
    }

    result.reset();
    return result;
}
// Returns true if calling back the session URI after the session has closed is possible.
// Currently this will be false only for PSTN P2P calls.
BOOL LLWebRTCVoiceClient::isSessionCallBackPossible(const LLUUID &session_id)
{
	sessionStatePtr_t session(findP2PSession(session_id));
    return session && session->isCallbackPossible() ? TRUE : FALSE;
}
void LLWebRTCVoiceClient::endUserIMSession(const LLUUID &uuid)
{
	// Not implemented
}
BOOL LLWebRTCVoiceClient::sendTextMessage(const LLUUID& participant_id, const std::string& message)
{
	//not implemented
	return true;
}
bool LLWebRTCVoiceClient::sessionState::isTextIMPossible()
{
	// This may change to be explicitly specified by vivox in the future...
	return !mSynthesizedCallerID;
}
// Returns true if the session can accepte text IM's.
// Currently this will be false only for PSTN P2P calls.
BOOL LLWebRTCVoiceClient::isSessionTextIMPossible(const LLUUID &session_id)
{
	bool result = TRUE;
	sessionState *session = findSession(session_id);

	if(session != NULL)
	{
		result = session->isTextIMPossible();
	}

	return result;
}
void LLWebRTCVoiceClient::reapSession(sessionState *session)
{
	if(session)
	{
		if(!session->mHandle.empty())
		{
			LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI << " (non-null session handle)" << LL_ENDL;
		}
		else if(session->mCreateInProgress)
		{
			LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI << " (create in progress)" << LL_ENDL;
		}
		else if(session->mMediaConnectInProgress)
		{
			LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI << " (connect in progress)" << LL_ENDL;
		}
		else if(session == mAudioSession)
		{
			LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI << " (it's the current session)" << LL_ENDL;
		}
		else if(session == mNextAudioSession)
		{
			LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI << " (it's the next session)" << LL_ENDL;
		}
		else
		{
			// TODO: Question: Should we check for queued text messages here?
			// We don't have a reason to keep tracking this session, so just delete it.
			LL_DEBUGS("Voice") << "deleting session " << session->mSIPURI << LL_ENDL;
			deleteSession(session);
			session = NULL;
		}
	}
	else
	{
//		LL_DEBUGS("Voice") << "session is NULL" << LL_ENDL;
	}
}
void LLWebRTCVoiceClient::leaveNonSpatialChannel()
{
	LL_DEBUGS("Voice")
		<< "called in state " << state2string(getState())
		<< LL_ENDL;

	// Make sure we don't rejoin the current session.
	sessionState *oldNextSession = mNextAudioSession;
	mNextAudioSession = NULL;

	// Most likely this will still be the current session at this point, but check it anyway.
	reapSession(oldNextSession);

	verifySessionState();

	sessionTerminate();
}
void LLWebRTCVoiceClient::setSessionURI(sessionState *session, const std::string &uri)
{
	// There used to be a map of session URIs to sessions, which made this complex....
	session->mSIPURI = uri;

	verifySessionState();
}
/*static*/
LLWebRTCVoiceClient::sessionState::ptr_t LLWebRTCVoiceClient::sessionState::matchSessionByChannelID(const std::string& channel_id)
{
    sessionStatePtr_t result;

    // *TODO: My kingdom for a lambda!
    std::map<std::string, ptr_t>::iterator it = mSessions.find(channel_id);
    if (it != mSessions.end())
    {
        result = (*it).second;
    }
    return result;
}
//------------------------------------------------------------------------
// Sessions

std::map<std::string, LLWebRTCVoiceClient::sessionState::ptr_t> LLWebRTCVoiceClient::sessionState::mSessions;

/*static*/
void LLWebRTCVoiceClient::sessionState::addSession(
    const std::string & channelID,
    LLWebRTCVoiceClient::sessionState::ptr_t& session)
{
    mSessions[channelID] = session;
}

LLWebRTCVoiceClient::sessionStatePtr_t LLWebRTCVoiceClient::addSession(const std::string &channel_id, sessionState::ptr_t session)
{
    sessionStatePtr_t existingSession = sessionState::matchSessionByChannelID(channel_id);
    if (!existingSession)
    {
        // No existing session found.

        LL_DEBUGS("Voice") << "adding new session with channel: " << channel_id << LL_ENDL;
        session->setMuteMic(mMuteMic);
        session->setMicGain(mMicGain);
        session->setSpeakerVolume(mSpeakerVolume);

        sessionState::addSession(channel_id, session);
        return session;
    }
    else
    {
        // Found an existing session
        LL_DEBUGS("Voice") << "Attempting to add already-existing session " << channel_id << LL_ENDL;
        existingSession->revive();

        return existingSession;
    }
}
void LLWebRTCVoiceClient::switchChannel(
	std::string uri,
	bool spatial,
	bool no_reconnect,
	bool is_p2p,
	std::string hash)
{
	// bool needsSwitch = false;

	// LL_DEBUGS("Voice")
	// 	<< "called in state " << state2string(getState())
	// 	<< " with uri \"" << uri << "\""
	// 	<< (spatial?", spatial is true":", spatial is false")
	// 	<< LL_ENDL;

	// switch(getState())
	// {
	// 	case stateJoinSessionFailed:
	// 	case stateJoinSessionFailedWaiting:
	// 	case stateNoChannel:
	// 	case stateRetrievingParcelVoiceInfo:
	// 		// Always switch to the new URI from these states.
	// 		needsSwitch = true;
	// 	break;

	// 	default:
	// 		if(mSessionTerminateRequested)
	// 		{
	// 			// If a terminate has been requested, we need to compare against where the URI we're already headed to.
	// 			if(mNextAudioSession)
	// 			{
	// 				if(mNextAudioSession->mSIPURI != uri)
	// 					needsSwitch = true;
	// 			}
	// 			else
	// 			{
	// 				// mNextAudioSession is null -- this probably means we're on our way back to spatial.
	// 				if(!uri.empty())
	// 				{
	// 					// We do want to process a switch in this case.
	// 					needsSwitch = true;
	// 				}
	// 			}
	// 		}
	// 		else
	// 		{
	// 			// Otherwise, compare against the URI we're in now.
	// 			if(mAudioSession)
	// 			{
	// 				if(mAudioSession->mSIPURI != uri)
	// 				{
	// 					needsSwitch = true;
	// 				}
	// 			}
	// 			else
	// 			{
	// 				if(!uri.empty())
	// 				{
	// 					// mAudioSession is null -- it's not clear what case would cause this.
	// 					// For now, log it as a warning and see if it ever crops up.
	// 					LL_WARNS("Voice") << "No current audio session." << LL_ENDL;
	// 				}
	// 			}
	// 		}
	// 	break;
	// }

	// if(needsSwitch)
	// {
	// 	if(uri.empty())
	// 	{
	// 		// Leave any channel we may be in
	// 		LL_DEBUGS("Voice") << "leaving channel" << LL_ENDL;

	// 		sessionState *oldSession = mNextAudioSession;
	// 		mNextAudioSession = NULL;

	// 		// The old session may now need to be deleted.
	// 		reapSession(oldSession);

	// 		notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED);
	// 	}
	// 	else
	// 	{
	// 		LL_DEBUGS("Voice") << "switching to channel " << uri << LL_ENDL;

	// 		mNextAudioSession = addSession(uri);
	// 		mNextAudioSession->mHash = hash;
	// 		mNextAudioSession->mIsSpatial = spatial;
	// 		mNextAudioSession->mReconnect = !no_reconnect;
	// 		mNextAudioSession->mIsP2P = is_p2p;
	// 	}

	// 	if(getState() >= stateRetrievingParcelVoiceInfo)
	// 	{
	// 		// If we're already in a channel, or if we're joining one, terminate
	// 		// so we can rejoin with the new session data.
	// 		sessionTerminate();
	// 	}
	// }
}
void LLWebRTCVoiceClient::setNonSpatialChannel(
	const std::string &uri,
	const std::string &credentials)
{
	switchChannel(uri, false, false, false, credentials);
}

void LLWebRTCVoiceClient::setSpatialChannel(
	const std::string &uri,
	const std::string &credentials)
{
	mSpatialSessionURI = uri;
	mSpatialSessionCredentials = credentials;
	mAreaVoiceDisabled = mSpatialSessionURI.empty();

	LL_DEBUGS("Voice") << "got spatial channel uri: \"" << uri << "\"" << LL_ENDL;

	if((mAudioSession && !(mAudioSession->mIsSpatial)) || (mNextAudioSession && !(mNextAudioSession->mIsSpatial)))
	{
		// User is in a non-spatial chat or joining a non-spatial chat.  Don't switch channels.
		LL_INFOS("Voice") << "in non-spatial chat, not switching channels" << LL_ENDL;
	}
	else
	{
		switchChannel(mSpatialSessionURI, true, false, false, mSpatialSessionCredentials);
	}
}
bool LLWebRTCVoiceClient::inSpatialChannel(void)
{
	bool result = false;

	if(mAudioSession)
		result = mAudioSession->mIsSpatial;

	return result;
}
std::string LLWebRTCVoiceClient::getAudioSessionURI()
{
	std::string result;

	if(mAudioSession)
		result = mAudioSession->mSIPURI;

	return result;
}
void LLWebRTCVoiceClient::addObserver(LLVoiceClientParticipantObserver* observer)
{
	mParticipantObservers.insert(observer);
}

void LLWebRTCVoiceClient::removeObserver(LLVoiceClientParticipantObserver* observer)
{
	mParticipantObservers.erase(observer);
}
void LLWebRTCVoiceClient::addObserver(LLVoiceClientStatusObserver* observer)
{
	mStatusObservers.insert(observer);
}
void LLWebRTCVoiceClient::addObserver(LLFriendObserver* observer)
{
	mFriendObservers.insert(observer);
}

void LLWebRTCVoiceClient::removeObserver(LLFriendObserver* observer)
{
	mFriendObservers.erase(observer);
}
void LLWebRTCVoiceClient::removeObserver(LLVoiceClientStatusObserver* observer)
{
	mStatusObservers.erase(observer);
}
void LLWebRTCVoiceClient::notifyFriendObservers()
{
	for (friend_observer_set_t::iterator it = mFriendObservers.begin();
		it != mFriendObservers.end();
		)
	{
		LLFriendObserver* observer = *it;
		it++;
		// The only friend-related thing we notify on is online/offline transitions.
		observer->changed(LLFriendObserver::ONLINE);
	}
}
void LLWebRTCVoiceClient::notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status)
{
	if(mAudioSession)
	{
		if(status == LLVoiceClientStatusObserver::ERROR_UNKNOWN)
		{
			switch(mAudioSession->mErrorStatusCode)
			{
				case 20713:		status = LLVoiceClientStatusObserver::ERROR_CHANNEL_FULL; 		break;
				case 20714:		status = LLVoiceClientStatusObserver::ERROR_CHANNEL_LOCKED; 	break;
				case 20715:
					//invalid channel, we may be using a set of poorly cached
					//info
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;
					break;
				case 1009:
					//invalid username and password
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;
					break;
			}

			// Reset the error code to make sure it won't be reused later by accident.
			mAudioSession->mErrorStatusCode = 0;
		}
		else if(status == LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL)
		{
			switch(mAudioSession->mErrorStatusCode)
			{
				case HTTP_NOT_FOUND:	// NOT_FOUND
				// *TODO: Should this be 503?
				case 480:	// TEMPORARILY_UNAVAILABLE
				case HTTP_REQUEST_TIME_OUT:	// REQUEST_TIMEOUT
					// call failed because other user was not available
					// treat this as an error case
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;

					// Reset the error code to make sure it won't be reused later by accident.
					mAudioSession->mErrorStatusCode = 0;
				break;
			}
		}
	}

	LL_DEBUGS("Voice")
		<< " " << LLVoiceClientStatusObserver::status2string(status)
		<< ", session URI " << getAudioSessionURI()
		<< (inSpatialChannel()?", proximal is true":", proximal is false")
	<< LL_ENDL;

	for (status_observer_set_t::iterator it = mStatusObservers.begin();
		it != mStatusObservers.end();
		)
	{
		LLVoiceClientStatusObserver* observer = *it;
		observer->onChange(status, getAudioSessionURI(), inSpatialChannel());
		// In case onError() deleted an entry.
		it = mStatusObservers.upper_bound(observer);
	}

	// skipped to avoid speak button blinking
	if (   status != LLVoiceClientStatusObserver::STATUS_JOINING
		&& status != LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL)
	{
		bool voice_status = LLVoiceClient::getInstance()->voiceEnabled() && LLVoiceClient::getInstance()->isVoiceWorking();

		gAgent.setVoiceConnected(voice_status);

		/*if (voice_status)
		{
			LLFirstUse::speak(true);
		}*/
	}
}
void LLWebRTCVoiceClient::sessionState::removeAllParticipants()
{
	LL_DEBUGS("Voice") << "called" << LL_ENDL;

	// Singu Note: mParticipantList has replaced both mParticipantsByURI and mParticipantsByUUID, meaning we don't have two maps to maintain any longer.
	mParticipantList.clear();
	mParticipantList.shrink_to_fit();
}


LLWebRTCVoiceClient::sessionState::sessionState() :
	mErrorStatusCode(0),
	mMediaStreamState(streamStateUnknown),
	mTextStreamState(streamStateUnknown),
	mCreateInProgress(false),
	mMediaConnectInProgress(false),
	mVoiceInvitePending(false),
	mTextInvitePending(false),
	mSynthesizedCallerID(false),
	mIsChannel(false),
	mIsSpatial(false),
	mIsP2P(false),
	mIncoming(false),
	mVoiceEnabled(false),
	mReconnect(false),
	mVolumeDirty(false),
	mMuteDirty(false),
	mNonFriendsAttenuationLevelDirty(false),
	mParticipantsChanged(false)
{
}

LLWebRTCVoiceClient::sessionState::~sessionState()
{
	removeAllParticipants();
}
void LLWebRTCVoiceClient::verifySessionState(void)
{
	// This is mostly intended for debugging problems with session state management.
	LL_DEBUGS("Voice") << "Total session count: " << mSessions.size() << " , session handle map size: " << mSessionsByHandle.size() << LL_ENDL;

	for(sessionIterator iter = sessionsBegin(); iter != sessionsEnd(); iter++)
	{
		sessionState *session = *iter;

		LL_DEBUGS("Voice") << "session " << session << ": handle " << session->mHandle << ", URI " << session->mSIPURI << LL_ENDL;

		if(!session->mHandle.empty())
		{
			// every session with a non-empty handle needs to be in the handle map
			sessionMap::iterator i2 = mSessionsByHandle.find(session->mHandle);
			if(i2 == mSessionsByHandle.end())
			{
				LL_ERRS("Voice") << "internal error (handle " << session->mHandle << " not found in session map)" << LL_ENDL;
			}
			else
			{
				if(i2->second != session)
				{
					LL_ERRS("Voice") << "internal error (handle " << session->mHandle << " in session map points to another session)" << LL_ENDL;
				}
			}
		}
	}

	// check that every entry in the handle map points to a valid session in the session set
	for(sessionMap::iterator iter = mSessionsByHandle.begin(); iter != mSessionsByHandle.end(); iter++)
	{
		sessionState *session = iter->second;
		sessionIterator i2 = mSessions.find(session);
		if(i2 == mSessions.end())
		{
			LL_ERRS("Voice") << "internal error (session for handle " << session->mHandle << " not found in session map)" << LL_ENDL;
		}
		else
		{
			if(session->mHandle != (*i2)->mHandle)
			{
				LL_ERRS("Voice") << "internal error (session for handle " << session->mHandle << " points to session with different handle " << (*i2)->mHandle << ")" << LL_ENDL;
			}
		}
	}
}
void LLWebRTCVoiceClient::setSessionHandle(sessionState *session, const std::string &handle)
{
	// Have to remove the session from the handle-indexed map before changing the handle, or things will break badly.

	if(!session->mHandle.empty())
	{
		// Remove session from the map if it should have been there.
		sessionMap::iterator iter = mSessionsByHandle.find(session->mHandle);
		if(iter != mSessionsByHandle.end())
		{
			if(iter->second != session)
			{
				LL_ERRS("Voice") << "Internal error: session mismatch!" << LL_ENDL;
			}

			mSessionsByHandle.erase(iter);
		}
		else
		{
			LL_ERRS("Voice") << "Internal error: session handle not found in map!" << LL_ENDL;
		}
	}

	session->mHandle = handle;

	if(!handle.empty())
	{
		mSessionsByHandle.insert(sessionMap::value_type(session->mHandle, session));
	}

	verifySessionState();
}


/////////////////////////////
// LLVoiceWebRTCConnection
// These connections manage state transitions, negotiating webrtc connections,
// and other such things for a single connection to a Secondlife WebRTC server.
// Multiple of these connections may be active at once, in the case of
// cross-region voice, or when a new connection is being created before the old
// has a chance to shut down.
LLVoiceWebRTCConnection::LLVoiceWebRTCConnection(const LLUUID &regionID, const std::string &channelID) :
    mWebRTCAudioInterface(nullptr),
    mWebRTCDataInterface(nullptr),
    mVoiceConnectionState(VOICE_STATE_START_SESSION),
    mCurrentStatus(LLVoiceClientStatusObserver::STATUS_VOICE_ENABLED),
    mMuted(true),
    mShutDown(false),
    mIceCompleted(false),
    mSpeakerVolume(0.0),
    mMicGain(0.0),
    mOutstandingRequests(0),
    mChannelID(channelID),
    mRegionID(regionID),
    mRetryWaitPeriod(0)
{
	LL_INFOS() << "WebRTC : Connection constructor" << LL_ENDL;
    // retries wait a short period...randomize it so
    // all clients don't try to reconnect at once.
    mRetryWaitSecs = ((F32) rand() / (RAND_MAX)) + 0.5;

    mWebRTCPeerConnectionInterface = llwebrtc::newPeerConnection();
    mWebRTCPeerConnectionInterface->setSignalingObserver(this);
    //mMainQueue = LL::WorkQueue::getInstance("mainloop");
}

LLVoiceWebRTCConnection::~LLVoiceWebRTCConnection()
{
    if (LLWebRTCVoiceClient::isShuttingDown())
    {
        // peer connection and observers will be cleaned up
        // by llwebrtc::terminate() on shutdown.
        return;
    }
    if (mWebRTCPeerConnectionInterface)
    {
        llwebrtc::freePeerConnection(mWebRTCPeerConnectionInterface);
        mWebRTCPeerConnectionInterface = nullptr;
    }
}
// Primary state machine for negotiating a single voice connection to the
// Secondlife WebRTC server.
bool LLVoiceWebRTCConnection::connectionStateMachine()
{
	LL_INFOS() << "WebRTC : Connection connectionStateMachine" << LL_ENDL;
    // processIceUpdates();

    // switch (getVoiceConnectionState())
    // {
    //     case VOICE_STATE_START_SESSION:
    //     {
    //         LL_PROFILE_ZONE_NAMED_CATEGORY_VOICE("VOICE_STATE_START_SESSION")
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //             break;
    //         }
    //         mIceCompleted = false;
    //         setVoiceConnectionState(VOICE_STATE_WAIT_FOR_SESSION_START);
    //         // tell the webrtc library that we want a connection.  The library will
    //         // respond with an offer on a separate thread, which will cause
    //         // the session state to change.
    //         if (!mWebRTCPeerConnectionInterface->initializeConnection())
    //         {
    //             setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
    //         }
    //         break;
    //     }

    //     case VOICE_STATE_WAIT_FOR_SESSION_START:
    //     {
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //         }
    //         break;
    //     }

    //     case VOICE_STATE_REQUEST_CONNECTION:
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //             break;
    //         }
    //         // Ask the sim to ask the Secondlife WebRTC server for a connection to
    //         // a given voice channel.  On completion, we'll move on to the
    //         // VOICE_STATE_SESSION_ESTABLISHED via a callback on a webrtc thread.
    //         setVoiceConnectionState(VOICE_STATE_CONNECTION_WAIT);
    //         LLCoros::getInstance()->launch("LLVoiceWebRTCConnection::requestVoiceConnectionCoro",
    //                                        boost::bind(&LLVoiceWebRTCConnection::requestVoiceConnectionCoro, this));
    //         break;

    //     case VOICE_STATE_CONNECTION_WAIT:
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //         }
    //         break;

    //     case VOICE_STATE_SESSION_ESTABLISHED:
    //     {
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //             break;
    //         }
    //         // update the peer connection with the various characteristics of
    //         // this connection.
    //         mWebRTCAudioInterface->setMute(mMuted);
    //         mWebRTCAudioInterface->setReceiveVolume(mSpeakerVolume);
    //         mWebRTCAudioInterface->setSendVolume(mMicGain);
    //         LLWebRTCVoiceClient::getInstance()->OnConnectionEstablished(mChannelID, mRegionID);
    //         setVoiceConnectionState(VOICE_STATE_WAIT_FOR_DATA_CHANNEL);
    //         break;
    //     }

    //     case VOICE_STATE_WAIT_FOR_DATA_CHANNEL:
    //     {
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //             break;
    //         }
    //         if (mWebRTCDataInterface) // the interface will be set when the session is negotiated.
    //         {
    //             sendJoin();  // tell the Secondlife WebRTC server that we're here via the data channel.
    //             setVoiceConnectionState(VOICE_STATE_SESSION_UP);
    //             LLWebRTCVoiceClient::getInstance()->sendPositionUpdate(true);
    //         }
    //         break;
    //     }

    //     case VOICE_STATE_SESSION_UP:
    //     {
    //         mRetryWaitPeriod = 0;
    //         mRetryWaitSecs   = ((F32) rand() / (RAND_MAX)) + 0.5;

    //         // we'll stay here as long as the session remains up.
    //         if (mShutDown)
    //         {
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //         }
    //         break;
    //     }

    //     case VOICE_STATE_SESSION_RETRY:
    //         // only retry ever 'n' seconds
    //         if (mRetryWaitPeriod++ * UPDATE_THROTTLE_SECONDS > mRetryWaitSecs)
    //         {
    //             // something went wrong, so notify that the connection has failed.
    //             LLWebRTCVoiceClient::getInstance()->OnConnectionFailure(mChannelID, mRegionID, mCurrentStatus);
    //             setVoiceConnectionState(VOICE_STATE_DISCONNECT);
    //             mRetryWaitPeriod = 0;
    //             if (mRetryWaitSecs < MAX_RETRY_WAIT_SECONDS)
    //             {
    //                 // back off the retry period, and do it by a small random
    //                 // bit so all clients don't reconnect at once.
    //                 mRetryWaitSecs += ((F32) rand() / (RAND_MAX)) + 0.5;
    //                 mRetryWaitPeriod = 0;
    //             }
    //         }
    //         break;

    //     case VOICE_STATE_DISCONNECT:
    //         LLCoros::instance().launch("LLVoiceWebRTCConnection::breakVoiceConnectionCoro",
    //                                    boost::bind(&LLVoiceWebRTCConnection::breakVoiceConnectionCoro, this));
    //         break;

    //     case VOICE_STATE_WAIT_FOR_EXIT:
    //         break;

    //     case VOICE_STATE_SESSION_EXIT:
    //     {
    //         {
    //             if (!mShutDown)
    //             {
    //                 mVoiceConnectionState = VOICE_STATE_START_SESSION;
    //             }
    //             else
    //             {
    //                 // if we still have outstanding http or webrtc calls, wait for them to
    //                 // complete so we don't delete objects while they still may be used.
    //                 if (mOutstandingRequests <= 0)
    //                 {
    //                     LLWebRTCVoiceClient::getInstance()->OnConnectionShutDown(mChannelID, mRegionID);
    //                     return false;
    //                 }
    //             }
    //         }
    //         break;
    //     }

    //     default:
    //     {
    //         LL_WARNS("Voice") << "Unknown voice control state " << getVoiceConnectionState() << LL_ENDL;
    //         return false;
    //     }
    // }
    return true;
}
LLWebRTCVoiceClient::estateSessionState::estateSessionState()
{
    mHangupOnLastLeave = false;
    mNotifyOnFirstJoin = false;
    mChannelID         = "Estate";
    LLUUID region_id   = gAgent.getRegion()->getRegionID();

    mWebRTCConnections.emplace_back(new LLVoiceWebRTCSpatialConnection(region_id, INVALID_PARCEL_ID, "Estate"));
}
// processing of spatial voice connection states requires special handling.
// as neighboring regions need to be started up or shut down depending
// on our location.
bool LLWebRTCVoiceClient::estateSessionState::processConnectionStates()
{
    
    if (!sShuttingDown)
    {
        // Estate voice requires connection to neighboring regions.
        std::set<LLUUID> neighbor_ids = LLWebRTCVoiceClient::getInstance()->getNeighboringRegions();

        for (auto &connection : mWebRTCConnections)
        {
            boost::shared_ptr<LLVoiceWebRTCSpatialConnection> spatialConnection =
                boost::static_pointer_cast<LLVoiceWebRTCSpatialConnection>(connection);

            LLUUID regionID = spatialConnection.get()->getRegionID();

            if (neighbor_ids.find(regionID) == neighbor_ids.end())
            {
                // shut down connections to neighbors that are too far away.
                spatialConnection.get()->shutDown();
            }
            neighbor_ids.erase(regionID);
        }

        // add new connections for new neighbors
        for (auto &neighbor : neighbor_ids)
        {
            connectionPtr_t connection(new LLVoiceWebRTCSpatialConnection(neighbor, INVALID_PARCEL_ID, mChannelID));

            mWebRTCConnections.push_back(connection);
            connection->setMicGain(mMicGain);
            connection->setMuteMic(mMuted);
            connection->setSpeakerVolume(mSpeakerVolume);
        }
    }
    return LLWebRTCVoiceClient::sessionState::processConnectionStates();
}
/////////////////////////////
// WebRTC Spatial Connection

LLVoiceWebRTCSpatialConnection::LLVoiceWebRTCSpatialConnection(const LLUUID &regionID,
                                                               S32 parcelLocalID,
                                                               const std::string &channelID) :
    LLVoiceWebRTCConnection(regionID, channelID),
    mParcelLocalID(parcelLocalID)
{
}

LLVoiceWebRTCSpatialConnection::~LLVoiceWebRTCSpatialConnection()
{
    if (LLWebRTCVoiceClient::isShuttingDown())
    {
        // peer connection and observers will be cleaned up
        // by llwebrtc::terminate() on shutdown.
        return;
    }
    assert(mOutstandingRequests == 0);
    mWebRTCPeerConnectionInterface->unsetSignalingObserver(this);
}

void LLVoiceWebRTCSpatialConnection::setMuteMic(bool muted)
{
    if (mMuted != muted)
    {
        mMuted = muted;
        if (mWebRTCAudioInterface)
        {
            LLViewerRegion *regionp = gAgent.getRegion();
            if (regionp && mRegionID == regionp->getRegionID())
            {
                mWebRTCAudioInterface->setMute(muted);
            }
            else
            {
                // Always mute this agent with respect to neighboring regions.
                // Peers don't want to hear this agent from multiple regions
                // as that'll echo.
                mWebRTCAudioInterface->setMute(true);
            }
        }
    }
}
void LLVoiceWebRTCConnection::setMuteMic(bool muted)
{
    mMuted = muted;
    if (mWebRTCAudioInterface)
    {
        mWebRTCAudioInterface->setMute(muted);
    }
}

void LLVoiceWebRTCConnection::setMicGain(F32 gain)
{
    mMicGain = gain;
    if (mWebRTCAudioInterface)
    {
        mWebRTCAudioInterface->setSendVolume(gain);
    }
}
void LLVoiceWebRTCConnection::setSpeakerVolume(F32 volume)
{
    mSpeakerVolume = volume;
    if (mWebRTCAudioInterface)
    {
        mWebRTCAudioInterface->setReceiveVolume(volume);
    }
}
// Tell the simulator to tell the Secondlife WebRTC server that we want a voice
// connection. The SDP is sent up as part of this, and the simulator will respond
// with an 'answer' which is in the form of another SDP.  The webrtc library
// will use the offer and answer to negotiate the session.
void LLVoiceWebRTCSpatialConnection::requestVoiceConnection()
{

    LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(mRegionID);

    LL_DEBUGS("Voice") << "Requesting voice connection." << LL_ENDL;
    if (!regionp || !regionp->capabilitiesReceived())
    {
        LL_DEBUGS("Voice") << "no capabilities for voice provisioning; waiting " << LL_ENDL;
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
    if (url.empty())
    {
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    LL_DEBUGS("Voice") << "region ready for voice provisioning; url=" << url << LL_ENDL;

    //LLVoiceWebRTCStats::getInstance()->provisionAttemptStart();
    LLSD body;
    LLSD jsep;
    jsep["type"] = "offer";
    jsep["sdp"] = mChannelSDP;
    body["jsep"] = jsep;
    if (mParcelLocalID != INVALID_PARCEL_ID)
    {
        body["parcel_local_id"] = mParcelLocalID;
    }
    body["channel_type"]      = "local";
    body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;
    // LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
    //     new LLCoreHttpUtil::HttpCoroutineAdapter("LLVoiceWebRTCAdHocConnection::requestVoiceConnection",
    //                                              LLCore::HttpRequest::DEFAULT_POLICY_ID));
    // LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    // LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

    // httpOpts->setWantHeaders(true);

	
    LLHTTPClient::post(url, body, new LLCoroResponder(
            boost::bind(&LLVoiceWebRTCSpatialConnection::OnVoiceConnectionRequestSuccess, this, _1)));
    mOutstandingRequests++;
    // LLSD result = httpAdapter->postAndSuspend(httpRequest, url, body, httpOpts);

    // LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    // LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    // if (status)
    // {
    //     OnVoiceConnectionRequestSuccess(result);
    // }
    // else
    // {
    //     switch (status.getType())
    //     {
    //         case HTTP_CONFLICT:
    //             mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_FULL;
    //             break;
    //         case HTTP_UNAUTHORIZED:
    //             mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_LOCKED;
    //             break;
    //         default:
    //             mCurrentStatus = LLVoiceClientStatusObserver::ERROR_UNKNOWN;
    //             break;
    //     }
    //     setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
    // }
    // mOutstandingRequests--;
}
void LLVoiceWebRTCConnection::OnVoiceConnectionRequestSuccess(const LLCoroResponder& responder)
{
	 LLSD result = responder.getContent();

	LL_INFOS() << "OnVoiceConnectionRequestSuccess" << result.asString() << LL_ENDL;
    // if (LLWebRTCVoiceClient::isShuttingDown())
    // {
    //     return;
    // }
    // //LLVoiceWebRTCStats::getInstance()->provisionAttemptEnd(true);

    // if (result.has("viewer_session") &&
    //     result.has("jsep") &&
    //     result["jsep"].has("type") &&
    //     result["jsep"]["type"] == "answer" &&
    //     result["jsep"].has("sdp"))
    // {
    //     mRemoteChannelSDP = result["jsep"]["sdp"].asString();
    //     mViewerSession    = result["viewer_session"];
    // }
    // else
    // {
    //     LL_WARNS("Voice") << "Invalid voice provision request result:" << result << LL_ENDL;
    //     setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
    //     return;
    // }

    // LL_DEBUGS("Voice") << "ProvisionVoiceAccountRequest response"
    //                    << " channel sdp " << mRemoteChannelSDP << LL_ENDL;
    // mWebRTCPeerConnectionInterface->AnswerAvailable(mRemoteChannelSDP);
}

// ICE (Interactive Connectivity Establishment)
// When WebRTC tries to negotiate a connection to the Secondlife WebRTC Server,
// the negotiation will result in a few updates about the best path
// to which to connect.
// The Secondlife servers are configured for ICE trickling, where, after a session is partially
// negotiated, updates about the best connectivity paths may trickle in.  These need to be
// sent to the Secondlife WebRTC server via the simulator so that both sides have a clear
// view of the network environment.

// callback from llwebrtc
void LLVoiceWebRTCConnection::OnIceGatheringState(llwebrtc::LLWebRTCSignalingObserver::EIceGatheringState state)
{
    

	switch (state)
	{
		case llwebrtc::LLWebRTCSignalingObserver::EIceGatheringState::ICE_GATHERING_COMPLETE:
		{
			mIceCompleted = true;
			break;
		}
		case llwebrtc::LLWebRTCSignalingObserver::EIceGatheringState::ICE_GATHERING_NEW:
		{
			mIceCompleted = false;
		}
		default:
			break;
	}
        
}

// callback from llwebrtc
void LLVoiceWebRTCConnection::OnIceCandidate(const llwebrtc::LLWebRTCIceCandidate& candidate)
{
	LL_INFOS() << "LLVoiceWebRTCConnection::OnIceCandidate()" <<LL_ENDL;
    mIceCandidates.push_back(candidate); 
}

void LLVoiceWebRTCConnection::processIceUpdates()
{
    mOutstandingRequests++;
    processIceUpdatesCoro();
}
// Ice candidates may be streamed in before or after the SDP offer is available (see below)
// This function determines whether candidates are available to send to the Secondlife WebRTC
// server via the simulator.  If so, and there are no more candidates, this code
// will make the cap call to the server sending up the ICE candidates.
void LLVoiceWebRTCConnection::processIceUpdatesCoro()
{
	//TODO
	LL_INFOS() << "LLVoiceWebRTCConnection::processIceUpdatesCoro()" <<LL_ENDL;
//    if (mShutDown || LLWebRTCVoiceClient::isShuttingDown())
//     {
//         mOutstandingRequests--;
//         return;
//     }

//     bool iceCompleted = false;
//     LLSD body;
//     if (!mIceCandidates.empty() || mIceCompleted)
//     {
//         LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(mRegionID);
//         if (!regionp || !regionp->capabilitiesReceived())
//         {
//             LL_DEBUGS("Voice") << "no capabilities for ice gathering; waiting " << LL_ENDL;
//             mOutstandingRequests--;
//             return;
//         }

//         std::string url = regionp->getCapability("VoiceSignalingRequest");
//         if (url.empty())
//         {
//             mOutstandingRequests--;
//             return;
//         }

//         LL_DEBUGS("Voice") << "region ready to complete voice signaling; url=" << url << LL_ENDL;
//         if (!mIceCandidates.empty())
//         {
//             LLSD candidates = LLSD::emptyArray();
//             for (auto &ice_candidate : mIceCandidates)
//             {
//                 LLSD body_candidate;
//                 body_candidate["sdpMid"]        = ice_candidate.mSdpMid;
//                 body_candidate["sdpMLineIndex"] = ice_candidate.mMLineIndex;
//                 body_candidate["candidate"]     = ice_candidate.mCandidate;
//                 candidates.append(body_candidate);
//             }
//             body["candidates"] = candidates;
//             mIceCandidates.clear();
//         }
//         else if (mIceCompleted)
//         {
//             LLSD body_candidate;
//             body_candidate["completed"] = true;
//             body["candidate"]           = body_candidate;
//             iceCompleted                = mIceCompleted;
//             mIceCompleted               = false;
//         }

//         body["viewer_session"]    = mViewerSession;
//         body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;

//         LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
//             new LLCoreHttpUtil::HttpCoroutineAdapter("LLVoiceWebRTCAdHocConnection::processIceUpdatesCoro",
//                                                         LLCore::HttpRequest::DEFAULT_POLICY_ID));
//         LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
//         LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

//         httpOpts->setWantHeaders(true);

//         LLSD result = httpAdapter->postAndSuspend(httpRequest, url, body, httpOpts);

//         if (LLWebRTCVoiceClient::isShuttingDown())
//         {
//             return;
//         }

//         LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
//         LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

//         if (!status)
//         {
//             // couldn't trickle the candidates, so restart the session.
//             setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
//         }
//     }
//     mOutstandingRequests--;
}
// An 'Offer' comes in the form of a SDP (Session Description Protocol)
// which contains all sorts of info about the session, from network paths
// to the type of session (audio, video) to characteristics (the encoder type.)
// This SDP also serves as the 'ticket' to the server, security-wise.
// The Offer is retrieved from the WebRTC library on the client,
// and is passed to the simulator via a CAP, which then passes
// it on to the Secondlife WebRTC server.

//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so
// we shouldn't be getting this callback on a nonexistant
// this pointer.

// callback from llwebrtc
void LLVoiceWebRTCConnection::OnOfferAvailable(const std::string &sdp)
{
     
	if (mShutDown)
	{
		return;
	}
	LL_INFOS("Voice") << "On Offer Available." << LL_ENDL;
	mChannelSDP = sdp;
	if (mVoiceConnectionState == VOICE_STATE_WAIT_FOR_SESSION_START)
	{
		mVoiceConnectionState = VOICE_STATE_REQUEST_CONNECTION;
	}
     
}


//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so
// we shouldn't be getting this callback on a nonexistant
// this pointer.
// nor should audio_interface be invalid if the LLWebRTCVoiceConnection
// is shut down.

// callback from llwebrtc
void LLVoiceWebRTCConnection::OnAudioEstablished(llwebrtc::LLWebRTCAudioInterface* audio_interface)
{
    
		if (mShutDown)
		{
			return;
		}
		LL_INFOS("Voice") << "On AudioEstablished." << LL_ENDL;
		mWebRTCAudioInterface = audio_interface;
		setVoiceConnectionState(VOICE_STATE_SESSION_ESTABLISHED);

}


//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so
// we shouldn't be getting this callback on a nonexistant
// this pointer.

// callback from llwebrtc
void LLVoiceWebRTCConnection::OnRenegotiationNeeded()
{
    
	LL_INFOS("Voice") << "Voice channel requires renegotiation." << LL_ENDL;
	if (!mShutDown)
	{
		setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
	}
   
}
// Data has been received on the webrtc data channel
// incoming data will be a json structure (if it's not binary.)  We may pack
// binary for size reasons.  Most of the keys in the json objects are
// single or double characters for size reasons.
// The primary element is:
// An object where each key is an agent id.  (in the future, we may allow
// integer indices into an agentid list, populated on join commands.  For size.
// Each key will point to a json object with keys identifying what's updated.
// 'p'  - audio source power (level/volume) (int8 as int)
// 'j'  - object of join data (currently only a boolean 'p' marking a primary participant)
// 'l'  - boolean, always true if exists.
// 'v'  - boolean - voice activity has been detected.

// llwebrtc callback
void LLVoiceWebRTCConnection::OnDataReceived(const std::string& data, bool binary)
{
    LLVoiceWebRTCConnection::OnDataReceivedImpl(data, binary);
}
//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so 
// we shouldn't be getting this callback on a nonexistant
// this pointer.
void LLVoiceWebRTCConnection::OnDataReceivedImpl(const std::string &data, bool binary)
{
	LL_INFOS() << "LLVoiceWebRTCConnection::OnDataReceivedImpl" << LL_ENDL;
	//TODO
    // if (mShutDown)
    // {
    //     return;
    // }

    // if (binary)
    // {
    //     LL_WARNS("Voice") << "Binary data received from data channel." << LL_ENDL;
    //     return;
    // }

    // Json::Reader reader;
    // Json::Value  voice_data;
    // if (reader.parse(data, voice_data, false))  // don't collect comments
    // {
    //     if (!voice_data.isObject())
    //     {
    //         LL_WARNS("Voice") << "Expected object from data channel:" << data << LL_ENDL;
    //         return;
    //     }
    //     bool new_participant = false;
    //     Json::Value      mute = Json::objectValue;
    //     Json::Value      user_gain = Json::objectValue;
    //     for (auto &participant_id : voice_data.getMemberNames())
    //     {
    //         LLUUID agent_id(participant_id);
    //         if (agent_id.isNull())
    //         {
    //             LL_WARNS("Voice") << "Bad participant ID from data channel (" << participant_id << "):" << data << LL_ENDL;
    //             continue;
    //         }

    //         LLWebRTCVoiceClient::participantStatePtr_t participant =
    //             LLWebRTCVoiceClient::getInstance()->findParticipantByID(mChannelID, agent_id);
    //         bool joined  = false;
    //         bool primary = false;  // we ignore any 'joins' reported about participants
    //                                // that come from voice servers that aren't their primary
    //                                // voice server.  This will happen with cross-region voice
    //                                // where a participant on a neighboring region may be
    //                                // connected to multiple servers.  We don't want to
    //                                // add new identical participants from all of those servers.
    //         if (voice_data[participant_id].isMember("j"))
    //         {
    //             // a new participant has announced that they're joining.
    //             joined  = true;
    //             primary = voice_data[participant_id]["j"].get("p", Json::Value(false)).asBool();

    //             // track incoming participants that are muted so we can mute their connections (or set their volume)
    //             bool isMuted = LLMuteList::getInstance()->isMuted(agent_id, LLMute::flagVoiceChat);
    //             if (isMuted)
    //             {
    //                 mute[participant_id] = true;
    //             }
    //             F32 volume;
    //             if(LLSpeakerVolumeStorage::getInstance()->getSpeakerVolume(agent_id, volume))
    //             {
    //                 user_gain[participant_id] = (uint32_t)(volume * 200);
    //             }
    //         }

    //         new_participant |= joined;
    //         if (!participant && joined && (primary || !isSpatial()))
    //         {
    //             participant = LLWebRTCVoiceClient::getInstance()->addParticipantByID(mChannelID, agent_id);
    //         }

    //         if (participant)
    //         {
    //             if (voice_data[participant_id].get("l", Json::Value(false)).asBool())
    //             {
    //                 // an existing participant is leaving.
    //                 if (agent_id != gAgentID)
    //                 {
    //                     LLWebRTCVoiceClient::getInstance()->removeParticipantByID(mChannelID, agent_id);
    //                 }
    //             }
    //             else
    //             {
    //                 // we got a 'power' update.
    //                 F32 level = (F32) (voice_data[participant_id].get("p", Json::Value(participant->mLevel)).asInt()) / 128;
    //                 // convert to decibles
    //                 participant->mLevel = level;

    //                 if (voice_data[participant_id].isMember("v"))
    //                 {
    //                     participant->mIsSpeaking = voice_data[participant_id].get("v", Json::Value(false)).asBool();
    //                 }
    //             }
    //         }
    //     }

    //     // tell the simulator to set the mute and volume data for this
    //     // participant, if there are any updates.
    //     Json::FastWriter writer;
    //     Json::Value      root     = Json::objectValue;
    //     if (mute.size() > 0)
    //     {
    //         root["m"] = mute;
    //     }
    //     if (user_gain.size() > 0)
    //     {
    //         root["ug"] = user_gain;
    //     }
    //     if (root.size() > 0)
    //     {
    //         std::string json_data = writer.write(root);
    //         mWebRTCDataInterface->sendData(json_data, false);
    //     }
    // }
}
//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so
// we shouldn't be getting this callback on a nonexistant
// this pointer.
// nor should data_interface be invalid if the LLWebRTCVoiceConnection
// is shut down.

// llwebrtc callback
void LLVoiceWebRTCConnection::OnDataChannelReady(llwebrtc::LLWebRTCDataInterface *data_interface)
{

	if (mShutDown)
	{
		return;
	}

	if (data_interface)
	{
		mWebRTCDataInterface = data_interface;
		mWebRTCDataInterface->setDataObserver(this);
	}

}
void LLWebRTCVoiceClient::sessionState::setMuteMic(bool muted)
{
    mMuted = muted;
    for (auto &connection : mWebRTCConnections)
    {
        connection->setMuteMic(muted);
    }
}
void LLWebRTCVoiceClient::sessionState::setMicGain(F32 gain)
{
    mMicGain = gain;
    for (auto &connection : mWebRTCConnections)
    {
        connection->setMicGain(gain);
    }
}

void LLWebRTCVoiceClient::sessionState::setSpeakerVolume(F32 volume)
{
    mSpeakerVolume = volume;
    for (auto &connection : mWebRTCConnections)
    {
        connection->setSpeakerVolume(volume);
    }
}
// in case we drop into a session (spatial, etc.) right after
// telling the session to shut down, revive it so it reconnects.
void LLWebRTCVoiceClient::sessionState::revive()
{
    mShuttingDown = false;
}
//=========================================================================
// the following are methods to support the coroutine implementation of the
// voice connection and processing.  They should only be called in the context
// of a coroutine.
//
//

void LLWebRTCVoiceClient::sessionState::processSessionStates()
{
	LL_INFOS() <<" LLWebRTCVoiceClient::sessionState::processSessionStates()" << LL_ENDL;
    auto iter = mSessions.begin();
    while (iter != mSessions.end())
    {
        if (!iter->second->processConnectionStates() && iter->second->mShuttingDown)
        {
            // if the connections associated with a session are gone,
            // and this session is shutting down, remove it.
            iter = mSessions.erase(iter);
        }
        else
        {
            iter++;
        }
    }
}
// process the states on each connection associated with a session.
bool LLWebRTCVoiceClient::sessionState::processConnectionStates()
{
    
    std::list<connectionPtr_t>::iterator iter = mWebRTCConnections.begin();
    while (iter != mWebRTCConnections.end())
    {
        if (!iter->get()->connectionStateMachine())
        {
            // if the state machine returns false, the connection is shut down
            // so delete it.
            iter = mWebRTCConnections.erase(iter);
        }
        else
        {
            ++iter;
        }
    }
    return !mWebRTCConnections.empty();
}