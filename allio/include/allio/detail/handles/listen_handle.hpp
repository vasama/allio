#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/detail/handles/socket_handle.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>

namespace allio::detail {

struct listen_backlog_t
{
	std::optional<uint32_t> backlog;

	struct tag_t
	{
		struct argument_t
		{
			uint32_t value;

			friend void tag_invoke(set_argument_t, listen_backlog_t& args, argument_t const& value)
			{
				args.backlog = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(uint32_t const backlog)
		{
			return { backlog };
		}
	};
};
inline constexpr listen_backlog_t::tag_t listen_backlog = {};

template<typename SocketHandle>
struct accept_result
{
	SocketHandle socket;
	network_endpoint endpoint;
};

namespace socket_io {

struct listen_t
{
	using mutation_tag = producer_t;

	using result_type = void;

	using required_params_type = network_endpoint_t;
	using optional_params_type = listen_backlog_t;

	using runtime_tag = bounded_runtime_t;
};

struct accept_t
{
	template<typename H, typename M>
	using result_type_template = accept_result<basic_handle<typename H::socket_handle_tag, M>>;

	using required_params_type = no_parameters_t;
	using optional_params_type = deadline_t;
};

} // namespace socket_io

template<std::derived_from<object_t> BaseHandleTag, typename SocketHandleTag>
struct basic_listen_handle_t : BaseHandleTag
{
	using base_type = BaseHandleTag;

	using listen_t = socket_io::listen_t;
	using accept_t = socket_io::accept_t;

	using operations = type_list<
		listen_t,
		accept_t
	>;

	template<typename H, typename M>
	struct concrete_interface : base_type::template concrete_interface<H, M>
	{
		using socket_handle_type = basic_handle<SocketHandleTag, M>;
		using accept_result_type = accept_result<socket_handle_type>;

		[[nodiscard]] auto accept(auto&&... args) const
		{
			return generic_io<accept_t>(
				static_cast<H const&>(*this),
				io_args<accept_t>()(vsm_forward(args)...));
		}
	};
};

template<typename BaseHandleTag, typename SocketHandleTag>
void _listen_handle_tag(basic_listen_handle_t<BaseHandleTag, SocketHandleTag> const&);

template<typename T>
concept listen_handle_tag = requires(T const& tag)
{
	_listen_handle_tag(tag);
};



struct raw_listen_handle_t : basic_listen_handle_t<platform_object_t, raw_socket_handle_t>
{
	using socket_handle_tag = raw_socket_handle_t;

	using base_type = basic_listen_handle_t<platform_object_t, raw_socket_handle_t>;

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(
		object_t::close_t,
		native_type& h,
		io_parameters_t<object_t::close_t> const& args);

	static vsm::result<void> blocking_io(
		socket_io::listen_t,
		native_type& h,
		io_parameters_t<socket_io::listen_t> const& args);

	static vsm::result<accept_result<blocking_handle<socket_handle_tag>>> blocking_io(
		socket_io::accept_t,
		native_type const& h,
		io_parameters_t<socket_io::accept_t> const& args);
};
using abstract_raw_listen_handle = abstract_handle<raw_listen_handle_t>;


struct listen_handle_t : basic_listen_handle_t<object_t, socket_handle_t>
{
	using socket_handle_tag = socket_handle_t;

	using base_type = basic_listen_handle_t<object_t, socket_handle_t>;

	struct native_type : base_type::native_type
	{
	};

	static vsm::result<void> blocking_io(
		socket_io::listen_t,
		native_type& h,
		io_parameters_t<socket_io::listen_t> const& args);

	static vsm::result<accept_result<blocking_handle<socket_handle_tag>>> blocking_io(
		socket_io::accept_t,
		native_type const& h,
		io_parameters_t<socket_io::accept_t> const& args);
};
using abstract_listen_handle = abstract_handle<listen_handle_t>;


namespace _listen_blocking {

using listen_handle = blocking_handle<listen_handle_t>;

template<listen_handle_tag Object = listen_handle_t>
vsm::result<blocking_handle<Object>> listen(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<Object>> r(vsm::result_value);
	vsm_try_void(blocking_io<typename Object::listen_t>(
		*r,
		io_args<typename Object::listen_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


using raw_listen_handle = blocking_handle<raw_listen_handle_t>;

vsm::result<raw_listen_handle> raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listne<raw_listen_handle_t>(endpoint, vsm_forward(args)...);
}

} // namespace _listen_blocking

namespace _listen_async {

template<typename MultiplexerHandle>
using basic_listen_handle = async_handle<listen_handle_t, MultiplexerHandle>;

template<listen_handle_tag Object = listen_handle_t>
ex::sender auto listen(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<Object, typename Object::listen_t>(
		vsm_lazy(io_args<typename Object::listen_t>(endpoint)(vsm_forward(args)...)));
}


template<typename MultiplexerHandle>
using basic_raw_listen_handle = async_handle<listen_handle_t, MultiplexerHandle>;

ex::sender auto raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listen<raw_listen_handle_t>(endpoint, vsm_forward(args)...);
}

} // namespace _listen_async

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/listen_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/listen_handle.hpp>
#endif
