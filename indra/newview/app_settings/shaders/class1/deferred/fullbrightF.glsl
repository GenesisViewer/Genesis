/** 
 * @file fullbrightF.glsl
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

#if !HAS_DIFFUSE_LOOKUP
uniform sampler2D diffuseMap;
#endif

VARYING vec3 vary_position;
VARYING vec4 vertex_color;
VARYING vec2 vary_texcoord0;

vec3 srgb_to_linear(vec3 cs);
vec3 linear_to_srgb(vec3 cl);

vec3 fullbrightAtmosTransportDeferred(vec3 light)
{
	return light;
}

vec3 fullbrightScaleSoftClipDeferred(vec3 light)
{
	//soft clip effect:
	return light;
}

#ifdef HAS_ALPHA_MASK
uniform float minimum_alpha;
#endif

#ifdef WATER_FOG
uniform vec4 waterPlane;
uniform vec4 waterFogColor;
uniform float waterFogDensity;
uniform float waterFogKS;

vec4 applyWaterFogDeferred(vec3 pos, vec4 color)
{
	//normalize view vector
	vec3 view = normalize(pos);
	float es = -(dot(view, waterPlane.xyz));

	//find intersection point with water plane and eye vector
	
	//get eye depth
	float e0 = max(-waterPlane.w, 0.0);
	
	vec3 int_v = waterPlane.w > 0.0 ? view * waterPlane.w/es : vec3(0.0, 0.0, 0.0);
	
	//get object depth
	float depth = length(pos - int_v);
		
	//get "thickness" of water
	float l = max(depth, 0.1);

	float kd = waterFogDensity;
	float ks = waterFogKS;
	vec4 kc = waterFogColor;
	
	float F = 0.98;
	
	float t1 = -kd * pow(F, ks * e0);
	float t2 = kd + ks * es;
	float t3 = pow(F, t2*l) - 1.0;
	
	float L = min(t1/t2*t3, 1.0);
	
	float D = pow(0.98, l*kd);
	
	color.rgb = color.rgb * D + kc.rgb * L;
	color.a = kc.a + color.a;
	
	return color;
}
#endif

void main() 
{
#if HAS_DIFFUSE_LOOKUP
	vec4 color = diffuseLookup(vary_texcoord0.xy);
#else
	vec4 color = texture2D(diffuseMap, vary_texcoord0.xy);
#endif

	float final_alpha = color.a * vertex_color.a;

#ifdef HAS_ALPHA_MASK
	if (color.a < minimum_alpha)
	{
		discard;
	}
#endif

	color.rgb *= vertex_color.rgb;
	color.rgb = srgb_to_linear(color.rgb);
	color.rgb = fullbrightAtmosTransportDeferred(color.rgb);
	color.rgb = fullbrightScaleSoftClipDeferred(color.rgb);
	
	color.rgb = linear_to_srgb(color.rgb);

#ifdef WATER_FOG
	vec3 pos = vary_position;
	vec4 fogged = applyWaterFogDeferred(pos, vec4(color.rgb, final_alpha));
	color.rgb = fogged.rgb;
#ifndef HAS_ALPHA_MASK
	color.a   = fogged.a;
#endif
#else
#ifndef HAS_ALPHA_MASK
	color.a   = final_alpha;
#endif
#endif
#ifdef HAS_ALPHA_MASK
	color.a = 0.0;
#endif 

	frag_color.rgb = color.rgb;
	frag_color.a   = color.a;
}

