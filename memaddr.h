#ifndef MEMADDR_H
#define MEMADDR_H
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>
#include <stddef.h>

class CMemory
{
public:
	CMemory() = default;
	CMemory(const uintptr_t ptr) : ptr(ptr) {}
	CMemory(const void* ptr) : ptr(uintptr_t(ptr)) {}

	inline operator uintptr_t() const
	{
		return ptr;
	}

	inline operator void*() const
	{
		return reinterpret_cast<void*>(ptr);
	}

	inline operator bool() const
	{
		return ptr != 0;
	}

	inline bool operator!= (const CMemory& addr) const
	{
		return ptr != addr.ptr;
	}

	inline bool operator== (const CMemory& addr) const
	{
		return ptr == addr.ptr;
	}

	inline bool operator== (const uintptr_t& addr) const
	{
		return ptr == addr;
	}

	inline uintptr_t GetPtr() const
	{
		return ptr;
	}

	template<class T> inline T GetValue() const
	{
		return *reinterpret_cast<T*>(ptr);
	}

	template<typename T> inline T CCast() const
	{
		return (T)ptr;
	}

	template<typename T> inline T RCast() const
	{
		return reinterpret_cast<T>(ptr);
	}

	inline CMemory Offset(ptrdiff_t offset) const
	{
		return CMemory(ptr + offset);
	}

	inline CMemory OffsetSelf(ptrdiff_t offset)
	{
		ptr += offset;
		return *this;
	}

	inline CMemory Deref(int deref = 1) const
	{
		uintptr_t reference = ptr;

		while (deref--)
		{
			if (reference)
				reference = *reinterpret_cast<uintptr_t*>(reference);
		}

		return CMemory(reference);
	}

	inline CMemory DerefSelf(int deref = 1)
	{
		while (deref--)
		{
			if (ptr)
				ptr = *reinterpret_cast<uintptr_t*>(ptr);
		}

		return *this;
	}

	CMemory FollowNearCall(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5) const;
	CMemory FollowNearCallSelf(const ptrdiff_t opcodeOffset = 0x1, const ptrdiff_t nextInstructionOffset = 0x5);
	CMemory ResolveRelativeAddress(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4) const;
	CMemory ResolveRelativeAddressSelf(const ptrdiff_t registerOffset = 0x0, const ptrdiff_t nextInstructionOffset = 0x4);

private:
	uintptr_t ptr = 0;
};

#endif // MEMADDR_H
