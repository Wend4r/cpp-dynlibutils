//
// DynLibUtils
// Copyright (C) 2023 komashchenko (Phoenix)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_MODULE_HPP
#define DYNLIBUTILS_MODULE_HPP

#pragma once

#include "memaddr.hpp"

#include <emmintrin.h>

#include <array>
#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

#ifdef __cpp_concepts
#	include <concepts>
#endif

#ifdef __cpp_consteval
#	define DYNLIB_COMPILE_TIME_EXPR consteval
#else
#	define DYNLIB_COMPILE_TIME_EXPR constexpr
#endif

namespace DynLibUtils
{
	struct Section_t
	{
		Section_t() noexcept : m_nSectionSize(0) {}
		Section_t(const Section_t&) = default;
		Section_t& operator=(const Section_t&) = default;
		Section_t(Section_t&& other) noexcept : m_nSectionSize(std::move(other.m_nSectionSize)), m_svSectionName(std::move(other.m_svSectionName)), m_pBase(std::move(other.m_pBase)) {}
		Section_t(size_t nSectionSize, const std::string_view svSectionName, uintptr_t pSectionBase) : m_nSectionSize(nSectionSize), m_svSectionName(svSectionName), m_pBase(pSectionBase) {}

		[[nodiscard]]
		bool IsValid() const noexcept { return m_pBase.IsValid(); }

		std::size_t m_nSectionSize;     // Size of the section.
		std::string m_svSectionName;    // Name of the section.
		CMemory m_pBase;                // Start address of the section.
	}; // struct Section_t

	static constexpr std::size_t sm_nMaxPatternSize = 64;
	static constexpr std::size_t sm_nMaxSimdBlocks = 1 << 6; // 64 blocks = 1024 bytes per chunk.

	template<std::size_t N>
	struct Pattern_t
	{
		static constexpr auto sm_nMaxSize = N;

		std::size_t m_nSize = 0;
		std::array<std::uint8_t, N> m_aBytes{};
		std::array<char, N> m_aMask{};
	}; // struct Pattern_t

	// Concept for pattern callback.
	// Signature: bool callback(std::size_t index, CMemory match)
	// Returns:   false -> continue scanning.
	//            true  -> stop scanning.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
	template<typename T>
	concept PatternCallback_t = requires(T func, std::size_t index, CMemory match)
	{
		{ func(index, match) } -> std::same_as<bool>;
	};
#else
#	define PatternCallback_t typename
#endif

class CModule
{
private:
	std::string m_sPath;
	std::string m_sLastError;
	std::vector<Section_t> m_vecSections;
	const Section_t *m_pExecutableSection;
	void* m_pHandle;

public:
	template<std::size_t N>
	struct SignatureIterator_t : public Pattern_t<N>
	{
		using Base_t = Pattern_t<N>;

		CModule* m_pModule;

		constexpr SignatureIterator_t(Base_t&& pattern, CModule* module) : Base_t(std::move(pattern)), m_pModule(module) {}

		[[nodiscard]]
		CMemory operator()(const CMemory pStart = nullptr, const Section_t* pSection = nullptr) const
		{
			return m_pModule->FindPattern<N>(CMemory(Base_t::m_aBytes.data()), std::string_view(Base_t::m_aMask.data(), Base_t::m_nSize), pStart, pSection);
		}

		[[nodiscard]]
		CMemory Offset(const std::ptrdiff_t offset, const CMemory pStart = nullptr, const Section_t* pSection = nullptr) const
		{
			return operator()(pStart, pSection).Offset(offset);
		}

		[[nodiscard]]
		CMemory Deref(const std::uintptr_t deref = 1, const CMemory pStart = nullptr, const Section_t* pSection = nullptr) const
		{
			return operator()(pStart, pSection).Deref(deref);
		}

