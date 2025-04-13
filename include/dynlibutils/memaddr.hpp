// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#ifndef DYNLIBUTILS_MEMADDR_HPP
#define DYNLIBUTILS_MEMADDR_HPP

#pragma once

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
	constexpr CMemory& operator=(const CMemory&) noexcept = default;
	constexpr CMemory(CMemory&& other) noexcept : m_addr(std::move(other.m_addr)) {}
	constexpr CMemory(const std::uintptr_t addr) : m_addr(addr) {}
	constexpr CMemory(const void* ptr = nullptr) : m_ptr(ptr) {}

	/// Conversion operators.
	constexpr operator const void*() const noexcept { return GetPtr(); }
	constexpr operator std::uintptr_t() const noexcept { return GetAddr(); }

	/// Compare operators.
	bool operator==(const CMemory right) const noexcept { return m_addr == right.m_addr; }
	bool operator!=(const CMemory right) const noexcept { return !operator==(right); }
	bool operator<(const CMemory right) const noexcept { return m_addr < right.m_addr; }

	// Addition and subtraction operators.
	CMemory operator+(const std::size_t right) const noexcept { return Offset(right); }
	CMemory operator-(const std::size_t right) const noexcept { return Offset(-right); }
	CMemory operator+(const std::ptrdiff_t right) const noexcept { return Offset(right); }
	CMemory operator-(const std::ptrdiff_t right) const noexcept { return Offset(-right); }
	CMemory operator+(const CMemory right) const noexcept { return Offset(static_cast<std::ptrdiff_t>(right.m_addr)); }
	CMemory operator-(const CMemory right) const noexcept { return Offset(static_cast<std::ptrdiff_t>(right.m_addr)); }

	/// Cast methods.
	template<typename PTR> constexpr PTR CCast() const noexcept { return (PTR)m_addr; }
	template<typename PTR> constexpr PTR RCast() const noexcept { return reinterpret_cast<PTR>(m_addr); }
	template<typename PTR> constexpr PTR UCast() const noexcept { union { PTR cptr; std::uintptr_t addr; } cast; cast.addr = m_addr; return cast.cptr; }

	/// Access methods.
	constexpr const void* GetPtr() const noexcept { return m_ptr; }
	constexpr std::ptrdiff_t GetAddr() const noexcept { return m_addr; }
	template<typename T> constexpr T &GetRef() const noexcept { return *RCast<T*>(); }
	template<typename T> constexpr T Get() const { return GetRef<T>(); }

	// Checks methods.
	bool IsValid() const noexcept { return GetPtr() != nullptr; }

	// Offset methods.
	CMemory Offset(const std::ptrdiff_t offset) const noexcept { return m_addr + offset; }
	CMemory& OffsetSelf(const std::ptrdiff_t offset) noexcept { m_addr += offset; return *this; }

	// Multi-level dereferencing methods.
	CMemory Deref(std::uintptr_t deref = 1, std::ptrdiff_t offset = 0) const
	{
		std::uintptr_t base = m_addr;

		while (base && deref--)
		{
			base = *reinterpret_cast<std::uintptr_t*>(base + offset);
		}

		return base;
	}
	CMemory& DerefSelf(int deref = 1, std::ptrdiff_t offset = 0) { while (m_addr && deref--) m_addr = *reinterpret_cast<std::uintptr_t*>(m_addr + offset); return *this; }

	CMemory FollowNearCall(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5) const { return ResolveRelativeAddress(opcodeOffset, nextInstructionOffset); }
	CMemory& FollowNearCallSelf(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5) { return ResolveRelativeAddressSelf(opcodeOffset, nextInstructionOffset); }

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

protected:
	union
	{
		const void* m_ptr;
		std::uintptr_t m_addr;
	};
}; // class CMemory

template<typename T> 
class CMemoryView : protected CMemory
{
public:
	using CBase = CMemory;
	using CBase::CBase;

	constexpr CMemoryView& operator=(const CMemory& copyFrom) noexcept { static_cast<CBase>(*this) = copyFrom; };

	using Element_t = T;
	using CThis = CMemoryView<T>;

	// Addition and subtraction operators (view ones).
	CMemory operator+(const std::size_t right) const noexcept { return Offset(right); }
	CMemory operator-(const std::size_t right) const noexcept { return Offset(-right); }
	CMemory operator+(const std::ptrdiff_t right) const noexcept { return Offset(right); }
	CMemory operator-(const std::ptrdiff_t right) const noexcept { return Offset(-right); }
	CMemory operator+(const CMemory right) const noexcept { return Offset(static_cast<std::ptrdiff_t>(right.GetAddr())); }
	CMemory operator-(const CMemory right) const noexcept { return Offset(static_cast<std::ptrdiff_t>(right.GetAddr())); }

	using CMemory::IsValid;

	/// Cast methods (view ones).
	constexpr T* CCast() const noexcept { return CBase::CCast<T*>(); }
	constexpr T* RCast() const noexcept { return CBase::RCast<T*>(); }
	constexpr T* UCast() const noexcept { CBase::UCast<T*>(); }

	/// Access methods (view ones).
	constexpr T* GetPtr() const noexcept { return CBase::RCast<T*>(); }
	constexpr std::uintptr_t GetAddr() const noexcept { return CBase::RCast<T*>(); }
	constexpr T& GetRef() const noexcept { return *GetPtr(); }
	constexpr T Get() const { return GetRef(); }

	/// Offset methods (view ones; operators are used).
	CThis Offset(std::ptrdiff_t offset) const noexcept { return reinterpret_cast<CThis>(reinterpret_cast<T *>(CBase::m_addr) + offset); }
	CThis& OffsetSelf(std::ptrdiff_t offset) noexcept { reinterpret_cast<T *>(CBase::m_addr) += offset; return *this; }
}; // class CMemoryView

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MEMADDR_HPP
