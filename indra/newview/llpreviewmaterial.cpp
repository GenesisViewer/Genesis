/** 
 * @file llpreviewnotecard.cpp
 * @brief Implementation of the notecard editor
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewmaterial.h"
#include "llagent.h"
#include "llagentbenefits.h"
#include "roles_constants.h"
#include "llsdserialize.h"
#include "llscrollbar.h"
#include "llcolorswatch.h"
#include "llgltfmaterial.h"
#include "lluictrlfactory.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llnotificationsutil.h"
#include "llstatusbar.h" //can_afford_transaction
#include "tinygltf/tiny_gltf.h"
#include "lltinygltfhelper.h"
#include "lltrans.h"
#include "llviewermenufile.h" // upload_new_resource
#include "llfloaterperms.h"
#include <strstream>

const std::string MATERIAL_BASE_COLOR_DEFAULT_NAME = "Base Color";
const std::string MATERIAL_NORMAL_DEFAULT_NAME = "Normal";
const std::string MATERIAL_METALLIC_DEFAULT_NAME = "Metallic Roughness";
const std::string MATERIAL_EMISSIVE_DEFAULT_NAME = "Emissive";

// Dirty flags
static const U32 MATERIAL_BASE_COLOR_DIRTY = 0x1 << 0;
static const U32 MATERIAL_BASE_COLOR_TEX_DIRTY = 0x1 << 1;

static const U32 MATERIAL_NORMAL_TEX_DIRTY = 0x1 << 2;

static const U32 MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY = 0x1 << 3;
static const U32 MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY = 0x1 << 4;
static const U32 MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY = 0x1 << 5;

static const U32 MATERIAL_EMISIVE_COLOR_DIRTY = 0x1 << 6;
static const U32 MATERIAL_EMISIVE_TEX_DIRTY = 0x1 << 7;

static const U32 MATERIAL_DOUBLE_SIDED_DIRTY = 0x1 << 8;
static const U32 MATERIAL_ALPHA_MODE_DIRTY = 0x1 << 9;
static const U32 MATERIAL_ALPHA_CUTOFF_DIRTY = 0x1 << 10;

const S32 PREVIEW_MIN_WIDTH =
	2 * PREVIEW_BORDER +
	2 * PREVIEW_BUTTON_WIDTH + 
	PREVIEW_PAD + RESIZE_HANDLE_WIDTH +
	PREVIEW_PAD;
const S32 PREVIEW_MIN_HEIGHT = 
	2 * PREVIEW_BORDER +
	3*(20 + PREVIEW_PAD) +
	2 * SCROLLBAR_SIZE + 128;
// Default constructor
LLPreviewMaterial::LLPreviewMaterial(const std::string& name,
									 const LLRect& rect,
									 const std::string& title,
									 const LLUUID& item_id, 
									 const LLUUID& object_id,
									 const LLUUID& asset_id,
									 LLPointer<LLViewerInventoryItem> inv_item) :
	LLPreview(name, rect, title, item_id, object_id, TRUE,
			  PREVIEW_MIN_WIDTH,
			  PREVIEW_MIN_HEIGHT,
			  inv_item),
	mAssetID( asset_id ),
	mMaterialItemID(item_id),
	mObjectID(object_id), 
    mUnsavedChanges(0)
    , mRevertedChanges(0)
    , mExpectedUploadCost(0)
    , mUploadingTexturesCount(0)
    , mUploadingTexturesFailure(false)
{
    const LLInventoryItem* item = getItem();
    if (item)
    {
        mAssetID = item->getAssetUUID();
    }
    LLUICtrlFactory::getInstance()->buildFloater(this,"floater_material_editor.xml");
}

LLPreviewMaterial::~LLPreviewMaterial()
{
}
void LLPreviewMaterial::setCanSaveAs(bool value)
{
    if (!mIsOverride)
    {
        childSetEnabled("save_as", value);
    }
}

void LLPreviewMaterial::setCanSave(bool value)
{
    if (!mIsOverride)
    {
        childSetEnabled("save", value);
    }
}
void LLPreviewMaterial::setMaterialName(const std::string &name)
{
    setTitle(name);
    mMaterialName = name;
}
void LLPreviewMaterial::loadDefaults()
{
    tinygltf::Model model_in;
    model_in.materials.resize(1);
    setFromGltfModel(model_in, 0, true);
}
void LLPreviewMaterial::resetUnsavedChanges()
{
    mUnsavedChanges = 0;
    mRevertedChanges = 0;
    if (!mIsOverride)
    {
        //childSetVisible("unsaved_changes", false);
        setCanSave(false);

        mExpectedUploadCost = 0;
        getChild<LLUICtrl>("total_upload_fee")->setTextArg("[FEE]", llformat("%d", mExpectedUploadCost));
    }
}
//Should be in LLPreview maybe
BOOL LLPreviewMaterial::canModify(const LLUUID taskUUID, const LLInventoryItem* item) {
    const LLViewerObject* object = nullptr;
    if (taskUUID.notNull())
	{
		object = gObjectList.findObject(taskUUID);
	}

	if (object && !object->permModify())
	{
        // No permission to edit in-world inventory
        return FALSE;
	}

	return item && gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE);
}
void LLPreviewMaterial::markChangesUnsaved(U32 dirty_flag)
{
    mUnsavedChanges |= dirty_flag;
    if (mIsOverride)
    {
        // at the moment live editing (mIsOverride) applies everything 'live'
        // and "unsaved_changes", save/cancel buttons don't exist there
        return;
    }

    //childSetVisible("unsaved_changes", mUnsavedChanges);

    if (mUnsavedChanges)
    {
        const LLInventoryItem* item = getItem();
        if (item)
        {
            //LLPermissions perm(item->getPermissions());
            bool allow_modify = canModify(mObjectUUID, item);
            bool source_library = mObjectUUID.isNull() && gInventory.isObjectDescendentOf(mItemUUID, gInventory.getLibraryRootFolderID());
            bool source_notecard = mNotecardInventoryID.notNull();

            setCanSave(allow_modify && !source_library && !source_notecard);
        }
    }
    else
    {
        setCanSave(false);
    }

    S32 upload_texture_count = 0;
    if (mBaseColorTextureUploadId.notNull() && mBaseColorTextureUploadId == getBaseColorId())
    {
        upload_texture_count++;
    }
    if (mMetallicTextureUploadId.notNull() && mMetallicTextureUploadId == getMetallicRoughnessId())
    {
        upload_texture_count++;
    }
    if (mEmissiveTextureUploadId.notNull() && mEmissiveTextureUploadId == getEmissiveId())
    {
        upload_texture_count++;
    }
    if (mNormalTextureUploadId.notNull() && mNormalTextureUploadId == getNormalId())
    {
        upload_texture_count++;
    }

    mExpectedUploadCost = upload_texture_count * LLAgentBenefitsMgr::current().getTextureUploadCost();
    getChild<LLUICtrl>("total_upload_fee")->setTextArg("[FEE]", llformat("%d", mExpectedUploadCost));
}
LLUUID LLPreviewMaterial::getBaseColorId()
{
    return mBaseColorTextureCtrl->getValue().asUUID();
}

void LLPreviewMaterial::setBaseColorId(const LLUUID& id)
{
    mBaseColorTextureCtrl->setValue(id);
    mBaseColorTextureCtrl->setDefaultImageAssetID(id);
    mBaseColorTextureCtrl->setTentative(FALSE);
}

void LLPreviewMaterial::setBaseColorUploadId(const LLUUID& id)
{
    // Might be better to use local textures and
    // assign a fee in case of a local texture
    if (id.notNull())
    {
        // todo: this does not account for posibility of texture
        // being from inventory, need to check that
        childSetValue("base_color_upload_fee", getString("upload_fee_string"));
        // Only set if we will need to upload this texture
        mBaseColorTextureUploadId = id;
    }
    markChangesUnsaved(MATERIAL_BASE_COLOR_TEX_DIRTY);
}

LLColor4 LLPreviewMaterial::getBaseColor()
{
    LLColor4 ret = linearColor4(LLColor4(mBaseColorCtrl->getValue()));
    ret.mV[3] = getTransparency();
    return ret;
}

void LLPreviewMaterial::setBaseColor(const LLColor4& color)
{
    mBaseColorCtrl->setValue(srgbColor4(color).getValue());
    setTransparency(color.mV[3]);
}

F32 LLPreviewMaterial::getTransparency()
{
    return childGetValue("transparency").asReal();
}

void LLPreviewMaterial::setTransparency(F32 transparency)
{
    childSetValue("transparency", transparency);
}

std::string LLPreviewMaterial::getAlphaMode()
{
    return childGetValue("alpha mode").asString();
}

void LLPreviewMaterial::setAlphaMode(const std::string& alpha_mode)
{
    childSetValue("alpha mode", alpha_mode);
}

F32 LLPreviewMaterial::getAlphaCutoff()
{
    return childGetValue("alpha cutoff").asReal();
}

void LLPreviewMaterial::setAlphaCutoff(F32 alpha_cutoff)
{
    childSetValue("alpha cutoff", alpha_cutoff);
}


LLUUID LLPreviewMaterial::getMetallicRoughnessId()
{
    return mMetallicTextureCtrl->getValue().asUUID();
}

void LLPreviewMaterial::setMetallicRoughnessId(const LLUUID& id)
{
    mMetallicTextureCtrl->setValue(id);
    mMetallicTextureCtrl->setDefaultImageAssetID(id);
    mMetallicTextureCtrl->setTentative(FALSE);
}

void LLPreviewMaterial::setMetallicRoughnessUploadId(const LLUUID& id)
{
    if (id.notNull())
    {
        // todo: this does not account for posibility of texture
        // being from inventory, need to check that
        childSetValue("metallic_upload_fee", getString("upload_fee_string"));
        mMetallicTextureUploadId = id;
    }
    markChangesUnsaved(MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY);
}

F32 LLPreviewMaterial::getMetalnessFactor()
{
    return childGetValue("metalness factor").asReal();
}

void LLPreviewMaterial::setMetalnessFactor(F32 factor)
{
    childSetValue("metalness factor", factor);
}

F32 LLPreviewMaterial::getRoughnessFactor()
{
    return childGetValue("roughness factor").asReal();
}

void LLPreviewMaterial::setRoughnessFactor(F32 factor)
{
    childSetValue("roughness factor", factor);
}

LLUUID LLPreviewMaterial::getEmissiveId()
{
    return mEmissiveTextureCtrl->getValue().asUUID();
}

void LLPreviewMaterial::setEmissiveId(const LLUUID& id)
{
    mEmissiveTextureCtrl->setValue(id);
    mEmissiveTextureCtrl->setDefaultImageAssetID(id);
    mEmissiveTextureCtrl->setTentative(FALSE);
}

void LLPreviewMaterial::setEmissiveUploadId(const LLUUID& id)
{
    if (id.notNull())
    {
        // todo: this does not account for posibility of texture
        // being from inventory, need to check that
        childSetValue("emissive_upload_fee", getString("upload_fee_string"));
        mEmissiveTextureUploadId = id;
    }
    markChangesUnsaved(MATERIAL_EMISIVE_TEX_DIRTY);
}

LLColor4 LLPreviewMaterial::getEmissiveColor()
{
    return linearColor4(LLColor4(mEmissiveColorCtrl->getValue()));
}

void LLPreviewMaterial::setEmissiveColor(const LLColor4& color)
{
    mEmissiveColorCtrl->setValue(srgbColor4(color).getValue());
}

LLUUID LLPreviewMaterial::getNormalId()
{
    return mNormalTextureCtrl->getValue().asUUID();
}

void LLPreviewMaterial::setNormalId(const LLUUID& id)
{
    mNormalTextureCtrl->setValue(id);
    mNormalTextureCtrl->setDefaultImageAssetID(id);
    mNormalTextureCtrl->setTentative(FALSE);
}

void LLPreviewMaterial::setNormalUploadId(const LLUUID& id)
{
    if (id.notNull())
    {
        // todo: this does not account for posibility of texture
        // being from inventory, need to check that
        childSetValue("normal_upload_fee", getString("upload_fee_string"));
        mNormalTextureUploadId = id;
    }
    markChangesUnsaved(MATERIAL_NORMAL_TEX_DIRTY);
}

bool LLPreviewMaterial::getDoubleSided()
{
    return childGetValue("double sided").asBoolean();
}

void LLPreviewMaterial::setDoubleSided(bool double_sided)
{
    childSetValue("double sided", double_sided);
}
bool LLPreviewMaterial::setFromGltfModel(const tinygltf::Model& model, S32 index, bool set_textures)
{
    if (model.materials.size() > index)
    {
        const tinygltf::Material& material_in = model.materials[index];

        if (set_textures)
        {
            S32 index;
            LLUUID id;

            // get base color texture
            index = material_in.pbrMetallicRoughness.baseColorTexture.index;
            if (index >= 0)
            {
                id.set(model.images[index].uri);
                setBaseColorId(id);
            }
            else
            {
                setBaseColorId(LLUUID::null);
            }

            // get normal map
            index = material_in.normalTexture.index;
            if (index >= 0)
            {
                id.set(model.images[index].uri);
                setNormalId(id);
            }
            else
            {
                setNormalId(LLUUID::null);
            }

            // get metallic-roughness texture
            index = material_in.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (index >= 0)
            {
                id.set(model.images[index].uri);
                setMetallicRoughnessId(id);
            }
            else
            {
                setMetallicRoughnessId(LLUUID::null);
            }

            // get emissive texture
            index = material_in.emissiveTexture.index;
            if (index >= 0)
            {
                id.set(model.images[index].uri);
                setEmissiveId(id);
            }
            else
            {
                setEmissiveId(LLUUID::null);
            }
        }

        setAlphaMode(material_in.alphaMode);
        setAlphaCutoff(material_in.alphaCutoff);

        setBaseColor(LLTinyGLTFHelper::getColor(material_in.pbrMetallicRoughness.baseColorFactor));
        setEmissiveColor(LLTinyGLTFHelper::getColor(material_in.emissiveFactor));

        setMetalnessFactor(material_in.pbrMetallicRoughness.metallicFactor);
        setRoughnessFactor(material_in.pbrMetallicRoughness.roughnessFactor);

        setDoubleSided(material_in.doubleSided);
    }

    return true;
}
void LLPreviewMaterial::setEnableEditing(bool can_modify)
{
    childSetEnabled("double sided", can_modify);

    // BaseColor
    childSetEnabled("base color", can_modify);
    childSetEnabled("transparency", can_modify);
    childSetEnabled("alpha mode", can_modify);
    childSetEnabled("alpha cutoff", can_modify);

    // Metallic-Roughness
    childSetEnabled("metalness factor", can_modify);
    childSetEnabled("roughness factor", can_modify);

    // Metallic-Roughness
    childSetEnabled("metalness factor", can_modify);
    childSetEnabled("roughness factor", can_modify);

    // Emissive
    childSetEnabled("emissive color", can_modify);

    mBaseColorTextureCtrl->setEnabled(can_modify);
    mMetallicTextureCtrl->setEnabled(can_modify);
    mEmissiveTextureCtrl->setEnabled(can_modify);
    mNormalTextureCtrl->setEnabled(can_modify);
}
void LLPreviewMaterial::loadAsset()
{
    const LLInventoryItem* item;
    
    item = getItem();
   
    
    bool fail = false;

    if (item)
    {
        LLPermissions perm(item->getPermissions());
        bool allow_copy = gAgent.allowOperation(PERM_COPY, perm, GP_OBJECT_MANIPULATE);
        bool allow_modify = canModify(mObjectUUID, item);
        bool source_library = mObjectUUID.isNull() && gInventory.isObjectDescendentOf(mItemUUID, gInventory.getLibraryRootFolderID());
        setCanSaveAs(allow_copy);
        setMaterialName(item->getName());

        {
            mAssetID = item->getAssetUUID();

            if (mAssetID.isNull())
            {
                mAssetStatus = PREVIEW_ASSET_LOADED;
                loadDefaults();
                resetUnsavedChanges();
                setEnableEditing(allow_modify && !source_library);
            }
            else
            {
                LLUUID* new_uuid = new LLUUID(mItemUUID);
                LLHost source_sim = LLHost();
                LLSD* user_data = new LLSD();

                if (mObjectUUID.notNull())
                {
                    LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
                    if (objectp && objectp->getRegion())
                    {
                        source_sim = objectp->getRegion()->getHost();
                    }
                    else
                    {
                        // The object that we're trying to look at disappeared, bail.
                        LL_WARNS("MaterialEditor") << "Can't find object " << mObjectUUID << " associated with material." << LL_ENDL;
                        mAssetID.setNull();
                        mAssetStatus = PREVIEW_ASSET_LOADED;
                        resetUnsavedChanges();
                        setEnableEditing(allow_modify && !source_library);
                        return;
                    }
                    user_data->with("taskid", mObjectUUID).with("itemid", mItemUUID);
                }
                else
                {
                    user_data = new LLSD(mItemUUID);
                }

                setEnableEditing(false); // wait for it to load

                mAssetStatus = PREVIEW_ASSET_LOADING;

                // May callback immediately
                gAssetStorage->getAssetData(item->getAssetUUID(),
                    LLAssetType::AT_MATERIAL,
                    &onLoadComplete,
                    (void*)user_data,
                    TRUE);
                // gAssetStorage->getInvItemAsset(source_sim,
				// 								gAgent.getID(),
				// 								gAgent.getSessionID(),
				// 								item->getPermissions().getOwner(),
				// 								mObjectUUID,
				// 								item->getUUID(),
				// 								item->getAssetUUID(),
				// 								LLAssetType::AT_MATERIAL,
				// 								&onLoadComplete,
				// 								(void*)new_uuid,
				// 								TRUE);
            }
        }
    }
    else if (mObjectUUID.notNull() && mItemUUID.notNull())
    {
        LLViewerObject* objectp = gObjectList.findObject(mObjectUUID);
        if (objectp && (objectp->isInventoryPending() || objectp->isInventoryDirty()))
        {
            // It's a material in object's inventory and we failed to get it because inventory is not up to date.
            // Subscribe for callback and retry at inventoryChanged()
            registerVOInventoryListener(objectp, NULL); //removes previous listener

            if (objectp->isInventoryDirty())
            {
                objectp->requestInventory();
            }
        }
        else
        {
            fail = true;
        }
    }
    else
    {
        fail = true;
    }

    if (fail)
    {
        /*editor->setText(LLStringUtil::null);
        editor->makePristine();
        editor->setEnabled(TRUE);*/
        // Don't set asset status here; we may not have set the item id yet
        // (e.g. when this gets called initially)
        //mAssetStatus = PREVIEW_ASSET_LOADED;
    }
}
bool LLPreviewMaterial::decodeAsset(const std::vector<char>& buffer)
{
    LLSD asset;
    
    std::istrstream str(&buffer[0], buffer.size());
    if (LLSDSerialize::deserialize(asset, str, buffer.size()))
    {
        if (asset.has("version") && LLGLTFMaterial::isAcceptedVersion(asset["version"].asString()))
        {
            if (asset.has("type") && asset["type"].asString() == LLGLTFMaterial::ASSET_TYPE)
            {
                if (asset.has("data") && asset["data"].isString())
                {
                    std::string data = asset["data"];

                    tinygltf::TinyGLTF gltf;
                    tinygltf::TinyGLTF loader;
                    std::string        error_msg;
                    std::string        warn_msg;

                    tinygltf::Model model_in;

                    if (loader.LoadASCIIFromString(&model_in, &error_msg, &warn_msg, data.c_str(), data.length(), ""))
                    {
                        // assets are only supposed to have one item
                        // *NOTE: This duplicates some functionality from
                        // LLGLTFMaterial::fromJSON, but currently does the job
                        // better for the material editor use case.
                        // However, LLGLTFMaterial::asJSON should always be
                        // used when uploading materials, to ensure the
                        // asset is valid.
                        return setFromGltfModel(model_in, 0, true);
                    }
                    else
                    {
                        LL_WARNS("MaterialEditor")  << " Failed to decode material asset: " << LL_NEWLINE
                         << warn_msg << LL_NEWLINE
                         << error_msg << LL_ENDL;
                    }
                }
            }
        }
        else
        {
            LL_WARNS("MaterialEditor") << "Invalid LLSD content "<< asset <<  LL_ENDL;
        }
    }
    else
    {
        LL_WARNS("MaterialEditor") << "Failed to deserialize material LLSD for flaoter " << LL_ENDL;
    }

    return false;
}
bool LLPreviewMaterial::saveItem(LLPointer<LLInventoryItem>* itemptr)
{
	
	return true;
}
// virtual
BOOL LLPreviewMaterial::canClose()
{
	return TRUE;
}
// virtual
BOOL LLPreviewMaterial::postBuild()
{
	// if this is a 'live editor' instance, it is also
    // single instance and uses live overrides
    mIsOverride = FALSE;

    mBaseColorTextureCtrl = getChild<LLTextureCtrl>("base_color_texture");
    mMetallicTextureCtrl = getChild<LLTextureCtrl>("metallic_roughness_texture");
    mEmissiveTextureCtrl = getChild<LLTextureCtrl>("emissive_texture");
    mNormalTextureCtrl = getChild<LLTextureCtrl>("normal_texture");
    mBaseColorCtrl = getChild<LLColorSwatchCtrl>("base color");
    mEmissiveColorCtrl = getChild<LLColorSwatchCtrl>("emissive color");

    if (!gAgent.isGodlike())
    {
        // Only allow fully permissive textures
        mBaseColorTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
        mMetallicTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
        mEmissiveTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
        mNormalTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
    }

    // Texture callback
    mBaseColorTextureCtrl->setCommitCallback(boost::bind(&LLPreviewMaterial::onCommitTexture, this, _1, _2, MATERIAL_BASE_COLOR_TEX_DIRTY));
    mMetallicTextureCtrl->setCommitCallback(boost::bind(&LLPreviewMaterial::onCommitTexture, this, _1, _2, MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY));
    mEmissiveTextureCtrl->setCommitCallback(boost::bind(&LLPreviewMaterial::onCommitTexture, this, _1, _2, MATERIAL_EMISIVE_TEX_DIRTY));
    mNormalTextureCtrl->setCommitCallback(boost::bind(&LLPreviewMaterial::onCommitTexture, this, _1, _2, MATERIAL_NORMAL_TEX_DIRTY));

    if (mIsOverride)
    {
        // Live editing needs a recovery mechanism on cancel
        mBaseColorTextureCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_BASE_COLOR_TEX_DIRTY));
        mMetallicTextureCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY));
        mEmissiveTextureCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_EMISIVE_TEX_DIRTY));
        mNormalTextureCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_NORMAL_TEX_DIRTY));

        // Save applied changes on 'OK' to our recovery mechanism.
        mBaseColorTextureCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_BASE_COLOR_TEX_DIRTY));
        mMetallicTextureCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY));
        mEmissiveTextureCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_EMISIVE_TEX_DIRTY));
        mNormalTextureCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_NORMAL_TEX_DIRTY));
    }
    else
    {
        mBaseColorTextureCtrl->setCanApplyImmediately(false);
        mMetallicTextureCtrl->setCanApplyImmediately(false);
        mEmissiveTextureCtrl->setCanApplyImmediately(false);
        mNormalTextureCtrl->setCanApplyImmediately(false);
    }

    if (!mIsOverride)
    {
         childSetAction("save", boost::bind(&LLPreviewMaterial::onClickSave, this));
    //     childSetAction("save_as", boost::bind(&LLPreviewMaterial::onClickSaveAs, this));
    //     childSetAction("cancel", boost::bind(&LLPreviewMaterial::onClickCancel, this));
    }

    if (mIsOverride)
    {
        childSetVisible("base_color_upload_fee", FALSE);
        childSetVisible("metallic_upload_fee", FALSE);
        childSetVisible("emissive_upload_fee", FALSE);
        childSetVisible("normal_upload_fee", FALSE);
    }
    else
    {
        S32 upload_cost = LLAgentBenefitsMgr::current().getTextureUploadCost();
        getChild<LLUICtrl>("base_color_upload_fee")->setTextArg("[FEE]", llformat("%d", upload_cost));
        getChild<LLUICtrl>("metallic_upload_fee")->setTextArg("[FEE]", llformat("%d", upload_cost));
        getChild<LLUICtrl>("emissive_upload_fee")->setTextArg("[FEE]", llformat("%d", upload_cost));
        getChild<LLUICtrl>("normal_upload_fee")->setTextArg("[FEE]", llformat("%d", upload_cost));
    }

    boost::function<void(LLUICtrl*, void*)> changes_callback = [this](LLUICtrl * ctrl, void* userData)
    {
        const U32 *flag = (const U32*)userData;
        markChangesUnsaved(*flag);
        // Apply changes to object live
        applyToSelection();
    };
 
    childSetCommitCallback("double sided", changes_callback, (void*)&MATERIAL_DOUBLE_SIDED_DIRTY);

    // BaseColor
    mBaseColorCtrl->setCommitCallback(changes_callback, (void*)&MATERIAL_BASE_COLOR_DIRTY);
    if (mIsOverride)
    {
        mBaseColorCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_BASE_COLOR_DIRTY));
        mBaseColorCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_BASE_COLOR_DIRTY));
    }
    else
    {
        mBaseColorCtrl->setCanApplyImmediately(false);
    }
    // transparency is a part of base color
    childSetCommitCallback("transparency", changes_callback, (void*)&MATERIAL_BASE_COLOR_DIRTY);
    childSetCommitCallback("alpha mode", changes_callback, (void*)&MATERIAL_ALPHA_MODE_DIRTY);
    childSetCommitCallback("alpha cutoff", changes_callback, (void*)&MATERIAL_ALPHA_CUTOFF_DIRTY);

    // Metallic-Roughness
    childSetCommitCallback("metalness factor", changes_callback, (void*)&MATERIAL_METALLIC_ROUGHTNESS_METALNESS_DIRTY);
    childSetCommitCallback("roughness factor", changes_callback, (void*)&MATERIAL_METALLIC_ROUGHTNESS_ROUGHNESS_DIRTY);

    // Emissive
    mEmissiveColorCtrl->setCommitCallback(changes_callback, (void*)&MATERIAL_EMISIVE_COLOR_DIRTY);
    if (mIsOverride)
    {
        mEmissiveColorCtrl->setOnCancelCallback(boost::bind(&LLPreviewMaterial::onCancelCtrl, this, _1, _2, MATERIAL_EMISIVE_COLOR_DIRTY));
        mEmissiveColorCtrl->setOnSelectCallback(boost::bind(&LLPreviewMaterial::onSelectCtrl, this, _1, _2, MATERIAL_EMISIVE_COLOR_DIRTY));
    }
    else
    {
        mEmissiveColorCtrl->setCanApplyImmediately(false);
    }

    if (!mIsOverride)
    {
        // "unsaved_changes" doesn't exist in live editor
        //childSetVisible("unsaved_changes", mUnsavedChanges);

        // Doesn't exist in live editor
        getChild<LLUICtrl>("total_upload_fee")->setTextArg("[FEE]", llformat("%d", 0));
    }

    // Todo:
    // Disable/enable setCanApplyImmediately() based on
    // working from inventory, upload or editing inworld

	return LLPreview::postBuild();
}
void LLPreviewMaterial::onClickSave()
{
    if (!capabilitiesAvailable())
    {
        LLNotificationsUtil::add("MissingMaterialCaps");
        return;
    }
    if (!can_afford_transaction(mExpectedUploadCost))
    {
        LLSD args;
        args["COST"] = llformat("%d", mExpectedUploadCost);
        LLNotificationsUtil::add("ErrorCannotAffordUpload", args);
        return;
    }

    applyToSelection();
    saveIfNeeded();
}
//TODO
void LLPreviewMaterial::applyToSelection()
{
}
//TODO
void LLPreviewMaterial::onSelectCtrl(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag)
{
    mUnsavedChanges |= dirty_flag;
    applyToSelection();

    

    //LLSelectMgr::getInstance()->getSelection()->applyToNodes(&func);
}
void LLPreviewMaterial::onCommitTexture(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag)
{
    if (!mIsOverride)
    {
        std::string upload_fee_ctrl_name;
        LLUUID old_uuid;

        switch (dirty_flag)
        {
        case MATERIAL_BASE_COLOR_TEX_DIRTY:
        {
            upload_fee_ctrl_name = "base_color_upload_fee";
            old_uuid = mBaseColorTextureUploadId;
            break;
        }
        case MATERIAL_METALLIC_ROUGHTNESS_TEX_DIRTY:
        {
            upload_fee_ctrl_name = "metallic_upload_fee";
            old_uuid = mMetallicTextureUploadId;
            break;
        }
        case MATERIAL_EMISIVE_TEX_DIRTY:
        {
            upload_fee_ctrl_name = "emissive_upload_fee";
            old_uuid = mEmissiveTextureUploadId;
            break;
        }
        case MATERIAL_NORMAL_TEX_DIRTY:
        {
            upload_fee_ctrl_name = "normal_upload_fee";
            old_uuid = mNormalTextureUploadId;
            break;
        }
        default:
            break;
        }
        LLUUID new_val = ctrl->getValue().asUUID();
        if (new_val == old_uuid && old_uuid.notNull())
        {
            childSetValue(upload_fee_ctrl_name, getString("upload_fee_string"));
        }
        else
        {
            // Texture picker has 'apply now' with 'cancel' support.
            // Don't clean mBaseColorJ2C and mBaseColorFetched, it's our
            // storage in case user decides to cancel changes.
            // Without mBaseColorFetched, viewer will eventually cleanup
            // the texture that is not in use
            childSetValue(upload_fee_ctrl_name, getString("no_upload_fee_string"));
        }
    }

    markChangesUnsaved(dirty_flag);
    applyToSelection();
}
void LLPreviewMaterial::onCancelCtrl(LLUICtrl* ctrl, const LLSD& data, S32 dirty_flag)
{
    mRevertedChanges |= dirty_flag;
    applyToSelection();
}
void LLPreviewMaterial::inventoryChanged(LLViewerObject* object,
    LLInventoryObject::object_list_t* inventory,
    S32 serial_num,
    void* user_data)
{
    removeVOInventoryListener();
    loadAsset();
}
// static
LLPreviewMaterial* LLPreviewMaterial::getInstance(const LLUUID& item_id)
{
	LLPreview* instance = NULL;
	preview_map_t::iterator found_it = LLPreview::sInstances.find(item_id);
	if(found_it != LLPreview::sInstances.end())
	{
		instance = found_it->second;
	}
	return (LLPreviewMaterial*)instance;
}
// static
void LLPreviewMaterial::onLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid,
    LLAssetType::EType type,
    void* user_data, S32 status, LLExtStat ext_status)
{
    LLSD* floater_key = (LLSD*)user_data;
    LL_INFOS("MaterialEditor") << "loading " << asset_uuid << " for " << *floater_key << LL_ENDL;
    //LLPreviewMaterial* editor = LLFloaterReg::findTypedInstance<LLPreviewMaterial>("material_editor", *floater_key);
    LLPreviewMaterial* editor = LLPreviewMaterial::getInstance((*floater_key).asUUID());
    if (editor)
    {
        LL_INFOS("MaterialEditor") << "editor found for " << *floater_key << LL_ENDL;
        if (asset_uuid != editor->mAssetID)
        {
            LL_WARNS("MaterialEditor") << "Asset id mismatch, expected: " << editor->mAssetID << " got: " << asset_uuid << LL_ENDL;
        }
        if (0 == status)
        {
            LL_INFOS("MaterialEditor") << "loading from LVFS " << asset_uuid << LL_ENDL;
            LLVFile file(vfs, asset_uuid, type, LLVFile::READ);

			S32 file_length = file.getSize();

			std::vector<char> buffer(file_length + 1);
            file.read((U8*)&buffer[0], file_length);		/*Flawfinder: ignore*/

			

            editor->decodeAsset(buffer);

            BOOL allow_modify = editor->canModify(editor->mObjectUUID, editor->getItem());
            LL_INFOS("MaterialEditor") << "allow_modify " << allow_modify << LL_ENDL;
            BOOL source_library = editor->mObjectUUID.isNull() && gInventory.isObjectDescendentOf(editor->mItemUUID, gInventory.getLibraryRootFolderID());
            editor->setEnableEditing(allow_modify && !source_library);
            editor->resetUnsavedChanges();
            editor->mAssetStatus = PREVIEW_ASSET_LOADED;
            editor->setEnabled(true); // ready for use
           
        }
        else
        {
            if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
                LL_ERR_FILE_EMPTY == status)
            {
                LLNotificationsUtil::add("MaterialMissing");
            }
            else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
            {
                // Not supposed to happen?
                LL_WARNS("MaterialEditor") << "No permission to view material " << asset_uuid << LL_ENDL;
                LLNotificationsUtil::add("MaterialNoPermissions");
            }
            else
            {
                LLNotificationsUtil::add("UnableToLoadMaterial");
            }
            editor->setEnableEditing(false);

            LL_WARNS("MaterialEditor") << "Problem loading material: " << status << LL_ENDL;
            editor->mAssetStatus = PREVIEW_ASSET_ERROR;
        }
    }
    else
    {
        LL_DEBUGS("MaterialEditor") << "Floater " << *floater_key << " does not exist." << LL_ENDL;
    }
    delete floater_key;
}
bool LLPreviewMaterial::capabilitiesAvailable()
{
    const LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("MaterialEditor") << "Not connected to a region, cannot save material." << LL_ENDL;
        return false;
    }
    std::string agent_url = region->getCapability("UpdateMaterialAgentInventory");
    std::string task_url = region->getCapability("UpdateMaterialTaskInventory");

    return (!agent_url.empty() && !task_url.empty());
}
bool LLPreviewMaterial::saveIfNeeded()
{
    if (mUploadingTexturesCount > 0)
    {
        // Upload already in progress, wait until
        // textures upload will retry saving on callback.
        // Also should prevent some failure-callbacks
        return true;
    }
    if (saveTextures() > 0)
    {
        // started texture upload
        setEnabled(false);
        return true;
    }
    std::string buffer = getEncodedAsset();
    const LLInventoryItem* item = getItem();
    // save it out to database
    if (item)
    {
        if (!updateInventoryItem(buffer, mItemUUID, mObjectUUID))
        {
            return false;
        }

        if (mCloseAfterSave)
        {
            close();
        }
        else
        {
            mAssetStatus = PREVIEW_ASSET_LOADING;
            setEnabled(false);
        }
    }
    else
    { 
        // Make a new inventory item and set upload permissions
        LLPermissions local_permissions;
        local_permissions.init(gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null);

        U32 everyone_perm = LLFloaterPerms::getEveryonePerms("Materials");
        U32 group_perm = LLFloaterPerms::getGroupPerms("Materials");
        U32 next_owner_perm = LLFloaterPerms::getNextOwnerPerms("Materials");
        local_permissions.initMasks(PERM_ALL, PERM_ALL, everyone_perm, group_perm, next_owner_perm);

        std::string res_desc = buildMaterialDescription();
        createInventoryItem(buffer, mMaterialName, res_desc, local_permissions);

        // We do not update floater with uploaded asset yet, so just close it.
        close();
    }
    return true;
}
/**
 * Build a description of the material we just imported.
 * Currently this means a list of the textures present but we
 * may eventually want to make it more complete - will be guided
 * by what the content creators say they need.
 */
