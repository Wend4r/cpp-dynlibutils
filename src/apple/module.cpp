//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

typedef struct mach_header_64 MachHeader;
typedef struct segment_command_64 MachSegment;
typedef struct section_64 MachSection;
const uint32_t MACH_MAGIC = MH_MAGIC_64;
const uint32_t MACH_LOADCMD_SEGMENT = LC_SEGMENT_64;
const cpu_type_t MACH_CPU_TYPE = CPU_TYPE_X86_64;
const cpu_subtype_t MACH_CPU_SUBTYPE = CPU_SUBTYPE_X86_64_ALL;

typedef void * NSModule;

struct dlopen_handle {
	dev_t dev;		/* the path's device and inode number from stat(2) */
	ino_t ino;
	int dlopen_mode;	/* current dlopen mode for this handle */
	int dlopen_count;	/* number of times dlopen() called on this handle */
	NSModule module;	/* the NSModule returned by NSLinkModule() */
	struct dlopen_handle *prev;
	struct dlopen_handle *next;
};

using namespace DynLibUtils;

template<typename Mutex>
CAssemblyModule<Mutex>::~CAssemblyModule()
{
	if (IsValid())
		dlclose(GetPtr());
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module name
// Input  : svModuleName
//          bExtension
// Output : bool
//-----------------------------------------------------------------------------
template<typename Mutex>
bool CAssemblyModule<Mutex>::InitFromName(const std::string_view svModuleName, bool bExtension)
{
	if (IsValid())
		return false;

	if (svModuleName.empty())
		return false;

	std::string sModuleName(svModuleName);

	if (!bExtension)
		sModuleName.append(".dylib");

	SetPtr(dlopen(sModuleName.c_str(), RTLD_LAZY));

	return IsValid();
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module memory
// Input  : pModuleMemory
// Output : bool
//-----------------------------------------------------------------------------
template<typename Mutex>
bool CAssemblyModule<Mutex>::InitFromMemory(const CMemory& pModuleMemory, bool bForce)
{
	if (IsValid() && !bForce)
		return false;

	if (!pModuleMemory.IsValid())
		return false;

	Dl_info info;
	if (!dladdr(pModuleMemory, &info) || !info.dli_fbase || !info.dli_fname)
		return false;

	if (!LoadFromPath(info.dli_fname))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes a module descriptors
//-----------------------------------------------------------------------------
template<typename Mutex>
bool CAssemblyModule<Mutex>::LoadFromPath(const std::string_view svModelePath, int flags)
{
	void* handle = dlopen(svModelePath.data(), flags);
	if (!handle) {
		SaveLastError();
		return false;
	}

	SetPtr(handle);
	m_sPath = std::move(svModelePath);

	if (m_vecSections.size())
		return true;

	const auto* header = RCast<const MachHeader*>();

/*
	if (header->magic != MACH_MAGIC) {
		_error = "Not a valid Mach-O file.";
		return false;
	}

	if (header->cputype != MACH_CPU_TYPE || header->cpusubtype != MACH_CPU_SUBTYPE) {
		_error = "Not a valid Mach-O file architecture.";
		return false;
	}

	if (header->filetype != MH_DYLIB) {
		_error = "Mach-O file must be a dynamic library.";
		return false;
	}
*/

	const load_command* cmd = reinterpret_cast<const load_command*>(reinterpret_cast<uintptr_t>(header) + sizeof(MachHeader));
	for (uint32_t i = 0; i < header->ncmds; ++i) {
		if (cmd->cmd == MACH_LOADCMD_SEGMENT) {
			const MachSegment* seg = reinterpret_cast<const MachSegment*>(cmd);
			const MachSection* sec = reinterpret_cast<const MachSection*>(reinterpret_cast<uintptr_t>(seg) + sizeof(MachSegment));

			for (uint32_t j = 0; j < seg->nsects; ++j) {
				const MachSection& section = sec[j];
				m_vecSections.emplace_back(
					GetAddr() + section.addr,
					section.size,
					section.sectname
				);
			}
		}
		cmd = reinterpret_cast<const load_command*>(reinterpret_cast<uintptr_t>(cmd) + cmd->cmdsize);
	}

	m_pExecutableSection = GetSectionByName("__TEXT");
	assert(m_pExecutableSection != nullptr);

	return true;
}

template<typename Mutex>
bool CAssemblyModule<Mutex>::LoadFromPath(const std::string_view svModelePath)
{
	return LoadFromPath(svModelePath, RTLD_LAZY);
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svTableName
//          bDecorated
// Output : CMemory
//-----------------------------------------------------------------------------
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetVirtualTable(const std::string_view svTableName, bool bDecorated) const
{
	if (svTableName.empty())
		return DYNLIB_INVALID_MEMORY;

	// TODO: Implement

	return DYNLIB_INVALID_MEMORY;
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svFunctionName
// Output : CMemory
//-----------------------------------------------------------------------------
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetFunction(const std::string_view svFunctionName) const noexcept
{
	return CMemory((IsValid() && !svFunctionName.empty()) ? dlsym(GetPtr(), svFunctionName.data()) : nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module base
//-----------------------------------------------------------------------------
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetBase() const noexcept
{
	return CMemory(RCast<dlopen_handle*>()->module);
}

template<typename Mutex>
void CAssemblyModule<Mutex>::SaveLastError()
{
	m_sLastError = dlerror();
}