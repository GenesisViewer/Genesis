#ifndef GENX_FLOATERAREASEARCH_H
#define GENX_FLOATERAREASEARCH_H

#include "llfloater.h"

#include "lleventtimer.h"
#include "llviewerregion.h"
#include "llviewerparcelmgr.h"

class GenxFloaterAreaSearchParcelObserver;

class GenxFloaterAreaSearchObject {
public:	
	LLUUID uuid;
	S32 local_id;
	LLUUID owner_id;
	std::string owner_name;
	LLUUID group_id;
	std::string group_name;
	std::string name;
	std::string description;
	S32 distance;
	S32 price;
	S32 li; 
	S32 status;
	F64 lastRequestedTime;
};
class GenxFloaterAreaSearch : public LLFloater, public LLFloaterSingleton<GenxFloaterAreaSearch>{
public:	
    GenxFloaterAreaSearch(const LLSD& data);
	virtual ~GenxFloaterAreaSearch();
    /*virtual*/ BOOL postBuild();
	/*virtual*/ void close(bool app = false);
	/*virtual*/ void onOpen();
	
	void requestObjectProperties();
	void reloadList();
	static void processObjectPropertiesFamily(LLMessageSystem* msg, void** user_data);
	static void processObjectProperties(LLMessageSystem* msg);
	static void idle(void *user_data);
	BOOL isParcelOnly() { return mParcelOnly;}
	class LLViewerObject* getSelectedObject();
	void onDoubleClick();
	
private:
	LLViewerRegion* mLastRegion;
	S32 mLastParcel;
	void findNewObjects();
	void onSearchByName(LLUICtrl* caller, const LLSD& value);
	void onSearchByOwner();
	void onSearchByGroup();
	void onSearchByDistance();
	void onSearchByPrice();
	void updateResultList(GenxFloaterAreaSearchObject data);
	S32 computeDistance(GenxFloaterAreaSearchObject obj);
	bool matchFilters(GenxFloaterAreaSearchObject data);
	bool filterEdited;
	std::map<LLUUID, GenxFloaterAreaSearchObject> objects;
	std::string mSearchByName;
	LLUUID mSearchByOwner;
	LLUUID mSearchByGroup;
	BOOL mUseDistance;
	int mMinDistance;
	int mMaxDistance;
	int mMinPrice;
	int mMaxPrice;
	BOOL mIncludePriceless;
	BOOL mParcelOnly;
	LLVector3d lastAgentPos;
	F64 lastUpdatedTime;

	LLTextBox* mCounterText;

	
};

#endif