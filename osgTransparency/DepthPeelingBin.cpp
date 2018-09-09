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

// Including GL headers for various OpenGL tokens
#ifdef _WIN32
#define GL_GLEXT_PROTOTYPES
#include <osg/GL>

#include <GL/glext.h>
#endif

#ifdef __APPLE__
#define GL_RGBA32F 0x8814
#endif

#include "BaseParameters.h"
#include "DepthPeelingBin.h"

#include "util/constants.h"
#include "util/extensions.h"
#include "util/helpers.h" // Before including boost
#include "util/strings_array.h"
#include "util/trace.h"

#include <osg/Version>

#include <osg/BlendEquation>
#include <osg/BlendFunc>
#include <osg/ColorMask>
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 0)
#include <osg/ContextData>
#endif
#include <osg/Depth>
#include <osg/FrameBufferObject>
#include <osg/GL2Extensions>
#include <osg/Scissor>
#include <osg/TextureRectangle>
#include <osgDB/WriteFile>
#include <osgUtil/RenderLeaf>
#include <osgUtil/StateGraph>
#ifndef NDEBUG
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#endif

#include <boost/format.hpp>

#include <iostream>
#include <sstream>

#ifdef WIN32
#undef near
#undef far
#endif

namespace bbp
{
namespace osgTransparency
{
namespace
{
/*
  Static definitions and constants
*/
struct DebugPartition
{
    DebugPartition()
    {
        row = -1, col = -1;
        if (::getenv("OSGTRANSPARENCY_DEBUG_PARTITION") != 0)
        {
            char* endptr;
            col = strtol(::getenv("OSGTRANSPARENCY_DEBUG_PARTITION"), &endptr,
                         10);
            row = strtol(endptr + 1, &endptr, 10);
        }
    }
    operator bool() { return row != -1 && col != -1; }
    int row;
    int col;
} s_debugPartition;

#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 0)
class QueryObjectManager : public osg::GLObjectManager
{
public:
    QueryObjectManager(unsigned int contextID)
        : osg::GLObjectManager("osgTransparency::QueryObjectManager", contextID)
    {
    }

    virtual void deleteGLObject(GLuint handler)
    {
        const osg::GLExtensions* ext = osg::GLExtensions::Get(_contextID, true);
        ext->glDeleteQueries(1, &handler);
    }
};
#endif

/* Some declarations used to enhance readbility */
typedef std::map<GLenum, int> Modes;
typedef std::map<osg::ref_ptr<osg::StateAttribute>, int> Attributes;
typedef std::set<osg::ref_ptr<osg::Uniform>> Uniforms;
typedef std::map<const osg::StateSet*, osg::ref_ptr<osg::Program>> ProgramMap;

/*
  Helper functions
*/
typedef std::list<osg::ref_ptr<osg::TextureRectangle>> TextureList;

void _filterSmallerTextures(TextureList& textures, const int width,
                            const int height)
{
    TextureList::iterator i = textures.begin();
    while (i != textures.end())
    {
        osg::ref_ptr<osg::Texture> texture = *i;
        if (texture->getTextureHeight() < height ||
            texture->getTextureWidth() < width)
        {
            textures.erase(i++);
        }
        else
            ++i;
    }
}
}

/*
  Helper classes
*/
class DepthPeelingBin::_Impl
{
public:
    /*--- Public declarations ---*/

    class Context;
    class Screen;
    class Tile;

    /*--- Public  member functions ---*/

    static Context& getContext(const osg::State& state,
                               const Parameters& parameters);

private:
    /*--- Private member variables ---*/

    static std::map<unsigned int, boost::shared_ptr<Context>> s_context;
};

std::map<unsigned int, boost::shared_ptr<DepthPeelingBin::_Impl::Context>>
    DepthPeelingBin::_Impl::s_context;

class DepthPeelingBin::_Impl::Tile : public osg::Referenced
{
public:
    /*--- Public constructors/destructor ---*/

    /**
       \param x The x coordinate of the tile in screen space (left is 0)
       \param y The y coordinate of the tile in screen space (top is 0)
       \param width The width in screen spapce
       \param height The height in screen spapce
       \param padX The horizontal padding in screen dimensions
       \param padY The vertical padding in screen dimensions
     */
    Tile(Screen* screen, unsigned int row, unsigned int column, unsigned int x,
         unsigned int y, unsigned int width, unsigned int height,
         unsigned int padX, unsigned int padY);

    ~Tile();

    /*--- Public member functions ---*/

    /**
       Activates the tile for rendering and performs the first pass.
       Initialization requires the creation of the specific state sets for this
       tile (if not already created), adquisition of the textures needed,
       preparation of FBO objects.
     */
    bool init(DepthPeelingBin* bin, osg::RenderInfo& renderInfo,
              osgUtil::RenderLeaf*& previous);

    void peel(DepthPeelingBin* bin, osg::RenderInfo& renderInfo);

    void blend(osg::RenderInfo& renderInfo);

    void clampProjectionToNearFar();

protected:
    /*--- Protected member functions ---*/

    void finish();

    void createStateSets();

    void preparePeelFBOAndTextures(osg::State& state);

    void writeLayers(bool writeColor = false);

private:
    /*--- Private member variables ---*/

