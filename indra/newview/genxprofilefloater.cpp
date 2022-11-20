#include "llviewerprecompiledheaders.h"

#include "genxprofilefloater.h"
#include "lluictrlfactory.h"
#include "llavatarpropertiesprocessor.h"
#include "lltexturectrl.h"
#include "llnameeditor.h"
GenxFloaterAvatarInfo::floater_positions_t GenxFloaterAvatarInfo::floater_positions;
LLUUID GenxFloaterAvatarInfo::lastMoved;

GenxFloaterAvatarInfo::GenxFloaterAvatarInfo(const std::string& name, const LLUUID &avatar_id)
:	LLFloater(name), LLInstanceTracker<GenxFloaterAvatarInfo, LLUUID>(avatar_id),
	mAvatarID(avatar_id)
{
	setAutoFocus(true);

	LLCallbackMap::map_t factory_map;
	
	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_floater_profile.xml");
	setTitle(name);
    LLAvatarPropertiesProcessor::getInstance()->addObserver(mAvatarID, this);
    LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(mAvatarID);
    getChild<LLNameEditor>("dnname")->setNameID(avatar_id, LFIDBearer::AVATAR);

}
void GenxFloaterAvatarInfo::handleReshape(const LLRect& new_rect, bool by_user) {

	if (by_user && !isMinimized()) {
		GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=new_rect;
		GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
		gSavedSettings.setRect("FloaterProfileRect",new_rect);
	}
	LLFloater::handleReshape(new_rect, by_user);
}
void GenxFloaterAvatarInfo::setOpenedPosition() {
	if (GenxFloaterAvatarInfo::lastMoved.isNull()) {
		LLRect lastfloaterProfilePosition = gSavedSettings.getRect("FloaterProfileRect");
		if (lastfloaterProfilePosition.mLeft==0 && lastfloaterProfilePosition.mTop==0) {
			this->center();
		}
		else {
			this->setRect(lastfloaterProfilePosition);
		}
		GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
		GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
	} else {
		
		//LLRect lastFloaterPosition = GenxFloaterAvatarInfo::floater_positions[this->mAvatarID.asString()];
		floater_positions_t::iterator it = floater_positions.find(this->mAvatarID);
		if(it == floater_positions.end())
		{
			
			floater_positions_t::iterator lasFloaterMovedPosition = floater_positions.find(GenxFloaterAvatarInfo::lastMoved);
			this->setRect(lasFloaterMovedPosition->second);
			GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
			GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
			
		} else {
			
			this->setRect(it->second);
		}
		
	}

	BOOL overlapse=TRUE;
	while (overlapse) {
		//if I am on an existing floater
		overlapse=FALSE;
		for(floater_positions_t::iterator it = floater_positions.begin(); it != floater_positions.end();++it )
		{
			if((*it).second == getRect() && (*it).first != this->mAvatarID)
			{
				
				this->translate(20,-20);
				GenxFloaterAvatarInfo::floater_positions[this->mAvatarID]=this->getRect();
				GenxFloaterAvatarInfo::lastMoved=this->mAvatarID;
				
				overlapse=TRUE;
			}
			
			
			
		}
		
	}
	
}
// virtual
void GenxFloaterAvatarInfo::processProperties(void* data, EAvatarProcessorType type)
{
    const LLAvatarData* pAvatarData = static_cast<const LLAvatarData*>( data );
    if (pAvatarData && (mAvatarID == pAvatarData->avatar_id) && (pAvatarData->avatar_id != LLUUID::null))  
    {
        if(type == APT_PROPERTIES)
	    {
            getChild<LLTextureCtrl>("img2ndLife")->setImageAssetID(pAvatarData->image_id);
        }
    }
}
// virtual
GenxFloaterAvatarInfo::~GenxFloaterAvatarInfo()
{
    LLAvatarPropertiesProcessor::getInstance()->removeObserver(mAvatarID, this);
}