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

$DEFINES
//#define SLICES n
//#define ADJUST_QUANTILES_WITH_ALPHA?

#ifdef ADJUST_QUANTILES_WITH_ALPHA
#define POINTS SLICES
#else
#define POINTS SLICES - 1
#endif
#define SPLITS SLICES - 1

#if POINTS >= 1
uniform sampler2DRect depthPartitionTextures[(POINTS + 3) / 4];
#endif

/* Define for the return type and the value for the discard case. */
#if SPLITS == 1
#define RETURN_TYPE float
#elif SPLITS == 2
#define RETURN_TYPE vec2
#elif SPLITS == 3
#define RETURN_TYPE vec3
#elif SPLITS == 4
#define RETURN_TYPE vec4
#else
#define RETURN_TYPE bool
#endif

/* Defines for the vector type and swizzle declarations for the last texture
   fetch and the return value. */
/* Macro arithmetics are working funny */
#if POINTS == 1 || POINTS == 5
#define CHANNELS r
#elif POINTS == 2 || POINTS == 6
#define CHANNELS rg
#elif POINTS == 3 || POINTS == 7
#define CHANNELS rgb
#elif POINTS == 4 || POINTS == 8
#define CHANNELS rgba
#elif POINTS > 8
#error "Unsupported number of split points"
#endif

#if SPLITS > 4
/* Arrays seems to be passed by value and not be reference (?) so we need
   to use a global variable in this cases. */
float splitPoints[SPLITS];
#endif
/* Function prototype. */
RETURN_TYPE checkPartitionAndGetSplitPoints(const vec2 coord, const float depth)
{
/* Reading depth partition textures. */
#if POINTS > 4
    vec4 points[2];
    points[0] = texture2DRect(depthPartitionTextures[0], coord).rgba;
    points[1].CHANNELS =
        texture2DRect(depthPartitionTextures[1], coord).CHANNELS;
#elif POINTS != 0
    vec4 points[1];
    points[0].CHANNELS =
        texture2DRect(depthPartitionTextures[0], coord).CHANNELS;
#endif

#if defined ADJUST_QUANTILES_WITH_ALPHA
#if POINTS < 5
    if (depth > points[0][POINTS - 1])
        discard;
#else
    if (depth > points[1][POINTS - 5])
        discard;
#endif
#endif

#if SPLITS == 0
    return true;
#elif SPLITS == 1
    return points[0].r;
#elif SPLITS == 2
    return points[0].rg;
#elif SPLITS == 3
    return points[0].rgb;
#elif SPLITS == 4
    return points[0];
#else
    for (int i = 0; i < 4; ++i)
        splitPoints[i] = points[0][i];
    for (int i = 4; i < SPLITS; ++i)
        splitPoints[i] = points[1][i - 4];
    return true;
#endif
}
