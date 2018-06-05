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

#include "RenderContext.h"
#include "compare.h"
#include "util.h"

#include <osgTransparency/BaseRenderBin.h>

#include <osg/GL>

#include <osg/Geometry>
#include <osg/GraphicsContext>
#include <osg/PrimitiveSet>
#include <osgDB/WriteFile>
#include <osgViewer/Viewer>

#define BOOST_TEST_MODULE
#include <boost/test/unit_test.hpp>

using namespace bbp::osgTransparency;

namespace
{
osg::ref_ptr<osg::Drawable> createTriangle(const osg::Vec3 &a,
                                           const osg::Vec3 &b,
                                           const osg::Vec3 &c)
{
    osg::ref_ptr<osg::Geometry> geometry(new osg::Geometry());
    osg::Vec3Array *vertices = new osg::Vec3Array();
    vertices->push_back(a);
    vertices->push_back(b);
    vertices->push_back(c);
    geometry->setVertexArray(vertices);
    geometry->addPrimitiveSet(new osg::DrawArrays(GL_TRIANGLES, 0, 3));
    geometry->setUseDisplayList(false);
    geometry->setUseVertexBufferObjects(true);
    return geometry;
}

osg::ref_ptr<osg::Drawable> createTriangle()
{
    return createTriangle(osg::Vec3(-1, -1, 0), osg::Vec3(1, -1, 0),
                          osg::Vec3(0, 1, 0));
}
}

class SimpleRenderBin : public BaseRenderBin
{
public:
    SimpleRenderBin(test::RenderContext &renderContext)
        : _renderContext(renderContext)
    {
    }

    virtual void drawImplementation(osg::RenderInfo &renderInfo,
                                    osgUtil::RenderLeaf *& /*previous*/)
    {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, _renderContext.width, _renderContext.height);

        osg::ref_ptr<osg::Drawable> drawable = createTriangle();
        osg::ref_ptr<osg::Program> program = test::util::createTrivialProgram();

        program->apply(*renderInfo.getState());
        drawable->draw(renderInfo);
    }

private:
    test::RenderContext &_renderContext;
};

class RenderBinWithSimpleStateGraph : public BaseRenderBin
{
public:
    RenderBinWithSimpleStateGraph(test::RenderContext &renderContext)
        : _renderContext(renderContext)
        , _stateGraph(new osgUtil::StateGraph)
        , _currentStateGraph(_stateGraph)
        , _matrix(new osg::RefMatrix)
        , _projection(new osg::RefMatrix)
        , _stateSet(new osg::StateSet)
    {
        _projection->makeOrtho(-1, 1, -1, 1, 0, 1);
    }

    void addDrawable(osg::Drawable *drawable, osg::Program *program)
    {
        osg::StateSet *stateSet = drawable->getOrCreateStateSet();
        _programs[stateSet] = program;

        _currentStateGraph =
            _currentStateGraph->find_or_insert(drawable->getOrCreateStateSet());
        if (_currentStateGraph->leaves_empty())
            addStateGraph(_currentStateGraph);
        /* The traversal order parameter is irrelevant because leaves are
           not going to be sorted. */
        _currentStateGraph->addLeaf(
            new osgUtil::RenderLeaf(drawable, 0, _matrix, 0, 0));
    }

    virtual void drawImplementation(osg::RenderInfo &renderInfo,
                                    osgUtil::RenderLeaf *&previous)
    {
        glClearColor(1, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, _renderContext.width, _renderContext.height);

        render(renderInfo, previous, _stateSet, _programs, _projection);
    }

    virtual void reset()
    {
        _stateGraph = new osgUtil::StateGraph();
        _currentStateGraph = _stateGraph;
        _programs.clear();
        BaseRenderBin::reset();
    }

private:
    test::RenderContext &_renderContext;

    osg::ref_ptr<osgUtil::StateGraph> _stateGraph;
    osg::ref_ptr<osgUtil::StateGraph> _currentStateGraph;
    osg::ref_ptr<osg::RefMatrix> _matrix;
    osg::ref_ptr<osg::RefMatrix> _projection;

    osg::ref_ptr<osg::StateSet> _stateSet;
    ProgramMap _programs;
};

#define RC_200x200 test::SizedRenderContext<200, 200>

BOOST_FIXTURE_TEST_SUITE(suite, RC_200x200)

BOOST_AUTO_TEST_CASE(test_render_simple)
{
    osg::ref_ptr<SimpleRenderBin> renderBin(new SimpleRenderBin(*this));

    saveCurrentState();
    renderBin->draw(*renderInfo, previousLeaf);
    compareCurrentAndSavedState();

    BOOST_CHECK(test::compare("triangle.png", 0, 0, width, height));
}

BOOST_AUTO_TEST_CASE(test_render_state_graph)
{
    osg::ref_ptr<RenderBinWithSimpleStateGraph> renderBin(
        new RenderBinWithSimpleStateGraph(*this));

    osg::ref_ptr<osg::Drawable> triangle = createTriangle();
    renderBin->addDrawable(triangle, test::util::createTrivialProgram());
    saveCurrentState();
    renderBin->draw(*renderInfo, previousLeaf);
    compareCurrentAndSavedState();
    BOOST_CHECK(test::compare("triangle.png", 0, 0, width, height));

    renderBin->reset();
    previousLeaf = 0;

    renderBin->addDrawable(triangle,
                           test::util::createStretchProgram(-0.5, 0.5, -0.5,
                                                            0.5));
    renderBin->draw(*renderInfo, previousLeaf);
    BOOST_CHECK(test::compare("small_triangle.png", 0, 0, width, height));
}

BOOST_AUTO_TEST_SUITE_END()
