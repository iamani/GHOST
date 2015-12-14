[![Build Status](https://drone.io/bitbucket.org/essex/ghost/status.png)](https://drone.io/bitbucket.org/essex/ghost/latest)
---
# Disclaimer #
---

This is a pre-alpha release, so **please do not expect anything to work!** Just kidding... But some things may, and probably will, be broken. 
Nevertheless, please report any bugs, issues and feature requests to the [issue tracker](https://bitbucket.org/essex/ghost/issues).

---
# What is GHOST? #
---

GHOST stands for General, Hybrid and Optimized Sparse Toolkit. It provides
basic building blocks for computation with very large sparse or dense matrices. GHOST
is being developed as part of the [ESSEX](http://blogs.fau.de/essex/) project
under the umbrella of the Priority Programme 1648: Software for Exascale
Computing ([SPPEXA](http://www.sppexa.de/)) of the German Research Foundation (DFG).

---
# Examples #
---

Have a look at the [GHOST-Apps](https://bitbucket.org/essex/ghost-apps) and the [PHYSICS](https://bitbucket.org/essex/physics) project for some example applications using GHOST.

Also check out [PHIST](https://bitbucket.org/essex/phist), the sparse iterative solver toolkit
from the ESSEX project which supports GHOST as a kernel library.

---
# Dependencies and optional packages #
---

1. A C/C++ compiler
1. Perl (for automatic code generation)
1. [CMake](http://www.cmake.org) >= 2.8
1. [hwloc](http://www.open-mpi.org/projects/hwloc) >= 1.7 (older versions *may* work as well) ([Install notes](https://bitbucket.org/essex/ghost/wiki/Dependencies))
1. A BLAS library (e.g., [Intel MKL](http://software.intel.com/en-us/intel-mkl) (preferred) or [GSL](http://www.gnu.org/software/gsl/))

In order to use GHOST at its best and make use of the full functionality 
you can decide to enable (some of) the following optional dependencies:

1. An OpenMP-3.1-capable C/C++ compiler
1. MPI ([Tested versions](https://bitbucket.org/essex/ghost/wiki/Compatibility))
1. [CUDA](http://www.nvidia.com/cuda) for employing GPU computation ([Tested versions](https://bitbucket.org/essex/ghost/wiki/Compatibility))
1. [SCOTCH](http://www.labri.fr/perso/pelegrin/scotch/) for sparse matrix re-ordering ([Tested versions](https://bitbucket.org/essex/ghost/wiki/Compatibility), [Install notes](https://bitbucket.org/essex/ghost/wiki/Dependencies))
1. [ColPack](http://cscapes.cs.purdue.edu/coloringpage/software.htm) for sparse matrix coloring which is required for the CARP kernel.
1. A LAPACKE library. If MKL is used as the BLAS lib, LAPACKE is present automatically.

---
# Documentation #
---

Please note that the documentation is currently incomplete.
Have a look at the Doxygen pages of GHOST for [API documentation](http://mkreutzer.bitbucket.org/ghost_doc/) and also [information on specific topics](http://mkreutzer.bitbucket.org/ghost_doc/pages.html).

More information can be found in the [Wiki](https://bitbucket.org/essex/ghost/wiki) pages on Bitbucket.

Also, the API documentation can be generated by switching to the build/ directory of your checked out code (see installation documentation below) and type `make doc` (Doxygen must be available).
You can then open `doc/html/index.html` with a web browser.

---
# Installation #
---

First of all, clone the git repository:

`git clone git@bitbucket.org:essex/ghost.git && cd ghost/`

It is preferrable to perform an out-of-source build, i.e., create a build directory first:

`mkdir build && cd build/`

You can find [installation instructions for specific machines](https://bitbucket.org/essex/ghost/wiki/Installation%20instructions) in the wiki.
To do a quick build with the system compilers, MPI and OpenMP enabled, and the default settings for code generation:

`cmake .. -DCMAKE_INSTALL_PREFIX=<where-to-install>`

Once the Makefile is present you can type

`make && make install`

You can toggle shared/static libs with `-DBUILD_SHARED_LIBS=ON/OFF` (default: shared).

For interactive specification of build options and variables, use ccmake to configure and generate a Makefile:

`ccmake ..`

See the information about [Code generation](http://mkreutzer.bitbucket.org/ghost_docs/md__home_hpc_unrz_unrza317_proj_ESSEX_ghost_doxygen_codegeneration.html) for details on how to generate fast kernels.

If you do not want to use the system compilers, invoke (c)cmake as follows (e.g., Intel Compilers):

`CC=icc CXX=icpc ccmake ..`

The build system will try to find a Cblas header `*cblas.h` in default locations.
In some cases (if MKL or GSL Cblas is detected), a hint about the necessary BLAS libraries will be created for the future build of executables which link to GHOST.
If finding a Cblas header fails, or if you want to overwrite the found Cblas header, you can pass `-DCBLAS_INCLUDE_DIR=<dir-of-cblas-header>` to (c)cmake or set the value in ccmake.
If the detected Cblas is neither MKL or GSL, the variable `BLAS_LIBRARIES` has to be set manually before linking any application to GHOST.

The same applies for hwloc and the variables `HWLOC_INCLUDE_DIR` and `HWLOC_LIBRARIES`.