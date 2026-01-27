//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

using namespace DynLibUtils;

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : szModuleName (without extension .dll/.so)
//-----------------------------------------------------------------------------
template<typename Mutex>
CAssemblyModule<Mutex>::CAssemblyModule(const std::string_view szModuleName)
{
	InitFromName(szModuleName);
	m_pExecutableSection = nullptr;
}

//-----------------------------------------------------------------------------
// Purpose: constructor
// Input  : pModuleMemory
//-----------------------------------------------------------------------------
template<typename Mutex>
CAssemblyModule<Mutex>::CAssemblyModule(const CMemory& pModuleMemory)
{
	InitFromMemory(pModuleMemory);
	m_pExecutableSection = nullptr;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetFunctionByName(const std::string_view svFunctionName) const noexcept
{
	CCache hKey(svFunctionName, 1);
	if (auto pAddr = GetAddress(hKey))
	{
		return pAddr;
	}
	auto pAddr = GetFunction(svFunctionName);
	{
		UniqueLock_t lock(m_mutex);
		m_mapCached[std::move(hKey)] = pAddr;
	}
	return pAddr;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetVirtualTableByName(const std::string_view svTableName, bool bDecorated) const
{
	CCache hKey(svTableName, 2);
	if (auto pAddr = GetAddress(hKey))
	{
		return pAddr;
	}
	auto pAddr = GetVirtualTable(svTableName, bDecorated);
	{
		UniqueLock_t lock(m_mutex);
		m_mapCached[std::move(hKey)] = pAddr;
	}
	return pAddr;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::GetAddress(const CCache& hKey) const noexcept
{
	SharedLock_t lock(m_mutex);
	auto it = m_mapCached.find(hKey);
	if (it == m_mapCached.end())
	{
		return nullptr;
	}
	return it->second;
}

template<typename Mutex>
CMemory CAssemblyModule<Mutex>::FindPattern(const CMemoryView<std::uint8_t>& pPatternMem, const std::string_view svMask, const CMemory& pStartAddress, const Section_t* pModuleSection) const
{
	const auto* pPattern = pPatternMem.RCastView();

	CCache sKey(pPattern, svMask.size(), pStartAddress, pModuleSection);
	if (auto pAddr = GetAddress(sKey))
	{
		return pAddr;
	}

	const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

	if (!pSection || !pSection->IsValid())
		return DYNLIB_INVALID_MEMORY;

	const std::uintptr_t base = pSection->GetAddr();
	const std::size_t sectionSize = pSection->m_nSectionSize;
	const std::size_t patternSize = svMask.size();

	auto* pData = reinterpret_cast<std::uint8_t*>(base);
	const auto* pEnd = pData + sectionSize - patternSize;

	if (pStartAddress)
	{
		auto* start = pStartAddress.RCast<std::uint8_t*>();
		if (start < pData || start > pEnd)
			return DYNLIB_INVALID_MEMORY;

		pData = start;
	}

	auto numMasks = static_cast<std::uint8_t>(std::ceil(static_cast<float>(patternSize) / 16.f));

#if !DYNLIBUTILS_ARCH_ARM
	std::array<int, 64> masks = {};// 64*16 = enough masks for 1024 bytes.

	for (std::uint8_t i = 0; i < numMasks; ++i)
	{
		for (std::int8_t j = static_cast<std::int8_t>(std::min<std::size_t>(patternSize - i * 16, 16)) - 1; j >= 0; --j)
		{
			if (svMask[static_cast<std::size_t>(i * 16 + j)] == 'x')
			{
				masks[i] |= 1 << j;
			}
		}
	}

	const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern));
	__m128i xmm2, xmm3, msks;
	for (; pData != pEnd; _mm_prefetch(reinterpret_cast<const char*>(++pData + 64), _MM_HINT_NTA))
	{
		xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
		msks = _mm_cmpeq_epi8(xmm1, xmm2);
		if ((_mm_movemask_epi8(msks) & masks[0]) == masks[0])
		{
			bool found = true;
			for (uint8_t i = 1; i < numMasks; ++i)
			{
				xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pData + i * 16)));
				xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pPattern + i * 16)));
				msks = _mm_cmpeq_epi8(xmm2, xmm3);
				if ((_mm_movemask_epi8(msks) & masks[i]) != masks[i])
				{
					found = false;
					break;
				}
			}

			if (found)
			{
				UniqueLock_t lock(m_mutex);
				m_mapCached[std::move(sKey)] = pData;
				return pData;
			}
		}
	}
#else
	// Precompute contiguous 'x' runs for memcmp.
	struct SignatureMask_t
	{
		std::size_t offset;
		std::size_t length;
	};

	SignatureMask_t sigs[(s_nDefaultPatternSize - 1) / 2]; // upper bound is fine; SIZE is already capped upstream
	std::size_t numSigs = 0;

	{
		std::size_t i = 0;
		while (i < patternSize)
		{
			// Skip wildcards
			while (i < patternSize && svMask[i] != 'x')
				++i;

			if (i >= patternSize)
				break;

			const std::size_t start = i;
			while (i < patternSize && svMask[i] == 'x')
				++i;

			const std::size_t len = i - start;
			if (len)
			{
				if (numSigs < std::size(sigs))
				{
					sigs[numSigs++] = SignatureMask_t{ start, len };
				}
				else
				{
					// Fallback: if too many runs for the static buffer, do a simple byte-wise path later.
					numSigs = 0;
					break;
				}
			}
		}
	}

	// If mask has no 'x', first position matches trivially.
	if (numSigs == 0 && std::find(svMask.begin(), svMask.end(), 'x') == svMask.end())
	{
		UniqueLock_t lock(m_mutex);
		m_mapCached[std::move(sKey)] = pData;
		return pData;
	}

	// Main scan.
	for (; pData <= pEnd; ++pData)
	{
		bool bFound = true;

		if (numSigs)
		{
			// memcmp only over the strict segments
			for (std::size_t r = 0; r < numSigs; ++r)
			{
				const SignatureMask_t& run = sigs[r];
				if (std::memcmp(pData + run.offset, pPattern + run.offset, run.length) != 0)
				{
					bFound = false;
					break;
				}
			}
		}
		else
		{
			// Degenerate path if run buffer overflowed: byte-wise check honoring mask.
			for (std::size_t j = 0; j < patternSize; ++j)
			{
				if (svMask[j] == 'x' && pData[j] != pPattern[j])
				{
					bFound = false;
					break;
				}
			}
		}

		if (bFound)
		{
			UniqueLock_t lock(m_mutex);
			m_mapCached[std::move(sKey)] = pData;
			return pData;
		}
	}
#endif // !DYNLIBUTILS_ARCH_ARM

	return DYNLIB_INVALID_MEMORY;
}

#ifdef DYNLIBUTILS_SEPARATE_SOURCE_FILES
	#if DYNLIBUTILS_PLATFORM_WINDOWS
		#include "windows/module.cpp"
	#elif DYNLIBUTILS_PLATFORM_LINUX
		#include "linux/module.cpp"
	#elif DYNLIBUTILS_PLATFORM_APPLE
		#include "apple/module.cpp"
	#else
		#error "Unsupported platform"
	#endif
#endif

template class DynLibUtils::CAssemblyModule<DynLibUtils::CNullMutex>;
template class DynLibUtils::CAssemblyModule<std::shared_mutex>;