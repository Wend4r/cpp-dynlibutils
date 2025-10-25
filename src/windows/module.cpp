//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include "os.h"

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <cstring>
#include <cmath>

namespace DynLibUtils {

template<typename Mutex>
CAssemblyModule<Mutex>::~CAssemblyModule()
{
	if (IsValid())
		FreeLibrary(RCast<HMODULE>());
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
template<typename Mutex>
bool CAssemblyModule<Mutex>::InitFromName(const std::string_view svModuleName, bool bExtension)
{
	if (IsValid())
		return false;

	if (svModuleName.empty())
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
template<typename Mutex>
bool CAssemblyModule<Mutex>::InitFromMemory(const CMemory pModuleMemory, bool bForce)
{
	if (IsValid() && !bForce)
		return false;

	if (!pModuleMemory.IsValid())
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
template<typename Mutex>
bool CAssemblyModule<Mutex>::LoadFromPath(const std::string_view svModelePath, int flags)
{
	HMODULE handle = LoadLibraryExA(svModelePath.data(), nullptr, flags);
	if (!handle)
	{
		SaveLastError();
		return false;
	}

	IMAGE_DOS_HEADER* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(handle);
	IMAGE_NT_HEADERS64* pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(reinterpret_cast<std::uintptr_t>(handle) + pDOSHeader->e_lfanew);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(pNTHeaders); // Get first image section.

	for (WORD i = 0; i < pNTHeaders->FileHeader.NumberOfSections; ++i) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vecSections.emplace_back(static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(handle) + hCurrentSection.VirtualAddress), hCurrentSection.SizeOfRawData, reinterpret_cast<const char*>(hCurrentSection.Name)); // Push back a struct with the section data.
	}

	SetPtr(static_cast<void *>(handle));
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
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetVirtualTable(const std::string_view svTableName, bool bDecorated) const
{
	if (svTableName.empty())
		return DYNLIB_INVALID_MEMORY;

	const Section_t *pRunTimeData = GetSectionByName(".data"), *pReadOnlyData = GetSectionByName(".rdata");

	assert(pRunTimeData != nullptr);
	assert(pReadOnlyData != nullptr);

	std::string sDecoratedTableName(bDecorated ? svTableName : ".?AV" + std::string(svTableName) + "@@");
	std::string sMask(sDecoratedTableName.length() + 1, 'x');

	CMemory typeDescriptorName = FindPattern(sDecoratedTableName.data(), sMask, nullptr, pRunTimeData);
	if (!typeDescriptorName)
		return DYNLIB_INVALID_MEMORY;

	CMemory rttiTypeDescriptor = typeDescriptorName.Offset(-0x10);
	std::uintptr_t rttiTDRva = rttiTypeDescriptor.GetAddr() - GetBase().GetAddr(); // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.

	CMemory reference;
	while ((reference = FindPattern(&rttiTDRva, "xxxx", reference, pReadOnlyData))) // Get reference typeinfo in vtable
	{
		// Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		// Check that offset of this vtable is 0
		if (reference.Offset(-0xC).Get<int32_t>() == 1 && reference.Offset(-0x8).Get<int32_t>() == 0)
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
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetFunction(const std::string_view svFunctionName) const noexcept
{
	return CMemory((IsValid() && !svFunctionName.empty()) ? GetProcAddress(static_cast<HMODULE>(GetPtr()), svFunctionName.data()) : nullptr);
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module base
//-----------------------------------------------------------------------------
template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetBase() const noexcept
{
	return *this;
}

template<typename Mutex>
void CAssemblyModule<Mutex>::SaveLastError()
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
