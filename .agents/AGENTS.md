# Custom Rules for xRooFit Workspace

## Project requirements

This project requires ROOT and Python, and to compile it requires cmake as well. When first working with this project, check the user has ROOT installed with `root --version` and cmake installed with `cmake --version`. If the user does not, ask them for a recommendation of what setup script they should source in order to get the correct environment. If they have access to cvmfs they can also setup StatAnalysis which provides ROOT and Python with:

```bash
export ATLAS_LOCAL_ROOT_BASE=/cvmfs/atlas.cern.ch/repo/ATLASLocalRootBase
source ${ATLAS_LOCAL_ROOT_BASE}/user/atlasLocalSetup.sh
asetup StatAnalysis,0.8,latest
```

## Compiling the Project
If a `build` directory does not already exist, you should first create it and then compile the project with:

```
cmake -S xroofit -B build
cmake --build build
```

After the cmake configuration, the created `setup.sh` script in the build directory should be sourced so that ROOT will correctly find and load the libraries.

## Environment Setup
Whenever you run terminal commands in this workspace using the `run_command` tool, you must always prepend the command with the project's setup environment:
`source build/setup.sh && <command>`

## Verifying the Library Loading and Version
To verify that the project is correctly configured and the xRooFit library can be loaded, run:
```bash
python3 -c "import ROOT; print(ROOT.xRooFit.GetVersion())"
```
This should output the version string matching `build/versioning/xRooFitVersion.h`.

> [!NOTE]
> If a conflict arises with the host system's default ROOT (e.g. Homebrew ROOT version mismatch causing `fatal error: 'TError.h' file not found`), agents must first source the StatAnalysis setup script (either the local one at `~/CLionProjects/StatAnalysis/install-0.8/setup.sh` or the cvmfs equivalent) to load the correct compiler, Python, and ROOT versions.

