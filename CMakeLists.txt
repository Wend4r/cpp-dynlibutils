# DynLibUtils
# Copyright (C) 2023-2024 Wend4r & komashchenko
# Licensed under the MIT license. See LICENSE file in the project root for details.

cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0092)
	cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()

project(cpp-memory_utils
	VERSION 1.0.0
	DESCRIPTION "DynLibUtils"
	HOMEPAGE_URL "https://github.com/Wend4r/cpp-memory_utils"
	LANGUAGES C CXX
)

string(TOLOWER "${PROJECT_NAME}" PROJECT_NAME_LOWER)
string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)

set(PROJECT_OUTPUT_NAME "dynutils")

set(PROJECT_AUTHOR "Wend4r & komashchenko")
set(PROJECT_DESCRIPTION_FULL "A library for interacting with dynamically loaded libraries (DLL, SO or DYLIB)")
set(PROJECT_LICENSE "MIT")
string(TIMESTAMP PROJECT_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP PROJECT_BUILD_TIME "%H:%M:%S")

option(DYNLIBUTILS_USE_ABI0 "Enable use of the older C++ ABI, which was the default in GCC versions before GCC 5" ON)

set(EXTERNAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

include(cmake/platform/shared.cmake)

if(WINDOWS)
	include(cmake/platform/windows.cmake)
elseif(LINUX)
	include(cmake/platform/linux.cmake)
elseif(MACOS)
	include(cmake/platform/macos.cmake)
endif()

set(COMPILE_DEFINITIONS
	DYNLIBUTILS_SEPARATE_SOURCE_FILES
	DYNLIBUTILS_PLATFORM_LINUX=$<BOOL:${LINUX}>
	DYNLIBUTILS_PLATFORM_WINDOWS=$<BOOL:${WIN32}>
	DYNLIBUTILS_PLATFORM_APPLE=$<BOOL:${APPLE}>
)

set(INCLUDE_DIRS
	${INCLUDE_DIR}
)

if(WINDOWS)
	set(SOURCE_FILES
			${SOURCE_DIR}/windows/memaccessor.cpp
			${SOURCE_DIR}/windows/memprotector.cpp
			${SOURCE_DIR}/windows/module.cpp
	)
elseif(LINUX)
	set(SOURCE_FILES
			${SOURCE_DIR}/linux/memaccessor.cpp
			${SOURCE_DIR}/linux/memprotector.cpp
			${SOURCE_DIR}/linux/module.cpp
	)
elseif(MACOS)
	set(SOURCE_FILES
			${SOURCE_DIR}/apple/memaccessor.cpp
			${SOURCE_DIR}/apple/memprotector.cpp
			${SOURCE_DIR}/apple/module.cpp
	)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

list(APPEND SOURCE_FILES
		${SOURCE_DIR}/memaccessor.cpp
		${SOURCE_DIR}/memprotector.cpp
		${SOURCE_DIR}/module.cpp # always include last
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
	C_STANDARD 17
	C_STANDARD_REQUIRED ON
	C_EXTENSIONS OFF

	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_OUTPUT_NAME})

if(WINDOWS)
	set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(MACOS)
	set_target_properties(${PROJECT_NAME} PROPERTIES OSX_ARCHITECTURES "x86_64")
endif()

target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILE_OPTIONS} ${PLATFORM_COMPILE_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${LINKER_OPTIONS} ${PLATFORM_LINKER_OPTIONS})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFINITIONS} ${PLATFORM_COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME} PRIVATE ${LINK_LIBRARIES} ${CMAKE_DL_LIBS})
