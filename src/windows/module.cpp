//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>
#include <dynlibutils/defer.hpp>

#include <cstring>
#include <cmath>

#if DYNLIBUTILS_ARCH_BITS == 64
const WORD PE_FILE_MACHINE = IMAGE_FILE_MACHINE_AMD64;
const WORD PE_NT_OPTIONAL_HDR_MAGIC = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
#else
const WORD PE_FILE_MACHINE = IMAGE_FILE_MACHINE_I386;
const WORD PE_NT_OPTIONAL_HDR_MAGIC = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
#endif // DYNLIBUTILS_ARCH_BITS

using namespace DynLibUtils;

static std::string GetErrorMessage() {
	DWORD dwErrorCode = ::GetLastError();
	if (dwErrorCode == 0) {
		return {}; // No error message has been recorded
	}

	LPSTR messageBuffer = NULL;
	const DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM  | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
									  NULL, // (not used with FORMAT_MESSAGE_FROM_SYSTEM)
									  dwErrorCode,
									  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
									  reinterpret_cast<LPSTR>(&messageBuffer),
									  0,
									  NULL);
	if (!size) {
		return "Unknown error code: " + std::to_string(dwErrorCode);
	}

	std::string buffer(messageBuffer, size);
	LocalFree(messageBuffer);
	return buffer;
}

CModule::~CModule() {

	if (m_handle)
	{
		FreeLibrary(static_cast<HMODULE>(m_handle));
		m_handle = nullptr;
	}
}

static std::wstring GetModulePath(HMODULE hModule)
{
	std::wstring modulePath(MAX_PATH, L'\0');
	while (true)
	{
		size_t len = GetModuleFileNameW(hModule, modulePath.data(), static_cast<DWORD>(modulePath.length()));
		if (len == 0)
		{
			modulePath.clear();
			break;
		}

		if (len < modulePath.length())
		{
			modulePath.resize(len);
			break;
		}
		else
		{
			modulePath.resize(modulePath.length() * 2);
		}
	}

	return modulePath;
}

bool CModule::InitFromName(std::string_view moduleName, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections, bool extension)
{
	if (m_handle)
		return false;

	if (moduleName.empty())
		return false;

	std::filesystem::path name(moduleName);
	if (!extension && !name.has_extension())
		name += ".dll";

	HMODULE handle = GetModuleHandleW(name.c_str());
	if (!handle)
		return false;

	std::filesystem::path modulePath = ::GetModulePath(handle);
	if (modulePath.empty())
		return false;

	if (!Init(modulePath, flags, additionalSearchDirectories, sections))
		return false;

	return true;
}

bool CModule::InitFromMemory(CMemory moduleMemory, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
{
	if (m_handle)
		return false;

	if (!moduleMemory)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(moduleMemory, &mbi, sizeof(mbi)))
		return false;

	std::wstring modulePath = ::GetModulePath(reinterpret_cast<HMODULE>(mbi.AllocationBase));
	if (modulePath.empty())
		return false;

	if (!Init(modulePath, flags, additionalSearchDirectories, sections))
		return false;

	return true;
}

