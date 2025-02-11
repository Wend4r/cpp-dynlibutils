// DynLibUtils
// Copyright (C) 2023-2024 komashchenko (Phoenix) & Vladimir Ezhikov (Wend4r)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/nlist.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

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

CModule::~CModule()
{
	if (m_pHandle)
		dlclose(m_pHandle);
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module name
// Input  : svModuleName
//          bExtension
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromName(const std::string_view svModuleName, bool bExtension)
{
	if (m_pHandle)
		return false;

	if (svModuleName.empty())
		return false;

	std::string sModuleName(svModuleName);

	if (!bExtension)
		sModuleName.append(".dylib");

	m_pHandle = dlopen(sModuleName.c_str(), RTLD_LAZY);

	return (m_pHandle != nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module memory
// Input  : pModuleMemory
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromMemory(const CMemory pModuleMemory)
{
	if (m_pHandle)
		return false;

	if (!pModuleMemory)
		return false;

	Dl_info info;
	if (!dladdr(pModuleMemory, &info) || !info.dli_fbase || !info.dli_fname)
		return false;

	if (!LoadFromPath(info.dli_fname, RTLD_LAZY))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes a module descriptors
//-----------------------------------------------------------------------------
bool CModule::LoadFromPath(const std::string_view svModelePath, int flags)
{
	void* handle = dlopen(svModelePath.data(), flags);

	if (!handle) {
		return false;
	}

	m_pHandle = handle;
	m_sPath = std::move(svModelePath);

	if (m_vModuleSections.size())
		return true;

	const MachHeader* header = reinterpret_cast<const MachHeader*>(m_pHandle);

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
				m_vModuleSections.emplace_back(
					section.sectname,
					reinterpret_cast<uintptr_t>(m_pHandle) + section.addr,
					section.size
				);
			}
		}
		cmd = reinterpret_cast<const load_command*>(reinterpret_cast<uintptr_t>(cmd) + cmd->cmdsize);
	}

	m_ExecutableCode = GetSectionByName("__TEXT");

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svTableName
//          bDecorated
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetVirtualTableByName(const std::string_view svTableName, bool bDecorated) const
{
	if (svTableName.empty())
		return CMemory();

	// TODO: Implement

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svFunctionName
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetFunctionByName(const std::string_view svFunctionName) const noexcept
{
	if (!m_pHandle)
		return CMemory();

	if (svFunctionName.empty())
		return CMemory();

	return dlsym(m_pHandle, svFunctionName.data());
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module base
//-----------------------------------------------------------------------------
CMemory CModule::GetBase() const noexcept
{
	return static_cast<dlopen_handle*>(m_pHandle)->module;
}
