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

    // default material for reference
    static const LLGLTFMaterial sDefault;

    static const char* const ASSET_VERSION;
    static const char* const ASSET_TYPE;
    // Max allowed size of a GLTF material asset or override, when serialized
    // as a minified JSON string
    static constexpr size_t MAX_ASSET_LENGTH = 2048;
    static const std::array<std::string, 2> ACCEPTED_ASSET_VERSIONS;
    static bool isAcceptedVersion(const std::string& version) { return std::find(ACCEPTED_ASSET_VERSIONS.cbegin(), ACCEPTED_ASSET_VERSIONS.cend(), version) != ACCEPTED_ASSET_VERSIONS.cend(); }
};