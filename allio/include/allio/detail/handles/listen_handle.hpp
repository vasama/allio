#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/detail/handles/socket_handle.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/lazy.hpp>

namespace allio::detail {

template<typename SecurityProvider = default_security_provider>
class listen_handle_t;

struct backlog_t
{
	std::optional<uint32_t> backlog;

	struct tag_t
	{
		struct argument_t
		{
			uint32_t value;

			friend void tag_invoke(set_argument_t, backlog_t& args, argument_t const& value)
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
inline constexpr backlog_t::tag_t backlog = {};

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

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/listen_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/listen_handle.hpp>
#endif
