#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/socket_handle_base.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>
#include <vsm/standard.hpp>

namespace allio::detail {
namespace socket_io {

struct connect_t
{
	using mutation_tag = void;

	using result_type = void;
	using required_params_type = network_endpoint_t;
	using optional_params_type = deadline_t;
};

struct disconnect_t
{
	using mutation_tag = void;

	using result_type = void;
	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
};

struct stream_read_t
{
	using result_type = size_t;
	using required_params_type = untyped_buffers_t;
	using optional_params_type = deadline_t;
};

struct stream_write_t
{
	using result_type = size_t;
	using required_params_type = untyped_buffers_t;
	using optional_params_type = deadline_t;
};

} // namespace socket_io

template<std::derived_from<handle_t> BaseHandleTag>
struct basic_socket_handle_t : BaseHandleTag
{
	using base_type = BaseHandleTag;

	using connect_t = socket_io::connect_t;
	using disconnect_t = socket_io::disconnect_t;
	using read_some_t = socket_io::stream_read_t;
	using write_some_t = socket_io::stream_write_t;

	using asynchronous_operations = type_list<
		connect_t,
		disconnect_t,
		read_some_t,
		write_some_t
	>;

	template<typename H, typename M>
	struct concrete_interface : base_type::template concrete_interface<H, M>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffers)(vsm_forward(args)...));
		}
	};
};

template<typename BaseHandleTag>
void _socket_handle_tag(basic_socket_handle_t<BaseHandleTag> const&);

template<typename T>
concept socket_handle_tag = requires(T const& tag)
{
	_socket_handle_tag(tag);
};


struct raw_socket_handle_t : basic_socket_handle_t<platform_handle_t>
{
	using base_type = basic_socket_handle_t<platform_handle_t>;

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(handle_t::close_t, native_type& h, io_parameters_t<handle_t::close_t> const& args);

	static vsm::result<void> blocking_io(socket_io::connect_t, native_type& h, io_parameters_t<socket_io::connect_t> const& args);
	static vsm::result<void> blocking_io(socket_io::disconnect_t, native_type& h, io_parameters_t<socket_io::disconnect_t> const& args);

	static vsm::result<size_t> blocking_io(socket_io::stream_read_t, native_type const& h, io_parameters_t<socket_io::stream_read_t> const& args);
	static vsm::result<size_t> blocking_io(socket_io::stream_write_t, native_type const& h, io_parameters_t<socket_io::stream_write_t> const& args);
};
using abstract_raw_socket_handle = abstract_handle<raw_socket_handle_t>;
using blocking_raw_socket_handle = blocking_handle<raw_socket_handle_t>;

template<typename MultiplexerHandle>
using async_raw_socket_handle = async_handle<raw_socket_handle_t, MultiplexerHandle>;


struct socket_handle_t : basic_socket_handle_t<handle_t>
{
	using base_type = basic_socket_handle_t<handle_t>;

	struct native_type : base_type::native_type
	{
	};

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(socket_io::connect_t, native_type& h, io_parameters_t<socket_io::connect_t> const& args);
	static vsm::result<void> blocking_io(socket_io::disconnect_t, native_type& h, io_parameters_t<socket_io::disconnect_t> const& args);

	static vsm::result<size_t> blocking_io(socket_io::stream_read_t, native_type const& h, io_parameters_t<socket_io::stream_read_t> const& args);
	static vsm::result<size_t> blocking_io(socket_io::stream_write_t, native_type const& h, io_parameters_t<socket_io::stream_write_t> const& args);
};
using abstract_socket_handle = abstract_handle<socket_handle_t>;
using blocking_socket_handle = blocking_handle<socket_handle_t>;

template<typename MultiplexerHandle>
using async_socket_handle = async_handle<socket_handle_t, MultiplexerHandle>;


#if 0
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

	using asynchronous_operations = type_list<
		connect_t,
		disconnect_t,
		read_some_t,
		write_some_t
	>;
};

template<std::derived_from<socket_handle_base> Socket>
struct basic_socket_handle_t : Socket
{
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

	using asynchronous_operations = type_list_cat<
		base_type::asynchronous_operations,
		socket_handle_base::asynchronous_operations
	>;

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
#endif


template<socket_handle_tag HandleTag = socket_handle_t>
auto connect(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<HandleTag, socket_io::connect_t>(
		vsm_lazy(io_args<socket_io::connect_t>(endpoint)(vsm_forward(args)...)));
}

template<socket_handle_tag HandleTag = socket_handle_t>
vsm::result<blocking_handle<HandleTag>> connect_blocking(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<HandleTag>> r(vsm::result_value);
	vsm_try_void(blocking_io<socket_io::connect_t>(
		*r,
		io_args<socket_io::connect_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


auto raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_handle_t>(endpoint, vsm_forward(args)...);
}

vsm::result<blocking_raw_socket_handle> raw_connect_blocking(network_endpoint const& endpoint, auto&&... args)
{
	return connect_blocking<raw_socket_handle_t>(endpoint, vsm_forward(args)...);
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/socket_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/socket_handle.hpp>
#endif
