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
#define SLICES $SLICES

uniform float proj33;
uniform float proj34;

vec4 gl_FragData[(SLICES - 1) / 4 + 1];

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
#if SLICES > 5
/* Arrays seems to be passed by value and not be reference (?) so we need
   to use a global variable in this cases. */
extern float splitPoints[SLICES - 1];
#endif
RETURN_TYPE checkPartitionAndGetSplitPoints(const vec2 coord, float depth);

float fragmentDepth();

void main()
{
    vec2 coord = gl_FragCoord.xy;

#ifdef UNPROJECT_DEPTH
    float depth = -unproject(fragmentDepth());
#else
    float depth = fragmentDepth();
#endif

#if SLICES == 1
    checkPartitionAndGetSplitPoints(coord, depth);
    gl_FragData[0][0] = 1.0;
#else
#if SLICES == 2
    float splitPoints[1];
    splitPoints[0] = checkPartitionAndGetSplitPoints(coord, depth);
#elif SLICES < 6
    RETURN_TYPE splitPoints = checkPartitionAndGetSplitPoints(coord, depth);
#else
    checkPartitionAndGetSplitPoints(coord, depth);
#endif
    for (int i = 0; i < SLICES - 1; ++i)
    {
        if (depth < splitPoints[i])
        {
            gl_FragData[i / 4][i % 4] = 1.0;
            return;
        }
    }
    gl_FragData[(SLICES - 1) / 4][(SLICES - 1) % 4] = 1.0;
#endif
}
