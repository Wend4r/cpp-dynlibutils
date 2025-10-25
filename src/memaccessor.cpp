//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifdef DYNLIBUTILS_SEPARATE_SOURCE_FILES
	#if DYNLIBUTILS_PLATFORM_WINDOWS
		#include "windows/memaccessor.cpp"
	#elif DYNLIBUTILS_PLATFORM_LINUX
		#include "linux/memaccessor.cpp"
	#elif DYNLIBUTILS_PLATFORM_APPLE
		#include "apple/memaccessor.cpp"
	#else
		#error "Unsupported platform"
	#endif
#endif
