#pragma once

#include <allio/detail/handle.hpp>
#include <allio/platform.hpp>

namespace allio::detail {

class platform_handle : public handle
{
protected:
	using base_type = handle;

private:
	vsm::linear<native_platform_handle> m_native_handle;

public:
	struct impl_type;


	#define allio_platform_handle_create_parameters(type, data, ...) \
		type(allio::detail::platform_handle, create_parameters) \
		data(bool, multiplexable, false) \

	allio_interface_parameters(allio_platform_handle_create_parameters);


	struct duplicate_t;
	using duplicate_parameters = no_parameters;


	struct native_handle_type : base_type::native_handle_type
	{
		native_platform_handle native_handle;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_native_handle.value,
		};
	}


	[[nodiscard]] native_platform_handle get_platform_handle() const
	{
		return m_native_handle.value;
	}

protected:
	allio_detail_default_lifetime(platform_handle);
	
	explicit platform_handle(native_handle_type const& native)
		: base_type(native)
		, m_native_handle(native.native_handle)
	{
	}


	void set_native_handle(native_handle_type const& native)
	{
		base_type::set_native_handle(native);
		m_native_handle.value = native.native_handle;
	}


	void close(error_handler const* const error_handler)
	{
		if (m_native_handle.value != native_platform_handle::null)
		{
			vsm_verify(close_handle(m_native_handle.value, error_handler));
		}
	}


	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<duplicate_parameters> P = duplicate_parameters::interface>
		vsm::result<H> duplicate(P const& args = {}) const;
	};
};

} // namespace allio::detail
