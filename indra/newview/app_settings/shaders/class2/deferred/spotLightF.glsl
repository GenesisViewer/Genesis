/** 
 * @file spotLightF.glsl
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

uniform sampler2D diffuseRect;
uniform sampler2D specularRect;
uniform sampler2D depthMap;
uniform sampler2D normalMap;
uniform samplerCube environmentMap;
uniform sampler2D noiseMap;
uniform sampler2D projectionMap;
uniform sampler2D lightFunc;

uniform mat4 proj_mat; //screen space to light space
uniform float proj_near; //near clip for projection
uniform vec3 proj_p; //plane projection is emitting from (in screen space)
uniform vec3 proj_n;
uniform float proj_focus; //distance from plane to begin blurring
uniform float proj_lod;  //(number of mips in proj map)
uniform float proj_range; //range between near clip and far clip plane of projection
uniform float proj_ambient_lod;
uniform float proj_ambiance;
uniform float near_clip;
uniform float far_clip;

uniform vec3 proj_origin; //origin of projection to be used for angular attenuation
uniform float sun_wash;

// Shadow
uniform sampler2D lightMap;
uniform int proj_shadow_idx;
uniform float shadow_fade;

uniform vec3 color;
uniform float falloff;
uniform float size;

VARYING vec3 trans_center;
VARYING vec4 vary_fragcoord;

uniform mat4 inv_proj;
uniform vec2 noise_scale;

vec3 decode_normal(vec2 enc);
vec4 getPosition(vec2 pos_screen);
vec4 srgb_to_linear(vec4 cs);

vec4 texture2DLodSpecular(sampler2D projectionMap, vec2 tc, float lod)
{
	vec4 ret = texture2DLod(projectionMap, tc, lod);
	
	ret = srgb_to_linear(ret);
	vec2 dist = vec2(0.5) - abs(tc-vec2(0.5));
	float det = min(lod/(proj_lod*0.5), 1.0);
	float d = min(dist.x, dist.y);
	d *= min(1, d * (proj_lod - lod));
	float edge = 0.25*det;
	ret *= clamp(d/edge, 0.0, 1.0);
	
	return ret;
}

vec4 texture2DLodDiffuse(sampler2D projectionMap, vec2 tc, float lod)
{
	vec4 ret = texture2DLod(projectionMap, tc, lod);
	ret = srgb_to_linear(ret);

	vec2 dist = vec2(0.5) - abs(tc-vec2(0.5));
	
	float det = min(lod/(proj_lod*0.5), 1.0);
	
	float d = min(dist.x, dist.y);
	
	float edge = 0.25*det;
		
	ret *= clamp(d/edge, 0.0, 1.0);
	
	return ret;
}

vec4 texture2DLodAmbient(sampler2D projectionMap, vec2 tc, float lod)
{
	vec4 ret = texture2DLod(projectionMap, tc, lod);
	ret = srgb_to_linear(ret);
	
	vec2 dist = tc-vec2(0.5);
	
	float d = dot(dist,dist);
		
	ret *= min(clamp((0.25-d)/0.25, 0.0, 1.0), 1.0);
	
	return ret;
}

void main() 
{
	vec4 frag = vary_fragcoord;
	frag.xyz /= frag.w;
	frag.xyz = frag.xyz*0.5+0.5;
	
	vec3 pos = getPosition(frag.xy).xyz;
	vec3 lv = trans_center.xyz-pos.xyz;
	float dist = length(lv);
	dist /= size;
	if (dist > 1.0)
	{
		discard;
	}

	float shadow = 1.0;
	
	if (proj_shadow_idx >= 0)
	{
		vec4 shd = texture2D(lightMap, frag.xy);
		float sh[2];
		sh[0] = shd.b;
		sh[1] = shd.a;
		shadow = min(sh[proj_shadow_idx]+shadow_fade, 1.0);
	}

	vec3 norm = texture2D(normalMap, frag.xy).xyz;

	float envIntensity = norm.z;

	norm = decode_normal(norm.xy);
	
	norm = normalize(norm);
	float l_dist = -dot(lv, proj_n);
	
	vec4 proj_tc = (proj_mat * vec4(pos.xyz, 1.0));
	if (proj_tc.z < 0.0)
	{
		discard;
	}
	
	proj_tc.xyz /= proj_tc.w;
	
	float fa = falloff+1.0;
	float dist_atten = min(1.0-(dist-1.0*(1.0-fa))/fa, 1.0);
	dist_atten *= dist_atten;
	dist_atten *= 2.0;

	if (dist_atten <= 0.0)
	{
		discard;
	}
	
	lv = proj_origin-pos.xyz;
	lv = normalize(lv);
	float da = dot(norm, lv);

	vec3 col = vec3(0,0,0);

	vec3 diff_tex = texture2D(diffuseRect, frag.xy).rgb;
	vec3 dlit = vec3(0, 0, 0);

	float noise = texture2D(noiseMap, frag.xy*noise_scale).b;
	if (proj_tc.z > 0.0 &&
		proj_tc.x < 1.0 &&
		proj_tc.y < 1.0 &&
		proj_tc.x > 0.0 &&
		proj_tc.y > 0.0)
	{
		float lit = 0.0;
		float amb_da = proj_ambiance;
		
		if (da > 0.0)
		{
			lit = da * dist_atten * noise;

			float diff = clamp((l_dist-proj_focus)/proj_range, 0.0, 1.0);
			float lod = diff * proj_lod;

			vec4 plcol = texture2DLodDiffuse(projectionMap, proj_tc.xy, lod);

			dlit = color.rgb * plcol.rgb * plcol.a;

			col = dlit*lit*diff_tex*shadow;
			amb_da += (da*0.5)*(1.0-shadow)*proj_ambiance;
		}

		//float diff = clamp((proj_range-proj_focus)/proj_range, 0.0, 1.0);
		vec4 amb_plcol = texture2DLodAmbient(projectionMap, proj_tc.xy, proj_lod);

		amb_da += (da*da*0.5+0.5)*proj_ambiance;

		amb_da *= dist_atten * noise;

		amb_da = min(amb_da, 1.0-lit);

		col += amb_da*color*diff_tex*amb_plcol.rgb*amb_plcol.a;
	}


	vec4 spec = texture2D(specularRect, frag.xy);

	if (spec.a > 0.0)
	{
		dlit *= min(da*6.0, 1.0) * dist_atten;

		vec3 npos = -normalize(pos);

		//vec3 ref = dot(pos+lv, norm);
		vec3 h = normalize(lv+npos);
		float nh = dot(norm, h);
		float nv = dot(norm, npos);
		float vh = dot(npos, h);
		float sa = nh;
		float fres = pow(1 - dot(h, npos), 5)*0.4+0.5;

		float gtdenom = 2 * nh;
		float gt = max(0, min(gtdenom * nv / vh, gtdenom * da / vh));

		if (nh > 0.0)
		{
			float scol = fres*texture2D(lightFunc, vec2(nh, spec.a)).r*gt/(nh*da);
			col += dlit*scol*spec.rgb*shadow;
			//col += spec.rgb;
		}
	}	

	if (envIntensity > 0.0)
	{
		vec3 ref = reflect(normalize(pos), norm);
		
		//project from point pos in direction ref to plane proj_p, proj_n
		vec3 pdelta = proj_p-pos;
		float ds = dot(ref, proj_n);
		
		if (ds < 0.0)
		{
			vec3 pfinal = pos + ref * dot(pdelta, proj_n)/ds;

			vec4 stc = (proj_mat * vec4(pfinal.xyz, 1.0));

			if (stc.z > 0.0)
			{
				stc /= stc.w;

				if (stc.x < 1.0 &&
					stc.y < 1.0 &&
					stc.x > 0.0 &&
					stc.y > 0.0)
				{
					col += color.rgb * texture2DLodSpecular(projectionMap, stc.xy, (1 - spec.a) * (proj_lod * 0.6)).rgb * shadow * envIntensity;
				}
			}
		}
	}

	//not sure why, but this line prevents MATBUG-194
	col = max(col, vec3(0.0));

	frag_color.rgb = col;	
	frag_color.a = 0.0;
}