    Screen* _screen;

    osg::ref_ptr<osg::RefMatrix> _projection;
    osg::ref_ptr<osg::Scissor> _scissor;
    osg::ref_ptr<osg::Viewport> _viewport;

    unsigned int _row;
    unsigned int _column;

    /* The screen position of this tile */
    unsigned int _x;
    unsigned int _y;
    /* The final screen area covered by this tile */
    unsigned int _width;
    unsigned int _height;
    /* The padding this tile needs in screen coordinates */
    /** \todo We would need to extend the padding to both sides in order
        to improve the AA filtering once implemented. */
    unsigned int _padX;
    unsigned int _padY;

    osg::ref_ptr<osg::FrameBufferObject> _peelFBO;
    osg::ref_ptr<osg::FrameBufferObject> _auxFBO;
    /* The ping-pong textures with the depth maps */
    osg::ref_ptr<osg::TextureRectangle> _depthTextures[2];
    osg::ref_ptr<osg::TextureRectangle> _colorTexture;
    int _index;

    /* The offset and scaling that transform offscreen cordinates of this tile
       to onscreen coordinates. */
    osg::ref_ptr<osg::Uniform> _screenOffset;
    osg::ref_ptr<osg::Uniform> _screenScaling;
    osg::ref_ptr<osg::Uniform> _inverseScreenScaling;

    osg::ref_ptr<osg::StateSet> _firstPassStateSet;
    osg::ref_ptr<osg::StateSet> _peelStateSet;
    osg::ref_ptr<osg::StateSet> _blendStateSet;

    GLuint _query;
    GLuint _lastSamplesPassed;
    GLuint _timesSamplesRepeated;

    unsigned int _passes;
};

class DepthPeelingBin::_Impl::Screen : public osg::Referenced
{
public:
    /*--- Public declarations ---*/

    typedef std::list<Tile*> TileList;

    friend class Tile;

    /*--- Public constructor/destructor ---*/

    Screen(osg::RenderInfo& renderInfo, Context* context);

    /*--- Public member functions ---*/

    bool valid(const osg::Camera* camera)
    {
        float w = camera->getViewport()->width();
        float h = camera->getViewport()->height();
        float previousWidth = -1, previousHeight = -1;

        return (_camera == camera &&
                /* Testing if camera size has changed */
                _cameraWidth->get(previousWidth) &&
                _cameraHeight->get(previousHeight) && w == previousWidth &&
                h == previousHeight);
    }

    void startFrame()
    {
        assert(_unissuedTiles.empty());
        assert(_activeTiles.empty());
        for (std::vector<osg::ref_ptr<Tile>>::iterator tile = _tiles.begin();
             tile != _tiles.end(); ++tile)
        {
            _unissuedTiles.push_back(tile->get());
            (*tile)->clampProjectionToNearFar();
        }
    }

    Tile* getUnissuedTile()
    {
        return !_unissuedTiles.empty() ? _unissuedTiles.front() : 0;
    }

    TileList& getUnfinishedTiles() { return _activeTiles; }
    Context& getContext() { return *_context; }
    osg::Camera* getCamera() { return _camera; }
    unsigned int getWidth() const
    {
        return static_cast<unsigned int>(_camera->getViewport()->width());
    }

    unsigned int getHeight() const
    {
        return static_cast<unsigned int>(_camera->getViewport()->height());
    }

    void getProjectionMatrixUniforms(Uniforms& uniforms)
    {
        uniforms.insert(_projection_33);
        uniforms.insert(_projection_34);
    }

    osg::Viewport* getTileViewport() { return _offscreenViewport.get(); }
protected:
    /*--- Protected destructor ---*/
    ~Screen() {}
private:
    /*--- Private member variables ---*/

    Context* _context;

    osg::Camera* _camera;
    unsigned int _rows;
    unsigned int _columns;
    std::vector<osg::ref_ptr<Tile>> _tiles;

    TileList _unissuedTiles;
    TileList _activeTiles;

    osg::ref_ptr<osg::Uniform> _cameraWidth;
    osg::ref_ptr<osg::Uniform> _cameraHeight;
    osg::ref_ptr<osg::Uniform> _projection_33;
    osg::ref_ptr<osg::Uniform> _projection_34;

    osg::ref_ptr<osg::Viewport> _offscreenViewport;
};

class DepthPeelingBin::_Impl::Context
{
public:
    /*--- Public declarations ---*/

    typedef std::list<osg::ref_ptr<osg::TextureRectangle>> TextureList;

    /*--- Public member variables ---*/

    Parameters parameters;

    osg::ref_ptr<osg::TextureRectangle> targetBlendColorTexture;
    osg::ref_ptr<osg::StateSet> baseBlendStateSet;
    osg::ref_ptr<osg::FrameBufferObject> blendBuffer;

    osg::ref_ptr<osg::StateSet> baseFirstPassStateSet;
    ProgramMap firstPassPrograms;

    osg::ref_ptr<osg::StateSet> basePeelStateSet;
    ProgramMap peelPassPrograms;

    osg::ref_ptr<osg::StateSet> finalStateSet;

    osg::ref_ptr<osg::FrameBufferObject> auxiliaryBuffer;

