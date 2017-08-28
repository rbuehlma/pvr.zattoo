project(pvr.zattoo)

cmake_minimum_required(VERSION 2.8.12)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR})
set(PC_RapidJSON_INCLUDEDIR ${PROJECT_SOURCE_DIR}/depends/target/rapidjson/native/build/dummy/include)

enable_language(CXX)

execute_process(
	COMMAND make PREFIX=dummy
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/depends/target/rapidjson/
)

find_package(Kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(p8-platform REQUIRED)
find_package(RapidJSON REQUIRED)

include_directories(${kodiplatform_INCLUDE_DIRS}
                    ${p8-platform_INCLUDE_DIRS}
                    ${KODI_INCLUDE_DIR}
                    ${RAPIDJSON_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR}/lib)

set(DEPLIBS ${kodiplatform_LIBRARIES} 
            ${p8-platform_LIBRARIES}
)

set(ZATTOO_SOURCES
		lib/tinyxml2/tinyxml2.h
		lib/tinyxml2/tinyxml2.cpp
		src/Utils.cpp
		src/client.cpp
		src/Curl.cpp
		src/ZatData.cpp
		src/UpdateThread.cpp
		src/categories.cpp
)

if(WIN32)
	list(APPEND DEPLIBS ws2_32)
endif()

build_addon(pvr.zattoo ZATTOO DEPLIBS)

include(CPack)
