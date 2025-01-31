#pragma once

#include "PathUtils.hpp"

#ifdef _WIN32
#	undef max

#	include "Win32Call.hpp"
#	include "Win32Handle.hpp"

#	include <Windows.h>
#	include <winternl.h>
#endif // _WIN32

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <cassert>

namespace rapidio
{
	namespace detail
	{
		struct DestructionCallback
		{
			std::function<void()> Callback;

			DestructionCallback(std::function<void()> Func) : Callback(std::move(Func)) {}
			~DestructionCallback() { Callback(); }
		};
	} // namespace detail

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

	class FileView final
	{
	public:
		static std::optional<FileView> CreateViewFromExistingFile(const std::filesystem::path& InFilepath, const FileAccessMode InAccessMode, const FileOpenMode OpenMode, const size_t Size = 0);
		static std::optional<FileView> CreateViewForNewFile(const std::filesystem::path& InFilepath, size_t ExpectedFileSize);

		bool Seek(size_t Position);

		std::string Read(size_t BytesToRead, bool AutoGrowFileMapping = true);

		bool Write(const std::string& Data, size_t Offset = 0, bool AutoGrowFile = true, bool AutoGrowFileMapping = true);

	private:
		FileView(const std::string& InFilepath, const FileAccessMode InAccessMode);

		bool OpenFile(const FileAccessMode InAccessMode, const FileOpenMode OpenMode);
		bool GetFilesize();
		void* CreateFileMappingHandle(void* Handle, size_t Size);
		bool CreateNewFileMappingHandle(size_t Size);
		void* CreateMapViewOfFile(void* Handle, size_t Size);
		bool GetSystemAllocationGranularity();
		bool ReallocateFileMapping(size_t NewSize);
		bool ReallocateMappedViewOfFile(size_t NewSize);

		std::string Filepath;
		size_t Filesize = 0;
		size_t Filepointer = 0;
		FileAccessMode AccessMode;
		
		#ifdef _WIN32
		Win32Handle FileHandle;
		Win32Handle FileMappingHandle;
		Win32Handle MappedViewHandle;
		size_t FileMappingSize = 0;
		DWORD AllocationGranularity = 0;
		#endif // _WIN32
	};

	std::optional<FileView> FileView::CreateViewFromExistingFile(const std::filesystem::path& InFilepath, const FileAccessMode InAccessMode, const FileOpenMode OpenMode, size_t Size)
	{
		if (!PathUtils::DoesFileExist(InFilepath))
		{
			std::cerr << "FileView::CreateViewFromExistingFile > File must already exist!\n";
			return std::nullopt;
		}

		// Don't allow any creation here
		switch (OpenMode)
		{
			case FileOpenMode::CreateNew:
			case FileOpenMode::CreateAlways:
			case FileOpenMode::OpenAlways:
				std::cerr << "FileView::CreateViewFromExistingFile > Cannot create a file\n";
				return std::nullopt;
		}

		FileView View(InFilepath.string(), InAccessMode);

		if (!View.OpenFile(InAccessMode, OpenMode))
		{
			return std::nullopt;
		}

		if (!View.GetFilesize())
		{
			return std::nullopt;
		}

		View.FileMappingHandle = View.CreateFileMappingHandle(View.FileHandle, Size);
		if (!View.FileMappingHandle.IsValid())
		{
			HMODULE hNtDll = LoadLibraryW(L"ntdll.dll");
			if (!hNtDll) {
				// Handle error
				return std::nullopt;
			}
			typedef NTSTATUS(WINAPI* RtlGetLastNtStatusFn)();

			// Get the function address
			RtlGetLastNtStatusFn RtlGetLastNtStatus = (RtlGetLastNtStatusFn)GetProcAddress(hNtDll, "RtlGetLastNtStatus");
			if (!RtlGetLastNtStatus) {
				// Handle error
				FreeLibrary(hNtDll);
				return std::nullopt;
			}

			NTSTATUS status = RtlGetLastNtStatus();
			std::cerr << "Error Code: " << status << "\n";

			FreeLibrary(hNtDll);

			return std::nullopt;
		}

		if (!View.GetSystemAllocationGranularity())
		{
			return std::nullopt;
		}

		View.MappedViewHandle = { View.CreateMapViewOfFile(View.FileMappingHandle, 0), [](void* Handle) { return CALL_WIN32_RV(UnmapViewOfFile(Handle)) != 0; } };
		if (!View.MappedViewHandle.IsValid())
		{
			return std::nullopt;
		}

		return View;
	}

