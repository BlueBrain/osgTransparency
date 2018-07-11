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

#include "TextureBuffer.h"

#include "util/glerrors.h"

#include <osg/GLExtensions>
#include <osg/Geometry>
#include <osg/buffered_value>

#include <iostream>

namespace bbp
{
namespace osgTransparency
{
class TextureBuffer::TextureBufferObject : public osg::BufferObject
{
public:
    TextureBufferObject()
    {
        setTarget(GL_TEXTURE_BUFFER);
        setUsage(GL_DYNAMIC_DRAW);
    }

    TextureBufferObject(const TextureBufferObject& other,
                        const osg::CopyOp& copyOp = osg::CopyOp::SHALLOW_COPY)
        : osg::BufferObject(other, copyOp)
    {
    }

    META_Object(osgTransparency, TextureBufferObject);

    void bind(unsigned int contextID)
    {
        osg::GLBufferObject* buffer = getOrCreateGLBufferObject(contextID);
        buffer->bindBuffer();
    }

    void unbind(unsigned int contextID)
    {
        osg::GLBufferObject* buffer = getOrCreateGLBufferObject(contextID);
        buffer->unbindBuffer();
    }

    void bindTextureBuffer(osg::State& state, GLenum internalFormat)
    {
        unsigned int contextID = state.getContextID();
        osg::GLBufferObject* buffer = getOrCreateGLBufferObject(contextID);

        const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();

        extensions->glTexBuffer(GL_TEXTURE_BUFFER, internalFormat,
                                buffer->getGLObjectID());
    }

    void bufferData(osg::State& state, osg::Image* image)
    {
        const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();
        extensions->glBufferData(GL_TEXTURE_BUFFER,
                                 image->getImageSizeInBytes(), image->data(),
                                 getUsage());
    }

    void bufferSubData(osg::State& state, osg::Image* image)
    {
        const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();
        extensions->glBufferSubData(GL_TEXTURE_BUFFER, 0,
                                    image->getImageSizeInBytes(),
                                    image->data());
    }
};

/*
  Constructors/destructor
*/
TextureBuffer::TextureBuffer()
    : _textureWidth(0)
{
}

TextureBuffer::TextureBuffer(const TextureBuffer& other,
                             const osg::CopyOp& copyOp)
    : osg::Texture(other)
    , _image(copyOp(other._image.get()))
    , _textureWidth(other._textureWidth)
{
    abort(); // just in case
}

TextureBuffer::~TextureBuffer()
{
}

/*
  Member functions
*/
GLenum TextureBuffer::getTextureTarget() const
{
    return GL_TEXTURE_BUFFER;
}

void TextureBuffer::apply(osg::State& state) const
{
#if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE)
    const unsigned int contextID = state.getContextID();

    TextureObject* textureObject = getTextureObject(contextID);

    const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();

    if (textureObject)
    {
        if (_subloadCallback.valid())
        {
            /* Update the texture buffer */
            _textureBufferObject->bind(contextID);
            _subloadCallback->subload(*this, state);
            _textureBufferObject->unbind(contextID);
        }
        else if (_image.valid() &&
                 _modifiedCount[contextID] != _image->getModifiedCount())
        {
            /* Update the texture buffer */
            _textureBufferObject->bind(contextID);
            _textureBufferObject->bufferSubData(state, _image);

            _textureBufferObject->unbind(contextID);
            /* Update the modified tag to show that it is up to date. */
            _modifiedCount[contextID] = _image->getModifiedCount();
        }

        /* Binding the texture and its texture buffer object as texture
           storage. */
        textureObject->bind(state);
        _textureBufferObject->bindTextureBuffer(state, _internalFormat);
        _bindImageTexture(state, textureObject);
    }
    else if ((_image.valid() && _image->data()) || _subloadCallback.valid())
    {
        /* Creating the texture object and the TBO */
        _textureObjectBuffer[contextID] = textureObject =
            generateTextureObject(this, contextID, GL_TEXTURE_BUFFER);
        _textureBufferObject = new TextureBufferObject();

        if (_image.valid() && _image->data())
        {
            /* Temporary copy */
            osg::ref_ptr<osg::Image> image = _image;
            /* Compute the internal texture format,
               this set the _internalFormat to an appropriate value. */
            computeInternalFormat();
            /* Binding the TBO and copying data */
            _textureBufferObject->bind(contextID);
            _textureBufferObject->bufferData(state, image);
            _textureWidth = image->s();
            textureObject->setAllocated(true);
            _textureBufferObject->unbind(contextID);

            /* Update the modified tag. */
            _modifiedCount[contextID] = image->getModifiedCount();
        }
        else
        {
            _textureBufferObject->bind(contextID);
            _subloadCallback->load(*this, state);
            textureObject->setAllocated(true);
            _textureBufferObject->unbind(contextID);
        }
        textureObject->bind(state);
        _textureBufferObject->bindTextureBuffer(state, _internalFormat);
        _bindImageTexture(state, textureObject);
    }
    else
    {
        /* This texture type is input only (as far as I'm concerned), so
           it doesn't work without an attached image. */
        extensions->glBindBuffer(GL_TEXTURE_BUFFER, 0);
        glBindTexture(GL_TEXTURE_BUFFER, 0);
    }
#else
    OSG_NOTICE << "Warning: TextureBuffer::apply(State& state) not supported."
               << std::endl;
#endif
}

osg::GLBufferObject* TextureBuffer::getGLBufferObject(unsigned int contextID)
{
    if (_textureBufferObject)
        return _textureBufferObject->getGLBufferObject(contextID);
    else
        return 0;
}

void TextureBuffer::computeInternalFormat() const
{
    if (_internalFormatMode != USE_USER_DEFINED_FORMAT)
    {
        if (_internalFormatMode == USE_IMAGE_DATA_FORMAT)
        {
            if (_image.valid())
                _internalFormat = _image->getInternalTextureFormat();
        }
        else
        {
            std::cerr << "Unsupported internal format" << std::endl;
        }
    }
    computeInternalFormatType();
}

void TextureBuffer::_bindImageTexture(osg::State& state,
                                      TextureObject* textureObject) const
{
    const unsigned int contextID = state.getContextID();
    if (getTextureParameterDirty(contextID))
    {
        const osg::GLExtensions* extensions = state.get<osg::GLExtensions>();
        if (extensions->isBindImageTextureSupported() &&
            _imageAttachment.access != 0)
        {
            extensions->glBindImageTexture(
                _imageAttachment.unit, textureObject->id(),
                _imageAttachment.level, _imageAttachment.layered,
                _imageAttachment.layer, _imageAttachment.access,
                _imageAttachment.format != 0 ? _imageAttachment.format
                                             : _internalFormat);
        }
        getTextureParameterDirty(contextID) = false;
    }
}
}
}
