//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include <dynlibutils/memaccessor.hpp>
#include <dynlibutils/memprotector.hpp>

using namespace DynLibUtils;

CMemProtector::CMemProtector(CMemory address, size_t length, ProtFlag prot, bool unsetOnDestroy)
	: m_address{address}
	, m_length{length}
	, m_status{false}
	, m_unsetLater{unsetOnDestroy} {
	m_origProtection = CMemAccessor::MemProtect(address, length, prot, m_status);
}

CMemProtector::~CMemProtector() {
	if (m_origProtection == ProtFlag::UNSET || !m_unsetLater)
		return;

	CMemAccessor::MemProtect(m_address, m_length, m_origProtection, m_status);
}
