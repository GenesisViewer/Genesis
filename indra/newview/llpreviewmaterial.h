/*@file llpreviewnotecard.h
 * @brief LLPreviewNotecard class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLPREVIEWMATERIAL_H
#define LL_LLPREVIEWMATERIAL_H
#include "llpreview.h"
#include "llassetstorage.h"
#include "lltinygltfhelper.h"
#include "llvoinventorylistener.h"
#include "llviewertexture.h"
#include "lltexturectrl.h"
#include "llimagej2c.h"
class LLButton;
class LLColorSwatchCtrl;
class LLComboBox;
class LLGLTFMaterial;
class LLLocalGLTFMaterial;
class LLTextureCtrl;
class LLTextBox;
class LLViewerInventoryItem;

class LLButton;
class LLColorSwatchCtrl;
class LLComboBox;
class LLGLTFMaterial;
class LLLocalGLTFMaterial;
class LLTextureCtrl;
class LLTextBox;
class LLViewerInventoryItem;

namespace tinygltf
{
    class Model;
}

class LLPreviewMaterial : public LLPreview, public LLVOInventoryListener
{
public:
    LLPreviewMaterial(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id, 
									 const LLUUID& object_id,
									 const LLUUID& asset_id,
									 LLPointer<LLViewerInventoryItem> inv_item);
    virtual ~LLPreviewMaterial();
    // llpreview	
	virtual bool saveItem(LLPointer<LLInventoryItem>* itemptr);
    // llfloater
	virtual BOOL canClose();
    // llpanel
	virtual BOOL	postBuild();
    BOOL canModify(const LLUUID taskUUID, const LLInventoryItem* item);
    bool setFromGltfModel(const tinygltf::Model& model, S32 index, bool set_textures = false);
    // initialize the UI from a default GLTF material
    void loadDefaults();
    // get a dump of the json representation of the current state of the editor UI as a material object
    void getGLTFMaterial(LLGLTFMaterial* mat);
    void loadAsset() override;
    // for live preview, apply current material to currently selected object
    void applyToSelection();

    LLUUID getBaseColorId();
    void setBaseColorId(const LLUUID& id);
    void setBaseColorUploadId(const LLUUID& id);

    LLColor4 getBaseColor();

    // sets both base color and transparency
    void    setBaseColor(const LLColor4& color);

    F32     getTransparency();
    void     setTransparency(F32 transparency);

    std::string getAlphaMode();
    void setAlphaMode(const std::string& alpha_mode);

    F32 getAlphaCutoff();
    void setAlphaCutoff(F32 alpha_cutoff);
    
    void setMaterialName(const std::string &name);

    LLUUID getMetallicRoughnessId();
    void setMetallicRoughnessId(const LLUUID& id);
    void setMetallicRoughnessUploadId(const LLUUID& id);

    F32 getMetalnessFactor();
    void setMetalnessFactor(F32 factor);

    F32 getRoughnessFactor();
    void setRoughnessFactor(F32 factor);

    LLUUID getEmissiveId();
    void setEmissiveId(const LLUUID& id);
    void setEmissiveUploadId(const LLUUID& id);

    LLColor4 getEmissiveColor();
    void setEmissiveColor(const LLColor4& color);

    LLUUID getNormalId();
    void setNormalId(const LLUUID& id);
    void setNormalUploadId(const LLUUID& id);

    bool getDoubleSided();
    void setDoubleSided(bool double_sided);

    void setCanSaveAs(bool value);
    void setCanSave(bool value);

    static void onLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status);
    void inventoryChanged(LLViewerObject* object, LLInventoryObject::object_list_t* inventory, S32 serial_num, void* user_data) override;

    bool decodeAsset(const std::vector<char>& buffer);
    std::string getEncodedAsset();

    virtual const char *getTitleName() const { return "Material"; }
    void onCommitTexture(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag);
    void onCancelCtrl(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag);
    void onSelectCtrl(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag);

    void onClickSave();
    bool saveIfNeeded();
    S32 saveTextures();
    static bool capabilitiesAvailable();

    typedef std::function<void(LLUUID newAssetId, LLSD response)> upload_callback_f;
    static void saveTextureDone(LLUUID const& asset_id, void* user_data, S32 status,  LLExtStat ext_status, upload_callback_f cb);
    void saveTexture(LLImageJ2C* img, const std::string& name, const LLUUID& asset_id, LLAssetStorage::LLStoreAssetCallback cb);
    void setFailedToUploadTexture();
    void clearTextures();
protected:
    static LLPreviewMaterial* getInstance(const LLUUID& uuid);
    void setEnableEditing(bool can_modify);
private:
    bool updateInventoryItem(const std::string &buffer, const LLUUID &item_id, const LLUUID &task_id);
    void createInventoryItem(const std::string &buffer, const std::string &name, const std::string &desc, const LLPermissions& permissions);

    // utility function for building a description of the imported material
    // based on what we know about it.
    const std::string buildMaterialDescription();

    // last known name of each texture
    std::string mBaseColorName;
    std::string mNormalName;
    std::string mMetallicRoughnessName;
    std::string mEmissiveName;

    // keep pointers to fetched textures or viewer will remove them
    // if user temporary selects something else with 'apply now'
    LLPointer<LLViewerFetchedTexture> mBaseColorFetched;
    LLPointer<LLViewerFetchedTexture> mNormalFetched;
    LLPointer<LLViewerFetchedTexture> mMetallicRoughnessFetched;
    LLPointer<LLViewerFetchedTexture> mEmissiveFetched;

    // J2C versions of packed buffers for uploading
    LLPointer<LLImageJ2C> mBaseColorJ2C;
    LLPointer<LLImageJ2C> mNormalJ2C;
    LLPointer<LLImageJ2C> mMetallicRoughnessJ2C;
    LLPointer<LLImageJ2C> mEmissiveJ2C;

    LLTextureCtrl* mBaseColorTextureCtrl;
    LLTextureCtrl* mMetallicTextureCtrl;
    LLTextureCtrl* mEmissiveTextureCtrl;
    LLTextureCtrl* mNormalTextureCtrl;
    LLColorSwatchCtrl* mBaseColorCtrl;
    LLColorSwatchCtrl* mEmissiveColorCtrl;
    void resetUnsavedChanges();
    void markChangesUnsaved(U32 dirty_flag);
    U32 mUnsavedChanges; // flags to indicate individual changed parameters
    U32 mRevertedChanges; // flags to indicate individual reverted parameters
    S32 mUploadingTexturesCount;
    S32 mExpectedUploadCost;
    LLUUID mAssetID;

    // 'Default' texture, unless it's null or from inventory is the one with the fee
    LLUUID mBaseColorTextureUploadId;
    LLUUID mMetallicTextureUploadId;
    LLUUID mEmissiveTextureUploadId;
    LLUUID mNormalTextureUploadId;

	LLUUID mMaterialItemID;
	LLUUID mObjectID;    

    std::string mMaterialName;
    // I am unsure if this is always the same as mObjectUUID, or why it exists
	// at the LLPreview level.  JC 2009-06-24
	LLUUID mNotecardObjectID;
    bool mIsOverride = false;

    bool mUploadingTexturesFailure;
};
#endif //LL_LLPREVIEWMATERIAL_H