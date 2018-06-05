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

#extension GL_ARB_texture_rectangle : enable

$DEFINES

uniform float proj33;
uniform float proj34;
float unproject(float x)
{
    return -proj34 / (proj33 + 2.0 * x - 1.0);
}
float reproject(float x)
{
    return 0.5 * (1.0 - proj33 - proj34 / x);
}

float fragmentDepth();

void main()
{
    /* We want to compute:
       min = unproject(max_i{x_i} + delta)
       max = unproject(min_i{x_i} - delta)
       (Note that unproject is monotonically
        decreasing in [0,1] -> [-near,-far], that's the reason for using min_i
        for max and vice versa).
       However that would require two passes, so instead we try:
       min = min_i{unproject(x_i + delta)}
       max = max_i{unproject(x_i - delta)} */

    /* This code is being optimized to use RCP instead of DIV for reproject.
       The end result is that some values computed in the next steps' shaders
       lie outside the computed range.
       Forcing cgc to use the best precision is not working, so the current
       workaround is shifting the values with a reasonable offset for our
       scene dimensions. */
    float depth = -unproject(fragmentDepth());
#ifdef HALF_FLOAT_MINMAX
    float min = depth - 1;
    float max = depth + 1;
#else
    float min = depth - 0.005;
    float max = depth + 0.005;
#endif
    gl_FragColor.rg = vec2(-min, max);
}
