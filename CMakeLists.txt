project(pvr.zattoo)

cmake_minimum_required(VERSION 2.8)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(platform REQUIRED)
find_package(JsonCpp REQUIRED)


include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
					${JSONCPP_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib)

set(DEPLIBS ${kodiplatform_LIBRARIES} 
		${platform_LIBRARIES}
		${JSONCPP_LIBRARIES}
)

set(ZATTOO_SOURCES
		lib/tinyxml2/tinyxml2.h
		lib/tinyxml2/tinyxml2.cpp
		src/client.cpp
		src/ZatData.cpp
		 )

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)

