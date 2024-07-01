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

struct openssl_listen_socket_t;

template<>
struct native_handle<openssl_listen_socket_t> : native_handle<raw_listen_socket_t>
{
	openssl_ssl_ctx* ssl_ctx;
};

struct openssl_listen_socket_t : listen_socket_base_t<object_t>
{
	using base_type = listen_socket_base_t<object_t>;

	using socket_object_type = openssl_socket_t;
	using security_context_type = openssl_listen_socket_security_context;

	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<openssl_listen_socket_t>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<openssl_listen_socket_t>>& h,
		io_parameters_t<openssl_listen_socket_t, Operation> const& a)
	{
		if constexpr (std::is_same_v<Operation, accept_t>)
		{
			vsm_try(r, uniplexer_handle::blocking_io<openssl_listen_socket_t, accept_t>(
				h,
				a));
	
			return vsm::result<accept_result<basic_detached_handle<openssl_socket_t>>>(
				vsm::result_value,
				*rebind_handle<basic_detached_handle<openssl_socket_t>>(vsm_move(r.socket)),
				r.endpoint);
		}
		else
		{
			return uniplexer_handle::blocking_io<openssl_listen_socket_t, Operation>(h, a);
		}
	}

#if 0
	template<operation_c Operation>
	[[deprecated]] friend vsm::result<io_result_t<openssl_listen_socket_t, Operation>> tag_invoke(
		blocking_io_t<Operation>,
		handle_const_t<Operation, native_handle<openssl_listen_socket_t>>& h,
		io_parameters_t<openssl_listen_socket_t, Operation> const& a)
	{
		return uniplexer_handle::blocking_io<openssl_listen_socket_t, Operation>(h, a);
	}

	[[deprecated]] friend vsm::result<accept_result<basic_detached_handle<openssl_socket_t>>> tag_invoke(
		blocking_io_t<accept_t>,
		native_handle<openssl_listen_socket_t> const& h,
		io_parameters_t<openssl_listen_socket_t, accept_t> const& a)
	{
		vsm_try(r, uniplexer_handle::blocking_io<openssl_listen_socket_t, accept_t>(
			h,
			a));

		return vsm::result<accept_result<basic_detached_handle<openssl_socket_t>>>(
			vsm::result_value,
			*rebind_handle<basic_detached_handle<openssl_socket_t>>(vsm_move(r.socket)),
			r.endpoint);
		//return vsm_lazy(accept_result<basic_detached_handle<openssl_socket_t>>{ vsm_move(r.socket), r.endpoint, });
	}
#endif
};

template<multiplexer M>
struct async_connector<M, openssl_listen_socket_t>
	: async_connector_t<M, raw_listen_socket_t>
{
};

