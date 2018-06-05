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

#ifndef OSGTRANSPARENCY_DEPTHPEELINGBIN_H
#define OSGTRANSPARENCY_DEPTHPEELINGBIN_H

#include "BaseRenderBin.h"

namespace bbp
{
namespace osgTransparency
{
class OSGTRANSPARENCY_API DepthPeelingBin : public BaseRenderBin
{
public:
    /*--- Public constructors/destructor ---*/

    DepthPeelingBin();

    DepthPeelingBin(const Parameters &parameters);

    DepthPeelingBin(const DepthPeelingBin &renderBin,
                    const osg::CopyOp &copyop);

    /*--- Public member functions ---*/

    META_Object(osg, DepthPeelingBin);

    const Parameters &getParameters() const { return *_parameters; }
    Parameters &getParameters() { return *_parameters; }
    virtual void sort() {}
    /*--- Protected member functions ---*/
protected:
    virtual void drawImplementation(osg::RenderInfo &renderInfo,
                                    osgUtil::RenderLeaf *&previous);

private:
    /*--- Private declarations ---*/
    class _Impl;
};
}
}
#endif
