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
	vsm::linear<native_platform_handle> m_native_handle;

public:
	using base_type = handle;

	struct implementation;

	struct native_handle_type : base_type::native_handle_type
	{
		native_platform_handle handle;
	};


	#define allio_platform_handle_create_parameters(type, data, ...) \
		type(allio::platform_handle, create_parameters) \
		data(bool, multiplexable, false) \

	allio_interface_parameters(allio_platform_handle_create_parameters);


	native_platform_handle get_platform_handle() const
	{
		return m_native_handle.value;
	}

	native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_native_handle.value,
		};
	}

protected:
	explicit constexpr platform_handle(type_id<platform_handle> const type)
		: base_type(type)
	{
	}

	platform_handle(platform_handle&&) = default;
	platform_handle& operator=(platform_handle&&) = default;
	~platform_handle() = default;

	vsm::result<void> set_native_handle(native_handle_type handle);
	vsm::result<native_handle_type> release_native_handle();

	vsm::result<void> close_sync(basic_parameters const& args);
};

} // namespace allio