		[[nodiscard]]
		CMemory FollowCall(const std::ptrdiff_t opcodeOffset = 0x1, const std::ptrdiff_t nextInstructionOffset = 0x5, const CMemory pStart = nullptr, const Section_t* pSection = nullptr) const
		{
			return operator()(pStart, pSection).FollowNearCall(opcodeOffset, nextInstructionOffset);
		}
	}; // struct SignatureIterator_t

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

protected: // Internal pattern methods.
	template<std::size_t INDEX = 0, std::size_t N>
	[[always_inline, nodiscard]]
	static inline DYNLIB_COMPILE_TIME_EXPR void ProcessPattern(const char (&szInput)[N], std::size_t& i, std::size_t& nIndex, std::array<std::uint8_t, N / 2>& aBytes, std::array<char, N / 2>& aMask)
	{
		constexpr auto funcIsHexDigit = [](char c) -> bool
		{
			return ('0' <= c && c <= '9') || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
		};

		constexpr auto funcHexCharToByte = [](char c) -> std::uint8_t
		{
			return ('0' <= c && c <= '9') ? c - '0' : ('A' <= c && c <= 'F') ? c - 'A' + 10 : c - 'a' + 10;
		};

		constexpr std::size_t nLength = N - 1; // Exclude null-terminated character.

		if constexpr (INDEX < nLength)
		{
			const char c = szInput[i];

			if (c == ' ') 
			{
				i++;
				ProcessPattern<INDEX + 1>(szInput, i, nIndex, aBytes, aMask);
			}
			else if (c == '?')
			{
				aBytes[nIndex] = 0;
				aMask[nIndex] = '?';

				i++;

				if (i < nLength && szInput[i] == '?')
					i++;

				nIndex++;
				ProcessPattern<INDEX + 1>(szInput, i, nIndex, aBytes, aMask);
			}
			else if (funcIsHexDigit(c))
			{
				if (i + 1 < nLength)
				{
					if (funcIsHexDigit(szInput[i + 1]))
					{
						aBytes[nIndex] = (funcHexCharToByte(c) << 4) | funcHexCharToByte(szInput[i + 1]);
						aMask[nIndex] = 'x';

						i += 2;
						nIndex++;
						ProcessPattern<INDEX + 1>(szInput, i, nIndex, aBytes, aMask);
					}
					else
					{
						static_assert(R"(Invalid character in pattern. Allowed: <space> or pair: "0-9", "a-f", "A-F" or "?")");
					}
				}
				else
				{
					static_assert("Missing second hexadecimal digit in pattern");
				}
			}
			else
			{
				static_assert("Invalid character in pattern");
			}
		}
	}

public:
	//-----------------------------------------------------------------------------
	// Purpose: Converts a string pattern with wildcards to an array of bytes and mask
	// Input  : svInput - pattern string like "48 8B ?? 89 ?? ?? 41"
	// Output : Pattern_t<N / 2> (fixed-size array by N cells with mask and used size)
	//----------------------------------------------------------------------------
	template<std::size_t N>
	[[always_inline, nodiscard]]
	static inline DYNLIB_COMPILE_TIME_EXPR auto ParsePatternFromString(const char (&szInput)[N])
	{
		static_assert(N > 1, "Pattern cannot be empty");

		std::size_t i = 0;

		Pattern_t<N / 2> result{};

		ProcessPattern<0>(szInput, i, result.m_nSize, result.m_aBytes, result.m_aMask);

		return result;
	}

