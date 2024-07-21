
#include "llviewerprecompiledheaders.h"
#include "llvoicewebrtc.h"
#include "llcallbacklist.h"
#include "llagent.h"
#include "llappviewer.h"	// for gDisconnected, gDisableVoice
#include "llvoavatarself.h"
#include "llviewercamera.h"
#include "llworld.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "json/reader.h"
#include "json/writer.h"
#include "llavatarnamecache.h"
#include "llsdserialize.h"
#include <curl/curl.h>
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
//const S32 INVALID_PARCEL_ID = -1;

// Cosine of a "trivially" small angle
const F32 FOUR_DEGREES = 4.0f * (F_PI / 180.0f);
const F32 MINUSCULE_ANGLE_COS = (F32) cos(0.5f * FOUR_DEGREES);

const F32 MAX_AUDIO_DIST      = 50.0f;
const F32 VOLUME_SCALE_WEBRTC = 0.01f;
const F32 LEVEL_SCALE_WEBRTC  = 0.008f;

const F32 SPEAKING_AUDIO_LEVEL = 0.40;
// Don't send positional updates more frequently than this:
const F32 UPDATE_THROTTLE_SECONDS = 0.1f;
const F32 MAX_RETRY_WAIT_SECONDS  = 10.0f;
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
	LL_INFOS() << "LLWebRTCVoiceClient::init " << LL_ENDL;
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
	LL_INFOS()<< "LLWebRTCVoiceClient::updateSettings()"<<LL_ENDL;
	setVoiceEnabled(gSavedSettings.getBOOL("EnableVoiceChat"));
	setEarLocation(gSavedSettings.getS32("VoiceEarLocation"));

	std::string inputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
	setCaptureDevice(inputDevice);
	std::string outputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");
	setRenderDevice(outputDevice);
	F32 mic_level = gSavedSettings.getF32("AudioLevelMic");
	setMicGain(mic_level);
	setLipSyncEnabled(gSavedSettings.getBOOL("LipSyncEnabled"));
	LL_INFOS()<< "end of LLWebRTCVoiceClient::updateSettings()"<<LL_ENDL;
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


