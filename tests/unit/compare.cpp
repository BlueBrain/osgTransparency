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

#include "osgTransparency/util/paths.h"

#include <osg/Image>
#include <osg/io_utils>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>

#include <boost/filesystem/operations.hpp>
#include <boost/test/unit_test.hpp>

/**
   With this operator there's no need to check channel RMS error values one
   by one for image comparison (which also clutters the error output).
*/
namespace osg
{
bool operator<(const Vec4f &a, const float &b)
{
    for (unsigned int i = 0; i != 4; ++i)
        if (a[i] >= b)
            return false;
    return true;
}
}

namespace bbp
{
namespace osgTransparency
{
namespace test
{
namespace
{
unsigned int getChannels(const osg::Image &image)
{
    switch (image.getPixelFormat())
    {
    case GL_RGBA:
        return 4;
    case GL_RGB:
        return 3;
    case GL_RG:
        return 2;
    case GL_RED:
    case GL_GREEN:
    case GL_BLUE:
    case GL_ALPHA:
        return 1;
    default:
        abort();
    }
}

/** Return the per channel root mean square difference between two images.
    Assumes that images have the same size, pixel format and data type */
template <typename T>
osg::Vec4 perChannelRMS(const osg::Image &image1, const osg::Image &image2)
{
    osg::Vec4 rms(0, 0, 0, 0);

    for (int k = 0; k < image1.r(); ++k)
    {
        for (int j = 0; j < image1.t(); ++j)
        {
            for (int i = 0; i < image1.s(); ++i)
            {
                const T *data1 = (T *)image1.data(i, j, k);
                const T *data2 = (T *)image2.data(i, j, k);
                for (unsigned int c = 0; c != getChannels(image1); ++c)
                {
                    rms[c] += std::pow(data1[c] - data2[c], 2);
                }
            }
        }
    }

    for (unsigned int i = 0; i != 4; ++i)
        rms[i] = std::sqrt(rms[i]);
    return rms;
}
}

bool compare(const osg::Image &image1, const osg::Image &image2,
             const float channelThreshold)
{
    BOOST_CHECK_EQUAL(image1.getPixelFormat(), image2.getPixelFormat());
    BOOST_CHECK_EQUAL(image1.getDataType(), image2.getDataType());
    BOOST_CHECK_EQUAL(image1.s(), image2.s());
    BOOST_CHECK_EQUAL(image1.t(), image2.t());
    BOOST_CHECK_EQUAL(image1.r(), image2.r());
    osg::Vec4 channelRMS;
    switch (image1.getDataType())
    {
    case GL_UNSIGNED_BYTE:
        channelRMS = perChannelRMS<GLubyte>(image1, image2);
        break;
    case GL_FLOAT:;
        channelRMS = perChannelRMS<float>(image1, image2);
        break;
    default:
        abort();
    }
    return channelRMS < channelThreshold;
}

bool compare(const std::string &referenceFile, const int x, const int y,
             const int width, const int height, const GLenum pixelFormat,
             const GLenum type, const float channelThreshold)
{
    boost::filesystem::path path(SOURCE_PATH);
    path /= "tests/unit/img/" + referenceFile;
    BOOST_CHECK(boost::filesystem::is_regular_file(path));

    osg::ref_ptr<osg::Image> reference = osgDB::readImageFile(path.string());
    BOOST_CHECK(reference);

    osg::ref_ptr<osg::Image> image = new osg::Image();
    image->readPixels(x, y, width, height, pixelFormat, type);

    if (::getenv("KEEP_IMAGES") != 0)
    {
        osgDB::writeImageFile(*image, path.filename().string());
    }

    return compare(*reference, *image, channelThreshold);
}

bool compare(const std::string &referenceFile, const osg::Viewport &viewport,
             const GLenum pixelFormat, const GLenum type,
             const float channelThreshold)
{
    return compare(referenceFile, viewport.x(), viewport.y(), viewport.width(),
                   viewport.height(), pixelFormat, type, channelThreshold);
}
}
}
}
