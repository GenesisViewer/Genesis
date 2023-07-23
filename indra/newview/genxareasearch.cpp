#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "genxareasearch.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"
#include "llagent.h"
#include "llscrolllistctrl.h"
#include "llcombobox.h"
#include "llfiltereditor.h"
#include "llsliderctrl.h"
#include "llcheckboxctrl.h"
#include "llviewerparcelmgr.h"
#include <boost/thread.hpp>
GenxFloaterAreaSearch::GenxFloaterAreaSearch(const LLSD& data) :
	LLFloater(),
    LLEventTimer(F32 (3.0f))
{

	LLUICtrlFactory::getInstance()->buildFloater(this, "genx_area_search.xml");
    
}

GenxFloaterAreaSearch::~GenxFloaterAreaSearch()
{
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
	int rc;    
    char *zErrMsg = 0;
    rc = sqlite3_open(":memory:", &db);
    if  (rc) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    const char * sql ="pragma journal_mode = WAL;";
    sql = "pragma synchronous = normal;";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    sql = "pragma temp_store = memory;";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    sql = "pragma mmap_size = 30000000000;";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    sql = "CREATE TABLE OBJECTS (" \
    "UUID TEXT PRIMARY KEY," \
    "LOCAL_ID INTEGER,"\
    "STATUS INTEGER,"\
    "OWNER_ID TEXT ," \
    "GROUP_ID TEXT," \
    "NAME TEXT ," \
    "DESCRIPTION TEXT ," \
    "OWNER_NAME TEXT  ," \
    "GROUP_NAME TEXT  ," \
    "PRICE REAL," \
    "LI INTEGER,"\
    "DISTANCE REAL,"\
    "IN_PARCEL INTEGER"\
    ")";
    
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    sql = "CREATE TABLE AREASEARCH_VIEW (" \
    "UUID TEXT PRIMARY KEY," \
    "STATUS INTEGER,"\
    "OWNER_ID TEXT ," \
    "GROUP_ID TEXT," \
    "NAME TEXT ," \
    "DESCRIPTION TEXT ," \
    "OWNER_NAME TEXT  ," \
    "GROUP_NAME TEXT  ," \
    "PRICE REAL," \
    "LI INTEGER,"\
    "DISTANCE REAL"\
    ")";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    rc = sqlite3_create_function(db, "areasearch_compute_groupname", 1, SQLITE_UTF8, NULL, &areasearch_compute_groupname, NULL, NULL);
    if (rc != SQLITE_OK) {
        LL_WARNS() << "can't define areasearch_compute_groupname" << LL_ENDL;
        
    }
    rc = sqlite3_create_function(db, "areasearch_compute_ownername", 1, SQLITE_UTF8, NULL, &areasearch_compute_ownername, NULL, NULL);
    if (rc != SQLITE_OK) {
        LL_WARNS() << "can't define areasearch_compute_ownername" << LL_ENDL;
    }
    rc = sqlite3_create_function(db, "areasearch_compute_distance", 1, SQLITE_UTF8, NULL, &areasearch_compute_distance, NULL, NULL);
    if (rc != SQLITE_OK) {
        LL_WARNS() << "can't define areasearch_compute_distance" << LL_ENDL;
    }
    rc = sqlite3_create_function(db, "areasearch_in_parcel", 1, SQLITE_UTF8, NULL, &areasearch_in_parcel, NULL, NULL);
    if (rc != SQLITE_OK) {
        LL_WARNS() << "can't define areasearch_in_parcel" << LL_ENDL;
    }
    //search by name or description
    LLFilterEditor *name_filter = getChild<LLFilterEditor>("areasearch_lineedit");
    name_filter->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByName,this,_1,_2));
    LLComboBox *owner_combo = getChild<LLComboBox>("owner_filter");
    owner_combo->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByOwner,this));
    LLComboBox *group_combo = getChild<LLComboBox>("group_filter");
    group_combo->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByGroup,this));
    LLSliderCtrl *min_distance_slider = getChild<LLSliderCtrl>("min_dist");
    min_distance_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    mMinDistance=0;
    LLSliderCtrl *max_distance_slider = getChild<LLSliderCtrl>("max_dist");
    max_distance_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    mMaxDistance=256;
    LLCheckBoxCtrl *parcel_only_checkbox=getChild<LLCheckBoxCtrl>("parcel_only");
    parcel_only_checkbox->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByDistance,this));
    mParcelOnly=FALSE;
    LLSliderCtrl *min_price_slider = getChild<LLSliderCtrl>("min_price");
    min_price_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mMinPrice=0;
    LLSliderCtrl *max_price_slider = getChild<LLSliderCtrl>("max_price");
    max_price_slider->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mMaxPrice=S32_MAX;
    LLCheckBoxCtrl *include_priceless_checkbox=getChild<LLCheckBoxCtrl>("include_priceless");
    include_priceless_checkbox->setCommitCallback(boost::bind(&GenxFloaterAreaSearch::onSearchByPrice,this));
    mIncludePriceless=TRUE;
    requestPropertiesSent=false;
    tick();
    return TRUE;
}
void GenxFloaterAreaSearch::onOpen()
{
    
}
void GenxFloaterAreaSearch::onSearchByName(LLUICtrl* caller, const LLSD& value)
{
	std::string text = value.asString();
	LLStringUtil::toLower(text);
	caller->setValue(text);
 	
    
    mSearchByName=text;
    filterEdited=true;
    refreshList();
	
}
void GenxFloaterAreaSearch::onSearchByDistance()
{
	mMinDistance=getChild<LLSliderCtrl>("min_dist")->getValue().asInteger();
    mMaxDistance=getChild<LLSliderCtrl>("max_dist")->getValue().asInteger();
    mParcelOnly = getChild<LLCheckBoxCtrl>("parcel_only")->getValue().asBoolean();
    filterEdited=true;
    refreshList();
	
}
void GenxFloaterAreaSearch::onSearchByPrice()
{
	mMinPrice=getChild<LLSliderCtrl>("min_price")->getValue().asInteger();
    mMaxPrice=getChild<LLSliderCtrl>("max_price")->getValue().asInteger();
    mIncludePriceless = getChild<LLCheckBoxCtrl>("include_priceless")->getValue().asBoolean();
    filterEdited=true;
    refreshList();
	
}
void GenxFloaterAreaSearch::onSearchByOwner()
{
	mSearchByOwner = getChild<LLComboBox>("owner_filter")->getValue().asUUID();
	
    filterEdited=true;
    refreshList();
	
}
void GenxFloaterAreaSearch::onSearchByGroup()
{
	mSearchByGroup = getChild<LLComboBox>("group_filter")->getValue().asUUID();
	
    filterEdited=true;
    refreshList();
	
}
void GenxFloaterAreaSearch::cleanDB() {
    char *zErrMsg = 0;
    const char * sql = "DELETE FROM OBJECTS";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
}
void GenxFloaterAreaSearch::initDB() {
    char *zErrMsg = 0;
    int rc;
    sqlite3_stmt *stmt;
    
 
    const char * sql = "INSERT INTO OBJECTS (UUID,LOCAL_ID) VALUES (?,?) ON CONFLICT(UUID) DO NOTHING";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    S32 total = gObjectList.getNumObjects();

	LLViewerRegion* our_region = gAgent.getRegion();
	for (int i = 0; i < total; i++)
	{
		LLViewerObject *objectp = gObjectList.getObject(i);
		if (objectp)
		{
            if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
				!objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
			{
				LLUUID object_id = objectp->getID();
                
                
                sqlite3_bind_text(stmt,1,object_id.asString().c_str(), strlen(object_id.asString().c_str()),SQLITE_STATIC) ;
                sqlite3_bind_int(stmt,2,objectp->mLocalID) ;
                rc = sqlite3_step(stmt);
                
                sqlite3_reset(stmt);
            }
        }
    }
    sqlite3_finalize(stmt);  
    
    
    
    requestObjectProperties();  
    
}
static int areasearch_callback(void *param, int argc, char **argv, char **azColName)
{
	GenxFloaterAreaSearch* floater = GenxFloaterAreaSearch::findInstance();
	if(!floater)
		return 1;
    
    return floater->callback(argc, argv, azColName);
}
static int areasearch_callback_owner_combo(void *param, int argc, char **argv, char **azColName)
{
	GenxFloaterAreaSearch* floater = GenxFloaterAreaSearch::findInstance();
	if(!floater)
		return 1;
    LLComboBox *owner_combo = floater->getChild<LLComboBox>("owner_filter");
    std::string strUUID = argv[0];
        
    LLUUID uuid (strUUID);
   
    owner_combo->addSimpleElement(argv[1],ADD_BOTTOM,uuid);
    return 0;
    
}
static int areasearch_callback_group_combo(void *param, int argc, char **argv, char **azColName)
{
	GenxFloaterAreaSearch* floater = GenxFloaterAreaSearch::findInstance();
	if(!floater)
		return 1;
    LLComboBox *owner_combo = floater->getChild<LLComboBox>("group_filter");
    std::string strUUID = argv[0];
        
    LLUUID uuid (strUUID);
    
    owner_combo->addSimpleElement(argv[1],ADD_BOTTOM,uuid);
    return 0;
    
}
void GenxFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{

	GenxFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
    // GenxFloaterAreaSearchObject *data = new GenxFloaterAreaSearchObject();
   
	// msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, data->uuid);

    // msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, data->owner_id);
	// if (auto obj = gObjectList.findObject(data->uuid)) obj->mOwnerID = data->owner_id; 
	// msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, data->group_id);
	// msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, data->name);
	// msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, data->description);
    
    // LLViewerObject *objectp = gObjectList.findObject(data->uuid);
    // LLViewerRegion *our_region = gAgent.getRegion();
    // if (objectp)
    // {
    //     if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
    //         !objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
    //     {
    //         floater->updateObject(data);
    //     }
    // }
    // delete data;
    LLUUID uuid;
    msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, uuid);
    LL_INFOS() << "In processObjectPropertiesFamily " <<uuid<<LL_ENDL;
    LLViewerObject *objectp = gObjectList.findObject(uuid);
    LLViewerRegion *our_region = gAgent.getRegion();
    if (objectp)
    {
        if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
            !objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
        {
            char *zErrMsg = 0;
            int rc;
            sqlite3_stmt *stmt;
            const char * sql = "INSERT INTO OBJECTS (UUID,LOCAL_ID) VALUES (?,?) ON CONFLICT(UUID) DO NOTHING";
            rc = sqlite3_prepare_v2(floater->db, sql, -1, &stmt, NULL);
            sqlite3_bind_text(stmt,1,uuid.asString().c_str(), strlen(uuid.asString().c_str()),SQLITE_STATIC) ;
            sqlite3_bind_int(stmt,2,objectp->mLocalID) ;
            rc = sqlite3_step(stmt);
            
            sqlite3_reset(stmt);    
            sqlite3_finalize(stmt); 
        }
    }
    

    
}        
void GenxFloaterAreaSearch::updateObject(GenxFloaterAreaSearchObject *data) {
    int rc;
    sqlite3_stmt *stmt;
    
    const char *sql = "UPDATE OBJECTS SET OWNER_ID = ? , GROUP_ID = ?, NAME = ?, DESCRIPTION = ?,PRICE=?,LI=? WHERE UUID = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if  (rc) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    
    
    sqlite3_bind_text(stmt,7,data->uuid.asString().c_str(), strlen(data->uuid.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,1,data->owner_id.asString().c_str(), strlen(data->owner_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,2,data->group_id.asString().c_str(), strlen(data->group_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,3,data->name.c_str(), strlen(data->name.c_str()),SQLITE_TRANSIENT ) ;  
    sqlite3_bind_text(stmt,4,data->description.c_str(), strlen(data->description.c_str()),SQLITE_TRANSIENT ) ;
    sqlite3_bind_int(stmt,5,data->price ) ;
    sqlite3_bind_int(stmt,6,data->li ) ;
    rc = sqlite3_step(stmt);
    // if  (rc != SQLITE_OK) 
    //     LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    sqlite3_finalize(stmt);  
    
    sql = "INSERT INTO OBJECTS (UUID, OWNER_ID, GROUP_ID, NAME, DESCRIPTION ) VALUES (?,?,?,?,?)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    // if  (rc) 
    //     LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    
    
    sqlite3_bind_text(stmt,1,data->uuid.asString().c_str(), strlen(data->uuid.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,2,data->owner_id.asString().c_str(), strlen(data->owner_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,3,data->group_id.asString().c_str(), strlen(data->group_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,4,data->name.c_str(), strlen(data->name.c_str()),SQLITE_TRANSIENT ) ;  
    sqlite3_bind_text(stmt,5,data->description.c_str(), strlen(data->description.c_str()),SQLITE_TRANSIENT ) ;
    rc = sqlite3_step(stmt);
    if  (rc != SQLITE_OK) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    sqlite3_finalize(stmt);  
    
    
}
void GenxFloaterAreaSearch::requestObjectProperties() {
    int rc;
    sqlite3_stmt *stmt;
    char *zErrMsg = 0;
    const char *sql;
    //reading the objects
    sql = "SELECT UUID,LOCAL_ID FROM OBJECTS WHERE STATUS IS NULL ";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    bool new_message = true;
    bool send_message=false;
    int count = 0;
    LLMessageSystem* msg = gMessageSystem;
    while ( sqlite3_step(stmt) == SQLITE_ROW) {
        if (new_message) {
            msg->newMessageFast(_PREHASH_ObjectSelect);
            msg->nextBlockFast(_PREHASH_AgentData);
            msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
            msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
            count++;
            new_message=false; 
        }
            
        send_message=true;
        const unsigned char *uuid = sqlite3_column_text(stmt,0);
        LL_INFOS() << "INSERTING " << uuid << " INSIDE A MESSAGE" << LL_ENDL;
        int local_id = sqlite3_column_int(stmt,1);
       
        msg->nextBlockFast(_PREHASH_ObjectData);
        msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
        count++;
        if (msg->isSendFull(NULL) || count >= 255)
		{
			LL_INFOS() << "Sending full message " << LL_ENDL;
			gAgent.sendReliableMessage(); 
			count = 0;
			new_message = true;
            
		}

    }
    sqlite3_finalize(stmt); 
    if (!new_message)
	{
        LL_INFOS() << "Sending non full message " << LL_ENDL;
        
		gAgent.sendReliableMessage(); 
	}
    sql = "UPDATE OBJECTS SET STATUS = 1 WHERE STATUS IS NULL "  ;
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
}
void GenxFloaterAreaSearch::updateObjectsInfos() {
    char *zErrMsg = 0;
    int rc;
    char *sql;
    sql = "UPDATE OBJECTS SET OWNER_NAME=areasearch_compute_ownername(OWNER_ID) WHERE OWNER_NAME IS NULL AND OWNER_ID IS NOT NULL";
    
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    int nbOwnersUpdated = sqlite3_changes(db);
    sql = "UPDATE OBJECTS SET GROUP_NAME=areasearch_compute_groupname(GROUP_ID) WHERE GROUP_NAME IS NULL AND GROUP_ID IS NOT NULL";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    int nbGroupsUpdated = sqlite3_changes(db);
    sql = "UPDATE OBJECTS SET IN_PARCEL=areasearch_in_parcel(UUID)";
    sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    //prepare the combo filters
    if (nbOwnersUpdated>0) {
        getChild<LLComboBox>("owner_filter")->clearRows();
        getChild<LLComboBox>("owner_filter")->addSimpleElement("All Owners",ADD_BOTTOM,LLUUID());
        sql="SELECT DISTINCT OWNER_ID, OWNER_NAME FROM OBJECTS  WHERE OWNER_ID IS NOT NULL AND OWNER_NAME IS NOT NULL ORDER BY OWNER_NAME";
        rc = sqlite3_exec(db, sql, areasearch_callback_owner_combo, NULL, &zErrMsg);
        getChild<LLComboBox>("owner_filter")->setValue(mSearchByOwner);
    
    }
    if (nbGroupsUpdated>0) {
        getChild<LLComboBox>("group_filter")->clearRows();
        getChild<LLComboBox>("group_filter")->addSimpleElement("All Groups",ADD_BOTTOM,LLUUID());
        sql="SELECT DISTINCT GROUP_ID, GROUP_NAME FROM OBJECTS  WHERE GROUP_ID IS NOT NULL AND GROUP_NAME IS NOT NULL ORDER BY GROUP_NAME";
        rc = sqlite3_exec(db, sql, areasearch_callback_group_combo, NULL, &zErrMsg);
        getChild<LLComboBox>("group_filter")->setValue(mSearchByGroup);
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
        GenxFloaterAreaSearchObject *data = new GenxFloaterAreaSearchObject();
        data->uuid=object_id;
        msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, data->owner_id, i);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, data->group_id, i);
        msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, data->name, i);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, data->description, i);
        U8 sale_type;
	    msg->getU8Fast(_PREHASH_ObjectData, _PREHASH_SaleType, sale_type, i);
        msg->getS32Fast(_PREHASH_ObjectData, _PREHASH_SalePrice, data->price, i);
        if (sale_type==0) data->price=-1; // not for sale
        data->li=objectp->getLinksetCost();
        if (data->li<1) data->li=1;
        floater->updateObject(data);
        delete data;

        //update the max_price slider max value if ojbect price is superior
        int actualMaxValue = floater->getChild<LLSliderCtrl>("max_price")->getMaxValue(); 
        if (data->price> actualMaxValue) floater->getChild<LLSliderCtrl>("max_price")->setMaxValue(data->price);
    }
    floater->updateObjectsInfos();
    // floater->requestPropertiesSent=false;
    // floater->requestObjectProperties();
}
static void areasearch_compute_groupname(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = sqlite3_value_text(argv[0]);
    std::string strUUID = std::string(reinterpret_cast<const char*>(text));
    LLUUID uuid (strUUID);
    std::string groupName;
    BOOL found = gCacheName->getGroupName(uuid, groupName);
    if (found && !groupName.empty()) {
       sqlite3_result_text(context, groupName.c_str(), -1, SQLITE_TRANSIENT);
    } else {
       sqlite3_result_null(context);
    }

}
static void areasearch_in_parcel(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = sqlite3_value_text(argv[0]);
    std::string strUUID = std::string(reinterpret_cast<const char*>(text));
    LLUUID uuid (strUUID);
    LLViewerObject *objectp = gObjectList.findObject(uuid);
    BOOL in_parcel = objectp && LLViewerParcelMgr::instance().inAgentParcel(objectp->getPositionGlobal());
    if (in_parcel) {
       sqlite3_result_int(context, 1);
    } else {
       sqlite3_result_int(context, 0);
    }

}
static void areasearch_compute_ownername(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = sqlite3_value_text(argv[0]);

    std::string strUUID = std::string(reinterpret_cast<const char*>(text));
    LLUUID uuid (strUUID);
    std::string ownerName;
    BOOL found =  gCacheName->getFullName(uuid, ownerName);
    if (found && !ownerName.empty()) {
       sqlite3_result_text(context, ownerName.c_str(), -1, SQLITE_TRANSIENT);
      
    } else {
       sqlite3_result_null(context);
      
    }
    
}
static void areasearch_compute_distance(sqlite3_context *context, int argc, sqlite3_value **argv) {
    const unsigned char *text = sqlite3_value_text(argv[0]);
    std::string strUUID = std::string(reinterpret_cast<const char*>(text));
    LLUUID uuid (strUUID);
    LLViewerObject *objectp = gObjectList.findObject(uuid);
    if (objectp)
    {
        if (objectp->isHUDAttachment()) 
            sqlite3_result_int(context,0);
        else {
            S32 distance = (S32 )dist_vec(objectp->getPositionGlobal(),gAgent.getPositionGlobal()); 
            sqlite3_result_int(context,distance);
        }
    }else {
        sqlite3_result_int(context,-1);
    }

}
/*virtual*/
BOOL GenxFloaterAreaSearch::tick()
{
    // if the floater has been closed, no need to refresh datas
    GenxFloaterAreaSearch* floater = findInstance();
	if(!floater || !floater->instanceVisible())
		return TRUE;
    LLViewerRegion* our_region = gAgent.getRegion();
    if (our_region != mLastRegion) {
        LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
        result_list->clearRows();
        cleanDB();
        mLastRegion=our_region;
    }
    
    initDB();
    
    
    
    // sql = "UPDATE OBJECTS SET GROUP_NAME=areasearch_compute_groupname(GROUP_ID) WHERE GROUP_NAME IS NULL AND GROUP_ID IS NOT NULL";
    // rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
    
    //compute the distance
    
    char *sql = "UPDATE OBJECTS SET DISTANCE=areasearch_compute_distance(UUID) WHERE UUID IS NOT NULL";
    char *zErrMsg = 0;
    sqlite3_exec (floater->db, sql, NULL, NULL, &zErrMsg);
    
    
     refreshList();

    
   return FALSE;
}
void GenxFloaterAreaSearch::refreshList() {
    char *zErrMsg = 0;
    int rc;
    char *sql;
    sqlite3_stmt *stmt;
    
    if (filterEdited) {
        LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
        result_list->clearRows(); 
    }

    
    sql ="DELETE FROM AREASEARCH_VIEW";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
    sql = "INSERT INTO AREASEARCH_VIEW(UUID,NAME,DESCRIPTION,OWNER_NAME,GROUP_NAME,PRICE,LI,DISTANCE) " \
    "SELECT UUID,NAME,DESCRIPTION,OWNER_NAME,GROUP_NAME,PRICE,LI,DISTANCE FROM OBJECTS WHERE UUID IS NOT NULL AND STATUS IS NOT NULL AND DISTANCE BETWEEN ? AND ? AND (PRICE BETWEEN ? AND ? or PRICE= ?) AND IN_PARCEL >=?";
    
    int indexFilter=1;

    if (!mSearchByName.empty()) {
        
        const char* newsql = (std::string(sql) + " AND (instr(lower(NAME),?)>0 OR instr(lower(DESCRIPTION),?)>0) ").c_str();
        sql = (char*)newsql;

    }
    if(!mSearchByOwner.isNull()) {
         const char* newsql = (std::string(sql) + " AND OWNER_ID=? ").c_str();
         sql = (char*)newsql;
    }
    if(!mSearchByGroup.isNull()) {
         const char* newsql = (std::string(sql) + " AND GROUP_ID=? ").c_str();
         sql = (char*)newsql;
    }
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_int(stmt,indexFilter,mMinDistance);
    indexFilter++;
    sqlite3_bind_int(stmt,indexFilter,mMaxDistance);
    indexFilter++;
    sqlite3_bind_int(stmt,indexFilter,mMinPrice);
    indexFilter++;
    sqlite3_bind_int(stmt,indexFilter,mMaxPrice);
    indexFilter++;
    sqlite3_bind_int(stmt,indexFilter,mIncludePriceless?-1:-2);
    indexFilter++;
    sqlite3_bind_int(stmt,indexFilter,mParcelOnly?1:0);
    indexFilter++;
    if (!mSearchByName.empty()) {
        sqlite3_bind_text(stmt,indexFilter,mSearchByName.c_str(), strlen(mSearchByName.c_str()),SQLITE_TRANSIENT ) ;    
        sqlite3_bind_text(stmt,indexFilter+1,mSearchByName.c_str(), strlen(mSearchByName.c_str()),SQLITE_TRANSIENT ) ; 
        indexFilter+=2;
    }
    if(!mSearchByOwner.isNull()) {
         sqlite3_bind_text(stmt,indexFilter,mSearchByOwner.asString().c_str(), strlen(mSearchByOwner.asString().c_str()),SQLITE_TRANSIENT ) ; 
         indexFilter++;
    }
    if(!mSearchByGroup.isNull()) {
         sqlite3_bind_text(stmt,indexFilter,mSearchByGroup.asString().c_str(), strlen(mSearchByGroup.asString().c_str()),SQLITE_TRANSIENT ) ; 
         indexFilter++;
    }
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    sql = "SELECT UUID,NAME,DESCRIPTION,OWNER_NAME,GROUP_NAME,IIF(PRICE<0,'',PRICE) AS PRICE,LI,DISTANCE FROM AREASEARCH_VIEW WHERE DISTANCE>=0";
    /* Execute SQL statement */
    rc = sqlite3_exec(db, sql, areasearch_callback, NULL, &zErrMsg);
    filterEdited=false;
}
int GenxFloaterAreaSearch::callback(int argc, char **argv, char **azColName){
    
    LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
    
    std::string strUUID = argv[0];
        
    LLUUID uuid (strUUID);

        
    if (result_list->getItemIndex(uuid)>=0) {
        //the line exists, we only update the distance
            int i = argc-1;
            LLScrollListItem *row = result_list->getItem(LLSD(uuid));
            if (row) {
                
                LLScrollListColumn *column = result_list->getColumn(azColName[i]);
                if (column) {
                    LLScrollListCell *cell = row->getColumn(column->mIndex);
                    LLSD newValue = LLSD((argv[i] ? argv[i] : ""));
                    cell->setValue(newValue);

                }
                
            }
            
    } else {
            
            //line not found, we have to create it
            LLScrollListCell::Params cell_params;
            LLScrollListItem::Params row_params;
            row_params.value = strUUID;
            //read the colums, the sql query columns must match the scolllist colums names
            int i;
            for (i=1; i< argc; i++) {
                cell_params.column = azColName[i];
                cell_params.value = (argv[i] ? argv[i] : "");
                
                row_params.columns.add(cell_params);
            }

            result_list->addRow(row_params);
    }
        
   
   
   return 0;
}
