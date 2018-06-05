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

//#define POINTS x
//#define OPACITY_THRESHOLD x
//#define ROUND round or #define ROUND
$DEFINES
$OUT_BUFFERS_DECLARATION

#ifndef OPACITY_THRESHOLD
#define OPACITY_THRESHOLD 0.99
#endif

uniform sampler2DRect countTextures[8];
uniform float quantiles[POINTS];

void main()
{
    float counts[32];
    float accum = 0;
    float total;
    for (int i = 0; i < 8; ++i)
    {
        vec4 t = texture2DRect(countTextures[i], gl_FragCoord.xy).rgba;
        for (int j = 0; j < 4; ++j)
            counts[i * 4 + j] = t[j];
    }
    total = counts[31];

    if (total == 0)
        discard;

    float target_counts[8];
    bool written[8];
    for (int i = 0; i < 8 && i < POINTS; ++i)
    {
        target_counts[i] = ROUND(quantiles[i] * total);
        written[i] = false;
    }
    float left_accumulation = 0;
    for (int i = 0; i < 32; ++i)
    {
        float current_count = left_accumulation + counts[i];
#if POINTS < 5
        for (int j = 0; j < POINTS; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                gl_FragData[0][j] = float(i);
                gl_FragData[1][j] = left_accumulation;
                written[j] = true;
            }
        }
#else
        for (int j = 0; j < 4; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                gl_FragData[0][j] = float(i);
                gl_FragData[1][j] = left_accumulation;
                written[j] = true;
            }
        }
        for (int j = 4; j < POINTS; ++j)
        {
            if (!written[j] && current_count > target_counts[j])
            {
                gl_FragData[2][j - 4] = float(i);
                gl_FragData[3][j - 4] = left_accumulation;
                written[j] = true;
            }
        }
#endif
        left_accumulation = current_count;
    }

/* Saving the total per pixel fragment count in its texture for
   further access */
#if POINTS > 4
    const int buff = 4;
#else
    const int buff = 2;
#endif
    gl_FragData[buff].r = total;

#if POINTS > 8
#error "Unsupported number of simultaneous quantiles"
#endif
}
