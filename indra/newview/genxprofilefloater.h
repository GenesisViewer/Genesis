
#ifndef LL_GENXPROFILEFLOATER_H
#define LL_GENXPROFILEFLOATER_H

#include "llfloater.h"
#include "llinstancetracker.h"
#include "llavatarpropertiesprocessor.h"


class GenxFloaterAvatarInfo
:	public LLFloater, public LLInstanceTracker<GenxFloaterAvatarInfo,LLUUID>, public LLAvatarPropertiesObserver
{
public:

	GenxFloaterAvatarInfo(const std::string& name, const LLUUID &avatar_id);
	/*virtual*/ ~GenxFloaterAvatarInfo();
	void resetGroupList();
	void handleReshape(const LLRect& new_rect, bool by_user = false);
	void setOpenedPosition();
    /*virtual*/void processProperties(void* data, EAvatarProcessorType type);
	
private:
	LLUUID			mAvatarID;			// for which avatar is this window?
	typedef std::map<LLUUID, LLRect> floater_positions_t;
	void setOnlineStatus(bool online_status);
    void onSelectedContactSet();
    void manageMuteButtons();
    void toggleBlock();
	static floater_positions_t floater_positions;
	static LLUUID lastMoved;
};

#endif
