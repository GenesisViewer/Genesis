/**
 * @file llgltfmaterial.h
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

#pragma once

#include "llrefcount.h"
#include "llmemory.h"
#include "v4color.h"
#include "v3color.h"
#include "v2math.h"
#include "lluuid.h"
#include "hbxxh.h"

#include <string>
#include <map>

namespace tinygltf
{
    class Model;
    struct TextureInfo;
    class Value;
}
class LLTextureEntry;

class LLGLTFMaterial : public LLRefCount
{
public:
struct TextureTransform
    {
        LLVector2 mOffset = { 0.f, 0.f };
        LLVector2 mScale = { 1.f, 1.f };
        F32 mRotation = 0.f;

        void getPacked(F32 (&packed)[8]) const;

        bool operator==(const TextureTransform& other) const;
        bool operator!=(const TextureTransform& other) const { return !(*this == other); }
    };
    enum AlphaMode
    {
        ALPHA_MODE_OPAQUE = 0,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_MASK
    };
    enum TextureInfo : U32
    {
        GLTF_TEXTURE_INFO_BASE_COLOR,
        GLTF_TEXTURE_INFO_NORMAL,
        GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS,
        // *NOTE: GLTF_TEXTURE_INFO_OCCLUSION is currently ignored, in favor of
        // the values specified with GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS.
        // Currently, only ORM materials are supported (materials which define
        // occlusion, roughness, and metallic in the same texture).
        // -Cosmic,2023-01-26
        GLTF_TEXTURE_INFO_OCCLUSION = GLTF_TEXTURE_INFO_METALLIC_ROUGHNESS,
        GLTF_TEXTURE_INFO_EMISSIVE,

        GLTF_TEXTURE_INFO_COUNT
    };

    static const char* const GLTF_FILE_EXTENSION_TRANSFORM;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_SCALE;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_OFFSET;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_ROTATION;
    static const LLUUID GLTF_OVERRIDE_NULL_UUID;

    std::array<LLUUID, GLTF_TEXTURE_INFO_COUNT> mTextureId;
    std::array<TextureTransform, GLTF_TEXTURE_INFO_COUNT> mTextureTransform;
    // default material for reference
    static const LLGLTFMaterial sDefault;

    static const char* const ASSET_VERSION;
    static const char* const ASSET_TYPE;
    // Max allowed size of a GLTF material asset or override, when serialized
    // as a minified JSON string
    static constexpr size_t MAX_ASSET_LENGTH = 2048;
    static const std::array<std::string, 2> ACCEPTED_ASSET_VERSIONS;
    static bool isAcceptedVersion(const std::string& version) { return std::find(ACCEPTED_ASSET_VERSIONS.cbegin(), ACCEPTED_ASSET_VERSIONS.cend(), version) != ACCEPTED_ASSET_VERSIONS.cend(); }

    // NOTE: initialize values to defaults according to the GLTF spec
    // NOTE: these values should be in linear color space
    LLColor4 mBaseColor = LLColor4::white;
    LLColor3 mEmissiveColor = LLColor3::black;

    F32 mMetallicFactor = 1.f;
    F32 mRoughnessFactor = 1.f;
    F32 mAlphaCutoff = 0.5f;

    bool mDoubleSided = false;
    AlphaMode mAlphaMode = ALPHA_MODE_OPAQUE;

    // override specific flags for state that can't use off-by-epsilon or UUID hack
    bool mOverrideDoubleSided = false;
    bool mOverrideAlphaMode = false;
    // set mAlphaMode from string.
    // Anything otherthan "MASK" or "BLEND" sets mAlphaMode to ALPHA_MODE_OPAQUE
    void setAlphaMode(const std::string& mode, bool for_override = false);
    void setAlphaMode(S32 mode, bool for_override = false);
    static S32 getDefaultAlphaMode();
    const char* getAlphaMode() const;
    static bool getDefaultDoubleSided();
    static LLColor3 getDefaultEmissiveColor();
    // get the contents of this LLGLTFMaterial as a json string
    std::string asJSON(bool prettyprint = false) const;

    // write to given tinygltf::Model
    void writeToModel(tinygltf::Model& model, S32 mat_index) const;
    template<typename T>
    void writeToTexture(tinygltf::Model& model, T& texture_info, TextureInfo texture_info_id, bool force_write = false) const;
    template<typename T>
    static void writeToTexture(tinygltf::Model& model, T& texture_info, const LLUUID& texture_id, const TextureTransform& transform, bool force_write = false);
    template<typename T>
    static void allocateTextureImage(tinygltf::Model& model, T& texture_info, const std::string& uri);
};