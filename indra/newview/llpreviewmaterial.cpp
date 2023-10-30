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

#include "tinygltf/tiny_gltf.h"
#include "lltinygltfhelper.h"
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
	mObjectID(object_id)
{
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
        childSetVisible("unsaved_changes", false);
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

    childSetVisible("unsaved_changes", mUnsavedChanges);

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
    if (mNotecardInventoryID.notNull())
    {
        item = mAuxItem.get();
    }
    else
    {
        item = getItem();
    }
    
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

                if (mNotecardInventoryID.notNull())
                {
                    user_data->with("objectid", mNotecardObjectID).with("notecardid", mNotecardInventoryID);
                }
                else if (mObjectUUID.notNull())
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

    // if (!mIsOverride)
    // {
    //     childSetAction("save", boost::bind(&LLPreviewMaterial::onClickSave, this));
    //     childSetAction("save_as", boost::bind(&LLPreviewMaterial::onClickSaveAs, this));
    //     childSetAction("cancel", boost::bind(&LLPreviewMaterial::onClickCancel, this));
    // }

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
        childSetVisible("unsaved_changes", mUnsavedChanges);

        // Doesn't exist in live editor
        getChild<LLUICtrl>("total_upload_fee")->setTextArg("[FEE]", llformat("%d", 0));
    }

    // Todo:
    // Disable/enable setCanApplyImmediately() based on
    // working from inventory, upload or editing inworld

	return LLPreview::postBuild();
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
    LLPreviewMaterial* editor = LLPreviewMaterial::getInstance(*floater_key);
    if (editor)
    {
        if (asset_uuid != editor->mAssetID)
        {
            LL_WARNS("MaterialEditor") << "Asset id mismatch, expected: " << editor->mAssetID << " got: " << asset_uuid << LL_ENDL;
        }
        if (0 == status)
        {
            LLVFile file(vfs, asset_uuid, type, LLVFile::READ);

			S32 file_length = file.getSize();

			std::vector<char> buffer(file_length + 1);
            file.read((U8*)&buffer[0], file_length);		/*Flawfinder: ignore*/

			

            editor->decodeAsset(buffer);

            BOOL allow_modify = editor->canModify(editor->mObjectUUID, editor->getItem());
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