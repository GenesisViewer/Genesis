/**
 * @file terrainWaterF.glsl
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

VARYING vec4 vertex_color;
VARYING vec4 vary_texcoord0;
VARYING vec4 vary_texcoord1;

uniform sampler2D detail_0;
uniform sampler2D detail_1;
uniform sampler2D detail_2;
uniform sampler2D detail_3;
uniform sampler2D alpha_ramp;

vec3 atmosLighting(vec3 light);

vec4 applyWaterFog(vec4 color);

void main()
{
	/// Note: This should duplicate the blending functionality currently used for the terrain rendering.
	
	/// TODO Confirm tex coords and bind them appropriately in vert shader.
	vec4 color0 = texture2D(detail_0, vary_texcoord0.xy);
	vec4 color1 = texture2D(detail_1, vary_texcoord0.xy);
	vec4 color2 = texture2D(detail_2, vary_texcoord0.xy);
	vec4 color3 = texture2D(detail_3, vary_texcoord0.xy);

	float alpha1 = texture2D(alpha_ramp, vary_texcoord0.zw).a;
	float alpha2 = texture2D(alpha_ramp,vary_texcoord1.xy).a;
	float alphaFinal = texture2D(alpha_ramp, vary_texcoord1.zw).a;
	vec4 outColor = mix( mix(color3, color2, alpha2), mix(color1, color0, alpha1), alphaFinal );
	
	/// Add WL Components
	outColor.rgb = atmosLighting(outColor.rgb * vertex_color.rgb);
	
	outColor = applyWaterFog(outColor);
	frag_color = outColor;
}

