cmake_minimum_required(VERSION 3.5)
project(sqlite3)

include(ExternalProject)
externalproject_add(sqlite3
                    SOURCE_DIR ${CMAKE_SOURCE_DIR}
                    UPDATE_COMMAND autoreconf -vif
                    CONFIGURE_COMMAND <SOURCE_DIR>/configure
                      --prefix=${OUTPUT_DIR}
                      --disable-readline
                      --disable-shared
                      "CFLAGS=-fPIC"
                    INSTALL_COMMAND ""
                    BUILD_IN_SOURCE 1)

install(CODE "execute_process(COMMAND make install WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})")
