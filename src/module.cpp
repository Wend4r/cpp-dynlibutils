// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <cstring>
#include <cmath>
#include <emmintrin.h>

using namespace DynLibUtils;

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : szModuleName (without extension .dll/.so)
//-----------------------------------------------------------------------------
CModule::CModule(const std::string_view szModuleName) : m_pHandle(nullptr)
{
	InitFromName(szModuleName);
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
CModule::CModule(const CMemory pModuleMemory) : m_pHandle(nullptr)
{
	InitFromMemory(pModuleMemory);
}

//-----------------------------------------------------------------------------
// Purpose: Converts a string pattern with wildcards to an array of bytes and mask
// Input  : svInput
// Output : std::pair<std::vector<std::uint8_t>, std::string>
//-----------------------------------------------------------------------------
std::pair<std::vector<std::uint8_t>, std::string> CModule::PatternToMaskedBytes(const std::string_view svInput)
{
	char* pszPatternStart = const_cast<char*>(svInput.data());
	char* pszPatternEnd = pszPatternStart + svInput.size();
	std::vector<std::uint8_t> vBytes;
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
			vBytes.push_back(static_cast<std::uint8_t>(strtoul(pszCurrentByte, &pszCurrentByte, 16)));
			svMask += 'x';
		}
	}

	return std::make_pair(std::move(vBytes), std::move(svMask));
}

//-----------------------------------------------------------------------------
// Purpose: Finds an array of bytes in process memory using SIMD instructions
// Input  : *pPattern
//          szMask
//          pStartAddress
//          *pModuleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPattern(const CMemory pPattern, const std::string_view szMask, const CMemory pStartAddress, const ModuleSections_t* pModuleSection) const
{
	const std::uint8_t* pattern = pPattern.RCast<const std::uint8_t*>();
	const ModuleSections_t* section = pModuleSection ? pModuleSection : &m_ExecutableCode;
	if (!section->IsSectionValid())
		return CMemory();

	const uintptr_t nBase = section->m_pSectionBase;
	const size_t nSize = section->m_nSectionSize;

	const size_t nMaskLen = szMask.length();
	const std::uint8_t* pData = reinterpret_cast<std::uint8_t*>(nBase);
	const std::uint8_t* pEnd = pData + nSize - nMaskLen;

	if(pStartAddress)
	{
		const std::uint8_t* startAddress = pStartAddress.RCast<std::uint8_t*>();
		if(pData > startAddress || startAddress > pEnd)
			return CMemory();

		pData = startAddress;
	}

	int nMasks[64]; // 64*16 = enough masks for 1024 bytes.
	const std::uint8_t iNumMasks = static_cast<std::uint8_t>(std::ceil(static_cast<float>(nMaskLen) / 16.f));

	memset(nMasks, 0, iNumMasks * sizeof(int));
	for (std::uint8_t i = 0; i < iNumMasks; ++i)
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
		xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
		msks = _mm_cmpeq_epi8(xmm1, xmm2);
		if ((_mm_movemask_epi8(msks) & nMasks[0]) == nMasks[0])
		{
			bool bFound = true;
			for (std::uint8_t i = 1; i < iNumMasks; ++i)
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
				return pData;
		}
	}

	return CMemory();
}

//-----------------------------------------------------------------------------
// Purpose: Finds a string pattern in process memory using SIMD instructions
// Input  : svPattern
//          pStartAddress
//          *pModuleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPattern(const std::string_view svPattern, const CMemory pStartAddress, const ModuleSections_t* pModuleSection) const
{
	const std::pair patternInfo = PatternToMaskedBytes(svPattern);
	return FindPattern(patternInfo.first.data(), patternInfo.second, pStartAddress, pModuleSection);
}

//-----------------------------------------------------------------------------
// Purpose: Gets a module section by name (example: '.rdata', '.text')
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
// Purpose: Returns the module handle
//-----------------------------------------------------------------------------
void* CModule::GetHandle() const noexcept
{
	return m_pHandle;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module path
//-----------------------------------------------------------------------------
std::string_view CModule::GetPath() const
{
	return m_sPath;
}

std::string_view CModule::GetLastError() const
{
	return m_sLastError;
}

//-----------------------------------------------------------------------------
// Purpose: Returns the module name
//-----------------------------------------------------------------------------
std::string_view CModule::GetName() const
{
	std::string_view svModulePath(m_sPath);
	return svModulePath.substr(svModulePath.find_last_of("/\\") + 1);
}

#ifndef DYNLIBUTILS_SEPARATE_SOURCE_FILES
	#if defined _WIN32 && _M_X64
		#include "module_windows.cpp"
	#elif defined __linux__ && __x86_64__
		#include "module_linux.cpp"
	#elif defined __APPLE__ && __x86_64__
		#include "module_apple.cpp"
	#else
		#error "Unsupported platform"
	#endif
#endif
