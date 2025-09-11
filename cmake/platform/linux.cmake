# DynLibUtils
# Copyright (C) 2023-2025 Wend4r & komashchenko
# Licensed under the MIT license. See LICENSE file in the project root for details.

set(PLATFORM_COMPILE_OPTIONS
	${PLATFORM_COMPILE_OPTIONS}

	-Wall
	-Wno-array-bounds -Wno-attributes

	-mtune=generic -mmmx -msse -msse2 -msse3 -msse4 -msse4.1 -msse4.2
	-fvisibility=default -fPIC
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(PLATFORM_COMPILE_OPTIONS
		${PLATFORM_COMPILE_OPTIONS}

		-g3 -ggdb
	)
endif()

set(PLATFORM_COMPILE_DEFINITIONS
	${PLATFORM_COMPILE_DEFINITIONS}

	_GLIBCXX_USE_CXX11_ABI=$<IF:$<BOOL:${DYNLIBUTILS_USE_ABI0}>,0,1>
)
