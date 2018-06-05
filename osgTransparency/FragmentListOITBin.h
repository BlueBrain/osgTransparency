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

#ifndef OSGTRANSPARENCY_FRAGMENTLISTOITBIN_H
#define OSGTRANSPARENCY_FRAGMENTLISTOITBIN_H

#include <osg/GL>

#ifdef OSG_GL3_AVAILABLE

#include "BaseParameters.h"
#include "BaseRenderBin.h"

#include <boost/function.hpp>

namespace osg
{
class TextureRectangle;
}

namespace bbp
{
namespace osgTransparency
{
class TextureBuffer;
class FragmentData;

/**
   Callback invoked after the render pass that captures fragments is finished.

   The textures are not guaranteed to be up to date, it is the client's
   responsibility to call glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT) if
   nedeed.

   The function must return true if the implementation has to proceed with
   fragment sorting and displaying after being invoked, and false otherwise.
*/
typedef boost::function<bool(const FragmentData& data)> CaptureCallback;

class OSGTRANSPARENCY_API FragmentListOITBin : public BaseRenderBin
{
public:
    /*--- Public declarations ---*/

    class _Impl;

    struct OSGTRANSPARENCY_API Parameters : public BaseRenderBin::Parameters
    {
        Parameters();

        Parameters(const Parameters& other);

        ~Parameters();

        /** Sets a callback to be called after the fragments have been
            captured when this render bin is used inside a graphics context.

            This function is thread-safe to ensure different graphics contexts
            can use different threads, but for a given context, clients
            shouldn't modify the callback during the render traversal,
            otherwise whether the new or old callback gets called is undefined.

            @sa CaptureCallback.
            @version 0.8.0
        */
        void setCaptureCallback(unsigned int contextID,
                                const CaptureCallback& callback);

        /** Enable fragment list cut-off when the opacity is detected to
            accumulate over the given threshold.

            Only fragments whose depth is above the depth of the fragment at
            which alpha is detected to cross the threshold are discarded.
            The alpha accumulation is performed using an orderindependent
            formula, so how effective this optimization is is very scene
            dependent.

            @param threshold The alpha threshold. This value must be in (0, 1].
                   Due to the limited precision of the implementation, setting
                   this value to 1 may not be very effective on some scenes.
            @version 0.8.0
        */
        void enableAlphaCutOff(float threshold);

        void disableAlphaCutOff();

        /** @sa BaseRenderBin::Parameters::update */
        virtual bool update(const Parameters& other);

    private:
        bool isAlphaCutOffEnabled() const;

        friend class FragmentListOITBin::_Impl;
        class _Impl;
        _Impl* _impl;
    };

    FragmentListOITBin(const Parameters& parameters = Parameters());

    FragmentListOITBin(const FragmentListOITBin& renderBin,
                       const osg::CopyOp& copyop);

    /* Member fucntions */
public:
    META_Object(osgTransparency, FragmentListOITBin);

    const Parameters& getParameters() const
    {
        return static_cast<Parameters&>(*_parameters);
    }

    Parameters& getParameters()
    {
        return static_cast<Parameters&>(*_parameters);
    }

    virtual void sort() {}
protected:
    /*--- Protected member functions ---*/

    virtual void drawImplementation(osg::RenderInfo& renderInfo,
                                    osgUtil::RenderLeaf*& previous);
};

/**
   This class provides the methods to access to information about fragments
   captured during the rendering pass.
 */
class OSGTRANSPARENCY_API FragmentData
{
public:
    friend class FragmentListOITBin::_Impl;

    osg::State& getState() const;
    osg::TextureRectangle* getCounts() const;
    osg::TextureRectangle* getHeads() const;
    TextureBuffer* getFragments() const;

    /*
      Return the total number of fragments captured during rendering.

      This function requires a GPU readback which has been measured to take
      in the order of 10ths of us of CPU time in the tested hardware, assuming
      that the operation doesn't have to wait for the fragment capture to be
      finished.
    */
    OSGTRANSPARENCY_API size_t getNumFragments() const;

private:
    OSGTRANSPARENCY_API FragmentData(osg::State* state, void* data);
    OSGTRANSPARENCY_API FragmentData(const FragmentData&);
    OSGTRANSPARENCY_API FragmentData& operator=(const FragmentData&);

    osg::State* _state;
    /* Pimpl with unspecified type. */
    void* _data;
};

bool extractFragmentStatistics(const FragmentData& data);

void writeTextures(const std::string& filename, const FragmentData& data);
}
}

#endif

#endif
