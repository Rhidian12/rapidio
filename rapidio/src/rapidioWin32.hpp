#pragma once

#include "PathUtils.hpp"

#include "Win32Call.hpp"
#include "Win32Handle.hpp"

#include <fileapi.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <cassert>

namespace rapidio
{
	std::optional<FileView> FileView::CreateViewFromExistingFile(const std::filesystem::path& filepath, const FileAccessMode accessMode,
		const FileOpenMode openMode, size_t size)
	{
		if (!PathUtils::DoesFileExist(filepath))
		{
			std::cerr << "FileView::CreateViewFromExistingFile > File must already exist!\n";
			return std::nullopt;
		}

		// Don't allow any creation here
		switch (openMode)
		{
			case FileOpenMode::CreateNew:
			case FileOpenMode::CreateAlways:
				std::cerr << "FileView::CreateViewFromExistingFile > Cannot create a file\n";
				return std::nullopt;
		}

		FileView view(filepath.string(), accessMode);

		if (!view.OpenFile(accessMode, openMode))
		{
			return std::nullopt;
		}

		if (!view.GetFilesize())
		{
			return std::nullopt;
		}

		view.CreateFileMappingHandle(size);
		if (!view.m_fileMappingHandle.IsValid())
		{
			return std::nullopt;
		}

		view.CreateMapViewOfFile(0);
		if (!view.m_mappedViewHandle.IsValid())
		{
			return std::nullopt;
		}

		return view;
	}

	std::optional<FileView> FileView::CreateViewForNewFile(const std::filesystem::path& filepath, size_t expectedFileSize)
	{
		if (expectedFileSize == 0)
		{
			std::cerr << "FileView::CreateViewForNewFile > size cannot be 0\n";
			return std::nullopt;
		}

		if (PathUtils::DoesFileExist(filepath))
		{
			std::cerr << "FileView::CreateViewForNewFile > File cannot already exist!\n";
			return std::nullopt;
		}

		FileView view(filepath.string(), FileAccessMode::ReadWrite);

		if (!view.OpenFile(FileAccessMode::ReadWrite, FileOpenMode::CreateNew))
		{
			return std::nullopt;
		}

		if (!view.GetFilesize())
		{
			return std::nullopt;
		}

		view.CreateFileMappingHandle(expectedFileSize);
		if (!view.m_fileMappingHandle.IsValid())
		{
			return std::nullopt;
		}

		view.CreateMapViewOfFile(expectedFileSize);
		if (!view.m_mappedViewHandle.IsValid())
		{
			return std::nullopt;
		}

		return view;
	}

	bool FileView::Seek(size_t position)
	{
		if (position >= m_filesize)
		{
			std::cerr << "FileView::Seek > Cannot seek to past EOF\n";
			return false;
		}

		if (m_fileMappingSize > 0 && position >= m_fileMappingSize)
		{
			std::cerr << "FileView::Seek > Cannot seek past end of Mapped View\n";
			return false;
		}

		m_filepointer = position;
		return true;
	}

	std::string FileView::Read(size_t bytesToRead, bool autoGrowFileMapping /* = true */)
	{
		std::string temp;
		temp.reserve(bytesToRead);
		Read(temp, bytesToRead, autoGrowFileMapping);
		return temp;
	}

	template<IsBufferLike T>
	bool FileView::Read(T& buffer, size_t bytesToRead, bool autoGrowFileMapping /* = true */)
	{
		// Are we at EOF?
		if (m_filepointer >= m_filesize)
		{
			return false;
		}

		// If we're not at EOF, but reading 'BytesToRead' would push us past EOF, adjust 'BytesToRead' until we hit EOF
		if (bytesToRead + m_filepointer > m_filesize)
		{
			bytesToRead = m_filesize - m_filepointer;
		}

		// Check our mapped view size we created, if not 0
		// If 0, the previous check covers the filesize
		if (m_fileMappingSize > 0 && bytesToRead + m_filepointer > m_fileMappingSize)
		{
			if (!autoGrowFileMapping)
			{
				std::cerr << "FileView::Read > Reading " << bytesToRead << "  would read past Mapped View!\n";
				return false;
			}
			else
			{
				// Linearly grow the amount of data we are mapping
				size_t newSize = (bytesToRead + m_filepointer) * 2;
				if (newSize > m_filesize)
				{
					newSize = m_filesize;
				}

				ReallocateFileMapping(newSize);
			}
		}

		const size_t oldFilepointer = m_filepointer;
		m_filepointer += bytesToRead;
		buffer.assign(static_cast<char*>(m_mappedViewHandle.Get()) + oldFilepointer, bytesToRead);
		return true;
	}

