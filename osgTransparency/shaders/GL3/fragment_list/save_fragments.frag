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

#extension GL_EXT_gpu_shader4 : enable

$DEFINES

layout(size1x32) restrict uniform uimage2DRect listHead;
layout(size1x32) restrict uniform uimageBuffer fragmentBuffer;
layout(size1x32) restrict uniform uimage2DRect fragmentCounts;

layout(binding = 0) uniform atomic_uint counter;

#ifdef USE_ALPHA_CUTOFF
layout(size1x32) restrict uniform uimage2DRect depthTranspBuffer;
#endif

uniform float minTransparency;

float fragmentDepth();
vec4 shadeFragment();

void main(void)
{
    /* This has to be done before writing anything, as the client code in
       fragmentDepth and shadeFragment may discard the fragmet. */
    const float depth = fragmentDepth();

#ifdef USE_ALPHA_CUTOFF
    /* Checking the depth to decide if the fragment must be discarded. */
    int i = 0;
    uint depthTransp = imageLoad(depthTranspBuffer, ivec2(gl_FragCoord.xy)).r;
    uint maxDepthi = depthTransp & 0x00FFFFFF;
    uint transparency = depthTransp >> 24;
    const uint depthi = uint(floor(depth * 0x00FFFFFF));

    if (transparency <= minTransparency * 255.0 && depthi > maxDepthi)
        discard;
#endif

    /* Color clamping needed to ensure that each channel is within [0, 1]. */
    const vec4 color = clamp(shadeFragment(), vec4(0.0), vec4(1.0));

#ifdef USE_ALPHA_CUTOFF
    /* The formula below is an approximation of the more ideal t = t (1 - a)
       where t and a are floats in [0, 1]. The most conservative approach would
       be without 0.25, but the resulting formula converges to 0 very badly.
       The biased rounding provided by adding 0.25 overestimates the
       transparency in general, so it's still quite conservative while it
       converges much faster that using floor alone. */
    transparency -= uint(floor(transparency * color.a + 0.25));

    if (maxDepthi < depthi)
        maxDepthi = depthi;
    /* There's an anavoidable race condition here, the min operation tries
       to minimize the side effects. In the worst case scenario, which is
       rendering a primitive which has lots of overlapping triangles on the
       z-axis, the convergence of the transparency to 0 will happen much
       more slowly. */
    imageAtomicMin(depthTranspBuffer, ivec2(gl_FragCoord.xy),
                   maxDepthi | transparency << 24);
#endif

    /* Taking a new fragment from the fragment buffer. */
    uint index = atomicCounterIncrement(counter);

    /* Linking this fragment with the previous one. */
    uint next = imageAtomicExchange(listHead, ivec2(gl_FragCoord.xy), index);

    /* Storing the fragment info.
       Alpha channel goes without premultiplication. */
    const int offset = int(index) * 3;
    imageStore(fragmentBuffer, offset, uvec4(next));
    const uint idepth = floatBitsToUint(depth);
    imageStore(fragmentBuffer, offset + 1, uvec4(idepth));
    const uint icolor = uint(color[0] * 255) + (uint(color[1] * 255) << 8u) +
                        (uint(color[2] * 255) << 16u) +
                        (uint(color[3] * 255) << 24u);
    imageStore(fragmentBuffer, offset + 2, uvec4(icolor));

    /* Increasing the fragment count */
    imageAtomicAdd(fragmentCounts, ivec2(gl_FragCoord.xy), 1u);
}
