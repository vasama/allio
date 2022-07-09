#pragma once

#include <allio/handle.hpp>

#include <cstdint>

namespace allio {

enum class native_platform_handle : uintptr_t
{
	null = 0
};

class platform_handle : public handle
{
	detail::linear<native_platform_handle> m_native_handle;

public:
	struct native_handle_type : handle::native_handle_type
	{
		native_platform_handle handle;
	};


	native_platform_handle get_platform_handle() const
	{
		return m_native_handle.value;
	}

	native_handle_type get_native_handle() const
	{
		return
		{
			handle::get_native_handle(),
			m_native_handle.value,
		};
	}


	explicit operator bool() const
	{
		return m_native_handle.value != native_platform_handle::null;
	}

	bool operator!() const
	{
		return m_native_handle.value == native_platform_handle::null;
	}

protected:
	explicit platform_handle(native_handle_type const handle)
		: allio::handle(handle)
		, m_native_handle(handle.handle)
	{
	}

	platform_handle() = default;
	platform_handle(platform_handle&&) = default;
	platform_handle& operator=(platform_handle&&) = default;
	~platform_handle() = default;

	result<void> set_native_handle(native_handle_type handle);
	result<native_handle_type> release_native_handle();

	result<void> do_close();

private:
	result<void> do_close_sync();
};

} // namespace allio
