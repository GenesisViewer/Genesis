#ifndef LL_LLBULKUPLOADMGR_H
#define LL_LLBULKUPLOADMGR_H
#include "llsdutil.h"
#include "llhttpclient.h"
#include "llassetuploadresponders.h"
class LLBulkUploadMgr {
public:
    typedef std::vector<LLSD> bulk_upload_vec_t;
    typedef std::vector<LLAssetID> bulk_upload_uuid_t;
    typedef std::vector<LLAssetType::EType> bulk_upload_type_t;
    LLBulkUploadMgr();
	~LLBulkUploadMgr();
    void addAssetToBulkUpload(LLAssetID uuid,LLSD asset,LLAssetType::EType asset_type);
    void start();
    void setURL(std::string uploadurl) {this->url=uploadurl;};
    void end();
protected:
    bulk_upload_vec_t to_uploads;
    bulk_upload_uuid_t uuids;
    bulk_upload_type_t types;
    void bulk_upload(LLAssetID uuid,LLSD asset,LLAssetType::EType asset_type);
    std::string url;
};
class LLBulkUploadResponder :public LLHTTPClient::ResponderWithResult{
public :
    LLBulkUploadResponder(LLBulkUploadMgr* bulkUploadMgr,
    LLSD& post_data,
	LLUUID& vfile_id,
	LLAssetType::EType asset_type);

    ~LLBulkUploadResponder();
    void httpSuccess();
    void httpFailure();
    char const* getName() const { return "LLBulkUploadResponder"; }
private:
    LLBulkUploadMgr* bulkUploadMgr; 
    LLNewAgentInventoryResponder* inventoryResponder;  
    LLAssetID file_id;
    LLAssetType::EType asset_type;
};
#endif // LL_LLBULKUPLOADMGR_H