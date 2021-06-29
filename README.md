[TOC]

# Introduction {#Introduction}

Welcome to OSG Transparency, a C++ library that provides 3 algorithms to render
transparent geometry in OpenSceneGraph (OSG) in an improved way compared to
simple back-to-front sorting at object level (what OSG does by default).

There are several online resources available which may be of interest to
users:
* The [git repository](ssh://github.com/BlueBrain/osgTransparency.git)
* The online copy of this [documentation](https://bluebrain.github.io/osgTransparency-0.8/)
To keep track of the changes between releases check the [changelog](@ref Changelog).

For code examples on how to use the library check the @ref tutorial.

# Features {#Features}

This library provides 3 algorithm for order-independent-transparency when built
with GL2 support, plus a fourth one when built with GL3 support. The algorithms
are:
* Depth peeling: Simple multi-pass algorithm that sorts the fragment layers
  one by one ([original paper](http://developer.download.nvidia.com/SDK/10/opengl/src/dual_depth_peeling/doc/DualDepthPeeling.pdf)).
* Multi-layer depth peeling: Extension of depth peeling to peel several
  slices at the same time. The slices are load-balanced in a previous step.
  This algorithm is correct as depth peeling and also faster, but slower
  than bucket depth peeling.
* Fragment-linked-lists: An A-buffer implementation using lists of fragments.
  This algorithm makes use of the GL extension for random access image buffer
  objects and so, it is only available in the GL3 build.

All algorithms are implemented as classes that inherit from osgUtil::RenderBin.

# Building {#Building}

OSG Transparency is a cross-platform library, designed to run on any modern
operating system, including all Unix variants. OSG Transparency uses CMake to
create a platform-specific build environment. The following platforms and build
environments are tested:

* Linux: Ubuntu 16.04, RHEL 6.8 (Makefile, x64)

This library depends on Boost and OpenSceneGraph, which are available as binary
packages for Windows and system packages for RedHat and Debian based
distributions. Once the dependencies are installed, building the GL2 version
from source is as simple as:

    git clone --recursive https://github.com/BlueBrain/osgTransparency.git
    mkdir osgTransparency/build
    cd osgTransparency/build
    cmake ..
    make

For the GL3 version, OpenSceneGraph needs to be compiled with GL3 support,
refer to the OpenSceneGraph documentation for compilation instructions.

# Funding & Acknowledgment
 
The development of this software was supported by funding to the Blue Brain Project,
a research center of the École polytechnique fédérale de Lausanne (EPFL), from the
Swiss government’s ETH Board of the Swiss Federal Institutes of Technology.
