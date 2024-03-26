#ifndef DYNLIBUTILS_VIRTUAL_HPP
#define DYNLIBUTILS_VIRTUAL_HPP
#ifdef _WIN32
#pragma once
#endif

#include <stddef.h>

namespace DynLibUtils {

class VirtualTable
{
public:
	template<typename T>
	inline T GetMethod(ptrdiff_t nIndex)
	{
		return reinterpret_cast<T>((*(void ***)(this))[nIndex]);
	}

	template<typename T, typename... Args>
	inline T CallMethod(ptrdiff_t nIndex, Args... args)
	{
		return this->GetMethod<T (*)(VirtualTable *, Args...)>(nIndex)(this, args...);
	}
};

} // namespace DynLibUtils

#endif // DYNLIBUTILS_VIRTUAL_HPP