    osg::ref_ptr<osg::Geometry> quad;

    osgUtil::RenderLeaf* oldPrevious;

    /*--- Public constructor/destructor ---*/

    Context(unsigned int id, const Parameters& param)
        : parameters(param)
        , quad(createQuad())
        , oldPrevious(0)
        , _id(id)
        , _savedStackPosition(0)
    {
    }

    /*--- Public member functions ---*/

    Screen* getScreen() { return _screen.get(); }
    unsigned int getID() const { return _id; }
    bool updateParameters(const Parameters& param)
    {
        return parameters.update(param);
    }

    void createBuffersAndTextures();

    void createBaseStateSets();

    void updatePrograms(const ProgramMap& extraShaders);

    void startFrame(DepthPeelingBin* bin, osg::RenderInfo& renderInfo,
                    osgUtil::RenderLeaf*& previous);

    void finishFrame(osg::RenderInfo& renderInfo,
                     osgUtil::RenderLeaf*& previous);

    bool extractTextures(size_t count, GLenum format, TextureList& texture);

    void returnTextures(TextureList& textures)
    {
        TextureList* list = 0;
        GLint format = 0;
        while (!textures.empty())
        {
            TextureList::iterator t = textures.begin();
            if (list == 0 || format != (*t)->getInternalFormat())
            {
                format = (*t)->getInternalFormat();
                list = &_freeTextures[format];
            }
            list->splice(list->begin(), textures, t);
        }
    }

private:
    /*--- Private member variables ---*/

    ProgramMap _extraShaders;

    osg::ref_ptr<osg::Uniform> _lowerLeftCornerUniform;

    unsigned int _id;
    osg::ref_ptr<Screen> _screen;

    std::map<GLenum, TextureList> _freeTextures;

