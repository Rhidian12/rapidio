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
		static std::optional<FileView> CreateFileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode,
			const std::optional<size_t>& BytesToRead);

		std::string_view GetFileContents();

	private:
		FileView(const std::string& InFilepath);
		bool OpenFile(const FileAccessMode AccessMode, const FileOpenMode OpenMode);
		bool GetFilesize();
		bool CreateFileMappingHandle(const FileAccessMode AccessMode, const std::optional<size_t>& BytesToRead);
		bool CreateMapViewOfFile(const FileAccessMode AccessMode, const std::optional<size_t>& BytesToRead);
		bool GetSystemAllocationGranularity();

		std::string Filepath;
		int64_t Filesize;
		
		#ifdef _WIN32
		Win32Handle FileHandle;
		Win32Handle FileMappingHandle;
		Win32Handle MappedViewHandle;
		DWORD AllocationGranularity;
		#endif // _WIN32
	};

	std::optional<FileView> FileView::CreateFileView(const std::string& InFilepath, const FileAccessMode AccessMode, const FileOpenMode OpenMode,
		const std::optional<size_t>& BytesToRead)
	{
		FileView View(InFilepath);

		if (!View.OpenFile(AccessMode, OpenMode))
		{
			return std::nullopt;
		}

		if (!View.GetFilesize())
		{
			return std::nullopt;
		}

		if (!View.CreateFileMappingHandle(AccessMode, BytesToRead))
		{
			return std::nullopt;
		}

		if (!View.GetSystemAllocationGranularity())
		{
			return std::nullopt;
		}

		return View;
	}

	std::string_view FileView::GetFileContents()
	{
		return std::string_view();
	}

	FileView::FileView(const std::string& InFilepath)
		: Filepath(InFilepath)
	{}

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

		FileHandle = CALL_WIN32_RV_IGNORE_ERROR
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

		return FileHandle != nullptr;
		#endif // _WIN32
	}

	bool FileView::GetFilesize()
	{
		#ifdef _WIN32
		LARGE_INTEGER InFilesize;
		const BOOL Ret{ CALL_WIN32_RV(GetFileSizeEx(FileHandle, &InFilesize)) };
		Filesize = InFilesize.QuadPart;
		return Ret != 0;
		#endif // _WIN32
	}

	bool FileView::CreateFileMappingHandle(const FileAccessMode AccessMode, const std::optional<size_t>& BytesToRead)
	{
		#ifdef _WIN32
		constexpr DWORD ReadOnlyAccess = PAGE_READONLY;
		constexpr DWORD ReadWriteAccess = PAGE_READWRITE;
		const DWORD Size{ static_cast<DWORD>(BytesToRead.value_or(0)) };

		FileMappingHandle = CALL_WIN32_RV
		(
			CreateFileMappingA
			(
				FileHandle,
				nullptr,
				AccessMode == FileAccessMode::ReadOnly ? ReadOnlyAccess : ReadWriteAccess,
				HIWORD(Size), // If 'Size' is 0, we read the entire file
				LOWORD(Size),
				"") // [TODO]: assign a name here to ensure we don't re-create file mappings
		);

		return FileMappingHandle != nullptr;
		#endif // _WIN32
	}

	bool FileView::CreateMapViewOfFile(const FileAccessMode AccessMode, const std::optional<size_t>&)
	{
		#ifdef _WIN32
		constexpr DWORD ReadOnlyAccess = FILE_MAP_READ;
		constexpr DWORD ReadWriteAccess = FILE_MAP_ALL_ACCESS;
		constexpr DWORD Size{};
		MappedViewHandle = CALL_WIN32_RV
		(
			MapViewOfFile
			(
				FileMappingHandle,
				AccessMode == FileAccessMode::ReadOnly ? ReadOnlyAccess : ReadWriteAccess,
				484341,
				484341,
				Size // 0 means it will create a view of the entire mapped file
			)
		);

		return MappedViewHandle != nullptr;
		#endif // _WIN32
	}

	bool FileView::GetSystemAllocationGranularity()
	{
		#ifdef _WIN32
		SYSTEM_INFO SystemInfo;
		CALL_WIN32(GetNativeSystemInfo(&SystemInfo));
		AllocationGranularity = SystemInfo.dwAllocationGranularity;
		return true;
		#endif // _WIN32
	}
} // namespace rapidio