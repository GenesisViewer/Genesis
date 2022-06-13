/** 
 * @file glowV.glsl
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
 
uniform mat4 modelview_projection_matrix;

ATTRIBUTE vec3 position;

uniform vec2 glowDelta;

VARYING vec4 vary_texcoord0;
VARYING vec4 vary_texcoord1;
VARYING vec4 vary_texcoord2;
VARYING vec4 vary_texcoord3;

void main() 
{
	vec4 pos = modelview_projection_matrix * vec4(position.xyz, 1.0);
	gl_Position = pos;	
	vec2 texcoord = (pos.xy*0.5+0.5);
	
	vary_texcoord0.xy = texcoord + glowDelta*(-3.5);
	vary_texcoord1.xy = texcoord + glowDelta*(-2.5);
	vary_texcoord2.xy = texcoord + glowDelta*(-1.5);
	vary_texcoord3.xy = texcoord + glowDelta*(-0.5);
	vary_texcoord0.zw = texcoord + glowDelta*(0.5);
	vary_texcoord1.zw = texcoord + glowDelta*(1.5);
	vary_texcoord2.zw = texcoord + glowDelta*(2.5);
	vary_texcoord3.zw = texcoord + glowDelta*(3.5);
}
