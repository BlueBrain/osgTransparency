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

#include <osgTransparency/DepthPeelingBin.h>
#include <osgTransparency/FragmentListOITBin.h>
#include <osgTransparency/MultiLayerDepthPeelingBin.h>
#include <osgTransparency/MultiLayerParameters.h>

#include <osg/ArgumentParser>
#include <osg/DisplaySettings>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/ShapeDrawable>
#include <osgGA/TrackballManipulator>
#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <limits>

osg::Node *createCubesScene(unsigned int side, unsigned int cubesPerPrimitive,
                            float alpha);

int main(int argc, char *argv[])
{
    osg::ArgumentParser args(&argc, argv);
    unsigned int side = 2;
    args.read("--side", side);

    unsigned int slices = 0;
    args.read("--slices", slices);

    unsigned int passes = 0;
    args.read("--max-passes", passes);

    unsigned int frames = 0;
    args.read("--frames", frames);

    float alpha = 0.3;
    args.read("--alpha", alpha);

    bool distanceDependentAlpha = args.read("--alpha-by-distance");

    bool singleQuery = args.read("--single-query");

    unsigned int width = 800, height = 600;
    args.read("-g", width, height);

    unsigned int cubesPerPrimitive;
    if (!args.read("--cubes-per-primitive", cubesPerPrimitive) ||
        cubesPerPrimitive == 0)
    {
        cubesPerPrimitive = std::numeric_limits<unsigned int>::max();
    }

    const bool alphaAware = args.read("--alpha-aware");

    std::string algorithm = "depth-peeling";
    args.read("--algorithm", algorithm);

    bbp::osgTransparency::BaseRenderBin *renderBin = 0;

#ifdef OSG_GL3_AVAILABLE
    if (algorithm == "lists")
    {
        osg::DisplaySettings::instance()->setMinimumNumStencilBits(8);
        bbp::osgTransparency::FragmentListOITBin::Parameters parameters;
        if (args.read("--print-stats"))
        {
            parameters.setCaptureCallback(
                0, bbp::osgTransparency::extractFragmentStatistics);
        }
        if (alphaAware)
            parameters.enableAlphaCutOff(0.99);

        renderBin = new bbp::osgTransparency::FragmentListOITBin(parameters);
    }
#endif
    if (algorithm == "depth-peeling" || renderBin == 0)
    {
        if (slices == 0)
        {
            renderBin = new bbp::osgTransparency::DepthPeelingBin();
        }
        else
        {
            bbp::osgTransparency::MultiLayerDepthPeelingBin::Parameters
                parameters(slices, (void *)0, alphaAware);
            if (passes != 0)
                parameters.maximumPasses = passes;
            parameters.singleQueryPerPass = singleQuery;
            bbp::osgTransparency::MultiLayerDepthPeelingBin *mldpb =
                new bbp::osgTransparency::MultiLayerDepthPeelingBin(parameters);
            renderBin = mldpb;
        }
    }

    osgUtil::RenderBin::addRenderBinPrototype("alphaBlended", renderBin);

    osgViewer::Viewer viewer;

    osg::Node *scene = createCubesScene(side, cubesPerPrimitive, alpha);

    osg::StateSet *stateSet = scene->getOrCreateStateSet();
    stateSet->setRenderBinDetails(1, "alphaBlended");

    osg::Program *program = new osg::Program();
    if (distanceDependentAlpha)
    {
        /* Replacing the default fragment shader with a new one */
        osg::Shader *vert = new osg::Shader(osg::Shader::VERTEX);
        vert->setShaderSource(R"(
        #version 130
        out vec3 normal;
        out float distance;
        out vec4 color;
        void shadeVertex()
        {
            vec4 v = gl_ModelViewMatrix * gl_Vertex;
            gl_Position = gl_ProjectionMatrix * v;
            distance = min(200, -v.z);
            normal = gl_NormalMatrix * gl_Normal;
            color = gl_Color;
        }
        void trivialShadeVertex()
        {
            shadeVertex();
        })");
        program->addShader(vert);
        osg::Shader *frag = new osg::Shader(osg::Shader::FRAGMENT);
        frag->setShaderSource(R"(
        #version 130
        in vec4 color;
        in vec3 normal;
        in float distance;
        float fragmentAlpha() { return distance / 200.0 * color.a; }
        float fragmentDepth() { return gl_FragCoord.z; }
        vec4 shadeFragment()
        {
            vec3 n = normalize(normal);
            const vec3 l = vec3(0.0, 0.0, 1.0);
            float lambertTerm = dot(n, l);
            if (lambertTerm < 0.0)
                lambertTerm = -lambertTerm;
            vec3 shadedColor = color.rgb * lambertTerm;
            return vec4(shadedColor, fragmentAlpha());
        })");
        program->addShader(frag);
    }
    renderBin->addExtraShadersForState(stateSet, program);

    viewer.setSceneData(scene);
    viewer.setUpViewInWindow(50, 50, width, height);
    viewer.addEventHandler(new osgViewer::StatsHandler);

    if (frames)
    {
        viewer.setCameraManipulator(new osgGA::TrackballManipulator());
        for (unsigned int i = 0; i != frames; ++i)
            viewer.frame();
    }
    else
    {
        viewer.run();
    }
}

