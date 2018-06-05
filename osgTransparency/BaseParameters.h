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

#ifndef TRANSPARENCY_BASEPARAMETERS_H
#define TRANSPARENCY_BASEPARAMETERS_H

#include "BaseRenderBin.h"

namespace bbp
{
namespace osgTransparency
{
class OSGTRANSPARENCY_API BaseRenderBin::Parameters
{
public:
    /*--- Public declarations ---*/

    template <typename T>
    class Optional
    {
    public:
        Optional()
            : _value()
            , _valid(false)
        {
        }
        /* Convenience constructor used to reduce typing length when an
           optional parameter needs to be left undefined in the middle
           of a function call NULL or (void*)0 can be used to select this
           conversion constructor. */
        Optional(const void *)
            : _value()
            , _valid(false)
        {
        }
        /* non explicit on purpose */
        Optional(const T &v)
            : _value(v)
            , _valid(true)
        {
        }
        operator T() const
        {
            assert(_valid);
            return _value;
        }
        bool valid() const { return _valid; }
    private:
        const T _value;
        const bool _valid;
    };
    typedef Optional<unsigned int> OptUInt;
    typedef Optional<bool> OptBool;
    typedef Optional<float> OptFloat;

    /*--- Public attributes ---*/

    /** Maximum number of peeling passes for multi-pass algorithms.
        0 for unlimited passes. */
    unsigned int maximumPasses;
    /** When the number of samples that have passed through the graphics
        pipeline is less or equal to this number no more passes are
        performed. */
    unsigned int samplesCutoff;
    /** Index of the first texture unit that can be used by the algorithms
        in steps that depend on user given shaders. */
    unsigned int reservedTextureUnits;
    const unsigned int superSampling; //!< @todo

    bool singleQueryPerPass; //!< @internal

    /*--- Public constructors/detructor ---*/

    /**
       Creates a Parameters object with the following default values:
       * maximumPasses: 100
       * samplesCutoff: 0
    */
    Parameters(OptUInt superSampling = 1);

    virtual ~Parameters() {}
    /** @internal */
    /* Updates these parameters with the attributes from other object unless
       they are incompatible.
       Parameters are compatible when the const declared attributes are equal
       (this options require shared recompilation and buffer reallocation to
       be changed).
       @return True if objects are compatible and attributes updated.
    */
    bool update(const Parameters &other);

    virtual boost::shared_ptr<Parameters> clone() const;
};
}
}
#endif
