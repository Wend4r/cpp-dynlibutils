//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_DEFER_HPP
#define DYNLIBUTILS_DEFER_HPP
#pragma once

#include <type_traits>

namespace DynLibUtils {
	template<typename F>
	struct Defer
	{
		F f;

		Defer(F&& arg) noexcept(std::is_nothrow_move_constructible_v<F>)
			: f(std::move(arg))
		{}

		~Defer() noexcept(std::is_nothrow_invocable_v<F>)
		{
			f();
		}
	};

	template <typename F>
	Defer(F f) -> Defer<std::decay_t<F>>;
} // namespace DynLibUtils

#endif // DYNLIBUTILS_DEFER_HPP