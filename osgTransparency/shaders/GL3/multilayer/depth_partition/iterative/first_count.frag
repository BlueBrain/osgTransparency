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

#extension GL_ARB_texture_rectangle : enable
#extension GL_EXT_shader_image_load_store : enable

//#define ACCUMULATE_ALPHA?
//#define COUNT_TEXTURES x  "Must be a power of 2"
$DEFINES

out float total;

layout(r32ui) restrict uniform uimage2DRect counts[COUNT_TEXTURES];

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}
float reproject(float x)
{
    return 0.5 * (1.0 - proj33 - proj34 / x);
}

uniform sampler2DRect minMaxTexture;
float fragmentDepth();

/* http://graphics.stanford.edu/~seander/bithacks.html#ZerosOnRightLinear */
uint index_of_first_one(uint v)
{
    unsigned int c = 32; // c will be the number of zero bits on the right
    v &= -int(v);
    if (v != 0)
        c--;
    if ((v & 0x0000FFFF) != 0)
        c -= 16;
    if ((v & 0x00FF00FF) != 0)
        c -= 8;
    if ((v & 0x0F0F0F0F) != 0)
        c -= 4;
    if ((v & 0x33333333) != 0)
        c -= 2;
    if ((v & 0x55555555) != 0)
        c -= 1;
    return c;
}

void main()
{
    /* We count unprojected depth values */
    float depth = -unproject(fragmentDepth());
    vec2 minMax = texture2DRect(minMaxTexture, gl_FragCoord.xy).rg;
    float abs_min = -minMax.r;
    float abs_max = minMax.g;

    double width = double(abs_max - abs_min) * 0.03125;
    int interval = min(int((depth - abs_min) / width), 31);

    /* Histogram bins are interleaved to avoid colissions in the atomic
       operations */
    uint buffer = interval & (COUNT_TEXTURES - 1);
    uint index = interval >> index_of_first_one(COUNT_TEXTURES);
    uint bit = 1u << (COUNT_TEXTURES * index);
    /* Increasing the count of the bin in which this fragment's depth falls. */
    imageAtomicAdd(counts[buffer], ivec2(gl_FragCoord.xy), bit);
    /* Additive blending is used to count the total number of fragments. */
    total = 1;
}
