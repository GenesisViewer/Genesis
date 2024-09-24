#include "llviewerprecompiledheaders.h"
#include "llvoicewebrtc.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llparcelflags.h"
#include "llappviewer.h"
#include "llpanel.h"
#include "llvoicechannel.h"
#include "llcoros.h"
#include "llsdjson.h"
#include "llworld.h"
#include "json/reader.h"
#include "json/writer.h"

#include "llmutelist.h"
#include "hippogridmanager.h"
#include "llavatarnamecache.h"
#include "llsdserialize.h"
#include "llwebrtc.h"
#include <boost/thread.hpp>

const std::string WEBRTC_VOICE_SERVER_TYPE = "webrtc";

namespace {

    const F32 MAX_AUDIO_DIST      = 50.0f;
    const F32 VOLUME_SCALE_WEBRTC = 0.01f;
    const F32 LEVEL_SCALE_WEBRTC  = 0.008f;

    const F32 SPEAKING_AUDIO_LEVEL = 0.30;

    static const std::string REPORTED_VOICE_SERVER_TYPE = "Secondlife WebRTC Gateway";

    // Don't send positional updates more frequently than this:
    const F32 UPDATE_THROTTLE_SECONDS = 0.1f;
    const F32 MAX_RETRY_WAIT_SECONDS  = 10.0f;

    // Cosine of a "trivially" small angle
    const F32 FOUR_DEGREES = 4.0f * (F_PI / 180.0f);
    const F32 MINUSCULE_ANGLE_COS = (F32) cos(0.5f * FOUR_DEGREES);

}  // namespace

bool LLWebRTCVoiceClient::sShuttingDown = false;

//------------------------------------------------------------------------
// Sessions

std::map<std::string, LLWebRTCVoiceClient::sessionState::ptr_t> LLWebRTCVoiceClient::sessionState::sSessions;

LLWebRTCVoiceClient::LLWebRTCVoiceClient():
    mHidden(false),
    mTuningMode(false),
    mTuningMicGain(0.0),
    mTuningSpeakerVolume(50),  // Set to 50 so the user can hear themselves when he sets his mic volume
    mDevicesListUpdated(false),

    mSpatialCoordsDirty(false),

    mMuteMic(false),

    mEarLocation(0),
    mMicGain(0.0),

    mVoiceEnabled(false),
    mProcessChannels(false),

    mAvatarNameCacheConnection(),
    mIsInTuningMode(false),
    mIsProcessingChannels(false),
    mIsCoroutineActive(false),
    mWebRTCPump(),
    mWebRTCDeviceInterface(nullptr)
{
    sShuttingDown = false;

    mSpeakerVolume = 0.0;

    mVoiceVersion.serverVersion = "";
   
	mVoiceVersion.serverType = REPORTED_VOICE_SERVER_TYPE;
}

LLWebRTCVoiceClient::~LLWebRTCVoiceClient()
{
}

void LLWebRTCVoiceClient::init(LLPumpIO *pump)
{
    // constructor will set up LLVoiceClient::getInstance()
    llwebrtc::init((llwebrtc::LLWebRTCLogCallback*)this);

    mWebRTCDeviceInterface = llwebrtc::getDeviceInterface();
    mWebRTCDeviceInterface->setDevicesObserver(this);
    //mMainQueue = LL::WorkQueue::getInstance("mainloop");
    refreshDeviceLists();
}

void LLWebRTCVoiceClient::terminate()
{
}

const LLVoiceVersionInfo &LLWebRTCVoiceClient::getVersion()
{
    return mVoiceVersion;
}
void LLWebRTCVoiceClient::setMicGain(F32 gain)
{
    
        mMicGain = gain;
        mWebRTCDeviceInterface->setPeerConnectionGain(gain);
   
}

void LLWebRTCVoiceClient::updateSettings()
{
    setVoiceEnabled(gSavedSettings.getBOOL("EnableVoiceChat") && mProcessChannels);
    static LLCachedControl<S32> sVoiceEarLocation(gSavedSettings, "VoiceEarLocation");
    setEarLocation(sVoiceEarLocation);

    static LLCachedControl<std::string> sInputDevice(gSavedSettings, "VoiceInputAudioDevice");
    setCaptureDevice(sInputDevice);

    static LLCachedControl<std::string> sOutputDevice(gSavedSettings, "VoiceOutputAudioDevice");
    setRenderDevice(sOutputDevice);

    static LLCachedControl<F32> sMicLevel(gSavedSettings, "AudioLevelMic");
    setMicGain(sMicLevel);
    
    llwebrtc::LLWebRTCDeviceInterface::AudioConfig config;

    static LLCachedControl<bool> sEchoCancellation(gSavedSettings, "VoiceEchoCancellation", true);
    config.mEchoCancellation = sEchoCancellation;

    static LLCachedControl<bool> sAGC(gSavedSettings, "VoiceAutomaticGainControl", true);
    config.mAGC = sAGC;

    static LLCachedControl<U32> sNoiseSuppressionLevel(gSavedSettings,
                                                       "VoiceNoiseSuppressionLevel",
                                                       llwebrtc::LLWebRTCDeviceInterface::AudioConfig::ENoiseSuppressionLevel::NOISE_SUPPRESSION_LEVEL_VERY_HIGH);
    config.mNoiseSuppressionLevel = (llwebrtc::LLWebRTCDeviceInterface::AudioConfig::ENoiseSuppressionLevel) (U32)sNoiseSuppressionLevel;

    mWebRTCDeviceInterface->setAudioConfig(config);
}

bool LLWebRTCVoiceClient::isVoiceWorking() const
{
   return mIsProcessingChannels;
}

void LLWebRTCVoiceClient::tuningStart()
{
    if (!mIsInTuningMode)
    {
        mWebRTCDeviceInterface->setTuningMode(true);
        mIsInTuningMode = true;
    }
}
void LLWebRTCVoiceClient::tuningStop()
{
    if (mIsInTuningMode)
    {
        mWebRTCDeviceInterface->setTuningMode(false);
        mIsInTuningMode = false;
    }
}

bool LLWebRTCVoiceClient::inTuningMode()
{
    return mIsInTuningMode;
}
void LLWebRTCVoiceClient::tuningSetMicVolume(float volume)
{
    mTuningMicGain      = volume;
}

void LLWebRTCVoiceClient::tuningSetSpeakerVolume(float volume)
{

    if (volume != mTuningSpeakerVolume)
    {
        mTuningSpeakerVolume = volume;
    }
}
float LLWebRTCVoiceClient::getAudioLevel()
{
    if (mIsInTuningMode)
    {
        return (1.0 - mWebRTCDeviceInterface->getTuningAudioLevel() * LEVEL_SCALE_WEBRTC) * mTuningMicGain / 2.1;
    }
    else
    {
        return (1.0 - mWebRTCDeviceInterface->getPeerConnectionAudioLevel() * LEVEL_SCALE_WEBRTC) * mMicGain / 2.1;
    }
}
float LLWebRTCVoiceClient::tuningGetEnergy(void)
{
    return getAudioLevel();
}

void LLWebRTCVoiceClient::updateDefaultBoostLevel(float volume)

{
    //TODO
}

void LLWebRTCVoiceClient::setNonFriendsVoiceAttenuation(float volume)
{
}

