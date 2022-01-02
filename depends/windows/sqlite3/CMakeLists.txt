cmake_minimum_required(VERSION 3.13)

project(sqlite3 VERSION 3.38.3 LANGUAGES C)

add_library(${PROJECT_NAME}
	sqlite3.h
	sqlite3ext.h
	sqlite3.c
)

if(CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
	target_compile_definitions(sqlite3
		PRIVATE
		SQLITE_OS_WINRT
	)
endif()

install(TARGETS sqlite3 EXPORT sqlite3
	RUNTIME DESTINATION bin
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)

install(FILES
	${CMAKE_CURRENT_SOURCE_DIR}/sqlite3.h
	${CMAKE_CURRENT_SOURCE_DIR}/sqlite3ext.h
	DESTINATION include
)
