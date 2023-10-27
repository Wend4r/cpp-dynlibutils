#ifndef VIRTUAL_H
#define VIRTUAL_H
#ifdef _WIN32
#pragma once
#endif

#include <stddef.h>

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

#endif // VIRTUAL_H
