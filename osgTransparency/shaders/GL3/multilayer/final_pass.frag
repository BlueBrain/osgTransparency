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
#extension GL_ARB_texture_rectangle : enable

$DEFINES

uniform sampler2DArray frontColors;
uniform sampler2DArray backColors;

/* The lower left corner of the camera viewport */
uniform vec2 corner;

out vec4 outColor;

void main(void)
{
    /* Reading each layer colors */
    vec2 coord = gl_FragCoord.xy - corner;
    outColor = vec4(0.0);

    for (int i = 0; i < SLICES; ++i)
    {
        vec4 front = texelFetch(frontColors, ivec3(coord, i), 0);
        vec4 back = texelFetch(backColors, ivec3(coord, i), 0);

        /* 1 - buffer0.a contains Pi(1 - a_i) for all layers in the first
           slice. Front to back blending of slices is associative, so we blend
           the composition of layers using the front to back formula, the
           result with the next layer and so on so forth.*/
        outColor += front * (1.0 - outColor.a);
        outColor += back * (1.0 - outColor.a);
    }
}
