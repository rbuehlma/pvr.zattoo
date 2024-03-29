#.rst:
# FindSqlite3
# -----------
# Finds the SQLite3 library
#
# This will define the following variables::
#
# SQLITE3_FOUND - system has SQLite3
# SQLITE3_INCLUDE_DIRS - the SQLite3 include directory
# SQLITE3_LIBRARIES - the SQLite3 libraries

if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_SQLITE3 sqlite3 QUIET)
endif()

find_path(SQLITE3_INCLUDE_DIR NAMES sqlite3.h
                              PATHS ${PC_SQLITE3_INCLUDEDIR})
find_library(SQLITE3_LIBRARY NAMES sqlite3
                             PATHS ${PC_SQLITE3_LIBDIR})

set(SQLITE3_VERSION ${PC_SQLITE3_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Sqlite3
                                  REQUIRED_VARS SQLITE3_LIBRARY SQLITE3_INCLUDE_DIR
                                  VERSION_VAR SQLITE3_VERSION)

if(SQLITE3_FOUND)
  set(SQLITE3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIR})
  set(SQLITE3_LIBRARIES ${SQLITE3_LIBRARY})
endif()

mark_as_advanced(SQLITE3_INCLUDE_DIR SQLITE3_LIBRARY)
