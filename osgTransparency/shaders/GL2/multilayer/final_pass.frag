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

#extension GL_ARB_draw_buffers : enable
#extension GL_ARB_texture_rectangle : enable

$DEFINES

uniform sampler2DRect blendBuffers[SLICES * 2];

void main(void)
{
    /* Reading each layer colors */
    gl_FragColor = vec4(0.0);
    for (int i = 0; i < SLICES * 2; ++i)
    {
        vec4 color = texture2DRect(blendBuffers[i], gl_FragCoord.xy);
        /* 1 - buffer0.a contains Pi(1 - a_i) for all layers in the first
           slice. Front to back blending of slices is associative, so we blend
           the composition of layers for slice 1 and slice 2 using the front
           to back formula, the result with the next slice and so on so
           forth.*/
        gl_FragColor += color * (1.0 - gl_FragColor.a);
    }
}
