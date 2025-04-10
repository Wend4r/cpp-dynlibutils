// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#ifndef DYNLIBUTILS_MEMADDR_HPP
#define DYNLIBUTILS_MEMADDR_HPP
#ifdef _WIN32
#pragma once
#endif

#include <cstdint>
#include <cstddef>
#include <utility>

namespace DynLibUtils {

class CMemory
{
public:
	CMemory() : m_ptr(0) {}
	CMemory(const CMemory&) noexcept = default;
	CMemory& operator= (const CMemory&) noexcept = default;
	CMemory(CMemory&& other) noexcept : m_ptr(std::exchange(other.m_ptr, 0)) {}
	CMemory(const std::uintptr_t ptr) : m_ptr(ptr) {}
	CMemory(const void* ptr) : m_ptr(reinterpret_cast<std::uintptr_t>(ptr)) {}

	operator std::uintptr_t() const noexcept
	{
		return m_ptr;
	}

	operator void*() const noexcept
	{
		return reinterpret_cast<void*>(m_ptr);
	}

	bool operator!= (const CMemory& addr) const noexcept
	{
		return m_ptr != addr.m_ptr;
	}

	bool operator== (const CMemory& addr) const noexcept
	{
		return m_ptr == addr.m_ptr;
	}

	bool operator== (const std::uintptr_t& addr) const noexcept
	{
		return m_ptr == addr;
	}

	[[nodiscard]] std::uintptr_t GetPtr() const noexcept
	{
		return m_ptr;
	}

	template<class T> [[nodiscard]] T GetValue() const noexcept
	{
		return *reinterpret_cast<T*>(m_ptr);
	}

	template<typename T> [[nodiscard]] T CCast() const noexcept
	{
		return (T)m_ptr;
	}

	template<typename T> [[nodiscard]] T RCast() const noexcept
	{
		return reinterpret_cast<T>(m_ptr);
	}

	template<typename T> [[nodiscard]] T UCast() const noexcept
	{
		union { uintptr_t m_ptr; T cptr; } cast;
		return cast.m_ptr = m_ptr, cast.cptr;
	}

	[[nodiscard]] CMemory Offset(std::ptrdiff_t offset) const noexcept
	{
		return m_ptr + offset;
	}

	CMemory& OffsetSelf(std::ptrdiff_t offset) noexcept
	{
		m_ptr += offset;
		return *this;
	}

	[[nodiscard]] CMemory Deref(std::uintptr_t deref = 1) const
	{
		std::uintptr_t reference = m_ptr;

		while (deref--)
		{
			if (reference)
				reference = *reinterpret_cast<std::uintptr_t*>(reference);
		}

		return reference;
	}

	CMemory& DerefSelf(int deref = 1)
	{
		while (deref--)
		{
			if (m_ptr)
				m_ptr = *reinterpret_cast<std::uintptr_t*>(m_ptr);
		}

		return *this;
	}

	[[nodiscard]] CMemory FollowNearCall(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5) const
	{
		return ResolveRelativeAddress(opcodeOffset, nextInstructionOffset);
	}

	CMemory& FollowNearCallSelf(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5)
	{
		return ResolveRelativeAddressSelf(opcodeOffset, nextInstructionOffset);
	}

	[[nodiscard]] CMemory ResolveRelativeAddress(const std::ptrdiff_t registerOffset = 0x0, const std::ptrdiff_t nextInstructionOffset = 0x4) const
	{
		const std::uintptr_t skipRegister = m_ptr + registerOffset;
		const std::int32_t relativeAddress = *reinterpret_cast<std::int32_t*>(skipRegister);
		const std::uintptr_t nextInstruction = m_ptr + nextInstructionOffset;
		return nextInstruction + relativeAddress;
	}

	CMemory& ResolveRelativeAddressSelf(const std::ptrdiff_t registerOffset = 0x0, const std::ptrdiff_t nextInstructionOffset = 0x4)
	{
		const std::uintptr_t skipRegister = m_ptr + registerOffset;
		const std::int32_t relativeAddress = *reinterpret_cast<std::int32_t*>(skipRegister);
		const std::uintptr_t nextInstruction = m_ptr + nextInstructionOffset;
		m_ptr = nextInstruction + relativeAddress;

		return *this;
	}

private:
	std::uintptr_t m_ptr = 0;
}; // class CMemory

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MEMADDR_HPP
