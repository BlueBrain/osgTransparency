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
#extension GL_EXT_gpu_shader4 : enable

$DEFINES
#define SLICES $SLICES
#define DEPTH_BUFFERS ((SLICES + 1) / 2)

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}

#if SLICES == 2
#define RETURN_TYPE float
#elif SLICES == 3
#define RETURN_TYPE vec2
#elif SLICES == 4
#define RETURN_TYPE vec3
#elif SLICES == 5
#define RETURN_TYPE vec4
#else
#define RETURN_TYPE bool
#endif

float fragmentDepth();
vec4 shadeFragment();
#if SLICES > 5
extern float splitPoints[SLICES - 1];
#endif
RETURN_TYPE checkPartitionAndGetSplitPoints(const vec2 coord,
                                            const float depth);

vec4 gl_FragData[(SLICES + 1) / 2];

void main(void)
{
    vec2 coord = gl_FragCoord.xy;

#ifdef UNPROJECT_DEPTH
    float depth = -unproject(fragmentDepth());
#else
    float depth = fragmentDepth();
#endif

#if SLICES == 2
    float splitPoint = checkPartitionAndGetSplitPoints(coord, depth);
#elif SLICES < 6
    RETURN_TYPE splitPoints = checkPartitionAndGetSplitPoints(coord, depth);
#else
    checkPartitionAndGetSplitPoints(coord, depth);
#endif

    vec2 depths = vec2(-depth, depth);
#if SLICES == 1
    gl_FragData[0].xy = depths;
#elif SLICES == 2
    if (depth < splitPoint)
    {
        gl_FragData[0].xy = depths;
        gl_FragData[0].zw = vec2(-1.0, 0);
    }
    else
    {
        gl_FragData[0].xy = vec2(-1.0, 0);
        gl_FragData[0].zw = depths;
    }
#elif SLICES > 2
    for (int i = 0; i < DEPTH_BUFFERS; ++i)
        gl_FragData[i] = vec4(-1.0, 0.0, -1.0, 0.0);

    for (int i = 0; i < SLICES; ++i)
    {
        if (i == SLICES - 1 || depth < splitPoints[min(i, SLICES - 2)])
        {
            if (i % 2 == 0)
                gl_FragData[i / 2].xy = depths;
            else
                gl_FragData[i / 2].zw = depths;
            return;
        }
    }
#endif
}