    GLint _previousFBO;
    unsigned int _savedStackPosition;

#ifndef NDEBUG
    osg::ref_ptr<osg::StateSet> _oldState;
#endif
};

/*
  Tile definitions
*/

DepthPeelingBin::_Impl::Tile::Tile(Screen* screen, unsigned int row,
                                   unsigned int column, unsigned int x,
                                   unsigned int y, unsigned int width,
                                   unsigned int height, unsigned int padX,
                                   unsigned int padY)
    : _screen(screen)
    , _row(row)
    , _column(column)
    , _x(x)
    , _y(y)
    , _width(width)
    , _height(height)
    , _padX(padX)
    , _padY(padY)
    , _index(0)
    , _query(0)
    , _passes(0)
{
    unsigned int screenWidth = _screen->getWidth();
    unsigned int screenHeight = _screen->getHeight();

    /* Computing normalized tile height, width and position. */
    const double nX = _x / (double)screenWidth;
    const double nY = _y / (double)screenHeight;
    const double nPaddedWidth = (_width + _padX) / (double)screenWidth;
    const double nPaddedHeight = (_height + _padY) / (double)screenHeight;

    double left, right, bottom, top, near, far;
    _screen->getCamera()->getProjectionMatrix().getFrustum(left, right, bottom,
                                                           top, near, far);
    _projection = new osg::RefMatrix();
    _projection->makeFrustum(left + nX * (right - left),
                             left + (nX + nPaddedWidth) * (right - left),
                             bottom + nY * (top - bottom),
                             bottom + (nY + nPaddedHeight) * (top - bottom),
                             near, far);
    _viewport = new osg::Viewport(*_screen->getTileViewport());

    /* Transforming the padding to offscreen dimensions */
    const unsigned int offscreenWidth = _viewport->width();
    const unsigned int offscreenHeight = _viewport->height();

    const unsigned int offPadX = (unsigned int)osg::round(
        (offscreenWidth * padX) / (double)(padX + width));
    const unsigned int offPadY = (unsigned int)osg::round(
        (offscreenHeight * padY) / (double)(padY + height));
    /* This will discard the fragments. The view frustum culling algorithm
       should remove geometry outside this area because the projection matrix
       used for rendering has been enlarged to account for the padding */
    _scissor = new osg::Scissor(0, 0, offscreenWidth - offPadX,
                                offscreenHeight - offPadY);

    /* Creating common uniforms for this tile's shaders */
    const double xScale =
        (width == offscreenWidth ? 1 : (padX + width) / (double)offscreenWidth);
    const double yScale =
        (height == offscreenHeight ? 1
                                   : (padY + height) / (double)offscreenHeight);
    _screenOffset = new osg::Uniform("offset", osg::Vec2(_x, _y));
    _screenScaling = new osg::Uniform("scaling", osg::Vec2(xScale, yScale));
    _inverseScreenScaling =
        new osg::Uniform("inverseScaling", osg::Vec2(1 / xScale, 1 / yScale));
}

DepthPeelingBin::_Impl::Tile::~Tile()
{
/* The query object is leaked in older versions of OSG. */
#if OSG_VERSION_GREATER_OR_EQUAL(3, 5, 0)
    osg::get<QueryObjectManager>(_screen->getContext().getID())
        ->scheduleGLObjectForDeletion(_query);
#endif
}

void DepthPeelingBin::_Impl::Tile::createStateSets()
{
    /* Getting the context to shallow copy the base state sets from it */
    Context& c = _screen->getContext();

    Uniforms uniforms;
    Attributes attributes;
    using namespace keywords;

    /* First pass */
    _firstPassStateSet = new osg::StateSet(*c.baseFirstPassStateSet);
    /* The first pass needs the scaling and offset uniforms to convert
       offscreen coordinates to onscreen. */
    uniforms.insert(_screenOffset);
    uniforms.insert(_screenScaling);
    attributes[_viewport.get()] = ON_OVERRIDE_PROTECTED;
    attributes[_scissor.get()] = ON;
    setupStateSet(_firstPassStateSet.get(), _uniforms = uniforms,
                  _attributes = attributes);

    /* Peel pass */
    _peelStateSet = new osg::StateSet(*c.basePeelStateSet);
    uniforms.clear();
    attributes.clear();
    uniforms.insert(_screenOffset);
    uniforms.insert(_screenScaling);
    attributes[_viewport.get()] = ON_OVERRIDE_PROTECTED;
    attributes[_scissor.get()] = ON;
    setupStateSet(_peelStateSet.get(), _uniforms = uniforms,
                  _attributes = attributes);

    /* Blend pass */
    /* The blend pass needs the offset uniforms to convert onscreen
       coordinates to offscreen. */
    _blendStateSet = new osg::StateSet(*c.baseBlendStateSet);
    uniforms.clear();
    attributes.clear();
    attributes[new osg::Viewport(_x, _y, _width + _padX, _height + _padY)] =
        ON_OVERRIDE_PROTECTED;
    attributes[new osg::Scissor(_x, _y, _width, _height)] = ON;
    uniforms.insert(_screenOffset);
    setupStateSet(_blendStateSet.get(), _uniforms = uniforms,
                  _attributes = attributes);
}

bool DepthPeelingBin::_Impl::Tile::init(DepthPeelingBin* bin,
                                        osg::RenderInfo& renderInfo,
                                        osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    DrawExtensions* ext = getDrawExtensions(state.getContextID());

    if (!_firstPassStateSet.valid())
        createStateSets();

    if (_query == 0)
        ext->glGenQueries(1, &_query);

    Context& context = _screen->getContext();
    /* Checking if there are available textures/buffers for this tile */
    Context::TextureList textures;
    bool failed = !context.extractTextures(2, GL_R32F, textures) ||
                  !context.extractTextures(1, GL_RGBA32F_ARB, textures);

    if (failed)
    {
        context.returnTextures(textures);
        return false;
    }

    if (_peelFBO.get() == 0)
    {
        _auxFBO = new osg::FrameBufferObject();
        _peelFBO = new osg::FrameBufferObject();
    }

    /* Setting the textures and buffers */
    Context::TextureList::iterator texture = textures.begin();
    _depthTextures[0] = *(texture++);
    _depthTextures[1] = *(texture++);
    _colorTexture = *texture;
    _blendStateSet->setTextureAttributeAndModes(0, _colorTexture.get());

    _index = 0;
    _timesSamplesRepeated = 0;
    _lastSamplesPassed = 0;
    _passes = 0;

    /* Setting color buffer in the peel FBO */
    _peelFBO->setAttachment(COLOR_BUFFERS[1],
                            osg::FrameBufferAttachment(_colorTexture.get()));

    /* Rendering first pass */

    /* Clearing the first depth buffer and the target blend buffer */
    _auxFBO->setAttachment(COLOR_BUFFERS[0],
                           osg::FrameBufferAttachment(_depthTextures[0].get()));
    _auxFBO->apply(state);
    glDrawBuffer(GL_BUFFER_NAMES[0]);
    glClearColor(-1.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    ext->glBeginQuery(GL_SAMPLES_PASSED_ARB, _query);
    bin->render(renderInfo, previous, _firstPassStateSet.get(),
                context.firstPassPrograms, _projection.get());
    ext->glEndQuery(GL_SAMPLES_PASSED_ARB);

#ifndef NDEBUG
    if (state.getFrameStamp()->getFrameNumber() == 2)
        writeLayers(false);
#endif

    _screen->_activeTiles.push_back(this);
    for (Screen::TileList::iterator i = _screen->_unissuedTiles.begin();
         i != _screen->_unissuedTiles.end(); ++i)
    {
        if (*i == this)
        {
            _screen->_unissuedTiles.erase(i);
            break;
        }
    }

    return true;
}

void DepthPeelingBin::_Impl::Tile::peel(DepthPeelingBin* bin,
                                        osg::RenderInfo& renderInfo)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    DrawExtensions* ext = getDrawExtensions(state.getContextID());

    /* Checking the result of the last peel pass (or the first pass) */
    GLuint samplesPassed = 0;
    ext->glGetQueryObjectuiv(_query, GL_QUERY_RESULT_ARB, &samplesPassed);

    bool finished =
        samplesPassed <= _screen->getContext().parameters.samplesCutoff ||
        (_screen->getContext().parameters.maximumPasses != 0 &&
         _passes >= _screen->getContext().parameters.maximumPasses);
    if (!finished && _lastSamplesPassed == samplesPassed)
    {
        if (++_timesSamplesRepeated == 5)
        {
            std::cerr << "Possible infinite loop peeling tile " << _row << ", "
                      << _column << ". Finishing" << std::endl;
            finished = true;
        }
    }
    _lastSamplesPassed = samplesPassed;

    if (finished)
    {
        finish();
        return;
    }

    /* Rendering */
    osgUtil::RenderLeaf* previous = _screen->getContext().oldPrevious;
    preparePeelFBOAndTextures(state);

    ext->glBeginQuery(GL_SAMPLES_PASSED_ARB, _query);
    bin->render(renderInfo, previous, _peelStateSet.get(),
                _screen->getContext().peelPassPrograms, _projection.get());
    ext->glEndQuery(GL_SAMPLES_PASSED_ARB);

#ifndef NDEBUG
    if (state.getFrameStamp()->getFrameNumber() == 2)
        writeLayers(true);
#endif

    /* Swapping buffer indexing for next pass */
    _index = 1 - _index;
    ++_passes;
}

void DepthPeelingBin::_Impl::Tile::blend(osg::RenderInfo& renderInfo)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();

