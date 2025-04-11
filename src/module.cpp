// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <cstring>
#include <cmath>
#include <emmintrin.h>

using namespace DynLibUtils;

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : szModuleName (without extension .dll/.so)
//-----------------------------------------------------------------------------
CModule::CModule(const std::string_view szModuleName) : m_pHandle(nullptr)
{
	InitFromName(szModuleName);
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
CModule::CModule(const CMemory pModuleMemory) : m_pHandle(nullptr)
{
	InitFromMemory(pModuleMemory);
}

#ifndef DYNLIBUTILS_SEPARATE_SOURCE_FILES
	#if defined _WIN32 && _M_X64
		#include "module_windows.cpp"
	#elif defined __linux__ && __x86_64__
		#include "module_linux.cpp"
	#elif defined __APPLE__ && __x86_64__
		#include "module_apple.cpp"
	#else
		#error "Unsupported platform"
	#endif
#endif
