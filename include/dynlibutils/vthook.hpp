//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r) & komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_VTHOOK_HPP
#define DYNLIBUTILS_VTHOOK_HPP
#pragma once

#include "memaddr.hpp"

#if __unix__
#	include <sys/mman.h>
#	include <unistd.h>
#endif

#include <cassert>

namespace DynLibUtils {

class MemUnprotector
{
public:
	MemUnprotector(CMemory pAddress, size_t nLength = sizeof(void*))
	{
#if _WIN32
		VirtualProtect(pAddress, nLength, PAGE_EXECUTE_READWRITE, &m_nOldProtection);

		m_pAddress = pAddress;
		m_nLength = nLength;
#else
		long nPageSize = sysconf(_SC_PAGESIZE);
		CMemory pPageStart = pAddress & ~(nPageSize - 1l);
		CMemory pPageEnd = (pAddress + nLength + nPageSize - 1l) & ~(nPageSize - 1l);
		size_t nAligned = pPageEnd - pPageStart;

		mprotect(pPageStart, nAligned, PROT_READ | PROT_WRITE | PROT_EXEC);

		m_pAddress = pPageStart;
		m_nLength = nAligned;
		m_nOldProtection = PROT_READ | PROT_WRITE; //TODO: Need to parse /proc/self/maps
#endif
	}

	~MemUnprotector()
	{
#if _WIN32
		DWORD origProtect;
		VirtualProtect(m_pAddress, m_nLength, m_nOldProtection, &origProtect);
#else
		mprotect(m_pAddress, m_nLength, m_nOldProtection);
#endif
	}

private:
	size_t m_nLength;
	unsigned long m_nOldProtection;

	CMemory m_pAddress;
}; // class MemUnprotector

template <typename T, typename C, typename ...Args>
class VTHook
{
public:
	VTHook() = default;
	~VTHook() { Unhook(); }

	bool IsHooked() const { return m_pOriginalFn.IsValid(); }

	void Hook(CMemory pVTable, int index, T(*pFn)(C*, Args...))
	{
		assert(!IsHooked());

		m_vmpFn = pVTable.Offset(index * sizeof(void*));
		m_pOriginalFn = m_vmpFn.Deref();

		MemUnprotector unprotector(m_vmpFn);

		*m_vmpFn.RCast<T(**)(C*, Args...)>() = pFn;
	}

	void Unhook()
	{
		assert(IsHooked());

		MemUnprotector unprotector(m_vmpFn);

		*m_vmpFn.RCast<void**>() = m_pOriginalFn;

		m_vmpFn = nullptr;
		m_pOriginalFn = nullptr;
	}

	T Call(C* pThis, Args... args) { return m_pOriginalFn.RCast<T(*)(C*, Args...)>()(pThis, args...); }

private:
	CMemory m_vmpFn;
	CMemory m_pOriginalFn;
}; // class VTHook

} // namespace DynLibUtils

#endif // DYNLIBUTILS_VTHOOK_HPP
