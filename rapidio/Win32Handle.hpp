#pragma once
#ifdef _WIN32

#include "Win32Call.hpp"

#include <functional>
#include <string>

#ifdef RAPIDIO_USE_EXCEPTIONS
#	include <exception>
#else 
#	include <iostream>
#endif // RAPIDIO_USE_EXCEPTIONS

#pragma warning ( push )
#pragma warning ( disable : 4005 ) /* warning C4005: 'APIENTRY': macro redefinition */ 
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#pragma warning ( pop )


#define RAPIDIO_USE_EXCEPTIONS

namespace rapidio
{
	#ifdef RAPIDIO_USE_EXCEPTIONS
	class CloseHandleException : public std::exception
	{
	public:
		CloseHandleException(const char* Message)
			: exception(Message)
		{}
	};
	#endif // RAPIDIO_USE_EXCEPTIONS

	inline namespace Win32Utils
	{
		class Win32Handle final
		{
			using Deleter = std::function<bool(void*)>;

		public:
			Win32Handle();
			explicit Win32Handle(void* const Handle);
			Win32Handle(void* const Handle, const Deleter& CustomDeleter);
			~Win32Handle();

			Win32Handle(const Win32Handle&) noexcept = delete;
			Win32Handle(Win32Handle&& Other) noexcept;
			Win32Handle& operator=(const Win32Handle&) noexcept = delete;
			Win32Handle& operator=(Win32Handle&& Other) noexcept;

			Win32Handle& operator=(const void* const Other) noexcept;

			bool IsValid() const;

			void* const Get();
			const void* const Get() const;

			operator void* () const;

		private:
			void CloseHandle();

			Deleter CustomDeleter;
			void* Handle;
		};

		Win32Handle::Win32Handle()
			: Handle{ INVALID_HANDLE_VALUE }
			, CustomDeleter{}
		{}

		Win32Handle::Win32Handle(void* const Handle)
			: Handle{ Handle }
			, CustomDeleter{}
		{}

		Win32Handle::Win32Handle(void* const Handle, const Deleter& CustomDeleter)
			: Handle{ Handle }
			, CustomDeleter{ CustomDeleter }
		{}

		Win32Handle::~Win32Handle()
		{
			CloseHandle();
		}

		Win32Handle::Win32Handle(Win32Handle&& Other) noexcept
			: Handle{ std::move(Other.Handle) }
		{
			Other.Handle = INVALID_HANDLE_VALUE;
		}

		Win32Handle& Win32Handle::operator=(Win32Handle&& Other) noexcept
		{
			if (Handle)
			{
				CloseHandle();
			}

			Handle = std::move(Other.Handle);
			Other.Handle = INVALID_HANDLE_VALUE;

			return *this;
		}

		Win32Handle& Win32Handle::operator=(const void* const Other) noexcept
		{
			if (Handle)
			{
				CloseHandle();
			}

			Handle = const_cast<void*>(Other);

			return *this;
		}

		bool Win32Handle::IsValid() const
		{
			return Handle != INVALID_HANDLE_VALUE;
		}

		void* const Win32Handle::Get()
		{
			return Handle;
		}

		const void* const Win32Handle::Get() const
		{
			return Handle;
		}

		Win32Handle::operator void* () const
		{
			return Handle;
		}

		void Win32Handle::CloseHandle()
		{
			if (Handle)
			{
				if (CustomDeleter)
				{
					if (!CustomDeleter(Handle))
					{
						const std::string ErrorMessage = "Handle could not be closed through Custom Deleter\n";
						#ifdef RAPIDIO_USE_EXCEPTIONS
						throw CloseHandleException(ErrorMessage.c_str());
						#else
						std::cerr << ErrorMessage;
						#endif // RAPIDIO_USE_EXCEPTIONS
					}
				}
				else
				{
					auto Call = CALL_WIN32(::CloseHandle(Handle));

					if (!Call.GetSuccess())
					{
						const std::string ErrorMessage = "Handle could not be closed\n";
						#ifdef RAPIDIO_USE_EXCEPTIONS
						throw CloseHandleException(ErrorMessage.c_str());
						#else
						std::cerr << ErrorMessage;
						#endif // RAPIDIO_USE_EXCEPTIONS
					}
				}

				Handle = INVALID_HANDLE_VALUE;
			}
		}

	} // inline namespace Win32Utils
} // namespace rapidio

#endif // _WIN32