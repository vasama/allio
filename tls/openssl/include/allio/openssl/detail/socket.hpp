#pragma once

#include <allio/detail/blocking_multiplexer.hpp>
#include <allio/detail/handles/socket.hpp>
#include <allio/openssl/detail/openssl.hpp>

#include <variant>

namespace allio::detail {

class openssl_socket_security_context : public openssl_security_context
{
public:
	static vsm::result<openssl_socket_security_context> create(
		security_context_parameters const& a)
	{
		vsm_try(ssl_ctx, create_client_ssl_ctx(a));
		return vsm_lazy(openssl_socket_security_context(vsm_move(ssl_ctx)));
	}

private:
	using openssl_security_context::openssl_security_context;
};

struct openssl_socket_t : basic_socket_t<object_t>
{
	using security_context_type = openssl_socket_security_context;

	using base_type = basic_socket_t<object_t>;

	struct native_type : raw_socket_t::native_type
	{
		openssl_ssl* ssl;
	};

	template<operation_c Operation>
	friend vsm::result<io_result_t<openssl_socket_t, Operation>> tag_invoke(
		blocking_io_t<openssl_socket_t, Operation>,
		handle_const_t<Operation, native_type>& h,
		io_parameters_t<openssl_socket_t, Operation> const& a)
	{
		return blocking_multiplexer_handle::blocking_io<openssl_socket_t, Operation>(h, a);
	}
};

template<multiplexer M>
struct async_connector<M, openssl_socket_t>
	: async_connector<M, raw_socket_t>
{
};

template<object RawSocket, typename Implementation, typename... RawStates>
struct openssl_operation;

template<object RawSocket, typename M, object Socket, operation_c Operation, typename... RawStates>
struct openssl_operation<RawSocket, async_operation<M, Socket, Operation>, RawStates...>
	: async_operation_base
{
	using H = handle_const_t<Operation, typename Socket::native_type>;
	using C = handle_const_t<Operation, async_connector_t<M, Socket>>;
	using S = async_operation<M, Socket, Operation>;
	using A = io_parameters_t<Socket, Operation>;
	using R = io_result_t<Socket, Operation, multiplexer_handle_t<M>>;

	using _raw_read = async_operation_t<M, raw_socket_t, byte_io::stream_read_t>;
	using _raw_write = async_operation_t<M, raw_socket_t, byte_io::stream_write_t>;

	template<typename RawObject>
	struct _raw_close : async_operation_t<M, RawObject, close_t>
	{
		std::error_code error;

		explicit _raw_close(std::error_code const error)
			: error(error)
		{
		}
	};

	template<typename RawOperation>
	struct _raw
	{
		using type = RawOperation;
	};

	template<typename RawObject>
	struct _raw<async_operation_t<M, RawObject, close_t>>
	{
		using type = _raw_close<RawObject>;
	};

	io_handler<M>* _handler;
	std::variant<
		std::monostate,
		typename _raw<RawStates>::type...
	> _raw_state;


	static io_result<R> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s._handler = &handler;

		auto r = S::_submit(m, h, c, s, a, handler);

		if (r)
		{
			r = S::_continue(m, h, c, s, a, handler);
		}

		if (!r)
		{
			vsm_try_void(S::_on_error(m, h, c, s, a, handler, r.error()));
		}

		return S::_get_result(m, s);
	}

