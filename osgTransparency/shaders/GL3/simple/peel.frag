/* Copyright (c) 2006-2018, École Polytechnique Fédérale de Lausanne (EPFL) /
 *                           Blue Brain Project and
 *                          Universidad Politécnica de Madrid (UPM)
 *                          Juan Hernando <juan.hernando@epfl.ch>
 *
 * This file is part of osgTransparency
 * <https://github.com/BlueBrain/osgTransparency>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3.0 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#version 420

// XXX
//#define ALPHA_TEST_DISCARD

#extension GL_ARB_draw_buffers : enable
#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect depthBuffer;

/* Buffer where the front layer of the front half is being blended */
uniform sampler2DRect blendedBuffer;

uniform vec2 offset;
uniform vec2 scaling;

vec4 shadeFragment();

float fragmentDepth();

layout(location = 0) out float outDepth;
layout(location = 1) out vec4 outColor;

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}

void main(void)
{
    float frontDepth = -texture2DRect(depthBuffer, gl_FragCoord.xy).r;
    float depth = fragmentDepth();

/* Checking if the opacity of frontmost layer is above the discard
   threshold. */
#ifdef ALPHA_TEST_DISCARD
    vec2 coord = gl_FragCoord.xy * scaling + offset;
    if (textureRect(blendedBuffer, coord).a >= 0.95)
        discard;
#endif

    if (depth < frontDepth)
    {
        discard;
    }
    if (depth == frontDepth)
    {
        /* Shading fragment peeled front to back */
        vec4 color = shadeFragment();

        color.rgb *= color.a;
        outColor = color;
        outDepth = -1;
    }
    else
    {
        /* Peeling fragment */
        outColor = vec4(0.0);
        outDepth = -depth;
    }
}
