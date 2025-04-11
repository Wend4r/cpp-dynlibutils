//
// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_MODULE_HPP
#define DYNLIBUTILS_MODULE_HPP
#ifdef _WIN32
#pragma once
#endif

#include "memaddr.hpp"

#include <emmintrin.h>

#include <array>
#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

namespace DynLibUtils {

class CModule
{
public:
	struct Section_t
	{
		Section_t() noexcept : m_nSectionSize(0) {}
		Section_t(const Section_t&) = default;
		Section_t& operator=(const Section_t&) = default;
		Section_t(Section_t&& other) noexcept : m_nSectionSize(std::move(other.m_nSectionSize)), m_svSectionName(std::move(other.m_svSectionName)), m_pBase(std::move(other.m_pBase)) {}
		Section_t(size_t nSectionSize, const std::string_view svSectionName, uintptr_t pSectionBase) : m_nSectionSize(nSectionSize), m_svSectionName(svSectionName), m_pBase(pSectionBase) {}

		[[nodiscard]] bool IsValid() const noexcept { return m_pBase.IsValid(); }

		std::size_t m_nSectionSize;     // Size of the section.
		std::string m_svSectionName;    // Name of the section.
		CMemory m_pBase;                // Start address of the section.
	};

	static constexpr std::size_t sm_nMaxPatternSize = 64;

	template<std::size_t N>
	struct Pattern_t
	{
		static constexpr auto sm_nMaxSize = N;

		std::size_t m_nSize = 0;
		std::array<std::uint8_t, sm_nMaxPatternSize> m_aBytes{};
		std::array<char, sm_nMaxPatternSize> m_aMask{};
	};

	CModule() : m_pExecutableSection(nullptr), m_pHandle(nullptr) {}
	~CModule();

	CModule(const CModule&) = delete;
	CModule& operator=(const CModule&) = delete;
	CModule(CModule&& other) noexcept : m_sPath(std::move(other.m_sPath)), m_vecSections(std::move(other.m_vecSections)), m_pExecutableSection(std::move(other.m_pExecutableSection)), m_pHandle(std::move(other.m_pHandle)) {}
	explicit CModule(const std::string_view svModuleName);
	explicit CModule(const char* pszModuleName) : CModule(std::string_view(pszModuleName)) {}
	explicit CModule(const std::string& sModuleName) : CModule(std::string_view(sModuleName)) {}
	CModule(const CMemory pModuleMemory);

	bool LoadFromPath(const std::string_view svModelePath, int flags);

	bool InitFromName(const std::string_view svModuleName, bool bExtension = false);
	bool InitFromMemory(const CMemory pModuleMemory, bool bForce = true);

	//-----------------------------------------------------------------------------
	// Purpose: Converts a string pattern with wildcards to an array of bytes and mask
	// Input  : svInput - pattern string like "48 8B ?? 89 ?? ?? 41"
	// Output : Pattern_t<N> (fixed-size array by N cells with mask and used size)
	//-----------------------------------------------------------------------------
	template<std::size_t N = sm_nMaxPatternSize>
	[[always_inline]] [[nodiscard]] static inline constexpr Pattern_t<N> ParsePattern(const std::string_view svInput)
	{
		Pattern_t<N> result {};

		auto funcGetHexByte = [](char c) -> uint8_t
		{
			if ('0' <= c && c <= '9') return c - '0';
			if ('a' <= c && c <= 'f') return 10 + (c - 'a');
			if ('A' <= c && c <= 'F') return 10 + (c - 'A');

			return 0;
		};

		size_t n = 0;
		size_t nOut = 0;

		while (n < svInput.length() && nOut < sm_nMaxPatternSize)
		{
			if (svInput[n] == '?')
			{
				++n;

				if (n < svInput.size() && svInput[n] == '?')
					++n;

				result.m_aBytes[nOut] = 0x00;
				result.m_aMask[nOut] = '?';
				++nOut;
			}
			else if (n + 1 < svInput.size())
			{
				auto nLeft = funcGetHexByte(svInput[n]), nRight = funcGetHexByte(svInput[n + 1]);

				bool bIsValid = (nLeft && nRight);

				assert(bIsValid && R"(Passing invalid characters. Allowed: <space> or pair: "0-9", "a-f", "A-F" or "?")");
				if (!bIsValid)
				{
					++n;
					continue;
				}

				result.m_aBytes[nOut] = (nLeft << 4) | nRight;
				result.m_aMask[nOut] = 'x';
				++nOut;

				n += 2;
			}

			++n;
		}

		result.m_aMask[nOut] = '\0'; // Stores null-terminated character to FindPattern (raw). Don't do (N - 1).
		result.m_nSize = nOut;

		return result;
	}
	template<std::size_t N>
	[[always_inline]] [[nodiscard]] static inline constexpr Pattern_t<N> ParsePatternString(const char (&szInput)[N])
	{
		return ParsePattern<N>(std::string_view(szInput, N - 1));
	}

