//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_MODULE_HPP
#define DYNLIBUTILS_MODULE_HPP

#pragma once

#include "memaddr.hpp"

#include <emmintrin.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

#ifdef __cpp_concepts
#	include <concepts>
#endif

#ifdef __cpp_lib_debugging
#	include <debugging>
#endif

#ifdef __cpp_consteval
#	define DYNLIB_COMPILE_TIME_EXPR consteval
#else
#	define DYNLIB_COMPILE_TIME_EXPR constexpr
#endif

constexpr uint8_t INVALID_DYNLIB_BYTE = 0xFFu;

namespace DynLibUtils {

struct Section_t : public CMemory // Start address of the section.
{
	// Constructors.
	Section_t(CMemory pSectionBase = nullptr, size_t nSectionSize = 0, const std::string_view& svSectionName = {}) noexcept : CMemory(pSectionBase), m_nSectionSize(nSectionSize), m_svSectionName(svSectionName) {} // Default one.
	Section_t(Section_t&& other) noexcept = default;

	std::size_t m_nSectionSize;     // Size of the section.
	std::string m_svSectionName;    // Name of the section.
}; // struct Section_t

static constexpr std::size_t s_nDefaultPatternSize = 256;
static constexpr std::size_t s_nMaxSimdBlocks = 1 << 6; // 64 blocks = 1024 bytes per chunk.

template<std::size_t SIZE = 0l>
struct Pattern_t
{
	static constexpr std::size_t sm_nMaxSize = SIZE;

	// Constructors.
	constexpr Pattern_t(const Pattern_t<SIZE>& copyFrom) noexcept : m_nSize(copyFrom.m_nSize), m_aBytes(copyFrom.m_aBytes), m_aMask(copyFrom.m_aMask) {}
	constexpr Pattern_t(Pattern_t<SIZE>&& moveFrom) noexcept : m_nSize(std::move(moveFrom.m_nSize)), m_aBytes(std::move(moveFrom.m_aBytes)), m_aMask(std::move(moveFrom.m_aMask)) {}
	constexpr Pattern_t(std::size_t size = 0, const std::array<uint8_t, SIZE>& bytes = {}, const std::array<char, SIZE>& mask = {}) noexcept : m_nSize(size), m_aBytes(bytes), m_aMask(mask) {} // Default one.
	constexpr Pattern_t(std::size_t &&size, std::array<uint8_t, SIZE>&& bytes, const std::array<char, SIZE>&& mask) noexcept : m_nSize(std::move(size)), m_aBytes(std::move(bytes)), m_aMask(std::move(mask)) {}