const std::string LLPreviewMaterial::buildMaterialDescription()
{
    std::ostringstream desc;
    desc << LLTrans::getString("Material Texture Name Header");

    // add the texture names for each just so long as the material
    // we loaded has an entry for it (i think testing the texture 
    // control UUI for NULL is a valid metric for if it was loaded
    // or not but I suspect this code will change a lot so may need
    // to revisit
    if (!mBaseColorTextureCtrl->getValue().asUUID().isNull())
    {
        desc << mBaseColorName;
        desc << ", ";
    }
    if (!mMetallicTextureCtrl->getValue().asUUID().isNull())
    {
        desc << mMetallicRoughnessName;
        desc << ", ";
    }
    if (!mEmissiveTextureCtrl->getValue().asUUID().isNull())
    {
        desc << mEmissiveName;
        desc << ", ";
    }
    if (!mNormalTextureCtrl->getValue().asUUID().isNull())
    {
        desc << mNormalName;
    }

    // trim last char if it's a ',' in case there is no normal texture
    // present and the code above inserts one
    // (no need to check for string length - always has initial string)
    std::string::iterator iter = desc.str().end() - 1;
    if (*iter == ',')
    {
        desc.str().erase(iter);
    }

    // sanitize the material description so that it's compatible with the inventory
    // note: split this up because clang doesn't like operating directly on the
    // str() - error: lvalue reference to type 'basic_string<...>' cannot bind to a
    // temporary of type 'basic_string<...>'
    std::string inv_desc = desc.str();
    LLInventoryObject::correctInventoryName(inv_desc);

    return inv_desc;
}
S32 LLPreviewMaterial::saveTextures()
{
    mUploadingTexturesFailure = false; // not supposed to get here if already uploading

    S32 work_count = 0;
    LLUUID key = mMaterialItemID; // must be locally declared for lambda's capture to work
    if (mBaseColorTextureUploadId == getBaseColorId() && mBaseColorTextureUploadId.notNull())
    {
        mUploadingTexturesCount++;
        work_count++;
        saveTexture(mBaseColorJ2C, mBaseColorName, mBaseColorTextureUploadId, [key](const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status)
        {
            LLPreviewMaterial* me = LLPreviewMaterial::getInstance(key);
            if (me)
            {
                bool success = status == LL_ERR_NOERR;
                if (success)
                {
                    me->setBaseColorId(asset_id);

                    // discard upload buffers once texture have been saved
                    me->mBaseColorJ2C = nullptr;
                    me->mBaseColorFetched = nullptr;
                    me->mBaseColorTextureUploadId.setNull();

                    me->mUploadingTexturesCount--;

                    if (!me->mUploadingTexturesFailure)
                    {
                        // try saving
                        me->saveIfNeeded();
                    }
                    else if (me->mUploadingTexturesCount == 0)
                    {
                        me->setEnabled(true);
                    }
                }
                else
                {
                    // stop upload if possible, unblock and let user decide
                    me->setFailedToUploadTexture();
                }
            }
        });
    }
    if (mNormalTextureUploadId == getNormalId() && mNormalTextureUploadId.notNull())
    {
        mUploadingTexturesCount++;
        work_count++;
        saveTexture(mNormalJ2C, mNormalName, mNormalTextureUploadId, [key](const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status)
        {
            LLPreviewMaterial* me = LLPreviewMaterial::getInstance(key);
            if (me)
            {
                bool success = status == LL_ERR_NOERR;
                if (success)
                {
                    me->setNormalId(asset_id);

                    // discard upload buffers once texture have been saved
                    me->mNormalJ2C = nullptr;
                    me->mNormalFetched = nullptr;
                    me->mNormalTextureUploadId.setNull();

                    me->mUploadingTexturesCount--;

                    if (!me->mUploadingTexturesFailure)
                    {
                        // try saving
                        me->saveIfNeeded();
                    }
                    else if (me->mUploadingTexturesCount == 0)
                    {
                        me->setEnabled(true);
                    }
                }
                else
                {
                    // stop upload if possible, unblock and let user decide
                    me->setFailedToUploadTexture();
                }
            }
        });
    }
    if (mMetallicTextureUploadId == getMetallicRoughnessId() && mMetallicTextureUploadId.notNull())
    {
        mUploadingTexturesCount++;
        work_count++;
        saveTexture(mMetallicRoughnessJ2C, mMetallicRoughnessName, mMetallicTextureUploadId, [key](const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status)
        {
            LLPreviewMaterial* me = LLPreviewMaterial::getInstance(key);
            if (me)
            {
                bool success = status == LL_ERR_NOERR;
                if (success)
                {
                    me->setMetallicRoughnessId(asset_id);

                    // discard upload buffers once texture have been saved
                    me->mMetallicRoughnessJ2C = nullptr;
                    me->mMetallicRoughnessFetched = nullptr;
                    me->mMetallicTextureUploadId.setNull();

                    me->mUploadingTexturesCount--;

                    if (!me->mUploadingTexturesFailure)
                    {
                        // try saving
                        me->saveIfNeeded();
                    }
                    else if (me->mUploadingTexturesCount == 0)
                    {
                        me->setEnabled(true);
                    }
                }
                else
                {
                    // stop upload if possible, unblock and let user decide
                    me->setFailedToUploadTexture();
                }
            }
        });
    }

    if (mEmissiveTextureUploadId == getEmissiveId() && mEmissiveTextureUploadId.notNull())
    {
        mUploadingTexturesCount++;
        work_count++;
        saveTexture(mEmissiveJ2C, mEmissiveName, mEmissiveTextureUploadId, [key](const LLUUID &asset_id, void *user_data, S32 status, LLExtStat ext_status)
        {
            LLPreviewMaterial* me = LLPreviewMaterial::getInstance(key);
            if (me)
            {
                bool success = status == LL_ERR_NOERR;
                if (success)
                {
                    me->setEmissiveId(asset_id);

                    // discard upload buffers once texture have been saved
                    me->mEmissiveJ2C = nullptr;
                    me->mEmissiveFetched = nullptr;
                    me->mEmissiveTextureUploadId.setNull();

                    me->mUploadingTexturesCount--;

                    if (!me->mUploadingTexturesFailure)
                    {
                        // try saving
                        me->saveIfNeeded();
                    }
                    else if (me->mUploadingTexturesCount == 0)
                    {
                        me->setEnabled(true);
                    }
                }
                else
                {
                    // stop upload if possible, unblock and let user decide
                    me->setFailedToUploadTexture();
                }
            }
        });
    }

    if (!work_count)
    {
        // Discard upload buffers once textures have been confirmed as saved.
        // Otherwise we keep buffers for potential upload failure recovery.
        clearTextures();
    }

    // asset storage can callback immediately, causing a decrease
    // of mUploadingTexturesCount, report amount of work scheduled
    // not amount of work remaining
    return work_count;
}
void LLPreviewMaterial::clearTextures()
{
    mBaseColorJ2C = nullptr;
    mNormalJ2C = nullptr;
    mEmissiveJ2C = nullptr;
    mMetallicRoughnessJ2C = nullptr;

    mBaseColorFetched = nullptr;
    mNormalFetched = nullptr;
    mMetallicRoughnessFetched = nullptr;
    mEmissiveFetched = nullptr;

    mBaseColorTextureUploadId.setNull();
    mNormalTextureUploadId.setNull();
    mMetallicTextureUploadId.setNull();
    mEmissiveTextureUploadId.setNull();
}
void LLPreviewMaterial::setFailedToUploadTexture()
{
    mUploadingTexturesFailure = true;
    mUploadingTexturesCount--;
    if (mUploadingTexturesCount == 0)
    {
        setEnabled(true);
    }
}

