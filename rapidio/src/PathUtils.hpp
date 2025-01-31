#pragma once

#include <filesystem>

#ifdef _WIN32
#	include "Win32Call.hpp"

#	include <Windows.h>
#endif // _WIN32

namespace rapidio
{
	inline namespace PathUtils
	{
		bool DoesFileExist(const std::filesystem::path & Filepath)
		{
			#ifdef _WIN32
			const DWORD Attributes = CALL_WIN32_RV_IGNORE_ERROR(GetFileAttributesA(Filepath.string().c_str()), ERROR_FILE_NOT_FOUND);

			return Attributes != INVALID_FILE_ATTRIBUTES && !(Attributes & FILE_ATTRIBUTE_DIRECTORY);
			#endif // _WIN32
		}
	}
}