	template<IsBufferLike T>
	bool FileView::Write(T&& data, size_t offset /* = 0 */, bool autoGrowFile /* = true */, bool autoGrowFileMapping /* = true */)
	{
		if (m_accessMode == FileAccessMode::ReadOnly)
		{
			std::cerr << "FileView::Write > Cannot write to read-only mapping\n";
			return false;
		}

		if (const std::pair<bool, bool> tooSmall{ m_filesize > 0 && data.size() + offset > m_filesize, data.size() + offset > m_fileMappingSize };
			tooSmall.first || tooSmall.second)
		{
			if (tooSmall.first && !autoGrowFile)
			{
				std::cerr << "FileView::Write > size of data + offset is bigger than filesize with autogrow disabled!\n";
				return false;
			}

			if (tooSmall.second && !autoGrowFileMapping)
			{
				std::cerr << "FileView::Write > size of data + offset is bigger than mapped view of file with autogrow disabled!\n";
				return false;
			}

			size_t newSize = 0;

			if (tooSmall.first)
			{
				newSize = m_filesize + (data.size() + offset - m_filesize);
				ReallocateFileMapping(newSize);
			}
			else /* if (TooSmall.second) */
			{
				newSize = m_fileMappingSize + (data.size() + offset - m_fileMappingSize);
				if (newSize > m_filesize && !autoGrowFile)
				{
					std::cerr << "FileView::Write > size of data + offset is bigger than filesize with autogrow disabled!\n";
					return false;
				}

				ReallocateFileMapping(newSize);
			}
		}

		std::memcpy(static_cast<char*>(m_mappedViewHandle.Get()) + offset, static_cast<const void*>(data.data()), data.size());
		return true;
	}

	FileView::FileView(const std::string& filepath, const FileAccessMode accessMode)
		: m_filepath(filepath)
		, m_accessMode(accessMode)
	{
	}

	bool FileView::OpenFile(const FileAccessMode accessMode, const FileOpenMode OpenMode)
	{
		const bool doesFileExist = PathUtils::DoesFileExist(m_filepath);

		DWORD errorToIgnore{};

		switch (OpenMode)
		{
			case FileOpenMode::CreateNew:
				if (doesFileExist)
				{
					std::cerr << "FileView > OpenMode::CreateNew > File " << m_filepath << " already exists\n";
					return false;
				}
				if (accessMode != FileAccessMode::ReadWrite)
				{
					std::cerr << "FileView > OpenMode::CreateNew > requires ReadWrite AccessMode\n";
					return false;
				}
				errorToIgnore = ERROR_FILE_EXISTS;
				break;
			case FileOpenMode::OpenExisting:
				if (!doesFileExist)
				{
					std::cerr << "FileView > OpenMode::OpenExisting > File " << m_filepath << " does not exist\n";
					return false;
				}
				errorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::TruncateExisting:
				if (!doesFileExist)
				{
					std::cerr << "FileView > OpenMode::TruncateExisting > File " << m_filepath << " does not exist\n";
					return false;
				}
				errorToIgnore = ERROR_FILE_NOT_FOUND;
				break;
			case FileOpenMode::CreateAlways:
				if (accessMode != FileAccessMode::ReadWrite)
				{
					std::cerr << "FileView > OpenMode::CreateAlways requires ReadWrite AccessMode\n";
					return false;
				}

				errorToIgnore = ERROR_ALREADY_EXISTS;
				break;
			default:
				break;
		}

		m_fileHandle = CALL_WIN32_RV_IGNORE_ERROR
		(
			CreateFileA
			(
				m_filepath.c_str(),
				static_cast<DWORD>(m_accessMode),
				0, /* = Exclusive Access */
				nullptr,
				static_cast<DWORD>(OpenMode),
				FILE_ATTRIBUTE_NORMAL,
				nullptr
			),
			errorToIgnore
		);

		return m_fileHandle != nullptr;
	}