bool LLWebRTCVoiceClient::deviceSettingsAvailable()
{
    bool result = true;

    if(mRenderDevices.empty() || mCaptureDevices.empty())
        result = false;

    return result;
}
// the singleton 'this' pointer will outlive the work queue.
void LLWebRTCVoiceClient::OnDevicesChanged(const llwebrtc::LLWebRTCVoiceDeviceList& render_devices,
                                           const llwebrtc::LLWebRTCVoiceDeviceList& capture_devices)
{

    
    OnDevicesChangedImpl(render_devices, capture_devices);
        
}
void LLWebRTCVoiceClient::LogMessage(llwebrtc::LLWebRTCLogCallback::LogLevel level, const std::string& message)
{
    switch (level)
    {
    case llwebrtc::LLWebRTCLogCallback::LOG_LEVEL_VERBOSE:
        LL_DEBUGS("Voice") << message << LL_ENDL;
        break;
    case llwebrtc::LLWebRTCLogCallback::LOG_LEVEL_INFO:
        LL_INFOS("Voice") << message << LL_ENDL;
        break;
    case llwebrtc::LLWebRTCLogCallback::LOG_LEVEL_WARNING:
        LL_WARNS("Voice") << message << LL_ENDL;
        break;
    case llwebrtc::LLWebRTCLogCallback::LOG_LEVEL_ERROR:
        // use WARN so that we don't crash on a webrtc error.
        // webrtc will force a crash on a fatal error.
        LL_WARNS("Voice") << message << LL_ENDL;
        break;
    default:
        break;
    }
}
void LLWebRTCVoiceClient::OnDevicesChangedImpl(const llwebrtc::LLWebRTCVoiceDeviceList &render_devices,
                                               const llwebrtc::LLWebRTCVoiceDeviceList &capture_devices)
{
    if (sShuttingDown)
    {
        return;
    }
    
    std::string inputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
    std::string outputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");

    LL_DEBUGS("Voice") << "Setting devices to-input: '" << inputDevice << "' output: '" << outputDevice << "'" << LL_ENDL;
    clearRenderDevices();
    for (auto &device : render_devices)
    {
        addRenderDevice(LLVoiceDevice(device.mDisplayName, device.mID));
    }
    setRenderDevice(outputDevice);

    clearCaptureDevices();
    for (auto &device : capture_devices)
    {
        LL_DEBUGS("Voice") << "Checking capture device:'" << device.mID << "'" << LL_ENDL;

        addCaptureDevice(LLVoiceDevice(device.mDisplayName, device.mID));
    }
    setCaptureDevice(inputDevice);

    setDevicesListUpdated(true);
}
bool LLWebRTCVoiceClient::deviceSettingsUpdated()
{
    bool updated = mDevicesListUpdated;
    mDevicesListUpdated = false;
    return updated;
}
void LLWebRTCVoiceClient::clearCaptureDevices()
{
    LL_DEBUGS("Voice") << "called" << LL_ENDL;
    mCaptureDevices.clear();
}
void LLWebRTCVoiceClient::addCaptureDevice(const LLVoiceDevice &device)
{
    LL_INFOS("Voice") << "Voice Capture Device: '" << device.display_name << "' (" << device.full_name << ")" << LL_ENDL;
    mCaptureDevices.push_back(device);
}
void LLWebRTCVoiceClient::clearRenderDevices()
{
    LL_DEBUGS("Voice") << "called" << LL_ENDL;
    mRenderDevices.clear();
}
void LLWebRTCVoiceClient::addRenderDevice(const LLVoiceDevice &device)
{
    LL_INFOS("Voice") << "Voice Render Device: '" << device.display_name << "' (" << device.full_name << ")" << LL_ENDL;
    mRenderDevices.push_back(device);
}
void LLWebRTCVoiceClient::setDevicesListUpdated(bool state)
{
    mDevicesListUpdated = state;
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

void LLWebRTCVoiceClient::setCaptureDevice(const std::string &name)
{
     mWebRTCDeviceInterface->setCaptureDevice(name);
}

void LLWebRTCVoiceClient::setRenderDevice(const std::string &name)
{
    mWebRTCDeviceInterface->setRenderDevice(name);
}

LLVoiceDeviceList &LLWebRTCVoiceClient::getCaptureDevices()
{
    return mCaptureDevices;
}

LLVoiceDeviceList &LLWebRTCVoiceClient::getRenderDevices()
{
    return mRenderDevices;
}

void LLWebRTCVoiceClient::getParticipantList(uuid_set_t &participants)
{
    if (mProcessChannels && mSession)
    {
        for (participantUUIDMap::iterator iter = mSession->mParticipantsByUUID.begin();
            iter != mSession->mParticipantsByUUID.end();
            iter++)
        {
            participants.insert(iter->first);
        }
    }

}

bool LLWebRTCVoiceClient::isParticipant(const LLUUID &speaker_id)
{
   if (mProcessChannels && mSession)
    {
        return (mSession->mParticipantsByUUID.find(speaker_id) != mSession->mParticipantsByUUID.end());
    }
    return false;
}

BOOL LLWebRTCVoiceClient::sendTextMessage(const LLUUID &participant_id, const std::string &message)
{
    //Not used anymore
    return 0;
}

void LLWebRTCVoiceClient::endUserIMSession(const LLUUID &uuid)
{
    //Not used anymore
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
LLWebRTCVoiceClient::sessionStatePtr_t LLWebRTCVoiceClient::addSession(const std::string &channel_id, sessionState::ptr_t session)
{
    sessionStatePtr_t existingSession = sessionState::matchSessionByChannelID(channel_id);
    if (!existingSession)
    {
        // No existing session found.

        LL_DEBUGS("Voice") << "adding new session with channel: " << channel_id << LL_ENDL;
        session->setMuteMic(mMuteMic);
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
BOOL LLWebRTCVoiceClient::isSessionCallBackPossible(const LLUUID &session_id)
{
    sessionStatePtr_t session(findP2PSession(session_id));
    return session && session->isCallbackPossible();
}

BOOL LLWebRTCVoiceClient::isSessionTextIMPossible(const LLUUID &session_id)
{
    // WebRTC doesn't preclude text im
    return true;
}
bool LLWebRTCVoiceClient::inSpatialChannel()
{
    bool result = true;

    if (mNextSession)
    {
        result = mNextSession->isSpatial();
    }
    else if(mSession)
    {
        result = mSession->isSpatial();
    }

    return result;
}
void LLWebRTCVoiceClient::setNonSpatialChannel(const std::string &uri, const std::string &credentials)
{
    //NOT IMPLEMENTED IN LL
}
bool LLWebRTCVoiceClient::startEstateSession()
{
    leaveChannel();
    mNextSession = addSession("Estate", sessionState::ptr_t(new estateSessionState()));
    return true;
}
bool LLWebRTCVoiceClient::startParcelSession(const std::string &channelID, S32 parcelID)
{
    leaveChannel();
    mNextSession = addSession(channelID, sessionState::ptr_t(new parcelSessionState(channelID, parcelID)));
    return true;
}
bool LLWebRTCVoiceClient::startAdHocSession(const LLSD &channelInfo, bool notify_on_first_join, bool hangup_on_last_leave)
{
    leaveChannel();
    LL_WARNS("Voice") << "Start AdHoc Session " << channelInfo << LL_ENDL;
    std::string channelID = channelInfo["channel_uri"];
    std::string credentials = channelInfo["channel_credentials"];
    mNextSession = addSession(channelID,
                              sessionState::ptr_t(new adhocSessionState(channelID,
                                                                        credentials,
                                                                        notify_on_first_join,
                                                                        hangup_on_last_leave)));
    return true;
}
void LLWebRTCVoiceClient::setNonSpatialChannel(const LLSD& channelInfo, bool notify_on_first_join, bool hangup_on_last_leave)
{
    startAdHocSession(channelInfo, notify_on_first_join, hangup_on_last_leave);
}

bool LLWebRTCVoiceClient::inProximalChannel()
{
     return inSpatialChannel();
}
void LLWebRTCVoiceClient::setSpatialChannel(const std::string &uri, const std::string &credentials)
{
    //TODO
}
void LLWebRTCVoiceClient::deleteSession(const sessionStatePtr_t &session)
{
    if (!session)
    {
        return;
    }

    // At this point, the session should be unhooked from all lists and all state should be consistent.
    session->shutdownAllConnections();
    // If this is the current audio session, clean up the pointer which will soon be dangling.
    bool deleteAudioSession = mSession == session;
    bool deleteNextAudioSession = mNextSession == session;
    if (deleteAudioSession)
    {
        mSession.reset();
    }

    // ditto for the next audio session
    if (deleteNextAudioSession)
    {
        mNextSession.reset();
    }
}
bool LLWebRTCVoiceClient::setSpatialChannel(const LLSD &channelInfo)
{
    LL_INFOS("Voice") << "SetSpatialChannel " << channelInfo << LL_ENDL;
    LLViewerRegion *regionp = gAgent.getRegion();
    if (!regionp)
    {
        return false;
    }
    LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

    // we don't really have credentials for a spatial channel in webrtc,
    // it's all handled by the sim.
    if (channelInfo.isMap() && channelInfo.has("channel_uri"))
    {
        bool allow_voice = !channelInfo["channel_uri"].asString().empty();
        if (parcel)
        {
            parcel->setParcelFlag(PF_ALLOW_VOICE_CHAT, allow_voice);
            parcel->setParcelFlag(PF_USE_ESTATE_VOICE_CHAN, channelInfo["channel_uri"].asUUID() == regionp->getRegionID());
        }
        else
        {
            regionp->setRegionFlag(REGION_FLAGS_ALLOW_VOICE, allow_voice);
        }
    }
    return true;
}

void LLWebRTCVoiceClient::leaveNonSpatialChannel()
{
     LL_DEBUGS("Voice") << "Request to leave non-spatial channel." << LL_ENDL;

    // make sure we're not simply rejoining the current session
    deleteSession(mNextSession);

    leaveChannel();
}

void LLWebRTCVoiceClient::leaveChannel()
{
    if (mSession)
    {
        deleteSession(mSession);
    }

    if (mNextSession)
    {
        deleteSession(mNextSession);
    }

    
}

std::string LLWebRTCVoiceClient::getCurrentChannel()
{
    //NOT IMPLEMENTED IN LL
    return std::string();
}

void LLWebRTCVoiceClient::callUser(const LLUUID &uuid)
{
    //NOT IMPLEMENTED IN LL
}

bool LLWebRTCVoiceClient::isValidChannel(std::string &channelHandle)
{
    //NOT IMPLEMENTED IN LL
    return false;
}

bool LLWebRTCVoiceClient::answerInvite(std::string &channelHandle)
{
    //NOT IMPLEMENTED IN LL
    return false;
}

void LLWebRTCVoiceClient::declineInvite(std::string &channelHandle)
{
    //NOT IMPLEMENTED IN LL
}
void LLWebRTCVoiceClient::setVoiceVolume(F32 volume)
{
    if (volume != mSpeakerVolume)
    {
        {
            mSpeakerVolume      = volume;
        }
        sessionState::for_each(boost::bind(predSetSpeakerVolume, _1, volume));
    }
}
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
void LLWebRTCVoiceClient::updateNeighboringRegions()
{
    static const std::vector<LLVector3d> neighbors {LLVector3d(0.0f, 1.0f, 0.0f),  LLVector3d(0.707f, 0.707f, 0.0f),
                                                    LLVector3d(1.0f, 0.0f, 0.0f),  LLVector3d(0.707f, -0.707f, 0.0f),
                                                    LLVector3d(0.0f, -1.0f, 0.0f), LLVector3d(-0.707f, -0.707f, 0.0f),
                                                    LLVector3d(-1.0f, 0.0f, 0.0f), LLVector3d(-0.707f, 0.707f, 0.0f)};

    // Estate voice requires connection to neighboring regions.
    mNeighboringRegions.clear();

    // add current region.
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
void LLWebRTCVoiceClient::predSetSpeakerVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, F32 volume)
{
    session->setSpeakerVolume(volume);
}
bool LLWebRTCVoiceClient::voiceEnabled()
{
    static const LLCachedControl<bool> enable_voice_chat("EnableVoiceChat",true);
	static const LLCachedControl<bool> cmdline_disable_voice("CmdLineDisableVoice",false);
	return enable_voice_chat && !cmdline_disable_voice;
}

void LLWebRTCVoiceClient::setVoiceEnabled(bool enabled)
{
    LL_DEBUGS("Voice")
        << "( " << (enabled ? "enabled" : "disabled") << " )"
        << " was "<< (mVoiceEnabled ? "enabled" : "disabled")
        << " coro "<< (mIsCoroutineActive ? "active" : "inactive")
        << LL_ENDL;

    if (enabled != mVoiceEnabled)
    {
        // TODO: Refactor this so we don't call into LLVoiceChannel, but simply
        // use the status observer
        mVoiceEnabled = enabled;
        LLVoiceClientStatusObserver::EStatusType status;

        if (enabled)
        {
            LL_DEBUGS("Voice") << "enabling" << LL_ENDL;
            LLVoiceChannel::getCurrentVoiceChannel()->activate();
            status = LLVoiceClientStatusObserver::STATUS_VOICE_ENABLED;
            mSpatialCoordsDirty = true;
            updatePosition();
            if (!mIsCoroutineActive)
            {
                //LLCoros::instance().launch("LLWebRTCVoiceClient::voiceConnectionCoro",
                //     boost::bind(&LLWebRTCVoiceClient::voiceConnectionCoro, LLWebRTCVoiceClient::getInstance()));
                // post([]{
                // boost::coroutines2::coroutine<int>::pull_type voiceConnectionCoro{boost::bind(&LLWebRTCVoiceClient::voiceConnectionCoro, LLWebRTCVoiceClient::getInstance())};
                // })

                boost::thread t{boost::bind(&LLWebRTCVoiceClient::voiceConnectionCoro, LLWebRTCVoiceClient::getInstance())};
                
            }
            else
            {
                LL_DEBUGS("Voice") << "coro should be active.. not launching" << LL_ENDL;
            }
        }
        else
        {
            // Turning voice off looses your current channel -- this makes sure the UI isn't out of sync when you re-enable it.
            LLVoiceChannel::getCurrentVoiceChannel()->deactivate();
            gAgent.setVoiceConnected(false);
            status = LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED;
            cleanUp();
        }

        notifyStatusObservers(status);
    }
    else
    {
        LL_DEBUGS("Voice") << " no-op" << LL_ENDL;
    }
}

BOOL LLWebRTCVoiceClient::lipSyncEnabled()
{
    if ( mVoiceEnabled && mProcessChannels )
	{
		return mLipSyncEnabled;
	}
	else
	{
		return FALSE;
	}
}

void LLWebRTCVoiceClient::setLipSyncEnabled(BOOL enabled)
{
    mLipSyncEnabled=enabled;
}

void LLWebRTCVoiceClient::setMuteMic(bool muted)
{
    mMuteMic = muted;
    // when you're hidden, your mic is always muted.
    if (!mHidden)
    {
        sessionState::for_each(boost::bind(predSetMuteMic, _1, muted));
    }
}

void LLWebRTCVoiceClient::predSetMuteMic(const LLWebRTCVoiceClient::sessionStatePtr_t &session, bool muted)
{
    participantStatePtr_t participant = session->findParticipantByID(gAgentID);
    if (participant && muted)
    {
        participant->mLevel = 0.0;
    }
    session->setMuteMic(muted);
}
BOOL LLWebRTCVoiceClient::getVoiceEnabled(const LLUUID &id)
{
    BOOL result = FALSE;
    
    if (!mProcessChannels || !mSession)
    {
        return result;
    }
    participantStatePtr_t participant(mSession->findParticipantByID(id));
    if (participant)
    {
        result = participant->mVoiceEnabled;
    }
    return result;
}

std::string LLWebRTCVoiceClient::getDisplayName(const LLUUID &id)
{
    std::string result;
    if (mProcessChannels && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mDisplayName;
        }
    }
    return result;
}

BOOL LLWebRTCVoiceClient::isParticipantAvatar(const LLUUID &id)
{
     // WebRTC participants are always SL avatars.
    return true;
}

BOOL LLWebRTCVoiceClient::getIsSpeaking(const LLUUID &id)
{
    bool result = false;
    if (mProcessChannels && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mIsSpeaking;
        }
    }
    return result;
}
// TODO: Need to pull muted status from the webrtc server
BOOL LLWebRTCVoiceClient::getIsModeratorMuted(const LLUUID& id)
{
    BOOL result = false;
    if (mProcessChannels && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mIsModeratorMuted;
        }
    }
    return result;
}
F32 LLWebRTCVoiceClient::getCurrentPower(const LLUUID &id)
{
    F32 result = 0.0f;
    if (!mProcessChannels || !mSession)
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


// External accessors.
F32 LLWebRTCVoiceClient::getUserVolume(const LLUUID& id)
{
    // Minimum volume will be returned for users with voice disabled
    F32 result = LLVoiceClient::VOLUME_MIN;

    if (mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mVolume;
        }
    }

    return result;
}
void LLWebRTCVoiceClient::setUserVolume(const LLUUID& id, F32 volume)
{
    F32 clamped_volume = llclamp(volume, LLVoiceClient::VOLUME_MIN, LLVoiceClient::VOLUME_MAX);
    if(mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant && (participant->mAvatarID != gAgentID))
        {
            if (!is_approx_equal(volume, LLVoiceClient::VOLUME_DEFAULT))
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

            participant->mVolume = clamped_volume;
        }
    }
    sessionState::for_each(boost::bind(predSetUserVolume, _1, id, clamped_volume));
}

