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

#version 120
#extension GL_ARB_texture_rectangle : enable
#extension GL_EXT_gpu_shader4 : enable

$DEFINES

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

vec4 gl_FragData[8];

uniform sampler2DRect minMaxTexture;
float fragmentDepth();
float fragmentAlpha();

void main()
{
    /* We count unprojected depth values */
    float depth = -unproject(fragmentDepth());
    vec2 minMax = texture2DRect(minMaxTexture, gl_FragCoord.xy).rg;
    float abs_min = -minMax.r;
    float abs_max = minMax.g;

    float width = (abs_max - abs_min) * 0.041666666666666664;
    int interval = min(int((depth - abs_min) / width), 24);
    interval += interval / 3;
    int buff = interval >> 2;
    int index = interval & 3;

    /* As odd as it seems this code is used to prevent the Nvidia compiler
       to reorder the assembly code in such a way that next shaders obtain
       different results for the same calculations. What we really want to
       avoid is that some shaders use RCP and others DIV to calculate
       unproject. We try to stay to DIV since it looks more accurate than RCP.
       (the assembler instruction names follow NVidia conventions). */
    if (abs_min > depth)
    {
        buff = interval / 4;
        index = interval % 4;
    }
    vec4 color = vec4(index == 0, index == 1, index == 2, fragmentAlpha());

    for (int i = 0; i < 8; ++i)
        gl_FragData[i] = vec4(0);
    gl_FragData[buff] = color;
    gl_FragData[7].w = 1.0;
}