    /* Blending of front to back layer */
    _screen->getContext().blendBuffer->apply(state);
    state.pushStateSet(_blendStateSet.get());
    state.apply();
    if (_passes == 1)
    {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    _screen->getContext().quad->draw(renderInfo);
    state.popStateSet();
}

void DepthPeelingBin::_Impl::Tile::clampProjectionToNearFar()
{
    const osg::Matrix& projection = _screen->getCamera()->getProjectionMatrix();
    (*_projection)(2, 2) = projection(2, 2);
    (*_projection)(3, 2) = projection(3, 2);
}

void DepthPeelingBin::_Impl::Tile::finish()
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    Context& context = _screen->getContext();
    Context::TextureList textures;
    for (int i = 0; i < 2; ++i)
    {
        textures.push_back(_depthTextures[i]);
#ifndef NDEBUG
        _depthTextures[i] = 0;
#endif
    }
    textures.push_back(_colorTexture);
#ifndef NDEBUG
    _colorTexture = 0;
#endif

    context.returnTextures(textures);
    /* Removing the tile from the active list */
    for (Screen::TileList::iterator i = _screen->_activeTiles.begin();
         i != _screen->_activeTiles.end(); ++i)
    {
        if (*i == this)
        {
            _screen->_activeTiles.erase(i);
            break;
        }
    }
}

void DepthPeelingBin::_Impl::Tile::preparePeelFBOAndTextures(osg::State& state)
{
    osg::GL2Extensions* gl2e =
        osg::GL2Extensions::Get(state.getContextID(), true);

    /* Setting up next depth buffers and depth textures */
    osg::FrameBufferAttachment depth(_depthTextures[1 - _index].get());
    _peelFBO->setAttachment(COLOR_BUFFERS[0], depth);

    /* We leave the first 4 textures for the shading of the model */
    const Parameters& parameters = _screen->getContext().parameters;
    _peelStateSet->setTextureAttributeAndModes(parameters.reservedTextureUnits,
                                               _depthTextures[_index].get());

    /* Clearing buffers */
    _peelFBO->apply(state);
    glDrawBuffer(GL_BUFFER_NAMES[0]);
    glClearColor(-1000000000.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawBuffer(GL_BUFFER_NAMES[1]);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
    gl2e->glDrawBuffers(2, &GL_BUFFER_NAMES[0]);
}

/*
  Screen definitions
*/
DepthPeelingBin::_Impl::Screen::Screen(osg::RenderInfo& renderInfo,
                                       Context* context)
{
    _context = context;

    _camera = renderInfo.getCurrentCamera();
    GLint width = static_cast<GLint>(_camera->getViewport()->width());
    GLint height = static_cast<GLint>(_camera->getViewport()->height());

    /* Initializing the uniform variables that are related to the camera */
    _cameraWidth = new osg::Uniform("width", (float)width);
    _cameraHeight = new osg::Uniform("height", (float)height);

    /* Cells 3,3 and 3,4 of the projection matrix (standard notation) */
    _projection_33 = new osg::Uniform(osg::Uniform::FLOAT, "proj33");
    _projection_33->set((float)_camera->getProjectionMatrix()(2, 2));
    _projection_34 = new osg::Uniform(osg::Uniform::FLOAT, "proj34");
    _projection_34->set((float)_camera->getProjectionMatrix()(3, 2));

    /* Deciding the offscreen tile size */
    _rows = 1;
    _columns = 1;

    /* All tiles are equally sized, different widths and heights are ignored.
       This screen sizes are including the potential padding */
    unsigned int screenTileWidth = (width - 1) / _columns + 1;
    unsigned int screenTileHeight = (height - 1) / _rows + 1;
    _offscreenViewport =
        new osg::Viewport(0, 0, screenTileWidth, screenTileHeight);

    /* Creating the array of tiles for this screen */
    _tiles.reserve(_rows * _columns);
    /* All offscreen buffers will be equally sized but some tiles will be
       1 pixel narrower and/or shorter in screen space. Every tile will define
       the projection matrix and a scissor test to account for the padding to
       fill the offscreen buffer */
    int heightRemainder = height % _rows == 0 ? _rows : height % _rows;
    unsigned int y = 0;
    for (unsigned int i = 0; i < _rows; ++i, --heightRemainder)
    {
        unsigned int x = 0;
        unsigned int padY = heightRemainder < 0 ? 1 : 0;
        unsigned int h = screenTileHeight - padY;
        int widthRemainder =
            width % _columns == 0 ? _columns : width % _columns;
        for (unsigned int j = 0; j < _columns; ++j, --widthRemainder)
        {
            unsigned int padX = widthRemainder < 0 ? 1 : 0;
            unsigned int w = screenTileWidth - padX;
            _tiles.push_back(new Tile(this, i, j, x, y, w, h, padX, padY));
            x += w;
        }
        y += h;
    }
}

/*
  Context definitions
*/

bool DepthPeelingBin::_Impl::Context::extractTextures(size_t count,
                                                      GLenum format,
                                                      TextureList& textures)
{
    TextureList& list = _freeTextures[format];
    if (list.size() < count)
    {
        return false;
    }
    else
    {
        for (size_t i = 0; i < count; ++i)
            textures.splice(textures.end(), list, list.begin());
        return true;
    }
}

void DepthPeelingBin::_Impl::Context::createBuffersAndTextures()
{
    assert(_screen.valid());
    GLint screenWidth = _screen->getWidth();
    GLint screenHeight = _screen->getHeight();
    GLint tileWidth = _screen->getTileViewport()->width();
    GLint tileHeight = _screen->getTileViewport()->height();

    /* Creating FBOs */
    blendBuffer = new osg::FrameBufferObject();

    /* Keeping textures that already have at least the required size. */
    TextureList& glR32FTextures = _freeTextures[GL_R32F];
    TextureList& glRGBA32FTextures = _freeTextures[GL_RGBA32F];
    _filterSmallerTextures(glR32FTextures, tileWidth, tileHeight);
    _filterSmallerTextures(glRGBA32FTextures, tileWidth, tileHeight);

    /* Creating ping-pong depth and color textures */
    for (size_t i = glR32FTextures.size(); i < 2; ++i)
    {
        osg::TextureRectangle* depth =
            createTexture<osg::TextureRectangle>(tileWidth, tileHeight,
                                                 GL_R32F);
        glR32FTextures.push_back(depth);
    }

    for (size_t i = glRGBA32FTextures.size(); i < 2; ++i)
    {
        osg::TextureRectangle* color =
            createTexture<osg::TextureRectangle>(tileWidth, tileHeight,
                                                 GL_RGBA32F_ARB);
        glRGBA32FTextures.push_back(color);
    }

    /* Creating color textures for blending of each layer */
    targetBlendColorTexture =
        createTexture<osg::TextureRectangle>(screenWidth, screenHeight);
    osg::FrameBufferAttachment buffer(targetBlendColorTexture.get());
    blendBuffer->setAttachment(COLOR_BUFFERS[0], buffer);
}

void DepthPeelingBin::_Impl::Context::createBaseStateSets()
{
    using namespace keywords;

    Modes modes;
    Attributes attributes;
    Uniforms uniforms;

    /* The rendering code may be executed before any other object is drawn,
       that makes possible that the correct viewport hasn't been applied yet.
       The reason is really wierd, it seems that RenderStage's default
       viewport is always 800x600 regarless of window resizing or
       fullscreen mode.
       To avoid problems, we will add a viewport state attribute to all the
       steps of the rendering. */
    /*
      First pass state set
    */
    baseFirstPassStateSet = new osg::StateSet;
    /* Reserving the 4 first texture numbers for textures units used in the
       vertex shading */
    modes.clear();
    attributes.clear();
    uniforms.clear();
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    attributes[new osg::BlendEquation(RGBA_MAX)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    setupStateSet(baseFirstPassStateSet.get(), modes, attributes, uniforms);

    /*
       Peel state set
    */
    basePeelStateSet = new osg::StateSet;
    modes.clear();
    attributes.clear();
    uniforms.clear();
    modes[GL_DEPTH] = OFF;
    modes[GL_CULL_FACE] = OFF_OVERRIDE;
    attributes[new osg::BlendEquation(RGBA_MAX)] = ON_OVERRIDE;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE)] = ON_OVERRIDE;
    /* Reserving the 4 first texture numbers for textures units used in the
       vertex and fragment shading */
    uniforms.insert(
        new osg::Uniform("depthBuffer", (int)parameters.reservedTextureUnits));
    setupStateSet(basePeelStateSet.get(), modes, attributes, uniforms);
    setupTexture("blendedBuffer", (int)parameters.reservedTextureUnits + 1,
                 *basePeelStateSet, targetBlendColorTexture.get());

    /*
       Blend state set
    */
    baseBlendStateSet = new osg::StateSet;
    // clang-format off
    addProgram(
        baseBlendStateSet.get(), _vertex_shaders = strings(BYPASS_VERT_SHADER),
        _fragment_shaders = strings(R"(
        #extension GL_ARB_draw_buffers : enable
        #extension GL_ARB_tecture_rectangle : enable
        uniform sampler2DRect colorTexture;
        uniform vec2 offset;
        void main(void)
        {
            gl_FragColor =
               texture2DRect(colorTexture, gl_FragCoord.xy - offset);
        })"));
    // clang-format on
    modes.clear();
    attributes.clear();
    uniforms.clear();
    modes[GL_DEPTH] = OFF;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON;
    attributes[new osg::BlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE)] = ON;

    attributes[_screen->getCamera()->getViewport()] = ON_OVERRIDE_PROTECTED;
    uniforms.insert(new osg::Uniform("colorTexture", 0));
    setupStateSet(baseBlendStateSet.get(), modes, attributes, uniforms);

    /*
       Final copy state set
    */
    finalStateSet = new osg::StateSet();
    // clang-format off
    addProgram(
        finalStateSet.get(), _vertex_shaders = strings(BYPASS_VERT_SHADER),
        _fragment_shaders = strings(R"(
         #extension GL_ARB_tecture_rectangle : enable
         uniform sampler2DRect blendBuffer;
         uniform vec2 lowerLeftCorner;
         void main(void)
         {
             gl_FragColor = texture2DRect(
                 blendBuffer, gl_FragCoord.xy - lowerLeftCorner);
         })"));
    // clang-format on
    modes.clear();
    attributes.clear();
    uniforms.clear();
    _lowerLeftCornerUniform = new osg::Uniform("lowerLeftCorner", osg::Vec2());
    uniforms.insert(_lowerLeftCornerUniform);
    modes[GL_DEPTH] = OFF;
    attributes[new osg::BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)] = ON;
    attributes[new osg::BlendEquation(FUNC_ADD)] = ON;
    setupTexture("blendBuffer", 0, *finalStateSet,
                 targetBlendColorTexture.get());
    setupStateSet(finalStateSet.get(), modes, attributes, uniforms);
}

void DepthPeelingBin::_Impl::Context::updatePrograms(
    const ProgramMap& extraShaders)
{
    using namespace keywords;

    /*
      First pass programs
    */
    addPrograms(extraShaders, &firstPassPrograms,
                _vertex_shaders = strings(sm("trivialShadeVertex();")),
                _fragment_shaders = strings(R"(
        float fragmentDepth();
        void main()
        {
            gl_FragColor.r = -fragmentDepth();
        })"));
    // clang-format on
    /*
       Peel programs
    */
    addPrograms(extraShaders, &peelPassPrograms,
                _vertex_shaders = strings(sm("shadeVertex();")),
                _filenames = strings("peel.frag"), _shader_path = "simple/");
}

void DepthPeelingBin::_Impl::Context::startFrame(DepthPeelingBin* bin,
                                                 osg::RenderInfo& renderInfo,
                                                 osgUtil::RenderLeaf*& previous)
{
    osg::State& state = *renderInfo.getState();

    osg::Camera* camera = renderInfo.getCurrentCamera();
    if (getScreen() == 0 || !getScreen()->valid(camera))
    {
        _screen = new Screen(renderInfo, this);
        createBuffersAndTextures();
        createBaseStateSets();
    }

    ProgramMap newShaders;
    updateProgramMap(*bin->_extraShaders, _extraShaders, newShaders);
    if (!newShaders.empty())
        updatePrograms(newShaders);

    /* Saving part of the state to be restored at the end of the drawing. */
    glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &_previousFBO);
    oldPrevious = previous;
    _savedStackPosition = state.getStateSetStackSize();
#ifndef NDEBUG
    _oldState = new osg::StateSet;
    state.captureCurrentState(*_oldState);
#endif

    /* Clearing final blend buffer (could be done per tile to minimize state
       changed, then we need a scissor test).
       Applying the viewport since the current viewport should correspond to
       the camera and it's not necessarily at 0, 0. */
    _screen->getTileViewport()->apply(state);

    blendBuffer->apply(state);
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    _screen->startFrame();
}

void DepthPeelingBin::_Impl::Context::finishFrame(
    osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    osg::State& state = *renderInfo.getState();
    FBOExtensions* fbo_ext = getFBOExtensions(state.getContextID());
    osg::Camera* camera = renderInfo.getCurrentCamera();

    _lowerLeftCornerUniform->set(
        osg::Vec2(camera->getViewport()->x(), camera->getViewport()->y()));

    state.apply(finalStateSet.get());
    camera->getViewport()->apply(state);

/* Returning to previously bound buffer. */
#if OPENSCENEGRAPH_MAJOR_VERSION == 2 && OPENSCENEGRAPH_MINOR_VERSION <= 8
    fbo_ext->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, _previousFBO);
#else
    fbo_ext->glBindFramebuffer(GL_FRAMEBUFFER_EXT, _previousFBO);
#endif
    quad->draw(renderInfo);

