/** 
 * @file lightShinyF.glsl
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
VARYING vec2 vary_texcoord0;
VARYING vec3 vary_texcoord1;

uniform samplerCube environmentMap;
uniform sampler2D diffuseMap;

vec3 scaleSoftClip(vec3 light);
vec3 atmosLighting(vec3 light);
vec4 applyWaterFog(vec4 color);

void shiny_lighting()
{
	vec4 color = texture2D(diffuseMap,vary_texcoord0.xy);
	color.rgb *= vertex_color.rgb;
	
	vec3 envColor = textureCube(environmentMap, vary_texcoord1.xyz).rgb;	
	color.rgb = mix(color.rgb, envColor.rgb, vertex_color.a);

	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);
	color.a = 1.0;
	frag_color = color;
}

