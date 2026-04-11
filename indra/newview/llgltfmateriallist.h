/**
 * @file llgltfmateriallist.h
 * @brief Read-only GLTF material asset cache for Genesis PBR Lite
 */

#pragma once

#include "llassettype.h"
#include "llextendedstatus.h"
#include "llgltfmaterial.h"
#include "llpointer.h"

#include <unordered_map>
#include <vector>

class LLGLTFMaterial;

class LLGLTFMaterialList
{
public:
    LLGLTFMaterialList() {}

    // Returns cached material if available, otherwise triggers an async fetch.
    // Returns nullptr if not yet loaded.
    LLGLTFMaterial* getMaterial(const LLUUID& id);

    void addMaterial(const LLUUID& id, LLGLTFMaterial* material);
    void removeMaterial(const LLUUID& id);

    // Register an object as waiting for a material to load
    void addPendingObject(const LLUUID& material_id, const LLUUID& object_id);

protected:
    static void onAssetLoadComplete(
        const LLUUID& asset_uuid,
        LLAssetType::EType type,
        void* user_data,
        S32 status,
        LLExtStat ext_status);

    typedef boost::unordered_map<LLUUID, LLPointer<LLGLTFMaterial>> uuid_mat_map_t;
    uuid_mat_map_t mList;

    typedef boost::unordered_map<LLUUID, std::vector<LLUUID>> pending_map_t;
    pending_map_t mPendingObjects;
};

extern LLGLTFMaterialList gGLTFMaterialList;
