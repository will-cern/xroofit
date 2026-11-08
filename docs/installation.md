# Installation and Setup

This guide describes how to set up and compile xRooFit, run it via Docker, and integrate it into external CMake projects.

## Recommended Setup: StatAnalysis

The recommended way to use xRooFit is via the [ATLAS StatAnalysis](https://gitlab.cern.ch/atlas/StatAnalysis) release, where xRooFit comes pre-compiled alongside ROOT and other statistical tools.

[![Latest Release](https://gitlab.cern.ch/atlas/StatAnalysis/-/badges/release.svg)](https://gitlab.cern.ch/atlas/StatAnalysis/-/releases)

### Via Docker

Run an interactive shell inside the StatAnalysis Docker container:

```bash
docker run -it gitlab-registry.cern.ch/atlas/statanalysis:0-2-1
```
*(Adjust the tag numbers to select a more recent release).*

### On CERN / ATLAS Systems

ATLAS users can set up StatAnalysis directly on CVMFS:

```bash
export ATLAS_LOCAL_ROOT_BASE=/cvmfs/atlas.cern.ch/repo/ATLASLocalRootBase
source ${ATLAS_LOCAL_ROOT_BASE}/user/atlasLocalSetup.sh
asetup StatAnalysis,0.8,latest
```

## Interactive xRooBrowser via Docker

To run the interactive `xRooBrowser` GUI from Docker and open local workspace files without installing ROOT locally:

Define a shell function in your `.bashrc` / `.zshrc`:

```bash
xRooBrowser() {
    xhost +${hostname}
    docker run --rm --pull always -v "$1":/"$(basename "$1")" \
        -e DISPLAY=host.docker.internal:0 -it \
        gitlab-registry.cern.ch/atlas/statanalysis:0-2 \
        root -e 'xRooBrowser b; b.Open("'$(basename "$1")'")'
}
```

Then launch it on any ROOT or JSON workspace file:

```bash
xRooBrowser myWorkspace.root
```

## Compiling from Source

### Prerequisites

- A recent ROOT installation (ROOT ≥ 6.22 minimum requirement; ROOT ≥ 6.32 recommended, 6.38+ ideal)
- CMake (≥ 3.27 recommended)
- C++17 compatible compiler
- Python 3

### Build Instructions

1. Clone the repository:
   ```bash
   git clone https://github.com/will-cern/xroofit.git
   # or from CERN GitLab:
   # git clone ssh://git@gitlab.cern.ch:7999/will/xroofit.git
   ```

2. Configure and compile:
   ```bash
   mkdir build && cd build
   cmake ../xroofit
   make -j
   ```

3. Set up the environment:
   ```bash
   source setup.sh
   ```
   Sourcing `setup.sh` adds the library and Python bindings to your environment (`PATH`, `LD_LIBRARY_PATH`, `PYTHONPATH`).

4. Verify the installation:
   ```bash
   python3 -c "import ROOT; print(ROOT.xRooFit.GetVersion())"
   ```

## Building External Projects Against xRooFit

Here is an example `CMakeLists.txt` and `test.cxx` demonstrating how to build a C++ application against xRooFit, supporting both standalone xRooFit and the version bundled inside ROOT (`ROOT::Experimental::XRooFit`).

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.27)
project(TestProject)

find_package(xRooFit QUIET)
if(NOT xRooFit_FOUND)
    set(_xRooFitComp "RooFitXRooFit")
    message(STATUS "Using xRooFit from ROOT")
else()
    message(STATUS "Using xRooFit standalone")
    add_definitions(-DSA_XROOFIT) # Used in C++, see below
endif()

find_package(ROOT COMPONENTS RooFit ${_xRooFitComp} REQUIRED)

add_executable(testApp test.cxx)
target_include_directories(testApp PUBLIC ${ROOT_INCLUDE_DIRS} ${XROOFIT_INCLUDE_DIRS})
target_link_libraries(testApp PUBLIC ${ROOT_LIBRARIES} ${XROOFIT_LIBRARIES})
```

### test.cxx

```cpp
#include <iostream>

#ifdef SA_XROOFIT
// Standalone xRooFit
#include "xRooFit/xRooFit.h"
#else
// Bundled inside ROOT
#include "RooFit/xRooFit/xRooFit.h"
using namespace ROOT::Experimental::XRooFit;
#endif

int main() {
    std::cout << "xRooFit version: " << xRooFit::GetVersion()
              << " (" << xRooFit::GetVersionDate() << ")" << std::endl;
    return 0;
}
```

### Direct Target Linking (without `make install`)

If you build xRooFit in a sibling directory and source `setup.sh`, you can link against the imported `xRooFit` CMake target directly:

```cmake
cmake_minimum_required(VERSION 3.27)
project(TestProject)

find_package(xRooFit REQUIRED)

add_executable(testApp test.cxx)
target_link_libraries(testApp PRIVATE xRooFit)
```

