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

#include <cassert>
#include <vector>
#include <string>
#include <string_view>
#include <utility>

namespace DynLibUtils {

using MaskedBytes_t = std::pair<std::vector<std::uint8_t>, std::string>;

class CModule
{
public:
	struct ModuleSections_t
	{
		ModuleSections_t() : m_nSectionSize(0) {}
		ModuleSections_t(const ModuleSections_t&) = default;
		ModuleSections_t& operator= (const ModuleSections_t&) = default;
		ModuleSections_t(ModuleSections_t&& other) noexcept : m_svSectionName(std::move(other.m_svSectionName)), m_pSectionBase(std::move(other.m_pSectionBase)), m_nSectionSize(std::exchange(other.m_nSectionSize, 0)) {}
		ModuleSections_t(const std::string_view svSectionName, uintptr_t pSectionBase, size_t nSectionSize) : m_svSectionName(svSectionName), m_pSectionBase(pSectionBase), m_nSectionSize(nSectionSize) {}

		[[nodiscard]] inline bool IsSectionValid() const noexcept
		{
			return m_pSectionBase;
		}

		std::string m_svSectionName; // Name of section.
		CMemory m_pSectionBase;      // Start address of section.
		size_t m_nSectionSize;       // Size of section.
	};

	CModule() : m_pHandle(nullptr) {}
	~CModule();

	CModule (const CModule&) = delete;
	CModule& operator= (const CModule&) = delete;
	CModule(CModule&& other) noexcept : m_ExecutableCode(std::move(other.m_ExecutableCode)), m_sPath(std::move(other.m_sPath)), m_pHandle(std::exchange(other.m_pHandle, nullptr)), m_vModuleSections(std::move(other.m_vModuleSections)) {}
	explicit CModule(const std::string_view svModuleName);
	explicit CModule(const char* pszModuleName) : CModule(std::string_view(pszModuleName)) {}
	explicit CModule(const std::string& sModuleName) : CModule(std::string_view(sModuleName)) {}
	CModule(const CMemory pModuleMemory);

	bool LoadFromPath(const std::string_view svModelePath, int flags);

	bool InitFromName(const std::string_view svModuleName, bool bExtension = false);
	bool InitFromMemory(const CMemory pModuleMemory);

	[[nodiscard]] static MaskedBytes_t PatternToMaskedBytes(const std::string_view svInput);
	[[nodiscard]] CMemory FindPattern(const CMemory pPattern, const std::string_view szMask, const CMemory pStartAddress = nullptr, const ModuleSections_t* pModuleSection = nullptr) const;
	[[nodiscard]] CMemory FindPattern(const std::string_view svPattern, const CMemory pStartAddress = nullptr, const ModuleSections_t* pModuleSection = nullptr) const;

	[[nodiscard]] CMemory GetVirtualTableByName(const std::string_view svTableName, bool bDecorated = false) const;
	[[nodiscard]] CMemory GetFunctionByName(const std::string_view svFunctionName) const noexcept;

	[[nodiscard]] ModuleSections_t GetSectionByName(const std::string_view svSectionName) const;
	[[nodiscard]] void* GetHandle() const noexcept;
	[[nodiscard]] CMemory GetBase() const noexcept;
	[[nodiscard]] std::string_view GetPath() const;
	[[nodiscard]] std::string_view GetName() const;
	[[nodiscard]] std::string_view GetLastError() const;

private:
	void SaveLastError();

private:
	ModuleSections_t m_ExecutableCode;
	std::string m_sPath;
	std::string m_sLastError;
	void* m_pHandle;
	std::vector<ModuleSections_t> m_vModuleSections;
};

#ifdef __cpp_consteval
inline consteval MaskedBytes_t PatternToMaskedBytes(std::string_view svInput)
{
	std::vector<std::uint8_t> vecBytes;
	std::string sMask;

	auto funcCharIsHEX = [](char c)
	{
		return ('0' <= c && c <= '9') || 
		       ('a' <= c && c <= 'f') || 
		       ('A' <= c && c <= 'F');
	};

	auto funcHEXCharToValue = [](char c) -> std::uint8_t
	{
		if ('0' <= c && c <= '9') return c - '0';
		if ('a' <= c && c <= 'f') return 10 + (c - 'a');
		if ('A' <= c && c <= 'F') return 10 + (c - 'A');

		return 0;
	};

	for (std::size_t i = 0; i < svInput.size();)
	{
		if (svInput[i] == '?')
		{
			// Wildcard?
			if (i + 1 < svInput.size() && svInput[i + 1] == '?')
				++i;
			++i;

			vecBytes.push_back(0x00);
			sMask.push_back('?');
		}
		else if (funcCharIsHEX(svInput[i]) && i + 1 < svInput.size() && funcCharIsHEX(svInput[i + 1]))
		{
			std::uint8_t value = (funcHEXCharToValue(svInput[i]) << 4) | funcHEXCharToValue(svInput[i + 1]);

			vecBytes.push_back(value);
			sMask.push_back('x');

			i += 2;
		}
		else if (svInput[i] == ' ')
		{
			++i;
		}
		else
		{
			assert(false && R"(Passing invalid characters. Allowed ones: <space>, "0-9", "a-f", "A-F" or "?")");

			// Skip invalid characters silently.
			++i;
		}
	}

	return MaskedBytes_t{vecBytes, sMask};
}
#endif // defined(__cpp_consteval)

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_HPP
