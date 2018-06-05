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
//#define ROUND round or #define ROUND
$DEFINES
$OUT_BUFFERS_DECLARATION

#ifndef OPACITY_THRESHOLD
#define OPACITY_THRESHOLD 0.99
#endif

uniform sampler2DRect countTextures[8];
uniform float quantiles[POINTS];

int approx_required_layers(float alphas[8], float counts[24])
{
    float totals[8];
    for (int i = 0; i < 8; i++)
    {
        totals[i] = 0;
        for (int j = i * 3; j < i * 3 + 3; ++j)
            totals[i] += counts[j];
    }

    for (int i = 1; i < 8; ++i)
    {
        alphas[i] = alphas[i - 1] + (1 - alphas[i - 1]) * alphas[i];
    }

    int layers = 0;
    int k = 0;
    bool done = false;
    for (int i = 0; i < 8; ++i)
    {
        if (!done)
        {
            layers += int(totals[i]);
            k = i * 3;
        }
        done = alphas[i] > OPACITY_THRESHOLD;
    }
/* Saving the number of interval where alpha has gone above the opacity
   threshold */
#if POINTS > 4
    gl_FragData[4][1] = k + 3;
#else
    gl_FragData[2][1] = k + 3;
#endif
    return layers;
}

void main()
{
    float counts[24];
    float alphas[8];
    float total = 0;
    for (int i = 0; i < 8; ++i)
    {
        vec4 t = texture2DRect(countTextures[i], gl_FragCoord.xy).rgba;
        for (int j = 0; j < 3; ++j)
        {
            total += t[j];
            counts[i * 3 + j] = t[j];
        }
        alphas[i] = t.a;
    }
    if (total == 0)
        discard;
    total = approx_required_layers(alphas, counts);

    float target_counts[8];
    bool written[8];
    for (int i = 0; i < 8 && i < POINTS; ++i)
    {
        target_counts[i] = ROUND(quantiles[i] * total);
        written[i] = false;
    }
    float left_accumulation = 0;
    /* Loop over all the histogram intervals */
    for (int i = 0; i < 24; ++i)
    {
        float current_count = left_accumulation + counts[i];
#if POINTS < 5
        /* Checking which quantiles are found in the current index */
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
