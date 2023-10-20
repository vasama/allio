#pragma once

#include <allio/detail/handle.hpp>
#include <allio/platform.hpp>

namespace allio::detail {

struct platform_handle_t : handle_t
{
	using base_type = handle_t;

	struct impl_type;

	struct native_type : base_type::native_type
	{
		native_platform_handle platform_handle;
	};

	static void zero_native_handle(native_type& h)
	{
		base_type::zero_native_handle(h);
		h.platform_handle = native_platform_handle::null;
	}

	template<typename H>
	struct abstract_interface : base_type::abstract_interface<H>
	{
		[[nodiscard]] native_platform_handle platform_handle() const
		{
			return static_cast<H const&>(*this).native().platform_handle;
		}
	};

	static vsm::result<void> blocking_io(native_type& h, io_parameters_t<close_t> const& args);
};

#if 0
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

	void close(auto&& close_platform_handle)
	{
		auto const handle = m_native_handle.value;
		if (handle != native_platform_handle::null)
		{
			m_native_handle.value = native_platform_handle::null;
			vsm_forward(close_platform_handle)(handle);
		}

		base_type::close();
	}

	void close()
	{
		close([](native_platform_handle const handle)
		{
			vsm_verify(close_handle(handle));
		});
	}


	template<typename H, typename M>
	struct interface : base_type::interface<H, M>
	{
		template<parameters<duplicate_parameters> P = duplicate_parameters::interface>
		vsm::result<H> duplicate(P const& args = {}) const;
	};
};
#endif

} // namespace allio::detail
