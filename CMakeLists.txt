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

set(INCLUDE_DIR "include")
set(SOURCE_DIR "src")

include("cmake/platform/shared.cmake")

if(LINUX)
	include("cmake/platform/linux.cmake")
elseif(WINDOWS)
	include("cmake/platform/windows.cmake")
endif()

set(COMPILE_DEFINITIONS
	${COMPILE_DEFINITIONS}
	-DDYNLIBUTILS_SEPARATE_SOURCE_FILES
)

set(SOURCE_FILES
	${SOURCE_DIR}/module.cpp
)

if(LINUX)
	list(APPEND SOURCE_FILES ${SOURCE_DIR}/module_linux.cpp)
elseif(WINDOWS)
	list(APPEND SOURCE_FILES ${SOURCE_DIR}/module_windows.cpp)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})

set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY C_EXTENSIONS OFF)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_EXTENSIONS OFF)

target_compile_options(${PROJECT_NAME} PRIVATE ${COMPILER_OPTIONS})
target_link_options(${PROJECT_NAME} PRIVATE ${LINKER_OPTIONS})

target_compile_definitions(${PROJECT_NAME} PRIVATE ${COMPILE_DEFINITIONS})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIR})