void LLPreviewMaterial::saveTexture(LLImageJ2C* img, const std::string& name, const LLUUID& asset_id, LLAssetStorage::LLStoreAssetCallback cb)
{
    if (asset_id.isNull()
        || img == nullptr
        || img->getDataSize() == 0)
    {
        return;
    }

    // // copy image bytes into string
    // std::string buffer;
    // buffer.assign((const char*) img->getData(), img->getDataSize());

    U32 expected_upload_cost = LLAgentBenefitsMgr::current().getTextureUploadCost();

    LLSD key = mMaterialItemID;
    std::function<bool(LLUUID itemId, LLSD response, std::string reason)> failed_upload([key](LLUUID assetId, LLSD response, std::string reason)
    {
        LLPreviewMaterial* me = LLPreviewMaterial::getInstance(key);
        if (me)
        {
            me->setFailedToUploadTexture();
        }
        return true; // handled
    });
    
    // LLResourceUploadInfo::ptr_t uploadInfo(std::make_shared<LLNewBufferedResourceUploadInfo>(
    //     buffer,
    //     asset_id,
    //     name, 
    //     name, 
    //     0,
    //     LLFolderType::FT_TEXTURE, 
    //     LLInventoryType::IT_TEXTURE,
    //     LLAssetType::AT_TEXTURE,
    //     LLFloaterPerms::getNextOwnerPerms("Uploads"),
    //     LLFloaterPerms::getGroupPerms("Uploads"),
    //     LLFloaterPerms::getEveryonePerms("Uploads"),
    //     expected_upload_cost, 
    //     false,
    //     cb,
    //     failed_upload));

    // upload_new_resource(uploadInfo);
    // gen a new uuid for this asset
	LLTransactionID tid;
	tid.generate();
	LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	LLVFile::writeFile(img->getData(), img->getDataSize(), gVFS, new_asset_id, LLAssetType::AT_TEXTURE);
	
	
	upload_new_resource(tid,	// tid
				LLAssetType::AT_TEXTURE,
				name,
                name,
				0,
				LLFolderType::FT_TEXTURE,
				LLInventoryType::IT_TEXTURE,
				LLFloaterPerms::getNextOwnerPerms("Uploads"),  
				LLFloaterPerms::getGroupPerms("Uploads"), 
				LLFloaterPerms::getEveryonePerms("Uploads"),
				name,
				cb, expected_upload_cost, NULL,NULL);
	
}
// Get a dump of the json representation of the current state of the editor UI
// in GLTF format, excluding transforms as they are not supported in material
// assets. (See also LLGLTFMaterial::sanitizeAssetMaterial())
void LLPreviewMaterial::getGLTFMaterial(LLGLTFMaterial* mat)
{
    mat->mBaseColor = getBaseColor();
    mat->mBaseColor.mV[3] = getTransparency();
    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] = getBaseColorId();

    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_NORMAL] = getNormalId();

    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS] = getMetallicRoughnessId();
    mat->mMetallicFactor = getMetalnessFactor();
    mat->mRoughnessFactor = getRoughnessFactor();

    mat->mEmissiveColor = getEmissiveColor();
    mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_EMISSIVE] = getEmissiveId();

    mat->mDoubleSided = getDoubleSided();
    mat->setAlphaMode(getAlphaMode());
    mat->mAlphaCutoff = getAlphaCutoff();
}
std::string LLPreviewMaterial::getEncodedAsset()
{
    LLSD asset;
    asset["version"] = LLGLTFMaterial::ASSET_VERSION;
    asset["type"] = LLGLTFMaterial::ASSET_TYPE;
    LLGLTFMaterial mat;
    getGLTFMaterial(&mat);
    asset["data"] = mat.asJSON();

    std::ostringstream str;
    LLSDSerialize::serialize(asset, str, LLSDSerialize::LLSD_BINARY);

    return str.str();
}

