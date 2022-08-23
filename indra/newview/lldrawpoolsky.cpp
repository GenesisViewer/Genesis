/** 
 * @file lldrawpoolsky.cpp
 * @brief LLDrawPoolSky class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolsky.h"

#include "imageids.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewerregion.h"
#include "llvosky.h"
#include "llworld.h" // To get water height
#include "pipeline.h"
#include "llviewershadermgr.h"

LLDrawPoolSky::LLDrawPoolSky()
:	LLFacePool(POOL_SKY),
	
	mSkyTex(NULL),
	mShader(NULL)
{
}

LLDrawPool *LLDrawPoolSky::instancePool()
{
	return new LLDrawPoolSky();
}

void LLDrawPoolSky::prerender()
{
	//LL_INFOS() << " LLDrawPoolSky::prerender()" << LL_ENDL;
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_ENVIRONMENT); 
	gSky.mVOSkyp->updateGeometry(gSky.mVOSkyp->mDrawable);
	//LL_INFOS() << " LLDrawPoolSky::prerender() end" << LL_ENDL;
}

void LLDrawPoolSky::render(S32 pass)
{
	//LL_INFOS() << " LLDrawPoolSky::render()" << LL_ENDL;
	gGL.flush();

	if (mDrawFace.empty())
	{
		return;
	}
	//LL_INFOS() << " LLDrawPoolSky::render() 1" << LL_ENDL;
	// Don't draw the sky box if we can and are rendering the WL sky dome.
	
	if (gPipeline.canUseWindLightShaders())
	{
		//LL_INFOS() << " LLDrawPoolSky::render() 1.1" << LL_ENDL;
		return;
	}
	//LL_INFOS() << " LLDrawPoolSky::render() 2" << LL_ENDL;
	// use a shader only underwater
	if(mVertexShaderLevel > 0 && LLPipeline::sUnderWaterRender)
	{
		return;
		//mShader = &gObjectFullbrightWaterProgram;
		//mShader->bind();
	}
	//LL_INFOS() << " LLDrawPoolSky::render() 3" << LL_ENDL;
	if (LLGLSLShader::sNoFixedFunction)
	{ //just use the UI shader (generic single texture no lighting)
		gOneTextureNoColorProgram.bind();
	}
	else
	{
		// don't use shaders!
		if (gGLManager.mHasShaderObjects)
		{
			// Ironically, we must support shader objects to be
			// able to use this call.
			LLGLSLShader::bindNoShader();
		}
		mShader = NULL;
	}
	
	//LL_INFOS() << " LLDrawPoolSky::render() 4" << LL_ENDL;
	LLGLSPipelineSkyBox gls_skybox;

	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	LLGLSquashToFarClip far_clip(glh_get_current_projection());

	LLGLEnable<GL_FOG> fog_enable(mVertexShaderLevel < 1 && LLViewerCamera::getInstance()->cameraUnderWater());
	
	LLGLDisable<GL_CLIP_PLANE0> clip;
	//LL_INFOS() << " LLDrawPoolSky::render() 5" << LL_ENDL;
	gGL.pushMatrix();
	LLVector3 origin = LLViewerCamera::getInstance()->getOrigin();
	gGL.translatef(origin.mV[0], origin.mV[1], origin.mV[2]);

	S32 face_count = (S32)mDrawFace.size();

	LLVertexBuffer::unbind();
	gGL.diffuseColor4f(1,1,1,1);
	//LL_INFOS() << " LLDrawPoolSky::render() 6" << LL_ENDL;
	for (S32 i = 0; i < llmin(6, face_count); ++i)
	{
		renderSkyCubeFace(i);
	}
	//LL_INFOS() << " LLDrawPoolSky::render() 7" << LL_ENDL;
	gGL.popMatrix();
	//LL_INFOS() << " LLDrawPoolSky::render() end" << LL_ENDL;
}

void LLDrawPoolSky::renderSkyCubeFace(U8 side)
{
	LLFace &face = *mDrawFace[LLVOSky::FACE_SIDE0 + side];
	if (!face.getGeomCount())
	{
		return;
	}

	llassert(mSkyTex);
	mSkyTex[side].bindTexture(TRUE);
	
	face.renderIndexed();

	if (LLSkyTex::doInterpolate())
	{
		
		LLGLEnable<GL_BLEND> blend;
		mSkyTex[side].bindTexture(FALSE);
		gGL.diffuseColor4f(1, 1, 1, LLSkyTex::getInterpVal()); // lighting is disabled
		face.renderIndexed();
	}
}

void LLDrawPoolSky::endRenderPass( S32 pass )
{
}