void LLWebRTCVoiceClient::setAvatarPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot)
{
    if (dist_vec_squared(mAvatarPosition, position) > 0.01)
    {
        mAvatarPosition     = position;
        mSpatialCoordsDirty = true;
    }

    if (mAvatarVelocity != velocity)
    {
        mAvatarVelocity     = velocity;
        mSpatialCoordsDirty = true;
    }

    // If the two rotations are not exactly equal test their dot product
    // to get the cos of the angle between them.
    // If it is too small, don't update.
    F32 rot_cos_diff = llabs(dot(mAvatarRot, rot));
    if ((mAvatarRot != rot) && (rot_cos_diff < MINUSCULE_ANGLE_COS))
    {
        mAvatarRot          = rot;
        mSpatialCoordsDirty = true;
    }
}
// For spatial, determine which neighboring regions to connect to
// for cross-region voice.
void LLWebRTCVoiceClient::updateNeighboringRegions()
{
    static const std::vector<LLVector3d> neighbors {LLVector3d(0.0f, 1.0f, 0.0f),  LLVector3d(0.707f, 0.707f, 0.0f),
                                                    LLVector3d(1.0f, 0.0f, 0.0f),  LLVector3d(0.707f, -0.707f, 0.0f),
                                                    LLVector3d(0.0f, -1.0f, 0.0f), LLVector3d(-0.707f, -0.707f, 0.0f),
                                                    LLVector3d(-1.0f, 0.0f, 0.0f), LLVector3d(-0.707f, 0.707f, 0.0f)};

    // Estate voice requires connection to neighboring regions.
    mNeighboringRegions.clear();

    mNeighboringRegions.insert(gAgent.getRegion()->getRegionID());

    // base off of speaker position as it'll move more slowly than camera position.
    // Once we have hysteresis, we may be able to track off of speaker and camera position at 50m
    // TODO: Add hysteresis so we don't flip-flop connections to neighbors
    LLVector3d speaker_pos = LLWebRTCVoiceClient::getInstance()->getSpeakerPosition();
    for (auto &neighbor_pos : neighbors)
    {
        // include every region within 100m (2*MAX_AUDIO_DIST) to deal witht he fact that the camera
        // can stray 50m away from the avatar.
        LLViewerRegion *neighbor = LLWorld::instance().getRegionFromPosGlobal(speaker_pos + 2 * MAX_AUDIO_DIST * neighbor_pos);
        if (neighbor && !neighbor->getRegionID().isNull())
        {
            mNeighboringRegions.insert(neighbor->getRegionID());
        }
    }
}
// The listener (camera) must be within 50m of the
// avatar.  Enforce it on the client.
// This will also be enforced on the voice server
// based on position sent from the simulator to the
// voice server.
void LLWebRTCVoiceClient::enforceTether()
{
    LLVector3d tethered = mListenerRequestedPosition;

    // constrain 'tethered' to within 50m of mAvatarPosition.
    {
        LLVector3d camera_offset   = mListenerRequestedPosition - mAvatarPosition;
        F32        camera_distance = (F32) camera_offset.magVec();
        if (camera_distance > MAX_AUDIO_DIST)
        {
            tethered = mAvatarPosition + (MAX_AUDIO_DIST / camera_distance) * camera_offset;
        }
    }

    if (dist_vec_squared(mListenerPosition, tethered) > 0.01)
    {
        mListenerPosition   = tethered;
        mSpatialCoordsDirty = true;
    }
}
void LLWebRTCVoiceClient::updatePosition(void)
{

	LLViewerRegion *region = gAgent.getRegion();
    if (region && isAgentAvatarValid())
    {
        // get the avatar position.
        LLVector3d   avatar_pos  = gAgentAvatarp->getPositionGlobal();
        LLQuaternion avatar_qrot = gAgentAvatarp->getRootJoint()->getWorldRotation();

        avatar_pos += LLVector3d(0.f, 0.f, 1.f);  // bump it up to head height

        LLVector3d   earPosition;
        LLQuaternion earRot;
        switch (mEarLocation)
        {
            case earLocCamera:
            default:
                earPosition = region->getPosGlobalFromRegion(LLViewerCamera::getInstance()->getOrigin());
                earRot      = LLViewerCamera::getInstance()->getQuaternion();
                break;

            case earLocAvatar:
                earPosition = mAvatarPosition;
                earRot      = mAvatarRot;
                break;

            case earLocMixed:
                earPosition = mAvatarPosition;
                earRot      = LLViewerCamera::getInstance()->getQuaternion();
                break;
        }
        setListenerPosition(earPosition,      // position
                            LLVector3::zero,  // velocity
                            earRot);          // rotation matrix

        setAvatarPosition(avatar_pos,       // position
                          LLVector3::zero,  // velocity
                          avatar_qrot);     // rotation matrix

        enforceTether();

        updateNeighboringRegions();
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
	LL_INFOS() << "LLWebRTCVoiceClient::startEstateSession() leavechannel "  << LL_ENDL;
    mNextSession = addSession("Estate", sessionState::ptr_t(new estateSessionState()));
	LL_INFOS() << "LLWebRTCVoiceClient::startEstateSession() mNextSession "  << LL_ENDL;
    return true;
}
void LLWebRTCVoiceClient::stateMachine()
{
	LL_INFOS() << "LLWebRTCVoiceClient::stateMachine()" <<LL_ENDL;
	bool voiceEnabled = mVoiceEnabled;

	if (!isAgentAvatarValid())
	{
		return;
	}
	LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() isAgentAvatarValid" <<LL_ENDL;
	LLViewerRegion *regionp = gAgent.getRegion();
	if (!regionp)
	{
		return;
	}
	LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() getRegion" <<LL_ENDL;
	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
	LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() " << regionServerType <<LL_ENDL;
	if (regionServerType != "webrtc")
	{
		// we've switched away from webrtc voice, so shut all channels down.
		// leave channel can be called again and again without adverse effects.
		// it merely tells channels to shut down if they're not already doing so.
		leaveChannel();
	}
	else if (inSpatialChannel())
	{
		bool useEstateVoice = true;
		// add session for region or parcel voice.
		if (!regionp || regionp->getRegionID().isNull())
		{
			// no region, no voice.
			return;
		}

		voiceEnabled = voiceEnabled && regionp->isVoiceEnabled();
		LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() voiceEnabled " << voiceEnabled <<LL_ENDL;
		if (voiceEnabled)
		{
			LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
			// check to see if parcel changed.
			if (parcel && parcel->getLocalID() != INVALID_PARCEL_ID)
			{
				// parcel voice
				if (!parcel->getParcelFlagAllowVoice())
				{
					voiceEnabled = false;
				}
				else if (!parcel->getParcelFlagUseEstateVoiceChannel())
				{
					// use the parcel-specific voice channel.
					S32         parcel_local_id = parcel->getLocalID();
					std::string channelID       = regionp->getRegionID().asString() + "-" + std::to_string(parcel->getLocalID());

					useEstateVoice = false;
					if (!inOrJoiningChannel(channelID))
					{
						startParcelSession(channelID, parcel_local_id);
					}
				}
			}
			LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() voiceEnabled2 " << voiceEnabled <<LL_ENDL;
			if (voiceEnabled && useEstateVoice && !inEstateChannel())
			{
				LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() startEstateSession " << voiceEnabled <<LL_ENDL;
				// estate voice
				startEstateSession();
			}
		}
		if (!voiceEnabled)
		{
			LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() leaveChannel " << voiceEnabled <<LL_ENDL;
			// voice is disabled, so leave and disable PTT
			leaveChannel();
		}
		else
		{
			// we're in spatial voice, and voice is enabled, so determine positions in order
			// to send position updates.
			updatePosition();
		}
	}
	LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() processSessionStates "  <<LL_ENDL;
	sessionState::processSessionStates();
	if (regionServerType == "webrtc" && voiceEnabled)
	{
		LL_INFOS() << "LLWebRTCVoiceClient::stateMachine() sendPositionUpdate "  <<LL_ENDL;
		sendPositionUpdate(false);
		updateOwnVolume();
	}
}
float LLWebRTCVoiceClient::getAudioLevel()
{
    // if (mIsInTuningMode)
    // {
    //     return (1.0 - mWebRTCDeviceInterface->getTuningAudioLevel() * LEVEL_SCALE_WEBRTC) * mTuningMicGain / 2.1;
    // }
    // else
    // {
        return (1.0 - mWebRTCDeviceInterface->getPeerConnectionAudioLevel() * LEVEL_SCALE_WEBRTC) * mMicGain / 2.1;
    // }
}
// ------------------------------------------------------------------
// Predicates, for calls to all sessions

void LLWebRTCVoiceClient::predUpdateOwnVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, F32 audio_level)
{
    participantStatePtr_t participant = session->findParticipantByID(gAgentID);
    if (participant)
    {
        participant->mLevel = audio_level;
        // TODO: Add VAD for our own voice.
        participant->mIsSpeaking = audio_level > SPEAKING_AUDIO_LEVEL;
    }
}
// Update our own volume on our participant, so it'll show up
// in the UI.  This is done on all sessions, so switching
// sessions retains consistent volume levels.
void LLWebRTCVoiceClient::updateOwnVolume() {
    F32 audio_level = 0.0;
    if (!mMuteMic && !mTuningMode)
    {
        audio_level = getAudioLevel();
    }

    sessionState::for_each(boost::bind(predUpdateOwnVolume, _1, audio_level));
}
LLWebRTCVoiceClient::parcelSessionState::parcelSessionState(const std::string &channelID, S32 parcel_local_id)
{
    mHangupOnLastLeave = false;
    mNotifyOnFirstJoin = false;
    LLUUID region_id   = gAgent.getRegion()->getRegionID();
    mChannelID         = channelID;
    mWebRTCConnections.emplace_back(new LLVoiceWebRTCSpatialConnection(region_id, parcel_local_id, channelID));
}
bool LLWebRTCVoiceClient::startParcelSession(const std::string &channelID, S32 parcelID)
{
    leaveChannel();
    mNextSession = addSession(channelID, sessionState::ptr_t(new parcelSessionState(channelID, parcelID)));
    return true;
}
bool LLWebRTCVoiceClient::inOrJoiningChannel(const std::string& channelID)
{
    return (mSession && mSession->mChannelID == channelID) || (mNextSession && mNextSession->mChannelID == channelID);
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
	LL_INFOS("Voice") << "entering state " << state2string(inState) << LL_ENDL;

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
LLWebRTCVoiceClient::participantStatePtr_t LLWebRTCVoiceClient::sessionState::findParticipantByID(const LLUUID& id)
{
	participantStatePtr_t result;
    participantUUIDMap::iterator iter = mParticipantsByUUID.find(id);

    if(iter != mParticipantsByUUID.end())
    {
        result = iter->second;
    }

    return result;
}
LLWebRTCVoiceClient::participantStatePtr_t LLWebRTCVoiceClient::findParticipantByID(const std::string &channelID, const LLUUID &id)
{
	participantStatePtr_t result;
    LLWebRTCVoiceClient::sessionState::ptr_t session = sessionState::matchSessionByChannelID(channelID);

    if (session)
    {
        result = session->findParticipantByID(id);
    }

    return result;
}
bool LLWebRTCVoiceClient::isParticipant(const LLUUID &speaker_id)
{
	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
	if (regionServerType == "webrtc" && mSession)
    {
        return (mSession->mParticipantsByUUID.find(speaker_id) != mSession->mParticipantsByUUID.end());
    }
    return false;
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
	return TRUE;
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
	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
    if (regionServerType == "webrtc" && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mDisplayName;
        }
    }
    return result;
}
BOOL LLWebRTCVoiceClient::getIsSpeaking(const LLUUID& id)
{
	BOOL result = FALSE;
	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
	if (regionServerType == "webrtc" && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mIsSpeaking;
        }
    }
    return result;
}
BOOL LLWebRTCVoiceClient::getIsModeratorMuted(const LLUUID& id)
{
	BOOL result = FALSE;

	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
	if (regionServerType == "webrtc" && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mIsModeratorMuted;
        }
    }
    return result;
}
F32 LLWebRTCVoiceClient::getCurrentPower(const LLUUID& id)
{
	F32 result = 0;
	std::string regionServerType = gAgent.getRegion()->getVoiceServerType();
	if (regionServerType.empty()) {
			regionServerType = "vivox";
	}
	if (regionServerType == "webrtc" || !mSession)
    {
        return result;
    }
    participantStatePtr_t participant(mSession->findParticipantByID(id));
    if (participant)
    {
        if (participant->mIsSpeaking)
        {
            result = participant->mLevel;
        }
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

	participantStatePtr_t participant(mSession->findParticipantByID(id));
    if(participant)
    {
        result = participant->mOnMuteList;
    }

    return result;

	return result;
}
void LLWebRTCVoiceClient::setUserVolume(const LLUUID& id, F32 volume)
{
	if(mAudioSession)
	{
		participantStatePtr_t participant(mSession->findParticipantByID(id));
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

	participantStatePtr_t participant(mSession->findParticipantByID(id));
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
	participantStatePtr_t participant(mSession->findParticipantByID(id));
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
	LL_INFOS() << "LLWebRTCVoiceClient::deleteSession " << LL_ENDL;
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
	LL_INFOS("Voice")
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
	LL_INFOS() << "LLWebRTCVoiceClient::addSession"<<LL_ENDL;
    sessionStatePtr_t existingSession = sessionState::matchSessionByChannelID(channel_id);
	LL_INFOS() << "LLWebRTCVoiceClient::addSession existingSession"<<LL_ENDL;
    if (!existingSession)
    {
        // No existing session found.

        LL_INFOS("Voice") << "adding new session with channel: " << channel_id << LL_ENDL;
        session->setMuteMic(mMuteMic);
        session->setMicGain(mMicGain);
        session->setSpeakerVolume(mSpeakerVolume);

        sessionState::addSession(channel_id, session);
        return session;
    }
    else
    {
        // Found an existing session
        LL_INFOS("Voice") << "Attempting to add already-existing session " << channel_id << LL_ENDL;
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
	LL_INFOS() << "LLWebRTCVoiceClient::inSpatialChannel" <<LL_ENDL;
	bool result = true;

    if (mNextSession)
    {
        result = mNextSession->isSpatial();
    }
    else if(mSession)
    {
        result = mSession->isSpatial();
    }

    
	LL_INFOS() << "LLWebRTCVoiceClient::inSpatialChannel result " << result <<LL_ENDL;
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
// tell the Secondlife WebRTC server that
// we're joining and whether we're
// joining a server associated with the
// the region we currently occupy or not (primary)
// The WebRTC voice server will pass this info
// to peers.
void LLVoiceWebRTCConnection::sendJoin()
{
    Json::FastWriter writer;
    Json::Value      root     = Json::objectValue;
    Json::Value      join_obj = Json::objectValue;
    LLUUID           regionID = gAgent.getRegion()->getRegionID();
    if ((regionID == mRegionID) || !isSpatial())
    {
        join_obj["p"] = true;
    }
    root["j"]             = join_obj;
    std::string json_data = writer.write(root);
    mWebRTCDataInterface->sendData(json_data, false);
}
void LLWebRTCVoiceClient::sessionState::sendData(const std::string &data)
{
    for (auto &connection : mWebRTCConnections)
    {
        connection->sendData(data);
    }
}
void LLWebRTCVoiceClient::predSendData(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const std::string &spatial_data)
{
    if (session->isSpatial() && !spatial_data.empty())
    {
        session->sendData(spatial_data);
    }
}
/*static*/
void LLWebRTCVoiceClient::sessionState::for_eachPredicate(const std::pair<std::string, LLWebRTCVoiceClient::sessionState::wptr_t> &a, sessionFunc_t func)
{
    ptr_t aLock(a.second.lock());

    if (aLock)
        func(aLock);
    else
    {
        LL_WARNS("Voice") << "Stale handle in session map!" << LL_ENDL;
    }
}
void LLWebRTCVoiceClient::sessionState::for_each(sessionFunc_t func)
{
    std::for_each(mSessions.begin(), mSessions.end(), boost::bind(for_eachPredicate, _1, func));
}
// We send our position via a WebRTC data channel to the WebRTC
// server for fine-grained, low latency updates.  On the server,
// these updates will be 'tethered' to the actual position of the avatar.
// Those updates are higher latency, however.
// This mechanism gives low latency spatial updates and server-enforced
// prevention of 'evesdropping' by sending camera updates beyond the
// standard 50m
void LLWebRTCVoiceClient::sendPositionUpdate(bool force)
{
   
    Json::FastWriter writer;
    std::string      spatial_data;

    if (mSpatialCoordsDirty || force)
    {
        Json::Value   spatial = Json::objectValue;

        spatial["sp"]   = Json::objectValue;
        spatial["sp"]["x"] = (int) (mAvatarPosition[0] * 100);
        spatial["sp"]["y"] = (int) (mAvatarPosition[1] * 100);
        spatial["sp"]["z"] = (int) (mAvatarPosition[2] * 100);
        spatial["sh"]      = Json::objectValue;
        spatial["sh"]["x"] = (int) (mAvatarRot[0] * 100);
        spatial["sh"]["y"] = (int) (mAvatarRot[1] * 100);
        spatial["sh"]["z"] = (int) (mAvatarRot[2] * 100);
        spatial["sh"]["w"] = (int) (mAvatarRot[3] * 100);

        spatial["lp"]   = Json::objectValue;
        spatial["lp"]["x"] = (int) (mListenerPosition[0] * 100);
        spatial["lp"]["y"] = (int) (mListenerPosition[1] * 100);
        spatial["lp"]["z"] = (int) (mListenerPosition[2] * 100);
        spatial["lh"]      = Json::objectValue;
        spatial["lh"]["x"] = (int) (mListenerRot[0] * 100);
        spatial["lh"]["y"] = (int) (mListenerRot[1] * 100);
        spatial["lh"]["z"] = (int) (mListenerRot[2] * 100);
        spatial["lh"]["w"] = (int) (mListenerRot[3] * 100);

        mSpatialCoordsDirty = false;
        spatial_data = writer.write(spatial);

        sessionState::for_each(boost::bind(predSendData, _1, spatial_data));
    }
}
void LLVoiceWebRTCConnection::breakVoiceConnectionCoro() {
	if (mWebRTCDataInterface)
		{
			mWebRTCDataInterface->unsetDataObserver(this);
			mWebRTCDataInterface = nullptr;
		}
		mWebRTCAudioInterface   = nullptr;
		LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(mRegionID);
		if (!regionp || !regionp->capabilitiesReceived())
		{
			LL_DEBUGS("Voice") << "no capabilities for voice provisioning; waiting " << LL_ENDL;
			return;
		}

		std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
		if (url.empty())
		{
			return;
		}

		LL_DEBUGS("Voice") << "region ready for voice break; url=" << url << LL_ENDL;

		LLSD body;
		body["logout"]         = TRUE;
		body["viewer_session"] = mViewerSession;
		body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;
		mOutstandingRequests++;
		setVoiceConnectionState(VOICE_STATE_WAIT_FOR_EXIT);
		LLHTTPClient::post(url, body,new LLHTTPClient::ResponderIgnore);
		// LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
		// 	new LLCoreHttpUtil::HttpCoroutineAdapter("LLVoiceWebRTCAdHocConnection::breakVoiceConnection",
		// 											LLCore::HttpRequest::DEFAULT_POLICY_ID));
		// LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
		// LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

		// httpOpts->setWantHeaders(true);

		

		

		if (LLWebRTCVoiceClient::isShuttingDown())
		{
			return;
		}

		if (mWebRTCPeerConnectionInterface)
		{
			mWebRTCPeerConnectionInterface->shutdownConnection();
		}
		setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);

		mOutstandingRequests--;
}
// Primary state machine for negotiating a single voice connection to the
// Secondlife WebRTC server.
bool LLVoiceWebRTCConnection::connectionStateMachine()
{
	LL_INFOS() << "WebRTC : Connection connectionStateMachine" << LL_ENDL;
    processIceUpdates();
	LL_INFOS() << "WebRTC : Connection state " << getVoiceConnectionState()<<LL_ENDL;
    switch (getVoiceConnectionState())
    {
        case VOICE_STATE_START_SESSION:
        {
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
                break;
            }
            mIceCompleted = false;
            setVoiceConnectionState(VOICE_STATE_WAIT_FOR_SESSION_START);
            // tell the webrtc library that we want a connection.  The library will
            // respond with an offer on a separate thread, which will cause
            // the session state to change.
            if (!mWebRTCPeerConnectionInterface->initializeConnection())
            {
                setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
            }
            break;
        }

        case VOICE_STATE_WAIT_FOR_SESSION_START:
        {
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
            }
            break;
        }

        case VOICE_STATE_REQUEST_CONNECTION:
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
                break;
            }
            // Ask the sim to ask the Secondlife WebRTC server for a connection to
            // a given voice channel.  On completion, we'll move on to the
            // VOICE_STATE_SESSION_ESTABLISHED via a callback on a webrtc thread.
            setVoiceConnectionState(VOICE_STATE_CONNECTION_WAIT);
            // LLCoros::getInstance()->launch("LLVoiceWebRTCConnection::requestVoiceConnectionCoro",
            //                                boost::bind(&LLVoiceWebRTCConnection::requestVoiceConnectionCoro, this));
			requestVoiceConnection();
            break;

        case VOICE_STATE_CONNECTION_WAIT:
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
            }
            break;

        case VOICE_STATE_SESSION_ESTABLISHED:
        {
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
                break;
            }
            // update the peer connection with the various characteristics of
            // this connection.
            mWebRTCAudioInterface->setMute(mMuted);
            mWebRTCAudioInterface->setReceiveVolume(mSpeakerVolume);
            mWebRTCAudioInterface->setSendVolume(mMicGain);
            LLWebRTCVoiceClient::getInstance()->OnConnectionEstablished(mChannelID, mRegionID);
            setVoiceConnectionState(VOICE_STATE_WAIT_FOR_DATA_CHANNEL);
            break;
        }

        case VOICE_STATE_WAIT_FOR_DATA_CHANNEL:
        {
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
                break;
            }
            if (mWebRTCDataInterface) // the interface will be set when the session is negotiated.
            {
                sendJoin();  // tell the Secondlife WebRTC server that we're here via the data channel.
                setVoiceConnectionState(VOICE_STATE_SESSION_UP);
                LLWebRTCVoiceClient::getInstance()->sendPositionUpdate(true);
            }
            break;
        }

        case VOICE_STATE_SESSION_UP:
        {
            mRetryWaitPeriod = 0;
            mRetryWaitSecs   = ((F32) rand() / (RAND_MAX)) + 0.5;

            // we'll stay here as long as the session remains up.
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
            }
            break;
        }

        case VOICE_STATE_SESSION_RETRY:
            // only retry ever 'n' seconds
            if (mRetryWaitPeriod++ * UPDATE_THROTTLE_SECONDS > mRetryWaitSecs)
            {
                // something went wrong, so notify that the connection has failed.
                LLWebRTCVoiceClient::getInstance()->OnConnectionFailure(mChannelID, mRegionID, mCurrentStatus);
                setVoiceConnectionState(VOICE_STATE_DISCONNECT);
                mRetryWaitPeriod = 0;
                if (mRetryWaitSecs < MAX_RETRY_WAIT_SECONDS)
                {
                    // back off the retry period, and do it by a small random
                    // bit so all clients don't reconnect at once.
                    mRetryWaitSecs += ((F32) rand() / (RAND_MAX)) + 0.5;
                    mRetryWaitPeriod = 0;
                }
            }
            break;

        case VOICE_STATE_DISCONNECT:
            breakVoiceConnectionCoro();
            break;

        case VOICE_STATE_WAIT_FOR_EXIT:
            break;

        case VOICE_STATE_SESSION_EXIT:
        {
            {
                if (!mShutDown)
                {
                    mVoiceConnectionState = VOICE_STATE_START_SESSION;
                }
                else
                {
                    // if we still have outstanding http or webrtc calls, wait for them to
                    // complete so we don't delete objects while they still may be used.
                    if (mOutstandingRequests <= 0)
                    {
                        LLWebRTCVoiceClient::getInstance()->OnConnectionShutDown(mChannelID, mRegionID);
                        return false;
                    }
                }
            }
            break;
        }

        default:
        {
            LL_WARNS("Voice") << "Unknown voice control state " << getVoiceConnectionState() << LL_ENDL;
            return false;
        }
    }
    return true;
}
void LLWebRTCVoiceClient::sessionState::shutdownAllConnections()
{
    mShuttingDown = true;
    for (auto &&connection : mWebRTCConnections)
    {
        connection->shutDown();
    }
}
/////////////////////////////
// session control messages.
//
// these are called by the sessions to report
// status for a given channel.  By filtering
// on channel and region, these functions
// can send various notifications to
// other parts of the viewer, as well as
// managing housekeeping

