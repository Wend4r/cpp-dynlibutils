//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_PROTFLAG_HPP
#define DYNLIBUTILS_PROTFLAG_HPP
#pragma once

#include <cstdint>
#include <type_traits>

namespace DynLibUtils
{
	/**
	 * @enum ProtFlag
	 * @brief Enum representing memory protection flags.
	 */
	enum ProtFlag
	{
		UNSET = 0, /**< Value means this gives no information about protection state (un-read) */
		X = 1 << 1, /**< Execute permission */
		R = 1 << 2, /**< Read permission */
		W = 1 << 3, /**< Write permission */
		S = 1 << 4, /**< Shared memory */
		P = 1 << 5, /**< Private memory */
		N = 1 << 6, /**< Value equaling the Linux flag PROT_UNSET (read the prot, and the prot is unset) */
		RWX = R | W | X /**< Read, Write, and Execute permissions */
	};

	/**
	 * @brief Overloads the binary OR operator for ProtFlag.
	 *
	 * @param lhs The left-hand side ProtFlag value.
	 * @param rhs The right-hand side ProtFlag value.
	 * @return The combined ProtFlag value.
	 */
	constexpr ProtFlag operator|(ProtFlag lhs, ProtFlag rhs) noexcept
	{
		using underlying = typename std::underlying_type<ProtFlag>::type;
		return static_cast<ProtFlag> (
				static_cast<underlying>(lhs) | static_cast<underlying>(rhs)
		);
	}

	/**
	 * @brief Overloads the binary AND operator for ProtFlag.
	 *
	 * @param lhs The left-hand side ProtFlag value.
	 * @param rhs The right-hand side ProtFlag value.
	 * @return True if lhs contains rhs, false otherwise.
	 */
	constexpr bool operator&(ProtFlag lhs, ProtFlag rhs) noexcept
	{
		using underlying = typename std::underlying_type<ProtFlag>::type;
		return static_cast<underlying>(lhs) & static_cast<underlying>(rhs);
	}

	/**
	 * @brief Bitwise OR assignment operator for ProtFlag enum class.
	 *
	 * @param lhs Left-hand side ProtFlag.
	 * @param rhs Right-hand side ProtFlag.
	 * @return Reference to the left-hand side ProtFlag.
	 */
	constexpr ProtFlag& operator|=(ProtFlag& lhs, ProtFlag rhs) noexcept
	{
		lhs = lhs | rhs;
		return lhs;
	}
} // namespace DynLibUtils

#endif // DYNLIBUTILS_PROTFLAG_HPP
