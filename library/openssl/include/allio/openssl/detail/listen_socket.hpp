#pragma once

#include <allio/detail/handles/raw_listen_socket.hpp>
#include <allio/openssl/detail/socket.hpp>

#include <vsm/lazy.hpp>
#include <vsm/standard.hpp>

#include <variant>

namespace allio::detail {

class openssl_listen_socket_security_context : public openssl_security_context
{
public:
	static vsm::result<openssl_listen_socket_security_context> create(
		security_context_parameters const& a)
	{
		vsm_try(ssl_ctx, openssl_create_server_ssl_ctx(a));
		return vsm_lazy(openssl_listen_socket_security_context(vsm_move(ssl_ctx)));
	}

private:
	using openssl_security_context::openssl_security_context;
};

template<typename RawListenSocket>
struct openssl_listen_socket_t : listen_socket_base_t<object_t>
{
	using base_type = listen_socket_base_t<object_t>;

	using socket_object_type = openssl_socket_t<typename RawListenSocket::socket_object_type>;
	using security_context_type = openssl_listen_socket_security_context;

	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<openssl_listen_socket_t>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<openssl_listen_socket_t>>& h,
		io_parameters_t<openssl_listen_socket_t, Operation> const& a)
	{
		if constexpr (std::is_same_v<Operation, accept_t>)
		{
			vsm_try(r, uniplexer_handle::blocking_io<openssl_listen_socket_t, accept_t>(h, a));
			return rebind_handle<basic_detached_handle<socket_object_type>>(vsm_move(r));
		}
		else
		{
			return uniplexer_handle::blocking_io<openssl_listen_socket_t, Operation>(h, a);
		}
	}
};

template<typename RawListenSocket>
struct native_handle<openssl_listen_socket_t<RawListenSocket>> : native_handle<RawListenSocket>
{
	openssl_ssl_ctx* ssl_ctx;
};

template<typename M, typename RawListenSocket>
struct async_connector<M, openssl_listen_socket_t<RawListenSocket>>
	: async_connector_t<M, RawListenSocket>
{
};

// TODO: Listen should probably not use openssl_operation.
//       At least it does not require raw_read or raw_write.
template<typename M, typename RawListenSocket>
class async_operation<M, openssl_listen_socket_t<RawListenSocket>, listen_t>
	: public openssl_operation<
		M,
		openssl_listen_socket_t<RawListenSocket>,
		listen_t,
		typename RawListenSocket::socket_object_type,
		async_operation<M, RawListenSocket, listen_t>,
		async_operation<M, RawListenSocket, close_t>>
{
	using base = openssl_operation<
		M,
		openssl_listen_socket_t<RawListenSocket>,
		listen_t,
		typename RawListenSocket::socket_object_type,
		async_operation<M, RawListenSocket, listen_t>,
		async_operation<M, RawListenSocket, close_t>>;

	using raw_socket_object_type = typename RawListenSocket::socket_object_type;
	using socket_object_type = openssl_socket_t<raw_socket_object_type>;

	using H = native_handle<openssl_listen_socket_t<RawListenSocket>>;
	using C = async_connector_t<M, openssl_listen_socket_t<RawListenSocket>>;
	using S = async_operation_t<M, openssl_listen_socket_t<RawListenSocket>, listen_t>;
	using A = io_parameters_t<openssl_listen_socket_t<RawListenSocket>, listen_t>;

	using raw_listen = async_operation_t<M, RawListenSocket, listen_t>;
	using raw_close = openssl_raw_close<M, RawListenSocket>;

	static void _listen_completed(H& h, A const& a)
	{
		vsm_assert(a.security_context != nullptr);
		vsm_assert(h.ssl_ctx == nullptr);

		auto const ssl_ctx = detail::openssl_get_ssl_ctx(*a.security_context);
		detail::openssl_acquire_ssl_ctx(ssl_ctx);
		h.ssl_ctx = ssl_ctx;
	}

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		if (a.security_context == nullptr)
		{
			//TODO: Use a more specific error code.
			return vsm::unexpected(error::invalid_argument);
		}

		h = {};

		vsm_try_void(detail::submit_io(
			m,
			h,
			c,
			s.m_raw_state.template emplace<raw_listen>(),
			a,
			handler));

		_listen_completed(h, a);

		return {};
	}

	using base::_notify;

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		raw_listen& raw_state)
	{
		vsm_try_void(detail::notify_io(
			m,
			h,
			c,
			raw_state,
			a,
			vsm_move(status)));

		_listen_completed(h, a);

		return {};
	}

	static io_result<void> _continue(M&, H&, C&, S&, A const&, io_handler<M>&)
	{
		return {};
	}

	static io_result<void> _on_error(
		M& m,
		H& h,
		C& c,
		S& s,
		A const&,
		io_handler<M>& handler,
		std::error_code const e)
	{
		if (h.ssl_ctx != nullptr)
		{
			detail::openssl_release_ssl_ctx(h.ssl_ctx);
			h.ssl_ctx = nullptr;
		}
	
		if (h.flags[object_t::flags::not_null])
		{
			vsm_try_void(detail::submit_io(
				m,
				h,
				c,
				s.m_raw_state.template emplace<raw_close>(e),
				make_args<io_parameters_t<RawListenSocket, close_t>>(),
				handler));
		}

		return vsm::unexpected(e);
	}

	friend base;
};

