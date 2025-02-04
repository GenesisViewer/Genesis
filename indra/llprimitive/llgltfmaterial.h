#pragma once
#include "llrefcount.h"
#include "v4color.h"
#include "v3color.h"
#include "v2math.h"
#include "lluuid.h"
#include <array>
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

    // default material for reference
    static const LLGLTFMaterial sDefault;

    static const char* const ASSET_VERSION;
    static const char* const ASSET_TYPE;
    // Max allowed size of a GLTF material asset or override, when serialized
    // as a minified JSON string
    static constexpr size_t MAX_ASSET_LENGTH = 2048;
    static const std::array<std::string, 2> ACCEPTED_ASSET_VERSIONS;
    static bool isAcceptedVersion(const std::string& version) { return std::find(ACCEPTED_ASSET_VERSIONS.cbegin(), ACCEPTED_ASSET_VERSIONS.cend(), version) != ACCEPTED_ASSET_VERSIONS.cend(); } 

    LLGLTFMaterial();


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
    struct TextureTransform
    {
        LLVector2 mOffset = { 0.f, 0.f };
        LLVector2 mScale = { 1.f, 1.f };
        F32 mRotation = 0.f;

        static const size_t PACK_SIZE = 8;
        static const size_t PACK_TIGHT_SIZE = 5;
        using Pack = F32[PACK_SIZE];
        using PackTight = F32[PACK_TIGHT_SIZE];
        void getPacked(Pack& packed) const;
        void getPackedTight(PackTight& packed) const;

        bool operator==(const TextureTransform& other) const;
        bool operator!=(const TextureTransform& other) const { return !(*this == other); }
    };
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_SCALE;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_OFFSET;
    static const char* const GLTF_FILE_EXTENSION_TRANSFORM_ROTATION;
    static const LLUUID GLTF_OVERRIDE_NULL_UUID;
    // For local materials, they have to keep track of where
    // they are assigned to for full updates
    virtual void addTextureEntry(LLTextureEntry* te) {};
    virtual void removeTextureEntry(LLTextureEntry* te) {};   

    void setAlphaMode(S32 mode, bool for_override = false);

    // Default value accessors
    static F32 getDefaultAlphaCutoff();
    static S32 getDefaultAlphaMode();
    static F32 getDefaultMetallicFactor();
    static F32 getDefaultRoughnessFactor();
    static LLColor4 getDefaultBaseColor();
    static LLColor3 getDefaultEmissiveColor();
    static bool getDefaultDoubleSided();
    static LLVector2 getDefaultTextureOffset();
    static LLVector2 getDefaultTextureScale();
    static F32 getDefaultTextureRotation();

    // get the contents of this LLGLTFMaterial as a json string
    std::string asJSON(bool prettyprint = false) const;
    // write to given tinygltf::Model
    void writeToModel(tinygltf::Model& model, S32 mat_index) const;
    template<typename T>
    static void allocateTextureImage(tinygltf::Model& model, T& texture_info, const std::string& uri);
    template<typename T>
    void writeToTexture(tinygltf::Model& model, T& texture_info, TextureInfo texture_info_id, bool force_write = false) const;
    template<typename T>
    static void writeToTexture(tinygltf::Model& model, T& texture_info, const LLUUID& texture_id, const TextureTransform& transform, bool force_write = false);
    // set mAlphaMode from string.
    // Anything otherthan "MASK" or "BLEND" sets mAlphaMode to ALPHA_MODE_OPAQUE
    void setAlphaMode(const std::string& mode, bool for_override = false);

    const char* getAlphaMode() const;
public:
    // *TODO: If/when we implement additional GLTF extensions, they may not be
    // compatible with our GLTF terrain implementation. We may want to disallow
    // materials with some features from being set on terrain, if their
    // implementation on terrain is not compliant with the spec:
    //     - KHR_materials_transmission: Probably OK?
    //     - KHR_materials_ior: Probably OK?
    //     - KHR_materials_volume: Likely incompatible, as our terrain
    //       heightmaps cannot currently be described as finite enclosed
    //       volumes.
    // See also LLPanelRegionTerrainInfo::validateMaterials
    // These fields are local to viewer and are a part of local bitmap support
    typedef std::map<LLUUID, LLUUID> local_tex_map_t;
    local_tex_map_t mTrackingIdToLocalTexture;

    // Used to store a digest of mTrackingIdToLocalTexture when the latter is
    // not empty, or zero otherwise. HB
    U64 mLocalTexDataDigest;

    std::array<LLUUID, GLTF_TEXTURE_INFO_COUNT> mTextureId;
    std::array<TextureTransform, GLTF_TEXTURE_INFO_COUNT> mTextureTransform;

    // NOTE: initialize values to defaults according to the GLTF spec
    // NOTE: these values should be in linear color space
    LLColor4 mBaseColor;
    LLColor3 mEmissiveColor;

    F32 mMetallicFactor;
    F32 mRoughnessFactor;
    F32 mAlphaCutoff;

    AlphaMode mAlphaMode;

    bool mDoubleSided = false;

    // Override specific flags for state that can't use off-by-epsilon or UUID
    // hack
    bool mOverrideDoubleSided = false;
    bool mOverrideAlphaMode = false;

};