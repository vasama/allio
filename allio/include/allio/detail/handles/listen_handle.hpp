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
	using mutation_tag = void;

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

template<std::derived_from<handle_t> BaseHandleTag, typename SocketHandleTag>
struct basic_listen_handle_t : BaseHandleTag
{
	using base_type = BaseHandleTag;

	using listen_t = socket_io::listen_t;
	using accept_t = socket_io::accept_t;

	using asynchronous_operations = type_list<
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



struct raw_listen_handle_t : basic_listen_handle_t<platform_handle_t, raw_socket_handle_t>
{
	using socket_handle_tag = raw_socket_handle_t;

	using base_type = basic_listen_handle_t<platform_handle_t, raw_socket_handle_t>;

	using base_type::blocking_io;

	static vsm::result<void> blocking_io(
		handle_t::close_t,
		native_type& h,
		io_parameters_t<handle_t::close_t> const& args);

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
using blocking_raw_listen_handle = blocking_handle<raw_listen_handle_t>;

template<typename MultiplexerHandle>
using async_raw_listen_handle = async_handle<raw_listen_handle_t, MultiplexerHandle>;


struct listen_handle_t : basic_listen_handle_t<handle_t, socket_handle_t>
{
	using socket_handle_tag = socket_handle_t;

	using base_type = basic_listen_handle_t<handle_t, socket_handle_t>;

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
using blocking_listen_handle = blocking_handle<listen_handle_t>;

template<typename MultiplexerHandle>
using async_listen_handle = async_handle<listen_handle_t, MultiplexerHandle>;


#if 0
struct listen_handle_base;

template<std::derived_from<listen_handle_base> Socket>
struct basic_listen_handle_t;

template<typename Socket>
auto _accept_result(blocking_handle<basic_listen_handle_t<Socket>> const&)
	-> blocking_handle<basic_socket_handle_t<Socket>>;

template<typename Socket, typename MultiplexerHandle>
auto _accept_result(async_handle<basic_listen_handle_t<Socket>, MultiplexerHandle> const&)
	-> async_handle<basic_socket_handle_t<Socket>, MultiplexerHandle>;

struct listen_handle_base
{
	struct listen_t
	{
		using handle_type = listen_handle_base;
		using result_type = void;

		struct required_params_type
		{
			network_endpoint endpoint;
		};

		using optional_params_type = listen_backlog_t;
	};

	struct accept_t
	{
		using handle_type = listen_handle_base const;

		template<typename Handle>
		using result_type_template = decltype(_accept_result(std::declval<Handle const&>()));

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};
};

template<std::derived_from<listen_handle_base> Socket>
struct basic_listen_handle_t : Socket
{

};

template<typename SocketProvider>
struct basic_listen_handle_t : SocketProvider::handle_base_type
{
	using base_type = typename SocketProvider::handle_base_type;

	using native_type = socket_native_type<SocketProvider>;


	struct listen_t
	{
		using handle_type = basic_listen_handle_t;
		using result_type = void;

		struct required_params_type
		{
			network_endpoint endpoint;
		};

		using optional_params_type = backlog_t;
	};

	struct accept_t
	{
		using handle_type = basic_listen_handle_t const;

		template<typename MultiplexerHandle>
		using result_type_template = accept_result<SocketProvider, MultiplexerHandle>;

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};


	template<typename H>
	struct concrete_interface : base_type::template concrete_interface<H>
	{
		[[nodiscard]] auto accept(auto&&... args) const
		{
			return generic_io(
				static_cast<H const&>(*this),
				io_args<accept_t>()(vsm_forward(args)...));
		}
	};
};


struct raw_listen_handle_base_t
{
};

using raw_listen_handle_t = basic_listen_handle_t<raw_listen_handle_base_t>;
using abstract_raw_listen_handle = abstract_handle<raw_listen_handle_t>;
using blocking_raw_listen_handle = blocking_handle<raw_listen_handle_t>;


struct listen_handle_base_t
{
};

using listen_handle_t = basic_listen_handle_t<listen_handle_base_t>;
using abstract_listen_handle = abstract_handle<listen_handle_t>;
using blocking_listen_handle = blocking_handle<listen_handle_t>;
#endif


template<listen_handle_tag HandleTag = listen_handle_t>
auto listen(network_endpoint const& endpoint, auto&&... args)
{
	return io_handle_sender<HandleTag, socket_io::listen_t>(
		vsm_lazy(io_args<socket_io::listen_t>(endpoint)(vsm_forward(args)...)));
}

template<listen_handle_tag HandleTag = listen_handle_t>
vsm::result<blocking_handle<HandleTag>> listen_blocking(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_handle<HandleTag>> r(vsm::result_value);
	vsm_try_void(blocking_io<socket_io::listen_t>(
		*r,
		io_args<socket_io::listen_t>(endpoint)(vsm_forward(args)...)));
	return r;
}


auto raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listen<raw_listen_handle_t>(endpoint, vsm_forward(args)...);
}

vsm::result<blocking_raw_listen_handle> raw_listen_blocking(network_endpoint const& endpoint, auto&&... args)
{
	return listen_blocking<raw_listen_handle_t>(endpoint, vsm_forward(args)...);
}



#if 0
template<typename SecurityProvider = default_security_provider>
class listen_handle_t;

template<typename SecurityProvider, typename M>
struct _accept_result
{
	template<typename OtherM = M>
	using socket_handle = basic_handle<socket_handle_t<SecurityProvider>, OtherM>;

