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

#extension GL_EXT_shader_image_load_store : enable
#extension GL_ARB_texture_rectangle : enable

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

$DEFINES

layout(r32ui) restrict uniform uimage2DRect counts[POINTS];

uniform sampler2DRect minMaxTexture;
uniform sampler2DRect codedPreviousIntervalsTexture1;
#if POINTS > 4
uniform sampler2DRect codedPreviousIntervalsTexture2;
#endif

int findInterval(float depth, float abs_min, float abs_max,
                 int previous_intervals);

float fragmentDepth();

void main()
{
    vec2 minMax = texture2DRect(minMaxTexture, gl_FragCoord.xy).rg;
    float min = -minMax[0];
    float max = minMax[1];

    float depth = -unproject(fragmentDepth());

    vec4 code1 =
        texture2DRect(codedPreviousIntervalsTexture1, gl_FragCoord.xy).rgba;

    for (int i = 0; i < 4 && i < POINTS; ++i)
    {
        int code = int(code1[i]);
        int t = findInterval(depth, min, max, code);
        if (t != -1)
        {
            imageAtomicAdd(counts[i], ivec2(gl_FragCoord.xy), 1 << (t * 8));
        }
    }

#if POINTS > 4
    vec4 code2 =
        texture2DRect(codedPreviousIntervalsTexture2, gl_FragCoord.xy).rgba;
    for (int i = 4; i < POINTS; ++i)
    {
        int code = int(code2[i - 4]);
        int t = findInterval(depth, min, max, code);
        if (t != -1)
        {
            imageAtomicAdd(counts[i], ivec2(gl_FragCoord.xy), 1 << (t * 8));
        }
    }
#endif

#if POINTS > 8
#error "Unsupported number of simultaneous quantiles"
#endif
}