	// Fields. Available to anyone (so structure).
	std::size_t m_nSize;
	std::array<std::uint8_t, SIZE> m_aBytes;
	std::array<char, SIZE> m_aMask;
}; // struct Pattern_t

// Concept for pattern callback.
// Signature: bool callback(std::size_t index, CMemory match)
// Returns:   false -> stop scanning.
//            true  -> continue scanning.
#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
template<typename T>
concept PatternCallback_t = requires(T func, std::size_t index, CMemory match)
{
	{ func(index, match) } -> std::same_as<bool>;
};
#else
#	define PatternCallback_t typename
#endif

#if defined(__clang__)
#	define DYNLIB_FORCE_INLINE [[gnu::always_inline]] inline
#	define DYNLIB_NOINLINE [[gnu::noinline]]
#elif defined(__GNUC__)
#	define DYNLIB_FORCE_INLINE [[gnu::always_inline]] inline
#	define DYNLIB_NOINLINE [[gnu::noinline]]
#elif defined(_MSC_VER)
#	pragma warning(error: 4714)
#	define DYNLIB_FORCE_INLINE [[msvc::forceinline]]
#	define DYNLIB_NOINLINE [[msvc::noinline]]
#else
#	define DYNLIB_FORCE_INLINE inline
#	define DYNLIB_NOINLINE
#endif

#if __has_cpp_attribute(no_unique_address)
#if defined(_MSC_VER) && _MSC_VER >= 1929
#	define DYNLIB_NUA [[msvc::no_unique_address]]
#else
#	define DYNLIB_NUA [[no_unique_address]]
#endif
#else
#	define DYNLIB_NUA
#endif

template<std::size_t INDEX = 0, std::size_t N, std::size_t SIZE = (N - 1) / 2>
DYNLIB_FORCE_INLINE DYNLIB_COMPILE_TIME_EXPR void ProcessStringPattern(const char (&szInput)[N], std::size_t& n, std::size_t& nIndex, std::array<std::uint8_t, SIZE>& aBytes, std::array<char, SIZE>& aMask)
{
	static_assert(SIZE > 0, "Process pattern cannot be empty");

	constexpr auto funcIsHexDigit = [](char c) -> bool
	{
		return ('0' <= c && c <= '9') ||
		       ('A' <= c && c <= 'F') ||
		       ('a' <= c && c <= 'f');
	};

	constexpr auto funcHexCharToByte = [](char c) -> std::uint8_t
	{
		if ('0' <= c && c <= '9')
			return c - '0';

		if ('A' <= c && c <= 'F')
			return c - 'A' + 10;

		return c - 'a' + 10;
	};

	constexpr std::size_t nLength = N - 1; // Exclude null-terminated character.

	if constexpr (INDEX < nLength)
	{
		const char c = szInput[n];

		if (c == ' ')
		{
			n++;
			ProcessStringPattern<INDEX + 1>(szInput, n, nIndex, aBytes, aMask);
		}
		else if (c == '?')
		{
			aBytes[nIndex] = 0x00;
			aMask[nIndex] = '?';

			n++;

			if (n < nLength && szInput[n] == '?')
				n++;

			nIndex++;
			ProcessStringPattern<INDEX + 1>(szInput, n, nIndex, aBytes, aMask);
		}
		else if (funcIsHexDigit(c))
		{
			if (n + 1 < nLength)
			{
				const char c2 = szInput[n + 1];

				if (funcIsHexDigit(c2))
				{
					aBytes[nIndex] = (funcHexCharToByte(c) << 4) | funcHexCharToByte(c2);
					aMask[nIndex] = 'x';

					n += 2;
					nIndex++;
					ProcessStringPattern<INDEX + 1>(szInput, n, nIndex, aBytes, aMask);
				}
				else
				{
					n++;
					// Invalid character in pattern. Allowed pair: "0-9", "a-f", "A-F".
				}
			}
			else
			{
				n++;
				// Missing second hexadecimal digit in pattern.
			}
		}
		else
		{
			n++;
			// Invalid character in pattern. Allowed <space> or pair: "0-9", "a-f", "A-F" or "?".
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Converts a string pattern with wildcards to an array of bytes and mask
// Input  : svInput - pattern string like "48 8B ?? 89 ?? ?? 41"
// Output : Pattern_t<SIZE> (fixed-size array by N cells with mask and used size)
//----------------------------------------------------------------------------
template<std::size_t N, std::size_t SIZE = (N - 1) / 2>
[[nodiscard]] DYNLIB_FORCE_INLINE DYNLIB_COMPILE_TIME_EXPR auto ParseStringPattern(const char (&szInput)[N])
{
	static_assert(SIZE > 0, "Pattern cannot be empty");

	std::size_t n = 0;

	Pattern_t<SIZE> result{};

	ProcessStringPattern<0, N, SIZE>(szInput, n, result.m_nSize, result.m_aBytes, result.m_aMask);

	return result;
}

template<std::size_t N = s_nDefaultPatternSize, std::size_t SIZE = (N - 1) / 2>
[[nodiscard]]
inline auto ParsePattern(const std::string_view svInput)
{
	Pattern_t<SIZE> result {};

	auto funcGetHexByte = [](char c) -> uint8_t
	{
		if ('0' <= c && c <= '9') return c - '0';
		if ('a' <= c && c <= 'f') return 10 + (c - 'a');
		if ('A' <= c && c <= 'F') return 10 + (c - 'A');

		return INVALID_DYNLIB_BYTE;
	};

	size_t n = 0;
	std::uint32_t nOut = 0;

	while (n < svInput.length() && nOut < N)
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

			bool bIsValid = nLeft != INVALID_DYNLIB_BYTE && nRight != INVALID_DYNLIB_BYTE;

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

struct CCache
{
	std::string m_svPattern;
	uintptr_t m_nStart;
	uintptr_t m_pSectionAddr;
	size_t m_nSectionSize;

	CCache(std::string_view svName, uintptr_t nMeta = 0)
		: m_svPattern(svName)
		, m_nStart(nMeta)
		, m_pSectionAddr(0)
		, m_nSectionSize(0) {
	}

	CCache(
		const std::uint8_t* pPatternMem,
		const size_t nSize,
		const CMemory pStartAddress = nullptr,
		const Section_t* pModuleSection = nullptr
	)
		: m_svPattern(pPatternMem, pPatternMem + nSize)
		, m_nStart(pStartAddress.GetAddr())
		, m_pSectionAddr(pModuleSection ? pModuleSection->GetAddr() : 0)
		, m_nSectionSize(pModuleSection ? pModuleSection->m_nSectionSize : 0) {
	}

	bool operator==(const CCache& rhs) const noexcept
	{
		return m_svPattern == rhs.m_svPattern &&
		       m_nStart == rhs.m_nStart &&
		       m_pSectionAddr == rhs.m_pSectionAddr &&
		       m_nSectionSize == rhs.m_nSectionSize;
	}

	bool operator<(const CCache& rhs) const noexcept
	{
		if (m_svPattern != rhs.m_svPattern)
			return m_svPattern < rhs.m_svPattern;
		if (m_nStart != rhs.m_nStart)
			return m_nStart < rhs.m_nStart;
		if (m_pSectionAddr != rhs.m_pSectionAddr)
			return m_pSectionAddr < rhs.m_pSectionAddr;
		return m_nSectionSize < rhs.m_nSectionSize;
	}
};

struct CHash
{
	std::size_t operator()(const CCache& k) const noexcept
	{
		static constexpr std::size_t golden_ratio = 0x9e3779b9u;
		std::size_t h = std::hash<std::string>()(k.m_svPattern);
		h ^= std::hash<uintptr_t>()(k.m_nStart) + golden_ratio + (h << 6) + (h >> 2);
		h ^= std::hash<uintptr_t>()(k.m_pSectionAddr) + golden_ratio + (h << 6) + (h >> 2);
		h ^= std::hash<size_t>()(k.m_nSectionSize) + golden_ratio + (h << 6) + (h >> 2);
		return h;
	}
};

struct CNullMutex
{
	void lock() const noexcept {}
	void unlock() const noexcept {}
	bool try_lock() const noexcept { return true; }

	void lock_shared() const noexcept {}
	void unlock_shared() const noexcept {}
	bool try_lock_shared() const noexcept { return true; }
};

template<typename Mutex = CNullMutex>
class CAssemblyModule : public CMemory
{
	using UniqueLock_t = std::unique_lock<Mutex>;
	using SharedLock_t = std::shared_lock<Mutex>;
public:
	template<std::size_t SIZE>
	class CSignatureView : public Pattern_t<SIZE>
	{
		using Base_t = Pattern_t<SIZE>;

	private:
		CAssemblyModule* m_pModule;

	public:
		constexpr CSignatureView() : m_pModule(nullptr) {}
		constexpr CSignatureView(CSignatureView&& moveFrom) : Base_t(std::move(moveFrom)), m_pModule(std::move(moveFrom.m_pModule)) {}
		constexpr CSignatureView(const Base_t& pattern, CAssemblyModule* module) : Base_t(pattern), m_pModule(module) {}
		constexpr CSignatureView(Base_t&& pattern, CAssemblyModule* module) : Base_t(std::move(pattern)), m_pModule(module) {}

		bool IsValid() const { return m_pModule && m_pModule->IsValid(); }

		[[nodiscard]]
		CMemory operator()(const CMemory pStart = nullptr, const Section_t* pSection = nullptr) const
		{
			return Find(pStart, pSection);
		}

		[[nodiscard]] CMemory Find(const CMemory pStart, const Section_t* pSection = nullptr) const
		{
			return m_pModule->FindPattern<SIZE>(CMemory(Base_t::m_aBytes.data()), std::string_view(Base_t::m_aMask.data(), Base_t::m_nSize), pStart, pSection);
		}
		[[nodiscard]] CMemory OffsetAndFind(const std::ptrdiff_t offset, CMemory pStart, const Section_t* pSection = nullptr) const { return Find(pStart + offset, pSection); }
		[[nodiscard]] CMemory OffsetFromSelfAndFind(const CMemory pStart, const Section_t* pSection = nullptr) const { return OffsetAndFind(Base_t::m_nSize, pStart, pSection); }
		[[nodiscard]] CMemory DerefAndFind(const std::uintptr_t deref, CMemory pStart, const Section_t* pSection = nullptr) const { return Find(pStart.Deref(deref), pSection); }
	}; // class CSignatureView<SIZE>

private:
	[[nodiscard]] CMemory GetVirtualTable(const std::string_view svTableName, bool bDecorated = false) const;
	[[nodiscard]] CMemory GetFunction(const std::string_view svFunctionName) const noexcept;
	CMemory GetAddress(const CCache& hKey) const noexcept;

	std::string m_sPath;
	std::string m_sLastError;
	std::vector<Section_t> m_vecSections;

	const Section_t *m_pExecutableSection;

	mutable std::unordered_map<CCache, CMemory, CHash> m_mapCached;
	DYNLIB_NUA mutable Mutex m_mutex;

public:
	CAssemblyModule() : m_pExecutableSection(nullptr) {}
	~CAssemblyModule();

	CAssemblyModule(const CAssemblyModule&) = delete;
	CAssemblyModule& operator=(const CAssemblyModule&) = delete;
	CAssemblyModule(CAssemblyModule&& other) noexcept : CMemory(std::exchange(static_cast<CMemory &>(other), DYNLIB_INVALID_MEMORY)), m_sPath(std::move(other.m_sPath)), m_vecSections(std::move(other.m_vecSections)), m_pExecutableSection(std::move(other.m_pExecutableSection)) {}
	CAssemblyModule(const CMemory pModuleMemory);
	explicit CAssemblyModule(const std::string_view svModuleName);
	explicit CAssemblyModule(const char* pszModuleName) : CAssemblyModule(std::string_view(pszModuleName)) {}
	explicit CAssemblyModule(const std::string& sModuleName) : CAssemblyModule(std::string_view(sModuleName)) {}

	bool LoadFromPath(const std::string_view svModelePath, int flags);

	bool InitFromName(const std::string_view svModuleName, bool bExtension = false);
	bool InitFromMemory(const CMemory pModuleMemory, bool bForce = true);

	template<std::size_t N>
	[[nodiscard]]
	inline auto CreateSignature(const Pattern_t<N> &copyFrom)
	{
		static_assert(N > 0, "Pattern size must be > 0");

		return CSignatureView<N>(copyFrom, this);
	}

	template<std::size_t N>
	[[nodiscard]]
	inline auto CreateSignature(Pattern_t<N> &&moveFrom)
	{
		static_assert(N > 0, "Pattern size must be > 0");

		return CSignatureView<N>(std::move(moveFrom), this);
	}

	//-----------------------------------------------------------------------------
	// Purpose: Finds an array of bytes in process memory
	// Input  : *pPattern
	//          svMask
	//          pStartAddress
	//          *pModuleSection
	// Output : CMemory
	//-----------------------------------------------------------------------------
	template<std::size_t SIZE = (s_nDefaultPatternSize - 1) / 2>
	inline CMemory FindPattern(const CMemoryView<std::uint8_t> pPatternMem, const std::string_view svMask, const CMemory pStartAddress, const Section_t* pModuleSection) const
	{
		const auto* pPattern = pPatternMem.RCastView();

		CCache sKey(pPattern, svMask.size(), pStartAddress, pModuleSection);
		if (auto pAddr = GetAddress(sKey))
			return pAddr;

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

		// Precompute contiguous 'x' runs for memcmp.
		struct SignatureMask_t
		{
			std::size_t offset;
			std::size_t length;
		};

		SignatureMask_t iters[SIZE > 0 ? SIZE : 1]; // upper bound is fine; SIZE is already capped upstream
		std::size_t runCount = 0;

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
					if (runCount < std::size(iters))
					{
						iters[runCount++] = SignatureMask_t{ start, len };
					}
					else
					{
						// Fallback: if too many runs for the static buffer, do a simple byte-wise path later.
						runCount = 0;
						break;
					}
				}
			}
		}

		// If mask has no 'x', first position matches trivially.
		if (runCount == 0 && std::find(svMask.begin(), svMask.end(), 'x') == svMask.end())
		{
			UniqueLock_t lock(m_mutex);
			m_mapCached[std::move(sKey)] = pData;
			return pData;
		}

		// Main scan.
		for (; pData <= pEnd; ++pData)
		{
			bool bFound = true;

			if (runCount)
			{
				// memcmp only over the strict segments
				for (std::size_t r = 0; r < runCount; ++r)
				{
					const SignatureMask_t& run = iters[r];
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

		return DYNLIB_INVALID_MEMORY;
	}

	template<std::size_t SIZE>
	[[nodiscard]]
	inline CMemory FindPattern(const Pattern_t<SIZE>& copyPattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern<SIZE>(copyPattern.m_aBytes.data(), std::string_view(copyPattern.m_aMask.data(), copyPattern.m_nSize), pStartAddress, pModuleSection);
	}

	template<std::size_t SIZE>
	[[nodiscard]]
	inline CMemory FindPattern(Pattern_t<SIZE>&& movePattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		return FindPattern<SIZE>(std::move(movePattern.m_aBytes).data(), std::string_view(std::move(movePattern.m_aMask).data(), std::move(movePattern.m_nSize)), pStartAddress, pModuleSection);
	}

	template<std::size_t SIZE, PatternCallback_t FUNC>
	[[nodiscard]]
	std::size_t FindAllPatterns(const CSignatureView<SIZE>& sig, const FUNC& callback, CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		const Section_t* pSection = pModuleSection ? pModuleSection : m_pExecutableSection;

		if (!pSection || !pSection->IsValid())
			return 0;

		const CMemory pBase = *pSection;

		CMemory pIter = pStartAddress ? pStartAddress : pBase;

		std::size_t foundCount = 0;

		pIter = sig(pIter, pSection);

		do
		{
			if (!callback(foundCount, pIter)) // foundCount = the index of found pattern now.
				break;

			++foundCount;

			// Prevent excessive iterations: ensure pattern is correct.
			assert(1000 > foundCount);
		}
		while((pIter = sig.OffsetFromSelfAndFind(pIter, pSection)).IsValid());

		return foundCount; // Count of the found patterns.
	}

	[[nodiscard]] CMemory GetVirtualTableByName(const std::string_view svTableName, bool bDecorated = false) const;
	[[nodiscard]] CMemory GetFunctionByName(const std::string_view svFunctionName) const noexcept;

	[[nodiscard]] void* GetHandle() const noexcept { return GetPtr(); }
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
}; // class CAssemblyModule

using CModule = CAssemblyModule<CNullMutex>;

class Module final : public CModule
{
public:
	using CBase = CModule;
	using CBase::CBase;
};

extern template class CAssemblyModule<CNullMutex>;
extern template class CAssemblyModule<std::shared_mutex>;

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_HPP
