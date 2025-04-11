// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <cstring>
#include <cmath>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN

namespace DynLibUtils {

CModule::~CModule()
{
	if (m_pHandle)
		FreeLibrary(reinterpret_cast<HMODULE>(m_pHandle));
}

static std::string GetModulePath(HMODULE hModule)
{
	std::string modulePath(MAX_PATH, '\0');
	while (true)
	{
		size_t len = GetModuleFileNameA(hModule, modulePath.data(), static_cast<DWORD>(modulePath.length()));
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
			modulePath.resize(modulePath.length() * 2);
	}

	return modulePath;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module name
// Input  : svModuleName
//          bExtension
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromName(const std::string_view svModuleName, bool bExtension)
{
	assert(!svModuleName.empty());

	if (m_pHandle)
		return false;

	std::string sModuleName(svModuleName);
	if (!bExtension)
		sModuleName.append(".dll");

	HMODULE handle = GetModuleHandleA(sModuleName.c_str());
	if (!handle)
		return false;

	std::string modulePath = GetModulePath(handle);
	if(modulePath.empty())
		return false;

	if (!LoadFromPath(modulePath, DONT_RESOLVE_DLL_REFERENCES))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the module from module memory
// Input  : pModuleMemory
// Output : bool
//-----------------------------------------------------------------------------
bool CModule::InitFromMemory(const CMemory pModuleMemory, bool bForce)
{
	assert(pModuleMemory.IsValid());

	if (!bForce && m_pHandle)
		return false;

	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(pModuleMemory, &mbi, sizeof(mbi)))
		return false;

	std::string modulePath = GetModulePath(reinterpret_cast<HMODULE>(mbi.AllocationBase));
	if (modulePath.empty())
		return false;

	if (!LoadFromPath(modulePath, DONT_RESOLVE_DLL_REFERENCES))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Initializes a module descriptors
//-----------------------------------------------------------------------------
bool CModule::LoadFromPath(const std::string_view svModelePath, int flags)
{
	HMODULE handle = LoadLibraryExA(svModelePath.data(), nullptr, flags);
	if (!handle)
	{
		SaveLastError();
		return false;
	}

	IMAGE_DOS_HEADER* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(handle);
	IMAGE_NT_HEADERS64* pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<uintptr_t>(handle) + pDOSHeader->e_lfanew);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(pNTHeaders); // Get first image section.

	for (WORD i = 0; i < pNTHeaders->FileHeader.NumberOfSections; ++i) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vecSections.emplace_back(hCurrentSection.SizeOfRawData, reinterpret_cast<const char*>(hCurrentSection.Name), static_cast<uintptr_t>(reinterpret_cast<uintptr_t>(handle) + hCurrentSection.VirtualAddress)); // Push back a struct with the section data.
	}

	m_pHandle = handle;
	m_sPath.assign(svModelePath);

	m_pExecutableSection = GetSectionByName(".text");
	assert(m_pExecutableSection != nullptr);

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
	assert(!svTableName.empty());
	
	const Section_t *pRunTimeData = GetSectionByName(".data"), *pReadOnlyData = GetSectionByName(".rdata");

	assert(pRunTimeData != nullptr);
	assert(pReadOnlyData != nullptr);

	std::string sDecoratedTableName(bDecorated ? svTableName : ".?AV" + std::string(svTableName) + "@@");
	std::string sMask(sDecoratedTableName.length() + 1, 'x');

	CMemory typeDescriptorName = FindPattern(sDecoratedTableName.data(), sMask, nullptr, pRunTimeData);
	if (!typeDescriptorName)
		return DYNLIB_INVALID_MEMORY;

	CMemory rttiTypeDescriptor = typeDescriptorName.Offset(-0x10);
	const uintptr_t rttiTDRva = rttiTypeDescriptor - GetBase(); // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.

	CMemory reference;
	while ((reference = FindPattern(&rttiTDRva, "xxxx", reference, pReadOnlyData))) // Get reference typeinfo in vtable
	{
		// Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		// Check that offset of this vtable is 0
		if (reference.Offset(-0xC).GetValue<int32_t>() == 1 && reference.Offset(-0x8).GetValue<int32_t>() == 0)
		{
			CMemory referenceOffset = reference.Offset(-0xC);
			CMemory rttiCompleteObjectLocator = FindPattern(&referenceOffset, "xxxxxxxx", nullptr, pReadOnlyData);
			if (rttiCompleteObjectLocator)
				return rttiCompleteObjectLocator.Offset(0x8);
		}

		reference.OffsetSelf(0x4);
	}

	return DYNLIB_INVALID_MEMORY;
}

//-----------------------------------------------------------------------------
// Purpose: Gets an address of a virtual method table by rtti type descriptor name
// Input  : svFunctionName
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetFunctionByName(const std::string_view svFunctionName) const noexcept
{
	assert(!svFunctionName.empty());

	if(!m_pHandle)
		return DYNLIB_INVALID_MEMORY;

	return GetProcAddress(reinterpret_cast<HMODULE>(m_pHandle), svFunctionName.data());
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module base
//-----------------------------------------------------------------------------
CMemory CModule::GetBase() const noexcept
{
	return m_pHandle;
}

void CModule::SaveLastError()
{
	auto errorCode = ::GetLastError();
	if (errorCode == 0) {
		return;
	}

	LPSTR messageBuffer = nullptr;

	size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr,
			errorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			reinterpret_cast<LPSTR>(&messageBuffer),
			0,
			nullptr
	);

	m_sLastError.assign(messageBuffer, size);

	LocalFree(messageBuffer);
}

}; // namespace DynLibUtils
