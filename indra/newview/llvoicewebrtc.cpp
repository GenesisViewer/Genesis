#include "llviewerprecompiledheaders.h"
#include "llvoicewebrtc.h"
bool LLWebRTCVoiceClient::sShuttingDown = false;

// Defines the maximum number of times(in a row) "stateJoiningSession" case for spatial channel is reached in stateMachine()
// which is treated as normal. If this number is exceeded we suspect there is a problem with connection
// to voice server (EXT-4313). When voice works correctly, there is from 1 to 15 times. 50 was chosen
// to make sure we don't make mistake when slight connection problems happen- situation when connection to server is
// blocked is VERY rare and it's better to sacrifice response time in this situation for the sake of stability.
const int MAX_NORMAL_JOINING_SPATIAL_NUM = 50;

LLWebRTCVoiceClient::LLWebRTCVoiceClient(){

    mVoiceVersion.serverVersion = "";
    mVoiceVersion.serverType = "webrtc";

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
	setVoiceEnabled(gSavedSettings.getBOOL("EnableVoiceChat"));
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