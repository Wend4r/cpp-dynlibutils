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

#include <array>
#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

namespace DynLibUtils {

static constexpr size_t sm_nMaxPatternLength = 64;

struct MaskedBytes_t
{
	std::array<std::uint8_t, sm_nMaxPatternLength> m_aBytes{};
	std::array<char, sm_nMaxPatternLength> m_aMask{};
	std::size_t m_nSize = 0;
};

class CModule
{
public:
	struct Section_t
	{
		Section_t() : m_nSectionSize(0) {}
		Section_t(const Section_t&) = default;
		Section_t& operator=(const Section_t&) = default;
		Section_t(Section_t&& other) noexcept : m_nSectionSize(std::exchange(other.m_nSectionSize, 0)), m_svSectionName(std::move(other.m_svSectionName)), m_pBase(std::move(other.m_pBase)) {}
		Section_t(size_t nSectionSize, const std::string_view svSectionName, uintptr_t pSectionBase) : m_nSectionSize(nSectionSize), m_svSectionName(svSectionName), m_pBase(pSectionBase) {}

		[[nodiscard]] bool IsValid() const noexcept { return m_pBase.IsValid(); }

		size_t m_nSectionSize;          // Size of the section.
		std::string m_svSectionName;    // Name of the section.
		CMemory m_pBase;                // Start address of the section.
	};

	CModule() : m_pHandle(nullptr) {}
	~CModule();

	CModule(const CModule&) = delete;
	CModule& operator=(const CModule&) = delete;
	CModule(CModule&& other) noexcept : m_sPath(std::move(other.m_sPath)), m_vecSections(std::move(other.m_vecSections)), m_pExecutableSection(std::move(other.m_pExecutableSection)), m_pHandle(std::exchange(other.m_pHandle, nullptr)) {}
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
	// Output : MaskedBytes_t (fixed-size array with mask and used size)
	//-----------------------------------------------------------------------------
	[[nodiscard]] static constexpr MaskedBytes_t PatternToMaskedBytes(const std::string_view svInput)
	{
		MaskedBytes_t result {};

		auto funcIsHex = [](char c) -> bool
		{
			return ('0' <= c && c <= '9') ||
			       ('a' <= c && c <= 'f') ||
			       ('A' <= c && c <= 'F');
		};

		auto funcGetPatternByte = [](char c) -> uint8_t
		{
			if ('0' <= c && c <= '9') return c - '0';
			if ('a' <= c && c <= 'f') return 10 + (c - 'a');
			if ('A' <= c && c <= 'F') return 10 + (c - 'A');

			return 0;
		};

		size_t n = 0;
		size_t nOut = 0;

		while (n < svInput.length() && nOut < sm_nMaxPatternLength)
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
			else if (funcIsHex(svInput[n]) && n + 1 < svInput.size() && funcIsHex(svInput[n + 1]))
			{
				result.m_aBytes[nOut] = (funcGetPatternByte(svInput[n]) << 4) | funcGetPatternByte(svInput[n + 1]);
				result.m_aMask[nOut] = 'x';
				++nOut;

				n += 2;
			}
			else if (svInput[n] == ' ')
			{
				++n;
			}
			else
			{
				assert(false && R"(Passing invalid characters. Allowed ones: <space>, "0-9", "a-f", "A-F" or "?")");

				// Otherwise, skip invalid characters silently.
				++n;
			}
		}

		result.m_nSize = nOut;

		return result;
	}

#ifdef __cpp_consteval
	template<std::size_t N>
	[[nodiscard]] static consteval MaskedBytes_t PatternToMaskedBytes(const char (&szInput)[N])
	{
		return PatternToMaskedBytes(std::string_view(szInput, N - 1));
	}

	[[nodiscard]] static auto PatternToMaskedBytesAuto(auto input)
	{
		if consteval
		{
			return PatternToMaskedBytes(input);
		}
		else
		{
			return PatternToMaskedBytes(std::string_view(input));
		}
	}
#endif // defined(__cpp_consteval)
	[[nodiscard]] CMemory FindPattern(const CMemory pPattern, const std::string_view szMask, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const;
	[[nodiscard]] CMemory FindPattern(const std::string_view svPattern, const CMemory pStartAddress = nullptr, const Section_t* pModuleSection = nullptr) const
	{
		const auto maskedBytes = PatternToMaskedBytes(svPattern);

		return FindPattern(maskedBytes.m_aBytes.data(), maskedBytes.m_aMask.data(), pStartAddress, pModuleSection);
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
