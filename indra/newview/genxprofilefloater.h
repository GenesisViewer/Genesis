
#ifndef LL_GENXPROFILEFLOATER_H
#define LL_GENXPROFILEFLOATER_H

#include "llfloater.h"
#include "llinstancetracker.h"
#include "llavatarpropertiesprocessor.h"
#include "statemachine/aifilepicker.h"
#include "llcororesponder.h"


class GenxFloaterAvatarInfo
:	public LLFloater, public LLInstanceTracker<GenxFloaterAvatarInfo,LLUUID>, public LLAvatarPropertiesObserver
{
public:

	GenxFloaterAvatarInfo(const std::string& name, const LLUUID &avatar_id);
	/*virtual*/ ~GenxFloaterAvatarInfo();
	void resetGroupList();
	void handleReshape(const LLRect& new_rect, bool by_user = false);
	void setOpenedPosition();
	void updateData();
	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
    /*virtual*/void processProperties(void* data, EAvatarProcessorType type);
	
private:
	LLUUID			mAvatarID;			// for which avatar is this window?
	typedef std::map<LLUUID, LLRect> floater_positions_t;
	void setOnlineStatus(bool online_status);
    void onSelectedContactSet();
    void manageMuteButtons();
    void toggleBlock();
	void onClickCopy(const LLSD& val);
	void loadPicks();
	void pickNext();
	void pickPrev();
	void onSelectedPick();
	bool onPreSelectedPick();
	void displayPick(LLUUID pickId);
	void onClickPickTeleport();
	void onClicPickkMap();	
	void onClickUploadPhoto();
	void onClickChangePhoto();
	void saveProfile();
	void sendAvatarPropertiesUpdate();
	void onTextureSelectionChanged(LLInventoryItem* itemp);
	void sl_filepicker_callback(AIFilePicker* picker);
	void sl_http_upload_first_step(const LLCoroResponder& responder,std::string filename);
	void sl_http_upload_second_step(const LLCoroResponder& responder,std::string filename);
	static floater_positions_t floater_positions;
	static LLUUID lastMoved;
	std::map<LLUUID,LLSD > mPicks;
	
};

#endif
