[![License: GPL-2.0-or-later](https://img.shields.io/badge/License-GPL%20v2+-blue.svg)](LICENSE.md)
[![Build and run tests](https://github.com/rbuehlma/pvr.zattoo/actions/workflows/build.yml/badge.svg?branch=Omega)](https://github.com/rbuehlma/pvr.zattoo/actions/workflows/build.yml)
[![Build Status](https://jenkins.kodi.tv/view/Addons/job/rbuehlma/job/pvr.zattoo/job/Omega/badge/icon)](https://jenkins.kodi.tv/blue/organizations/jenkins/rbuehlma%2Fpvr.zattoo/branches/)

# Zattoo PVR addon for Kodi

This is a [Kodi](https://kodi.tv) PVR addon for streaming live TV from [zattoo](https://zattoo.com).

## Build instructions

1. `git clone --branch master https://github.com/xbmc/xbmc`
2. `git clone --branch Omega https://github.com/rbuehlma/pvr.zattoo`
3. `cd pvr.zattoo && mkdir build && cd build`
4. `cmake -DADDONS_TO_BUILD=pvr.zattoo -DADDON_SRC_PREFIX=../.. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../../xbmc/addons -DPACKAGE_ZIP=1 ../../xbmc/cmake/addons`
5. `make package-pvr.zattoo`
