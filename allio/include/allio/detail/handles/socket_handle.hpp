#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/socket_handle_base.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>
#include <vsm/standard.hpp>

namespace allio::detail {

struct socket_handle_base
{
	struct connect_t
	{
		using handle_type = socket_handle_base;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = deadline_t;
	};

	struct disconnect_t
	{
		using handle_type = socket_handle_base;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
	};

	struct read_some_t
	{
		using handle_type = socket_handle_base const;
		using result_type = size_t;

		using required_params_type = untyped_buffers_t;
		using optional_params_type = deadline_t;
	};

	struct write_some_t
	{
		using handle_type = socket_handle_base const;
		using result_type = size_t;

		using required_params_type = untyped_buffers_t;
		using optional_params_type = deadline_t;
	};
};

template<std::derived_from<socket_handle_base> Socket>
struct basic_socket_handle_t : Socket
{
	using asynchronous_operations = type_list_cat<
		typename Socket::base_type::asynchronous_operations,
		type_list<
			typename Socket::connect_t,
			typename Socket::disconnect_t,
			typename Socket::read_some_t,
			typename Socket::write_some_t
		>
	>;

	template<typename H>
	struct concrete_interface : Socket::base_type::template concrete_interface<H>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return generic_io(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
		{
			return generic_io(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return generic_io(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
		{
			return generic_io(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffers)(vsm_forward(args)...));
		}
	};
};


struct raw_socket_handle_base_t : platform_handle_t, socket_handle_base
{
	using base_type = platform_handle_t;

	struct native_type : base_type::native_type {};

	static vsm::result<void> blocking_io(native_type& h, io_parameters_t<connect_t> const& args);
	static vsm::result<void> blocking_io(native_type& h, io_parameters_t<disconnect_t> const& args);

	static vsm::result<size_t> blocking_io(native_type const& h, io_parameters_t<read_some_t> const& args);
	static vsm::result<size_t> blocking_io(native_type const& h, io_parameters_t<write_some_t> const& args);
};

using raw_socket_handle_t = basic_socket_handle_t<raw_socket_handle_base_t>;
using abstract_raw_socket_handle = abstract_handle<raw_socket_handle_t>;
using blocking_raw_socket_handle = blocking_handle<raw_socket_handle_t>;

template<multiplexer_for<raw_socket_handle_t> Multiplexer>
using async_raw_socket_handle = async_handle<raw_socket_handle_t, Multiplexer>;


struct socket_handle_base_t : platform_handle_t, socket_handle_base
{
	using base_type = platform_handle_t;

	struct native_type : base_type::native_type
	{
		secure_socket_source* socket_source;
		secure_socket_object* socket_object;
	};

	static vsm::result<void> blocking_io(native_type& h, io_parameters_t<connect_t> const& args);
	static vsm::result<void> blocking_io(native_type& h, io_parameters_t<disconnect_t> const& args);

	static vsm::result<void> blocking_io(native_type const& h, io_parameters_t<read_some_t> const& args);
	static vsm::result<void> blocking_io(native_type const& h, io_parameters_t<write_some_t> const& args);
};

using socket_handle_t = basic_socket_handle_t<default_security_provider>;
using abstract_socket_handle = abstract_handle<socket_handle_t>;
using blocking_socket_handle = blocking_handle<socket_handle_t>;

template<multiplexer_for<socket_handle_t> Multiplexer>
using async_socket_handle = async_handle<socket_handle_t, Multiplexer>;



auto connect(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<socket_handle_t, socket_handle_t::connect_t>(
		vsm_lazy(io_args<socket_handle_t::connect_t>(endpoint)(vsm_forward(args)...)));
}

vsm::result<blocking_socket_handle> blocking_connect(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_socket_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(
		*r,
		io_args<socket_handle_t::connect_t>(endpoint)(vsm_forward(args)...)));
	return r;
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/socket_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/socket_handle.hpp>
#endif
