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
private:
    std::string link_id;    
    LLUUID mLinkedItemsToReplaceOriginalUUID;
    LLUUID mReplaceByOriginalUUID;
};
#endif