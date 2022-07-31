#include <stdio.h>
#include "llsettingsbase.h"



#ifndef LLENVIRONMENT_H
#define LLENVIRONMENT_H

class LLEnvironment : public LLSingleton<LLEnvironment>
{
    
public:
	LLEnvironment();
	~LLEnvironment();
    enum EnvSelection_t
    {
        ENV_EDIT = 0,
        ENV_LOCAL,
        ENV_PUSH,
        ENV_PARCEL,
        ENV_REGION,
        ENV_DEFAULT,
        ENV_END,
        ENV_CURRENT = -1,
        ENV_NONE = -2
    };
    static const F64Seconds TRANSITION_DEFAULT;
    static const S32  NO_VERSION;
    void setManualEnvironment(EnvSelection_t env, const LLUUID &assetId, S32 env_version = NO_VERSION);
    void setSelectedEnvironment(EnvSelection_t env, LLSettingsBase::Seconds transition = TRANSITION_DEFAULT, bool forced = false);    
    void updateEnvironment(LLSettingsBase::Seconds transition = TRANSITION_DEFAULT, bool forced = false);
private:
    EnvSelection_t  mSelectedEnvironment;
};

#endif /* LLENVIRONMENT_H */
