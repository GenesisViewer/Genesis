/**
 * @file llgltfmaterial.cpp
 * @brief Material definition
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


#include "linden_common.h"

#include "llgltfmaterial.h"

#include "llsdserialize.h"

// NOTE -- this should be the one and only place tiny_gltf.h is included
#include "tinygltf/tiny_gltf.h"


const char* const LLGLTFMaterial::ASSET_VERSION = "1.1";
const char* const LLGLTFMaterial::ASSET_TYPE = "GLTF 2.0";
const std::array<std::string, 2> LLGLTFMaterial::ACCEPTED_ASSET_VERSIONS = { "1.0", "1.1" };

const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM = "KHR_texture_transform";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_SCALE = "scale";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_OFFSET = "offset";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_ROTATION = "rotation";

// Make a static default material for accessors
const LLGLTFMaterial LLGLTFMaterial::sDefault;

S32 LLGLTFMaterial::getDefaultAlphaMode()
{
    return (S32) sDefault.mAlphaMode;
}
void LLGLTFMaterial::setAlphaMode(const std::string& mode, bool for_override)
{
    S32 m = getDefaultAlphaMode();
    if (mode == "MASK")
    {
        m = ALPHA_MODE_MASK;
    }
    else if (mode == "BLEND")
    {
        m = ALPHA_MODE_BLEND;
    }

    setAlphaMode(m, for_override);
}
void LLGLTFMaterial::setAlphaMode(S32 mode, bool for_override)
{
    mAlphaMode = (AlphaMode) llclamp(mode, (S32) ALPHA_MODE_OPAQUE, (S32) ALPHA_MODE_MASK);
    mOverrideAlphaMode = for_override && mAlphaMode == getDefaultAlphaMode();
}

std::string LLGLTFMaterial::asJSON(bool prettyprint) const
{
    tinygltf::TinyGLTF gltf;

    tinygltf::Model model_out;

    std::ostringstream str;

    writeToModel(model_out, 0);

    // To ensure consistency in asset upload, this should be the only reference
    // to WriteGltfSceneToStream in the viewer.
    gltf.WriteGltfSceneToStream(&model_out, str, prettyprint, false);

    return str.str();
}
void LLGLTFMaterial::writeToModel(tinygltf::Model& model, S32 mat_index) const
{
    if (model.materials.size() < mat_index+1)
    {
        model.materials.resize(mat_index + 1);
    }

    tinygltf::Material& material_out = model.materials[mat_index];

    // set base color texture
    writeToTexture(model, material_out.pbrMetallicRoughness.baseColorTexture, GLTF_TEXTURE_INFO_BASE_COLOR);
    // set normal texture
    writeToTexture(model, material_out.normalTexture, GLTF_TEXTURE_INFO_NORMAL);
    // set metallic-roughness texture
    writeToTexture(model, material_out.pbrMetallicRoughness.metallicRoughnessTexture, GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS);
    // set emissive texture
    writeToTexture(model, material_out.emissiveTexture, GLTF_TEXTURE_INFO_EMISSIVE);
    // set occlusion texture
    // *NOTE: This is required for ORM materials for GLTF compliance.
    // See: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_occlusiontexture
    writeToTexture(model, material_out.occlusionTexture, GLTF_TEXTURE_INFO_OCCLUSION);


    material_out.alphaMode = getAlphaMode();
    material_out.alphaCutoff = mAlphaCutoff;

    mBaseColor.write(material_out.pbrMetallicRoughness.baseColorFactor);

    if (mEmissiveColor != LLGLTFMaterial::getDefaultEmissiveColor())
    {
        material_out.emissiveFactor.resize(3);
        mEmissiveColor.write(material_out.emissiveFactor);
    }

    material_out.pbrMetallicRoughness.metallicFactor = mMetallicFactor;
    material_out.pbrMetallicRoughness.roughnessFactor = mRoughnessFactor;

    material_out.doubleSided = mDoubleSided;

    // generate "extras" string
    tinygltf::Value::Object extras;
    bool write_extras = false;
    if (mOverrideAlphaMode && mAlphaMode == getDefaultAlphaMode())
    {
        extras["override_alpha_mode"] = tinygltf::Value(mOverrideAlphaMode);
        write_extras = true;
    }

    if (mOverrideDoubleSided && mDoubleSided == getDefaultDoubleSided())
    {
        extras["override_double_sided"] = tinygltf::Value(mOverrideDoubleSided);
        write_extras = true;
    }

    if (write_extras)
    {
        material_out.extras = tinygltf::Value(extras);
    }

    model.asset.version = "2.0";
}
const char* LLGLTFMaterial::getAlphaMode() const
{
    switch (mAlphaMode)
    {
    case ALPHA_MODE_MASK: return "MASK";
    case ALPHA_MODE_BLEND: return "BLEND";
    default: return "OPAQUE";
    }
}
bool LLGLTFMaterial::getDefaultDoubleSided()
{
    return sDefault.mDoubleSided;
}
LLColor3 LLGLTFMaterial::getDefaultEmissiveColor()
{
    return sDefault.mEmissiveColor;
}
// static
template<typename T>
void LLGLTFMaterial::allocateTextureImage(tinygltf::Model& model, T& texture_info, const std::string& uri)
{
    const S32 image_idx = model.images.size();
    model.images.emplace_back();
    model.images[image_idx].uri = uri;

    // The texture, not to be confused with the texture info
    const S32 texture_idx = model.textures.size();
    model.textures.emplace_back();
    tinygltf::Texture& texture = model.textures[texture_idx];
    texture.source = image_idx;

    texture_info.index = texture_idx;
}
// static
template<typename T>
void LLGLTFMaterial::writeToTexture(tinygltf::Model& model, T& texture_info, TextureInfo texture_info_id, bool force_write) const
{
    writeToTexture(model, texture_info, mTextureId[texture_info_id], mTextureTransform[texture_info_id], force_write);
}

// static
template<typename T>
void LLGLTFMaterial::writeToTexture(tinygltf::Model& model, T& texture_info, const LLUUID& texture_id, const TextureTransform& transform, bool force_write)
{
    const bool is_blank_transform = transform == sDefault.mTextureTransform[0];
    // Check if this material matches all the fallback values, and if so, then
    // skip including it to reduce material size
    if (!force_write && texture_id.isNull() && is_blank_transform)
    {
        return;
    }

    // tinygltf will discard this texture info if there is no valid texture,
    // causing potential loss of information for overrides, so ensure one is
    // defined. -Cosmic,2023-01-30
    allocateTextureImage(model, texture_info, texture_id.asString());

    if (!is_blank_transform)
    {
        tinygltf::Value::Object transform_map;
        transform_map[GLTF_FILE_EXTENSION_TRANSFORM_OFFSET] = tinygltf::Value(tinygltf::Value::Array({
            tinygltf::Value(transform.mOffset.mV[VX]),
            tinygltf::Value(transform.mOffset.mV[VY])
        }));
        transform_map[GLTF_FILE_EXTENSION_TRANSFORM_SCALE] = tinygltf::Value(tinygltf::Value::Array({
            tinygltf::Value(transform.mScale.mV[VX]),
            tinygltf::Value(transform.mScale.mV[VY])
        }));
        transform_map[GLTF_FILE_EXTENSION_TRANSFORM_ROTATION] = tinygltf::Value(transform.mRotation);
        texture_info.extensions[GLTF_FILE_EXTENSION_TRANSFORM] = tinygltf::Value(transform_map);
    }
}
bool LLGLTFMaterial::TextureTransform::operator==(const TextureTransform& other) const
{
    return mOffset == other.mOffset && mScale == other.mScale && mRotation == other.mRotation;
}