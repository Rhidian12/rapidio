#pragma once
#ifdef _WIN32

#define RAPIDIO_USE_EXCEPTIONS

#ifdef RAPIDIO_USE_EXCEPTIONS
#	include <exception>
#else 
#	include <iostream>
#endif // RAPIDIO_USE_EXCEPTIONS

#include <string_view>
#include <sstream>

#include <Windows.h>

namespace rapidio
{
	inline namespace Win32Utils
	{
		#ifdef RAPIDIO_USE_EXCEPTIONS
		class Win32CallFailed final : public std::exception
		{
		public:
			Win32CallFailed(const char* Message) : std::exception(Message) {}
		};
		#endif // RAPIDIO_USE_EXCEPTIONS

		class Win32APICallInfo final
		{
		public:
			Win32APICallInfo(const std::string_view File, const int32_t Line);
			Win32APICallInfo(const std::string_view File, const int32_t Line, const DWORD ErrorToIgnore);

			bool GetSuccess() const;

		private:
			template<typename Func>
			friend Win32APICallInfo Win32APICall(const Func& Function, const std::string_view File, const int32_t Line);
			template<typename Func>
			friend Win32APICallInfo Win32APICall_IgnoreError(const Func& Function, const DWORD ErrorToIgnore, const std::string_view File, const int32_t Line);
			template<typename T, typename Func>
			friend T Win32APICall_RV(const Func& Function, const std::string_view File, const int32_t Line);
			template<typename T, typename Func>
			friend T Win32APICall_RV_IgnoreError(const Func& Function, const DWORD ErrorToIgnore, const std::string_view File, const int32_t Line);

			void LogError() const;

			std::string_view File;
			int32_t Line;
			DWORD Result;
		};

		Win32APICallInfo::Win32APICallInfo(const std::string_view File, const int32_t Line)
			: Win32APICallInfo(File, Line, 0)
		{}

		Win32APICallInfo::Win32APICallInfo(const std::string_view InFile, const int32_t InLine, const DWORD ErrorToIgnore)
			: Result{}
			, File{ InFile }
			, Line{ InLine }
		{
			const DWORD Error{ GetLastError() };
			if (Error != ErrorToIgnore)
			{
				Result = Error;
			}

			SetLastError(ERROR_SUCCESS);
		}

		bool Win32APICallInfo::GetSuccess() const
		{
			return Result == ERROR_SUCCESS;
		}

		void Win32APICallInfo::LogError() const
		{
			if (GetSuccess())
			{
				return;
			}

			char* Buffer{};
			FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, Result, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (char*)&Buffer, 0, nullptr);
			std::ostringstream Stream;
			Stream << "[" << File << ", " << Line << "] Win32 API Call Error: " << Buffer << "\n";
			LocalFree(Buffer); // make sure to free the buffer before throwing

			#ifdef RAPIDIO_USE_EXCEPTIONS
			throw Win32CallFailed(Stream.str().c_str());
			#else
			std::cerr << Stream.str();
			#endif // RAPIDIO_USE_EXCEPTIONS
		}

		template<typename Func>
		Win32APICallInfo Win32APICall(const Func& Function, const std::string_view File, const int32_t Line)
		{
			Function();
			const Win32APICallInfo CallInfo{ File, Line };

			if (!CallInfo.GetSuccess())
			{
				CallInfo.LogError();
			}

			return CallInfo;
		}

		template<typename Func>
		Win32APICallInfo Win32APICall_IgnoreError(const Func& Function, const DWORD ErrorToIgnore, const std::string_view File, const int32_t Line)
		{
			Function();
			const Win32APICallInfo CallInfo{ File, Line, ErrorToIgnore };

			if (!CallInfo.GetSuccess())
			{
				CallInfo.LogError();
			}

			return CallInfo;
		}

		template<typename T, typename Func>
		T Win32APICall_RV(const Func& Function, const std::string_view File, const int32_t Line)
		{
			const T Result = Function();
			const Win32APICallInfo CallInfo{ File, Line };

			if (!CallInfo.GetSuccess())
			{
				CallInfo.LogError();
			}

			return Result;
		}

		template<typename T, typename Func>
		T Win32APICall_RV_IgnoreError(const Func& Function, const DWORD ErrorToIgnore, const std::string_view File, const int32_t Line)
		{
			const T Result = Function();
			const Win32APICallInfo CallInfo{ File, Line, ErrorToIgnore };

			if (!CallInfo.GetSuccess())
			{
				CallInfo.LogError();
			}

			return Result;
		}
	}
}

#ifdef _WIN32
#	define CALL_WIN32(FunctionCall) rapidio::Win32Utils::Win32APICall([&](){ return FunctionCall; }, __FILE__, __LINE__)
#	define CALL_WIN32_IGNORE_ERROR(FunctionCall, ErrorToIgnore) rapidio::Win32Utils::Win32APICall_IgnoreError([&](){ return FunctionCall; }, ErrorToIgnore, __FILE__, __LINE__)
#	define CALL_WIN32_RV(FunctionCall) rapidio::Win32Utils::Win32APICall_RV<decltype(FunctionCall)>([&](){ return FunctionCall; }, __FILE__, __LINE__)
#	define CALL_WIN32_RV_IGNORE_ERROR(FunctionCall, ErrorToIgnore) rapidio::Win32Utils::Win32APICall_RV_IgnoreError<decltype(FunctionCall)>([&](){ return FunctionCall; }, ErrorToIgnore, __FILE__, __LINE__)
#endif	

#endif // _WIN32