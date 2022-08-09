#include "llbulkuploadmgr.h"
#include "llhttpclient.h"
#include "llvfs.h"
LLBulkUploadResponder::LLBulkUploadResponder (LLBulkUploadMgr* bulkMgr,LLSD& post_data,
	LLUUID& vfile_id,
	LLAssetType::EType asset_type) {
    this->bulkUploadMgr=bulkMgr;
    this->file_id=vfile_id;
    this->asset_type=asset_type;
    this->inventoryResponder = new LLNewAgentInventoryResponder(post_data,vfile_id,asset_type);
}
LLBulkUploadResponder::~LLBulkUploadResponder () {
    
}
LLBulkUploadMgr::~LLBulkUploadMgr() {

}
LLBulkUploadMgr::LLBulkUploadMgr() {

}
void LLBulkUploadResponder::httpSuccess() {
    const std::string& state = mContent["state"].asString();
    if (state == "upload")
	{
        const std::string& uploader = mContent["uploader"].asString();
        LLHTTPClient::postFile(uploader, file_id, asset_type, this);
    }
    else if (state == "complete")
	{
        LL_INFOS() << "Bulk Upload ok with result " << mContent << LL_ENDL;
        gVFS->renameFile(file_id, asset_type, mContent["new_asset"].asUUID(), asset_type);
        this->inventoryResponder->uploadComplete(mContent);
        this->bulkUploadMgr->start();
    }
    
    
    
    
}
void LLBulkUploadResponder::httpFailure() {
    LL_INFOS() << "Bulk Upload KO with result " << mContent << LL_ENDL;
    this->inventoryResponder->uploadFailure(mContent);
    
}
void LLBulkUploadMgr::addAssetToBulkUpload(LLAssetID uuid,LLSD asset,LLAssetType::EType asset_type) {
    to_uploads.insert(to_uploads.begin(),asset);
    uuids.insert(uuids.begin(),uuid );
    types.insert(types.begin(),asset_type);
}

void LLBulkUploadMgr::start() {
    if (to_uploads.size()>0) {
        LLSD asset_to_upload=to_uploads.front();
        to_uploads.erase(to_uploads.begin());
        LLAssetID uuid = uuids.front();
        uuids.erase(uuids.begin());
        LLAssetType::EType type = types.front();
        types.erase(types.begin());
        bulk_upload(uuid,asset_to_upload,type);
        
    } else {
        end();
    }
    
}
void LLBulkUploadMgr::end() {
    LL_INFOS() << "bulk upload finished" << LL_ENDL;
    //update inventory
    //notify the price
}
void LLBulkUploadMgr::bulk_upload(LLAssetID uuid,LLSD asset,LLAssetType::EType asset_type) {
    LL_INFOS() << "bulk upload asset " << asset["name"] << LL_ENDL;
    LLHTTPClient::post(
            url,
            asset,
            new LLBulkUploadResponder(this,asset,uuid,asset_type));
    
    
}