	std::optional<FileView> FileView::CreateViewForNewFile(const std::filesystem::path& InFilepath, size_t ExpectedFileSize)
	{
		if (ExpectedFileSize == 0)
		{
			std::cerr << "FileView::CreateViewForNewFile > Size cannot be 0\n";
			return std::nullopt;
		}

		if (PathUtils::DoesFileExist(InFilepath))
		{
			std::cerr << "FileView::CreateViewForNewFile > File cannot already exist!\n";
			return std::nullopt;
		}

		FileView View(InFilepath.string(), FileAccessMode::ReadWrite);

		if (!View.OpenFile(FileAccessMode::ReadWrite, FileOpenMode::CreateNew))
		{
			return std::nullopt;
		}

		if (!View.GetFilesize())
		{
			return std::nullopt;
		}

		View.FileMappingHandle = View.CreateFileMappingHandle(View.FileHandle, ExpectedFileSize);
		if (!View.FileMappingHandle.IsValid())
		{
			return std::nullopt;
		}

		if (!View.GetSystemAllocationGranularity())
		{
			return std::nullopt;
		}

		View.MappedViewHandle = { View.CreateMapViewOfFile(View.FileMappingHandle, ExpectedFileSize), [](void* Handle) { return CALL_WIN32_RV(UnmapViewOfFile(Handle)) != 0; } };
		if (!View.MappedViewHandle.IsValid())
		{
			return std::nullopt;
		}

		return View;
	}

	bool FileView::Seek(size_t Position)
	{
		if (Position >= Filesize)
		{
			std::cerr << "FileView::Seek > Cannot seek to past EOF\n";
			return false;
		}

		if (FileMappingSize > 0 && Position >= FileMappingSize)
		{
			std::cerr << "FileView::Seek > Cannot seek past end of Mapped View\n";
			return false;
		}

		Filepointer = Position;
		return true;
	}

	std::string FileView::Read(size_t BytesToRead, bool AutoGrowFileMapping /* = true */)
	{
		// Are we at EOF?
		if (Filepointer >= Filesize)
		{
			return "";
		}

		// If we're not at EOF, but reading 'BytesToRead' would push us past EOF, adjust 'BytesToRead' until we hit EOF
		if (BytesToRead + Filepointer > Filesize)
		{
			BytesToRead = Filesize - Filepointer;
		}
		
		// Check our mapped view size we created, if not 0
		// If 0, the previous check covers the filesize
		if (FileMappingSize > 0 && BytesToRead + Filepointer > FileMappingSize)
		{
			if (!AutoGrowFileMapping)
			{
				std::cerr << "FileView::Read > Reading " << BytesToRead << "  would read past Mapped View!\n";
				return "";
			}
			else
			{
				// Linearly grow the amount of data we are mapping
				size_t NewSize = (BytesToRead + Filepointer) * 2;
				if (NewSize > Filesize)
				{
					NewSize = Filesize;
				}

				ReallocateFileMapping(NewSize);
			}
		}

		#ifdef _WIN32
		const size_t OldFilepointer = Filepointer;
		Filepointer += BytesToRead;
		return std::string(static_cast<char*>(MappedViewHandle.Get()) + OldFilepointer, BytesToRead);
		#endif // _WIN32
	}

	inline int filter(unsigned int Code, struct _EXCEPTION_POINTERS* ep)
	{
		// 3221225477
		if (Code == EXCEPTION_ACCESS_VIOLATION)
		{
			std::cerr << "Exception access violation!\n";
		}
		std::cerr << "were fucked: " << ep->ExceptionRecord->ExceptionCode << "\n";
		return EXCEPTION_EXECUTE_HANDLER;
	}