bool CModule::InitFromHandle(Handle moduleHandle, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
{
	if (m_handle)
		return false;

	if (!moduleHandle)
		return false;

	std::wstring modulePath = ::GetModulePath(reinterpret_cast<HMODULE>(static_cast<void*>(moduleHandle)));
	if (modulePath.empty())
		return false;

	if (!Init(modulePath, flags, additionalSearchDirectories, sections))
		return false;

	return true;
}

bool CModule::Init(std::filesystem::path modulePath, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
{
	std::vector<DLL_DIRECTORY_COOKIE> dirCookies;
	dirCookies.reserve(additionalSearchDirectories.size());
	for (const auto& directory : additionalSearchDirectories)
	{
		DLL_DIRECTORY_COOKIE cookie = AddDllDirectory(directory.c_str());
		if (cookie == nullptr)
			continue;
		dirCookies.push_back(cookie);
	}

	Defer _ = [&]()
	{
		for (auto& cookie : dirCookies)
			RemoveDllDirectory(cookie);
	};

	HMODULE hModule = LoadLibraryExW(modulePath.c_str(), nullptr, TranslateLoading(flags));
	if (!hModule)
	{
		m_error = GetErrorMessage();
		return false;
	}

	m_handle = hModule;
	m_path = std::move(modulePath);

	if (flags & LoadFlag::PinInMemory) {
		HMODULE hPinHandle = NULL;
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, reinterpret_cast<LPCWSTR>(hModule), &hPinHandle);
	}

	if (sections)
	{
		LoadSections();
	}

	return true;
}

bool CModule::LoadSections()
{
	IMAGE_DOS_HEADER* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_handle);
	IMAGE_NT_HEADERS* pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uintptr_t>(m_handle) + pDOSHeader->e_lfanew);
	/*
		IMAGE_FILE_HEADER* pFileHeader = &pNTHeaders->OptionalHeader;
		IMAGE_OPTIONAL_HEADER* pOptionalHeader = &pNTHeaders->OptionalHeader;;

		if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE || pNTHeaders->Signature != IMAGE_NT_SIGNATURE || pOptionalHeader->Magic != PE_NT_OPTIONAL_HDR_MAGIC)
		{
			m_error = "Not a valid DLL file.";
			return false;
		}

		if (pFileHeader->Machine != PE_FILE_MACHINE)
		{
			m_error = "Not a valid DLL file architecture.";
			return false;
		}

		if ((pFileHeader->Characteristics & IMAGE_FILE_DLL) == 0)
		{
			m_error = "DLL file must be a dynamic library.";
			return false;
		}
	*/
	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(pNTHeaders);// Get first image section.

	// Loop through the sections
	for (WORD i = 0; i < pNTHeaders->FileHeader.NumberOfSections; ++i) {
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_sections.emplace_back(
			reinterpret_cast<const char*>(hCurrentSection.Name),
			reinterpret_cast<uintptr_t>(m_handle) + hCurrentSection.VirtualAddress,
			hCurrentSection.SizeOfRawData);// Push back a struct with the section data.
	}

	m_executableCode = GetSectionByName(".text");

	return true;
}

CMemory CModule::GetVirtualTableByName(std::string_view tableName, bool decorated) const
{
	if (tableName.empty())
		return nullptr;

	CModule::Section runTimeData = GetSectionByName(".data"), readOnlyData = GetSectionByName(".rdata");
	if (!runTimeData || !readOnlyData)
		return nullptr;

	std::string decoratedTableName(decorated ? tableName : ".?AV" + std::string(tableName) + "@@");
	std::string mask(decoratedTableName.length() + 1, 'x');

	CMemory typeDescriptorName = FindPattern(decoratedTableName.data(), mask, nullptr, &runTimeData);
	if (!typeDescriptorName)
		return nullptr;

	CMemory rttiTypeDescriptor = typeDescriptorName.Offset(-0x10);
	const uintptr_t rttiTDRva = rttiTypeDescriptor - GetBase();// The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.

	CMemory reference; // Get reference typeinfo in vtable
	while ((reference = FindPattern(&rttiTDRva, "xxxx", reference, &readOnlyData)))
	{
		// Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		// Check that offset of this vtable is 0
		if (reference.Offset(-0xC).Get<int32_t>() == 1 && reference.Offset(-0x8).Get<int32_t>() == 0) {
			CMemory referenceOffset = reference.Offset(-0xC);
			CMemory rttiCompleteObjectLocator = FindPattern(&referenceOffset, "xxxxxxxx", nullptr, &readOnlyData);
			if (rttiCompleteObjectLocator)
				return rttiCompleteObjectLocator.Offset(0x8);
		}

		reference.OffsetSelf(0x4);
	}

	return nullptr;
}

CMemory CModule::GetFunctionByName(std::string_view functionName) const noexcept
{
	if (!m_handle)
		return nullptr;

	if (functionName.empty())
		return nullptr;

	return GetProcAddress(static_cast<HMODULE>(m_handle), functionName.data());
}

CMemory CModule::GetBase() const noexcept
{
	return m_handle;
}

