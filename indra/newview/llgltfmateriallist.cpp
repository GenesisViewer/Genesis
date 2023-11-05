/**
 * @file   llgltfmateriallist.cpp
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llgltfmateriallist.h"
#include "tinygltf/tiny_gltf.h"
#include "llsdserialize.h"
#include <strstream>
LLGLTFMaterialList gGLTFMaterialList;
const LLUUID LLGLTFMaterialList::BLANK_MATERIAL_ASSET_ID("968cbad0-4dad-d64e-71b5-72bf13ad051a");

class AssetLoadUserData
{
public:
    AssetLoadUserData() {}
    tinygltf::Model mModelIn;
    LLPointer<LLFetchedGLTFMaterial> mMaterial;
};


void LLGLTFMaterialList::addMaterial(const LLUUID& id, LLFetchedGLTFMaterial* material)
{
    mList[id] = material;
}
// static
void LLGLTFMaterialList::onAssetLoadComplete(LLVFS *vfs, const LLUUID &asset_id, LLAssetType::EType asset_type, void *user_data, S32 status, LLExtStat ext_status)
{
    AssetLoadUserData* asset_data = (AssetLoadUserData*)user_data;

    if (status != LL_ERR_NOERR)
    {
        LL_WARNS("GLTF") << "Error getting material asset data: " << LLAssetStorage::getErrorString(status) << " (" << status << ")" << LL_ENDL;
        asset_data->mMaterial->materialComplete();
        delete asset_data;
    }
    else
    {

        
        bool decoded=true;
       
        
        LLVFile file(vfs, asset_id, asset_type);
        auto size = file.getSize();
            
        S32 file_length = file.getSize();

        std::string buffer(file_length + 1, '\0');
        
        file.read((U8*)buffer.data(), file_length);
            
        LLSD asset;

        // read file into buffer
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

                        std::string warn_msg, error_msg;

                        tinygltf::TinyGLTF gltf;

                        if (!gltf.LoadASCIIFromString(&asset_data->mModelIn, &error_msg, &warn_msg, data.c_str(), data.length(), ""))
                        {
                            LL_WARNS("GLTF") << "Failed to decode material asset: "
                                << LL_NEWLINE
                                << warn_msg
                                << LL_NEWLINE
                                << error_msg
                                << LL_ENDL;
                            decoded=false;
                        }
                        decoded = true;
                    }
                }
            }
        }
        else
        {
            LL_WARNS("GLTF") << "Failed to deserialize material LLSD" << LL_ENDL;
        }
            
        
           

        if (decoded)
        {
            asset_data->mMaterial->setFromModel(asset_data->mModelIn, 0/*only one index*/);
        }
        else
        {
            LL_DEBUGS("GLTF") << "Failed to get material " << asset_id << LL_ENDL;
        }

        asset_data->mMaterial->materialComplete();

        delete asset_data;
        
    }
}
LLFetchedGLTFMaterial* LLGLTFMaterialList::getMaterial(const LLUUID& id)
{
    uuid_mat_map_t::iterator iter = mList.find(id);
    if (iter == mList.end())
    {
        LLFetchedGLTFMaterial* mat = new LLFetchedGLTFMaterial();
        mList[id] = mat;

        if (!mat->mFetching)
        {
            mat->materialBegin();

            AssetLoadUserData *user_data = new AssetLoadUserData();
            user_data->mMaterial = mat;
            gAssetStorage->getAssetData(id, LLAssetType::AT_MATERIAL, onAssetLoadComplete, (void*)user_data);
        }
        
        return mat;
    }

    return iter->second;
}