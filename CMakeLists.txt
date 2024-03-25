cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

if(POLICY CMP0092)
	cmake_policy(SET CMP0092 NEW) # Don't add -W3 warning level by default.
endif()

project(memory_utils
	VERSION 1.0.0
	DESCRIPTION "DynLibUtils"
	HOMEPAGE_URL "https://github.com/Wend4r/cpp-memory_utils"
	LANGUAGES C CXX
)

include("cmake/platform/shared.cmake")

if(LINUX)
	include("cmake/platform/linux.cmake")
elseif(WINDOWS)
	include("cmake/platform/windows.cmake")
endif()

set(SOURCE_FILES
	module.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
