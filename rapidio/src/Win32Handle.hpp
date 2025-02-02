#pragma once
#ifdef _WIN32

#include "Win32Call.hpp"

#include <functional>
#include <string>
#include <iostream>

#pragma warning ( push )
#pragma warning ( disable : 4005 ) /* warning C4005: 'APIENTRY': macro redefinition */ 
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#pragma warning ( pop )


namespace rapidio
{
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

			Win32Handle& operator=(void* const Other) noexcept;

			bool IsValid() const;

			void* const Get();
			const void* const Get() const;

			explicit operator void* () const;
			bool operator==(const Win32Handle & other) const noexcept;
			bool operator!=(const Win32Handle & other) const noexcept;
			bool operator==(void* other) const noexcept;
			bool operator!=(void* other) const noexcept;

			void Release();

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
			, CustomDeleter{ std::move(Other.CustomDeleter) }
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
			CustomDeleter = std::move(Other.CustomDeleter);
			Other.Handle = INVALID_HANDLE_VALUE;

			return *this;
		}

		Win32Handle& Win32Handle::operator=(void* const Other) noexcept
		{
			if (Handle)
			{
				CloseHandle();
			}

			Handle = Other;

			return *this;
		}

		bool Win32Handle::IsValid() const
		{
			return Handle && Handle != INVALID_HANDLE_VALUE;
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

		bool Win32Handle::operator==(const Win32Handle& other) const noexcept
		{
			return Handle == other.Handle;
		}

		bool Win32Handle::operator!=(const Win32Handle& other) const noexcept
		{
			return !(*this == other);
		}

		bool Win32Handle::operator==(void* other) const noexcept
		{
			return Handle == other;
		}

		bool Win32Handle::operator!=(void* other) const noexcept
		{
			return !(*this == other);
		}

		void Win32Handle::Release()
		{
			CloseHandle();
		}

		void Win32Handle::CloseHandle()
		{
			if (Handle && Handle != INVALID_HANDLE_VALUE)
			{
				if (CustomDeleter)
				{
					if (!CustomDeleter(Handle))
					{
						std::cerr << "Handle could not be closed through Custom Deleter\n";
					}
				}
				else
				{
					auto Call = CALL_WIN32(::CloseHandle(Handle));

					if (!Call.GetSuccess())
					{
						std::cerr << "Handle could not be closed\n";
					}
				}

				Handle = INVALID_HANDLE_VALUE;
			}
		}

	} // inline namespace Win32Utils
} // namespace rapidio

#endif // _WIN32