#pragma once

#error

#include <allio/detail/handle.hpp>
#include <allio/opaque_handle.h>

#include <vsm/unique_resource.hpp>

#include <functional>

namespace allio::detail {

template<typename Version>
using unique_opaque_handle = vsm::unique_resource<
	Version,
	std::function<void(Version)>;


class _opaque_handle : public handle
{
protected:
	using base_type = handle;

private:
	unique_opaque_handle m_native_handle;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		opaque_handle_v1 native_handle;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_native_handle.get(),
		};
	}


	[[nodiscard]] unique_opaque_handle const& get_opaque_handle() const
	{
		return m_handle.get();
	}


	struct poll_t;
	using poll_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			poll_t
		>
	>;

protected:
	allio_detail_default_lifetime(_opaque_handle);

	explicit _opaque_handle(native_handle_type const& native)
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
		template<parameters<poll_parameters> P = poll_parameters::interface>
		vsm::result<void> poll(P const& args = {}) const;
	};

	template<typename M, typename H>
	struct async_interface
	{
		template<parameters<poll_parameters> P = poll_parameters::interface>
		io_sender<H, poll_t> poll_async(P const& args = {}) const;
	};
};

using opaque_handle = basic_handle<detail::_opaque_handle>;

template<typename Multiplexer>
using basic_opaque_handle = async_handle<detail::_opaque_handle, Multiplexer>;


opaque_handle make_opaque_handle(opaque_handle_v1 const native);

} // namespace allio::detail