void LLWebRTCVoiceClient::userAuthorized(const std::string &user_id, const LLUUID &agentID)
{
    //NOT IMPLEMENTED IN LL
}

void LLWebRTCVoiceClient::OnConnectionEstablished(const std::string &channelID, const LLUUID &regionID)
{
    if (mNextSession && mNextSession->mChannelID == channelID)
    {
        if (mSession)
        {
            mSession->shutdownAllConnections();
        }
        mSession = mNextSession;
        mNextSession.reset();
    }

    if (mSession)
    {
        // Add ourselves as a participant.
        mSession->addParticipant(gAgentID, gAgent.getRegion()->getRegionID());
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

void LLWebRTCVoiceClient::OnConnectionShutDown(const std::string &channelID, const LLUUID &regionID)
{
    if (mSession && (mSession->mChannelID == channelID))
    {
        if (gAgent.getRegion()->getRegionID() == regionID)
        {
            if (mSession && mSession->mChannelID == channelID)
            {
                LL_DEBUGS("Voice") << "Main WebRTC Connection Shut Down." << LL_ENDL;
            }
        }
        mSession->removeAllParticipants(regionID);
    }
}

void LLWebRTCVoiceClient::OnConnectionFailure(const std::string &channelID, const LLUUID &regionID, LLVoiceClientStatusObserver::EStatusType status_type)
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
                earRot      = LLQuaternion( mAvatarRot);
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

        // update own region id to be the region id avatar is currently in.
        LLWebRTCVoiceClient::participantStatePtr_t participant = findParticipantByID("Estate", gAgentID);
        if(participant)
        {
            participant->mRegion = gAgent.getRegion()->getRegionID();
        }
    }
}
void LLWebRTCVoiceClient::setEarLocation(S32 loc)
{
    if (mEarLocation != loc)
    {
        LL_DEBUGS("Voice") << "Setting mEarLocation to " << loc << LL_ENDL;

        mEarLocation        = loc;
        mSpatialCoordsDirty = true;
    }
}
void LLWebRTCVoiceClient::predSendData(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const std::string &spatial_data)
{
    if (session->isSpatial() && !spatial_data.empty())
    {
        session->sendData(spatial_data);
    }
}
void LLWebRTCVoiceClient::sendPositionUpdate(bool force)
{
    std::string      spatial_data;

    if (mSpatialCoordsDirty || force)
    {
        
	

	
        LLSD spatial;
        LLSD avatarPosition;
        avatarPosition["x"] = (int) (mAvatarPosition[0] * 100);
        avatarPosition["y"] = (int) (mAvatarPosition[1] * 100);
        avatarPosition["z"] = (int) (mAvatarPosition[2] * 100);

        spatial["sp"] =avatarPosition;

        LLSD avatarRot;
        avatarRot["x"] = (int) (mAvatarRot[0] * 100);
        avatarRot["y"] = (int) (mAvatarRot[1] * 100);
        avatarRot["z"] = (int) (mAvatarRot[2] * 100);
        avatarRot["w"] = (int) (mAvatarRot[3] * 100);
        spatial["sh"]  = avatarRot;

        LLSD listenerPos;
        listenerPos["x"] = (int) (mListenerPosition[0] * 100);
        listenerPos["y"] = (int) (mListenerPosition[1] * 100);
        listenerPos["z"] = (int) (mListenerPosition[2] * 100);
        spatial["lp"]  = listenerPos;

        LLSD listenerRot;
        listenerRot["x"] = (int) (mListenerRot[0] * 100);
        listenerRot["y"] = (int) (mListenerRot[1] * 100);
        listenerRot["z"] = (int) (mListenerRot[2] * 100);
        listenerRot["w"] = (int) (mListenerRot[3] * 100);
        spatial["lh"]  = listenerRot;

       
        auto json = LlsdToJson(spatial);
        mSpatialCoordsDirty = false;
        spatial_data = json.dump();

        sessionState::for_each(boost::bind(predSendData, _1, spatial_data));
    }
}
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
void LLWebRTCVoiceClient::updateOwnVolume()
{
    F32 audio_level = 0.0;
    if (!mMuteMic && !mTuningMode)
    {
        audio_level = getAudioLevel();
    }

    sessionState::for_each(boost::bind(predUpdateOwnVolume, _1, audio_level));
}

