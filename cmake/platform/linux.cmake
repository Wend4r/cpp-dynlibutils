# DynLibUtils
# Copyright (C) 2023-2025 Wend4r & komashchenko
# Licensed under the MIT license. See LICENSE file in the project root for details.

set(PLATFORM_COMPILE_OPTIONS
	${PLATFORM_COMPILE_OPTIONS}

	-Wall
	-Wno-array-bounds -Wno-attributes

	-mtune=generic -mmmx -msse -msse2 -msse3 -msse4 -msse4.1 -msse4.2 -mavx2
	-fvisibility=default -fPIC
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(PLATFORM_COMPILE_OPTIONS
			${PLATFORM_COMPILE_OPTIONS}
			-g3 -gdwarf-4 -fno-omit-frame-pointer -fno-inline
	)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		set(PLATFORM_COMPILE_OPTIONS
				${PLATFORM_COMPILE_OPTIONS}
				-fstandalone-debug -glldb
		)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(PLATFORM_COMPILE_OPTIONS
				${PLATFORM_COMPILE_OPTIONS}
				-ggdb3
				-fno-eliminate-unused-debug-types
				-femit-class-debug-always
				-fvar-tracking
				-fvar-tracking-assignments
				-grecord-gcc-switches
		)
	endif()
endif()


set(PLATFORM_COMPILE_DEFINITIONS
	${PLATFORM_COMPILE_DEFINITIONS}

	_GLIBCXX_USE_CXX11_ABI=$<IF:$<BOOL:${DYNLIBUTILS_USE_ABI0}>,0,1>
)
