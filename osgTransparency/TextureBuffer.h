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

#ifndef TRANSPARENCY_TEXTUREBUFFER_H
#define TRANSPARENCY_TEXTUREBUFFER_H

#include <osg/BufferObject>
#include <osg/Image>
#include <osg/Texture>

#include <stdlib.h>

namespace bbp
{
namespace osgTransparency
{
/**
   This class is an alternative of osg::TextureBuffer that allows creation
   of textures without a backing osg::Image.
   To allocate the texture this class expects the user to use a subload
   callback.
*/
class TextureBuffer : public osg::Texture
{
public:
    /*--- Public declarations ---*/

    class SubloadCallback : public Referenced
    {
    public:
        virtual void load(const TextureBuffer&, osg::State&) const = 0;
        virtual void subload(const TextureBuffer&, osg::State&) const = 0;
    };

    /*-- Public constructors/destructor ---*/

    TextureBuffer();

    /** Copy constructor using CopyOp to manage deep vs shallow copy. */
    TextureBuffer(const TextureBuffer& other,
                  const osg::CopyOp& copyOp = osg::CopyOp::SHALLOW_COPY);

    virtual ~TextureBuffer();

    /*--- Pubic member functions ---*/

    META_StateAttribute(osg, TextureBuffer, TEXTURE);

    virtual int compare(const StateAttribute&) const { return -1; }
    virtual bool getModeUsage(StateAttribute::ModeUsage&) const
    {
        return false;
    }

    virtual GLenum getTextureTarget() const;

    /** Set the image */
    void setImage(osg::Image* image)
    {
        _image = image;
        _modifiedCount.setAllElementsTo(0);
    }

    /** Gets the texture image */
    virtual osg::Image* getImage() { return _image.get(); }
    /** Gets the const texture image */
    virtual const osg::Image* getImage() const { return _image.get(); }
    /** Sets the texture image, ignoring face. */
    virtual void setImage(unsigned int, osg::Image* image) { setImage(image); }
    /** Gets the texture image, ignoring face. */
    virtual osg::Image* getImage(unsigned int) { return _image.get(); }
    /** Gets the const texture image, ignoring face. */
    virtual const osg::Image* getImage(unsigned int) const
    {
        return _image.get();
    }

    /** Gets the number of images that can be assigned to the Texture. */
    virtual unsigned int getNumImages() const { return 1; }
    void setSubloadCallback(SubloadCallback* callback)
    {
        _subloadCallback = callback;
    }

    SubloadCallback* getSubloadCallback() { return _subloadCallback.get(); }
    const SubloadCallback* getSubloadCallback() const
    {
        return _subloadCallback.get();
    }

    virtual int getTextureWidth() const { return _textureWidth; }
    virtual int getTextureHeight() const { return 1; }
    virtual int getTextureDepth() const { return 1; }
    void setTextureWidth(int width) const { _textureWidth = width; }
    /** Texture is a pure virtual base class, apply must be overridden. */
    virtual void apply(osg::State& state) const;

    osg::GLBufferObject* getGLBufferObject(unsigned int contextID);

protected:
    /*--- Protected member functions ---*/
    virtual void computeInternalFormat() const;

    virtual void allocateMipmap(osg::State&) const {}
private:
    /*--- Private declarations ---*/

    class TextureBufferObject;

    /*--- Private member variables ---*/

    osg::ref_ptr<osg::Image> _image;
    osg::ref_ptr<SubloadCallback> _subloadCallback;

    mutable osg::ref_ptr<TextureBufferObject> _textureBufferObject;

    typedef osg::buffered_value<unsigned int> ImageModifiedCount;
    mutable ImageModifiedCount _modifiedCount;

    mutable GLsizei _textureWidth;

    /*--- Private member functions ---*/
    void _bindImageTexture(osg::State& state,
                           TextureObject* textureObject) const;
};
}
}
#endif
