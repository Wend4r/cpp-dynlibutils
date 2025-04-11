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
// Purpose: Finds an array of bytes in process memory using SIMD instructions
// Input  : *pPattern
//          svMask
//          pStartAddress
//          *pModuleSection
// Output : CMemory
//-----------------------------------------------------------------------------
CMemory CModule::FindPattern(const CMemory pPatternMem, const std::string_view svMask, const CMemory pStartAddress, const Section_t* pModuleSection) const
{
	const auto* pPattern = pPatternMem.RCast<const std::uint8_t*>();

	const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

	assert(pSection->IsValid());

	const uintptr_t base = pSection->m_pBase;
	const size_t sectionSize = pSection->m_nSectionSize;
	const size_t patternSize = svMask.length();

	auto* pData = reinterpret_cast<std::uint8_t*>(base);
	const auto* end = pData + sectionSize - patternSize;

	if (pStartAddress)
	{
		auto* start = pStartAddress.RCast<std::uint8_t*>();
		if (start < pData || start > end)
			return DYNLIB_INVALID_MEMORY;

		pData = start;
	}

	// Prepare bitmasks for each 16-byte chunk of the pPattern.
	const std::uint8_t numMasks = static_cast<std::uint8_t>((patternSize + 15) / 16);

	int masks[64] = {0}; // Support up to 1024-byte patterns.

	for (std::uint8_t i = 0; i < numMasks; ++i)
	{
		const size_t offset = i * 16;
		const size_t chunkSize = std::min<size_t>(patternSize - offset, 16);

		for (std::uint8_t j = 0; j < chunkSize; ++j)
		{
			if (svMask[offset + j] == 'x')
			{
				masks[i] |= (1 << j); // Set bit for bytes that must match.
			}
		}
	}

	const __m128i patternChunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern));

	for (; pData <= end; ++pData)
	{
		// Prefetch next memory region to reduce cache misses.
		_mm_prefetch(reinterpret_cast<const char*>(pData + 64), _MM_HINT_NTA);

		// Compare first 16-byte block.
		__m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
		__m128i cmpMask = _mm_cmpeq_epi8(patternChunk, chunk);

		if ((_mm_movemask_epi8(cmpMask) & masks[0]) != masks[0])
			continue;

		// If pattern is longer than 16 bytes, verify remaining blocks:
		{
			bool bFound = true;

			for (std::uint8_t i = 1; i < numMasks; ++i)
			{
				const __m128i chunkPattern = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern + i * 16));
				const __m128i chunkMemory = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData + i * 16));
				const __m128i cmp = _mm_cmpeq_epi8(chunkPattern, chunkMemory);

				if ((_mm_movemask_epi8(cmp) & masks[i]) != masks[i])
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
				return pData;
		}
	}

	return DYNLIB_INVALID_MEMORY;
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