// set volume level (gain level) for another user.
void LLWebRTCVoiceClient::predSetUserVolume(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const LLUUID &id, F32 volume)
{
    session->setUserVolume(id, volume);
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
    F32 rot_cos_diff = llabs(dot(LLQuaternion(mAvatarRot), LLQuaternion(rot)));
    if ((LLQuaternion(mAvatarRot) != rot) && (rot_cos_diff < MINUSCULE_ANGLE_COS))
    {
        mAvatarRot          = rot;
        mSpatialCoordsDirty = true;
    }
}
BOOL LLWebRTCVoiceClient::getOnMuteList(const LLUUID &id)
{
     bool result = false;
    if (mProcessChannels && mSession)
    {
        participantStatePtr_t participant(mSession->findParticipantByID(id));
        if (participant)
        {
            result = participant->mOnMuteList;
        }
    }
    return result;
}

// Observers
void LLWebRTCVoiceClient::addObserver(LLVoiceClientParticipantObserver *observer)
{
    mParticipantObservers.insert(observer);
}

void LLWebRTCVoiceClient::removeObserver(LLVoiceClientParticipantObserver *observer)
{
    mParticipantObservers.erase(observer);
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
void LLWebRTCVoiceClient::addObserver(LLVoiceClientStatusObserver *observer)
{
    mStatusObservers.insert(observer);
}

void LLWebRTCVoiceClient::removeObserver(LLVoiceClientStatusObserver *observer)
{
    mStatusObservers.erase(observer);
}

void LLWebRTCVoiceClient::notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status)
{
   
    LL_DEBUGS("Voice") << "( " << LLVoiceClientStatusObserver::status2string(status) << " )"
                       << " mSession=" << mSession << LL_ENDL;

    LL_DEBUGS("Voice") << " " << LLVoiceClientStatusObserver::status2string(status) << ", session channelInfo "
                       << getAudioSessionChannelInfo() << ", proximal is " << inSpatialChannel() << LL_ENDL;

    mIsProcessingChannels = status == LLVoiceClientStatusObserver::STATUS_JOINED;

    LLSD channelInfo = getAudioSessionChannelInfo();
    for (status_observer_set_t::iterator it = mStatusObservers.begin(); it != mStatusObservers.end();)
    {
        LLVoiceClientStatusObserver *observer = *it;
        observer->onChange(status, channelInfo, inSpatialChannel());
        // In case onError() deleted an entry.
        it = mStatusObservers.upper_bound(observer);
    }

    // skipped to avoid speak button blinking
    if (status != LLVoiceClientStatusObserver::STATUS_JOINING &&
        status != LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL &&
        status != LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED)
    {
        bool voice_status = mIsProcessingChannels;

        gAgent.setVoiceConnected(voice_status);

        
    }
}

void LLWebRTCVoiceClient::addObserver(LLFriendObserver *observer)
{
}

void LLWebRTCVoiceClient::removeObserver(LLFriendObserver *observer)
{
}
std::string LLWebRTCVoiceClient::sipURIFromID(const LLUUID &id)
{
    return id.asString();
}

bool LLWebRTCVoiceClient::setVoiceEffect(const LLUUID &id)
{
    //NO VOICE EFFECT ON WEBRTC
    return false;
}

const LLUUID LLWebRTCVoiceClient::getVoiceEffect()
{
    //NO VOICE EFFECT ON WEBRTC
    return LLUUID();
}

LLSD LLWebRTCVoiceClient::getVoiceEffectProperties(const LLUUID &id)
{
    //NO VOICE EFFECT ON WEBRTC
    return LLSD();
}

void LLWebRTCVoiceClient::refreshVoiceEffectLists(bool clear_lists)
{
    //NO VOICE EFFECT ON WEBRTC
}

const voice_effect_list_t &LLWebRTCVoiceClient::getVoiceEffectList() const
{
    return voice_effect_list_t();
}

const voice_effect_list_t &LLWebRTCVoiceClient::getVoiceEffectTemplateList() const
{
    return voice_effect_list_t();
}

void LLWebRTCVoiceClient::addObserver(LLVoiceEffectObserver *observer)
{
     //NO VOICE EFFECT ON WEBRTC
}

void LLWebRTCVoiceClient::removeObserver(LLVoiceEffectObserver *observer)
{
    //NO VOICE EFFECT ON WEBRTC
}

void LLWebRTCVoiceClient::enablePreviewBuffer(bool enable)
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
}

void LLWebRTCVoiceClient::recordPreviewBuffer()
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
}

void LLWebRTCVoiceClient::playPreviewBuffer(const LLUUID &effect_id)
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
}

void LLWebRTCVoiceClient::stopPreviewBuffer()
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
}

bool LLWebRTCVoiceClient::isPreviewRecording()
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
    return false;
}

bool LLWebRTCVoiceClient::isPreviewPlaying()
{
    //NOT IMPLEMENTED IN LL
    //TODO see if we can implement it on RTC
    return false;
}

LLSD LLWebRTCVoiceClient::getAudioSessionChannelInfo()
{
    LLSD result;

    if (mSession)
    {
        result["voice_server_type"]   = WEBRTC_VOICE_SERVER_TYPE;
        result["channel_uri"]         = mSession->mChannelID;
    }

    return result;
}
// protected provider-level participant management.
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
LLWebRTCVoiceClient::participantStatePtr_t LLWebRTCVoiceClient::addParticipantByID(const std::string &channelID, const LLUUID &id, const LLUUID &region)
{
    participantStatePtr_t result;
    LLWebRTCVoiceClient::sessionState::ptr_t session = sessionState::matchSessionByChannelID(channelID);
    if (session)
    {
        result = session->addParticipant(id, region);
        if (session->mNotifyOnFirstJoin && (id != gAgentID))
        {
            notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_JOINED);
        }
    }
    return result;
}