	struct type
	{
		socket_handle<> socket;
		network_endpoint endpoint;

		template<typename OtherM>
		friend vsm::result<socket_handle<OtherM>> tag_invoke(rebind_handle_t<OtherM>, type&& self)
		{
			vsm_try(socket, rebind_handle<OtherM>(vsm_move(self.socket)));
			return vsm_lazy(type{ vsm_move(socket), self.endpoint });
		}
	};
};

template<typename SecurityProvider, typename M>
using accept_result = typename _accept_result<SecurityProvider, M>::type;

template<typename SecurityProvider>
class _listen_handle : public socket_handle_base<SecurityProvider>
{
	using _socket_handle_t = socket_handle_t<SecurityProvider>;
	using _listen_handle_t = listen_handle_t<SecurityProvider>;

	template<typename M>
	using socket_handle = basic_handle<_socket_handle_t, M>;

	template<typename M>
	using accept_result = detail::accept_result<SecurityProvider, M>;

protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct listen_t
	{
		static constexpr bool producer = true;

		using handle_type = _listen_handle_t;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = backlog_t;
	};

	struct accept_t
	{
		using handle_type = _listen_handle_t const;

		template<typename M>
		using result_type_template = accept_result<M>;

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	using asynchronous_operations = type_list_cat<
		typename base_type::asynchronous_operations,
		type_list<listen_t, accept_t>
	>;

protected:
	template<typename H, typename M>
	struct interface : base_type::template interface<H, M>
	{
		using socket_handle_type = socket_handle<M>;
		using accept_result_type = accept_result<M>;

		[[nodiscard]] auto accept(auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<accept_t>()(vsm_forward(args)...));
		}
	};
};

template<>
class listen_handle_t<void> : public _listen_handle<void>
{
protected:
	static vsm::result<void> do_blocking_io(
		listen_handle_t& h,
		io_result_ref_t<listen_t> r,
		io_parameters_t<listen_t> const& args);

	static vsm::result<void> do_blocking_io(
		listen_handle_t const& h,
		io_result_ref_t<accept_t> r,
		io_parameters_t<accept_t> const& args);
};

template<typename SecurityProvider>
class listen_handle_t : public _listen_handle<SecurityProvider>
{
	using _h = _listen_handle<SecurityProvider>;

protected:

#if 0 //TODO: Use tag_invoke in the security provider instead?
	static vsm::result<void> do_blocking_io(
		listen_handle_t& h,
		io_result_ref_t<typename _h::listen_t> const r,
		io_parameters_t<typename _h::listen_t> const& args)
	{
		vsm_try(socket_source, get_socket_source(args));
		vsm_try(listen_socket, socket_source.listen(args));
		set_native_handle(*listen_socket.release_socket_handle());
		return {};
	}

	static vsm::result<void> do_blocking_io(
		listen_handle_t const& h,
		io_result_ref_t<typename _h::accept_t> const r,
		io_parameters_t<typename _h::accept_t> const& args)
	{
		_h::m_secure_socket.accept();
		return {};
	}
#endif
};

using raw_listen_handle_t = listen_handle_t<void>;

template<typename Multiplexer>
//using basic_listen_handle = basic_handle<listen_handle_t<>, Multiplexer>;
using basic_listen_handle = basic_handle<listen_handle_t<void>, Multiplexer>;

template<typename Multiplexer>
using basic_raw_listen_handle = basic_handle<raw_listen_handle_t, Multiplexer>;


vsm::result<basic_listen_handle<void>> listen(
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_listen_handle<void>;

	h_type h;
	vsm_try_void(blocking_io(
		h,
		no_result,
		io_args<h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
vsm::result<basic_listen_handle<multiplexer_handle_t<Multiplexer>>> listen(
	Multiplexer&& multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_listen_handle<multiplexer_handle_t<Multiplexer>>;

	h_type h(vsm_forward(multiplexer));
	vsm_try_void(blocking_io(
		h,
		no_result,
		io_args<typename h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}
#endif

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/listen_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/listen_handle.hpp>
#endif
