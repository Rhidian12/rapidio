#pragma once
#ifdef _WIN32

#include <iostream>
#include <string_view>
#include <sstream>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace rapidio
{
	inline namespace Win32Utils
	{
		class Win32APICallInfo final
		{
		public:
			Win32APICallInfo(const std::string_view file, const int32_t line);
			Win32APICallInfo(const std::string_view file, const int32_t line, const DWORD errorToIgnore);

			bool GetSuccess() const;

		private:
			template<typename Func>
			friend Win32APICallInfo Win32APICall(const Func& function, const std::string_view file, const int32_t line);
			template<typename Func>
			friend Win32APICallInfo Win32APICall_IgnoreError(const Func& function, const DWORD errorToIgnore, const std::string_view file, const int32_t line);
			template<typename T, typename Func>
			friend T Win32APICall_RV(const Func& function, const std::string_view file, const int32_t line);
			template<typename T, typename Func>
			friend T Win32APICall_RV_IgnoreError(const Func& function, const DWORD errorToIgnore, const std::string_view file, const int32_t line);

			void LogError() const;

			std::string_view m_file;
			int32_t m_line;
			DWORD m_result;
		};

		Win32APICallInfo::Win32APICallInfo(const std::string_view file, const int32_t line)
			: Win32APICallInfo(file, line, 0)
		{}

		Win32APICallInfo::Win32APICallInfo(const std::string_view file, const int32_t line, const DWORD errorToIgnore)
			: m_result{}
			, m_file{ file }
			, m_line{ line }
		{
			const DWORD error{ GetLastError() };
			if (error != errorToIgnore)
			{
				m_result = error;
			}

			SetLastError(ERROR_SUCCESS);
		}

		bool Win32APICallInfo::GetSuccess() const
		{
			return m_result == ERROR_SUCCESS;
		}

		void Win32APICallInfo::LogError() const
		{
			if (GetSuccess())
			{
				return;
			}

			char* buffer{};
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, m_result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&buffer, 0, nullptr);
			std::ostringstream stream;
			stream << "[" << m_file << ", " << m_line << "] Win32 API Call Error: " << buffer << "\n";
			LocalFree(buffer); // make sure to free the buffer

			std::cerr << stream.str();
		}

		template<typename Func>
		Win32APICallInfo Win32APICall(const Func& function, const std::string_view file, const int32_t line)
		{
			function();
			const Win32APICallInfo callInfo{ file, line };

			if (!callInfo.GetSuccess())
			{
				callInfo.LogError();
			}

			return callInfo;
		}

		template<typename Func>
		Win32APICallInfo Win32APICall_IgnoreError(const Func& function, const DWORD errorToIgnore, const std::string_view file, const int32_t line)
		{
			function();
			const Win32APICallInfo callInfo{ file, line, errorToIgnore };

			if (!callInfo.GetSuccess())
			{
				callInfo.LogError();
			}

			return callInfo;
		}

		template<typename T, typename Func>
		T Win32APICall_RV(const Func& function, const std::string_view file, const int32_t line)
		{
			const T result = function();
			const Win32APICallInfo callInfo{ file, line };

			if (!callInfo.GetSuccess())
			{
				callInfo.LogError();
			}

			return result;
		}

		template<typename T, typename Func>
		T Win32APICall_RV_IgnoreError(const Func& function, const DWORD errorToIgnore, const std::string_view file, const int32_t line)
		{
			const T result = function();
			const Win32APICallInfo callInfo{ file, line, errorToIgnore };

			if (!callInfo.GetSuccess())
			{
				callInfo.LogError();
			}

			return result;
		}
	}
}

#ifdef _WIN32
#	define CALL_WIN32(functionCall) rapidio::Win32Utils::Win32APICall([&](){ return functionCall; }, __FILE__, __LINE__)
#	define CALL_WIN32_IGNORE_ERROR(functionCall, errorToIgnore) rapidio::Win32Utils::Win32APICall_IgnoreError([&](){ return functionCall; }, errorToIgnore, __FILE__, __LINE__)
#	define CALL_WIN32_RV(functionCall) rapidio::Win32Utils::Win32APICall_RV<decltype(functionCall)>([&](){ return functionCall; }, __FILE__, __LINE__)
#	define CALL_WIN32_RV_IGNORE_ERROR(functionCall, errorToIgnore) rapidio::Win32Utils::Win32APICall_RV_IgnoreError<decltype(functionCall)>([&](){ return functionCall; }, errorToIgnore, __FILE__, __LINE__)
#endif	

#endif // _WIN32