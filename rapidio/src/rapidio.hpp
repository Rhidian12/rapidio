#pragma once

#ifdef _WIN32
#	include "Win32Handle.hpp"
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

	/// <summary>
	/// Requirements for a buffer-like to be passed to 'Read()' and 'Write()'
	/// T must have a .data() returning a type convertible to void*
	/// T must have a .size() returning a size_t which denotes the size of the buffer returned by .data()
	/// T must have a .assign() which accepts a buffer and a size of the given buffer
	/// Note that std::string fits all of these requirements from the get-go, but a custom buffer can be provided
	/// </summary>
	template<typename T>
	concept IsBufferLike = requires (std::remove_cvref_t<T> buff)
	{
		{ buff.data() };
		std::is_convertible_v<decltype(buff.data()), void*>;

		{ buff.size() } -> std::same_as<size_t>;

		{ buff.assign(std::declval<char*>(), std::declval<size_t>()) };
	};

	class FileView final
	{
	public:
		/// <summary>
		/// Creates a FileView object from an existing file on the filesystem.
		/// </summary>
		/// <param name="filepath">Path to the file to be mapped</param>
		/// <param name="accessMode">Should the file be opened with ReadOnly or ReadWrite permissions?</param>
		/// <param name="openMode">How should the file be opened? Only allowed values are OpenExisting or TruncateExisting</param>
		/// <param name="fileMappingSize">How much of the file should be mapped? If set to 0, the entire file is mapped</param>
		/// <param name="offset">Offset into the file to create the file mapping. Note that this must be a multiple of the system allocation granularity,
		/// which can be retrieved by calling FileView::GetSystemAllocationGranularity().
		/// This value will be automatically adjusted to fit the allocation granularity</param>
		/// <returns>std::nullopt if the FileView could not be created. A valid optional of a FileView if the FileView was sucessfully created</returns>
		static std::optional<FileView> CreateViewFromExistingFile(const std::filesystem::path& filepath, FileAccessMode accessMode,
			FileOpenMode openMode, size_t fileMappingSize = 0, size_t offset = 0);

		/// <summary>
		/// Creates a FileView object for a non-existing file on the filesystem. 
		/// !!! IMPORTANT !!! The file will be created on the filesystem with an initial size of 'expectedFileSize'
		/// </summary>
		/// <param name="filepath">Path to the file to be created and mapped</param>
		/// <param name="expectedFileSize">Initial size of the file</param>
		/// <returns>std::nullopt if the FileView could not be created. A valid optional of a FileView if the FileView was sucessfully created</returns>
		static std::optional<FileView> CreateViewForNewFile(const std::filesystem::path& filepath, size_t expectedFileSize);

		/// <summary>
		/// Static function to get the system allocation granularity
		/// </summary>
		/// <returns>The system allocation granularity</returns>
		static size_t GetSystemAllocationGranularity();

		// Sets filepointer to a specific position		
		bool Seek(size_t position);

		/// <summary>
		/// Read bytes from the filepointer and return a std::string. The string reserves the required data before any data is read
		/// If data should not be copied, consider using a custom Buffer-like
		/// </summary>
		/// <param name="bytesToRead">Number of bytes to read</param>
		/// <param name="autoGrowFileMapping">if set to true, will automatically re-allocate the filemapping if the read amount of bytes exceeds the mapped file size</param>
		/// <returns>std::string containing read data</returns>
		std::string Read(size_t bytesToRead, bool autoGrowFileMapping = true);

		/// <summary>
		/// Read bytes from the filepointer and assign the data to the given Buffer-like. The buffer given to the Buffer-like object is valid
		/// as long as no re-allocation of the file mapping takes place
		/// </summary>
		/// <param name="buffer">The buffer-like object to store the read data in</param>
		/// <param name="bytesToRead">Number of bytes to read</param>
		/// <param name="autoGrowFileMapping">if set to true, will automatically re-allocate the filemapping if the read amount of bytes exceeds the mapped file size</param>
		/// <returns>Returns true upon successful reading of the data</returns>
		template<IsBufferLike T>
		bool Read(T& buffer, size_t bytesToRead, bool autoGrowFileMapping = true);

		/// <summary>
		/// Write a buffer to the mapped file at the provided offset.
		/// </summary>
		/// <param name="data">Buffer containing data to write to mapped file</param>
		/// <param name="offset">Offset (from start of file) to write data to</param>
		/// <param name="autoGrowFile">If set to true, will automatically increase filesize to required size to write data</param>
		/// <param name="autoGrowFileMapping">If set to true, will automatically increase mapped file size to required size to write data</param>
		/// <returns>Returns true upon successful writing of data</returns>
		template<IsBufferLike T>
		bool Write(T&& data, size_t offset = 0, bool autoGrowFile = true, bool autoGrowFileMapping = true);

	private:
		FileView(const std::string& filepath, const FileAccessMode accessMode);

		bool OpenFile(const FileAccessMode accessMode, const FileOpenMode opemMode);
		bool GetFilesize();
		void CreateFileMappingHandle(size_t size);
		bool CreateNewFileMappingHandle(size_t size);
		void CreateMapViewOfFile(size_t size, size_t offset);
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
		size_t m_allocationGranularity;
		#endif // _WIN32
	};
} // namespace rapidio

#ifdef _WIN32
#	include "rapidioWin32.hpp"
#endif // _WIN32