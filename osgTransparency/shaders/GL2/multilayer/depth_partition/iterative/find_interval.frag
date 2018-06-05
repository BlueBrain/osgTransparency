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

uniform int iteration;

//#define DOUBLE_WIDTH ?
//#define FIRST_INTERVAL_SEARCH_WIDTH x
$DEFINES

int findInterval(float depth, float abs_min, float abs_max,
                 int previous_intervals)
{
    if (depth > abs_max || depth <= abs_min)
        return -1;
#if DOUBLE_WIDTH
    const float width_reduction = 0.125; // 1 / 8.0
    const int shift = 3;
    const int intervals = 8;
    int mask = 0x07;
#else
    const float width_reduction = 0.25; // 1 / 4.0;
    const int shift = 2;
    const int intervals = 4;
    int mask = 0x03;
#endif

    float width = (abs_max - abs_min) * FIRST_INTERVAL_SEARCH_WIDTH;
    int interval_index = previous_intervals & 0x1F; /* 5 lower bits */
    float start = abs_min + width * float(interval_index);

    /* At each iteration we take the next 2-3 bits (depending on the number
       of intervals per point for iterations after the first) from the
       search code.
       These bits represent the interval in which a particular quantile point
       has been classified in previous iterations. Using that index we
       advance the starting point for the next interval set and divide the
       width according by the number of intervals. */
    previous_intervals >>= 5;
    for (int i = 1; i < iteration; ++i)
    {
        width *= width_reduction;
        int interval_index = previous_intervals & mask;
        start += width * float(interval_index);
        previous_intervals >>= shift;
    }
    /* Finally we compute the interval width for the current search range. */
    width *= width_reduction;

    /* Due to the limited precision, there's a chance that fragment depths
       fall onto one of the interval edges. It's been checked that with
       certain quantile quantities (0, 1) this can cause redundant count
       of points as both on the left of the range and on the first interval.
       There's little that can be done except from re-doing the left count
       for each range, however it requires either extra buffer memory or an
       additional pass, therefore it's not an option.
       Doing depth <= start seems to behave worse, and that's the only reason
       for considering an open search interval. */
    if (depth < start)
        /* This is a fragment at the left of the current search range. */
        return -1;
    /* Conversion from float to int drops the fractional part. */
    int interval = int((depth - start) / width);
    if (interval >= intervals)
        return -1;
    return interval;
}
