cmake_minimum_required(VERSION 3.5)
project(pvr.zattoo)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(PkgConfig)
find_package(Kodi REQUIRED)
find_package(RapidJSON 1.0.2 REQUIRED)
find_package(Sqlite3 REQUIRED)
find_package(TinyXML2 REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${RAPIDJSON_INCLUDE_DIRS}
                    ${SQLITE3_INCLUDE_DIRS}
                    ${TINYXML2_INCLUDE_DIRS}
)

set(DEPLIBS ${SQLITE3_LIBRARIES}
            ${TINYXML2_LIBRARIES})

set(ZATTOO_SOURCES
    src/sql/SQLConnection.cpp
    src/sql/EpgDB.cpp
    src/sql/RecordingsDB.cpp
    src/sql/ParameterDB.cpp
    src/http/Curl.cpp
    src/http/Cache.cpp
    src/http/HttpClient.cpp
    src/epg/EpgProvider.cpp
    src/epg/ZattooEpgProvider.cpp
    src/Utils.cpp
    src/Settings.cpp
    src/ZatData.cpp
    src/Session.cpp
    src/UpdateThread.cpp
    src/categories.cpp
    src/md5.cpp
)

set(ZATTOO_HEADERS
    src/sql/SQLConnection.h
    src/sql/EpgDB.h
    src/sql/RecordingsDB.h
    src/sql/ParameterDB.h
    src/http/Curl.h
    src/http/Cache.h
    src/http/HttpClient.h
    src/epg/EpgProvider.h
    src/epg/ZattooEpgProvider.h
    src/UpdateThread.h
    src/Settings.h
    src/Utils.h
    src/ZatChannel.h
    src/ZatData.h
    src/Session.h
    src/categories.h
    src/windows.h
    src/md5.h
    src/to_string.h
)

if(WIN32)
  list(APPEND DEPLIBS ws2_32)
endif()

if ( CMAKE_COMPILER_IS_GNUCC )
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall")
endif ( CMAKE_COMPILER_IS_GNUCC )
if ( MSVC )
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} /W4")
endif ( MSVC )

addon_version(pvr.zattoo ZATTOO)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DZATTOO_VERSION=${ZATTOO_VERSION} -DKODI_VERSION=${APP_VERSION_CODE}")

build_addon(pvr.zattoo ZATTOO DEPLIBS)

set_target_properties(pvr.zattoo PROPERTIES CXX_VISIBILITY_PRESET internal
                                            C_VISIBILITY_PRESET internal
                                            VISIBLITY_INLINES_HIDDEN ON)

include(CPack)