	bool FileView::Write(const std::string& Data, size_t Offset /* = 0 */, bool AutoGrowFile /* = true */, bool AutoGrowFileMapping /* = true */)
	{
		if (AccessMode == FileAccessMode::ReadOnly)
		{
			std::cerr << "FileView::Write > Cannot write to read-only mapping\n";
			return false;
		}

		if (const std::pair<bool, bool> TooSmall{ Filesize > 0 && Data.size() + Offset > Filesize, Data.size() + Offset > FileMappingSize }; (TooSmall.first) || TooSmall.second)
		{
			if (TooSmall.first && !AutoGrowFile)
			{
				std::cerr << "FileView::Write > Size of data + offset is bigger than filesize with autogrow disabled!\n";
				return false;
			}

			if (TooSmall.second && !AutoGrowFileMapping)
			{
				std::cerr << "FileView::Write > Size of data + offset is bigger than mapped view of file with autogrow disabled!\n";
				return false;
			}

			size_t NewSize = 0;

			if (TooSmall.first)
			{
				NewSize = Filesize + (Data.size() + Offset - Filesize);
				ReallocateFileMapping(NewSize);
			}
			else /* if (TooSmall.second) */
			{
				NewSize = FileMappingSize + (Data.size() + Offset - FileMappingSize);
				if (NewSize > Filesize && !AutoGrowFile)
				{
					std::cerr << "FileView::Write > Size of data + offset is bigger than filesize with autogrow disabled!\n";
					return false;
				}

				ReallocateFileMapping(NewSize);
			}
		}

		__try
		{
			// std::snprintf(static_cast<char*>(MappedViewHandle.Get()) + Offset, Data.size() + 1, "%s", Data.data());
			std::memcpy(static_cast<char*>(MappedViewHandle.Get()) + Offset, Data.data(), Data.size());
		}
		__except (filter(GetExceptionCode(), GetExceptionInformation()))
		{
 			std::cerr << "were fucked\n";
		}
		return true;
	}

	FileView::FileView(const std::string& InFilepath, const FileAccessMode InAccessMode)
		: Filepath(InFilepath)
		, AccessMode(InAccessMode)
	{}

