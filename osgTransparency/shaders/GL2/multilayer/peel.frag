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

#version 130 // To workaround a problem in the vizcluster
#extension GL_EXT_gpu_shader4 : enable
#extension GL_ARB_draw_buffers : enable
#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_shader_bit_encoding : enable

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}

$DEFINES
// External defines:
// SLICES x
// OPACITY_THRESHOLD x
// UNPROJECT_DEPTH
// OPACITY_THRESHOLD

#define DEPTH_BUFFERS ((SLICES + 1) / 2)

uniform sampler2DRect depthBuffers[DEPTH_BUFFERS];

/* Buffers where the front layer of the different slices are being blended.
   This buffers are used for early discard of fragments. */
uniform sampler2DRect frontBlendedBuffers[SLICES];
uniform sampler2DRect backBlendedBuffers[SLICES];

out vec4 depths[DEPTH_BUFFERS];
out vec4 colors[((SLICES + 3) / 4) * 2];

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

float fragmentDepth();
vec4 shadeFragment();

#if SLICES > 5
/* Arrays seems to be passed by value and not be reference (?) so we need
   to use a global variable in this cases. */
extern float splitPoints[SLICES - 1];
#endif
RETURN_TYPE checkPartitionAndGetSplitPoints(const vec2 coord,
                                            const float depth);

void writeShadedLayerFragment(const int layer)
{
    /* Shading a previously peeled fragment */
    /* Clamping before alpha premultiplication may overdarken light spots */
    vec4 color = clamp(shadeFragment(), vec4(0.0), vec4(1.0));

    /* Front layers and back layers are not mixed in the same output texture
       The following calculations assume that front layers are even and back
       layers odd. The final layout for the maximum number of layers would be
       texture1: r = layer 0, b = layer 2, g = layer 4, a = layer 6
       texture2: r = layer 1, b = layer 3, g = layer 5, a = layer 7
       texture3: r = layer 8, b = layer 10, g = layer 12, a = layer 14
       texture4: r = layer 9, b = layer 11, g = layer 13, a = layer 15
    */
    const int buff = DEPTH_BUFFERS + (layer / 8) * 2 + layer % 2;
    const int channel = layer / 2 % 4;

    /* Coding the float rgba color in a single 32bit float. Coding 4 unsigned
       chars in a float and passing them through the GL_RGBA_MAX blending
       operator works as long as the most significant byte avoids the following
       values:
       0xff, 0x7f - which combined with a 1 at bit 23 produces the exponent
                    that signals 'nan'.
       0x80, 0x00 - this can easily yield denormalized representations. In
                    current architectures denormalized numbers collapse to
                    0.
       The alpha channel is coded in the most significant byte avoiding 0xff,
       0x7f and 0x80. For 0x00 there is no problem because if alpha equals 0
       the rest of the channels don't matter. This leaves 254 usable values
       out of 256 possible. The codification is quite simple, the normalized
       alpha value is multiplied by 252 and is it's value is greater or equal
       to 127 then it's incremented by two. */
    ivec3 rgb = ivec3(round(255.0 * color.rgb));
    int a = int(round(252.0 * color.a));
    if (a >= 127)
        a += 2;
    int icolor = (a << 24) | (rgb[0] << 16) | (rgb[1] << 8) | rgb[2];
    colors[buff - DEPTH_BUFFERS][channel] = intBitsToFloat(icolor);
}

vec4 currentDepths[DEPTH_BUFFERS];

void peelLayer(const int slice, const float fragDepth)
{
    const int buff = slice / 2;
    const int channel = (slice * 2) % 4;
    float frontDepth = currentDepths[buff][channel];
    float backDepth = currentDepths[buff][channel + 1];

    if (-fragDepth > frontDepth || fragDepth > backDepth)
        discard;

    if (-fragDepth == frontDepth)
        writeShadedLayerFragment(slice * 2);
    else if (fragDepth == backDepth)
        writeShadedLayerFragment(slice * 2 + 1);
    else
    {
        /* Peeling a new layer */
        depths[buff][channel] = -fragDepth;
        depths[buff][channel + 1] = fragDepth;
    }
}

void main(void)
{
#ifdef UNPROJECT_DEPTH
    float depth = -unproject(fragmentDepth());
#else
    float depth = fragmentDepth();
#endif
    for (int i = 0; i < DEPTH_BUFFERS; ++i)
        currentDepths[i] = texture2DRect(depthBuffers[i], gl_FragCoord.xy);

    vec2 coord = gl_FragCoord.xy;
/* Getting split points (adjusted to the range accepted by alpha
   accumulation check if enabled). */
#if SLICES == 2
    float splitPoint = checkPartitionAndGetSplitPoints(coord, depth);
#elif SLICES < 6
    RETURN_TYPE splitPoints = checkPartitionAndGetSplitPoints(coord, depth);
#else
    checkPartitionAndGetSplitPoints(coord, depth);
#endif
    for (int i = 0; i < DEPTH_BUFFERS; ++i)
        depths[i] = vec4(-1.0, 0.0, -1.0, 0.0);

    for (int i = 0; i < (SLICES + 3) / 4 * 2; ++i)
        colors[i] = vec4(intBitsToFloat(int(0xFF800000))); // Minus infinite

#ifdef OPACITY_THRESHOLD
    float accumAlpha = texture2DRect(frontBlendedBuffers[0], coord).a;
    float backAlpha = texture2DRect(backBlendedBuffers[0], coord).a;
#endif

    for (int i = 0; i < SLICES; ++i)
    {
/* Checking accumulated visibility */
#ifdef OPACITY_THRESHOLD
        if (i > 0)
        {
            accumAlpha += (1.0 - accumAlpha) * backAlpha;
            accumAlpha += (1.0 - accumAlpha) *
                          texture2DRect(frontBlendedBuffers[i], coord).a;
            backAlpha = texture2DRect(backBlendedBuffers[i], coord).a;
        }
        if (accumAlpha >= OPACITY_THRESHOLD)
            discard;
#endif

/* Checking if the depth value is inside the current depth range */
#if SLICES == 1
        peelLayer(0, depth);
#else
#if SLICES == 2
        if (i == SLICES - 1 || depth < splitPoint)
#else
        if (i == SLICES - 1 || depth < splitPoints[min(i, SLICES - 2)])
#endif
        {
            peelLayer(i, depth);
            return;
        }
#endif
    }
}