	bool FileView::GetFilesize()
	{
		LARGE_INTEGER filesize;
		const BOOL Ret{ CALL_WIN32_RV(GetFileSizeEx(static_cast<void*>(m_fileHandle), &filesize)) };
		m_filesize = static_cast<size_t>(filesize.QuadPart);
		return Ret != 0;
	}

	void FileView::CreateFileMappingHandle(size_t size)
	{
		if (m_accessMode == FileAccessMode::ReadOnly)
		{
			assert(size < m_filesize);
		}

		const DWORD hiWord = static_cast<DWORD>(static_cast<uint64_t>(size >> 32) & 0xFFFFFFFF);
		const DWORD loWord = static_cast<DWORD>(static_cast<uint64_t>(size) & 0xFFFFFFFF);

		m_fileMappingSize = size;

		m_fileMappingHandle = CALL_WIN32_RV
		(
			CreateFileMappingA
			(
				static_cast<void*>(m_fileHandle),
				nullptr,
				m_accessMode == FileAccessMode::ReadOnly ? PAGE_READONLY : PAGE_READWRITE,
				hiWord, // If 'size' is 0, we read the entire file
				loWord,
				"") // [TODO]: assign a name here to ensure we don't re-create file mappings
		);
	}

	bool FileView::CreateNewFileMappingHandle(size_t size)
	{
		m_fileMappingSize = size;
		m_filesize = size;

		m_fileMappingHandle = CALL_WIN32_RV
		(
			CreateFileMappingA
			(
				INVALID_HANDLE_VALUE,
				nullptr,
				PAGE_READWRITE,
				HIWORD(size), // How big should our file be?
				LOWORD(size), // How big should our file be?
				"") // [TODO]: assign a name here to ensure we don't re-create file mappings
		);

		return m_fileMappingHandle != nullptr;
	}

	void FileView::CreateMapViewOfFile(size_t size)
	{
		// File offset must be a multiple of system allocation granularity
		// const DWORD FileMapViewOffset = FilepointerOffset / AllocationGranularity * AllocationGranularity;

		// Get actual file mapping size
		// DWORD FileMappingViewSize{ (FilepointerOffset % AllocationGranularity) + static_cast<DWORD>(BytesToRead) };

		if (size > m_filesize)
		{
			size = 0;
		}

		m_mappedViewHandle = { CALL_WIN32_RV
		(
			MapViewOfFile
			(
				static_cast<void*>(m_fileMappingHandle),
				m_accessMode == FileAccessMode::ReadOnly ? FILE_MAP_READ : FILE_MAP_WRITE,
				0,
				0,
				size // 0 means it will create a view of the entire mapped file
			)
		), [](void* handle) { return CALL_WIN32_RV(UnmapViewOfFile(handle)) != 0; } };
	}

	//bool FileView::GetSystemAllocationGranularity()
	//{
	//	SYSTEM_INFO SystemInfo;
	//	CALL_WIN32(GetNativeSystemInfo(&SystemInfo));
	//	m_allocationGranularity = SystemInfo.dwAllocationGranularity;
	//	return true;
	//}

	bool FileView::ReallocateFileMapping(size_t newSize)
	{
		if (!PathUtils::DoesFileExist(m_filepath))
		{
			std::cerr << "Can only grow file mapping of existing files\n";
			return false;
		}

		// Release our Map and MapView
		m_mappedViewHandle.Release();
		m_fileMappingHandle.Release();

		CreateFileMappingHandle(newSize);
		CreateMapViewOfFile(0);

		if (!(m_fileMappingHandle.IsValid() && m_mappedViewHandle.IsValid()))
		{
			std::cerr << "Could not grow FileMapping\n";
			return false;
		}

		return true;
	}

	bool FileView::ReallocateMappedViewOfFile(size_t newSize)
	{
		if (newSize > m_filesize)
		{
			std::cerr << "Cannot grow mapped view of file past filesize\n";
			return false;
		}

		m_mappedViewHandle.Release();
		CreateMapViewOfFile(0);

		if (!m_mappedViewHandle.IsValid())
		{
			std::cerr << "Could not grow mapped view of file\n";
			return false;
		}

		return true;
	}
} // namespace rapidio