	bool FileView::OpenFile(const FileAccessMode InAccessMode, const FileOpenMode OpenMode)
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
					std::cerr << "FileView > OpenMode::CreateNew > File " << Filepath << " already exists\n";
					return false;
				}
				if (InAccessMode != FileAccessMode::ReadWrite)
				{
					std::cerr << "FileView > OpenMode::CreateNew > requires ReadWrite AccessMode\n";
					return false;
				}
				ErrorToIgnore = ERROR_FILE_EXISTS;
				break;
			case FileOpenMode::OpenExisting:
				if (!bDoesFileExist)
				{
					std::cerr << "FileView > OpenMode::OpenExisting > File " << Filepath << " does not exist\n";
					return false;
				}
				ErrorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::TruncateExisting:
				if (!bDoesFileExist)
				{
					std::cerr << "FileView > OpenMode::TruncateExisting > File " << Filepath << " does not exist\n";
					return false;
				}
				ErrorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::CreateAlways:
				if (InAccessMode != FileAccessMode::ReadWrite)
				{
					std::cerr << "FileView > OpenMode::CreateAlways requires ReadWrite AccessMode\n";
					return false;
				}

				ErrorToIgnore = ERROR_ALREADY_EXISTS;
				break;
			case FileOpenMode::OpenAlways:
				if (InAccessMode != FileAccessMode::ReadWrite)
				{
					std::cerr << "FileView > OpenMode::OpenAlways requires ReadWrite AccessMode\n";
					return false;
				}

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
		Filesize = static_cast<size_t>(InFilesize.QuadPart);
		return Ret != 0;
		#endif // _WIN32
	}

	void* FileView::CreateFileMappingHandle(void* Handle, size_t Size)
	{
		#ifdef _WIN32
		constexpr DWORD ReadOnlyAccess = PAGE_READONLY;
		constexpr DWORD ReadWriteAccess = PAGE_READWRITE;

		if (AccessMode == FileAccessMode::ReadOnly)
		{
			assert(Size < Filesize);
		}

		const DWORD HiWord = static_cast<DWORD>(static_cast<uint64_t>(Size >> 32) & 0xFFFFFFFF);
		const DWORD LoWord = static_cast<DWORD>(static_cast<uint64_t>(Size) & 0xFFFFFFFF);

		FileMappingSize = Size;

		return CALL_WIN32_RV
		(
			CreateFileMappingA
			(
				Handle,
				nullptr,
				AccessMode == FileAccessMode::ReadOnly ? ReadOnlyAccess : ReadWriteAccess,
				HiWord, // If 'Size' is 0, we read the entire file
				LoWord,
				"") // [TODO]: assign a name here to ensure we don't re-create file mappings
		);
		#endif // _WIN32
	}

	bool FileView::CreateNewFileMappingHandle(size_t Size)
	{
		#ifdef _WIN32
		FileMappingSize = Size;
		Filesize = Size;

		FileMappingHandle = CALL_WIN32_RV
		(
			CreateFileMappingA
			(
				INVALID_HANDLE_VALUE,
				nullptr,
				PAGE_READWRITE,
				HIWORD(Size), // How big should our file be?
				LOWORD(Size), // How big should our file be?
				"") // [TODO]: assign a name here to ensure we don't re-create file mappings
		);

		return FileMappingHandle != nullptr;
		#endif // _WIN32
	}

	void* FileView::CreateMapViewOfFile(void* Handle, size_t Size)
	{
		#ifdef _WIN32
		constexpr DWORD ReadOnlyAccess = FILE_MAP_READ;
		constexpr DWORD ReadWriteAccess = FILE_MAP_WRITE;

		// File offset must be a multiple of system allocation granularity
		// const DWORD FileMapViewOffset = FilepointerOffset / AllocationGranularity * AllocationGranularity;

		// Get actual file mapping size
		// DWORD FileMappingViewSize{ (FilepointerOffset % AllocationGranularity) + static_cast<DWORD>(BytesToRead) };

		if (Size > Filesize)
		{
			Size = 0;
		}

		return CALL_WIN32_RV
		(
			MapViewOfFile
			(
				Handle,
				AccessMode == FileAccessMode::ReadOnly ? ReadOnlyAccess : ReadWriteAccess,
				0,
				0,
				Size // 0 means it will create a view of the entire mapped file
			)
		);
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

	bool FileView::ReallocateFileMapping(size_t NewSize)
	{
		if (!PathUtils::DoesFileExist(Filepath))
		{
			std::cerr << "Can only grow file mapping of existing files\n";
			return false;
		}

		// Release our Map and MapView
		MappedViewHandle.Release();
		FileMappingHandle.Release();

		FileMappingHandle = CreateFileMappingHandle(FileHandle, NewSize);
		MappedViewHandle = { CreateMapViewOfFile(FileMappingHandle, 0), [](void* Handle) { return CALL_WIN32_RV(UnmapViewOfFile(Handle)) != 0; } };

		if (!(FileMappingHandle.IsValid() && MappedViewHandle.IsValid()))
		{
			std::cerr << "Could not grow FileMapping\n";
			return false;
		}

		return true;
	}

	bool FileView::ReallocateMappedViewOfFile(size_t NewSize)
	{
		if (NewSize > Filesize)
		{
			std::cerr << "Cannot grow mapped view of file past filesize\n";
			return false;
		}

		MappedViewHandle.Release();
		MappedViewHandle = { CreateMapViewOfFile(FileMappingHandle, 0), [](void* Handle) { return CALL_WIN32_RV(UnmapViewOfFile(Handle)) != 0; } };

		if (!MappedViewHandle.IsValid())
		{
			std::cerr << "Could not grow mapped view of file\n";
			return false;
		}

		return true;
	}
} // namespace rapidio