	static io_result<R> notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status)
	{
		io_handler<M>& handler = *s._handler;

		vsm_try_void(std::visit([&]<typename RawState>(RawState& raw_state) -> io_result<void>
		{
			if constexpr (std::is_same_v<RawState, std::monostate>)
			{
				vsm_unreachable();
			}
			else
			{
				auto r = S::_notify(m, h, c, s, a, vsm_move(status), raw_state);

				if constexpr (!vsm::is_instance_of_v<RawState, _raw_close>)
				{
					if (r)
					{
						r = S::_continue(m, h, c, s, a, handler);
					}

					if (!r)
					{
						r = S::_on_error(m, h, c, s, a, handler, r.error());
					}
				}

				return r;
			}
		}, s._raw_state));

		return S::_get_result(m, s);
	}

	static void cancel(M& m, H const& h, C const& c, S& s)
	{
#if 0 //TODO: Cancellation thread safety.
		std::visit([&]<typename RawState>(RawState& raw_state)
		{
			if constexpr (std::is_same_v<RawState, std::monostate>)
			{
				//TODO: Is this unreachable?
			}
			else
			{
				cancel_io(m, h, c, raw_state);
			}
		}, s._raw_state);
#endif
	}


	static auto const& _get_rw_h(H& h, S&)
	{
		return h;
	}

	static auto const& _get_rw_c(C& c, S&)
	{
		return c;
	}

	template<typename Callable>
	static auto _enter(M& m, H& h, C& c, S& s, io_handler<M>& handler, Callable&& callable)
		-> io_result<typename std::invoke_result_t<Callable>::value_type::value_type>
	{
		using value_type = typename std::invoke_result_t<Callable>::value_type::value_type;

		while (true)
		{
			vsm_try(r, callable());

			if (r)
			{
				if constexpr (std::is_void_v<value_type>)
				{
					return {};
				}
				else
				{
					return vsm_move(*r);
				}
			}

			auto& rw_h = S::_get_rw_h(h, s);
			auto& rw_c = S::_get_rw_c(c, s);

			switch (r.error())
			{
			case openssl_request_kind::read:
				{
					vsm_try(transferred, submit_io(
						m,
						rw_h,
						rw_c,
						s._raw_state.emplace<_raw_read>(),
						make_io_args<RawSocket, byte_io::stream_read_t>(rw_h.ssl->get_read_buffer())(),
						handler));
					rw_h.ssl->read_completed(transferred);
				}
				break;

			case openssl_request_kind::write:
				{
					vsm_try(transferred, submit_io(
						m,
						rw_h,
						rw_c,
						s._raw_state.emplace<_raw_write>(),
						make_io_args<RawSocket, byte_io::stream_write_t>(rw_h.ssl->get_write_buffer())(),
						handler));
					rw_h.ssl->write_completed(transferred);
				}
				break;
			}
		}
	}


	template<typename RawState>
	static io_result<void> _notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status, RawState& raw_state)
	{
		return notify_io(m, h, c, raw_state, a, status);
	}

	static io_result<void> _notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status, _raw_read& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);
		vsm_try(transferred, notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			make_io_args<RawSocket, byte_io::stream_read_t>(rw_h.ssl->get_read_buffer())(),
			vsm_move(status)));
		rw_h.ssl->read_completed(transferred);
		return {};
	}

	static io_result<void> _notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status, _raw_write& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);
		vsm_try(transferred, notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			make_io_args<RawSocket, byte_io::stream_write_t>(rw_h.ssl->get_write_buffer())(),
			vsm_move(status)));
		rw_h.ssl->write_completed(transferred);
		return {};
	}

	static io_result<void> _notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status, _raw_close<RawSocket>& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			h,
			c,
			raw_state,
			make_io_args<RawSocket, close_t>()(),
			vsm_move(status)));

		h.platform_handle = native_platform_handle::null;

		return vsm::unexpected(raw_state.error);
	}

	static void _delete(H& h)
	{
		if (h.ssl != nullptr)
		{
			h.ssl->delete_context();
			h.ssl = nullptr;
		}
	}

	static io_result<void> _on_error(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler, std::error_code const e)
	{
		S::_delete(h);

		if (h.platform_handle != native_platform_handle::null)
		{
			vsm_try_void(submit_io(
				m,
				h,
				c,
				s._raw_state.emplace<_raw_close<RawSocket>>(e),
				make_io_args<RawSocket, close_t>()(),
				handler));

			h.platform_handle = native_platform_handle::null;
		}

		return vsm::unexpected(e);
	}

	static io_result<void> _get_result(M&, S&) requires std::is_void_v<R>
	{
		return {};
	}
};

template<multiplexer M>
struct async_operation<M, openssl_socket_t, socket_io::connect_t>
	: openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, socket_io::connect_t>,
		async_operation_t<M, raw_socket_t, socket_io::connect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, socket_io::connect_t>,
		async_operation_t<M, raw_socket_t, socket_io::connect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>;

	using H = openssl_socket_t::native_type;
	using C = async_connector_t<M, openssl_socket_t>;
	using S = async_operation_t<M, openssl_socket_t, socket_io::connect_t>;
	using A = io_parameters_t<openssl_socket_t, socket_io::connect_t>;

	using _raw_connect = async_operation_t<M, raw_socket_t, socket_io::connect_t>;

	static vsm::result<void> _connect_completed(H& h, A const& a)
	{
		vsm_assert(a.security_context != nullptr);
		vsm_assert(h.ssl == nullptr);

		auto const ssl_ctx = openssl_get_ssl_ctx(*a.security_context);

		// The client TLS context is created after successful raw connect.
		vsm_try_assign(h.ssl, openssl_ssl::create(ssl_ctx));

		return {};
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
			s._raw_state.emplace<_raw_connect>(),
			a,
			handler));

		vsm_try_void(_connect_completed(h, a));

		return {};
	}

	using _base::_notify;

	static io_result<void> _notify(M& m, H& h, C& c, S& s, A const& a, M::io_status_type&& status, _raw_connect& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			h,
			c,
			raw_state,
			a,
			vsm_move(status)));

		vsm_try_void(_connect_completed(h, a));

		return {};
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler)
	{
		vsm_assert(h.ssl != nullptr);

		vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
		{
			return h.ssl->connect();
		}));

		return {};
	}
};

