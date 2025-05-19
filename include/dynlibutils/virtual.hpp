// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#ifndef DYNLIBUTILS_VIRTUAL_HPP
#define DYNLIBUTILS_VIRTUAL_HPP

#pragma once

#include "memaddr.hpp"

#include <cstddef>
#include <cstdint>
#include <bit>
#include <type_traits>

#define DYNLIB_INVALID_VCALL -1

namespace DynLibUtils {

template<auto METHOD>
constexpr std::ptrdiff_t GetVirtualIndex() noexcept
{
	static_assert(std::is_member_function_pointer_v<decltype(METHOD)>, "Templated method must be a pointer-to-member-function");

	// --- Itanium C++ ABI: PMF.ptr = 1 + offset_in_bytes for virtual ones ---
	struct ItaniumPMF
	{
		union
		{
			std::ptrdiff_t addr;
			void* ptr;
		};

		std::ptrdiff_t adj;
	};
	constexpr union { decltype(METHOD) m; ItaniumPMF pmf; } u{ METHOD };

#if defined(_MSC_VER)
	auto *pAddr = reinterpret_cast<unsigned char *>(u.pmf.ptr);

	// Skip JMP-thunk
	if (*pAddr == 0xE9)
	{
		// May or may not be!
		// Check where it'd jump
		pAddr += 5 /*size of the instruction*/ + *(unsigned long*)(pAddr + 1);
	}

	// Check whether it's a virtual function call
	// They look like this:
	// 004125A0 8B 01            mov         eax,dword ptr [ecx]
	// 004125A2 FF 60 04         jmp         dword ptr [eax+4]
	//		==OR==
	// 00411B80 8B 01            mov         eax,dword ptr [ecx]
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// However, for vararg functions, they look like this:
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 0048F0B6 FF 60 08         jmp         dword ptr [eax+8]
	//		==OR==
	// 0048F0B0 8B 44 24 04      mov         eax,dword ptr [esp+4]
	// 0048F0B4 8B 00            mov         eax,dword ptr [eax]
	// 00411B82 FF A0 18 03 00 00 jmp         dword ptr [eax+318h]

	// With varargs, the this pointer is passed as if it was the first argument

	bool ok = false;

	if (pAddr[0] == 0x8B && pAddr[1] == 0x44 && pAddr[2] == 0x24 && pAddr[3] == 0x04 &&
	    pAddr[4] == 0x8B && pAddr[5] == 0x00)
	{
		pAddr += 6; ok = true;
	}
	else if (pAddr[0] == 0x8B && pAddr[1] == 0x01)
	{
		pAddr += 2; ok = true;
	}
	else if (pAddr[0] == 0x48 && pAddr[1] == 0x8B && pAddr[2] == 0x01)
	{
		pAddr += 3; ok = true;
	}

	if (!ok)
		return DYNLIB_INVALID_VCALL;

	// FF /60 /A0 /20
	if (*pAddr++ == 0xFF)
	{
		if (*pAddr == 0x60)
		{
			return static_cast<std::ptrdiff_t>(*++pAddr) / sizeof(void *);
		}
		else if (*pAddr == 0xA0)
		{
			return static_cast<std::ptrdiff_t>(*reinterpret_cast<unsigned int*>(++pAddr)) / sizeof(void *);
		}
		else if (*pAddr == 0x20)
		{
			return 0;
		}
		else
		{
			return DYNLIB_INVALID_VCALL;
		}
	}

#elif defined(__GNUG__) || defined(__clang__)
	return (u.pmf.addr - 1u) / sizeof(void*);
#else
	static_assert(false, "Unsupported compiler");
#endif

	return DYNLIB_INVALID_VCALL;
}

class CVirtualTable : public CMemoryView<void *>
{
public: // Types.
	using CBase = CMemoryView<void *>;
	using CThis = CVirtualTable;

public: // Constructors.
	CVirtualTable() : CBase(nullptr) {}
	template<class T> CVirtualTable(T *pClass) : CBase(*reinterpret_cast<void **>(pClass)) {}

public: // Getters.
	template<typename R> R GetMethod(std::ptrdiff_t nIndex) const { return reinterpret_cast<R>(CBase::Offset(nIndex).GetPtr()); }

public: // Callers.
	template<typename R, typename... Args> R CallMethod(std::ptrdiff_t nIndex, Args... args) { return GetMethod<R (*)(void *, Args...)>(nIndex)(this, args...); }
	template<typename R, typename... Args> R CallMethod(std::ptrdiff_t nIndex, Args... args) const { return const_cast<CThis *>(this)->CallMethod(nIndex, args...); }
}; // class CVirtualTable

class VirtualTable final : public CVirtualTable
{
public:
	using CBase = CVirtualTable;
	using CBase::CBase;
	// ...
}; // class VirtualTable

} // namespace DynLibUtils

#endif // DYNLIBUTILS_VIRTUAL_HPP
