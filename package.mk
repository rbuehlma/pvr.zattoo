################################################################################
#      This file is part of LibreELEC - https://LibreELEC.tv
#      Copyright (C) 2016 Team LibreELEC
#
#  LibreELEC is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  LibreELEC is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with LibreELEC.  If not, see <http://www.gnu.org/licenses/>.
################################################################################

PKG_NAME="pvr.zattoo"
PKG_VERSION="3bfa6ea"
PKG_REV="1"
PKG_ARCH="any"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/rbuehlma/pvr.zattoo"
PKG_URL="https://github.com/rbuehlma/pvr.zattoo/archive/$PKG_VERSION.tar.gz"
PKG_DEPENDS_TARGET="toolchain kodi-platform tinyxml2 yajl curl"
PKG_PRIORITY="optional"
PKG_SECTION=""
PKG_SHORTDESC="pvr.zattoo"
PKG_LONGDESC="pvr.zattoo"
PKG_AUTORECONF="no"

PKG_IS_ADDON="yes"
PKG_ADDON_TYPE="xbmc.pvrclient"

#pre_configure_target() {
#  export YAJL_INCLUDE_DIR=$(get_build_dir yajl)/
#  echo "$YAJL_INCLUDE_DIR"
#}

configure_target() {
  cmake -DCMAKE_TOOLCHAIN_FILE=$CMAKE_CONF \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_MODULE_PATH=$SYSROOT_PREFIX/usr/lib/kodi \
        -DCMAKE_PREFIX_PATH=$SYSROOT_PREFIX/usr \
        ..
}

addon() {
  mkdir -p $ADDON_BUILD/$PKG_ADDON_ID/
  cp -R $PKG_BUILD/.install_pkg/usr/share/kodi/addons/$PKG_NAME/* $ADDON_BUILD/$PKG_ADDON_ID/

  ADDONSO=$(xmlstarlet sel -t -v "/addon/extension/@library_linux" $ADDON_BUILD/$PKG_ADDON_ID/addon.xml)
  cp -L $PKG_BUILD/.install_pkg/usr/lib/kodi/addons/$PKG_NAME/$ADDONSO $ADDON_BUILD/$PKG_ADDON_ID/
}