osg::Node *createCubesScene(unsigned int side, unsigned int cubesPerPrimitive,
                            float alpha)
{
    osg::Geode *geode = new osg::Geode();

    osg::ref_ptr<osg::Vec3Array> vertices = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array();
    osg::ref_ptr<osg::Vec4Array> colors = new osg::Vec4Array();
    const size_t count = 24;
    int indices[] = {0, 2, 3, 1, 2, 6, 7, 3, 2, 0, 4, 6,
                     0, 1, 5, 4, 1, 3, 7, 5, 4, 5, 7, 6};
    unsigned int cubes = 0;
    float red = 1;
    float green = 1;
    for (unsigned i = 0; i < side; ++i)
    {
        for (unsigned j = 0; j < side; ++j)
        {
            for (unsigned k = 0; k < side; ++k)
            {
                osg::Vec3 center(i, j, k);
                osg::Vec3 corners[8];
                for (int n = 0; n < 8; ++n)
                {
                    osg::Vec3 vertex(n % 2, (n / 2) % 2, n / 4);
                    corners[n] = vertex * 0.8 + center;
                }
#ifdef _WIN32
                const float rnd = rand() / (float)RAND_MAX;
#else
                const float rnd = drand48();
#endif
                osg::Vec4 color(red, green, rnd, alpha);
                for (size_t n = 0; n < count; ++n)
                {
                    colors->push_back(color);
                    vertices->push_back(corners[indices[n]]);
                }
                osg::Vec3 normal[] = {osg::Vec3(0, 0, -1), osg::Vec3(0, 1, 0),
                                      osg::Vec3(-1, 0, 0), osg::Vec3(0, -1, 0),
                                      osg::Vec3(1, 0, 0),  osg::Vec3(0, 0, 1)};
                for (int n = 0; n < 6; ++n)
                    for (int m = 0; m < 4; ++m)
                        normals->push_back(normal[n]);

                if (++cubes == cubesPerPrimitive)
                {
                    osg::Geometry *geometry = new osg::Geometry();
                    geometry->setVertexArray(vertices);
                    geometry->setNormalArray(normals);
                    geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
                    geometry->setColorArray(colors);
                    geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
                    osg::DrawArrays *quad =
                        new osg::DrawArrays(GL_QUADS, 0,
                                            count * cubesPerPrimitive);
                    geometry->addPrimitiveSet(quad);
                    geometry->setUseVertexBufferObjects(true);
                    geometry->setUseDisplayList(false);
                    geode->addDrawable(geometry);
                    cubes = 0;
                    vertices = new osg::Vec3Array();
                    normals = new osg::Vec3Array();
                    colors = new osg::Vec4Array();
                    red = 0.25 + 0.75 * drand48();
                    green = 0.25 + 0.75 * drand48();
                }
            }
        }
    }
    if (cubes != 0)
    {
        osg::Geometry *geometry = new osg::Geometry();
        geometry->setVertexArray(vertices);
        geometry->setNormalArray(normals);
        geometry->setNormalBinding(osg::Geometry::BIND_PER_VERTEX);
        geometry->setColorArray(colors);
        geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);
        osg::DrawArrays *quad = new osg::DrawArrays(GL_QUADS, 0, count * cubes);
        geometry->addPrimitiveSet(quad);
        geometry->setUseVertexBufferObjects(true);
        geometry->setUseDisplayList(false);
        geode->addDrawable(geometry);
    }

    return geode;
}
