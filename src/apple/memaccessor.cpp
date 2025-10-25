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
	bool res = std::memcpy(dest, src, size) != nullptr;
	if (res)
		written = size;
	else
		written = 0;

	return res;
}

bool CMemAccessor::SafeMemRead(CMemory src, CMemory dest, size_t size, size_t& read) noexcept
{
	bool res = std::memcpy(dest, src, size) != nullptr;
	if (res)
		read = size;
	else
		read = 0;

	return res;
}

ProtFlag CMemAccessor::MemProtect(CMemory dest, size_t size, ProtFlag prot, bool& status)
{
	static auto pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
	status = mach_vm_protect(mach_task_self(), static_cast<mach_vm_address_t>(MemoryRound(dest, pageSize)), static_cast<mach_vm_size_t>(MemoryRoundUp(size, pageSize)), FALSE, TranslateProtection(prot)) == KERN_SUCCESS;
	return ProtFlag::R | ProtFlag::X;
}
