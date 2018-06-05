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

//#define POINTS x
//#define OPACITY_THRESHOLD x
$DEFINES

#ifndef OPACITY_THRESHOLD
#define OPACITY_THRESHOLD 0.99
#endif

uniform usampler2DRect countTextures[8];
uniform sampler2DRect totalCountsTexture;
uniform float quantiles[POINTS];

out vec4 codedIntervals[(POINTS + 3) / 4];
out vec4 leftAccumulations[(POINTS + 3) / 4];

uint counts[32];
bool read[8];

uint readCount(uint i)
{
    /* If the texture for this interval count has not been read,
       access it.
       Notice that histogram bins have been interleaved in such a way
       that bin 0 is texture 0 chunk 0, bin 1 is texture 1 chunk 0, bin 8
       is texture 0 chunk 1 and so on so forth. */
    uint index = i & 7;
    if (!read[index])
    {
        uint x = texture2DRect(countTextures[index], gl_FragCoord.xy).r;
        counts[index] = x & 0xFF;
        counts[index + 8] = (x >> 8u) & 0xFF;
        counts[index + 16] = (x >> 16u) & 0xFF;
        counts[index + 24] = (x >> 24u) & 0xFF;
        read[index] = true;
    }
    return counts[i];
}

void main()
{
    uint total = uint(texture2DRect(totalCountsTexture, gl_FragCoord.xy).r);

    if (total == 0)
        discard;

    /* Clearing the flags for which count textures have been read. */
    for (int i = 0; i < 8; ++i)
        read[i] = false;

    /* Computing the target fragment counts for the required quantiles. */
    float target_counts[8];
    bool written[8];
    for (int i = 0; i < 8 && i < POINTS; ++i)
    {
        target_counts[i] = quantiles[i] * total;
        written[i] = false;
    }

    float left_accumulation = 0;
    for (int i = 0; i < 32; ++i)
    {
        float current_count = left_accumulation + readCount(i);
#if POINTS < 5
        for (int j = 0; j < POINTS; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                codedIntervals[0][j] = float(i);
                leftAccumulations[0][j] = left_accumulation;
                written[j] = true;
            }
        }
#else
        for (int j = 0; j < 4; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                codedIntervals[0][j] = float(i);
                leftAccumulations[0][j] = left_accumulation;
                written[j] = true;
            }
        }
        for (int j = 4; j < POINTS; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                codedIntervals[1][j - 4] = float(i);
                leftAccumulations[1][j - 4] = left_accumulation;
                written[j] = true;
            }
        }
#endif
        left_accumulation = current_count;
    }

#if POINTS > 8
#error "Unsupported number of simultaneous quantiles"
#endif
}
