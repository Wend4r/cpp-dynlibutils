//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#include <dynlibutils/module.hpp>
#include <dynlibutils/memaddr.hpp>

#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>

#if !DYNLIBUTILS_ARCH_ARM
#if DYNLIBUTILS_COMPILER_GCC && !DYNLIBUTILS_COMPILER_CLANG && !defined(NDEBUG)
#undef __OPTIMIZE__
#endif // !defined(NDEBUG)
#include <emmintrin.h>
#endif // !DYNLIBUTILS_ARCH_ARM

using namespace DynLibUtils;

CModule::CModule(std::string_view moduleName, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
	: m_handle{nullptr}
{
	InitFromName(moduleName, flags, additionalSearchDirectories, sections);
}

CModule::CModule(CMemory moduleMemory, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
	: m_handle{nullptr}
{
	InitFromMemory(moduleMemory, flags, additionalSearchDirectories, sections);
}

CModule::CModule(Handle moduleHandle, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
	: m_handle{nullptr}
{
	InitFromHandle(moduleHandle, flags, additionalSearchDirectories, sections);
}

CModule::CModule(const std::filesystem::path& modulePath, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections)
	: m_handle{nullptr}
{
	Init(modulePath, flags, additionalSearchDirectories, sections);
}

CModule::CModule(CModule&& other) noexcept
    : m_handle(other.m_handle),
      m_path(std::move(other.m_path)),
      m_error(std::move(other.m_error)),
      m_executableCode(std::move(other.m_executableCode)),
      m_sections(std::move(other.m_sections))
{
    other.m_handle = nullptr;
}

CModule& CModule::operator=(CModule&& other) noexcept
{
	if (this != &other)
	{
		m_handle = other.m_handle;
		m_path = std::move(other.m_path);
		m_error = std::move(other.m_error);
		m_executableCode = std::move(other.m_executableCode);
		m_sections = std::move(other.m_sections);

		other.m_handle = nullptr;
	}
	return *this;
}

std::pair<std::vector<uint8_t>, std::string> CModule::PatternToMaskedBytes(std::string_view input)
{
	std::pair<std::vector<uint8_t>, std::string> ret;
	auto& [bytes, mask] = ret;

	char* pPatternStart = const_cast<char*>(input.data());
	char* pPatternEnd = pPatternStart + input.size();

	bytes.reserve(input.size() / 3 + 1);
	mask.reserve(input.size() / 3 + 1);

	for (char* pCurrentByte = pPatternStart; pCurrentByte < pPatternEnd; ++pCurrentByte) {
		if (*pCurrentByte == '?') {
			++pCurrentByte;
			if (*pCurrentByte == '?') {
				++pCurrentByte;// Skip double wildcard.
			}

			bytes.push_back(0);// Push the byte back as invalid.
			mask += '?';
		} else {
			bytes.push_back(static_cast<uint8_t>(std::strtoul(pCurrentByte, &pCurrentByte, 16)));
			mask += 'x';
		}
	}

	return ret;
}

CMemory CModule::FindPattern(CMemory pattern, std::string_view mask, CMemory startAddress, const Section* moduleSection) const
{
	const uint8_t* pPattern = pattern.RCast<const uint8_t*>();
	const Section& section = moduleSection ? *moduleSection : m_executableCode;
	if (!section)
		return nullptr;

	const uintptr_t base = section.base;
	const size_t size = section.size;

	const size_t maskLen = mask.length();
	const uint8_t* pData = reinterpret_cast<uint8_t*>(base);
	const uint8_t* pEnd = pData + size - maskLen;

	if (startAddress) {
		const uint8_t* pStartAddress = startAddress.RCast<uint8_t*>();
		if (pData > pStartAddress || pStartAddress > pEnd)
			return nullptr;

		pData = pStartAddress;
	}

#if !DYNLIBUTILS_ARCH_ARM
	std::array<int, 64> masks = {};// 64*16 = enough masks for 1024 bytes.
	const uint8_t numMasks = static_cast<uint8_t>(std::ceil(static_cast<float>(maskLen) / 16.f));

	for (uint8_t i = 0; i < numMasks; ++i) {
		for (int8_t j = static_cast<int8_t>(std::min<size_t>(maskLen - i * 16, 16)) - 1; j >= 0; --j) {
			if (mask[static_cast<size_t>(i * 16 + j)] == 'x') {
				masks[i] |= 1 << j;
			}
		}
	}

	const __m128i xmm1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pPattern));
	__m128i xmm2, xmm3, msks;
	for (; pData != pEnd; _mm_prefetch(reinterpret_cast<const char*>(++pData + 64), _MM_HINT_NTA)) {
		xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
		msks = _mm_cmpeq_epi8(xmm1, xmm2);
		if ((_mm_movemask_epi8(msks) & masks[0]) == masks[0]) {
			bool found = true;
			for (uint8_t i = 1; i < numMasks; ++i) {
				xmm2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pData + i * 16)));
				xmm3 = _mm_loadu_si128(reinterpret_cast<const __m128i*>((pPattern + i * 16)));
				msks = _mm_cmpeq_epi8(xmm2, xmm3);
				if ((_mm_movemask_epi8(msks) & masks[i]) != masks[i]) {
					found = false;
					break;
				}
			}

			if (found)
				return pData;
		}
	}
#else
	for (; pData != pEnd; ++pData) {
		bool found = false;

		for (size_t i = 0; i < maskLen; ++i) {
			if (mask[i] == 'x' || pPattern[i] == *(pData + i)) {
				found = true;
			} else {
				found = false;
				break;
			}
		}

		if (found)
			return pData;
	}
#endif // !DYNLIBUTILS_ARCH_ARM
	return nullptr;
}

CMemory CModule::FindPattern(std::string_view pattern, CMemory startAddress, Section* moduleSection) const
{
	const std::pair patternInfo = PatternToMaskedBytes(pattern);
	return FindPattern(patternInfo.first.data(), patternInfo.second, startAddress, moduleSection);
}

CModule::Section CModule::GetSectionByName(std::string_view sectionName) const noexcept
{
	auto it = std::find_if(m_sections.begin(), m_sections.end(), [&sectionName](const auto& section) {
		return section.name == sectionName;
	});
	if (it != m_sections.end())
		return *it;
	return {};
}

void* CModule::GetHandle() const noexcept
{
	return m_handle;
}

const std::filesystem::path& CModule::GetPath() const noexcept
{
	return m_path;
}

const std::string& CModule::GetError() const noexcept
{
	return m_error;
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
