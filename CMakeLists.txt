project(pvr.zattoo)

cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(ADDONS_PREFER_STATIC_LIBS ON)

enable_language(CXX)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(Yajl REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${YAJL_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}/lib)

set(DEPLIBS ${kodiplatform_LIBRARIES} 
            ${p8-platform_LIBRARIES}
            ${YAJL_LIBRARIES}
)

set(ZATTOO_SOURCES
		lib/tinyxml2/tinyxml2.h
		lib/tinyxml2/tinyxml2.cpp
		src/Utils.cpp
		src/client.cpp
		src/ZatData.cpp
		src/JsonParser.cpp
		 )

if(WIN32)
	list(APPEND DEPLIBS ws2_32)
endif()

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)


