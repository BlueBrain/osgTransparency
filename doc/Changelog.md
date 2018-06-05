Changelog  {#Changelog}
=========

# Release 0.8.1 (23-May-2017)

### Enhancements

* Better clean up of OpenGL query objects when building for OSG >= 3.3.3

# Release 0.8.0 (09-Dec-2016)

### Enhancements

* An optimization called alpha cut-off has been implemented in
  FragmentListOITBin. This optimization consist on discarding fragments known
  to be occluded due to alpha accumulation. Alpha accumulation is computed
  in an order independent manner, so the effectiveness is very scene dependent.
  In particular this is very useful in scenes with high depth complexity and
  relatively high average alpha (> 0.5).
  The optimization can be enabled with a new parameter to the algorithm.
* GPU timing added to FragmentListOITBin for profiling.

### API Changes

* Changed FragmentListOITBin::Parameters to replace the callback variable
  with a setter function that allows a different callback per context.
* The signature of the fragment capture callback function has been changed to
  take a class with accessor methods instead of a collection of textures.
* FragmentListOITBin::Parameters::sortAndDisplay removed, the capture callback
  has been modified to use its return value instead of a member variable.
* New functions added to FragmentListOITBin::Parameters to enable and disable
  alpha cut-off.

# Release 0.7.0 (30-Jun-2016)

### Enhancements

* Optimizations in fragment list OIT
** Backwards Memory Allocation algorithm by P. Knowles et al.
** In place heapsort for sorting fragments lists longer than 32 fragments.

### API Changes

* Added a new parameter to FragmentListOITBin that allows to hook a callback
  function to be invoked after the pass that captures fragments.
* Bucket depth peeling algorithm removed.

# Release 0.6.1 (9-Nov-2015)

### Bug fixes

* Improved render bin clean up in multilayer depth peeling and fragment linked
  lists algorithms. This allows to reuse the render bins with a new
  osgViewer::Viewer safely.

# Release 0.6.0 (6-Jul-2015)

### Enhancements

* Adapted the GL3 version of the algorithms to support OSG 3.3.4.

### Bug fixes

* Fixes in the profiling of the algorithm using fragment lists.
* Bugfix for nvidia driver 346.35.
* Fixed client CMake configuration script.

# Release 0.5.0 (3-Oct-2104)

### New Features

* The opacity threshold parameter in multi-layer depth peeling is now used
  to choose a simplified code path that doesn't check accumulated alpha
  when its value is >= 1.0.

### API Changes

* MultiLayerDepthPeelingBin::Parameter::opacityThreshold changed to be a
  constant parameters that must be given at construction time.  Setting a new
  prototype bin with a different threshold value will require shader
  compilation at the next render traversal because shaders cannot be reused.

# Relase 0.4.1 (9-Jun-2014)

### New Features

* Added a new environmental variable to properly support shader path relocation
  as needed by GNU modules.

### Bug fixes

* Performance bug fix in the occlusion query mechanism selection. The default
  mode was the slowest one.
* Several bug fixes related to support for dynamic viewport and buffer size
  changes in multilayer and simple depth peeling.

# Release 0.4.0 (19-Mar-2014)

### Bug Fixes

* Workaround for apparently buggy texture accesses inside the peel loop.

# Release 0.3.3 (24-Sep-2013)

### Bug Fixes

* Fixed a bug related to state set handling: user given shaders
  were polluted with internal shaders. This was causing problems in
  some scenarios.
* Fixed a problem with backface culling of screen aligned quads. This was
  affecting RTNeuron.

# Release 0.3.2 (24-Jul-2013)

### Bug Fixes

* Fixed a bug related to dynamic state set handling: depth partition shaders
  were erroneously duplicated in several GLSL programs, which caused a link
  time error.

# Release 0.3.1 (5-Jul-2013)

### Bug Fixes

* Fixed a problem with state management in scenes that have more complex
  scenegraphs than the cube grid example.
* Fixed an error in the detection of attribute changes in multi layer
  depth peeling

#  Release 0.3.0 (26-Jun-2013)

This is a maintenance release with no new notable features.

### API Changes

- Added a new parameter for algorithm configuration that lets the
  user reserve the first n texture unit for custom shading code.

### Bug Fixes

- [OSGTR-1](https://bbpteam.epfl.ch/project/issues/browse/OSGTR-1):
  Handling new sate sets and changes in shaders from
  previously reported state sets.

# Release 0.2.0 (21-Mar-2013)

### Enhancements

* During rendering, a shading programs is associated to each state set found
  in the scenegraph. Previously, it was mandatory to provide shaders for each
  state set using BaseRenderbin::addExtraShadersForState.  Now default shaders
  are created for those state sets the user hasn't provide any shaders. It is
  an error to provide shaders which don't follow the specification described
  in addExtraShadersForState.

### Optimizations

* Coarse grained sorting in state management. State set changes are now
  correctly applied only once per leaf list.
* Occlusion queries improved

### Examples

* Extra options added

### API Changes

* Algorithm parameter handling changed. No more internal global variables.

### Bug Fixes

* State management bugs fixed (applying state once per leaf is not needed
  anymore to ensure correctness).
* Support for Nvidia drivers version 310

# Release 0.1.0 (4-Dec-2012)

This is the first release of OSG Transparency, a library that provides 3 render
bins for transparent geometry.

### Known bugs

* The rendering of opaque and transparent geometry cannot be mixed.  Objects
  rendered with OSG Transparency will not be in correct depth with objects
  rendered by other render bin.

