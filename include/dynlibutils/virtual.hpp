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

#define DYNLIB_INVALID_VMETHOD_INDEX -1

namespace DynLibUtils {

template<auto METHOD, class T>
inline std::ptrdiff_t GetVirtualMethodIndex(T *pClass) noexcept
{
	static_assert(std::is_member_function_pointer_v<decltype(METHOD)>, "Templated method must be a pointer-to-member-function");

#if defined(_MSC_VER)
	// --- MSVC ABI: runtime scan vtable (no constexpr!) ---
	struct MSVCPMF { void* ptr; std::ptrdiff_t adj; };
	union { decltype(METHOD) m; MSVCPMF pmf; } u{ METHOD };
	void* target = u.pmf.ptr;

	void** vtbl = *reinterpret_cast<void***>(pClass);

	constexpr std::size_t header = 2; // [RTTI][offset-to-top]

	for (std::size_t n = header; ; ++n)
		if (vtbl[n] == target)
			return n - header;

#elif defined(__GNUG__) || defined(__clang__)
	// --- Itanium C++ ABI: PMF.ptr = 1 + offset_in_bytes for virtual ones ---
	struct ItaniumPMF { void* ptr; std::ptrdiff_t adj; };
	union { decltype(METHOD) m; ItaniumPMF pmf; } u{ METHOD };

	constexpr auto raw = reinterpret_cast<std::uintptr_t>(u.pmf.ptr);

	static_assert((raw & 1u) != 0, "Not a virtual member function");

	return (raw - 1u) / sizeof(void*);
#else
	static_assert(false, "Unsupported compiler");
#endif

	return DYNLIB_INVALID_VMETHOD_INDEX;
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
	template<typename R> R &GetMethod(std::ptrdiff_t nIndex) { return CBase::Offset(nIndex).RCast<R &>(); }
	template<typename R> R GetMethod(std::ptrdiff_t nIndex) const { return CBase::Offset(nIndex).RCast<R>(); }

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