// A connection to a channel was successfully established,
// so shut down the current session and move on to the next
// if one is available.
// if the current session is the one that was established,
// notify the observers.
void LLWebRTCVoiceClient::OnConnectionEstablished(const std::string &channelID, const LLUUID &regionID)
{
    
    if (gAgent.getRegion()->getRegionID() == regionID)
    {
        if (mNextSession && mNextSession->mChannelID == channelID)
        {
            if (mSession)
            {
                mSession->shutdownAllConnections();
            }
            mSession = mNextSession;
            mNextSession.reset();

            // Add ourselves as a participant.
            mSession->addParticipant(gAgentID);
        }

        // The current session was established.
        if (mSession && mSession->mChannelID == channelID)
        {
            LLWebRTCVoiceClient::getInstance()->notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_LOGGED_IN);

            // only set status to joined if asked to.  This will happen in the case where we're not
            // doing an ad-hoc based p2p session. Those sessions expect a STATUS_JOINED when the peer
            // has, in fact, joined, which we detect elsewhere.
            if (!mSession->mNotifyOnFirstJoin)
            {
                LLWebRTCVoiceClient::getInstance()->notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_JOINED);
            }
        }
    }
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
struct memory {
  char *response;
  size_t size;
};
 
static size_t curl_callback(char *data, size_t size, size_t nmemb, void *clientp)
{
  size_t realsize = size * nmemb;
  struct memory *mem = (struct memory *)clientp;
 
  char *ptr = (char *) realloc(mem->response, mem->size + realsize + 1);
  if(!ptr)
    return 0;  /* out of memory! */
 
  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;
 
  return realsize;
}
// Tell the simulator to tell the Secondlife WebRTC server that we want a voice
// connection. The SDP is sent up as part of this, and the simulator will respond
// with an 'answer' which is in the form of another SDP.  The webrtc library
// will use the offer and answer to negotiate the session.
void LLVoiceWebRTCSpatialConnection::requestVoiceConnection()
{

    LLViewerRegion *regionp = gAgent.getRegion();

    LL_INFOS("Voice") << "Requesting voice connection." << LL_ENDL;
    if (!regionp || !regionp->capabilitiesReceived())
    {
        LL_INFOS("Voice") << "no capabilities for voice provisioning; waiting " << LL_ENDL;
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
    if (url.empty())
    {
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    LL_INFOS("Voice") << "region ready for voice provisioning; url=" << url << LL_ENDL;

    //LLVoiceWebRTCStats::getInstance()->provisionAttemptStart();
    LLSD body;
    LLSD jsep;
    jsep["type"] = "offer";
    jsep["sdp"] = mChannelSDP;
    body["jsep"] = jsep;
	mParcelLocalID = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
    if (mParcelLocalID != INVALID_PARCEL_ID)
    {
        body["parcel_local_id"] = mParcelLocalID;
    }
    body["channel_type"]      = "local";
    body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;
    LL_INFOS() << "requestVoiceConnection " << ll_print_sd(body) << LL_ENDL;
    // LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    // LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

    // httpOpts->setWantHeaders(true);

	
    // LLHTTPClient::post(url, body, new LLCoroResponder(
    //         boost::bind(&LLVoiceWebRTCSpatialConnection::OnVoiceConnectionRequestSuccess, this, _1)));
	// LLCoroResponderRaw *responder = new LLCoroResponderRaw(boost::bind(&LLVoiceWebRTCSpatialConnection::OnVoiceConnectionRequestSuccess, this, _1));
	
	// LLHTTPClient::post(url, body, responder);
    // mOutstandingRequests++;


	CURL *curl;
  	CURLcode res;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	struct memory chunk = {0};
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	struct curl_slist *hs=NULL;
	hs = curl_slist_append(hs, "Content-Type: application/llsd+xml");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hs);
  	if(curl) {
		 curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ll_print_sd(body));
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		res = curl_easy_perform(curl);
		/* Check for errors */
		if(res != CURLE_OK)
			LL_INFOS() << "requestVoiceConnection failed " << curl_easy_strerror(res) << LL_ENDL;
		else {
			LL_INFOS() << "requestVoiceConnection successed " << chunk.response << LL_ENDL;
			LLSD result = LLSD();
			std::istringstream resultstream = std::istringstream(chunk.response);
			LLSDSerialize::fromXML(result, resultstream);
			
			if (result.has("viewer_session") &&
				result.has("jsep") &&
				result["jsep"].has("type") &&
				result["jsep"]["type"] == "answer" &&
				result["jsep"].has("sdp"))
			{
				mRemoteChannelSDP = result["jsep"]["sdp"].asString();
				mViewerSession    = result["viewer_session"];
				mWebRTCPeerConnectionInterface->AnswerAvailable(mRemoteChannelSDP);
			}
			else
			{
				LL_WARNS("Voice") << "Invalid voice provision request result:" << result << LL_ENDL;
				setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
				
			}

		}	
		free(chunk.response);
		curl_easy_cleanup(curl);
	}


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
void LLVoiceWebRTCConnection::OnVoiceConnectionRequestSuccess(const LLCoroResponderRaw& responder)
{
	LL_INFOS() << "OnVoiceConnectionRequestSuccess" << responder.getStatus() << LL_ENDL;
	LL_INFOS() << "OnVoiceConnectionRequestSuccess" << responder.getHeaders() << LL_ENDL; 
	 LLSD result = responder.getContent();

	LL_INFOS() << "OnVoiceConnectionRequestSuccess" << ll_print_sd(result) << LL_ENDL;
	
    // if (LLWebRTCVoiceClient::isShuttingDown())
    // {
    //     return;
    // }
    //LLVoiceWebRTCStats::getInstance()->provisionAttemptEnd(true);

    if (result.has("viewer_session") &&
        result.has("jsep") &&
        result["jsep"].has("type") &&
        result["jsep"]["type"] == "answer" &&
        result["jsep"].has("sdp"))
    {
        mRemoteChannelSDP = result["jsep"]["sdp"].asString();
        mViewerSession    = result["viewer_session"];
    }
    else
    {
        LL_WARNS("Voice") << "Invalid voice provision request result:" << result << LL_ENDL;
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    LL_DEBUGS("Voice") << "ProvisionVoiceAccountRequest response"
                       << " channel sdp " << mRemoteChannelSDP << LL_ENDL;
    mWebRTCPeerConnectionInterface->AnswerAvailable(mRemoteChannelSDP);
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
    
	LL_INFOS() << "LLVoiceWebRTCConnection::OnIceGatheringState " << state << LL_ENDL;
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
   if (mShutDown || LLWebRTCVoiceClient::isShuttingDown())
    {
        mOutstandingRequests--;
        return;
    }

    bool iceCompleted = false;
    LLSD body;
    if (!mIceCandidates.empty() || mIceCompleted)
    {
        LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(mRegionID);
        if (!regionp || !regionp->capabilitiesReceived())
        {
            LL_DEBUGS("Voice") << "no capabilities for ice gathering; waiting " << LL_ENDL;
            mOutstandingRequests--;
            return;
        }

        std::string url = regionp->getCapability("VoiceSignalingRequest");
        if (url.empty())
        {
            mOutstandingRequests--;
            return;
        }

        LL_DEBUGS("Voice") << "region ready to complete voice signaling; url=" << url << LL_ENDL;
        if (!mIceCandidates.empty())
        {
            LLSD candidates = LLSD::emptyArray();
            for (auto &ice_candidate : mIceCandidates)
            {
                LLSD body_candidate;
                body_candidate["sdpMid"]        = ice_candidate.mSdpMid;
                body_candidate["sdpMLineIndex"] = ice_candidate.mMLineIndex;
                body_candidate["candidate"]     = ice_candidate.mCandidate;
                candidates.append(body_candidate);
            }
            body["candidates"] = candidates;
            mIceCandidates.clear();
        }
        else if (mIceCompleted)
        {
            LLSD body_candidate;
            body_candidate["completed"] = true;
            body["candidate"]           = body_candidate;
            iceCompleted                = mIceCompleted;
            mIceCompleted               = false;
        }

        body["viewer_session"]    = mViewerSession;
        body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;

        // LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
        //     new LLCoreHttpUtil::HttpCoroutineAdapter("LLVoiceWebRTCAdHocConnection::processIceUpdatesCoro",
        //                                                 LLCore::HttpRequest::DEFAULT_POLICY_ID));
        // LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
        // LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

        // httpOpts->setWantHeaders(true);

        // LLSD result = httpAdapter->postAndSuspend(httpRequest, url, body, httpOpts);

        // if (LLWebRTCVoiceClient::isShuttingDown())
        // {
        //     return;
        // }

        // LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
        // LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

        // if (!status)
        // {
        //     // couldn't trickle the candidates, so restart the session.
        //     setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        // }
    }
    mOutstandingRequests--;
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
	LL_INFOS("Voice") << "On Offer Available." << sdp<< LL_ENDL;
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
void LLWebRTCVoiceClient::notifyParticipantObservers()
{
    for (observer_set_t::iterator it = mParticipantObservers.begin(); it != mParticipantObservers.end();)
    {
        LLVoiceClientParticipantObserver *observer = *it;
        observer->onParticipantsChanged();
        // In case onParticipantsChanged() deleted an entry.
        it = mParticipantObservers.upper_bound(observer);
    }
}
// Name resolution
void LLWebRTCVoiceClient::lookupName(const LLUUID &id)
{
    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }
    mAvatarNameCacheConnection = LLAvatarNameCache::get(id, boost::bind(&LLWebRTCVoiceClient::onAvatarNameCache, this, _1, _2));
}

void LLWebRTCVoiceClient::onAvatarNameCache(const LLUUID& agent_id,
                                           const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();
    std::string display_name = av_name.getDisplayName();
    avatarNameResolved(agent_id, display_name);
}

void LLWebRTCVoiceClient::predAvatarNameResolution(const LLWebRTCVoiceClient::sessionStatePtr_t &session, LLUUID id, std::string name)
{
    participantStatePtr_t participant(session->findParticipantByID(id));
    if (participant)
    {
        // Found -- fill in the name
        participant->mDisplayName = name;
        // and post a "participants updated" message to listeners later.
        LLWebRTCVoiceClient::getInstance()->notifyParticipantObservers();
    }
}

void LLWebRTCVoiceClient::avatarNameResolved(const LLUUID &id, const std::string &name)
{
    sessionState::for_each(boost::bind(predAvatarNameResolution, _1, id, name));
}
//  participantState level participant management
LLWebRTCVoiceClient::participantState::participantState(const LLUUID& agent_id) :
     mURI(agent_id.asString()),
     mAvatarID(agent_id),
     mIsSpeaking(false),
     mIsModeratorMuted(false),
     mLevel(0.f),
     mVolume(LLVoiceClient::getInstance()->getDefaultBoostlevel())
{
}
LLWebRTCVoiceClient::participantStatePtr_t LLWebRTCVoiceClient::sessionState::addParticipant(const LLUUID& agent_id)
{

    
	LL_INFOS() << "LLWebRTCVoiceClient::sessionState::addParticipant " << LL_ENDL;
    participantStatePtr_t result;

    participantUUIDMap::iterator iter = mParticipantsByUUID.find(agent_id);

    if (iter != mParticipantsByUUID.end())
    {
        result = iter->second;
    }

    if(!result)
    {
        // participant isn't already in one list or the other.
        result.reset(new participantState(agent_id));
        mParticipantsByUUID.insert(participantUUIDMap::value_type(agent_id, result));
        result->mAvatarID      = agent_id;

        LLWebRTCVoiceClient::getInstance()->lookupName(agent_id);

        LLSpeakerVolumeStorage::getInstance()->getSpeakerVolume(result->mAvatarID, result->mVolume);
        if (!LLWebRTCVoiceClient::sShuttingDown)
        {
            LLWebRTCVoiceClient::getInstance()->notifyParticipantObservers();
        }

        LL_DEBUGS("Voice") << "Participant \"" << result->mURI << "\" added." << LL_ENDL;
    }

    return result;
}
void LLWebRTCVoiceClient::OnConnectionShutDown(const std::string &channelID, const LLUUID &regionID)
{
    if (gAgent.getRegion()->getRegionID() == regionID)
    {
        if (mSession && mSession->mChannelID == channelID)
        {
            LL_DEBUGS("Voice") << "Main WebRTC Connection Shut Down." << LL_ENDL;
        }
    }
}

void LLWebRTCVoiceClient::OnConnectionFailure(const std::string                       &channelID,
                                              const LLUUID                            &regionID,
                                              LLVoiceClientStatusObserver::EStatusType status_type)
{
    LL_DEBUGS("Voice") << "A connection failed.  channel:" << channelID << LL_ENDL;
    if (gAgent.getRegion()->getRegionID() == regionID)
    {
        if (mNextSession && mNextSession->mChannelID == channelID)
        {
            LLWebRTCVoiceClient::getInstance()->notifyStatusObservers(status_type);
        }
        else if (mSession && mSession->mChannelID == channelID)
        {
            LLWebRTCVoiceClient::getInstance()->notifyStatusObservers(status_type);
        }
    }
}
// Send data to the Secondlife WebRTC server via the webrtc
// data channel.
void LLVoiceWebRTCConnection::sendData(const std::string &data)
{
    if (getVoiceConnectionState() == VOICE_STATE_SESSION_UP && mWebRTCDataInterface)
    {
        mWebRTCDataInterface->sendData(data, false);
    }
}
void LLWebRTCVoiceClient::setListenerPosition(const LLVector3d &position, const LLVector3 &velocity, const LLQuaternion &rot)
{
    mListenerRequestedPosition = position;

    if (mListenerVelocity != velocity)
    {
        mListenerVelocity   = velocity;
        mSpatialCoordsDirty = true;
    }

    if (mListenerRot != rot)
    {
        mListenerRot        = rot;
        mSpatialCoordsDirty = true;
    }
}