void LLWebRTCVoiceClient::removeParticipantByID(const std::string &channelID, const LLUUID &id, const LLUUID& region)
{
    

    participantStatePtr_t result;
    LLWebRTCVoiceClient::sessionState::ptr_t session = sessionState::matchSessionByChannelID(channelID);
    if (session)
    {
        participantStatePtr_t participant = session->findParticipantByID(id);
        if (participant && (participant->mRegion == region))
        {
            session->removeParticipant(participant);
        }
    }
}
bool LLWebRTCVoiceClient::inOrJoiningChannel(const std::string& channelID)
{
    return (mSession && mSession->mChannelID == channelID) || (mNextSession && mNextSession->mChannelID == channelID);
}
bool LLWebRTCVoiceClient::inEstateChannel()
{
    return (mSession && mSession->isEstate()) || (mNextSession && mNextSession->isEstate());

}
void LLWebRTCVoiceClient::voiceConnectionCoro()
{
    LL_INFOS("Voice") << "starting" << LL_ENDL;
    mIsCoroutineActive = true;
    
    try
    {
        LLMuteList::getInstance()->addObserver(this);
        while (!sShuttingDown)
        {
            
            // TODO: Doing some measurement and calculation here,
            // we could reduce the timeout to take into account the
            // time spent on the previous loop to have the loop
            // cycle at exactly 100ms, instead of 100ms + loop
            // execution time.
            // Could help with voice updates making for smoother
            // voice when we're busy.
            //llcoro::suspendUntilTimeout(UPDATE_THROTTLE_SECONDS);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (sShuttingDown) return; // 'this' migh already be invalid
            bool voiceEnabled = mVoiceEnabled;

            if (!isAgentAvatarValid())
            {
                continue;
            }

            LLViewerRegion *regionp = gAgent.getRegion();
            if (!regionp)
            {
                continue;
            }

            if (!mProcessChannels)
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
                    continue;
                }

                voiceEnabled = voiceEnabled && regionp->isVoiceEnabled();

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
                    if (voiceEnabled && useEstateVoice && !inEstateChannel())
                    {
                        // estate voice
                        startEstateSession();
                    }
                }
                if (!voiceEnabled)
                {
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
            
            if  (sShuttingDown)
            {
                return;
            }
            sessionState::processSessionStates();
            if (mProcessChannels && voiceEnabled && !mHidden)
            {
                sendPositionUpdate(false);
                updateOwnVolume();
            }
            
        }
    }
    
    catch (...)
    {
        // Ideally for Windows need to log SEH exception instead or to set SEH
        // handlers but bugsplat shows local variables for windows, which should
        // be enough
        LL_WARNS("Voice") << "voiceConnectionStateMachine crashed" << LL_ENDL;
        throw;
    }

    cleanUp();
}
// determine whether we're processing channels, or whether
// another voice provider is.
void LLWebRTCVoiceClient::processChannels(bool process)
{
    mProcessChannels = process;
}
////////////////////////
///LLMuteListObserver
///

void LLWebRTCVoiceClient::onChange()
{
}

