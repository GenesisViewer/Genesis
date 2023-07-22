#ifndef GENX_FLOATERAREASEARCH_H
#define GENX_FLOATERAREASEARCH_H

#include "llfloater.h"
#include "sqlite3.h"
#include "lleventtimer.h"
#include "llviewerregion.h"

class GenxFloaterAreaSearchObject {
public:	
	LLUUID uuid;
	LLUUID owner_id;
	LLUUID group_id;
	std::string name;
	std::string description;
	std::string group;
	S32 price;
	S32 li; 
};
class GenxFloaterAreaSearch : public LLFloater, public LLFloaterSingleton<GenxFloaterAreaSearch>,public LLEventTimer
{
public:	
    GenxFloaterAreaSearch(const LLSD& data);
	virtual ~GenxFloaterAreaSearch();
    /*virtual*/ BOOL postBuild();
	/*virtual*/ void close(bool app = false);
	/*virtual*/ void onOpen();
	/*virtual*/ BOOL tick() override;
	int callback(int argc, char **argv, char **azColName);
	void requestObjectProperties();
	void updateObject(GenxFloaterAreaSearchObject *data);
	static void processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data);
	static void processObjectProperties(LLMessageSystem* msg);
	std::string mSearchByName;
	LLUUID mSearchByOwner;
	LLUUID mSearchByGroup;
	sqlite3	*db;
private:
	
	void initDB();
	void cleanDB();
	LLViewerRegion* mLastRegion;
	
	void onSearchByName(LLUICtrl* caller, const LLSD& value);
	void onSearchByOwner();
	void onSearchByGroup();
	bool filterEdited;
	void refreshList();
	bool requestPropertiesSent;
};
static void areasearch_compute_groupname(sqlite3_context *context, int argc, sqlite3_value **argv);
static void areasearch_compute_ownername(sqlite3_context *context, int argc, sqlite3_value **argv);
static void areasearch_compute_distance(sqlite3_context *context, int argc, sqlite3_value **argv);
static int areasearch_callback(void *param, int argc, char **argv, char **azColName);
static int areasearch_callback_owner_combo(void *param, int argc, char **argv, char **azColName);
static int areasearch_callback_group_combo(void *param, int argc, char **argv, char **azColName);
#endif