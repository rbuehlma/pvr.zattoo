project(pvr.zattoo)

cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(CURL REQUIRED)
find_package(JsonCpp REQUIRED)



include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
					${CURL_INCLUDE_DIRS}
					${JSONCPP_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib)

set(DEPLIBS ${kodiplatform_LIBRARIES} 
		${p8-platform_LIBRARIES}
		${CURL_LIBRARIES}
		${JSONCPP_LIBRARIES}
)


if(NOT WIN32 AND (CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX))
  if(HAS_BUILTIN_SYNC_ADD_AND_FETCH)
    add_definitions("-DHAS_BUILTIN_SYNC_ADD_AND_FETCH")
  endif()
  if(HAS_BUILTIN_SYNC_SUB_AND_FETCH)
    add_definitions("-DHAS_BUILTIN_SYNC_SUB_AND_FETCH")
  endif()
endif()


set(ZATTOO_SOURCES
		lib/tinyxml2/tinyxml2.h
		lib/tinyxml2/tinyxml2.cpp
		src/ZatData.cpp
		src/client.cpp )

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)

