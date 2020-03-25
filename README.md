[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE.md)

# Zattoo PVR addon for Kodi

This is a [Kodi](https://kodi.tv) PVR addon for streaming live TV from [zattoo](https://zattoo.com).

## Build instructions

1. `git clone --branch master https://github.com/xbmc/xbmc`
2. `git clone https://github.com/rbuehlma/pvr.zattoo`
3. `cd pvr.zattoo && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.zattoo -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make package-pvr.zattoo`
