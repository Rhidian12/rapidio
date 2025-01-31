#pragma once

#ifdef _WIN32
#	include "Win32Handle.hpp"

#	include <Windows.h>
#endif // _WIN32

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace rapidio
{
	enum class FileAccessMode : uint32_t
	{
		#ifdef _WIN32
		ReadOnly = GENERIC_READ,
		ReadWrite = ReadOnly | GENERIC_WRITE
		#endif // _WIN32
	};

	enum class FileOpenMode : uint32_t
	{
		#ifdef _WIN32
		CreateNew = CREATE_NEW,
		CreateAlways = CREATE_ALWAYS,
		OpenExisting = OPEN_EXISTING,
		TruncateExisting = TRUNCATE_EXISTING
		#endif // _WIN32
	};

	class FileView final
	{
	public:
		static std::optional<FileView> CreateViewFromExistingFile(const std::filesystem::path& filepath, const FileAccessMode accessMode,
			const FileOpenMode openMode, const size_t size = 0);
		static std::optional<FileView> CreateViewForNewFile(const std::filesystem::path& filepath, size_t expectedFileSize);

		bool Seek(size_t position);

		std::string Read(size_t bytesToRead, bool autoGrowFileMapping = true);

		bool Write(const std::string& data, size_t offset = 0, bool autoGrowFile = true, bool autoGrowFileMapping = true);

	private:
		FileView(const std::string& filepath, const FileAccessMode accessMode);

		bool OpenFile(const FileAccessMode accessMode, const FileOpenMode opemMode);
		bool GetFilesize();
		void CreateFileMappingHandle(size_t size);
		bool CreateNewFileMappingHandle(size_t size);
		void CreateMapViewOfFile(size_t size);
		bool ReallocateFileMapping(size_t newSize);
		bool ReallocateMappedViewOfFile(size_t newSize);

		std::string m_filepath;
		size_t m_filesize = 0;
		size_t m_filepointer = 0;
		FileAccessMode m_accessMode;
		
		#ifdef _WIN32
		Win32Handle m_fileHandle;
		Win32Handle m_fileMappingHandle;
		Win32Handle m_mappedViewHandle;
		size_t m_fileMappingSize = 0;
		#endif // _WIN32
	};
} // namespace rapidio

#ifdef _WIN32
#	include "rapidioWin32.hpp"
#endif // _WIN32