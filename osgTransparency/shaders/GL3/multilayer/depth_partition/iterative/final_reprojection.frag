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
#extension GL_EXT_gpu_shader4 : enable

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

//#define POINTS x
//#define FIRST_INTERVAL_SEARCH_WIDTH x
//#define ADJUST_QUANTILES_WITH_ALPHA
//#define DOUBLE_WIDTH?
//#define PROJECT_Z?
$DEFINES

uniform sampler2DRect minMaxTexture;
uniform sampler2DRect totalCountsTexture;
uniform sampler2DRect leftAccumTextures[(POINTS - 1) / 4 + 1];
uniform sampler2DRect codedPreviousIntervalsTextures[(POINTS - 1) / 4 + 1];
uniform usampler2DRect countTextures[POINTS];

uniform float quantiles[POINTS];

uint get_interval_count(int point, int interval)
{
    uint texel = texture2DRect(countTextures[point], gl_FragCoord.xy).r;
    return texel >> (interval * 8) & 0xFF;
}

out vec4 splitPoints[POINTS / 4 + 1];

void main()
{
    vec2 minMax = texture2DRect(minMaxTexture, gl_FragCoord.xy).rg;
    float abs_min = -minMax.x;
    float abs_max = minMax.y;
    float total = texture2DRect(totalCountsTexture, gl_FragCoord.xy).r;
    if (total == 0.0)
        discard;

    vec4 codes1 =
        texture2DRect(codedPreviousIntervalsTextures[0], gl_FragCoord.xy);
    vec4 left_accums1 = texture2DRect(leftAccumTextures[0], gl_FragCoord.xy);
#if POINTS > 4
    vec4 codes2 =
        texture2DRect(codedPreviousIntervalsTextures[1], gl_FragCoord.xy);
    vec4 left_accums2 = texture2DRect(leftAccumTextures[1], gl_FragCoord.xy);
#endif

    float full_width = (abs_max - abs_min) * 0.03125;

#ifdef ADJUST_QUANTILES_WITH_ALPHA
    float last_interval = texture2DRect(totalCountsTexture, gl_FragCoord.xy).g;
    float value = abs_min + full_width * last_interval;
#ifdef PROJECT_Z
    splitPoints[POINTS / 4][POINTS % 4] = reproject(-value);
#else
    splitPoints[POINTS / 4][POINTS % 4] = value;
#endif
#endif

    for (int point = 0; point < POINTS; ++point)
    {
        float quantile = quantiles[point];

        float width = full_width;

        const float width_reduction = 0.25; // 1 / 4.0;
        const int shift = 2;
        const int intervals = 4;
        int mask = 0x03;

        float left;
        int code;
#if POINTS > 4
        if (point < 4)
        {
            left = left_accums1[point];
            code = int(codes1[point]);
        }
        else
        {
            left = left_accums2[point - 4];
            code = int(codes2[point - 4]);
        }
#else
        left = left_accums1[point];
        code = int(codes1[point]);
#endif
        int interval = code & 0x1F; /* lower 5 bits */
        float start = abs_min + width * interval;

        code >>= 5;

        for (int i = 0; i < ITERATIONS; ++i)
        {
            width *= width_reduction;
            interval = code & mask;
            start += width * float(interval);
            code >>= shift;
        }

        float value;

        if (quantile == 0.0)
        {
#ifdef PROJECT_Z
            value = 0;
#else
            value = abs_min;
#endif
        }
        else if (quantile == 1.0)
        {
#ifdef PROJECT_Z
            value = 1;
#else
            value = abs_max;
#endif
        }
        else
        {
            float count = get_interval_count(point, interval);
            float expected = total * quantile;
            if (expected - left < (count + left) - expected)
                value = start;
            else
                value = start + width;
#ifdef PROJECT_Z
            value = reproject(-value);
#endif
        }
        splitPoints[point / 4][point % 4] = value;
    }
}
