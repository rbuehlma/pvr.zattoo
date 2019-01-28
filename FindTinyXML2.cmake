# - Find TinyXML2
# Find the native TinyXML2 includes and library
#
#   TINYXML2_FOUND        - True if TinyXML2 found.
#   TINYXML2_INCLUDE_DIRS - where to find tinyxml2.h, etc.
#   TINYXML2_LIBRARIES    - List of libraries when using TinyXML2.
#

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_TINYXML2 tinyxml2 QUIET)
endif()

find_path(TINYXML2_INCLUDE_DIR tinyxml2.h PATHS ${PC_TINYXML2_INCLUDEDIR})
find_library(TINYXML2_LIBRARY NAMES tinyxml2 PATHS ${PC_TINYXML2_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TinyXML2
                                  REQUIRED_VARS TINYXML2_LIBRARY TINYXML2_INCLUDE_DIR
                                  VERSION_VAR ${PC_TINYXML2_VERSION})

if(TINYXML2_FOUND)
  set(TINYXML2_INCLUDE_DIRS ${TINYXML2_INCLUDE_DIR})
  set(TINYXML2_LIBRARIES ${TINYXML2_LIBRARY})
endif()

mark_as_advanced(TINYXML2_INCLUDE_DIR TINYXML2_LIBRARY)
