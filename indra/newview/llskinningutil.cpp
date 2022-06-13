/** 
* @file llskinningutil.cpp
* @brief  Functions for mesh object skinning
* @author vir@lindenlab.com
*
* $LicenseInfo:firstyear=2015&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2015, Linden Research, Inc.
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

#include "llskinningutil.h"
#include "llvoavatar.h"
#include "llviewercontrol.h"
#include "llmeshrepository.h"

// static
void LLSkinningUtil::initClass()
{
}

U32 LLSkinningUtil::getMaxJointCount()
{
    U32 result = LL_MAX_JOINTS_PER_MESH_OBJECT;
	return result;
}

U32 LLSkinningUtil::getMeshJointCount(const LLMeshSkinInfo *skin)
{
	return llmin((U32)getMaxJointCount(), (U32)skin->mJointNames.size());
}

void LLSkinningUtil::scrubInvalidJoints(LLVOAvatar *avatar, LLMeshSkinInfo* skin)
{
    if (skin->mInvalidJointsScrubbed)
    {
        return;
    }
    for (U32 j = 0; j < skin->mJointNames.size(); ++j)
    {
        // Fix invalid names to "mPelvis". Currently meshes with
        // invalid names will be blocked on upload, so this is just
        // needed for handling of any legacy bad data.
        if (!avatar->getJoint(skin->mJointNames[j]))
        {
            LL_DEBUGS("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint " << skin->mJointNames[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " mesh rigged to invalid joint" << skin->mJointNames[j] << LL_ENDL;
            skin->mJointNames[j] = "mPelvis";
            skin->mJointNumsInitialized = false; // force update after names change.
        }
    }
    skin->mInvalidJointsScrubbed = true;
}

// static
void LLSkinningUtil::initSkinningMatrixPalette(
    LLMatrix4a* mat,
    S32 count, 
    const LLMeshSkinInfo* skin,
    LLVOAvatar *avatar,
    bool relative_to_avatar)
{
	LLVector4a pos = LLVector4a::getZero();
	if (relative_to_avatar)
	{
		pos.load3(avatar->getPosition().mV);
		pos.mul(-1.f);
	}

    initJointNums(const_cast<LLMeshSkinInfo*>(skin), avatar);
    for (U32 j = 0; j < (U32)count; ++j)
    {
        LLJoint *joint = avatar->getJoint(skin->mJointNums[j]);
        if (joint)
        {
			LLMatrix4a bind;
			bind.loadu((F32*)skin->mInvBindMatrix[j].mMatrix);
			LLMatrix4a world = joint->getWorldMatrix();
			world.getRow<3>().add(pos); // Append pos into world matrix.
			mat[j].setMul(world, bind);	
        }
        else
        {
            mat[j].loadu((F32*)skin->mInvBindMatrix[j].mMatrix);
            // This  shouldn't  happen   -  in  mesh  upload,  skinned
            // rendering  should  be disabled  unless  all joints  are
            // valid.  In other  cases of  skinned  rendering, invalid
            // joints should already have  been removed during scrubInvalidJoints().
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " rigged to invalid joint name " << skin->mJointNames[j] 
                                    << " num " << skin->mJointNums[j] << LL_ENDL;
            LL_WARNS_ONCE("Avatar") << avatar->getFullname() 
                                    << " avatar build state: isBuilt() " << avatar->isBuilt() 
                                    << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
        }
    }
}


void LLSkinningUtil::checkSkinWeights(const LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
#ifndef LL_RELEASE_FOR_DOWNLOAD
	const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        const F32 *w = weights[j].getF32ptr();
            
        F32 wsum = 0.0;
        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            llassert(i>=0);
            llassert(i<max_joints);
            wsum += w[k]-i;
        }
        llassert(wsum > 0.0f);
    }
#endif
}

void LLSkinningUtil::scrubSkinWeights(LLVector4a* weights, U32 num_vertices, const LLMeshSkinInfo* skin)
{
    const S32 max_joints = skin->mJointNames.size();
    for (U32 j=0; j<num_vertices; j++)
    {
        F32 *w = weights[j].getF32ptr();

        for (U32 k=0; k<4; ++k)
        {
            S32 i = llfloor(w[k]);
            F32 f = w[k]-i;
            i = llclamp(i,0,max_joints-1);
            w[k] = i + f;
        }
    }
	checkSkinWeights(weights, num_vertices, skin);
}

void LLSkinningUtil::getPerVertexSkinMatrix(
    const F32* weights,
    LLMatrix4a* mat,
    bool handle_bad_scale,
    LLMatrix4a& final_mat,
    U32 max_joints)
{
    bool valid_weights = true;
	final_mat.clear();

	S32 idx[4];

	LLVector4 wght;

	F32 scale = 0.f;
	for (U32 k = 0; k < 4; k++)
	{
		F32 w = weights[k];

		idx[k] = (S32) w;

		wght[k] = w - idx[k];
		scale += wght[k];
	}
	if (handle_bad_scale && scale <= 0.f)
	{
		wght = LLVector4(F32_MAX,F32_MAX,F32_MAX,F32_MAX);
		valid_weights = false;
	}
	else
	{
		// This is enforced  in unpackVolumeFaces()
		llassert(scale>0.f);

		wght *= 1.f/scale;
	}

	for (U32 k = 0; k < 4; k++)
	{
		F32 w = wght[k];

		LLMatrix4a src;
		src.setMul(mat[idx[k]], w);

		final_mat.add(src);
	}
	// SL-366 - with weight validation/cleanup code, it should no longer be
	// possible to hit the bad scale case.
	llassert(valid_weights);
}

void LLSkinningUtil::initJointNums(LLMeshSkinInfo* skin, LLVOAvatar *avatar)
{
    if (!skin->mJointNumsInitialized)
    {
        for (U32 j = 0; j < skin->mJointNames.size(); ++j)
        {
            LLJoint *joint = NULL;
            if (skin->mJointNums[j] == -1)
            {
                joint = avatar->getJoint(skin->mJointNames[j]);
                if (joint)
                {
                    skin->mJointNums[j] = joint->getJointNum();
                    if (skin->mJointNums[j] < 0)
                    {
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " joint has unusual number " << skin->mJointNames[j] << ": " << skin->mJointNums[j] << LL_ENDL;
                        LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
                    }
                }
                else
                {
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " unable to find joint " << skin->mJointNames[j] << LL_ENDL;
                    LL_WARNS_ONCE("Avatar") << avatar->getFullname() << " avatar build state: isBuilt() " << avatar->isBuilt() << " mInitFlags " << avatar->mInitFlags << LL_ENDL;
#if 0
                    dump_avatar_and_skin_state("initJointNums joint not found", avatar, skin);
#endif
                }
            }
        }
        skin->mJointNumsInitialized = true;
    }
}

static LLTrace::BlockTimerStatHandle FTM_FACE_RIGGING_INFO("Face Rigging Info");

// How many copies of the same code do we need, LL?
void LLSkinningUtil::updateRiggingInfo(const LLMeshSkinInfo* skin, LLVOAvatar *avatar, LLVolumeFace& vol_face)
{
    LL_RECORD_BLOCK_TIME(FTM_FACE_RIGGING_INFO);

    if (vol_face.mJointRiggingInfoTab.needsUpdate())
    {
        S32 num_verts = vol_face.mNumVertices;
        if (num_verts>0 && vol_face.mWeights && (skin->mJointNames.size()>0))
        {
            initJointNums(const_cast<LLMeshSkinInfo*>(skin), avatar);
            if (vol_face.mJointRiggingInfoTab.size()==0)
            {
                //std::set<S32> active_joints;
                //S32 active_verts = 0;
                vol_face.mJointRiggingInfoTab.resize(LL_CHARACTER_MAX_ANIMATED_JOINTS);
                LLJointRiggingInfoTab &rig_info_tab = vol_face.mJointRiggingInfoTab;
                LLMatrix4a bind_shape;
                bind_shape.loadu(skin->mBindShapeMatrix);
                LLMatrix4a matrixPalette[LL_CHARACTER_MAX_ANIMATED_JOINTS];
                for (U32 i = 0; i < llmin(skin->mInvBindMatrix.size(), (size_t)LL_CHARACTER_MAX_ANIMATED_JOINTS); ++i)
                {
                    LLMatrix4a inverse_bind;
                    inverse_bind.loadu(skin->mInvBindMatrix[i]);
                    matrixPalette[i].setMul(inverse_bind, bind_shape);
                }
                for (S32 i=0; i<vol_face.mNumVertices; i++)
                {
                    LLVector4a& pos = vol_face.mPositions[i];
                    F32 *weights = vol_face.mWeights[i].getF32ptr();
                    LLVector4 wght;
                    S32 idx[4];
                    F32 scale = 0.0f;
                    // FIXME unpacking of weights should be pulled into a common function and optimized if possible.
                    for (U32 k = 0; k < 4; k++)
                    {
                        F32 w = weights[k];
                        idx[k] = llclamp((S32) floorf(w), (S32)0, (S32)LL_CHARACTER_MAX_ANIMATED_JOINTS-1);
                        wght[k] = w - idx[k];
                        scale += wght[k];
                    }
                    if (scale > 0.0f)
                    {
                        for (U32 k=0; k<4; ++k)
                        {
                            wght[k] /= scale;
                        }
                    }
                    for (U32 k=0; k<4; ++k)
                    {
                        S32 joint_index = idx[k];
                        if (wght[k] > 0.0f)
                        {
                            S32 joint_num = skin->mJointNums[joint_index];
                            if (joint_num >= 0 && joint_num < LL_CHARACTER_MAX_ANIMATED_JOINTS)
                            {
                                rig_info_tab[joint_num].setIsRiggedTo(true);
                                LLVector4a pos_joint_space;
                                matrixPalette[joint_index].affineTransform(pos, pos_joint_space);
                                pos_joint_space.mul(wght[k]);
                                LLVector4a *extents = rig_info_tab[joint_num].getRiggedExtents();
                                update_min_max(extents[0], extents[1], pos_joint_space);
                            }
                        }
                    }
                }
                //LL_DEBUGS("RigSpammish") << "built rigging info for vf " << &vol_face 
                //                         << " num_verts " << vol_face.mNumVertices
                //                         << " active joints " << active_joints.size()
                //                         << " active verts " << active_verts
                //                         << LL_ENDL; 
                vol_face.mJointRiggingInfoTab.setNeedsUpdate(false);
            }
        }
        if (vol_face.mJointRiggingInfoTab.size()!=0)
        {
            LL_DEBUGS("RigSpammish") << "we have rigging info for vf " << &vol_face 
                                     << " num_verts " << vol_face.mNumVertices << LL_ENDL; 
        }
        else
        {
            LL_DEBUGS("RigSpammish") << "no rigging info for vf " << &vol_face 
                                     << " num_verts " << vol_face.mNumVertices << LL_ENDL; 
        }

    }
}

// This is used for extracting rotation from a bind shape matrix that
// already has scales baked in
LLQuaternion LLSkinningUtil::getUnscaledQuaternion(const LLMatrix4& mat4)
{
    LLMatrix3 bind_mat = mat4.getMat3();
    for (auto i = 0; i < 3; i++)
    {
        F32 len = 0.0f;
        for (auto j = 0; j < 3; j++)
        {
            len += bind_mat.mMatrix[i][j] * bind_mat.mMatrix[i][j];
        }
        if (len > 0.0f)
        {
            len = sqrt(len);
            for (auto j = 0; j < 3; j++)
            {
                bind_mat.mMatrix[i][j] /= len;
            }
        }
    }
    bind_mat.invert();
    LLQuaternion bind_rot = bind_mat.quaternion();
    bind_rot.normalize();
    return bind_rot;
}
