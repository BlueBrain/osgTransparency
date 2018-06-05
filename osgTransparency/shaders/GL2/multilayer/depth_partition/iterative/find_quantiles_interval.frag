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

//#define ROUND round or #define ROUND
$DEFINES
$OUT_BUFFERS_DECLARATION

#if POINTS <= 4 && defined DOUBLE_WIDTH
#define INTERVALS 8
#else
#define INTERVALS 4
#endif

uniform int iteration;
uniform sampler2DRect totalCountsTexture;
uniform sampler2DRect leftAccumTextures[POINTS / 4 + 1];
uniform float quantiles[POINTS];
#ifdef DOUBLE_WIDTH
uniform sampler2DRect countTextures[POINTS * 2];
#else
uniform sampler2DRect countTextures[POINTS];
#endif

/**
  The value returned is the interval number (from 0 to INTERVALS - 1) and the
  number of fragments that have been counted from the first interval up to the
  interval in which the quantile has been found (this last one not included).
*/
ivec2 find_quantile_interval(int index, float quantile, float total,
                             int initial_left)
{
    float next_quantile = 0.0;
/* Reading the fragment count of each interval in the current search
   range. Depending on the number of intervals a vec type can be used
   or an array is needed. */
#ifndef DOUBLE_WIDTH
    /* The compiler optimizes out any extra component initialized here
       (at least cgc does). */
    vec4 counts = texture2DRect(countTextures[index], gl_FragCoord.xy).rgba;
#else
    float counts[INTERVALS];
    vec4 t1 = texture2DRect(countTextures[index * 2], gl_FragCoord.xy).rgba;
    vec4 t2 = texture2DRect(countTextures[index * 2 + 1], gl_FragCoord.xy).rgba;
    for (int i = 0; i < 4; ++i)
        counts[i] = t1[i];
    for (int i = 0; i < INTERVALS - 4; ++i)
        counts[4 + i] = t2[i];
#endif
    /* This apparently stupid code performs better because the threads do
       not diverge and all executed in parallel.
       A while loop was considered initially, but it seems that the branching
       introduced that way causes the threads to be serialized. */
    int i = 0;
    float accum = 0.0;
    float current_left = float(initial_left);
    float target_count = ROUND(quantile * total);

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

#ifdef DOUBLE_WIDTH
    int shift = 3 * (iteration - 1) + 5;
#else
    int shift = 2 * (iteration - 1) + 5;
#endif
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
    for (int i = 0; i < POINTS && i < 4; ++i)
    {
        ivec2 interval_and_addition =
            find_quantile_interval(i, quantiles[i], total,
                                   current_left_side_accumulation[i]);
        /* Writing the quantity that needs to be merge in the buffer that
           codifies the sequence of intervals for this quantile. */
        gl_FragData[0][i] = float(interval_and_addition[0] << shift);
        /* And writing the increment for the count of fragments that fall
           on the left of the current interval in which the quantile is
           known to be. */
        gl_FragData[1][i] = float(interval_and_addition[1]);
    }
#if POINTS > 4
    /* Repeating for the reamaining points if any. */
    for (int i = 0; i < POINTS - 4; ++i)
    {
        int index = i + 4;
        ivec2 interval_and_addition =
            find_quantile_interval(index, quantiles[index], total,
                                   current_left_side_accumulation[index]);
        gl_FragData[2][i] = float(interval_and_addition[0] << shift);
        gl_FragData[3][i] = float(interval_and_addition[1]);
    }
#endif
}
