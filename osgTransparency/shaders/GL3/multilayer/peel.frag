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

#extension GL_EXT_shader_image_load_store : enable
#extension GL_ARB_texture_rectangle : enable

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}

$DEFINES
// External defines:
// SLICES x
// PACKED_RGBA?
// OPACITY_THRESHOLD x
#ifndef OPACITY_THRESHOLD
#define OPACITY_THRESHOLD 0.99
#endif

#define DEPTH_BUFFERS ((SLICES + 1) / 2)

uniform sampler2DRect depthBuffers[DEPTH_BUFFERS];

/* Each sampler2DArray  image2DArray pair uses the same underlying texture. */

uniform sampler2DArray frontInColor;
layout(rgba16f) uniform image2DArray frontOutColor;

uniform sampler2DArray backInColor;
layout(rgba16f) uniform image2DArray backOutColor;

out vec4 outDepths[DEPTH_BUFFERS];

/* Note that with integer division (x / 4) * 2 does not necessarily
   equal x / 2. Thus, the previous calculations mustn't be simplified. */

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
vec4 shadeFragment();

void writeShadedLayerFragment(const int layer)
{
    /* Shading a previously peeled fragment.
       Despite with 16 bit color textures clamping wouldn't be needed, reading
       back the framebuffer as 8 bit per channel from non clamped colors yields
       bad results, probably due to conversion overflows. */
    vec4 color = clamp(shadeFragment(), vec4(0.0), vec4(1.0));

    /* Premultiplying alpha. */
    color.rgb *= color.a;

    /* Front layers and back layers are not mixed in the same output
       texture. Since each texel coordinate can be written at most once
       during a peel pass, there's no need to ensure any memory coherency
       when reading the input textures. */
    if ((layer & 1) != 0)
    {
        /* Back to front compositing */
        const vec4 current =
            texelFetch(backInColor, ivec3(gl_FragCoord.xy, layer >> 1), 0);
        imageStore(backOutColor, ivec3(gl_FragCoord.xy, layer >> 1),
                   color + current * (1.0 - color.a));
    }
    else
    {
        /* Front to back compositing */
        const vec4 current =
            texelFetch(frontInColor, ivec3(gl_FragCoord.xy, layer >> 1), 0);
        imageStore(frontOutColor, ivec3(gl_FragCoord.xy, layer >> 1),
                   current + color * (1.0 - current.a));
    }
}

vec4 currentDepths[DEPTH_BUFFERS];

void peelLayer(const int slice, const float depth)
{
    const int buff = slice / 2;
    const int channel = (slice * 2) % 4;
    const float frontDepth = currentDepths[buff][channel];
    const float backDepth = currentDepths[buff][channel + 1];

    if (-depth > frontDepth || depth > backDepth)
        discard;

    if (-depth == frontDepth)
        writeShadedLayerFragment(slice * 2);
    else if (depth == backDepth)
        writeShadedLayerFragment(slice * 2 + 1);
    else
    {
        /* Peeling a new layer */
        outDepths[buff][channel] = -depth;
        outDepths[buff][channel + 1] = depth;
    }
}

void main(void)
{
#ifdef UNPROJECT_DEPTH
    const float depth = -unproject(fragmentDepth());
#else
    const float depth = fragmentDepth();
#endif
    for (int i = 0; i < DEPTH_BUFFERS; ++i)
        currentDepths[i] = texture2DRect(depthBuffers[i], gl_FragCoord.xy);

    const vec2 coord = gl_FragCoord.xy;
/* Getting split points with depth range check included. */
#if SLICES == 2
    const float splitPoint = checkPartitionAndGetSplitPoints(coord, depth);
#elif SLICES < 6
    const RETURN_TYPE splitPoints =
        checkPartitionAndGetSplitPoints(coord, depth);
#elif SLICES != 1
    checkPartitionAndGetSplitPoints(coord, depth);
#endif

    for (int i = 0; i < DEPTH_BUFFERS; ++i)
        outDepths[i] = vec4(-1.0, 0.0, -1.0, 0.0);

    // TODO: Fix this
    // float accumAlpha = texture2DRect(frontBlendedBuffers[0], coord).a;

    for (int i = 0; i < SLICES; ++i)
    {
// TODO: Fix this
///* Checking accumulated visibility */
// if (i > 0) {
//    accumAlpha += (1.0 - accumAlpha) *
//        texture2DRect(backBlendedBuffers[i - 1], coord).a;
//    accumAlpha += (1.0 - accumAlpha) *
//        texture2DRect(frontBlendedBuffers[i], coord).a;
//}
// if (accumAlpha >= OPACITY_THRESHOLD)
//    discard;

/* Checking if the depth value is inside the current depth range */
#if SLICES == 2
        if (i == SLICES - 1 || depth < splitPoint)
        {
#elif SLICES != 1
        if (i == SLICES - 1 || depth < splitPoints[min(i, SLICES - 2)])
        {
#endif
            peelLayer(i, depth);
            return;
#if SLICES != 1
        }
#endif
    }
}
