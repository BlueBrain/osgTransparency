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
/* With NVIDIA's driver, integer textures don't work if this extension
   is not enabled !? */
#extension GL_EXT_gpu_shader4 : enable

$DEFINES

const uint INTERVALS = 4;

uniform int iteration;
uniform sampler2DRect totalCountsTexture;
uniform sampler2DRect leftAccumTextures[POINTS / 4 + 1];
uniform usampler2DRect countTextures[8];
uniform float quantiles[POINTS];

out vec4 codedIntervals[(POINTS + 3) / 4];
out vec4 leftAccumulations[(POINTS + 3) / 4];

/**
  The value returned is the interval number (from 0 to 4 - 1) and the
  number of fragments that have been counted from the first interval up to the
  interval in which the quantile has been found (this last one not included).
*/
ivec2 find_quantile_interval(int index, float quantile, float total,
                             int initial_left)
{
    float next_quantile = 0.0;

    vec4 counts;
    uint texel = texture2DRect(countTextures[index], gl_FragCoord.xy).r;
    counts[0] = texel & 0xFF;
    counts[1] = (texel >> 8u) & 0xFF;
    counts[2] = (texel >> 16u) & 0xFF;
    counts[3] = (texel >> 24u) & 0xFF;

    int i = 0;

    float accum = 0.0;
    float current_left = float(initial_left);
    float target_count = quantile * total;

    for (int j = 0; j < INTERVALS - 1; ++j)
    {
        target_count -= counts[j];
        bool c = current_left <= target_count;
        accum += c ? counts[j] : 0.0;
        i += int(c);
    }

    /* Returning the interval and the left side count increment. */
    return ivec2(i, int(accum));
}

void main()
{
#if POINTS > 8
#error "Unsupported number of simultaneous quantiles"
#endif

    int shift = 2 * (iteration - 1) + 5;

    float total = texture2DRect(totalCountsTexture, gl_FragCoord.xy).r;
    if (total == 0.0)
        discard;

    int current_left_side_accumulation[POINTS];

/* Reading texture data with the fragments counts accumulated at the left
   of the current search interval corresponding to each of the
   quantiles. */
#if POINTS < 4
    vec4 t = texture2DRect(leftAccumTextures[0], gl_FragCoord.xy).rgba;
    for (int i = 0; i < POINTS; ++i)
        current_left_side_accumulation[i] = int(t[i]);
#else
    vec4 t1 = texture2DRect(leftAccumTextures[0], gl_FragCoord.xy).rgba;
    vec4 t2 = texture2DRect(leftAccumTextures[1], gl_FragCoord.xy).rgba;
    for (int i = 0; i < 4; ++i)
        current_left_side_accumulation[i] = int(t1[i]);
    for (int i = 4; i < POINTS; ++i)
        current_left_side_accumulation[i] = int(t2[i - 4]);
#endif
    /* Finding the interval and the number of fragments that are on the left
       to that interval for at most the first 4 quantiles. */
    for (int i = 0; i < 4 && i < POINTS; ++i)
    {
        ivec2 interval_and_addition =
            find_quantile_interval(i, quantiles[i], total,
                                   current_left_side_accumulation[i]);
        /* Writing the quantity that needs to be merge in the buffer that
           codifies the sequence of intervals for this quantile. */
        codedIntervals[0][i] = float(interval_and_addition[0] << shift);
        /* And writing the increment for the count of fragments that fall
           on the left of the current interval in which the quantile is
           known to be. */
        leftAccumulations[0][i] = float(interval_and_addition[1]);
    }
#if POINTS > 4
    /* Repeating for the reamaining points if any. */
    for (int i = 0; i < POINTS - 4; ++i)
    {
        int index = i + 4;
        ivec2 interval_and_addition =
            find_quantile_interval(index, quantiles[index], total,
                                   current_left_side_accumulation[index]);
        codedIntervals[1][i] = float(interval_and_addition[0] << shift);
        leftAccumulations[1][i] = float(interval_and_addition[1]);
    }
#endif
}
