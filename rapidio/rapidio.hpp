#pragma once

#define RAPIDIO_USE_EXCEPTIONS

#include "PathUtils.hpp"

#ifdef _WIN32
#	include "Win32Call.hpp"
#	include "Win32Handle.hpp"

#	include <Windows.h>
#endif // _WIN32

#ifdef RAPIDIO_USE_EXCEPTIONS
#	include <exception>
#endif // RAPIDIO_USE_EXCEPTIONS

#include <filesystem>
#include <string>

namespace rapidio
{
	enum class FileAccessMode
	{
		#ifdef _WIN32
		Read = GENERIC_READ,
		Write = GENERIC_WRITE,
		ReadWrite = Read | Write
		#endif // _WIN32
	};

	enum class FileOpenMode
	{
		#ifdef _WIN32
		CreateNew = CREATE_NEW,
		CreateAlways = CREATE_ALWAYS,
		OpenExisting = OPEN_EXISTING,
		OpenAlways = OPEN_ALWAYS,
		TruncateExisting = TRUNCATE_EXISTING
		#endif // _WIN32
	};

	#ifdef RAPIDIO_USE_EXCEPTIONS
	class FileExistError final : public std::exception
	{
	public:
		FileExistError(const std::string& Message) : std::exception(Message.c_str()) {}
	};
	#endif // RAPIDIO_USE_EXCEPTIONS

	class FileView final
	{
	public:
		FileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode);

	private:
		bool OpenFile(const FileAccessMode AccessMode, const FileOpenMode OpenMode);

		std::string Filepath;
		
		#ifdef _WIN32
		Win32Handle Handle;
		#endif // _WIN32
	};

	FileView::FileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode)
		: Filepath(InFilepath)
	{
		OpenFile(AccessMode, OpenMode);
	}

	bool FileView::OpenFile(const FileAccessMode AccessMode, const FileOpenMode OpenMode)
	{
		const bool bDoesFileExist = PathUtils::DoesFileExist(Filepath);

		#ifdef _WIN32
		constexpr DWORD ExclusiveAccess = 0;

		DWORD ErrorToIgnore{};

		switch (OpenMode)
		{
			case FileOpenMode::CreateNew:
				if (bDoesFileExist)
				{
				#ifdef RAPIDIO_USE_EXCEPTIONS
					throw FileExistError("FileView > OpenMode::CreateNew > File " + Filepath + " already exists");
				#else
					std::cerr << "FileView > OpenMode::CreateNew > File " << Filepath << " already exists\n";
					return false;
				#endif // RAPIDIO_USE_EXCEPTIONS
				}
				ErrorToIgnore = ERROR_FILE_EXISTS;
				break;
			case FileOpenMode::OpenExisting:
				if (!bDoesFileExist)
				{
					#ifdef RAPIDIO_USE_EXCEPTIONS
					throw FileExistError("FileView > OpenMode::OpenExisting > File " + Filepath + " does not exist");
					#else
					std::cerr << "FileView > OpenMode::OpenExisting > File " << Filepath << " does not exist\n";
					return false;
					#endif // RAPIDIO_USE_EXCEPTIONS
				}
				ErrorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::TruncateExisting:
				if (!bDoesFileExist)
				{
					#ifdef RAPIDIO_USE_EXCEPTIONS
					throw FileExistError("FileView > OpenMode::TruncateExisting > File " + Filepath + " does not exist");
					#else
					std::cerr << "FileView > OpenMode::TruncateExisting > File " << Filepath << " does not exist\n";
					return false;
					#endif // RAPIDIO_USE_EXCEPTIONS
				}
				ErrorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::CreateAlways:
			case FileOpenMode::OpenAlways:
				ErrorToIgnore = ERROR_ALREADY_EXISTS;
				break;
			default:
				break;
		}

		Handle = CALL_WIN32_RV_IGNORE_ERROR
		(
			CreateFileA
			(
				Filepath.c_str(),
				static_cast<DWORD>(AccessMode),
				ExclusiveAccess,
				nullptr,
				static_cast<DWORD>(OpenMode),
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			),
			ErrorToIgnore
		);

		return true;
		#endif // _WIN32
	}
} // namespace rapidio