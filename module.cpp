#include <cmath>
#include <emmintrin.h>
#include "module.h"
#if defined _WIN32 && _M_X64
#include <windows.h>
#elif defined __linux__ && __x86_64__
#include <cstring>
#include <link.h>
#include <unistd.h>
#else
#error "Unsupported platform"
#endif

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : szModuleName (without extension .dll/.so)
//-----------------------------------------------------------------------------
CModule::CModule(const std::string_view szModuleName)
{
	InitFromName(szModuleName);
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
CModule::CModule(const CMemory pModuleMemory)
{
	InitFromMemory(pModuleMemory);
}

//-----------------------------------------------------------------------------
// Purpose: initializes class from module name (without extension .dll/.so)
// Input  : szModuleName
//-----------------------------------------------------------------------------
void CModule::InitFromName(const std::string_view szModuleName)
{
	std::string szFullModuleName(szModuleName);

#if defined _WIN32 && _M_X64
	szFullModuleName.append(".dll");

	m_pModuleBase = reinterpret_cast<uintptr_t>(GetModuleHandleA(szFullModuleName.c_str()));
	if(!m_pModuleBase)
		return;
#else
	szFullModuleName.append(".so");

	struct dl_data
	{
		ElfW(Addr) addr;
		const char* moduleName;
	} dldata{0, szFullModuleName.c_str()};

	dl_iterate_phdr([](dl_phdr_info* info, size_t /* size */, void* data)
	{
		dl_data* dldata = reinterpret_cast<dl_data*>(data);

		if (std::strstr(info->dlpi_name, dldata->moduleName) != nullptr)
		{
			dldata->addr = info->dlpi_addr;
		}

		return 0;
	}, &dldata);

	if(!dldata.addr)
		return;

	m_pModuleBase = reinterpret_cast<uintptr_t>(dldata.addr);
#endif

	m_svModuleName.assign(std::move(szFullModuleName));

	Init();
	LoadSections();
}

//-----------------------------------------------------------------------------
// Purpose: initializes class from module memory
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
void CModule::InitFromMemory(const CMemory pModuleMemory)
{
#if defined _WIN32 && _M_X64
	MEMORY_BASIC_INFORMATION mbi;
	if (!VirtualQuery(pModuleMemory, &mbi, sizeof(mbi)) || mbi.AllocationBase == nullptr)
		return;

	m_pModuleBase = reinterpret_cast<uintptr_t>(mbi.AllocationBase);

	char szPath[MAX_PATH];
	size_t nLen = GetModuleFileNameA(reinterpret_cast<HMODULE>(mbi.AllocationBase), szPath, sizeof(szPath));
	m_svModuleName.assign(szPath, nLen);
#else
	Dl_info info;
	if (!dladdr(pModuleMemory, &info) || !info.dli_fbase || !info.dli_fname)
		return;

	m_pModuleBase = reinterpret_cast<uintptr_t>(info.dli_fbase);
	m_svModuleName.assign(info.dli_fname);
#endif

	m_svModuleName.assign(m_svModuleName.substr(m_svModuleName.find_last_of("/\\") + 1)); // Leave only file name.

	Init();
	LoadSections();
}

//-----------------------------------------------------------------------------
// Purpose: initializes module descriptors
//-----------------------------------------------------------------------------
void CModule::Init()
{
#if defined _WIN32 && _M_X64
	IMAGE_DOS_HEADER* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(m_pModuleBase);
	IMAGE_NT_HEADERS64* pNTHeaders = reinterpret_cast<IMAGE_NT_HEADERS64*>(m_pModuleBase + pDOSHeader->e_lfanew);

	const IMAGE_SECTION_HEADER* hSection = IMAGE_FIRST_SECTION(pNTHeaders); // Get first image section.

	for (WORD i = 0; i < pNTHeaders->FileHeader.NumberOfSections; i++) // Loop through the sections.
	{
		const IMAGE_SECTION_HEADER& hCurrentSection = hSection[i]; // Get current section.
		m_vModuleSections.emplace_back(reinterpret_cast<const char*>(hCurrentSection.Name), static_cast<uintptr_t>(m_pModuleBase + hCurrentSection.VirtualAddress), hCurrentSection.SizeOfRawData); // Push back a struct with the section data.
	}
#else
	Elf64_Ehdr* pEhdr = reinterpret_cast<Elf64_Ehdr*>(m_pModuleBase);
	Elf64_Phdr* pPhdr = reinterpret_cast<Elf64_Phdr*>(m_pModuleBase + pEhdr->e_phoff);

	for (Elf64_Half i = 0; i < pEhdr->e_phnum; i++) // Loop through the sections.
	{
		Elf64_Phdr& phdr = pPhdr[i];

		if (phdr.p_type == PT_LOAD && phdr.p_flags == (PF_X | PF_R))
		{
			/* From glibc, elf/dl-load.c:
			 * c->mapend = ((ph->p_vaddr + ph->p_filesz + GLRO(dl_pagesize) - 1)
			 * & ~(GLRO(dl_pagesize) - 1));
			 *
			 * In glibc, the segment file size is aligned up to the nearest page size and
			 * added to the virtual address of the segment. We just want the size here.
			 */
			size_t pagesize = sysconf(_SC_PAGESIZE);
			size_t nSectionSize = (phdr.p_filesz + pagesize - 1) & ~(pagesize - 1);

			// We can't get the section names, but most likely it will be exactly .text.
			m_vModuleSections.emplace_back(".text", m_pModuleBase + phdr.p_vaddr, nSectionSize);

			break;
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: initializes the default executable segments
//-----------------------------------------------------------------------------
void CModule::LoadSections()
{
	m_ExecutableCode = GetSectionByName(".text");
}

//-----------------------------------------------------------------------------
// Purpose: converting a string pattern with wildcards to an array of bytes and mask
// Input  : svInput
// Output : std::pair<std::vector<uint8_t>, std::string>
//-----------------------------------------------------------------------------
std::pair<std::vector<uint8_t>, std::string> CModule::PatternToMaskedBytes(const std::string_view svInput)
{
	char* pszPatternStart = const_cast<char*>(svInput.data());
	char* pszPatternEnd = pszPatternStart + svInput.size();
	std::vector<uint8_t> vBytes;
	std::string svMask;

	for (char* pszCurrentByte = pszPatternStart; pszCurrentByte < pszPatternEnd; ++pszCurrentByte)
	{
		if (*pszCurrentByte == '?')
		{
			++pszCurrentByte;
			if (*pszCurrentByte == '?')
			{
				++pszCurrentByte; // Skip double wildcard.
			}

			vBytes.push_back(0); // Push the byte back as invalid.
			svMask += '?';
		}
		else
		{
			vBytes.push_back(static_cast<uint8_t>(strtoul(pszCurrentByte, &pszCurrentByte, 16)));
			svMask += 'x';
		}
	}

	return std::make_pair(std::move(vBytes), std::move(svMask));
}

//-----------------------------------------------------------------------------
// Purpose: find array of bytes in process memory using SIMD instructions
// Input  : *pPattern - first and last byte must be known
//          szMask
//          startAddress
//          *moduleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPatternSIMD(const void* pPattern, const std::string_view szMask, const CMemory startAddress, const ModuleSections_t* moduleSection) const
{
	const uint8_t* pattern = reinterpret_cast<const uint8_t*>(pPattern);
	const ModuleSections_t* section = moduleSection ? moduleSection : &m_ExecutableCode;
	if (!section->IsSectionValid())
		return CMemory();

	const uintptr_t nBase = section->m_pSectionBase;
	const size_t nSize = section->m_nSectionSize;

	const size_t nMaskLen = szMask.length();
	const uint8_t* pData = reinterpret_cast<uint8_t*>(nBase);
	const uint8_t* pEnd = pData + nSize - nMaskLen;

	if(startAddress)
	{
		const uint8_t* pStartAddress = startAddress.RCast<uint8_t*>();
		if(pData > pStartAddress || pStartAddress > pEnd)
			return CMemory();

		pData = pStartAddress;
	}

	int nMasks[64]; // 64*16 = enough masks for 1024 bytes.
	const uint8_t iNumMasks = static_cast<uint8_t>(std::ceil(static_cast<float>(nMaskLen) / 16.f));

	memset(nMasks, 0, iNumMasks * sizeof(int));
	for (uint8_t i = 0; i < iNumMasks; i++)
	{
		for (int8_t j = static_cast<int8_t>(std::min<size_t>(nMaskLen - i * 16, 16)) - 1; j >= 0; --j)
		{
			if (szMask[i * 16 + j] == 'x')
			{
				nMasks[i] |= 1 << j;
			}
		}
	}

	const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pattern));
	__m128i xmm2, xmm3, msks;
	for (; pData != pEnd; _mm_prefetch(reinterpret_cast<const char*>(++pData + 64), _MM_HINT_NTA))
	{
		if (pattern[0] == pData[0] && pattern[nMaskLen - 1] == pData[nMaskLen - 1])
		{
			xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
			msks = _mm_cmpeq_epi8(xmm1, xmm2);
			if ((_mm_movemask_epi8(msks) & nMasks[0]) == nMasks[0])
			{
				bool bFound = true;
				for (uint8_t i = 1; i < iNumMasks; i++)
				{
					xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pData + i * 16)));
					xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pattern + i * 16)));
					msks = _mm_cmpeq_epi8(xmm2, xmm3);
					if ((_mm_movemask_epi8(msks) & nMasks[i]) != nMasks[i])
					{
						bFound = false;
						break;
					}
				}
				if (bFound)
				{
					return static_cast<CMemory>((&*(const_cast<uint8_t*>(pData))));
				}
			}
		}
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: find a string pattern in process memory using SIMD instructions
// Input  : svPattern - first and last byte must be known
//          startAddress
//			*moduleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPatternSIMD(const std::string_view svPattern, const CMemory startAddress, const ModuleSections_t* moduleSection) const
{
	const std::pair patternInfo = PatternToMaskedBytes(svPattern);
	return FindPatternSIMD(patternInfo.first.data(), patternInfo.second, startAddress, moduleSection);
}

