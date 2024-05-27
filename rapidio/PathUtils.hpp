#pragma once

#include <string_view>

#ifdef _WIN32
#	include "Win32Call.hpp"

#	include <Windows.h>
#endif // _WIN32

namespace rapidio
{
	inline namespace PathUtils
	{
		bool DoesFileExist(const std::string_view Filepath)
		{
			#ifdef _WIN32
			const DWORD Attributes = CALL_WIN32_RV(GetFileAttributesA(Filepath.data()));

			return Attributes != INVALID_FILE_ATTRIBUTES && !(Attributes & FILE_ATTRIBUTE_DIRECTORY);
			#endif // _WIN32
		}
	}
}