    state.popStateSetStackToSize(_savedStackPosition);
    previous = oldPrevious;
    state.apply();

#ifndef NDEBUG
    osg::ref_ptr<osg::StateSet> currentState(new osg::StateSet);
    state.captureCurrentState(*currentState);
    assert(*currentState == *_oldState);
#endif
}

/*
  Other definitions waiting requiring previous declarations
*/
DepthPeelingBin::_Impl::Context& DepthPeelingBin::_Impl::getContext(
    const osg::State& state, const Parameters& parameters)
{
    static OpenThreads::Mutex contextMapMutex;
    /* Multiple draw threads might be trying to create their own
       alpha-blending context */
    OpenThreads::ScopedLock<OpenThreads::Mutex> lock(contextMapMutex);

    unsigned int contextID = state.getContextID();
    boost::shared_ptr<Context>& context = s_context[contextID];
    if (!context || !context->updateParameters(parameters))
        context.reset(new Context(contextID, parameters));

    return *context;
}

/*
  Member functions
*/

DepthPeelingBin::DepthPeelingBin()
{
}

DepthPeelingBin::DepthPeelingBin(const Parameters& parameters)
    : BaseRenderBin(boost::shared_ptr<Parameters>(new Parameters(parameters)))
{
}

DepthPeelingBin::DepthPeelingBin(const DepthPeelingBin& renderBin,
                                 const osg::CopyOp& copyop)
    : BaseRenderBin(renderBin, copyop)
{
}