namespace DynLibUtils
{
	int TranslateLoading(LoadFlag flags) noexcept
	{
		int winFlags = 0;
		if (flags & LoadFlag::DontResolveDllReferences) winFlags |= DONT_RESOLVE_DLL_REFERENCES;
		if (flags & LoadFlag::AlteredSearchPath) winFlags |= LOAD_WITH_ALTERED_SEARCH_PATH;
		if (flags & LoadFlag::AsDatafile) winFlags |= LOAD_LIBRARY_AS_DATAFILE;
		if (flags & LoadFlag::AsDatafileExclusive) winFlags |= LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE;
		if (flags & LoadFlag::AsImageResource) winFlags |= LOAD_LIBRARY_AS_IMAGE_RESOURCE;
		if (flags & LoadFlag::SearchApplicationDir) winFlags |= LOAD_LIBRARY_SEARCH_APPLICATION_DIR;
		if (flags & LoadFlag::SearchDefaultDirs) winFlags |= LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
		if (flags & LoadFlag::SearchDllLoadDir) winFlags |= LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR;
		if (flags & LoadFlag::SearchSystem32) winFlags |= LOAD_LIBRARY_SEARCH_SYSTEM32;
		if (flags & LoadFlag::SearchUserDirs) winFlags |= LOAD_LIBRARY_SEARCH_USER_DIRS;
		if (flags & LoadFlag::RequireSignedTarget) winFlags |= LOAD_LIBRARY_REQUIRE_SIGNED_TARGET;
		if (flags & LoadFlag::IgnoreAuthzLevel) winFlags |= LOAD_IGNORE_CODE_AUTHZ_LEVEL;
	#ifdef LOAD_LIBRARY_SAFE_CURRENT_DIRS
		if (flags & LoadFlag::SafeCurrentDirs) winFlags |= LOAD_LIBRARY_SAFE_CURRENT_DIRS;
	#endif // LOAD_LIBRARY_SAFE_CURRENT_DIRS
		return winFlags;
	}

	LoadFlag TranslateLoading(int flags) noexcept
	{
		LoadFlag loadFlags = LoadFlag::Default;
		if (flags & DONT_RESOLVE_DLL_REFERENCES) loadFlags = loadFlags | LoadFlag::DontResolveDllReferences;
		if (flags & LOAD_WITH_ALTERED_SEARCH_PATH) loadFlags = loadFlags | LoadFlag::AlteredSearchPath;
		if (flags & LOAD_LIBRARY_AS_DATAFILE) loadFlags = loadFlags | LoadFlag::AsDatafile;
		if (flags & LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE) loadFlags = loadFlags | LoadFlag::AsDatafileExclusive;
		if (flags & LOAD_LIBRARY_AS_IMAGE_RESOURCE) loadFlags = loadFlags | LoadFlag::AsImageResource;
		if (flags & LOAD_LIBRARY_SEARCH_APPLICATION_DIR) loadFlags = loadFlags | LoadFlag::SearchApplicationDir;
		if (flags & LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) loadFlags = loadFlags | LoadFlag::SearchDefaultDirs;
		if (flags & LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR) loadFlags = loadFlags | LoadFlag::SearchDllLoadDir;
		if (flags & LOAD_LIBRARY_SEARCH_SYSTEM32) loadFlags = loadFlags | LoadFlag::SearchSystem32;
		if (flags & LOAD_LIBRARY_SEARCH_USER_DIRS) loadFlags = loadFlags | LoadFlag::SearchUserDirs;
		if (flags & LOAD_LIBRARY_REQUIRE_SIGNED_TARGET) loadFlags = loadFlags | LoadFlag::RequireSignedTarget;
		if (flags & LOAD_IGNORE_CODE_AUTHZ_LEVEL) loadFlags = loadFlags | LoadFlag::IgnoreAuthzLevel;
	#ifdef LOAD_LIBRARY_SAFE_CURRENT_DIRS
		if (flags & LOAD_LIBRARY_SAFE_CURRENT_DIRS) loadFlags = loadFlags | LoadFlag::SafeCurrentDirs;
	#endif // LOAD_LIBRARY_SAFE_CURRENT_DIRS
		return loadFlags;
	}
}