#ifndef MODULE_H
#define MODULE_H
#ifdef _WIN32
#pragma once
#endif

#include <string>
#include <vector>
#include "memaddr.h"

class CModule
{
public:
	struct ModuleSections_t
	{
		ModuleSections_t() = default;
		ModuleSections_t(const std::string_view svSectionName, uintptr_t pSectionBase, size_t nSectionSize) :
			m_svSectionName(svSectionName), m_pSectionBase(pSectionBase), m_nSectionSize(nSectionSize) {}

		bool IsSectionValid() const
		{
			return m_nSectionSize != 0;
		}

		std::string    m_svSectionName;           // Name of section.
		uintptr_t m_pSectionBase{};          // Start address of section.
		size_t    m_nSectionSize{};          // Size of section.
	};

	CModule(const std::string_view szModuleName);
	CModule(const CMemory pModuleMemory);

	void InitFromName(const std::string_view szModuleName);
	void InitFromMemory(const CMemory pModuleMemory);

	static std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(const std::string_view svInput);
	CMemory FindPatternSIMD(const void* pPattern, const std::string_view szMask, const CMemory startAddress = nullptr, const ModuleSections_t* moduleSection = nullptr) const;
	CMemory FindPatternSIMD(const std::string_view svPattern, const CMemory startAddress = nullptr, const ModuleSections_t* moduleSection = nullptr) const;

	CMemory FindVirtualTableByName(const std::string_view svTableName, bool bFullName = false) const;

	ModuleSections_t GetSectionByName(const std::string_view svSectionName) const;
	uintptr_t        GetModuleBase() const;
	std::string_view GetModuleName() const;

private:
	void Init();
	void LoadSections();

	ModuleSections_t         m_ExecutableCode;
	std::string              m_svModuleName;
	uintptr_t                m_pModuleBase{};
	std::vector<ModuleSections_t> m_vModuleSections;
};

#endif // MODULE_H
