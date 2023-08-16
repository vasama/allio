#pragma once

#include <allio/handle2.hpp>
#include <allio/platform.hpp>

namespace allio {

class platform_handle : public handle
{
protected:
	using base_type = handle;

private:
	vsm::linear<native_platform_handle> m_native_handle;

public:
	struct duplicate_tag;
	struct duplicate_parameters {};


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
	
	platform_handle(native_handle_type const& native)
		: base_type(native)
		, m_native_handle(native.native_handle)
	{
	}


	void set_native_handle(native_handle_type const& native)
	{
		base_type::set_native_handle(native);
		m_native_handle.value = native.native_handle;
	}


	template<typename H>
	struct sync_interface
	{
		template<parameters<duplicate_parameters> P = duplicate_parameters::interface>
		vsm::result<H> duplicate(P const& args = {}) const;
	};
};

} // namespace allio