//-----------------------------------------------------------------------------
// Purpose: get address of a virtual method table by rtti type descriptor name
// Input  : svTableName
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::GetVirtualTableByName(const std::string_view svTableName, bool bFullName) const
{
	if (!m_ExecutableCode.IsSectionValid())
		return CMemory();

	std::string szFullTableName;
	if (bFullName)
	{
		szFullTableName.assign(svTableName);
	}
	else
	{
#if defined _WIN32 && _M_X64
		szFullTableName.assign(".?AV" + std::string(svTableName) + "@@");
#else
		szFullTableName.assign(std::to_string(svTableName.length()) + std::string(svTableName));
#endif
	}

	std::string szMask(szFullTableName.length() + 1, 'x');

#if defined _WIN32 && _M_X64
	CModule::ModuleSections_t runTimeData = GetSectionByName(".data");
	CMemory typeDescriptorName = FindPatternSIMD(szFullTableName.data(), szMask, nullptr, &runTimeData);
	if (!typeDescriptorName)
		return CMemory();
	
	CModule::ModuleSections_t readOnlyData = GetSectionByName(".rdata");
	
	CMemory rttiTypeDescriptor = typeDescriptorName.Offset(-0x10);
	const uintptr_t rttiTDRva = rttiTypeDescriptor.GetPtr() - m_pModuleBase; // The RTTI gets referenced by a 4-Byte RVA address. We need to scan for that address.
	
	CMemory reference;
	while ((reference = FindPatternSIMD(&rttiTDRva, "xxxx", reference, &readOnlyData))) // Get reference typeinfo in vtable
	{
		// Check if we got a RTTI Object Locator for this reference by checking if -0xC is 1, which is the 'signature' field which is always 1 on x64.
		// Check that offset of this vtable is 0
		if (reference.Offset(-0xC).GetValue<int32_t>() == 1 && reference.Offset(-0x8).GetValue<int32_t>() == 0)
		{
			CMemory referenceOffset = reference.Offset(-0xC);
			CMemory rttiCompleteObjectLocator = FindPatternSIMD(&referenceOffset, "xxxxxxxx", nullptr, &readOnlyData);
			if(rttiCompleteObjectLocator)
				return rttiCompleteObjectLocator.Offset(0x8);
		}
	
		reference.OffsetSelf(0x4);
	}
#else
	CMemory typeInfoName = FindPatternSIMD(szFullTableName.data(), szMask);
	if(!typeInfoName)
		return CMemory();

	CMemory referenceTypeName = FindPatternSIMD(typeInfoName, "xxxxxxxx"); // Get reference to type name.
	if(!referenceTypeName)
		return CMemory();

	CMemory typeInfo = referenceTypeName.Offset(-0x8); // Offset -0x8 to typeinfo.

	CMemory reference;
	while((reference = FindPatternSIMD(typeInfo, "xxxxxxxx", reference))) // Get reference typeinfo in vtable
	{
		if(reference.Offset(-0x8).GetValue<int64_t>() == 0) // Offset to this.
		{
			return reference.Offset(0x8);
		}

		reference.OffsetSelf(0x8);
	}
#endif

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: get the module section by name (example: '.rdata', '.text')
// Input  : svModuleName
// Output : ModuleSections_t
//-----------------------------------------------------------------------------
CModule::ModuleSections_t CModule::GetSectionByName(const std::string_view svSectionName) const
{
	for (const ModuleSections_t& section : m_vModuleSections)
	{
		if (section.m_svSectionName == svSectionName)
			return section;
	}

	return ModuleSections_t();
}

//-----------------------------------------------------------------------------
// Purpose: returns the module base
//-----------------------------------------------------------------------------
uintptr_t CModule::GetModuleBase() const
{
	return m_pModuleBase;
}

//-----------------------------------------------------------------------------
// Purpose: returns the module name
//-----------------------------------------------------------------------------
std::string_view CModule::GetModuleName() const
{
	return m_svModuleName;
}
