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

#define DYNLIB_INVALID_MEMORY DynLibUtils::CMemory(nullptr)

namespace DynLibUtils {

class CMemory
{
public:
	// Constructor ones.
	constexpr CMemory(const CMemory&) noexcept = default;
	constexpr CMemory& operator= (const CMemory&) noexcept = default;
	constexpr CMemory(CMemory&& other) noexcept : m_addr(std::move(other.m_addr)) {}
	constexpr CMemory(const std::uintptr_t addr) : m_addr(addr) {}
	constexpr CMemory(const void* ptr = nullptr) : m_ptr(ptr) {}

	/// Conversion operators.
	constexpr operator const void*() const noexcept { return GetPointer(); }
	constexpr operator std::uintptr_t() const noexcept { return GetAddress(); }

	/// Compare operators.
	bool operator==(const CMemory& comp) const noexcept { return m_addr == comp.m_addr; }
	bool operator!=(const CMemory& comp) const noexcept { return !operator==(comp); }
	bool operator<(const CMemory& comp) const noexcept { return m_addr < comp.m_addr; }

	/// Cast methods.
	template<typename T> constexpr T CCast() const noexcept { return (T)m_addr; }
	template<typename T> constexpr T RCast() const noexcept { return reinterpret_cast<T>(m_addr); }
	template<typename T> constexpr T UCast() const noexcept { union { T cptr; std::uintptr_t m_addr; } cast; return cast.m_addr = m_addr, cast.cptr; }

	/// Access methods.
	constexpr const void* GetPointer() const noexcept { return m_ptr; }
	constexpr std::ptrdiff_t GetAddress() const noexcept { return m_addr; }
	template<class T> constexpr T Get() const noexcept { return *UCast<T*>(); }
	template<class T> constexpr T GetValue() const noexcept { return Get<T>(); }

	// Checks methods.
	bool IsValid() const noexcept { return GetPointer() != nullptr; }

	// Offset methods.
	CMemory Offset(std::ptrdiff_t offset) const noexcept { return m_addr + offset; }
	CMemory& OffsetSelf(std::ptrdiff_t offset) noexcept { m_addr += offset; return *this; }
	CMemory Deref(std::uintptr_t deref = 1) const
	{
		std::uintptr_t reference = m_addr;

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
			if (m_addr)
				m_addr = *reinterpret_cast<std::uintptr_t*>(m_addr);
		}

		return *this;
	}

	CMemory FollowNearCall(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5) const
	{
		return ResolveRelativeAddress(opcodeOffset, nextInstructionOffset);
	}

	CMemory& FollowNearCallSelf(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5)
	{
		return ResolveRelativeAddressSelf(opcodeOffset, nextInstructionOffset);
	}

	CMemory ResolveRelativeAddress(const std::ptrdiff_t registerOffset = 0x0, const std::ptrdiff_t nextInstructionOffset = 0x4) const
	{
		const std::uintptr_t skipRegister = m_addr + registerOffset;
		const std::uintptr_t nextInstruction = m_addr + nextInstructionOffset;
		const std::int32_t relativeAddress = *reinterpret_cast<std::int32_t*>(skipRegister);

		return nextInstruction + relativeAddress;
	}

	CMemory& ResolveRelativeAddressSelf(const std::ptrdiff_t registerOffset = 0x0, const std::ptrdiff_t nextInstructionOffset = 0x4)
	{
		const std::uintptr_t skipRegister = m_addr + registerOffset;
		const std::uintptr_t nextInstruction = m_addr + nextInstructionOffset;
		const std::int32_t relativeAddress = *reinterpret_cast<std::int32_t*>(skipRegister);

		m_addr = nextInstruction + relativeAddress;

		return *this;
	}

private:
	union
	{
		const void* m_ptr;
		std::uintptr_t m_addr;
	};
}; // class CMemory

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MEMADDR_HPP
