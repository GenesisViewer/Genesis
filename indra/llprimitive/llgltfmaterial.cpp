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