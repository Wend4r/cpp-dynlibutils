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
template<typename Mutex>
CAssemblyModule<Mutex>::CAssemblyModule(const std::string_view szModuleName)
{
	InitFromName(szModuleName);
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
template<typename Mutex>
CAssemblyModule<Mutex>::CAssemblyModule(const CMemory pModuleMemory)
{
	InitFromMemory(pModuleMemory);
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetFunctionByName(const std::string_view svFunctionName) const noexcept
{
	CCache hKey(svFunctionName, 1);
	if (auto pAddr = GetAddress(hKey))
	{
		return pAddr;
	}
	auto pAddr = GetFunction(svFunctionName);
	{
		UniqueLock_t lock(m_mutex);
		m_mapCached[std::move(hKey)] = pAddr;
	}
	return pAddr;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetVirtualTableByName(const std::string_view svTableName, bool bDecorated) const
{
	CCache hKey(svTableName, 2);
	if (auto pAddr = GetAddress(hKey))
	{
		return pAddr;
	}
	auto pAddr = GetVirtualTable(svTableName, bDecorated);
	{
		UniqueLock_t lock(m_mutex);
		m_mapCached[std::move(hKey)] = pAddr;
	}
	return pAddr;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetAddress(const CCache& hKey) const noexcept
{
	SharedLock_t lock(m_mutex);
	auto it = m_mapCached.find(hKey);
	if (it == m_mapCached.end())
	{
		return nullptr;
	}
	return it->second;
}

#ifdef DYNLIBUTILS_SEPARATE_SOURCE_FILES
	#if defined _WIN32 && _M_X64
		#include "linux/windows.cpp"
	#elif defined __linux__ && __x86_64__
		#include "linux/module.cpp"
	#elif defined __APPLE__ && __x86_64__
		#include "apple/module.cpp"
	#else
		#error "Unsupported platform"
	#endif
#endif

template class CAssemblyModule<CNullMutex>;
template class CAssemblyModule<std::shared_mutex>;