	//-----------------------------------------------------------------------------
	// Purpose: Finds an array of bytes in process memory using SIMD instructions
	// Input  : *pPattern
	//          svMask
	//          pStartAddress
	//          *pModuleSection
	// Output : CMemory
	//-----------------------------------------------------------------------------
	template<std::size_t N = sm_nMaxPatternSize>
	[[always_inline]] inline constexpr CMemory FindPattern(const CMemory pPatternMem, const std::string_view svMask, const CMemory pStartAddress, const Section_t* pModuleSection) const
	{
		const auto* pPattern = pPatternMem.RCast<std::uint8_t*>();

		const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

		assert(pSection && pSection->IsValid());

		const uintptr_t base = pSection->m_pBase;
		const std::size_t sectionSize = pSection->m_nSectionSize;
		const std::size_t patternSize = svMask.length();

		auto* pData = reinterpret_cast<std::uint8_t*>(base);
		const auto* pEnd = pData + sectionSize - patternSize;

		if (pStartAddress)
		{
			auto* start = pStartAddress.RCast<std::uint8_t*>();
			if (start < pData || start > pEnd)
				return DYNLIB_INVALID_MEMORY;

			pData = start;
		}

		// Prepare bitmasks for each 16-byte chunk of the pPattern.
		constexpr std::uint8_t kMaskGroupSize = N / 4;
		const std::uint8_t numMasks = static_cast<std::uint8_t>((patternSize + (kMaskGroupSize - 1)) / kMaskGroupSize);

		int masks[N] = {0}; // Support up to 1024-byte patterns.

		for (std::uint8_t i = 0; i < numMasks; ++i)
		{
			const std::size_t offset = i * sizeof(__m128i);
			const auto chunkSize = std::min<std::size_t>(patternSize - offset, sizeof(__m128i));

			for (std::uint8_t j = 0; j < chunkSize; ++j)
			{
				if (svMask[offset + j] == 'x')
				{
					masks[i] |= (1 << j); // Set bit for bytes that must match.
				}
			}
		}

		const __m128i patternChunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern));

		for (; pData <= pEnd; ++pData)
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
					const __m128i chunkPattern = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern + i * sizeof(__m128i)));
					const __m128i chunkMemory = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData + i * sizeof(__m128i)));
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
	template<std::size_t N> [[always_inline]] [[nodiscard]] inline constexpr CMemory FindPattern(Pattern_t<N>&& pattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern<N>(CMemory(pattern.m_aBytes.data()), std::string_view(pattern.m_aMask.data()), pStartAddress, pModuleSection);
	}
	template<std::size_t N> [[always_inline]] [[nodiscard]] inline constexpr CMemory FindPatternString(const char (&szInput)[N], const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern(ParsePatternString(szInput), pStartAddress, pModuleSection);
	}
	[[nodiscard]] CMemory FindPattern(const std::string_view svPattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern(ParsePattern(svPattern), pStartAddress, pModuleSection);
	}

	[[nodiscard]] CMemory GetVirtualTableByName(const std::string_view svTableName, bool bDecorated = false) const;
	[[nodiscard]] CMemory GetFunctionByName(const std::string_view svFunctionName) const noexcept;

	[[nodiscard]] void* GetHandle() const noexcept { return m_pHandle; }
	[[nodiscard]] CMemory GetBase() const noexcept;
	[[nodiscard]] std::string_view GetPath() const { return m_sPath; }
	[[nodiscard]] std::string_view GetLastError() const { return m_sLastError; }
	[[nodiscard]] std::string_view GetName() const { std::string_view svModulePath(m_sPath); return svModulePath.substr(svModulePath.find_last_of("/\\") + 1); }
	[[nodiscard]] const Section_t *GetSectionByName(const std::string_view svSectionName) const
	{
		for (const auto& section : m_vecSections)
			if (svSectionName == section.m_svSectionName)
				return &section;

		return nullptr;
	}

protected:
	void SaveLastError();

private:
	std::string m_sPath;
	std::string m_sLastError;
	std::vector<Section_t> m_vecSections;
	const Section_t *m_pExecutableSection;
	void* m_pHandle;
};

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_HPP