template<typename M, typename RawListenSocket>
class async_operation<M, openssl_listen_socket_t<RawListenSocket>, accept_t>
	: public openssl_operation<
		M,
		openssl_listen_socket_t<RawListenSocket>,
		accept_t,
		typename RawListenSocket::socket_object_type,
		async_operation<M, RawListenSocket, accept_t>,
		async_operation<M, typename RawListenSocket::socket_object_type, close_t>>
{
	using base = openssl_operation<
		M,
		openssl_listen_socket_t<RawListenSocket>,
		accept_t,
		typename RawListenSocket::socket_object_type,
		async_operation<M, RawListenSocket, accept_t>,
		async_operation<M, typename RawListenSocket::socket_object_type, close_t>>;

	using raw_socket_object_type = typename RawListenSocket::socket_object_type;
	using socket_object_type = openssl_socket_t<raw_socket_object_type>;

	using H = native_handle<openssl_listen_socket_t<RawListenSocket>> const;
	using C = async_connector_t<M, openssl_listen_socket_t<RawListenSocket>> const;
	using S = async_operation_t<M, openssl_listen_socket_t<RawListenSocket>, accept_t>;
	using A = io_parameters_t<openssl_listen_socket_t<RawListenSocket>, accept_t>;
	using R = basic_attached_handle<socket_object_type, multiplexer_handle_t<M>>;

	using raw_accept = async_operation_t<M, RawListenSocket, accept_t>;
	using raw_close = openssl_raw_close<M, raw_socket_object_type>;

	native_handle<socket_object_type> m_socket_h;
	vsm_no_unique_address async_connector_t<M, socket_object_type> m_socket_c;

	static vsm::result<void> _accept_completed(H const& h, S& s, auto&& socket)
	{
		// The client TLS context is created after successful raw accept.
		vsm_try_assign(s.m_socket_h.openssl, detail::new_openssl_socket(h.ssl_ctx));

		auto [socket_h, socket_c] = socket.release();
		static_cast<native_handle<raw_socket_object_type>&>(s.m_socket_h) = vsm_move(socket_h);
		static_cast<async_connector_t<M, raw_socket_object_type>&>(s.m_socket_c) = vsm_move(socket_c);

		return {};
	}

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s.m_socket_h = {};

		vsm_try(socket, detail::submit_io(
			m,
			h,
			c,
			s.m_raw_state.template emplace<raw_accept>(),
			a,
			handler));

		vsm_try_void(_accept_completed(h, s, vsm_move(socket)));

		return {};
	}

	using base::_notify;

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		raw_accept& raw_state)
	{
		vsm_try(r, detail::notify_io(
			m,
			h,
			c,
			raw_state,
			a,
			handler,
			status));

		vsm_try_void(_accept_completed(h, s, vsm_move(r)));

		return {};
	}

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		raw_close& raw_state)
	{
		vsm_try_void(detail::notify_io(
			m,
			s.m_socket_h,
			s.m_socket_c,
			raw_state,
			make_args<io_parameters_t<raw_socket_object_type, close_t>>(),
			handler,
			vsm_move(status)));

		return vsm::unexpected(raw_state.error);
	}

	static decltype(m_socket_h) const& _get_rw_h(H&, S& s)
	{
		return s.m_socket_h;
	}

	static decltype(m_socket_c) const& _get_rw_c(C&, S& s)
	{
		return s.m_socket_c;
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler)
	{
		return base::_enter(m, h, c, s, handler, [&]()
		{
			return s.m_socket_h.openssl->accept();
		});
	}

	static io_result<void> _on_error(
		M& m,
		H&,
		C&,
		S& s,
		A const&,
		io_handler<M>& handler,
		std::error_code const e)
	{
		if (s.m_socket_h.openssl != nullptr)
		{
			detail::delete_openssl_socket(s.m_socket_h.openssl);
			s.m_socket_h.openssl = nullptr;
		}

		if (s.m_socket_h.flags[object_t::flags::not_null])
		{
			vsm_try_void(detail::submit_io(
				m,
				s.m_socket_h,
				s.m_socket_c,
				s.m_raw_state.template emplace<raw_close>(e),
				make_args<io_parameters_t<raw_socket_object_type, close_t>>(),
				handler));
		}

		return vsm::unexpected(e);
	}

	static io_result<R> _get_result(M& m, S& s)
	{
		return vsm_lazy(R(
			adopt_handle_t(),
			m,
			vsm_move(s.m_socket_h),
			vsm_move(s.m_socket_c)));
	}

	friend base;
};

template<typename M, typename RawListenSocket>
struct async_operation<M, openssl_listen_socket_t<RawListenSocket>, close_t>
	: async_operation_t<M, RawListenSocket, close_t>
{
	//TODO: release ssl ctx
};

} // namespace allio::detail
