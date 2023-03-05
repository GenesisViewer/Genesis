#include "genxreplacelinksfloater.h"
#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "llinventory.h"
#include "llwearable.h"
#include "llinventorymodel.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "genxdroptarget.h"
#include <sstream>
GenxFloaterReplaceLinks::GenxFloaterReplaceLinks(const LLUUID& id)
:	LLFloater("Replace Links")
{
    this->link_id=id.asString();
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_replace_links.xml");
    this->center();
    this->setCanClose(true);
    
    
}
BOOL GenxFloaterReplaceLinks::postBuild() {
    LLUUID link_uuid = LLUUID(link_id);
    LLViewerInventoryItem *item =  gInventory.getItem(link_uuid);
    LL_INFOS() << "replace links find item " << link_uuid.asString() << LL_ENDL;
    if (item) {
        LL_INFOS() << "replace links  item found " << item->getName() << LL_ENDL;
        getChild<LLLineEditor>("link_name")->setText(item->getName());
    } else {
        LL_INFOS() << "replace links  item not found "  << LL_ENDL;
        this->close();
    }

    //search the original object
    LLViewerInventoryItem *itemOriginal = gInventory.getLinkedItem(link_uuid);
    if (itemOriginal) {
        //search the number of links for this item;
        LLInventoryModel::item_array_t links= gInventory.collectLinksTo(itemOriginal->getUUID());
        std::stringstream str;
        str << links.size() << " items found";
        getChild<LLTextBox>("links_found_count")->setText(str.str());
        getChild<GenxDropTarget>("drop_target_rect")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onInventoryItemDropped, this));
        mLinkedItemsToReplaceOriginalUUID = itemOriginal->getUUID();

        getChild<LLButton>("close_button")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onCloseButton, this));
        getChild<LLButton>("replacelinks_button")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onReplaceButton, this));
    }
    else {
       this->close();
    }

    return true;

   
}
void GenxFloaterReplaceLinks::onCloseButton() {
    this->close();
}
void GenxFloaterReplaceLinks::onInventoryItemDropped() {
    getChild<LLTextBox>("status_text")->setText(std::string(""));
    LLInventoryItem* item = getChild<GenxDropTarget>("drop_target_rect")->getItem();
    if (item) 
        LL_INFOS() << "Item dropped " << item->getUUID()<<LL_ENDL;

    else 
        this->close();

    if (item->getIsLinkType()) {
         LLViewerInventoryItem *itemOriginal = gInventory.getLinkedItem(item->getUUID());
         if (itemOriginal) {
            mReplaceByOriginalUUID = itemOriginal->getUUID();
         } else {
            getChild<LLTextBox>("status_text")->setText(std::string ("Broken link item"));
         }
    } else {
        mReplaceByOriginalUUID = item->getUUID();
    }   
    bool can_replace=true;
    if (mReplaceByOriginalUUID == mLinkedItemsToReplaceOriginalUUID) {
        getChild<LLTextBox>("status_text")->setText(std::string("The items are the same, won't replace"));
        can_replace=false;
    }
    getChild<LLButton>("replacelinks_button")->setEnabled(can_replace);
}

void GenxFloaterReplaceLinks::onReplaceButton() {
     LLViewerInventoryItem* replaceitem = gInventory.getItem(mReplaceByOriginalUUID);
     LLInventoryModel::item_array_t links= gInventory.collectLinksTo(mLinkedItemsToReplaceOriginalUUID);
     
     for (LLInventoryModel::item_array_t::iterator it = links.begin();
		 it != links.end();
		 ++it)
	{
        LLUUID link_id = (*it).get()->getUUID();
        LL_INFOS() << "running the replace process for " << link_id.asString() << LL_ENDL;
        LLViewerInventoryItem *item =  gInventory.getItem(link_id);
        
        const LLViewerInventoryCategory *folder = gInventory.getFirstNondefaultParent (link_id);
        
        if (folder) 
        {
            LL_INFOS() << "I have to replace " << item->getName() << " in folder " << folder->getName() << LL_ENDL;
            //creating a link
            LL_INFOS() << "Creating a link  for " << replaceitem->getName() << " in folder " << folder->getName() << LL_ENDL;
            // LLConstPointer<LLInventoryObject> baseobj = gInventory.getObject(id);
	        // link_inventory_object(category, baseobj, cb);
            LL_INFOS() << "Deleting link  for " << item->getName() << " in folder " << folder->getName() << LL_ENDL;
            

        }
        
        
    }
}