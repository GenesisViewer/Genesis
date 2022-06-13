/** 
 * @file pointLightF.glsl
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
uniform sampler2D normalMap;
uniform samplerCube environmentMap;
uniform sampler2D noiseMap;
uniform sampler2D lightFunc;
uniform sampler2D depthMap;

uniform vec3 env_mat[3];
uniform float sun_wash;

uniform vec3 color;
uniform float falloff;
uniform float size;

VARYING vec4 vary_fragcoord;
VARYING vec3 trans_center;

uniform mat4 inv_proj;
uniform vec4 viewport;
uniform vec2 noise_scale;

vec3 decode_normal(vec2 enc);
vec4 getPosition(vec2 pos_screen);

void main() 
{
	vec4 frag = vary_fragcoord;
	frag.xyz /= frag.w;
	frag.xyz = frag.xyz*0.5+0.5;
	
	vec3 pos = getPosition(frag.xy).xyz;
	vec3 lv = trans_center.xyz-pos;
	float dist = length(lv);
	dist /= size;
	if (dist > 1.0)
	{
		discard;
	}
	
	vec3 norm = texture2D(normalMap, frag.xy).xyz;
	norm = decode_normal(norm.xy); // unpack norm
	float da = dot(norm, lv);
	if (da < 0.0)
	{
		discard;
	}
	
	norm = normalize(norm);
	lv = normalize(lv);
	da = dot(norm, lv);
	
	float noise = texture2D(noiseMap, frag.xy*noise_scale).b;
	
	vec3 col = texture2D(diffuseRect, frag.xy).rgb;
	float fa = falloff+1.0;
	float dist_atten = clamp(1.0-(dist-1.0*(1.0-fa))/fa, 0.0, 1.0);
	dist_atten *= dist_atten;
	dist_atten *= 2.0;
	
	float lit = da * dist_atten * noise;

	col = color.rgb*lit*col;

	vec4 spec = texture2D(specularRect, frag.xy);
	if (spec.a > 0.0)
	{
		lit = min(da*6.0, 1.0) * dist_atten;

		vec3 npos = -normalize(pos);
		vec3 h = normalize(lv+npos);
		float nh = dot(norm, h);
		float nv = dot(norm, npos);
		float vh = dot(npos, h);
		float sa = nh;
		float fres = pow(1 - dot(h, npos), 5) * 0.4+0.5;
		float gtdenom = 2 * nh;
		float gt = max(0,(min(gtdenom * nv / vh, gtdenom * da / vh)));

		if (nh > 0.0)
		{
			float scol = fres*texture2D(lightFunc, vec2(nh, spec.a)).r*gt/(nh*da);
			col += lit*scol*color.rgb*spec.rgb;
		}
	}
	
	if (dot(col, col) <= 0.0)
	{
		discard;
	}
		
	frag_color.rgb = col;	
	frag_color.a = 0.0;
}
