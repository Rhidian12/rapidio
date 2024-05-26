#pragma once

#define RAPIDIO_USE_EXCEPTIONS

#include "Win32Handle.hpp"

#include <filesystem>
#include <string>

#ifdef _WIN32
#	include <Windows.h>
#endif // _WIN32

namespace rapidio
{
	enum class FileAccessMode
	{
		#ifdef _WIN32
		READ = GENERIC_READ,
		WRITE = GENERIC_WRITE,
		READWRITE = READ | WRITE
		#endif // _WIN32
	};

	enum class FileOpenMode
	{
		#ifdef _WIN32
		CREATENEW = CREATE_NEW,
		CREATEALWAYS = CREATE_ALWAYS,
		OPENEXISTING = OPEN_EXISTING,
		OPENALWAYS = OPEN_ALWAYS,
		TRUNCATEEXISTING = TRUNCATE_EXISTING
		#endif // _WIN32
	};

	class FileView final
	{
	public:
		FileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode);

	private:
	};

	FileView::FileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode)
	{
		#ifdef _WIN32
		constexpr DWORD ExclusiveAccess = 0;
		CreateFileA(InFilepath.c_str(), static_cast<DWORD>(AccessMode), ExclusiveAccess, nullptr, static_cast<DWORD>(OpenMode), FILE_ATTRIBUTE_NORMAL, nullptr);
		#endif // _WIN32
	}
} // namespace rapidio