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

#extension GL_EXT_gpu_shader4 : enable
#extension GL_ARB_draw_buffers : enable
#extension GL_ARB_texture_rectangle : enable
#extension GL_ARB_shader_bit_encoding : enable

$DEFINES
// External defines:
// SLICES x

/* This shader is used alternativly for the front and back layers */

uniform sampler2DRect colorTextures[(SLICES + 3) / 4];

void main(void)
{
    for (int i = 0; i < (SLICES + 3) / 4; ++i)
    {
        /** \todo Avoid 4 channel texture reads when possible */
        vec4 layerColors = texture2DRect(colorTextures[i], gl_FragCoord.xy);
        for (int j = 0; j < 4 && j + (i * 4) < SLICES; ++j)
        {
            int color = int(floatBitsToInt(layerColors[j]));
            int index = i * 4 + j;
            if (color != int(0xFF800000))
            {
                // We use unsigned numbers because otherwise >> operator carries
                // the sign bit to the right.
                unsigned int alpha = unsigned int(color) >> 24;
                if (alpha >= 129u)
                    alpha -= 2;
                vec4 rgba =
                    vec4(float((color >> 16) & 255) / 255.0,
                         float((color >> 8) & 255) / 255.0,
                         float(color & 255) / 255.0, float(alpha) / 253.0);
                gl_FragData[index] = vec4(rgba.rgb * rgba.a, rgba.a);
            }
            else
            {
                gl_FragData[index] = vec4(0);
            }
        }
    }
}
