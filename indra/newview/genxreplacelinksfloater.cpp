#include "genxreplacelinksfloater.h"
#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "llinventory.h"
#include "llwearable.h"
#include "llinventorymodel.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "genxdroptarget.h"
#include "llappearancemgr.h"
#include <sstream>
bool GenxFloaterReplaceLinks::opened=false;
GenxFloaterReplaceLinks::GenxFloaterReplaceLinks(const LLUUID& id)
:	LLFloater("Replace Links")
{
    this->link_id=id.asString();
    LLUICtrlFactory::getInstance()->buildFloater(this, "floater_replace_links.xml");
    this->center();
    this->setCanClose(true);
    
    
}
BOOL GenxFloaterReplaceLinks::postBuild() {
    if (opened) this->close();
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
        
        getChild<GenxDropTarget>("drop_target_rect")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onInventoryItemDropped, this));
        mLinkedItemsToReplaceOriginalUUID = itemOriginal->getUUID();
        //search the number of links for this item;
        LLInventoryModel::item_array_t links= gInventory.collectLinksTo(mLinkedItemsToReplaceOriginalUUID);
        mCounter = links.size();
        countItemLinks();
        getChild<LLButton>("close_button")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onCloseButton, this));
        getChild<LLButton>("replacelinks_button")->setCommitCallback(boost::bind(&GenxFloaterReplaceLinks::onReplaceButton, this));
    }
    else {
       this->close();
    }
    opened = true;
    return true;

   
}
void GenxFloaterReplaceLinks::onClose(bool app_quitting)
{
	opened=false;
    LLFloater::onClose(app_quitting);
}
void GenxFloaterReplaceLinks::countItemLinks() {
    
    std::stringstream str;
    str << mCounter << " items found";
    getChild<LLTextBox>("links_found_count")->setText(str.str());
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
    this->setCanClose(false);
    getChild<LLTextBox>("status_text")->setText(std::string("Replacing links, please wait, it may be long!!"));
    getChild<LLButton>("close_button")->setEnabled(false);
    //<code from Firestorm viewer>
    const LLUUID cof_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT, false);
	const LLUUID outfit_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS, false);
    //</code from Firestorm viewer>
    LLViewerInventoryItem* replaceitem = gInventory.getItem(mReplaceByOriginalUUID);
    LLInventoryModel::item_array_t links= gInventory.collectLinksTo(mLinkedItemsToReplaceOriginalUUID);
    mCounter=links.size();
    const LLUUID baseoutfit_id = LLAppearanceMgr::instance().getBaseOutfitUUID();
    //bool need_to_update_current_outfit = FALSE;
    for (LLInventoryModel::item_array_t::iterator it = links.begin();
		 it != links.end();
		 ++it)
	{
        LLUUID link_id = (*it).get()->getUUID();
        LL_INFOS() << "running the replace process for " << link_id.asString() << LL_ENDL;
        LLViewerInventoryItem *item =  gInventory.getItem(link_id);
        
        const LLViewerInventoryCategory *folder = gInventory.getCategory (item->getParentUUID());
        if (folder->getUUID() == cof_folder_id) {
            mCounter--;
            countItemLinks();
            continue;
        }
        if (folder) 
        {
            
            LL_INFOS() << "I have to replace " << item->getName() << " in folder " << folder->getName() << LL_ENDL;
            //creating a link
            LL_INFOS() << "Creating a link  for " << replaceitem->getName() << " in folder " << folder->getName() << LL_ENDL;
            LLConstPointer<LLInventoryObject> baseobj = gInventory.getObject(replaceitem->getUUID());
            LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(&GenxFloaterReplaceLinks::createLinkCallback,getDerivedHandle<GenxFloaterReplaceLinks>(),link_id, folder->getUUID(), baseoutfit_id));
            link_inventory_object(folder->getUUID(), baseobj, cb);
            
            
        }
        
        
    }
    
        
    
}
void GenxFloaterReplaceLinks::createLinkCallback(LLHandle<GenxFloaterReplaceLinks> floater_handle,const LLUUID& link_id,const LLUUID& outfit_folder_id,const LLUUID& baseoutfit_id)
{
    
        LLPointer<LLInventoryCallback> cb2 = new LLBoostFuncInventoryCallback(boost::bind(&GenxFloaterReplaceLinks::itemRemovedCallback,floater_handle, outfit_folder_id, baseoutfit_id));
        
        remove_inventory_object(link_id,cb2);
}
// static
//this is taken from Firestorm Viewer
void GenxFloaterReplaceLinks::itemRemovedCallback(LLHandle<GenxFloaterReplaceLinks> floater_handle,const LLUUID& outfit_folder_id,const LLUUID& baseoutfit_id)
{
    LL_INFOS() << " itemRemovedCallback : Base outfit folder = " <<   baseoutfit_id.asString() << ", current folder = " << outfit_folder_id.asString() << LL_ENDL;
	if (outfit_folder_id.notNull())
	{
        
		LLAppearanceMgr::getInstance()->updateClothingOrderingInfo(outfit_folder_id);
        
	}
    if (baseoutfit_id.notNull() && baseoutfit_id==outfit_folder_id) {
        LLAppearanceMgr::instance().replaceCurrentOutfit(baseoutfit_id);
        
        
    }
    floater_handle.get()->decreaseCounter();
    if (floater_handle.get()->getCounter() == 0) {
        floater_handle.get()->getChild<LLTextBox>("links_found_count")->setText(std::string(""));
        floater_handle.get()->getChild<LLTextBox>("status_text")->setText(std::string("Replacing links is done, you can close me"));
        floater_handle.get()->getChild<LLButton>("close_button")->setEnabled(true);
        floater_handle.get()->setCanClose(true);
    }
    floater_handle.get()->countItemLinks();
	
}
