// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#ifndef DYNLIBUTILS_VIRTUAL_HPP
#define DYNLIBUTILS_VIRTUAL_HPP
#ifdef _WIN32
#pragma once
#endif

#include "memaddr.hpp"

namespace DynLibUtils {

class CVirtualTable
{
public: // Types.
	using CThis = CVirtualTable;

public: // Constructor.
	CVirtualTable() = default;
	template<class T> CVirtualTable(T *pClass) { this = reinterpret_cast<CThis *>(pClass); }

public: // Getters.
	template<typename R> R GetMethod(ptrdiff_t nIndex) const { return reinterpret_cast<R>((*CMemory(this).RCast<void ***>())[nIndex]); }

public: // Call methods.
	template<typename R, typename... Args> R CallMethod(ptrdiff_t nIndex, Args... args) { return this->GetMethod<R (*)(CThis *, Args...)>(nIndex)(this, args...); }
	template<typename R, typename... Args> R CallMethod(ptrdiff_t nIndex, Args... args) const { return const_cast<CThis *>(this)->CallMethod(nIndex, args...); }
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
