//
// DynLibUtils
// Copyright (C) 2023-2025 Vladimir Ezhikov (Wend4r), Borys Komashchenko (Phoenix), Nikita Ushakov (qubka)
// Licensed under the MIT license. See LICENSE file in the project root for details.
//

#ifndef DYNLIBUTILS_MODULE_HPP
#define DYNLIBUTILS_MODULE_HPP

#pragma once

#include "memaddr.hpp"
#include "loadflag.hpp"

#include <string>
#include <vector>
#include <filesystem>

namespace DynLibUtils {
	/**
	 * @class CModule
	 * @brief Represents an assembly (module) within a process.
	 */
	class CModule {
	public:
		/**
		 * @struct Section
		 * @brief Represents a section of the assembly.
		 */
		struct Section {
			/**
			 * @brief Default constructor initializing size to 0.
			 */
			Section() : size{0} {}

			/**
			 * @brief Parameterized constructor.
			 * @param sectionName The name of the section.
			 * @param sectionBase The base address of the section.
			 * @param sectionSize The size of the section.
			 */
			Section(std::string_view sectionName, uintptr_t sectionBase, size_t sectionSize)
				: name(sectionName), base{sectionBase}, size{sectionSize}
			{}

			/**
			 * @brief Checks if the section is valid.
			 * @return True if the section is valid, false otherwise.
			 */
			operator bool() const noexcept { return base; }

			std::string name{}; //!< The name of the section.
			CMemory base;       //!< The base address of the section.
			size_t size;        //!< The size of the section.
		};

		/**
		 * @struct Handle
		 * @brief Represents a system handle.
		 */
		struct Handle {
			/**
			 * @brief Constructor to initialize the handle.
			 * @param systemHandle The handle value to initialize with.
			 */
			Handle(void* systemHandle) : handle{systemHandle} {}

			/**
			 * @brief Checks if the handle is valid.
			 * @return True if the handle is valid, false otherwise.
			 */
			operator bool() const noexcept { return handle; }

			/**
			 * @brief Converts the handle to a void pointer.
			 * @return The internal handle as a void pointer.
			 */
			operator void*() const noexcept { return handle; }

			void* handle{}; ///< The system handle.
		};

		/**
		 * @brief Default constructor initializing handle to nullptr.
		 */
		CModule() : m_handle{nullptr} {}

		/**
		 * @brief Destructor.
		 */
		~CModule();

		// Delete copy constructor and copy assignment operator.
		CModule(const CModule&) = delete;
		CModule& operator=(const CModule&) = delete;

		// Delete move constructor and move assignment operator.
		CModule(CModule&& rhs) noexcept;
		CModule& operator=(CModule&& rhs) noexcept;

		using SearchDirs = std::vector<std::filesystem::path>;
		constexpr static LoadFlag kDefault = LoadFlag::Lazy | LoadFlag::Noload | LoadFlag::DontResolveDllReferences;

		/**
		 * @brief Constructs an CModule object with the specified module name, flags, and sections.
		 * @param moduleName The name of the module.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(std::string_view moduleName, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true);

		/**
		 * @brief Constructs an CModule object with a char pointer as module name.
		 * @param moduleName The name of the module as a char pointer.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(const char* moduleName, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true)
			: CModule(std::string_view(moduleName), flags, additionalSearchDirectories, sections) {}

		/**
		 * @brief Constructs an CModule object with a string as module name.
		 * @param moduleName The name of the module as a string.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(const std::string& moduleName, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true)
			: CModule(std::string_view(moduleName), flags, additionalSearchDirectories, sections) {}

		/**
		 * @brief Constructs an CModule object with a filesystem path as module path.
		 * @param modulePath The filesystem path of the module.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(const std::filesystem::path& modulePath, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true);

		/**
		 * @brief Constructs an CModule object with a memory address.
		 * @param moduleMemory The memory address of the module.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(CMemory moduleMemory, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true);

		/**
		 * @brief Constructs an CModule object with a memory address.
		 * @param moduleHandle The system handle of the module.
		 * @param flags Optional flags for module initialization.
		 * @param additionalSearchDirectories Optional additional search directories.
		 * @param sections Optional flag indicating if sections should be initialized.
		 */
		explicit CModule(Handle moduleHandle, LoadFlag flags = kDefault, const SearchDirs& additionalSearchDirectories = {}, bool sections = true);

