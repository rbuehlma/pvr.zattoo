cmake_minimum_required(VERSION 3.5)
project(pvr.zattoo)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

find_package(PkgConfig)
find_package(Kodi REQUIRED)
find_package(RapidJSON 1.0.2 REQUIRED)
find_package(TinyXML2 REQUIRED)

include_directories(${KODI_INCLUDE_DIR}
                    ${RAPIDJSON_INCLUDE_DIRS}
                    ${TINYXML2_INCLUDE_DIRS}
)

set(DEPLIBS ${TINYXML2_LIBRARIES}
)

set(ZATTOO_SOURCES
		src/Utils.cpp
		src/client.cpp
		src/Curl.cpp
		src/Cache.cpp
		src/Settings.cpp
		src/ZatData.cpp
		src/UpdateThread.cpp
		src/XmlTV.cpp
		src/categories.cpp
		src/md5.cpp
)

set(ZATTOO_HEADERS
		src/Cache.h
		src/Curl.h
		src/UpdateThread.h
		src/XmlTV.h
		src/Settings.h
		src/Utils.h
		src/ZatChannel.h
		src/ZatData.h
		src/categories.h
		src/client.h
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
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -DZATTOO_VERSION=${ZATTOO_VERSION} -DKODI_VERSION=${kodiplatform_VERSION}")

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)
