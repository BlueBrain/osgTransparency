Tutorial {#tutorial}
========
This brief tutorial shows the key steps to set up multi-layer depth peeling
with 4 slices per pass.

The first step is to add the render bin to use to the set of render bins
known to OSG.

\code
bbp::osgTransparency::MultiLayerDepthPeelingBin *renderBin =
  renderBin = new bbp::osgTransparency::MultiLayerDepthPeelingBin();
osgUtil::RenderBin::addRenderBinPrototype("alphaBlended", renderBin);
renderBin->setDefaultNumSlices(4);
\endcode

Next, the objects to render must include at least a vertex
and fragment shader inside an osg::Program part of their osg::StateSet.
These shaders provide functions needed by the render bins to complete the
GLSL programs used. The function addExtraShadersForState is used to inform
our MultiLayerDepthPeelingBin about which shaders to use for a osg::StateSet.
\code
osg::Program *program = new osg::Program;
// ... create the extra shaders and add them to the program
renderBin->addExtraShadersForState(stateSet, program);
\endcode
Warning, the program object must not be part of the osg::StateSet to add.

And example of basic vertex shader is:
\code
#version 130
out vec3 normal;

// Writes to gl_Position.
void shadeVertex()
{
    gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * gl_Vertex;
    normal = gl_NormalMatrix * gl_Normal;
}

// A potentially simplified version of the funcion above to be used
// in passes that only need to know the final fragment depth.
void trivialShadeVertex()
{
    shadeVertex();
}
\endcode

And for the fragment shader:
\code
#version 130
in vec3 normal;

// Returns the final color to apply for a shaded fragment
vec4 shadeFragment()
{
    vec4 color = vec4(1, 0, 0, 0.2);
    vec3 n = normalize(normal);
    const vec3 l = vec3(0.0, 0.0, 1.0);
    float lambertTerm = dot(n, l);
    if (lambertTerm < 0.0)
        lambertTerm = -lambertTerm;
    vec3 shadedColor = color.rgb * lambertTerm;
    return vec4(shadedColor, color.a);
}

// Returns the alpha value of the fragment.
// This method is used in some optimizations which are not enabled by
// default. It is safe to return 0 as a fallback value.
float fragmentAlpha()
{
    return color.a;
}

// Returns the depth value of the fragment.
// This functions is provided for shaders which can modify the fragment depth
// from the value given in gl_FragCoord.z
float fragmentDepth()
{
    return gl_FragCoord.z;
}
\endcode

Note that there is no main in any of the shaders above (each step of every
algorithm provides their own ones). The functions and prototypes in the
examples above are the interface that the internal shaders expect for each
state set that has been enabled.

Geometry shaders are also supported without any special consideration.
The current implementation cannot mix opaque and transparent geometry
properly. The main reason is that the z-buffer of the render bins is
independent from the z-buffer of the framebuffer. Future releases will
address this problem.

Finally, a complete code example can be found [here](https://bbpteam.epfl.ch/reps/viz/osgTransparency.git/tree/examples/example.cpp)

