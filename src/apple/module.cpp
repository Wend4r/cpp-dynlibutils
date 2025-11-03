//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#if DYNLIBUTILS_ARCH_BITS == 64
typedef struct mach_header_64 MachHeader;
typedef struct segment_command_64 MachSegment;
typedef struct section_64 MachSection;
const uint32_t MACH_MAGIC = MH_MAGIC_64;
const uint32_t MACH_LOADCMD_SEGMENT = LC_SEGMENT_64;
const cpu_type_t MACH_CPU_TYPE = CPU_TYPE_X86_64;
const cpu_subtype_t MACH_CPU_SUBTYPE = CPU_SUBTYPE_X86_64_ALL;
#else
typedef struct mach_header MachHeader;
typedef struct segment_command MachSegment;
typedef struct section MachSection;
const uint32_t MACH_MAGIC = MH_MAGIC;
const uint32_t MACH_LOADCMD_SEGMENT = LC_SEGMENT;
const cpu_type_t MACH_CPU_TYPE = CPU_TYPE_I386;
const cpu_subtype_t MACH_CPU_SUBTYPE = CPU_SUBTYPE_I386_ALL;
#endif // DYNLIBUTILS_ARCH_BITS

typedef void* NSModule;

struct dlopen_handle
{
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
	if (m_handle)
	{
		dlclose(m_handle);
		m_handle = nullptr;
	}
}

bool CModule::InitFromName(std::string_view /*moduleName*/, LoadFlag /*flags*/, const SearchDirs& /*additionalSearchDirectories*/, bool /*extension*/, bool /*sections*/)
{
	// TODO: Implement
	return false;
}

bool CModule::InitFromMemory(CMemory moduleMemory, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
{
	if (m_handle)
		return false;

	if (!moduleMemory)
		return false;

	Dl_info info;
	if (!dladdr(moduleMemory, &info) || !info.dli_fbase || !info.dli_fname)
		return false;

	if (!Init(info.dli_fname, flags, additionalSearchDirectories, sections))
		return false;

	return true;
}

bool CModule::InitFromHandle(Handle /*moduleHandle*/, LoadFlag /*flags*/, const SearchDirs& /*additionalSearchDirectories*/, bool /*sections*/)
{
	// TODO: Implement
	return false;
}

bool CModule::Init(std::filesystem::path modulePath, LoadFlag flags, const SearchDirs& /*additionalSearchDirectories*/, bool sections)
{
	// Cannot set LYLD_LIBRARY_PATH at runtime, so use rpath flag

	void* handle = dlopen(modulePath.c_str(), TranslateLoading(flags));
	if (!handle)
	{
		m_error = dlerror();
		return false;
	}

	m_handle = handle;
	m_path = std::move(modulePath);

	if (sections)
	{
		return LoadSections();
	}

	return true;
}

bool CModule::LoadSections()
{
	const MachHeader* header = reinterpret_cast<const MachHeader*>(m_handle);
	/*
	if (header->magic != MACH_MAGIC)
	{
		m_error = "Not a valid Mach-O file.";
		return false;
	}

	if (header->cputype != MACH_CPU_TYPE || header->cpusubtype != MACH_CPU_SUBTYPE)
	{
		m_error = "Not a valid Mach-O file architecture.";
		return false;
	}

	if (header->filetype != MH_DYLIB)
	{
		m_error = "Mach-O file must be a dynamic library.";
		return false;
	}
	*/
	const load_command* cmd = reinterpret_cast<const load_command*>(reinterpret_cast<uintptr_t>(header) + sizeof(MachHeader));
	for (uint32_t i = 0; i < header->ncmds; ++i)
	{
		if (cmd->cmd == MACH_LOADCMD_SEGMENT)
		{
			const MachSegment* seg = reinterpret_cast<const MachSegment*>(cmd);
			const MachSection* sec = reinterpret_cast<const MachSection*>(reinterpret_cast<uintptr_t>(seg) + sizeof(MachSegment));

			for (uint32_t j = 0; j < seg->nsects; ++j)
			{
				const MachSection& section = sec[j];
				m_sections.emplace_back(
					section.sectname,
					reinterpret_cast<uintptr_t>(m_handle) + section.addr,
					section.size
				);
			}
		}
		cmd = reinterpret_cast<const load_command*>(reinterpret_cast<uintptr_t>(cmd) + cmd->cmdsize);
	}

	m_executableCode = GetSectionByName("__TEXT");
	return true;
}

CMemory CModule::GetVirtualTableByName(std::string_view tableName, bool /*decorated*/) const
{
	if (tableName.empty())
		return nullptr;

	// TODO: Implement

	return nullptr;
}

CMemory CModule::GetFunctionByName(std::string_view functionName) const noexcept
{
	if (!m_handle)
		return nullptr;

	if (functionName.empty())
		return nullptr;

	return dlsym(m_handle, functionName.data());
}

CMemory CModule::GetBase() const noexcept
{
	return reinterpret_cast<dlopen_handle*>(m_handle)->module;
}

namespace DynLibUtils
{
	int TranslateLoading(LoadFlag flags) noexcept
	{
		int unixFlags = 0;
		if (flags & LoadFlag::Lazy) unixFlags |= RTLD_LAZY;
		if (flags & LoadFlag::Now) unixFlags |= RTLD_NOW;
		if (flags & LoadFlag::Global) unixFlags |= RTLD_GLOBAL;
		if (flags & LoadFlag::Local) unixFlags |= RTLD_LOCAL;
		if (flags & LoadFlag::Nodelete) unixFlags |= RTLD_NODELETE;
		if (flags & LoadFlag::Noload) unixFlags |= RTLD_NOLOAD;
	#ifdef RTLD_DEEPBIND
		if (flags & LoadFlag::Deepbind) unixFlags |= RTLD_DEEPBIND;
	#endif // RTLD_DEEPBIND
		return unixFlags;
	}

	LoadFlag TranslateLoading(int flags) noexcept
	{
		LoadFlag loadFlags = LoadFlag::Default;
		if (flags & RTLD_LAZY) loadFlags = loadFlags | LoadFlag::Lazy;
		if (flags & RTLD_NOW) loadFlags = loadFlags | LoadFlag::Now;
		if (flags & RTLD_GLOBAL) loadFlags = loadFlags | LoadFlag::Global;
		if (flags & RTLD_LOCAL) loadFlags = loadFlags | LoadFlag::Local;
		if (flags & RTLD_NODELETE) loadFlags = loadFlags | LoadFlag::Nodelete;
		if (flags & RTLD_NOLOAD) loadFlags = loadFlags | LoadFlag::Noload;
	#ifdef RTLD_DEEPBIND
		if (flags & RTLD_DEEPBIND) loadFlags = loadFlags | LoadFlag::Deepbind;
	#endif // RTLD_DEEPBIND
		return loadFlags;
	}
}