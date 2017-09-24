project(pvr.zattoo)

cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(PkgConfig)
find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(RapidJSON REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${RAPIDJSON_INCLUDE_DIRS}
)

set(DEPLIBS ${kodiplatform_LIBRARIES} 
            ${p8-platform_LIBRARIES}
)

set(ZATTOO_SOURCES
		src/Utils.cpp
		src/client.cpp
		src/Curl.cpp
		src/Cache.cpp
		src/ZatData.cpp
		src/UpdateThread.cpp
		src/categories.cpp
		src/md5.cpp
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

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)