void DepthPeelingBin::drawImplementation(osg::RenderInfo& renderInfo,
                                         osgUtil::RenderLeaf*& previous)
{
    OSGTRANSPARENCY_TRACE_FUNCTION();

    /* This render bin must be transparent to the state management.
       Our goal here is that OSG has a consistent State object and that
       the following algorithm is transparent to any calling prerender bins.
       That means that they have to find the state stack and the current
       OpenGL state in the state you can expect from a regular bin. */
    typedef _Impl::Screen Screen;
    typedef _Impl::Tile Tile;

    /** \bug The current implementation doesn't support recursive calls
        resulting from nested render bins */
    osg::State& state = *renderInfo.getState();
    DepthPeelingBin::_Impl::Context& context =
        _Impl::getContext(state, *_parameters);

    context.startFrame(this, renderInfo, previous);

    Screen* screen = context.getScreen();
    while (!screen->getUnfinishedTiles().empty() || screen->getUnissuedTile())
    {
        /* First we advance one rendering step in all the initialized tiles */
        Screen::TileList tiles = screen->getUnfinishedTiles();
        for (Screen::TileList::iterator t = tiles.begin(); t != tiles.end();
             ++t)
        {
            Tile* tile = *t;
            tile->blend(renderInfo);
            tile->peel(this, renderInfo);
        }
        /* Then we check whether we can initialize any new tile and do it*/
        for (Tile* tile = screen->getUnissuedTile();
             tile != 0 && tile->init(this, renderInfo, previous);
             tile = screen->getUnissuedTile())
        {
            /* We shade the first layer right away */
            tile->peel(this, renderInfo);
        }
    }

    context.finishFrame(renderInfo, previous);
}

