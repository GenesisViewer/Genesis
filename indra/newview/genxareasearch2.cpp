#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "genxareasearch2.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"
#include "llagent.h"
#include "llscrolllistctrl.h"
#include "llcombobox.h"
#include "llfiltereditor.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "llviewerparcelmgr.h"
#include <boost/thread.hpp>
#include "llcallbacklist.h"
#include "llparcel.h"
#include "llframetimer.h"

GenxFloaterAreaSearch::GenxFloaterAreaSearch(const LLSD& data) :
	LLFloater()
{

	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_area_search.xml");
    
	
   
	
}

GenxFloaterAreaSearch::~GenxFloaterAreaSearch()
{
    gIdleCallbacks.deleteFunction(idle, this);
   

}
void GenxFloaterAreaSearch::close(bool app)
{
    if (app )
	{
		LLFloater::close(app);
	}
	else
	{
		setVisible(FALSE);
	}
}
BOOL GenxFloaterAreaSearch::postBuild()
{
	
    //search by name or description
    LLFilterEditor *name_filter = getChild<LLFilterEditor>("areasearch_lineedit");
    name_filter->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByName,this,_1,_2));
    
    
    
   
    LLComboBox *owner_combo = getChild<LLComboBox>("owner_filter");
    owner_combo->addSimpleElement("All Owners",ADD_TOP,LLUUID());
    owner_combo->setValue(LLUUID());
    owner_combo->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByOwner,this));
    LLComboBox *group_combo = getChild<LLComboBox>("group_filter");
    group_combo->addSimpleElement("All Groups",ADD_TOP,LLUUID());
    group_combo->setValue(LLUUID());
    group_combo->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByGroup,this));
    LLSpinCtrl *min_distance_slider = getChild<LLSpinCtrl>("min_dist");
    min_distance_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    mMinDistance=0;
    LLSpinCtrl *max_distance_slider = getChild<LLSpinCtrl>("max_dist");
    
    max_distance_slider->setValue(999999999);
    mMaxDistance=999999999;
    max_distance_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    LLCheckBoxCtrl *parcel_only_checkbox=getChild<LLCheckBoxCtrl>("parcel_only");
    parcel_only_checkbox->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    mParcelOnly=FALSE;
    LLSpinCtrl *min_price_slider = getChild<LLSpinCtrl>("min_price");
    min_price_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mMinPrice=0;
    LLSpinCtrl *max_price_slider = getChild<LLSpinCtrl>("max_price");
    max_price_slider->setValue(999999999);
    max_price_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mMaxPrice=999999999;
    LLCheckBoxCtrl *include_price_checkbox=getChild<LLCheckBoxCtrl>("use_price_filter");
    include_price_checkbox->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mIncludePriceless=TRUE;
    
    
	gIdleCallbacks.addFunction(idle, this);

    mCounterText = getChild<LLTextBox>("counter");

    
    
    return TRUE;
}
void GenxFloaterAreaSearch::findNewObjects() {
    S32 total = gObjectList.getNumObjects();

	LLViewerRegion* our_region = gAgent.getRegion();
	for (int i = 0; i < total; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
            if (objectp->getID().isNull())
            {
                
                continue;
            }
            if (!objectp->getRegion() && objectp->getRegion() != our_region)
            {
                continue;
            }

            
            if (objectp->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
            {
                continue;
            }

            
            if (!objectp->mbCanSelect)
            {
                continue;
            }

           
            if (objectp->isAvatar())
            {
                continue;
            }
            if (!objectp->isRoot()){
                continue;
            }
            if (objectp->flagTemporary() || objectp->flagTemporaryOnRez()){
                continue;
            }
            
				
            GenxFloaterAreaSearchObject obj;
            obj.uuid=objectp->getID();
            obj.local_id=objectp->getLocalID();
            
            obj.status=3;
            if (objects.count(objectp->getID()) == 0)
                objects[objectp->getID()] = obj;
                
            
        }
    }
}
void GenxFloaterAreaSearch::onOpen()
{
    objects.clear();
    LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
    result_list->clearRows();
    LLViewerRegion* our_region = gAgent.getRegion();
    findNewObjects();
    mLastRegion=our_region;
    mLastParcel = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
    
    filterEdited=false;
    //mCounterText->setText(LLStringExplicit("Loading..."));
    requestObjectProperties();
}
S32 GenxFloaterAreaSearch::computeDistance(GenxFloaterAreaSearchObject obj) {
    
    LLViewerObject *objectp = gObjectList.findObject(obj.uuid);
    if (objectp)
    {
        if (objectp->isHUDAttachment()) 
            return 0;
        else {
            S32 distance = (S32 )dist_vec(objectp->getPositionGlobal(),gAgent.getPositionGlobal()); 
            return distance;
        }
    }else {
        return -1;
    }
    
}
//static
void GenxFloaterAreaSearch::idle(void* user_data)
{
    bool agent_moved;
	GenxFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
    if (LLFrameTimer::getTotalSeconds()-floater->lastUpdatedTime<3 && !floater->filterEdited) return;  //don't update too much  
    floater->lastUpdatedTime=LLFrameTimer::getTotalSeconds();
    
    floater->findNewObjects();

	floater->requestObjectProperties();
    LLScrollListCtrl *result_list =  floater->getChild<LLScrollListCtrl>("result_list");
    LLScrollListColumn* distance_column = result_list->getColumn("DISTANCE");
    LLScrollListColumn* li_column = result_list->getColumn("LI");
    LLScrollListColumn* group_column = result_list->getColumn("GROUP_NAME");
    LLScrollListColumn* owner_column = result_list->getColumn("OWNER_NAME");
    LLComboBox *owner_combo = floater->getChild<LLComboBox>("owner_filter");
    LLComboBox *group_combo = floater->getChild<LLComboBox>("group_filter");
    S32 countPendingObjects=0;
    S32 countObjects=0;
    if (floater->mLastRegion != gAgent.getRegion()) {
        floater->mLastRegion=gAgent.getRegion();
        floater->objects.clear();
        result_list->clearRows();
        group_combo->clearRows();
        owner_combo->clearRows();
        floater->onOpen();
        return;
    }
    
    if (floater->mLastParcel!=LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID()) {
        floater->mLastParcel = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
        floater->reloadList();
    }
    if (floater->lastAgentPos!=gAgent.getPositionGlobal()) {
        floater->lastAgentPos=gAgent.getPositionGlobal();
        agent_moved=true;
        for (auto& obj : floater->objects)
	    {
            S32 distance = floater->computeDistance(obj.second);
            obj.second.distance=distance;
                            
        }
    }
    //TODO: find a way to avoid to do it too often
    group_combo->clearRows();
    owner_combo->clearRows();
    
    std::vector<LLUUID> uniqueGroupIds;
    std::vector<LLUUID> uniqueOwnerIds;
    for (auto& obj : floater->objects)
    {
        
        if (!obj.second.group_name.empty() && std::find(uniqueGroupIds.begin(), uniqueGroupIds.end(), obj.second.group_id)==uniqueGroupIds.end()){
            group_combo->addSimpleElement(obj.second.group_name,ADD_BOTTOM,obj.second.group_id);
            uniqueGroupIds.push_back(obj.second.group_id);
        }
        if (!obj.second.owner_name.empty() && std::find(uniqueOwnerIds.begin(), uniqueOwnerIds.end(), obj.second.owner_id)==uniqueOwnerIds.end()){
            owner_combo->addSimpleElement(obj.second.owner_name,ADD_BOTTOM,obj.second.owner_id);
            uniqueOwnerIds.push_back(obj.second.owner_id);
        }

        if (obj.second.status>0)
            countPendingObjects+=1;
        if (obj.second.status==-1)
            countObjects+=1;
                        
    }
    group_combo->sortByName();
    owner_combo->sortByName();
    group_combo->addSimpleElement("All Groups",ADD_TOP,LLUUID());
    owner_combo->addSimpleElement("All Owners",ADD_TOP,LLUUID());
   
    uniqueGroupIds.clear();
    uniqueOwnerIds.clear();

        
    //update the distance,costs or delete items
    std::vector<LLScrollListItem*> items = result_list->getAllData(); 
    for (const auto item : items)
	{
		const LLUUID& row_id = item->getUUID();
		LLViewerObject* objectp = gObjectList.findObject(row_id);
        if (!objectp )
		{
            floater->objects.erase(row_id);
			result_list->deleteSingleItem(result_list->getItemIndex(row_id));
		}
		else
		{
			if (agent_moved)
			{
				item->getColumn(distance_column->mIndex)->setValue(floater->objects[row_id].distance);
			}
		}
        if (objectp) {
            S32 object_cost = (objectp->getLinksetCost()<1?1:objectp->getLinksetCost());
            if (item->getColumn(li_column->mIndex)->getValue().asInteger() != object_cost) {
                item->getColumn(li_column->mIndex)->setValue(object_cost);
            }
        }
        
    }
    if (floater->filterEdited) {
        floater->reloadList();
        floater->filterEdited=false;
    }
    floater->mCounterText->setText(llformat("%d listed/%d pending", result_list->getItemCount(), countPendingObjects));
}
void GenxFloaterAreaSearch::reloadList() {
    LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
    result_list->clearRows();
    for (auto& obj : objects)
    {
        bool match=matchFilters(obj.second);
        if (match)
            updateResultList(obj.second);
                        
    }

}
void GenxFloaterAreaSearch::onSearchByName(LLUICtrl* caller, const LLSD& value)
{
	std::string text = value.asString();
	LLStringUtil::toLower(text);
	caller->setValue(text);
 	
    
    mSearchByName=text;
    filterEdited=true;
   
	
}
void GenxFloaterAreaSearch::onSearchByDistance()
{
    mUseDistance = getChild<LLCheckBoxCtrl>("use_distance_filter")->getValue().asBoolean();
	mMinDistance=getChild<LLSpinCtrl>("min_dist")->getValue().asInteger();
    mMaxDistance=getChild<LLSpinCtrl>("max_dist")->getValue().asInteger();
    mParcelOnly = getChild<LLCheckBoxCtrl>("parcel_only")->getValue().asBoolean();
    filterEdited=true;
    
	
}
void GenxFloaterAreaSearch::onSearchByPrice()
{
	mMinPrice=getChild<LLSpinCtrl>("min_price")->getValue().asInteger();
    mMaxPrice=getChild<LLSpinCtrl>("max_price")->getValue().asInteger();
    mIncludePriceless = !getChild<LLCheckBoxCtrl>("use_price_filter")->getValue().asBoolean();
    filterEdited=true;
    
	
}
void GenxFloaterAreaSearch::onSearchByOwner()
{
	mSearchByOwner = getChild<LLComboBox>("owner_filter")->getValue().asUUID();
	
    filterEdited=true;
    
	
}
void GenxFloaterAreaSearch::onSearchByGroup()
{
	mSearchByGroup = getChild<LLComboBox>("group_filter")->getValue().asUUID();
	
    filterEdited=true;
    
	
}

void GenxFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{

	GenxFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
    
    LLUUID uuid;
    msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, uuid);
    
    LLViewerObject *objectp = gObjectList.findObject(uuid);
    LLViewerRegion *our_region = gAgent.getRegion();
    if (objectp)
    {
        if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
            !objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
        {
            GenxFloaterAreaSearchObject obj;
            obj.uuid=objectp->getID();
            obj.local_id=objectp->getLocalID();
            obj.status=3;
            floater->objects[objectp->getID()] = obj;
        }
    }
    

    
}        

void GenxFloaterAreaSearch::requestObjectProperties() {
    bool new_message = true;
    bool send_message=false;
    int count = 0;
    LLMessageSystem* msg = gMessageSystem;
    for (auto& obj : objects)
	{
		if (obj.second.status>0 && (LLFrameTimer::getTotalSeconds()-obj.second.lastRequestedTime>3 ))
		{
            
			if (new_message) {
                msg->newMessageFast(_PREHASH_ObjectSelect);
                msg->nextBlockFast(_PREHASH_AgentData);
                msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
                msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
                count++;
                new_message=false; 
            }
                
            send_message=true;
            msg->nextBlockFast(_PREHASH_ObjectData);
            msg->addU32Fast(_PREHASH_ObjectLocalID, obj.second.local_id);
            count++;
            if (msg->isSendFull(NULL) || count >= 255)
            {
                
                gAgent.sendReliableMessage(); 
                count = 0;
                new_message = true;
                
            }
           
            obj.second.status-=1;
            obj.second.lastRequestedTime=LLFrameTimer::getTotalSeconds();
		}
	}
    if (!new_message)
    {
        
        gAgent.sendReliableMessage(); 
    }
}

