#pragma once

#include <allio/detail/handles/listen_socket.hpp>
#include <allio/openssl/detail/openssl.hpp>
#include <allio/openssl/detail/socket.hpp>

#include <variant>

namespace allio::detail {

struct openssl_listen_socket_t : basic_listen_socket_t<object_t, openssl_socket_t>
{
	using base_type = basic_listen_socket_t<object_t, openssl_socket_t>;

	struct native_type : raw_listen_socket_t::native_type
	{
		openssl_context* context;

		template<typename O>
		static vsm::result<io_result_t<O, openssl_listen_socket_t>> tag_invoke(
			blocking_io_t<O>,
			handle_const_t<O, native_type>& h,
			io_parameters_t<O> const& args)
		{
			return blocking_multiplexer::do_io<openssl_listen_socket_t, O>(h, args);
		}

		static vsm::result<accept_result<blocking_handle<openssl_socket_t>>> tag_invoke(
			blocking_io_t<accept_t>,
			native_type const& h,
			io_parameters_t<accept_t> const& args)
		{
			vsm_try(r, blocking_multiplexer::do_io<openssl_listen_socket_t, accept_t>(h, args));

			//openssl_socket_t::native_type socket_h;
			//connector_t<blocking_multiplexer, openssl_socket_t> socket_c;
			//r.socket.release(socket_h, socket_c);

			return vsm_lazy(accept_result<blocking_handle<openssl_socket_t>>
			{
				//blocking_handle<openssl_socket_t>
				//(
				//	adopt_handle_t(),
				//	socket_h
				//),
				vsm_move(r.socket),
				r.endpoint,
			});
		}
	};
};

template<typename M>
struct connector<M, openssl_listen_socket_t>
	: connector<M, raw_listen_socket_t>
{
};

template<typename M>
struct operation<M, openssl_listen_socket_t, socket_io::listen_t>
	: openssl_operation_impl<
		raw_listen_socket_t,
		operation<M, openssl_listen_socket_t, socket_io::listen_t>,
		operation_t<M, raw_listen_socket_t, socket_io::listen_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>
{
	using _base = openssl_operation_impl<
		raw_listen_socket_t,
		operation<M, openssl_listen_socket_t, socket_io::listen_t>,
		operation_t<M, raw_listen_socket_t, socket_io::listen_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>;

	using H = openssl_listen_socket_t;
	using N = H::native_type;
	using O = socket_io::listen_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	using _raw_listen = operation_t<M, raw_listen_socket_t, socket_io::listen_t>;
	using _raw_close = typename _base::template _raw_close<raw_listen_socket_t>;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		h = {};

		return submit_io(
			m,
			h,
			c,
			s._raw_state.emplace<_raw_listen>(),
			args,
			handler);
	}

	static io_result<void> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status)
	{
		vsm_assert(h.context == nullptr);

		// The server TLS context is created after successful raw listen.
		vsm_try_assign(h.context, openssl_context::create(openssl_mode::server));

		return {};
	}
};

template<typename M>
struct operation<M, openssl_listen_socket_t, openssl_listen_socket_t::accept_t>
	: openssl_operation_impl<
		raw_listen_socket_t,
		operation<M, openssl_listen_socket_t, socket_io::accept_t>,
		operation_t<M, raw_listen_socket_t, socket_io::accept_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>
{
	using _base = openssl_operation_impl<
		raw_listen_socket_t,
		operation<M, openssl_listen_socket_t, socket_io::accept_t>,
		operation_t<M, raw_listen_socket_t, socket_io::accept_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>;

	using H = openssl_listen_socket_t;
	using N = H::native_type const;
	using O = H::accept_t;
	using C = connector_t<M, H> const;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;
	using R = accept_result<async_handle<openssl_socket_t, multiplexer_handle_t<M>>>;

	using _raw_accept = operation_t<M, raw_listen_socket_t, socket_io::accept_t>;
	using _raw_close = typename _base::template _raw_close<raw_socket_t>;

	using _socket_handle_type = async_handle<openssl_socket_t, multiplexer_handle_t<M>>;

	openssl_socket_t::native_type _h;
	vsm_no_unique_address connector_t<M, openssl_socket_t> _c;
	network_endpoint _endpoint;

	static vsm::result<void> _accept_completed(S& s, auto&& r)
	{
		// The client TLS context is created after successful raw accept.
		vsm_try_assign(s._h.context, openssl_context::create(openssl_mode::client));

		r.socket.release(s._h, s._c);
		s._endpoint = r.endpoint;

		return {};
	}

	static io_result<void> _submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		s._h = {};

		vsm_try(r, submit_io(
			m,
			h,
			c,
			s._raw_state.emplace<_raw_accept>(),
			args,
			handler));

		vsm_try_void(_accept_completed(s, vsm_move(r)));

		return {};
	}

	using _base::_notify;

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_accept& raw_state)
	{
		vsm_try(r, notify_io(
			m,
			h,
			c,
			raw_state,
			args,
			status));

		vsm_try_void(_accept_completed(s, vsm_move(r)));

		return {};
	}

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_close& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			s._h,
			s._c,
			raw_state,
			io_args<object_t::close_t>()(),
			vsm_move(status)));

		return vsm::unexpected(raw_state.error);
	}

	static decltype(_h) const& _get_rw_h(N& h, S& s)
	{
		return s._h;
	}

	static decltype(_c) const& _get_rw_c(C& c, S& s)
	{
		return s._c;
	}

	static io_result<void> _continue(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
		{
			return h.context->accept();
		}));

		//TODO: ktls

		return {};
	}

	static io_result<void> _on_error(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler, std::error_code const e)
	{
		if (s._h.context != nullptr)
		{
			s._h.context->delete_context();
			s._h.context = nullptr;
		}

		if (s._h.flags[object_t::flags::not_null])
		{
			vsm_try_void(submit_io(
				m,
				s._h,
				s._c,
				s._raw_state.emplace<_raw_close>(e),
				io_args<object_t::close_t>()(),
				handler));
		}

		return vsm::unexpected(e);
	}

	static auto _get_result(M& m, S& s)
	{
		return vsm_lazy(R
		{
			_socket_handle_type
			(
				adopt_handle_t(),
				vsm_move(s._h),
				m,
				vsm_move(s._c)
			),
			s._endpoint,
		});
	}
};

template<typename M>
struct operation<M, openssl_listen_socket_t, openssl_listen_socket_t::close_t>
	: operation_t<M, raw_listen_socket_t, raw_listen_socket_t::close_t>
{
};

} // namespace allio::detail
