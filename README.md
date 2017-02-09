# Zattoo PVR addon for Kodi

This is a [Kodi] (http://kodi.tv) PVR addon for streaming live TV from [zattoo](https://zattoo.com).

## Build instructions

1. `git clone -b zattoo https://github.com/rbuehlma/xbmc`
2. `git clone -b zattoo https://github.com/rbuehlma/pvr.zattoo.git`
3. `cd pvr.zattoo && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.zattoo -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/project/cmake/addons`
5. `make package-pvr.zattoo`
