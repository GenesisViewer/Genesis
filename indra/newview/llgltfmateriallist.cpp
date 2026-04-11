/**
 * @file llgltfmateriallist.cpp
 * @brief Read-only GLTF material asset cache for Genesis PBR Lite
 */

#include "llviewerprecompiledheaders.h"

#include "llgltfmateriallist.h"

#include "llassetstorage.h"
#include "llfilesystem.h"
#include "llsdserialize.h"
#include "lltinygltfhelper.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "pipeline.h"

#include "tinygltf/tiny_gltf.h"

LLGLTFMaterialList gGLTFMaterialList;

LLGLTFMaterial* LLGLTFMaterialList::getMaterial(const LLUUID& id)
{
    if (id.isNull())
    {
        return nullptr;
    }

    uuid_mat_map_t::iterator iter = mList.find(id);
    if (iter != mList.end())
    {
        // Return the material if fully populated, nullptr if placeholder
        LLGLTFMaterial* mat = iter->second;
        if (mat && mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR].notNull())
        {
            return mat;
        }
        return nullptr;
    }

    // Not cached yet, create a placeholder and kick off an async fetch
    LLGLTFMaterial* mat = new LLGLTFMaterial();
    mList[id] = mat;

    LLUUID* id_copy = new LLUUID(id);
    gAssetStorage->getAssetData(
        id,
        LLAssetType::AT_MATERIAL,
        onAssetLoadComplete,
        (void*)id_copy);

    return nullptr;
}

void LLGLTFMaterialList::addMaterial(const LLUUID& id, LLGLTFMaterial* material)
{
    mList[id] = material;
}

void LLGLTFMaterialList::removeMaterial(const LLUUID& id)
{
    mList.erase(id);
}

void LLGLTFMaterialList::addPendingObject(const LLUUID& material_id, const LLUUID& object_id)
{
    mPendingObjects[material_id].push_back(object_id);
}

//static
void LLGLTFMaterialList::onAssetLoadComplete(
    const LLUUID& asset_uuid,
    LLAssetType::EType type,
    void* user_data,
    S32 status,
    LLExtStat ext_status)
{
    LLUUID material_id = user_data ? *(LLUUID*)user_data : asset_uuid;
    delete (LLUUID*)user_data;

    if (status != LL_ERR_NOERR)
    {
        LL_WARNS("GLTF") << "Failed to load material asset " << asset_uuid
        << " error: " << status << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    // Read asset bytes from cache
    LLFileSystem file(asset_uuid, type, LLFileSystem::READ);
    S32 size = file.getSize();
    if (size <= 0)
    {
        LL_WARNS("GLTF") << "Empty material asset " << asset_uuid << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    std::vector<char> buffer(size);
    file.read((U8*)buffer.data(), size);

    // Parse the LLSD wrapper
    std::string json_str(buffer.data(), size);
    LLSD asset;
    std::istringstream istr(json_str);
    if (!LLSDSerialize::deserialize(asset, istr, json_str.size()))
    {
        LL_WARNS("GLTF") << "Failed to parse material LLSD for " << asset_uuid << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    if (!asset.has("version") || !LLGLTFMaterial::isAcceptedVersion(asset["version"].asString()))
    {
        LL_WARNS("GLTF") << "Unrecognized material version for " << asset_uuid << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    if (!asset.has("type") || asset["type"].asString() != LLGLTFMaterial::ASSET_TYPE)
    {
        LL_WARNS("GLTF") << "Wrong asset type for " << asset_uuid << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    if (!asset.has("data"))
    {
        LL_WARNS("GLTF") << "No data field in material asset " << asset_uuid << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    std::string gltf_json = asset["data"].asString();

    // Parse via tinygltf
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warn, error;

    bool ok = loader.LoadASCIIFromString(
        &model, &error, &warn, gltf_json.c_str(), gltf_json.size(), "");

    if (!ok || model.materials.empty())
    {
        LL_WARNS("GLTF") << "Failed to parse GLTF JSON for " << asset_uuid
        << " error: " << error << LL_ENDL;
        gGLTFMaterialList.removeMaterial(asset_uuid);
        return;
    }

    // Build LLGLTFMaterial from parsed model
    LLGLTFMaterial* mat = new LLGLTFMaterial();
    const tinygltf::Material& gltf_mat = model.materials[0];

    // Base color texture
    int base_color_idx = gltf_mat.pbrMetallicRoughness.baseColorTexture.index;
    if (base_color_idx >= 0 && base_color_idx < (int)model.textures.size())
    {
        int img_idx = model.textures[base_color_idx].source;
        if (img_idx >= 0 && img_idx < (int)model.images.size())
        {
            LLUUID tex_id;
            tex_id.set(model.images[img_idx].uri, false);
            if (tex_id.notNull())
            {
                mat->mTextureId[LLGLTFMaterial::GLTF_TEXTURE_INFO_BASE_COLOR] = tex_id;
            }
        }
    }

    gGLTFMaterialList.addMaterial(asset_uuid, mat);

    // Notify any objects waiting on this material
    auto it = gGLTFMaterialList.mPendingObjects.find(asset_uuid);
    if (it != gGLTFMaterialList.mPendingObjects.end())
    {
        for (const LLUUID& obj_id : it->second)
        {
            LLViewerObject* obj = gObjectList.findObject(obj_id);
            if (obj && obj->mDrawable.notNull())
            {
                const LLRenderMaterialParams* params = obj->getRenderMaterialParams();
                obj->setRenderMaterialIDs(params);
                gPipeline.markTextured(obj->mDrawable);
            }
        }
        gGLTFMaterialList.mPendingObjects.erase(it);
    }
}
