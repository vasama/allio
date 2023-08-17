#pragma once

#error

#include <allio/detail/handle.hpp>
#include <allio/opaque_handle.h>

#include <vsm/unique_resource.hpp>

namespace allio::detail {

/// @brief ABI-stable native handle type for asynchronous polling
///        across ABI boundaries such as dynamically linked libraries.
///        Instead of exposing allio types in your dynamic library ABI,
///        return a native_opaque_handle instead and wrap it afterwards
///        in @ref opaque_handle for RAII.
using native_opaque_handle = allio_opaque_handle;

using unique_opaque_handle = vsm::unique_resource<
	native_opaque_handle,
	vsm::function<void(native_opaque_handle)>;


namespace detail {

class _opaque_handle : public handle
{
protected:
	using base_type = handle;

private:
	unique_opaque_handle m_native_handle;

public:
	struct native_handle_type : base_type::native_handle_type
	{
		native_opaque_handle native_handle;
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


	struct poll_tag;
	using poll_parameters = deadline_parameters;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			poll_tag
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
		basic_sender<M, H, poll_tag> poll_async(P const& args = {}) const;
	};
};

} // namespace detail

using opaque_handle = basic_handle<detail::_opaque_handle>;

template<typename Multiplexer>
using basic_opaque_handle = async_handle<detail::_opaque_handle, Multiplexer>;

} // namespace allio
