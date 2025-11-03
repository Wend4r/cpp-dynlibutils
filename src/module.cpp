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
#include <immintrin.h>
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

namespace {
#ifdef __SSE2__
CMemory FindPatternSSE2(uint8_t* pData, const uint8_t* pEnd, const uint8_t* pattern, const uint8_t* mask, size_t maskLen)
{
    __m128i patternVec = _mm_setzero_si128();
    __m128i maskVec = _mm_setzero_si128();

    std::memcpy(&patternVec, pattern, maskLen);
    std::memcpy(&maskVec, mask, maskLen);

    const uint8_t* simdEnd = pEnd - 16 + maskLen;

	unsigned int expectedMask;
	if (maskLen >= 16) {
		expectedMask = 0xFFFF;
	} else {
		expectedMask = (1U << maskLen) - 1;
	}

    for (; pData <= simdEnd; ++pData) {
        __m128i dataVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(pData));
        __m128i maskedData = _mm_and_si128(dataVec, maskVec);
        __m128i maskedPattern = _mm_and_si128(patternVec, maskVec);
        __m128i cmp = _mm_cmpeq_epi8(maskedData, maskedPattern);

        unsigned int matchMask = _mm_movemask_epi8(cmp);

        if ((matchMask & expectedMask) == expectedMask) {
            return pData;
        }
    }

    return nullptr;
}
#endif

#ifdef __AVX2__
CMemory FindPatternAVX2(uint8_t* pData, const uint8_t* pEnd, const uint8_t* pattern, const uint8_t* mask, size_t maskLen)
{
    __m256i patternVec = _mm256_setzero_si256();
    __m256i maskVec = _mm256_setzero_si256();

    std::memcpy(&patternVec, pattern, maskLen);
    std::memcpy(&maskVec, mask, maskLen);

    const uint8_t* simdEnd = pEnd - 32 + maskLen;

	unsigned int expectedMask;
	if (maskLen >= 32) {
		expectedMask = 0xFFFFFFFF;
	} else {
		expectedMask = (1U << maskLen) - 1;
	}

    for (; pData <= simdEnd; ++pData) {
        __m256i dataVec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(pData));
        __m256i maskedData = _mm256_and_si256(dataVec, maskVec);
        __m256i maskedPattern = _mm256_and_si256(patternVec, maskVec);
        __m256i cmp = _mm256_cmpeq_epi8(maskedData, maskedPattern);

        unsigned int matchMask = _mm256_movemask_epi8(cmp);

        if ((matchMask & expectedMask) == expectedMask) {
            return pData;
        }
    }

    return nullptr;
}
#endif

CMemory FindPatternScalar(uint8_t* pData, const uint8_t* pEnd, const uint8_t* pattern, const char* mask, size_t maskLen)
{
    for (; pData <= pEnd; ++pData)
    {
        bool found = true;
        for (size_t i = 0; i < maskLen; ++i)
        {
            if (mask[i] == 'x' && pattern[i] != pData[i]) {
                found = false;
                break;
            }
        }
        if (found)
            return pData;
    }
    return nullptr;
}
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

	if (maskLen == 0 || size < maskLen)
		return nullptr;

	uint8_t* pData = reinterpret_cast<uint8_t*>(base);
	uint8_t* pEnd = pData + size - maskLen;

	if (startAddress)
	{
		uint8_t* pStartAddress = startAddress.RCast<uint8_t*>();
		if (pData > pStartAddress || pStartAddress > pEnd)
			return nullptr;
		pData = pStartAddress;
	}

#if defined(__AVX2__) || defined(__SSE2__)
	// Preprocess: create filtered pattern (replace wildcards with 0) and mask
	std::vector<uint8_t> filteredPattern(maskLen);
	std::vector<uint8_t> binaryMask(maskLen);

	for (size_t i = 0; i < maskLen; ++i)
	{
		if (mask[i] == 'x')
		{
			filteredPattern[i] = pPattern[i];
			binaryMask[i] = 0xFF;
		}
		else
		{
			filteredPattern[i] = 0;
			binaryMask[i] = 0;
		}
	}
#endif

#if defined(__AVX2__)
	// AVX2 version (32 bytes at a time)
	if (maskLen <= 32)
	{
		return FindPatternAVX2(pData, pEnd, filteredPattern.data(), binaryMask.data(), maskLen);
	}
#endif

#if defined(__SSE2__)
	// SSE2 version (16 bytes at a time)
	if (maskLen <= 16)
	{
		return FindPatternSSE2(pData, pEnd, filteredPattern.data(), binaryMask.data(), maskLen);
	}
#endif

	// Fallback for long patterns or no SIMD support
	return FindPatternScalar(pData, pEnd, pPattern, mask.data(), maskLen);
}

CMemory CModule::FindPattern(std::string_view pattern, CMemory startAddress, Section* moduleSection) const
{
	const std::pair patternInfo = PatternToMaskedBytes(pattern);
	return FindPattern(CMemory(patternInfo.first.data()), patternInfo.second, startAddress, moduleSection);
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