void GenxFloaterAreaSearch::processObjectProperties(LLMessageSystem* msg)
{
    
    GenxFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
    S32 count = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
    for (S32 i = 0; i < count; i++)
	{
		LLUUID object_id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id, i);
        
		if (object_id.isNull())
		{
			LL_WARNS() << "Got Object Properties with NULL id" << LL_ENDL;
			continue;
		}

		LLViewerObject* objectp = gObjectList.findObject(object_id);
		if (!objectp)
		{
			continue;
		}
        if (floater->objects.count(object_id)>0 && floater->objects[object_id].status==-1) 
            continue;
        GenxFloaterAreaSearchObject data = GenxFloaterAreaSearchObject();
        data.uuid=object_id;
        msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, data.owner_id, i);
        //get the ownername
        if (data.owner_id.isNull())
            continue;
            
        
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, data.group_id, i);
        //get the group name
        if (data.group_id.isNull())
            continue;
        gCacheName->getFullName(data.owner_id,data.owner_name);
        gCacheName->getGroupName(data.group_id, data.group_name);
        
        msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, data.name, i);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, data.description, i);
        U8 sale_type;
	    msg->getU8Fast(_PREHASH_ObjectData, _PREHASH_SaleType, sale_type, i);
        msg->getS32Fast(_PREHASH_ObjectData, _PREHASH_SalePrice, data.price, i);
        if (sale_type==0) data.price=-1; // not for sale
        data.li=objectp->getLinksetCost();
        if (data.li<1) data.li=1;
        S32 distance = floater->computeDistance(data);
        data.distance=distance;
        data.status=-1;
        floater->objects[object_id] = data;

        
        if (floater->matchFilters(data))
            floater->updateResultList(data);
    }
    
}
bool GenxFloaterAreaSearch::matchFilters(GenxFloaterAreaSearchObject data) {
    LLViewerObject* objectp = gObjectList.findObject(data.uuid);
    if (!objectp)
    {
        return false;
    }
    if (data.status>-1) {
        return false;
    }
    bool match=true;
    if (!mSearchByOwner.isNull() && data.owner_id.asString() != mSearchByOwner.asString()) {
        match=false;
    }
    if (!mSearchByGroup.isNull() && data.group_id.asString() != mSearchByGroup.asString()) {
        match=false;
    }
    
    if ((!mIncludePriceless) && (mMinPrice>data.price || mMaxPrice<data.price) && data.price>=0){
        match = false;
    }
    if ((mUseDistance) && (mMinDistance>data.distance || mMaxDistance<data.distance)){
        match=false;
    }
    if (!mSearchByName.empty()) {
        
        std::string lower_name = data.name;
        LLStringUtil::toLower(lower_name);
        std::string lower_desc = data.description;
        LLStringUtil::toLower(lower_desc);

        if (lower_name.find(mSearchByName) == std::string::npos && lower_desc.find(mSearchByName) == std::string::npos){
            match=false;
        }

    }
    BOOL in_parcel = objectp && LLViewerParcelMgr::instance().inAgentParcel(objectp->getPositionGlobal());
    if (mParcelOnly && !in_parcel) {
        match=false;
    }
    return match;
}
void GenxFloaterAreaSearch::updateResultList(GenxFloaterAreaSearchObject data) {
    if (data.status != -1) return;
    LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
    
            
    
    LLScrollListCell::Params cell_params;
    LLScrollListItem::Params row_params;
    row_params.value = LLSD(data.uuid);
        //dist
    cell_params.column = "DISTANCE";
    cell_params.value=data.distance;
    row_params.columns.add(cell_params);
    //name
    cell_params.column = "NAME";
    cell_params.value=data.name;
    row_params.columns.add(cell_params);
    //description
    cell_params.column = "DESCRIPTION";
    cell_params.value=data.description;
    row_params.columns.add(cell_params);
    //owner
    cell_params.column = "OWNER_NAME";
    if (data.owner_name.empty())  {
        BOOL owner_found =  gCacheName->getFullName(data.owner_id, data.owner_name);
        if (!owner_found)
            data.owner_name="";
    }
        
    cell_params.value=data.owner_name;
    row_params.columns.add(cell_params);
    
    //group name
    cell_params.column = "GROUP_NAME";
    if (data.group_name.empty()) {
        BOOL group_found=gCacheName->getGroupName(data.group_id, data.group_name);
        if (!group_found)
             data.group_name = "";  
    }
        
    cell_params.value=data.group_name;
    row_params.columns.add(cell_params);
    

    //price
    if (data.price>=0) {
        cell_params.column = "PRICE";
        cell_params.value=data.price;
        row_params.columns.add(cell_params);
    }
    //price
    cell_params.column = "LI";
    cell_params.value=data.li;
    row_params.columns.add(cell_params);
    LLScrollListItem *row= result_list->addRow(row_params);
    
    
}


