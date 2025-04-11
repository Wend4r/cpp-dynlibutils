// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#ifndef DYNLIBUTILS_VIRTUAL_HPP
#define DYNLIBUTILS_VIRTUAL_HPP

#pragma once

namespace DynLibUtils {

class CVirtualTable
{
public: // Types.
	using CThis = CVirtualTable;

public: // Constructors.
	CVirtualTable() : m_pVTFs(nullptr) {}
	template<class T> CVirtualTable(T *pClass) : m_pVTFs(reinterpret_cast<void **>(pClass)) {}

public: // Getters.
	template<typename R> R &GetMethod(std::ptrdiff_t nIndex) { return reinterpret_cast<R &>(m_pVTFs[nIndex]); }
	template<typename R> R GetMethod(std::ptrdiff_t nIndex) const { return reinterpret_cast<R>(m_pVTFs[nIndex]); }

public: // Callers.
	template<typename R, typename... Args> R CallMethod(std::ptrdiff_t nIndex, Args... args) { return GetMethod<R (*)(void *, Args...)>(nIndex)(m_pVTFs, args...); }
	template<typename R, typename... Args> R CallMethod(std::ptrdiff_t nIndex, Args... args) const { return const_cast<CThis *>(this)->CallMethod(nIndex, args...); }

#if __has_cpp_attribute(no_unique_address)
	[[no_unique_address]] 
#endif
	void **m_pVTFs;
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