		/**
		 * @brief Converts a string pattern with wildcards to an array of bytes and mask.
		 * @param input The input pattern string.
		 * @return A pair containing the byte array and the mask string.
		 */
		static std::pair<std::vector<uint8_t>, std::string> PatternToMaskedBytes(std::string_view input);

		/**
		 * @brief Finds an array of bytes in process memory using SIMD instructions.
		 * @param pattern The byte pattern to search for.
		 * @param mask The mask corresponding to the byte pattern.
		 * @param startAddress The start address for the search.
		 * @param moduleSection The module section to search within.
		 * @return The memory address where the pattern is found, or nullptr if not found.
		 */
		CMemory FindPattern(CMemory pattern, std::string_view mask, CMemory startAddress = nullptr, const Section* moduleSection = nullptr) const;

		/**
		 * @brief Finds a string pattern in process memory using SIMD instructions.
		 * @param pattern The string pattern to search for.
		 * @param startAddress The start address for the search.
		 * @param moduleSection The module section to search within.
		 * @return The memory address where the pattern is found, or nullptr if not found.
		 */
		CMemory FindPattern(std::string_view pattern, CMemory startAddress = nullptr, Section* moduleSection = nullptr) const;

		template<std::size_t MaxSize>
		struct Pattern {
			std::array<uint8_t, MaxSize> bytes{};
			std::array<char, MaxSize> mask{};
			std::size_t size{};
		};

		// constexpr hex parser
		constexpr uint8_t HexCharToByte(char c) {
			return ('0' <= c && c <= '9') ? c - '0' :
				('a' <= c && c <= 'f') ? 10 + (c - 'a') :
				('A' <= c && c <= 'F') ? 10 + (c - 'A') : 0xFF;
		}

		template<std::size_t N, std::size_t MaxSize>
		constexpr Pattern<MaxSize> PatternToMaskedBytes(const char (&input)[N]) {
			Pattern<MaxSize> result{};
			std::size_t outIndex = 0;

			for (std::size_t i = 0; i < N-1; ++i) { // skip null
				if (outIndex >= MaxSize) break;

				if (input[i] == '?') {
					++i;
					if (input[i] == '?') ++i; // skip double wildcard
					result.bytes[outIndex] = 0;
					result.mask[outIndex++] = '?';
				} else {
					uint8_t hi = HexCharToByte(input[i++]);
					uint8_t lo = HexCharToByte(input[i]);
					result.bytes[outIndex] = (hi << 4) | lo;
					result.mask[outIndex++] = 'x';
				}
			}

			result.size = outIndex;
			return result;
		}

		/**
		 * @brief Finds a string pattern in process memory using SIMD instructions.
		 * @param pattern The string pattern to search for.
		 * @param startAddress The start address for the search.
		 * @param moduleSection The module section to search within.
		 * @return The memory address where the pattern is found, or nullptr if not found.
		 */
		template<std::size_t N, std::size_t MaxSize>
		CMemory FindPattern(const char (&pattern)[N], CMemory startAddress = nullptr, Section* moduleSection = nullptr) const {
			constexpr auto maskedPattern = PatternToMaskedBytes<N, MaxSize>(pattern);
			return FindPattern(
				CMemory(maskedPattern.bytes.data()),
				std::string_view(maskedPattern.mask.data(), maskedPattern.size),
				startAddress,
				moduleSection
			);
		}

		/**
		 * @brief Gets an address of a virtual method table by RTTI type descriptor name.
		 * @param tableName The name of the virtual table.
		 * @param decorated Indicates whether the name is decorated.
		 * @return The memory address of the virtual table, or nullptr if not found.
		 */
		CMemory GetVirtualTableByName(std::string_view tableName, bool decorated = false) const;

		/**
		 * @brief Gets an address of a function by its name.
		 * @param functionName The name of the function.
		 * @return The memory address of the function, or nullptr if not found.
		 */
		CMemory GetFunctionByName(std::string_view functionName) const noexcept;

		/**
		 * @brief Gets a module section by name.
		 * @param sectionName The name of the section (e.g., ".rdata", ".text").
		 * @return The Section object representing the module section.
		 */
		Section GetSectionByName(std::string_view sectionName) const noexcept;

