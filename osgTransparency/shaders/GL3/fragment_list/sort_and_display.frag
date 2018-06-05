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

layout(early_fragment_tests) in;

const int MAX_FRAGMENTS_PER_LIST = $MAX_FRAGMENTS_PER_LIST;

uniform usampler2DRect listHead;

uniform usamplerBuffer fragmentBuffer;
/* The lower left corner of the camera viewport */
uniform vec2 corner;

layout(location = 0) out vec4 outColor;

#define SWAP(array, i, j, type)    \
    {                              \
        const type tmp = array[i]; \
        array[i] = array[j];       \
        array[j] = tmp;            \
    }

float depths[$MAX_FRAGMENTS_PER_LIST];
uint icolors[$MAX_FRAGMENTS_PER_LIST];
uint size = 0;

vec4 unpackColor(uint icolor)
{
    const vec4 color = vec4(float(icolor & 0xFFu) / 255.0,
                            float((icolor & 0xFF00u) >> 8u) / 255.0,
                            float((icolor & 0xFF0000u) >> 16u) / 255.0,
                            float((icolor & 0xFF000000u) >> 24u) / 255.0);
    /* Premultiplying alpha */
    return vec4(color.rgb * color.a, color.a);
}

void copyToArrays()
{
    const vec2 coord = gl_FragCoord.xy - corner;

    uint index = texture2DRect(listHead, coord).r;

    if (index == 0xFFFFFFFF)
        discard;

    while (index != 0xFFFFFFFF)
    {
        const int offset = int(index) * 3;
        index = texelFetchBuffer(fragmentBuffer, offset)[0];
        const uint idepth = texelFetchBuffer(fragmentBuffer, offset + 1)[0];
        depths[size] = uintBitsToFloat(idepth);
        icolors[size] = texelFetchBuffer(fragmentBuffer, offset + 2)[0];
        ++size;
    }
}

void insertSort()
{
    for (int i = 1; i < size && i < MAX_FRAGMENTS_PER_LIST; ++i)
    {
        for (int j = i; j > 0; --j)
        {
            if (depths[j] < depths[j - 1])
            {
                SWAP(depths, j, j - 1, float);
                SWAP(icolors, j, j - 1, uint);
            }
            else
                break;
        }
    }
}

#define PARENT(index) ((index - 1) / 2)
#define LEFT(index) (index * 2 + 1)
#define RIGHT(index) (index * 2 + 2)

void fixHeapDown(uint start, uint end)
{
    uint root = start;
    uint left = LEFT(root);
    uint right = RIGHT(root);
    while (left <= end)
    {
        /* Find which of the elements at the root and its children is the
           smallest */
        uint largest = depths[root] < depths[left] ? left : root;
        largest =
            right <= end && depths[largest] < depths[right] ? right : largest;
        if (largest == root)
            /* Nothing needs to be changed */
            return;
        SWAP(depths, largest, root, float);
        SWAP(icolors, largest, root, uint);
        root = largest;
        left = LEFT(root);
        right = RIGHT(root);
    }
}

void heapify()
{
    for (uint start = PARENT(size - 1); start > 0; --start)
        fixHeapDown(start, size - 1);
    fixHeapDown(0, size - 1);
}

void heapSort()
{
    if (size <= 1)
        return;

    heapify();
    for (uint end = size - 1; end > 0; --end)
    {
        SWAP(depths, end, 0, float);
        SWAP(icolors, end, 0, uint);
        fixHeapDown(0, end - 1);
    }
}

void blendAndDisplay()
{
    vec4 color = unpackColor(icolors[0]);

    float depth = depths[0];
    for (uint i = 1; i < size; ++i)
    {
        if (depths[i] < depth)
        {
            outColor = vec4(1, 0, 1, 1);
            return;
        }
        depth = depths[i];

        color += unpackColor(icolors[i]) * (1 - color.a);
    }

    outColor = color;
}

void main(void)
{
    copyToArrays();
#if $MAX_FRAGMENTS_PER_LIST < 32
    insertSort();
#else
    heapSort();
#endif
    blendAndDisplay();
}
