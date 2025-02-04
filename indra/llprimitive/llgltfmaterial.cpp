#include "llgltfmaterial.h"
// NOTE -- this should be the one and only place tiny_gltf.h is included
#include "tinygltf/tiny_gltf.h"
const char* const LLGLTFMaterial::ASSET_VERSION = "1.1";
const char* const LLGLTFMaterial::ASSET_TYPE = "GLTF 2.0";
const std::array<std::string, 2> LLGLTFMaterial::ACCEPTED_ASSET_VERSIONS = { "1.0", "1.1" };

const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM = "KHR_texture_transform";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_SCALE = "scale";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_OFFSET = "offset";
const char* const LLGLTFMaterial::GLTF_FILE_EXTENSION_TRANSFORM_ROTATION = "rotation";

// special UUID that indicates a null UUID in override data
const LLUUID LLGLTFMaterial::GLTF_OVERRIDE_NULL_UUID = LLUUID("ffffffff-ffff-ffff-ffff-ffffffffffff");
LLGLTFMaterial::LLGLTFMaterial()
{
    // IMPORTANT: since we use the hash of the member variables memory block of
    // this class to detect changes, we must ensure that all its padding bytes
    // have been zeroed out. But of course, we must leave the LLRefCount member
    // variable untouched (and skip it when hashing), and we cannot either
    // touch the local texture overrides map (else we destroy pointers, and
    // sundry private data, which would lead to a crash when using that map).
    // The variable members have therefore been arranged so that anything,
    // starting at mLocalTexDataDigest and up to the end of the members, can be
    // safely zeroed. HB
    const size_t offset = intptr_t(&mLocalTexDataDigest) - intptr_t(this);
    memset((void*)((const char*)this + offset), 0, sizeof(*this) - offset);

    // Now that we zeroed out our member variables, we can set the ones that
    // should not be zero to their default value. HB
    mBaseColor.set(1.f, 1.f, 1.f, 1.f);
    mMetallicFactor = mRoughnessFactor = 1.f;
    mAlphaCutoff = 0.5f;
    for (U32 i = 0; i < GLTF_TEXTURE_INFO_COUNT; ++i)
    {
        mTextureTransform[i].mScale.set(1.f, 1.f);
#if 0
        mTextureTransform[i].mOffset.clear();
        mTextureTransform[i].mRotation = 0.f;
#endif
    }
#if 0
    mLocalTexDataDigest = 0;
    mAlphaMode = ALPHA_MODE_OPAQUE;    // This is 0
    mOverrideDoubleSided = mOverrideAlphaMode = false;
#endif
}
void LLGLTFMaterial::TextureTransform::getPacked(Pack& packed) const
{
    packed[0] = mScale.mV[VX];
    packed[1] = mScale.mV[VY];
    packed[2] = mRotation;
    packed[4] = mOffset.mV[VX];
    packed[5] = mOffset.mV[VY];
    // Not used but nonetheless zeroed for proper hashing. HB
    packed[3] = packed[6] = packed[7] = 0.f;
}

void LLGLTFMaterial::TextureTransform::getPackedTight(PackTight& packed) const
{
    packed[0] = mScale.mV[VX];
    packed[1] = mScale.mV[VY];
    packed[2] = mRotation;
    packed[3] = mOffset.mV[VX];
    packed[4] = mOffset.mV[VY];
}

bool LLGLTFMaterial::TextureTransform::operator==(const TextureTransform& other) const
{
    return mOffset == other.mOffset && mScale == other.mScale && mRotation == other.mRotation;
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

const char* LLGLTFMaterial::getAlphaMode() const
{
    switch (mAlphaMode)
    {
    case ALPHA_MODE_MASK: return "MASK";
    case ALPHA_MODE_BLEND: return "BLEND";
    default: return "OPAQUE";
    }
}

void LLGLTFMaterial::setAlphaMode(S32 mode, bool for_override)
{
    mAlphaMode = (AlphaMode) llclamp(mode, (S32) ALPHA_MODE_OPAQUE, (S32) ALPHA_MODE_MASK);
    mOverrideAlphaMode = for_override && mAlphaMode == getDefaultAlphaMode();
}


// Default value accessors (NOTE: these MUST match the GLTF specification)

// Make a static default material for accessors
const LLGLTFMaterial LLGLTFMaterial::sDefault;

F32 LLGLTFMaterial::getDefaultAlphaCutoff()
{
    return sDefault.mAlphaCutoff;
}

S32 LLGLTFMaterial::getDefaultAlphaMode()
{
    return (S32) sDefault.mAlphaMode;
}

F32 LLGLTFMaterial::getDefaultMetallicFactor()
{
    return sDefault.mMetallicFactor;
}

F32 LLGLTFMaterial::getDefaultRoughnessFactor()
{
    return sDefault.mRoughnessFactor;
}

LLColor4 LLGLTFMaterial::getDefaultBaseColor()
{
    return sDefault.mBaseColor;
}

LLColor3 LLGLTFMaterial::getDefaultEmissiveColor()
{
    return sDefault.mEmissiveColor;
}

bool LLGLTFMaterial::getDefaultDoubleSided()
{
    return sDefault.mDoubleSided;
}

LLVector2 LLGLTFMaterial::getDefaultTextureOffset()
{
    return sDefault.mTextureTransform[0].mOffset;
}

LLVector2 LLGLTFMaterial::getDefaultTextureScale()
{
    return sDefault.mTextureTransform[0].mScale;
}

F32 LLGLTFMaterial::getDefaultTextureRotation()
{
    return sDefault.mTextureTransform[0].mRotation;
}
// static
template<typename T>
void LLGLTFMaterial::allocateTextureImage(tinygltf::Model& model, T& texture_info, const std::string& uri)
{
    const S32 image_idx = static_cast<S32>(model.images.size());
    model.images.emplace_back();
    model.images[image_idx].uri = uri;

    // The texture, not to be confused with the texture info
    const S32 texture_idx = static_cast<S32>(model.textures.size());
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