		/**
		 * @brief Returns the module handle.
		 * @return The module handle.
		 */
		void* GetHandle() const noexcept;

		/**
		 * @brief Returns the module base address.
		 * @return The base address of the module.
		 */
		CMemory GetBase() const noexcept;

		/**
		 * @brief Returns the module path.
		 * @return The path of the module.
		 */
		const std::filesystem::path& GetPath() const noexcept;

		/**
		 * @brief Returns the module error.
		 * @return The error string of the module.
		 */
		const std::string& GetError() const noexcept;

		/**
		 * @brief Checks if the assembly is valid.
		 * @return True if the assembly is valid, false otherwise.
		 */
		bool IsValid() const noexcept { return m_handle != nullptr; }

		/**
		 * @brief Conversion operator to check if the assembly is valid.
		 * @return True if the assembly is valid, false otherwise.
		 */
		explicit operator bool() const noexcept { return m_handle != nullptr; }

		/**
		 * @brief Equality operator.
		 * @param assembly The other CModule object to compare with.
		 * @return True if both CModule objects are equal, false otherwise.
		 */
		bool operator==(const CModule& assembly) const noexcept { return m_handle == assembly.m_handle; }

	private:
		/**
		 * @brief Initializes module descriptors.
		 * @param modulePath The path of the module.
		 * @param flags Flags for module initialization.
		 * @param additionalSearchDirectories Additional search directories.
		 * @param sections Flag indicating if sections should be initialized.
		 * @return True if initialization was successful, false otherwise.
		 */
		bool Init(std::filesystem::path modulePath, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections);

		/**
		 * @brief Initializes the assembly from a module name.
		 * @param moduleName The name of the module.
		 * @param flags Flags for module initialization.
		 * @param additionalSearchDirectories Additional search directories.
		 * @param sections Flag indicating if sections should be initialized.
		 * @param extension Indicates if an extension is used.
		 * @return True if initialization was successful, false otherwise.
		 */
		bool InitFromName(std::string_view moduleName, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections, bool extension = false);

		/**
		 * @brief Initializes the assembly from memory.
		 * @param moduleMemory The memory address of the module.
		 * @param flags Flags for module initialization.
		 * @param additionalSearchDirectories Additional search directories.
		 * @param sections Flag indicating if sections should be initialized.
		 * @return True if initialization was successful, false otherwise.
		 */
		bool InitFromMemory(CMemory moduleMemory, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections);

		/**
		 * @brief Initializes the assembly from handle.
		 * @param moduleHandle The system handle of the module.
		 * @param flags Flags for module initialization.
		 * @param additionalSearchDirectories Additional search directories.
		 * @param sections Flag indicating if sections should be initialized.
		 * @return True if initialization was successful, false otherwise.
		 */
		bool InitFromHandle(Handle moduleHandle, LoadFlag flags, const SearchDirs& additionalSearchDirectories, bool sections);

		/**
		 * @brief Loads the sections of the module into memory.
		 *
		 * This function is responsible for loading the individual sections of
		 * a module into memory. Sections typically represent different parts
		 * of a module, such as executable code, read-only data, or other data
		 * segments. The function ensures that these sections are properly loaded
		 * and ready for execution or inspection.
		 *
		 * @return True if the sections were successfully loaded, false otherwise.
		 */
		bool LoadSections();

	private:
		void* m_handle;                //!< The handle to the module.
		std::filesystem::path m_path;  //!< The path of the module.
		std::string m_error;           //!< The error of the module.
		Section m_executableCode;      //!< The section representing executable code.
		std::vector<Section> m_sections; //!< A vector of sections in the module.
	};

	/**
	 * @brief Translates loading flags to an integer representation.
	 *
	 * @param flags The loading flags to translate.
	 * @return An integer representation of the loading flags.
	 */
	int TranslateLoading(LoadFlag flags) noexcept;

	/**
	 * @brief Translates an integer representation of loading flags to LoadFlag.
	 *
	 * @param flags The integer representation of the loading flags.
	 * @return The corresponding LoadFlag.
	 */
	LoadFlag TranslateLoading(int flags) noexcept;

} // namespace DynLibUtils

#endif // DYNLIBUTILS_MODULE_HPP
