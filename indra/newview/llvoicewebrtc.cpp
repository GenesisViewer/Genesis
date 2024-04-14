#include "llviewerprecompiledheaders.h"
#include "llvoicewebrtc.h"
bool LLWebRTCVoiceClient::sShuttingDown = false;
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