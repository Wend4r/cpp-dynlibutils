# DynLibUtils
# Copyright (C) 2023-2025 Wend4r & komashchenko
# Licensed under the MIT license. See LICENSE file in the project root for details.

set(PLATFORM_COMPILE_OPTIONS
	${PLATFORM_COMPILE_OPTIONS}

	-Wall
	-Wno-attributes

	-fvisibility=default -fPIC
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(PLATFORM_COMPILE_OPTIONS
		${PLATFORM_COMPILE_OPTIONS}

		-g3 -ggdb
	)
endif()