template<multiplexer M>
struct async_operation<M, openssl_socket_t, socket_io::disconnect_t>
	: openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, socket_io::disconnect_t>,
		async_operation_t<M, raw_socket_t, socket_io::disconnect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, socket_io::disconnect_t>,
		async_operation_t<M, raw_socket_t, socket_io::disconnect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>;

	using H = openssl_socket_t::native_type;
	using C = async_connector_t<M, openssl_socket_t>;
	using S = async_operation_t<M, openssl_socket_t, socket_io::disconnect_t>;
	using A = io_parameters_t<openssl_socket_t, socket_io::disconnect_t>;

	using _raw_disconnect = async_operation_t<M, raw_socket_t, socket_io::disconnect_t>;

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		vsm_assert(h.ssl != nullptr);

		return {};
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		if (h.ssl != nullptr)
		{
			vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
			{
				return h.ssl->disconnect();
			}));
		}

		if (h.flags[object_t::flags::not_null])
		{
			vsm_try_void(submit_io(
				m,
				h,
				c,
				s._raw_state.emplace<_raw_disconnect>(),
				make_io_args<raw_socket_t, socket_io::disconnect_t>()(),
				handler));
		}

		return {};
	}
};

template<multiplexer M, vsm::any_of<byte_io::stream_read_t, byte_io::stream_write_t> Operation>
struct async_operation<M, openssl_socket_t, Operation>
	: openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, Operation>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>>
{
	using _base = openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, Operation>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>>;

	using H = openssl_socket_t::native_type const;
	using C = async_connector_t<M, openssl_socket_t> const;
	using S = async_operation_t<M, openssl_socket_t, Operation>;
	using A = io_parameters_t<openssl_socket_t, Operation>;

	size_t buffer_index;
	size_t buffer_offset;
	size_t transferred;

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s.buffer_index = 0;
		s.buffer_offset = 0;
		s.transferred = 0;

		return _continue(m, h, c, s, a, handler);
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		auto const buffers = a.buffers.buffers();
		for (; s.buffer_index < buffers.size(); ++s.buffer_index)
		{
			auto const buffer = buffers[s.buffer_index];
			vsm_assert(s.buffer_offset < buffer.size());

			auto const remaining_data = buffer.data() + s.buffer_offset;
			auto const remaining_size = buffer.size() - s.buffer_offset;

			vsm_try(transferred, _base::_enter(m, h, c, s, handler, [&]()
			{
				if constexpr (std::is_same_v<Operation, byte_io::stream_read_t>)
				{
					return h.ssl->read(remaining_data, remaining_size);
				}
				else
				{
					return h.ssl->write(remaining_data, remaining_size);
				}
			}));
			vsm_assert(transferred <= remaining_size);

			if (transferred == remaining_size)
			{
				++s.buffer_index;
				s.buffer_offset = 0;
			}
			else
			{
				s.buffer_offset += transferred;
			}
		}
		return {};
	}

	static io_result<void> _on_error(M&, H&, C&, S& s, A const&, io_handler<M>&, std::error_code const e)
	{
		if (s.transferred != 0)
		{
			return {};
		}

		return vsm::unexpected(e);
	}

	static size_t _get_result(M&, S& s)
	{
		return s.transferred;
	}
};

template<multiplexer M>
struct async_operation<M, openssl_socket_t, close_t>
	: async_operation_t<M, raw_socket_t, close_t>
{
	using H = openssl_socket_t::native_type;
	using C = async_connector_t<M, openssl_socket_t>;
	using S = async_operation_t<M, openssl_socket_t, close_t>;
	using A = io_parameters_t<openssl_socket_t, close_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		vsm_assert(h.ssl != nullptr);

		h.ssl->delete_context();
		h.ssl = nullptr;

		return submit_io(
			m,
			static_cast<raw_socket_t::native_type&>(h),
			static_cast<async_connector_t<M, raw_socket_t>&>(c),
			static_cast<async_operation_t<M, raw_socket_t, close_t>&>(s),
			a,
			handler);
	}
};

} // namespace allio::detail