bool LLPreviewMaterial::updateInventoryItem(const std::string &buffer, const LLUUID &item_id, const LLUUID &task_id)
{
    const LLViewerRegion* region = gAgent.getRegion();
    if (!region)
    {
        LL_WARNS("MaterialEditor") << "Not connected to a region, cannot save material." << LL_ENDL;
        return false;
    }
    std::string agent_url = region->getCapability("UpdateMaterialAgentInventory");
    std::string task_url = region->getCapability("UpdateMaterialTaskInventory");

    if (!agent_url.empty() && !task_url.empty())
    {
        std::string url;
        //LLResourceUploadInfo::ptr_t uploadInfo;

        if (task_id.isNull() && !agent_url.empty())
        {
            // We need to update the asset information
            LLTransactionID tid;
            LLAssetID asset_id;
            tid.generate();
            asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

            LLVFile file(gVFS, asset_id, LLAssetType::AT_MATERIAL, LLVFile::APPEND);
            S32 size = buffer.length() + 1;
            file.setMaxSize(size);
            file.write((U8*)buffer.c_str(), size);

            // uploadInfo = std::make_shared<LLBufferedAssetUploadInfo>(item_id, LLAssetType::AT_MATERIAL, buffer,
            //     [](LLUUID itemId, LLUUID newAssetId, LLUUID newItemId, LLSD)
            //     {
            //         // done callback
            //         LLMaterialEditor::finishInventoryUpload(itemId, newAssetId, newItemId);
            //     },
            //     [](LLUUID itemId, LLUUID taskId, LLSD response, std::string reason)
            //     {
            //         // failure callback
            //         LLMaterialEditor* me = LLFloaterReg::findTypedInstance<LLMaterialEditor>("material_editor", LLSD(itemId));
            //         if (me)
            //         {
            //             me->setEnabled(true);
            //         }
            //         return true;
            //     }
            //     );
            // url = agent_url;
            LLSD body;
				body["item_id"] = mItemUUID;
            LLHTTPClient::post(agent_url, body,
					new LLUpdateAgentInventoryResponder(body, asset_id, LLAssetType::AT_MATERIAL));

        }
        // else if (!task_id.isNull() && !task_url.empty())
        // {
        //     uploadInfo = std::make_shared<LLBufferedAssetUploadInfo>(task_id, item_id, LLAssetType::AT_MATERIAL, buffer,
        //         [](LLUUID itemId, LLUUID task_id, LLUUID newAssetId, LLSD)
        //         {
        //             // done callback
        //             LLMaterialEditor::finishTaskUpload(itemId, newAssetId, task_id);
        //         },
        //         [](LLUUID itemId, LLUUID task_id, LLSD response, std::string reason)
        //         {
        //             // failure callback
        //             LLSD floater_key;
        //             floater_key["taskid"] = task_id;
        //             floater_key["itemid"] = itemId;
        //             LLMaterialEditor* me = LLFloaterReg::findTypedInstance<LLMaterialEditor>("material_editor", floater_key);
        //             if (me)
        //             {
        //                 me->setEnabled(true);
        //             }
        //             return true;
        //         }
        //         );
        //     url = task_url;
        // }

        if (!url.empty())
        {
            //LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);
            
        }
        else
        {
            return false;
        }

    }
    else // !gAssetStorage
    {
        LL_WARNS("MaterialEditor") << "Not connected to an materials capable region." << LL_ENDL;
        return false;
    }

    // todo: apply permissions from textures here if server doesn't
    // if any texture is 'no transfer', material should be 'no transfer' as well

    return true;
}
void LLPreviewMaterial::createInventoryItem(const std::string &buffer, const std::string &name, const std::string &desc, const LLPermissions& permissions)
{
    // gen a new uuid for this asset
    LLTransactionID tid;
    tid.generate();     // timestamp-based randomization + uniquification
    //LLUUID parent = gInventory.findUserDefinedCategoryUUIDForType(LLFolderType::FT_MATERIAL);
    LLUUID parent = gInventory.findCategoryUUIDForType(LLFolderType::FT_MATERIAL);
    const U8 subtype = 0;//NO_INV_SUBTYPE;  // TODO maybe use AT_SETTINGS and LLSettingsType::ST_MATERIAL ?

    // LLPointer<LLObjectsMaterialItemCallback> cb = new LLObjectsMaterialItemCallback(permissions, buffer, name);
    // create_inventory_item(gAgent.getID(), gAgent.getSessionID(), parent, tid, name, desc,
    //     LLAssetType::AT_MATERIAL, LLInventoryType::IT_MATERIAL, subtype, permissions.getMaskNextOwner(),
    //     cb);
}