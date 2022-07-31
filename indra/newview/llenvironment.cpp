#include "llviewerprecompiledheaders.h"
#include "llenvironment.h"
#include <stdio.h>



LLEnvironment::LLEnvironment() 
{
	
}
LLEnvironment::~LLEnvironment()
{
	
}

std::string env_selection_to_string(LLEnvironment::EnvSelection_t sel)
{
#define RTNENUM(E) case LLEnvironment::E: return #E
    switch (sel){
        RTNENUM(ENV_EDIT);
        RTNENUM(ENV_LOCAL);
        RTNENUM(ENV_PUSH);
        RTNENUM(ENV_PARCEL);
        RTNENUM(ENV_REGION);
        RTNENUM(ENV_DEFAULT);
        RTNENUM(ENV_END);
        RTNENUM(ENV_CURRENT);
        RTNENUM(ENV_NONE);
    default:
        return llformat("Unknown(%d)", sel);
    }
#undef RTNENUM
}

void LLEnvironment::setManualEnvironment(EnvSelection_t env, const LLUUID &assetId, S32 env_version)
{
    //genesis comment
    //LLSettingsBase::Seconds transitionTime(static_cast<F64>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
    //setEnvironment(env, assetId, transitionTime, env_version);
    //genesis comment
}

void LLEnvironment::setSelectedEnvironment(LLEnvironment::EnvSelection_t env, LLSettingsBase::Seconds transition, bool forced)
{
// [RLVa:KB] - Checked: RLVa-2.4 (@setenv)
//Genesis comment
//    if ( (!RlvActions::canChangeEnvironment()) && (LLEnvironment::ENV_EDIT != env) )
 //   {
  //      return;
  //  }
//Genesis comment

// [/RLVa:KB]

    mSelectedEnvironment = env;
    updateEnvironment(transition, forced);
    LL_DEBUGS("ENVIRONMENT") << "Setting environment " << env_selection_to_string(env) << " with transition: " << transition << LL_ENDL;
}
void LLEnvironment::updateEnvironment(LLSettingsBase::Seconds transition, bool forced)
{
    //Genesis TODO real update
}


const F64Seconds LLEnvironment::TRANSITION_DEFAULT(5.0f);
const S32 LLEnvironment::NO_VERSION(-3);