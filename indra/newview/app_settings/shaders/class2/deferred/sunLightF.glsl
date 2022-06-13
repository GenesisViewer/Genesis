/** 
 * @file sunLightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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



#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

//class 2, shadows, no SSAO

uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform sampler2DShadow shadowMap0;
uniform sampler2DShadow shadowMap1;
uniform sampler2DShadow shadowMap2;
uniform sampler2DShadow shadowMap3;
uniform sampler2DShadow shadowMap4;
uniform sampler2DShadow shadowMap5;
//uniform sampler2D noiseMap;	//Random dither.

// Inputs
uniform mat4 shadow_matrix[6];
uniform vec4 shadow_clip;
uniform float ssao_radius;
uniform float ssao_max_radius;
uniform float ssao_factor;
uniform float ssao_factor_inv;

VARYING vec2 vary_fragcoord;
uniform vec2 kern_scale;

uniform mat4 inv_proj;
uniform vec2 proj_shadow_res;
uniform vec3 sun_dir;

uniform vec2 shadow_res;
uniform float shadow_bias;
uniform float shadow_offset;

uniform float spot_shadow_bias;
uniform float spot_shadow_offset;

vec3 decode_normal(vec2 enc);
vec4 getPosition(vec2 pos_screen);

float calcShadow( sampler2DShadow shadowMap, vec4 stc, vec2 res, vec2 pos_screen )
{
	//stc.x += (((texture2D(noiseMap, pos_screen/128.0).x)-.5)/shadow_res.x);	//Random dither.

	vec2 off = vec2(1,1.5)/res;
	stc.x = floor(stc.x*res.x + fract(pos_screen.y*(1.0/kern_scale.y)*0.5))*off.x;


	float shadow = shadow2D(shadowMap, stc.xyz).x; // cs
	shadow += shadow2D(shadowMap, stc.xyz+vec3(off.x*2.0, off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(off.x, -off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-off.x*2.0, off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-off.x, -off.y, 0.0)).x;

	return shadow;
}

float pcfShadow(sampler2DShadow shadowMap, vec4 stc, float scl, vec2 pos_screen)
{
	stc.xyz /= stc.w;
	stc.z += shadow_bias;

	return calcShadow(shadowMap, stc, shadow_res, pos_screen)*0.2;
}

float pcfSpotShadow(sampler2DShadow shadowMap, vec4 stc, float scl, vec2 pos_screen)
{
	stc.xyz /= stc.w;
	stc.z += spot_shadow_bias*scl;

	return calcShadow(shadowMap, stc, proj_shadow_res, pos_screen)*0.2;
}

void main() 
{
	vec2 pos_screen = vary_fragcoord.xy;
	
	//try doing an unproject here
	
	vec4 pos = getPosition(pos_screen);
	
	vec3 norm = texture2D(normalMap, pos_screen).xyz;
	norm = decode_normal(norm.xy); // unpack norm
		
	/*if (pos.z == 0.0) // do nothing for sky *FIX: REMOVE THIS IF/WHEN THE POSITION MAP IS BEING USED AS A STENCIL
	{
		frag_color = vec4(0.0); // doesn't matter
		return;
	}*/
	
	float shadow = 0.0;
	float dp_directional_light = max(0.0, dot(norm, sun_dir.xyz));

	vec3 shadow_pos = pos.xyz;
	vec3 offset = sun_dir.xyz * (1.0-dp_directional_light);
	
	vec4 spos = vec4(shadow_pos+offset*shadow_offset, 1.0);
	
	if (spos.z > -shadow_clip.w)
	{	
		if (dp_directional_light == 0.0)
		{
			// if we know this point is facing away from the sun then we know it's in shadow without having to do a squirrelly shadow-map lookup
			shadow = 0.0;
		}
		else
		{
			vec4 lpos;
			
			vec4 near_split = shadow_clip*-0.75;
			vec4 far_split = shadow_clip*-1.25;
			vec4 transition_domain = near_split-far_split;
			float weight = 0.0;

			if (spos.z < near_split.z)
			{
				lpos = shadow_matrix[3]*spos;
				
				float w = 1.0;
				w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;
				shadow += pcfShadow(shadowMap3, lpos, 0.25, pos_screen)*w;
				weight += w;
				shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
			}

			if (spos.z < near_split.y && spos.z > far_split.z)
			{
				lpos = shadow_matrix[2]*spos;
				
				float w = 1.0;
				w -= max(spos.z-far_split.y, 0.0)/transition_domain.y;
				w -= max(near_split.z-spos.z, 0.0)/transition_domain.z;
				shadow += pcfShadow(shadowMap2, lpos, 0.5, pos_screen)*w;
				weight += w;
			}

			if (spos.z < near_split.x && spos.z > far_split.y)
			{
				lpos = shadow_matrix[1]*spos;

				float w = 1.0;
				w -= max(spos.z-far_split.x, 0.0)/transition_domain.x;
				w -= max(near_split.y-spos.z, 0.0)/transition_domain.y;
				shadow += pcfShadow(shadowMap1, lpos, 0.75, pos_screen)*w;
				weight += w;
			}

			if (spos.z > far_split.x)
			{
				lpos = shadow_matrix[0]*spos;
				
				float w = 1.0;
				w -= max(near_split.x-spos.z, 0.0)/transition_domain.x;
				
				shadow += pcfShadow(shadowMap0, lpos, 1.0, pos_screen)*w;
				weight += w;
			}
		

			shadow /= weight;

			// take the most-shadowed value out of these two:
			//  * the blurred sun shadow in the light (shadow) map
			//  * an unblurred dot product between the sun and this norm
			// the goal is to err on the side of most-shadow to fill-in shadow holes and reduce artifacting
			shadow = min(shadow, dp_directional_light);
			
			//lpos.xy /= lpos.w*32.0;
			//if (fract(lpos.x) < 0.1 || fract(lpos.y) < 0.1)
			//{
			//	shadow = 0.0;
			//}
			
		}
	}
	else
	{
		// more distant than the shadow map covers
		shadow = 1.0;
	}
	
	frag_color[0] = shadow;
	frag_color[1] = 1.0;
	
	spos = vec4(shadow_pos+norm*spot_shadow_offset, 1.0);
	
	//spotlight shadow 1
	vec4 lpos = shadow_matrix[4]*spos;
	frag_color[2] = pcfSpotShadow(shadowMap4, lpos, 0.8, pos_screen); 
	
	//spotlight shadow 2
	lpos = shadow_matrix[5]*spos;
	frag_color[3] = pcfSpotShadow(shadowMap5, lpos, 0.8, pos_screen); 

	//frag_color.rgb = pos.xyz;
	//frag_color.b = shadow;
}
