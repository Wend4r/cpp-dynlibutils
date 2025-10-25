//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/memaccessor.hpp>
#include <dynlibutils/memprotector.hpp>

namespace DynLibUtils {
	int TranslateProtection(ProtFlag flags) noexcept
	{
		int nativeFlag = VM_PROT_NONE;
		if (flags & ProtFlag::X)
			nativeFlag |= PROT_EXEC;

		if (flags & ProtFlag::R)
			nativeFlag |= PROT_READ;

		if (flags & ProtFlag::W)
			nativeFlag |= PROT_WRITE;

		if (flags & ProtFlag::N)
			nativeFlag = PROT_NONE;

		return nativeFlag;
	}

	ProtFlag TranslateProtection(int prot) noexcept
	{
		ProtFlag flags = ProtFlag::UNSET;

		if (prot & VM_PROT_EXECUTE)
			flags = flags | ProtFlag::X;

		if (prot & VM_PROT_READ)
			flags = flags | ProtFlag::R;

		if (prot & VM_PROT_WRITE)
			flags = flags | ProtFlag::W;

		if (prot == VM_PROT_NONE)
			flags = flags | ProtFlag::N;

		return flags;
	}
}