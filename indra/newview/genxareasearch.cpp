#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"
#include "genxareasearch.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "lltracker.h"
#include "llviewerobjectlist.h"
#include "llagent.h"
#include "llscrolllistctrl.h"
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
    const char * sql = "CREATE TABLE OBJECTS (" \
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
    return TRUE;
}
void GenxFloaterAreaSearch::onOpen()
{
    
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
    
 
    const char * sql = "INSERT INTO OBJECTS (UUID) VALUES (?) ON CONFLICT(UUID) DO NOTHING";
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
                
                LL_INFOS() << "Inserting into objects " << object_id << LL_ENDL;
                sqlite3_bind_text(stmt,1,object_id.asString().c_str(), strlen(object_id.asString().c_str()),SQLITE_STATIC) ;
                rc = sqlite3_step(stmt);
                if  (rc) 
                    LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
                sqlite3_reset(stmt);
            }
        }
    }
    sqlite3_finalize(stmt);  
    
    //reading the objects
    sql = "SELECT UUID FROM OBJECTS WHERE STATUS IS NULL";
    
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    while ( sqlite3_step(stmt) == SQLITE_ROW) {
        
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        std::string strUUID = std::string(reinterpret_cast<const char*>(text));
        LLUUID uuid (strUUID);
        LL_INFOS() <<"UUID in OBJECTS " << uuid.asString() << LL_ENDL;
        LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_RequestFlags, 0 );
		msg->addUUIDFast(_PREHASH_ObjectID, uuid);
		gAgent.sendReliableMessage();

    }
    sqlite3_finalize(stmt);  
}
static int areasearch_callback(void *param, int argc, char **argv, char **azColName)
{
	GenxFloaterAreaSearch* floater = GenxFloaterAreaSearch::findInstance();
	if(!floater)
		return 1;
    
    return floater->callback(argc, argv, azColName);
}
void GenxFloaterAreaSearch::processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data)
{

	GenxFloaterAreaSearch* floater = findInstance();
	if(!floater)
		return;
    GenxFloaterAreaSearchObject *data = new GenxFloaterAreaSearchObject();
   
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, data->uuid);

    msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, data->owner_id);
	if (auto obj = gObjectList.findObject(data->uuid)) obj->mOwnerID = data->owner_id; 
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, data->group_id);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, data->name);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, data->description);
    LLViewerObject *objectp = gObjectList.findObject(data->uuid);
    LLViewerRegion *our_region = gAgent.getRegion();
    if (objectp)
    {
        if (objectp->getRegion() == our_region && !objectp->isAvatar() && objectp->isRoot() &&
            !objectp->flagTemporary() && !objectp->flagTemporaryOnRez())
        {
            floater->updateObject(data);
        }
    }
    delete data;
   

    
}        
void GenxFloaterAreaSearch::updateObject(GenxFloaterAreaSearchObject *data) {
    int rc;
    sqlite3_stmt *stmt;
    // const char *sql = "INSERT INTO OBJECTS (UUID, OWNER_ID, GROUP_ID, NAME, DESCRIPTION,STATUS ) VALUES (?,?,?,?,?,1) ON CONFLICT (UUID) DO UPDATE SET " \
    //                 " OWNER_ID=excluded.OWNER_ID, GROUP_ID=excluded.GROUP_ID, NAME=excluded.NAME, DESCRIPTION = excluded.DESCRIPTION,OWNER_NAME=NULL, GROUP_NAME=NULL,DISTANCE=NULL,STATUS=1  ";
    const char *sql = "UPDATE OBJECTS SET OWNER_ID = ? , GROUP_ID = ?, NAME = ?, DESCRIPTION = ?,STATUS = 1 WHERE UUID = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if  (rc) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    
    LL_INFOS() << "Owner for this object " << data->uuid.asString() << " is " << data->owner_id << LL_ENDL;
    
    sqlite3_bind_text(stmt,5,data->uuid.asString().c_str(), strlen(data->uuid.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,1,data->owner_id.asString().c_str(), strlen(data->owner_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,2,data->group_id.asString().c_str(), strlen(data->group_id.asString().c_str()),SQLITE_TRANSIENT ) ;    
    sqlite3_bind_text(stmt,3,data->name.c_str(), strlen(data->name.c_str()),SQLITE_TRANSIENT ) ;  
    sqlite3_bind_text(stmt,4,data->description.c_str(), strlen(data->description.c_str()),SQLITE_TRANSIENT ) ;
    rc = sqlite3_step(stmt);
    if  (rc != SQLITE_OK) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    sqlite3_finalize(stmt);  
    
    sql = "INSERT INTO OBJECTS (UUID, OWNER_ID, GROUP_ID, NAME, DESCRIPTION,STATUS ) VALUES (?,?,?,?,?,1)";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if  (rc) 
        LL_INFOS () << sqlite3_errmsg(db) <<LL_ENDL;
    
    LL_INFOS() << "Owner for this object " << data->uuid.asString() << " is " << data->owner_id << LL_ENDL;
    
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
    LLViewerRegion* our_region = gAgent.getRegion();
    if (our_region != mLastRegion ) {
        LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
        result_list->clearRows();
        cleanDB();
        mLastRegion=our_region;
    }
    initDB();
    char *zErrMsg = 0;
    int rc;
    char *sql;
    sqlite3_stmt *stmt;
    
    LL_INFOS() << "GenxFloaterAreaSearch::tick() 1" << LL_ENDL;
    sql = "UPDATE OBJECTS SET OWNER_NAME=areasearch_compute_ownername(OWNER_ID) WHERE OWNER_NAME IS NULL AND OWNER_ID IS NOT NULL";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg);
    LL_INFOS() << "GenxFloaterAreaSearch::tick() 2" << LL_ENDL;
    sql = "UPDATE OBJECTS SET GROUP_NAME=areasearch_compute_groupname(GROUP_ID) WHERE GROUP_NAME IS NULL AND GROUP_ID IS NOT NULL";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
    
    //compute the distance
    LL_INFOS() << "GenxFloaterAreaSearch::tick() 3" << LL_ENDL;
    sql = "UPDATE OBJECTS SET DISTANCE=areasearch_compute_distance(UUID) WHERE UUID IS NOT NULL";
    rc = sqlite3_exec (db, sql, NULL, NULL, &zErrMsg); 
    LL_INFOS() << "GenxFloaterAreaSearch::tick() 4" << LL_ENDL;
    sql = "SELECT UUID,NAME,DESCRIPTION,OWNER_NAME,GROUP_NAME,PRICE,LI,DISTANCE FROM OBJECTS WHERE UUID IS NOT NULL AND STATUS IS NOT NULL";

   /* Execute SQL statement */
   rc = sqlite3_exec(db, sql, areasearch_callback, NULL, &zErrMsg);
//    sql = "SELECT * FROM OBJECTS WHERE UUID IS NOT NULL AND STATUS IS NOT NULL";

//    /* Execute SQL statement */
//    rc = sqlite3_exec(db, sql, areasearch_callback, NULL, &zErrMsg);
   return FALSE;
}

int GenxFloaterAreaSearch::callback(int argc, char **argv, char **azColName){
    if (argc >=10) {
    int i;
      for(i = 0; i<argc; i++){
        LL_INFOS() << azColName[i] <<  (argv[i] ? argv[i] : "NULL") <<LL_ENDL;
      }
    } else {
   LLScrollListCtrl *result_list =  getChild<LLScrollListCtrl>("result_list");
  
    std::string strUUID = argv[0];
    
    LLUUID uuid (strUUID);
    
   if (result_list->getItemIndex(uuid)>=0) {
        int i;
        LLScrollListItem *row = result_list->getItem(LLSD(uuid));
        if (row) {
            for (i=1; i< argc; i++) {
                LLScrollListColumn *column = result_list->getColumn(azColName[i]);
                if (column) {
                    LLScrollListCell *cell = row->getColumn(column->mIndex);
                    cell->setValue(LLSD((argv[i] ? argv[i] : "")));

                }
            }
        }
        
   } else {
        LL_INFOS() << "callback line not found " << strUUID << LL_ENDL;
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
   //fprintf(stderr, "%s: ", (const char*)data);
   //LLSD row;
		// row["columns"][0]["value"] = notification->getName();
		// row["columns"][0]["column"] = "name";

		// row["columns"][1]["value"] = notification->getMessage();
		// row["columns"][1]["column"] = "content";

		// row["columns"][2]["value"] = notification->getDate();
		// row["columns"][2]["column"] = "date";
		// row["columns"][2]["type"] = "date";
//    for(i = 0; i<argc; i++){
//       LL_INFOS() << azColName[i] <<  (argv[i] ? argv[i] : "NULL") <<LL_ENDL;
//    }
   
   }
   return 0;
}