	template<std::size_t N = sm_nMaxPatternSize / 2>
	[[nodiscard]]
	static auto ParsePattern(const std::string_view svInput)
	{
		Pattern_t<N / 2> result {};

		auto funcGetHexByte = [](char c) -> uint8_t
		{
			if ('0' <= c && c <= '9') return c - '0';
			if ('a' <= c && c <= 'f') return 10 + (c - 'a');
			if ('A' <= c && c <= 'F') return 10 + (c - 'A');

			return 0;
		};

		size_t n = 0;
		std::uint32_t nOut = 0;

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

				bool bIsValid = nLeft && nRight;

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
	[[always_inline, nodiscard]]
	inline DYNLIB_COMPILE_TIME_EXPR auto CreateSignatureFromString(const char (&szInput)[N])
	{
		return SignatureIterator_t<N / 2>(ParsePatternFromString<N / 2>(szInput), this);
	}

	template<std::size_t N>
	[[nodiscard]]
	auto CreateSignature(const std::string_view svInput)
	{
		return SignatureIterator_t<N / 2>(ParsePattern<N / 2>(svInput), this);
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
	[[always_inline, flatten, hot]]
	inline CMemory FindPattern(const CMemory pPatternMem, const std::string_view svMask, const CMemory pStartAddress, const Section_t* pModuleSection) const
	{
		const auto* pPattern = pPatternMem.RCast<const std::uint8_t*>();

		const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

		if (!pSection || !pSection->IsValid())
			return DYNLIB_INVALID_MEMORY;

		const std::uintptr_t base = pSection->m_pBase;
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

		constexpr auto kSimdBytes = sizeof(__m128i); // 128 bits = 16 bytes.
		constexpr auto kMaxSimdBlocks = std::max<std::size_t>(1ul, N);

		const std::size_t numBlocks = (patternSize + (kSimdBytes - 1)) / kSimdBytes;

		std::uint16_t bitMasks[kMaxSimdBlocks] = {};
		__m128i patternChunks[kMaxSimdBlocks];

		for (std::size_t i = 0; i < numBlocks; ++i)
		{
			const std::size_t offset = i * kSimdBytes;
			patternChunks[i] = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern + offset));

			for (std::size_t j = 0; j < kSimdBytes; ++j)
			{
				const std::size_t idx = offset + j;
				if (idx >= patternSize)
					break;

				if (svMask[idx] == 'x')
					bitMasks[i] |= (1u << j);
			}
		}

		for (; pData <= pEnd; ++pData)
		{
			_mm_prefetch(reinterpret_cast<const char*>(pData + 64), _MM_HINT_NTA);

			bool bFound = true;

			for (std::size_t i = 0; i < numBlocks; ++i)
			{
				const __m128i dataChunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData + i * kSimdBytes));
				const __m128i cmp = _mm_cmpeq_epi8(dataChunk, patternChunks[i]);
				const int mask = _mm_movemask_epi8(cmp);

				if ((mask & bitMasks[i]) != bitMasks[i])
				{
					bFound = false;
					break;
				}
			}

			if (bFound)
				return CMemory(pData);
		}

		return DYNLIB_INVALID_MEMORY;
	}

	template<std::size_t N>
	[[nodiscard]]
	inline CMemory FindPattern(const Pattern_t<N>&& pattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern<N>(CMemory(pattern.m_aBytes.data()), std::string_view(pattern.m_aMask.data()), pStartAddress, pModuleSection);
	}

	template<std::size_t N, PatternCallback_t FUNC>
	std::size_t FindAllPatterns(const SignatureIterator_t<N>& sig, const FUNC& callback, CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

		if (!pSection || !pSection->IsValid())
			return 0;

		const std::uintptr_t base = pSection->m_pBase;
		const std::size_t sectionSize = pSection->m_nSectionSize;

		std::uint8_t* pStart = pStartAddress ? pStartAddress.RCast<std::uint8_t*>() : reinterpret_cast<std::uint8_t*>(base);
		const std::uint8_t* pEnd = reinterpret_cast<const std::uint8_t*>(base + sectionSize);

		std::size_t foundLength = 0;

		for (CMemory match; 
		     (match = sig(pStart, pSection)).IsValid() && 
		     pEnd <= match.RCast<const std::uint8_t*>(); 
		     pStart = match.Offset(1).RCast<std::uint8_t*>()
		)
		{
			if (callback(foundLength, match))
				break;

			foundLength++;
		}

		return foundLength;
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
}; // class CModule

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_HPP
