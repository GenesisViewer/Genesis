# Genesis-Eve

Genesis-Eve is a viewer for Second Life metaverse.

## Tools needed
These tools are needed to be able to build the project
 - [Python 3](https://www.python.org/downloads/) version >=3.9 (Enable the feature "Add python.exe to Path")
 - [CMake](http://www.cmake.org/download/) (Win32 Installer) (Select to Add to the system PATH)
 - [Visual Studio 2019 (Community)](https://visualstudio.microsoft.com/downloads/) (not preview) Select the Desktop Development with C++ workload
 - [NSIS 3](https://nsis.sourceforge.io/Download)+ [StdUtils (zip)](https://github.com/lordmulder/stdutils/releases) + [7z Plugin (top download)](https://nsis.sourceforge.io/Nsis7z_plug-in) + [INetC (top DL)](https://nsis.sourceforge.io/Inetc_plug-in) (Only if you plan to package for distribution) 

Extract StdUtils and move the Include directory into your NSIS install and
move Plugins\Unicode\StdUtils.dll to plugins\x86-unicode\ in your NSIS install
Extract the 7z Plugin and move the Plugins directory into your NSIS install directory
Extract the INetC Plugin and move the Plugins directory into your NSIS Install directory

## Install autobuild
pip install git+https://bitbucket.org/lindenlab/autobuild.git@v3

## Generating the project
autobuild configure -cRelease -A64 -- -DFMOD:BOOL=TRUE

## Building the project
autobuild build --no-configure -cRelease -A64