void DepthPeelingBin::_Impl::Tile::writeLayers(bool writeColor)
{
    const osg::Viewport* vp = _screen->getCamera()->getViewport();

    using boost::str;
    using boost::format;
    static float old_depth;
    if (writeColor &&
        (::getenv("OSGTRANSPARENCY_WRITE_COLOR_LAYERS") != 0 ||
         ::getenv("OSGTRANSPARENCY_WRITE_ALL_LAYERS") != 0 || s_debugPartition))
    {
        osg::ref_ptr<osg::Image> image = new osg::Image;
        glReadBuffer(GL_BUFFER_NAMES[1]);
        image->readPixels(0, 0, vp->width(), vp->height(), GL_RGBA,
                          s_debugPartition ? GL_FLOAT : GL_UNSIGNED_BYTE);
        if (s_debugPartition)
        {
            if (::getenv("OSGTRANSPARENCY_DEBUG_ALPHA_ACCUMULATION"))
            {
                std::cout << old_depth << ':'
                          << ((float*)image->data(s_debugPartition.col,
                                                  s_debugPartition.row))[3]
                          << ' ';
            }
            else
            {
                std::cout << old_depth << ' ';
            }
        }
        else
        {
            static int i = 0;
            std::string filename =
                boost::str(boost::format("color_layer_%03d.png") % i++);
            osgDB::writeImageFile(*image, filename);
        }
    }
    if (::getenv("OSGTRANSPARENCY_WRITE_DEPTH_LAYERS") != 0 ||
        ::getenv("OSGTRANSPARENCY_WRITE_ALL_LAYERS") != 0 || s_debugPartition)
    {
        osg::ref_ptr<osg::Image> image = new osg::Image();
        glReadBuffer(GL_BUFFER_NAMES[0]);
        image->readPixels(0, 0, vp->width(), vp->height(), GL_RED, GL_FLOAT);
        if (!s_debugPartition)
            image->setPixelFormat(GL_LUMINANCE);
        static int i = 0;
        std::string filename = str(format("depth_layer_%03d.tiff") % i++);
        /* Inverting the channel of the image. */
        for (int s = 0; s < image->s(); ++s)
        {
            for (int t = 0; t < image->t(); ++t)
            {
                *(float*)image->data(s, t) = -*(float*)image->data(s, t);
            }
        }
        if (s_debugPartition)
        {
            old_depth = *(float*)image->data(s_debugPartition.col,
                                             s_debugPartition.row);
        }
        else
        {
            osgDB::writeImageFile(*image, filename);
        }
    }
}
}
}
