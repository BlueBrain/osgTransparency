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
$OUT_BUFFERS_DECLARATION

uniform sampler2DRect minMaxTexture;
uniform sampler2DRect codedPreviousIntervalsTextures[(POINTS + 3) / 4];

int findInterval(float depth, float abs_min, float abs_max,
                 int previous_intervals);

float fragmentDepth();

void main()
{
    vec2 minMax = texture2DRect(minMaxTexture, gl_FragCoord.xy).rg;
    float min = -minMax[0];
    float max = minMax[1];

    float depth = -unproject(fragmentDepth());

    int codes[8];
    vec4 code1 =
        texture2DRect(codedPreviousIntervalsTextures[0], gl_FragCoord.xy).rgba;
    for (int i = 0; i < 4; ++i)
        codes[i] = int(code1[i]);
#if POINTS > 4
    vec4 code2 =
        texture2DRect(codedPreviousIntervalsTextures[1], gl_FragCoord.xy).rgba;
    for (int i = 0; i < 4; ++i)
        codes[4 + i] = int(code2[i]);
#endif

/* Writing default output values. */
#if POINTS > 4 || !defined DOUBLE_WIDTH
    for (int i = 0; i < POINTS; ++i)
        gl_FragData[i] = vec4(0);
#else
    for (int i = 0; i < POINTS * 2; ++i)
        gl_FragData[i] = vec4(0);
#endif

#if POINTS > 4 || !defined DOUBLE_WIDTH
    for (int i = 0; i < POINTS; ++i)
    {
        int t = findInterval(depth, min, max, codes[i]);
        if (t != -1)
            gl_FragData[i] = vec4(float(t == 0), float(t == 1), float(t == 2),
                                  float(t == 3));
    }
#else
    for (int i = 0; i < POINTS; ++i)
    {
        int t = findInterval(depth, min, max, codes[i]);
        if (t > 3)
        {
            vec4 output = vec4(t == 4, t == 5, t == 6, t == 7);
            gl_FragData[i * 2 + 1] = output;
        }
        else if (t != -1)
        {
            vec4 output = vec4(t == 0, t == 1, t == 2, t == 3);
            gl_FragData[i * 2] = output;
        }
    }
#endif

#if POINTS > 8
#error "Unsupported number of simultaneous quantiles"
#endif
}
