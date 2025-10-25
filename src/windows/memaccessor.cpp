//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/memaccessor.hpp>
#include <dynlibutils/memprotector.hpp>

#include <cstring>

using namespace DynLibUtils;

bool CMemAccessor::MemCopy(CMemory dest, CMemory src, size_t size)
{
	std::memcpy(dest, src, size);
	return true;
}

bool CMemAccessor::SafeMemCopy(CMemory dest, CMemory src, size_t size, size_t& written) noexcept
{
	written = 0;
	return WriteProcessMemory(GetCurrentProcess(), dest, src, (SIZE_T)size, (PSIZE_T)&written);
}

bool CMemAccessor::SafeMemRead(CMemory src, CMemory dest, size_t size, size_t& read) noexcept
{
	HANDLE process = GetCurrentProcess();
	read = 0;

	if (ReadProcessMemory(process, src, dest, size, (PSIZE_T)&read) && read > 0)
		return true;

	// Tries to read again on a partial copy, but limited by the end of the memory region
	if (GetLastError() == ERROR_PARTIAL_COPY)
	{
		MEMORY_BASIC_INFORMATION info;
		if (VirtualQueryEx(process, src, &info, sizeof(info)) != 0)
		{
			uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
			if (src + size > end)
				return ReadProcessMemory(process, src, dest, end - src, (PSIZE_T)&read) && read > 0;
		}
	}
	return false;
}

ProtFlag CMemAccessor::MemProtect(CMemory dest, size_t size, ProtFlag prot, bool& status)
{
	DWORD orig;
	status = VirtualProtect(dest, size, TranslateProtection(prot), &orig) != 0;
	return TranslateProtection(static_cast<int>(prot));
}
