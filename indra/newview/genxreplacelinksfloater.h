#ifndef LL_GENXREPLACELINKSFLOATER_H
#define LL_GENXREPLACELINKSFLOATER_H

#include "llfloater.h"

class GenxFloaterReplaceLinks
:	public LLFloater
{
public:

	GenxFloaterReplaceLinks(const LLUUID& id);
    BOOL postBuild();
    void onInventoryItemDropped();
    void onCloseButton();
    void onReplaceButton();
    void decreaseCounter(){mCounter--;}
    int getCounter() {return mCounter;}
    static void itemRemovedCallback(LLHandle<GenxFloaterReplaceLinks> floater_handle,const LLUUID& outfit_folder_id,const LLUUID& baseoutfit_id);
    static void createLinkCallback(LLHandle<GenxFloaterReplaceLinks> floater_handle,const LLUUID& link_id,const LLUUID& outfit_folder_id,const LLUUID& baseoutfit_id);
private:
    std::string link_id;    
    LLUUID mLinkedItemsToReplaceOriginalUUID;
    LLUUID mReplaceByOriginalUUID;
    void countItemLinks();
    int mCounter;
};
#endif