template<multiplexer M>
struct async_operation<M, openssl_listen_socket_t, listen_t>
	: openssl_operation<
		raw_listen_socket_t,
		async_operation<M, openssl_listen_socket_t, listen_t>,
		async_operation_t<M, raw_listen_socket_t, listen_t>,
		async_operation_t<M, raw_listen_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_listen_socket_t,
		async_operation<M, openssl_listen_socket_t, listen_t>,
		async_operation_t<M, raw_listen_socket_t, listen_t>,
		async_operation_t<M, raw_listen_socket_t, close_t>>;

	using H = native_handle<openssl_listen_socket_t>;
	using C = async_connector_t<M, openssl_listen_socket_t>;
	using S = async_operation_t<M, openssl_listen_socket_t, listen_t>;
	using A = io_parameters_t<openssl_listen_socket_t, listen_t>;

	using _raw_listen = async_operation_t<M, raw_listen_socket_t, listen_t>;
	using _raw_close = typename _base::template _raw_close<raw_listen_socket_t>;

	static void _listen_completed(H& h, A const& a)
	{
		vsm_assert(a.security_context != nullptr);
		vsm_assert(h.ssl_ctx == nullptr);

		auto const ssl_ctx = openssl_get_ssl_ctx(*a.security_context);
		openssl_acquire_ssl_ctx(ssl_ctx);
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

		vsm_try_void(submit_io(
			m,
			h,
			c,
			s.m_raw_state.emplace<_raw_listen>(),
			a,
			handler));

		_listen_completed(h, a);

		return {};
	}

	using _base::_notify;

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		_raw_listen& raw_state)
	{
		vsm_try_void(notify_io(
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

	static void _delete(H& h)
	{
		if (h.ssl_ctx != nullptr)
		{
			openssl_release_ssl_ctx(h.ssl_ctx);
			h.ssl_ctx = nullptr;
		}
	}
};

template<multiplexer M>
struct async_operation<M, openssl_listen_socket_t, accept_t>
	: openssl_operation<
		raw_listen_socket_t,
		async_operation<M, openssl_listen_socket_t, accept_t>,
		async_operation_t<M, raw_listen_socket_t, accept_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_listen_socket_t,
		async_operation<M, openssl_listen_socket_t, accept_t>,
		async_operation_t<M, raw_listen_socket_t, accept_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>;

	using H = native_handle<openssl_listen_socket_t> const;
	using C = async_connector_t<M, openssl_listen_socket_t> const;
	using S = async_operation_t<M, openssl_listen_socket_t, accept_t>;
	using A = io_parameters_t<openssl_listen_socket_t, accept_t>;
	using R = accept_result<basic_attached_handle<openssl_socket_t, multiplexer_handle_t<M>>>;

	using _raw_accept = async_operation_t<M, raw_listen_socket_t, accept_t>;
	using _raw_close = typename _base::template _raw_close<raw_socket_t>;

	using _socket_handle_type = basic_attached_handle<openssl_socket_t, multiplexer_handle_t<M>>;

	native_handle<openssl_socket_t> _h;
	vsm_no_unique_address async_connector_t<M, openssl_socket_t> _c;
	network_endpoint _endpoint;

	static vsm::result<void> _accept_completed(H const& h, S& s, auto&& r)
	{
		// The client TLS context is created after successful raw accept.
		vsm_try_assign(s._h.ssl, openssl_state::create(h.ssl_ctx));

		auto [new_h, new_c] = r.socket.release();
		static_cast<native_handle<raw_socket_t>&>(s._h) = vsm_move(new_h);
		static_cast<async_connector_t<M, raw_socket_t>&>(s._c) = vsm_move(new_c);
		s._endpoint = r.endpoint;

		return {};
	}

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s._h = {};

		vsm_try(r, submit_io(
			m,
			h,
			c,
			s.m_raw_state.emplace<_raw_accept>(),
			a,
			handler));

		vsm_try_void(_accept_completed(h, s, vsm_move(r)));

		return {};
	}

	using _base::_notify;

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		_raw_accept& raw_state)
	{
		vsm_try(r, notify_io(
			m,
			h,
			c,
			raw_state,
			a,
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
		M::io_status_type&& status,
		_raw_close& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			s._h,
			s._c,
			raw_state,
			make_args<io_parameters_t<raw_socket_t, close_t>>(),
			vsm_move(status)));

		return vsm::unexpected(raw_state.error);
	}

	static decltype(_h) const& _get_rw_h(H&, S& s)
	{
		return s._h;
	}

	static decltype(_c) const& _get_rw_c(C&, S& s)
	{
		return s._c;
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler)
	{
		vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
		{
			return s._h.ssl->accept();
		}));

		return {};
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
		if (s._h.ssl != nullptr)
		{
			s._h.ssl->delete_context();
			s._h.ssl = nullptr;
		}

		if (s._h.flags[object_t::flags::not_null])
		{
			vsm_try_void(submit_io(
				m,
				s._h,
				s._c,
				s.m_raw_state.emplace<_raw_close>(e),
				make_args<io_parameters_t<raw_socket_t, close_t>>(),
				handler));
		}

		return vsm::unexpected(e);
	}

	static io_result<R> _get_result(M& m, S& s)
	{
		return vsm_lazy(R
		{
			_socket_handle_type
			(
				adopt_handle_t(),
				m,
				vsm_move(s._h),
				vsm_move(s._c)
			),
			s._endpoint,
		});
	}
};

template<multiplexer M>
struct async_operation<M, openssl_listen_socket_t, close_t>
	: async_operation_t<M, raw_listen_socket_t, close_t>
{
	//TODO: release ssl ctx
};

} // namespace allio::detail
