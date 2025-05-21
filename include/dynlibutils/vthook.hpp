//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r) & komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_VTHOOK_HPP
#define DYNLIBUTILS_VTHOOK_HPP
#pragma once

#include "memaddr.hpp"
#include "virtual.hpp"

#if _WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	undef WIN32_LEAN_AND_MEAN
#else
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace DynLibUtils {

using ProtectFlags_t = unsigned long;

class VirtualUnprotector
{
public:
	VirtualUnprotector(void *pTarget, std::size_t nLength = sizeof(void*))
	{
#if _WIN32
		m_nLength = nLength;
		m_pTarget = pTarget;

		assert(VirtualProtect(pTarget, nLength, PAGE_EXECUTE_READWRITE, &m_nOldProtect));
#else
		long pageSize = sysconf(_SC_PAGESIZE);

		assert(pageSize >= 0);

		auto nPageSize = static_cast<std::uintptr_t>(pageSize);

		auto pAddress = reinterpret_cast<std::uintptr_t>(pTarget);
		CMemory        pPageStart = pAddress & ~(nPageSize - 1l);
		std::uintptr_t pPageEnd = (pAddress + nLength + nPageSize - 1l) & ~(nPageSize - 1l);
		auto nAligned = static_cast<std::size_t>(pPageEnd - pPageStart);

		m_nOldProtect = PROT_READ;
		m_nLength = nAligned;
		m_pTarget = pPageStart;

		assert(!mprotect(pPageStart, nAligned, PROT_READ | PROT_WRITE));
#endif
	}

	~VirtualUnprotector()
	{
#if _WIN32
		DWORD origProtect;
		assert(VirtualProtect(m_pTarget, m_nLength, m_nOldProtect, &origProtect));
#else
		assert(!mprotect(m_pTarget, m_nLength, m_nOldProtect));
#endif
	}

private:
	ProtectFlags_t m_nOldProtect;
	std::size_t m_nLength;
	CMemory m_pTarget;
}; // class VirtualUnprotector

template <typename T, typename C, typename ...Args>
class VTHook
{
public:
	VTHook() = default;
	~VTHook() { Unhook(); }

	void Clear()
	{
		m_vmpFn = nullptr;
		m_pOriginalFn = nullptr;
	}

	bool IsHooked() const { return m_pOriginalFn.IsValid(); }
	T Call(C* pThis, Args... args) { return m_pOriginalFn.RCast<T(*)(C*, Args...)>()(pThis, args...); }

	void Hook(CVirtualTable pVTable, std::ptrdiff_t nIndex, T(*pFn)(C*, Args...))
	{
		assert(!IsHooked());

		m_vmpFn = &pVTable.GetMethod<void *>(nIndex);
		m_pOriginalFn = m_vmpFn.Deref();

		HookImpl(pFn);
	}

	void Unhook()
	{
		assert(IsHooked());

		UnhookImpl();
		Clear();
	}

protected:
	void HookImpl(T(*pfnTarget)(C*, Args...))
	{
		VirtualUnprotector unprotect(m_vmpFn);

		*m_vmpFn.RCast<T(**)(C*, Args...)>() = pfnTarget;
	}

	void UnhookImpl()
	{
		VirtualUnprotector unprotect(m_vmpFn);

		*m_vmpFn.RCast<void **>() = m_pOriginalFn;
	}

private:
	CMemory m_vmpFn;
	CMemory m_pOriginalFn;
}; // class VTHook

} // namespace DynLibUtils

#endif // DYNLIBUTILS_VTHOOK_HPP
