# - C++ Requests, Curl for People
# This module is a libcurl wrapper written in modern C++.
# It provides an easy, intuitive, and efficient interface to
# a host of networking methods.
#
# Finding this module will define the following variables:
#  CPR_FOUND - True if the core library has been found
#  CPR_LIBRARIES - Path to the core library archive
#  CPR_INCLUDE_DIRS - Path to the include directories. Gives access
#                     to cpr.h, which must be included in every
#                     file that uses this interface

if(NOT CPR_FIND_VERSION)
  set(CPR_FIND_VERSION 1.2.0)
endif()

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_CPR cpr>=${CPR_FIND_VERSION} QUIET)
endif()

find_path(CPR_INCLUDE_DIR
            NAMES cpr.h
            PATHS ${PC_CPR_INCLUDEDIR})

find_library(CPR_LIBRARY
             NAMES cpr
             PATHS ${PC_CPR_LIBDIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CPR REQUIRED_VARS CPR_LIBRARY CPR_INCLUDE_DIR)

if(CPR_FOUND)
    set(CPR_LIBRARIES ${CPR_LIBRARY})
    set(CPR_INCLUDE_DIRS ${CPR_INCLUDE_DIR})
endif()
