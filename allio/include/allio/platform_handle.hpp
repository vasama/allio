#pragma once

#include <allio/handle.hpp>
#include <allio/platform.hpp>

namespace allio {

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


	[[nodiscard]] native_platform_handle get_platform_handle() const
	{
		return m_native_handle.value;
	}

	[[nodiscard]] native_handle_type get_native_handle() const
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

	static bool check_native_handle(native_handle_type const& handle);
	void set_native_handle(native_handle_type const& handle);
	native_handle_type release_native_handle();

	template<std::derived_from<native_handle_type> NativeHandle>
	static vsm::result<NativeHandle> duplicate(NativeHandle handle)
	{
		native_handle_type& h = handle;
		vsm_try_assign(h.handle, duplicate(h.handle));
		return handle;
	}

	//vsm::result<void> close_sync(basic_parameters const& args);

private:
	static vsm::result<native_platform_handle> duplicate(native_platform_handle handle);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args);
};

} // namespace allio
