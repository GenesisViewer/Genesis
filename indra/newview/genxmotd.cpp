#include "llviewerprecompiledheaders.h"
#include "genxmotd.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "lltimer.h"

GenxMOTD::GenxMOTD()
{
	U64 expireTime=1704067200000000;
	
	U32 countLogins = gSavedSettings.getU32("GenxCountLogins");
	countLogins++;
	gSavedSettings.setU32("GenxCountLogins",countLogins);
	U64 epochTime = LLTimer::getTotalTime();
	mShowGenxMOTD = (expireTime>epochTime) && (countLogins % 25 == 0);
	LL_INFOS() << "Expire Time " << expireTime << " total time " << epochTime << LL_ENDL;
	LL_INFOS() << "Genx MOTD " << mShowGenxMOTD << LL_ENDL;
}
std::string GenxMOTD::getMOTD() {
	if (mShowGenxMOTD) {
		
		gSavedSettings.setU32("GenxCountLogins",0);
		return LLTrans::getString("GenxMOTD");
	} else {
		return "";
	}
}