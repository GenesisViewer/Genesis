/** 
 * @file class1/environment/starsF.glsl
 *
 * $LicenseInfo:firstyear=2021&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2021, Linden Research, Inc.
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

uniform sampler2D diffuseMap;

uniform float custom_alpha;

VARYING vec4 vertex_color;
VARYING vec2 vary_texcoord0;

// See:
// ALM off: class1/environment/starsF.glsl
// ALM on : class1/deferred/starsF.glsl
void main() 
{
    vec4 color = texture2D(diffuseMap, vary_texcoord0.xy);
    color.rgb = pow(color.rgb, vec3(0.45));
    color.rgb *= vertex_color.rgb;
    color.a *= max(custom_alpha, vertex_color.a);

    frag_color = color;
    gl_FragDepth = LL_SHADER_CONST_STAR_DEPTH; // SL-14113 Moon Haze -- Stars need to depth test behind the moon
}