void LLWebRTCVoiceClient::onChangeDetailed(const LLMute& mute)
{
    if (mute.mType == LLMute::AGENT)
    {
        bool muted = ((mute.mFlags & LLMute::flagVoiceChat) == 0);
        sessionState::for_each(boost::bind(predSetUserMute, _1, mute.mID, muted));
    }
}
void LLWebRTCVoiceClient::predSetUserMute(const LLWebRTCVoiceClient::sessionStatePtr_t &session, const LLUUID &id, bool mute)
{
    session->setUserMute(id, mute);
}
void LLWebRTCVoiceClient::predShutdownSession(const LLWebRTCVoiceClient::sessionStatePtr_t& session)
{
    session->shutdownAllConnections();
}
void LLWebRTCVoiceClient::cleanUp()
{
    mNextSession.reset();
    mSession.reset();
    mNeighboringRegions.clear();
    sessionState::for_each(boost::bind(predShutdownSession, _1));
    LL_DEBUGS("Voice") << "Exiting" << LL_ENDL;
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
    mOutstandingRequests(0),
    mChannelID(channelID),
    mRegionID(regionID),
    mPrimary(true),
    mRetryWaitPeriod(0)
{

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
    mWebRTCPeerConnectionInterface->unsetSignalingObserver(this);
    llwebrtc::freePeerConnection(mWebRTCPeerConnectionInterface);
}

void LLVoiceWebRTCConnection::OnIceGatheringState(EIceGatheringState state)
{
    
    LL_DEBUGS("Voice") << "Ice Gathering voice account. " << state << LL_ENDL;

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

void LLVoiceWebRTCConnection::OnIceCandidate(const llwebrtc::LLWebRTCIceCandidate &candidate)
{
    mIceCandidates.push_back(candidate);
}
void LLVoiceWebRTCConnection::OnOfferAvailable(const std::string &sdp)
{
    if (mShutDown)
    {
        return;
    }
    LL_DEBUGS("Voice") << "On Offer Available." << LL_ENDL;
    mChannelSDP = sdp;
    if (mVoiceConnectionState == VOICE_STATE_WAIT_FOR_SESSION_START)
    {
        mVoiceConnectionState = VOICE_STATE_REQUEST_CONNECTION;
    }
}
void LLVoiceWebRTCConnection::OnRenegotiationNeeded()
{
    LL_DEBUGS("Voice") << "Voice channel requires renegotiation." << LL_ENDL;
    if (!mShutDown)
    {
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
    }
    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_UNKNOWN;
}
void LLVoiceWebRTCConnection::OnPeerConnectionClosed()
{
    LL_DEBUGS("Voice") << "Peer connection has closed." << LL_ENDL;
    if (mVoiceConnectionState == VOICE_STATE_WAIT_FOR_CLOSE)
    {
        setVoiceConnectionState(VOICE_STATE_CLOSED);
        mOutstandingRequests--;
    }
    else if (LLWebRTCVoiceClient::isShuttingDown())
    {
        // disconnect was initialized by llwebrtc::terminate() instead of connectionStateMachine
        LL_INFOS("Voice") << "Peer connection has closed, but state is " << mVoiceConnectionState << LL_ENDL;
        setVoiceConnectionState(VOICE_STATE_CLOSED);
    }
}
void LLVoiceWebRTCConnection::OnAudioEstablished(llwebrtc::LLWebRTCAudioInterface *audio_interface)
{
    if (mShutDown)
    {
        return;
    }
    LL_DEBUGS("Voice") << "On AudioEstablished." << LL_ENDL;
    mWebRTCAudioInterface = audio_interface;
    setVoiceConnectionState(VOICE_STATE_SESSION_ESTABLISHED);
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
    //do it asynchronous?
    LLVoiceWebRTCConnection::OnDataReceivedImpl(data, binary);
}

void LLVoiceWebRTCConnection::OnDataChannelReady(llwebrtc::LLWebRTCDataInterface * data_interface)
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

//
// The LLWebRTCVoiceConnection object will not be deleted
// before the webrtc connection itself is shut down, so
// we shouldn't be getting this callback on a nonexistant
// this pointer.
void LLVoiceWebRTCConnection::OnDataReceivedImpl(const std::string &data, bool binary)
{
   

    if (mShutDown)
    {
        return;
    }

    if (binary)
    {
        LL_WARNS("Voice") << "Binary data received from data channel." << LL_ENDL;
        return;
    }

    Json::Reader reader;
    Json::Value  voice_data;
    if (reader.parse(data, voice_data, false))  // don't collect comments
    {
        if (!voice_data.isObject())
        {
            LL_WARNS("Voice") << "Expected object from data channel:" << data << LL_ENDL;
            return;
        }
        bool new_participant = false;
        Json::Value      mute = Json::objectValue;
        Json::Value      user_gain = Json::objectValue;
        for (auto &participant_id : voice_data.getMemberNames())
        {
            LLUUID agent_id(participant_id);
            if (agent_id.isNull())
            {
                LL_WARNS("Voice") << "Bad participant ID from data channel (" << participant_id << "):" << data << LL_ENDL;
                continue;
            }

            LLWebRTCVoiceClient::participantStatePtr_t participant =
                LLWebRTCVoiceClient::getInstance()->findParticipantByID(mChannelID, agent_id);
            bool joined  = false;
            bool primary = false;  // we ignore any 'joins' reported about participants
                                   // that come from voice servers that aren't their primary
                                   // voice server.  This will happen with cross-region voice
                                   // where a participant on a neighboring region may be
                                   // connected to multiple servers.  We don't want to
                                   // add new identical participants from all of those servers.
            if (voice_data[participant_id].isMember("j"))
            {
                // a new participant has announced that they're joining.
                joined  = true;
                primary = voice_data[participant_id]["j"].get("p", Json::Value(false)).asBool();

                // track incoming participants that are muted so we can mute their connections (or set their volume)
                bool isMuted = LLMuteList::getInstance()->isMuted(agent_id, LLMute::flagVoiceChat);
                if (isMuted)
                {
                    mute[participant_id] = true;
                }
                else {
                    mute[participant_id] = false;
                }
                F32 volume;
                if(LLSpeakerVolumeStorage::getInstance()->getSpeakerVolume(agent_id, volume))
                {
                    user_gain[participant_id] = (uint32_t)(volume * 200);
                }
            }

            new_participant |= joined;
            if (!participant && joined && (primary || !isSpatial()))
            {
                participant = LLWebRTCVoiceClient::getInstance()->addParticipantByID(mChannelID, agent_id, mRegionID);
            }

            if (participant)
            {
                if (voice_data[participant_id].get("l", Json::Value(false)).asBool())
                {
                    // an existing participant is leaving.
                    if (agent_id != gAgentID)
                    {
                        LLWebRTCVoiceClient::getInstance()->removeParticipantByID(mChannelID, agent_id, mRegionID);
                    }
                }
                else
                {
                    // we got a 'power' update.
                    F32 level = (F32) (voice_data[participant_id].get("p", Json::Value(participant->mLevel)).asInt()) / 128;
                    // convert to decibles
                    participant->mLevel = level;

                    if (voice_data[participant_id].isMember("v"))
                    {
                        participant->mIsSpeaking = voice_data[participant_id].get("v", Json::Value(false)).asBool();
                    }
                }
            }
        }

        // tell the simulator to set the mute and volume data for this
        // participant, if there are any updates.
        Json::FastWriter writer;
        Json::Value      root     = Json::objectValue;
        if (mute.size() > 0)
        {
            root["m"] = mute;
        }
        if (user_gain.size() > 0)
        {
            root["ug"] = user_gain;
        }
        if (root.size() > 0)
        {
            std::string json_data = writer.write(root);
            mWebRTCDataInterface->sendData(json_data, false);
        }
    }
}
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
void LLVoiceWebRTCConnection::sendData(const std::string &data)
{
    if (getVoiceConnectionState() == VOICE_STATE_SESSION_UP && mWebRTCDataInterface)
    {
        mWebRTCDataInterface->sendData(data, false);
    }
}
void LLVoiceWebRTCConnection::processIceUpdates()
{
    mOutstandingRequests++;

    // LLCoros::getInstance()->launch("LLVoiceWebRTCConnection::processIceUpdatesCoro",
    //                                boost::bind(&LLVoiceWebRTCConnection::processIceUpdatesCoro, this->shared_from_this()));
    processIceUpdatesCoro(this->shared_from_this());    
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
void LLVoiceWebRTCConnection::processIceUpdatesCoro(connectionPtr_t connection)
{
    if (connection->mShutDown || LLWebRTCVoiceClient::isShuttingDown())
    {
        connection->mOutstandingRequests--;
        return;
    }

    bool iceCompleted = false;
    LLSD body;
    if (!connection->mIceCandidates.empty() || connection->mIceCompleted)
    {
        LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(connection->mRegionID);
        if (!regionp || !regionp->capabilitiesReceived())
        {
            LL_DEBUGS("Voice") << "no capabilities for ice gathering; waiting " << LL_ENDL;
            connection->mOutstandingRequests--;
            return;
        }

        std::string url = regionp->getCapability("VoiceSignalingRequest");
        if (url.empty())
        {
            connection->mOutstandingRequests--;
            return;
        }

        LL_DEBUGS("Voice") << "region ready to complete voice signaling; url=" << url << LL_ENDL;
        if (!connection->mIceCandidates.empty())
        {
            LLSD candidates = LLSD::emptyArray();
            for (auto &ice_candidate : connection->mIceCandidates)
            {
                LLSD body_candidate;
                body_candidate["sdpMid"]        = ice_candidate.mSdpMid;
                body_candidate["sdpMLineIndex"] = ice_candidate.mMLineIndex;
                body_candidate["candidate"]     = ice_candidate.mCandidate;
                candidates.append(body_candidate);
            }
            body["candidates"] = candidates;
            connection->mIceCandidates.clear();
        }
        else if (connection->mIceCompleted)
        {
            LLSD body_candidate;
            body_candidate["completed"] = true;
            body["candidate"]           = body_candidate;
            iceCompleted                = connection->mIceCompleted;
            connection->mIceCompleted   = false;
        }

        body["viewer_session"]    = connection->mViewerSession;
        body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;

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
            if(res != CURLE_OK) {
                LL_INFOS() << "requestVoiceConnection failed " << curl_easy_strerror(res) << LL_ENDL;
                connection->setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
            }
            free(chunk.response);
            curl_easy_cleanup(curl);
        }
    }
    connection->mOutstandingRequests--;
}

void LLVoiceWebRTCConnection::setMuteMic(bool muted)
{
    mMuted = muted;
    if (mWebRTCAudioInterface)
    {
        mWebRTCAudioInterface->setMute(muted);
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

void LLVoiceWebRTCConnection::setUserVolume(const LLUUID &id, F32 volume)
{
    Json::Value root = Json::objectValue;
    Json::Value user_gain = Json::objectValue;
    user_gain[id.asString()] = (uint32_t)(volume*200);  // give it two decimal places with a range from 0-200, where 100 is normal
    root["ug"] = user_gain;
    Json::FastWriter writer;
    std::string json_data = writer.write(root);
    if (mWebRTCDataInterface)
    {
        mWebRTCDataInterface->sendData(json_data, false);
    }
}

void LLVoiceWebRTCConnection::setUserMute(const LLUUID &id, bool mute)
{
    


    Json::Value root = Json::objectValue;
    Json::Value user_mute = Json::objectValue;
    user_mute[id.asString()] = mute ;
    root["m"] = user_mute;
    Json::FastWriter writer;
    std::string json_data = writer.write(root);


    if (mWebRTCDataInterface)
    {
        mWebRTCDataInterface->sendData(json_data, false);
    }
}
static llwebrtc::LLWebRTCPeerConnectionInterface::InitOptions getConnectionOptions()
{
    llwebrtc::LLWebRTCPeerConnectionInterface::InitOptions options;
    llwebrtc::LLWebRTCPeerConnectionInterface::InitOptions::IceServers servers;

    // TODO: Pull these from login
    //std::string grid = LLGridManager::getInstance()->getGridLoginID();

    //TODO update the defualt grid file to add the loginID property (agni or aditi)
    std::string gridnick = gHippoGridManager->getCurrentGridNick();
    std::string grid = "agni";
    if (gridnick == "secondlife_beta" ) {
        grid = "aditi";
    }
    //std::transform(grid.begin(), grid.end(), grid.begin(), [](unsigned char c){ return std::tolower(c); });
    int num_servers = 2;
    if (grid == "agni")
    {
        num_servers = 3;
    }
    for (int i=1; i <= num_servers; i++)
    {
        servers.mUrls.push_back(llformat("stun:stun%d.%s.secondlife.io:3478", i, grid.c_str()));
    }
    options.mServers.push_back(servers);
    return options;
}
bool LLVoiceWebRTCConnection::connectionStateMachine()
{
    processIceUpdates();
    
    switch (getVoiceConnectionState())
    {
        case VOICE_STATE_START_SESSION:
        {
            
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
                break;
            }
            mIceCompleted = false;
            setVoiceConnectionState(VOICE_STATE_WAIT_FOR_SESSION_START);

            // tell the webrtc library that we want a connection.  The library will
            // respond with an offer on a separate thread, which will cause
            // the session state to change.
            if (!mWebRTCPeerConnectionInterface->initializeConnection(getConnectionOptions()))
            {
                setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
            }
            break;
        }

        case VOICE_STATE_WAIT_FOR_SESSION_START:
        {
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
            }
            break;
        }

        case VOICE_STATE_REQUEST_CONNECTION:
            if (mShutDown)
            {
                setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
                break;
            }
            // Ask the sim to ask the Secondlife WebRTC server for a connection to
            // a given voice channel.  On completion, we'll move on to the
            // VOICE_STATE_SESSION_ESTABLISHED via a callback on a webrtc thread.
            setVoiceConnectionState(VOICE_STATE_CONNECTION_WAIT);
            // LLCoros::getInstance()->launch("LLVoiceWebRTCConnection::requestVoiceConnectionCoro",
            //                                boost::bind(&LLVoiceWebRTCConnection::requestVoiceConnectionCoro, this->shared_from_this()));
            requestVoiceConnectionCoro(this->shared_from_this());
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
                if (isSpatial())
                {
                    LLWebRTCVoiceClient::getInstance()->updatePosition();
                    LLWebRTCVoiceClient::getInstance()->sendPositionUpdate(true);
                }
            }
            break;
        }

        case VOICE_STATE_SESSION_UP:
        {
            mRetryWaitPeriod = 0;
            mRetryWaitSecs   = ((F32) rand() / (RAND_MAX)) + 0.5;
            LLUUID agentRegionID;
            if (isSpatial() && gAgent.getRegion())
            {

                bool primary = (mRegionID == gAgent.getRegion()->getRegionID());
                if (primary != mPrimary)
                {
                    mPrimary = primary;
                    sendJoin();
                }
            }

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
            if (!LLWebRTCVoiceClient::isShuttingDown())
            {
                setVoiceConnectionState(VOICE_STATE_WAIT_FOR_EXIT);
                // LLCoros::instance().launch("LLVoiceWebRTCConnection::breakVoiceConnectionCoro",
                //                            boost::bind(&LLVoiceWebRTCConnection::breakVoiceConnectionCoro, this->shared_from_this()));
                breakVoiceConnectionCoro(this->shared_from_this());
            }
            else
            {
                // llwebrtc::terminate() is already shuting down the connection.
                setVoiceConnectionState(VOICE_STATE_WAIT_FOR_CLOSE);
                mOutstandingRequests++;
            }
            break;

        case VOICE_STATE_WAIT_FOR_EXIT:
            break;

        case VOICE_STATE_SESSION_EXIT:
        {
            setVoiceConnectionState(VOICE_STATE_WAIT_FOR_CLOSE);
            mOutstandingRequests++;
            if (!LLWebRTCVoiceClient::isShuttingDown())
            {
                mWebRTCPeerConnectionInterface->shutdownConnection();
            }
            // else was already posted by llwebrtc::terminate().
            break;
        case VOICE_STATE_WAIT_FOR_CLOSE:
            break;
        case VOICE_STATE_CLOSED:
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

void LLVoiceWebRTCConnection::OnVoiceConnectionRequestSuccess(const LLSD &result)
{
    if (LLWebRTCVoiceClient::isShuttingDown())
    {
        return;
    }
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
        setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
        return;
    }

    LL_DEBUGS("Voice") << "ProvisionVoiceAccountRequest response"
                       << " channel sdp " << mRemoteChannelSDP << LL_ENDL;
    mWebRTCPeerConnectionInterface->AnswerAvailable(mRemoteChannelSDP);
}

void LLVoiceWebRTCConnection::breakVoiceConnectionCoro(connectionPtr_t connection)
{
    LL_DEBUGS("Voice") << "Disconnecting voice." << LL_ENDL;
    if (connection->mWebRTCDataInterface)
    {
        connection->mWebRTCDataInterface->unsetDataObserver(connection.get());
        connection->mWebRTCDataInterface = nullptr;
    }
    connection->mWebRTCAudioInterface   = nullptr;
    LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(connection->mRegionID);
    if (!regionp || !regionp->capabilitiesReceived())
    {
        LL_DEBUGS("Voice") << "no capabilities for voice provisioning; waiting " << LL_ENDL;
        connection->setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        connection->mOutstandingRequests--;
        return;
    }

    std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
    if (url.empty())
    {
        connection->setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        connection->mOutstandingRequests--;
        return;
    }

    LL_DEBUGS("Voice") << "region ready for voice break; url=" << url << LL_ENDL;

    //LLVoiceWebRTCStats::getInstance()->provisionAttemptStart();
    LLSD body;
    body["logout"]         = true;
    body["viewer_session"] = connection->mViewerSession;
    body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;

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
        
        free(chunk.response);
        curl_easy_cleanup(curl);
    }
    connection->mOutstandingRequests--;

    if (connection->getVoiceConnectionState() == VOICE_STATE_WAIT_FOR_EXIT)
    {
        connection->setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
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
void LLWebRTCVoiceClient::onAvatarNameCache(const LLUUID &agent_id, const LLAvatarName &av_name)
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

LLWebRTCVoiceClient::sessionState::sessionState() :
    mHangupOnLastLeave(false),
    mNotifyOnFirstJoin(false),
    mMuted(false),
    mSpeakerVolume(1.0),
    mShuttingDown(false)
{
}
//  participantState level participant management
LLWebRTCVoiceClient::participantState::participantState(const LLUUID& agent_id, const LLUUID& region) :
     mURI(agent_id.asString()),
     mAvatarID(agent_id),
     mIsSpeaking(false),
     mIsModeratorMuted(false),
     mOnMuteList(false),
     mLevel(0.f),
     mVolume(LLVoiceClient::VOLUME_DEFAULT),
     mRegion(region)
{
}
LLWebRTCVoiceClient::participantStatePtr_t LLWebRTCVoiceClient::sessionState::addParticipant(const LLUUID& agent_id, const LLUUID& region)
{

    

    participantStatePtr_t result;

    participantUUIDMap::iterator iter = mParticipantsByUUID.find(agent_id);

    if (iter != mParticipantsByUUID.end())
    {
        result = iter->second;
        result->mRegion = region;
    }

    if (!result)
    {
        // participant isn't already in one list or the other.
        result.reset(new participantState(agent_id, region));
        mParticipantsByUUID.insert(participantUUIDMap::value_type(agent_id, result));
        result->mAvatarID = agent_id;
    }

    LLWebRTCVoiceClient::getInstance()->lookupName(agent_id);

    LLSpeakerVolumeStorage::getInstance()->getSpeakerVolume(result->mAvatarID, result->mVolume);
    if (!LLWebRTCVoiceClient::sShuttingDown)
    {
        LLWebRTCVoiceClient::getInstance()->notifyParticipantObservers();
    }

    LL_DEBUGS("Voice") << "Participant \"" << result->mURI << "\" added." << LL_ENDL;

    return result;
}
void LLWebRTCVoiceClient::sessionState::removeParticipant(const LLWebRTCVoiceClient::participantStatePtr_t &participant)
{
   

    if (participant)
    {
        LLUUID participantID = participant->mAvatarID;
        participantUUIDMap::iterator iter = mParticipantsByUUID.find(participant->mAvatarID);

        LL_DEBUGS("Voice") << "participant \"" << participant->mURI << "\" (" << participantID << ") removed." << LL_ENDL;

        if (iter == mParticipantsByUUID.end())
        {
            LL_WARNS("Voice") << "Internal error: participant ID " << participantID << " not in UUID map" << LL_ENDL;
        }
        else
        {
            mParticipantsByUUID.erase(iter);
            if (!LLWebRTCVoiceClient::sShuttingDown)
            {
                LLWebRTCVoiceClient::getInstance()->notifyParticipantObservers();
            }
        }
        if (mHangupOnLastLeave && (participantID != gAgentID) && (mParticipantsByUUID.size() <= 1) && LLWebRTCVoiceClient::instanceExists())
        {
            LLWebRTCVoiceClient::getInstance()->notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL);
        }
    }
}

void LLWebRTCVoiceClient::sessionState::removeAllParticipants(const LLUUID &region)
{
    std::vector<participantStatePtr_t> participantsToRemove;

    for (auto& participantEntry : mParticipantsByUUID)
    {
        if (region.isNull() || (participantEntry.second->mRegion == region))
        {
            participantsToRemove.push_back(participantEntry.second);
        }
    }
    for (auto& participant : participantsToRemove)
    {
        removeParticipant(participant);
    }
}
// session-level participant management

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
/*static*/
LLWebRTCVoiceClient::sessionState::ptr_t LLWebRTCVoiceClient::sessionState::matchSessionByChannelID(const std::string& channel_id)
{
    sessionStatePtr_t result;

    // *TODO: My kingdom for a lambda!
    std::map<std::string, ptr_t>::iterator it = sSessions.find(channel_id);
    if (it != sSessions.end())
    {
        result = (*it).second;
    }
    return result;
}
void LLWebRTCVoiceClient::sessionState::for_each(sessionFunc_t func)
{
    std::for_each(sSessions.begin(), sSessions.end(), boost::bind(for_eachPredicate, _1, func));
}

void LLWebRTCVoiceClient::sessionState::reapEmptySessions()
{
    std::map<std::string, ptr_t>::iterator iter;
    for (iter = sSessions.begin(); iter != sSessions.end();)
    {
        if (iter->second->isEmpty())
        {
            iter = sSessions.erase(iter);
        }
        else
        {
            ++iter;
        }
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
void LLWebRTCVoiceClient::sessionState::shutdownAllConnections()
{
    mShuttingDown = true;
    for (auto &&connection : mWebRTCConnections)
    {
        connection->shutDown();
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
   
    auto iter = sSessions.begin();
    while (iter != sSessions.end())
    {
        if (!iter->second->processConnectionStates() && iter->second->mShuttingDown)
        {
            // if the connections associated with a session are gone,
            // and this session is shutting down, remove it.
            iter = sSessions.erase(iter);
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
void LLWebRTCVoiceClient::sessionState::sendData(const std::string &data)
{
    for (auto &connection : mWebRTCConnections)
    {
        connection->sendData(data);
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

void LLWebRTCVoiceClient::sessionState::setSpeakerVolume(F32 volume)
{
    mSpeakerVolume = volume;
    for (auto &connection : mWebRTCConnections)
    {
        connection->setSpeakerVolume(volume);
    }
}

void LLWebRTCVoiceClient::sessionState::setUserVolume(const LLUUID &id, F32 volume)
{
    if (mParticipantsByUUID.find(id) == mParticipantsByUUID.end())
    {
        return;
    }
    for (auto &connection : mWebRTCConnections)
    {
        connection->setUserVolume(id, volume);
    }
}
void LLWebRTCVoiceClient::sessionState::setUserMute(const LLUUID &id, bool mute)
{
    if (mParticipantsByUUID.find(id) == mParticipantsByUUID.end())
    {
        return;
    }
    for (auto &connection : mWebRTCConnections)
    {
        connection->setUserMute(id, mute);
    }
}
/*static*/
void LLWebRTCVoiceClient::sessionState::addSession(
    const std::string & channelID,
    LLWebRTCVoiceClient::sessionState::ptr_t& session)
{
    sSessions[channelID] = session;
}
LLWebRTCVoiceClient::sessionState::~sessionState()
{
    LL_DEBUGS("Voice") << "Destroying session CHANNEL=" << mChannelID << LL_ENDL;

    if (!mShuttingDown)
    {
        shutdownAllConnections();
    }
    mWebRTCConnections.clear();

    removeAllParticipants();
}
void LLWebRTCVoiceClient::sessionState::clearSessions()
{
    sSessions.clear();
}
// processing of spatial voice connection states requires special handling.
// as neighboring regions need to be started up or shut down depending
// on our location.
bool LLWebRTCVoiceClient::estateSessionState::processConnectionStates()
{

    if (!mShuttingDown)
    {
        // Estate voice requires connection to neighboring regions.
        std::set<LLUUID> neighbor_ids = LLWebRTCVoiceClient::getInstance()->getNeighboringRegions();

        for (auto &connection : mWebRTCConnections)
        {
            std::shared_ptr<LLVoiceWebRTCSpatialConnection> spatialConnection =
                std::static_pointer_cast<LLVoiceWebRTCSpatialConnection>(connection);

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
            connection->setMuteMic(mMuted);
            connection->setSpeakerVolume(mSpeakerVolume);
        }
    }
    return LLWebRTCVoiceClient::sessionState::processConnectionStates();
}

// Various session state constructors.

LLWebRTCVoiceClient::estateSessionState::estateSessionState()
{
    mHangupOnLastLeave = false;
    mNotifyOnFirstJoin = false;
    mChannelID         = "Estate";
    LLUUID region_id   = gAgent.getRegion()->getRegionID();

    mWebRTCConnections.emplace_back(new LLVoiceWebRTCSpatialConnection(region_id, INVALID_PARCEL_ID, "Estate"));
}
LLWebRTCVoiceClient::parcelSessionState::parcelSessionState(const std::string &channelID, S32 parcel_local_id)
{
    mHangupOnLastLeave = false;
    mNotifyOnFirstJoin = false;
    LLUUID region_id   = gAgent.getRegion()->getRegionID();
    mChannelID         = channelID;
    mWebRTCConnections.emplace_back(new LLVoiceWebRTCSpatialConnection(region_id, parcel_local_id, channelID));
}

LLWebRTCVoiceClient::adhocSessionState::adhocSessionState(const std::string &channelID,
                                                          const std::string &credentials,
                                                          bool notify_on_first_join,
                                                          bool hangup_on_last_leave) :
    mCredentials(credentials)
{
    mHangupOnLastLeave = hangup_on_last_leave;
    mNotifyOnFirstJoin = notify_on_first_join;
    LLUUID region_id   = gAgent.getRegion()->getRegionID();
    mChannelID         = channelID;
    mWebRTCConnections.emplace_back(new LLVoiceWebRTCAdHocConnection(region_id, channelID, credentials));
}


/////////////////////////////
// WebRTC Spatial Connection

LLVoiceWebRTCSpatialConnection::LLVoiceWebRTCSpatialConnection(const LLUUID &regionID,
                                                               S32 parcelLocalID,
                                                               const std::string &channelID) :
    LLVoiceWebRTCConnection(regionID, channelID),
    mParcelLocalID(parcelLocalID)
{
    if (gAgent.getRegion())
    {
        mPrimary = (regionID == gAgent.getRegion()->getRegionID());
    }
}

LLVoiceWebRTCSpatialConnection::~LLVoiceWebRTCSpatialConnection()
{
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

        // try again.
        setVoiceConnectionState(VOICE_STATE_REQUEST_CONNECTION);
        return;
    }

    std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
    if (url.empty())
    {
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    LL_DEBUGS("Voice") << "region ready for voice provisioning; url=" << url << LL_ENDL;

    
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
    mOutstandingRequests++;
    // LLSD result = httpAdapter->postAndSuspend(httpRequest, url, body, httpOpts);

    // LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    // LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);
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

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ll_print_sd(body));
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		res = curl_easy_perform(curl);
        LL_INFOS() <<"request voice connection " << ll_print_sd(body) << LL_ENDL;
        if(res != CURLE_OK){
			LL_INFOS() << "requestVoiceConnection failed " << curl_easy_strerror(res) << LL_ENDL;
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            switch (http_code)
            {
                case 409:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_FULL;
                    break;
                case 401:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_LOCKED;
                    break;
                default:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_UNKNOWN;
                    break;
            }
            setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
		}else {
            LL_INFOS() << "requestVoiceConnection successed " << chunk.response << LL_ENDL;
			LLSD result = LLSD();
			std::istringstream resultstream = std::istringstream(chunk.response);
			LLSDSerialize::fromXML(result, resultstream);
            OnVoiceConnectionRequestSuccess(result);
        }
    }


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
    //     setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
    // }
    mOutstandingRequests--;
}


/////////////////////////////
// WebRTC Spatial Connection

LLVoiceWebRTCAdHocConnection::LLVoiceWebRTCAdHocConnection(const LLUUID &regionID,
                                                           const std::string& channelID,
                                                           const std::string& credentials) :
    LLVoiceWebRTCConnection(regionID, channelID),
    mCredentials(credentials)
{
}

LLVoiceWebRTCAdHocConnection::~LLVoiceWebRTCAdHocConnection()
{
}
// Add-hoc connections require a different channel type
// as they go to a different set of Secondlife WebRTC servers.
// They also require credentials for the given channels.
// So, we have a separate requestVoiceConnection call.
void LLVoiceWebRTCAdHocConnection::requestVoiceConnection()
{
    LLViewerRegion *regionp = LLWorld::instance().getRegionFromID(mRegionID);

    LL_DEBUGS("Voice") << "Requesting voice connection." << LL_ENDL;
    if (!regionp || !regionp->capabilitiesReceived())
    {
        LL_DEBUGS("Voice") << "no capabilities for voice provisioning; retrying " << LL_ENDL;
        // try again.
        setVoiceConnectionState(VOICE_STATE_REQUEST_CONNECTION);
        return;
    }

    std::string url = regionp->getCapability("ProvisionVoiceAccountRequest");
    if (url.empty())
    {
        setVoiceConnectionState(VOICE_STATE_SESSION_RETRY);
        return;
    }

    
    LLSD body;
    LLSD jsep;
    jsep["type"] = "offer";
    {
        jsep["sdp"] = mChannelSDP;
    }
    body["jsep"] = jsep;
    body["credentials"] = mCredentials;
    body["channel"] = mChannelID;
    body["channel_type"] = "multiagent";
    body["voice_server_type"] = WEBRTC_VOICE_SERVER_TYPE;

    // LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
    //     new LLCoreHttpUtil::HttpCoroutineAdapter("LLVoiceWebRTCAdHocConnection::requestVoiceConnection",
    //                                              LLCore::HttpRequest::DEFAULT_POLICY_ID));
    // LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    // LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);

    // httpOpts->setWantHeaders(true);
    // mOutstandingRequests++;
    // LLSD result = httpAdapter->postAndSuspend(httpRequest, url, body, httpOpts);

    // LLSD               httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    // LLCore::HttpStatus status      = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);
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

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		/* Now specify the POST data */
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ll_print_sd(body));
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		res = curl_easy_perform(curl);

        if(res != CURLE_OK){
			LL_INFOS() << "requestVoiceConnection failed " << curl_easy_strerror(res) << LL_ENDL;
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            switch (http_code)
            {
                case 409:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_FULL;
                    break;
                case 401:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_CHANNEL_LOCKED;
                    break;
                default:
                    mCurrentStatus = LLVoiceClientStatusObserver::ERROR_UNKNOWN;
                    break;
            }
            setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
		} else {
            LL_INFOS() << "requestVoiceConnection successed " << chunk.response << LL_ENDL;
			LLSD result = LLSD();
			std::istringstream resultstream = std::istringstream(chunk.response);
			LLSDSerialize::fromXML(result, resultstream);
            OnVoiceConnectionRequestSuccess(result);
        }
    }
    // if (!status)
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
    //     setVoiceConnectionState(VOICE_STATE_SESSION_EXIT);
    // }
    // else
    // {
    //     OnVoiceConnectionRequestSuccess(result);
    // }
    mOutstandingRequests--;
}