#pragma once

#include <allio/detail/handles/raw_socket.hpp>
#include <allio/detail/uniplexer.hpp>
#include <allio/openssl/detail/openssl.hpp>

#include <vsm/lazy.hpp>

#include <variant>

namespace allio::detail {

class openssl_socket_security_context : public openssl_security_context
{
public:
	static vsm::result<openssl_socket_security_context> create(
		security_context_parameters const& a)
	{
		vsm_try(ssl_ctx, openssl_create_client_ssl_ctx(a));
		return vsm_lazy(openssl_socket_security_context(vsm_move(ssl_ctx)));
	}

private:
	using openssl_security_context::openssl_security_context;
};

struct openssl_socket_t;

template<>
struct native_handle<openssl_socket_t> : native_handle<raw_socket_t>
{
	openssl_state_base* ssl;
};

struct openssl_socket_t : socket_base_t<object_t>
{
	using base_type = raw_socket_t;

	using security_context_type = openssl_socket_security_context;

	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<openssl_socket_t>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<openssl_socket_t>>& h,
		io_parameters_t<openssl_socket_t, Operation> const& a)
	{
		return uniplexer_handle::blocking_io<openssl_socket_t, Operation>(h, a);
	}
};

template<multiplexer M>
struct async_connector<M, openssl_socket_t>
	: async_connector<M, raw_socket_t>
{
};

template<typename... RawStates>
struct openssl_operation_storage
{
	static constexpr size_t size = std::max({ sizeof(RawStates)... });
	static constexpr size_t alignment = std::max({ alignof(RawStates)... });

	alignas(alignment) unsigned char m_storage[size];

	template<typename T, typename... Args>
	[[nodiscard]] T& emplace(Args&&... args)
	{
		return *::new (m_storage) T(vsm_forward(args)...);
	}

	template<typename T>
	[[nodiscard]] T& get()
	{
		return *std::launder(reinterpret_cast<T*>(m_storage));
	}

	template<typename T>
	[[nodiscard]] static openssl_operation_storage& from(T& object)
	{
		return *reinterpret_cast<openssl_operation_storage*>(
			std::launder(reinterpret_cast<decltype(m_storage)*>(object)));
	}
};

template<object RawSocket, typename Implementation, typename... RawStates>
struct openssl_operation;

template<object RawSocket, typename M, object Socket, operation_c Operation, typename... RawStates>
struct openssl_operation<RawSocket, async_operation<M, Socket, Operation>, RawStates...>
	: openssl_operation_base
{
	using H = handle_const_t<Operation, native_handle<Socket>>;
	using C = handle_const_t<Operation, async_connector_t<M, Socket>>;
	using S = async_operation<M, Socket, Operation>;
	using A = io_parameters_t<Socket, Operation>;
	using R = io_result_t<basic_attached_handle<Socket, multiplexer_handle_t<M>>, Operation>;

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

	io_handler<M>* m_handler;
	std::variant<
		std::monostate,
		typename _raw<RawStates>::type...
	> m_raw_state;


	static io_result<R> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s.m_handler = &handler;

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
		io_handler<M>& handler = *s.m_handler;

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
		}, s.m_raw_state));

		return S::_get_result(m, s);
	}

	static void cancel(M& m, H const& h, C const& c, S& s)
	{
#if 0 //TODO: Cancelation thread safety.
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
		}, s.m_raw_state);
#endif
	}


	static io_result<void> _synchronize(H& h)
	{
		openssl_state_base* const state = h.ssl;

		if (state->m_queue.push_back(this))
		{

		}

		return io_pending(error::operation_pending);
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

			if (rw_h.ssl->m_want_write)
			{
				auto a = io_parameters_t<RawSocket, byte_io::stream_write_t>{};
				a.buffers = rw_h.ssl->get_write_buffer();

				vsm_try(transferred, detail::submit_io(
					m,
					rw_h,
					rw_c,
					s.m_raw_state.emplace<_raw_write>(),
					a,
					handler));

				rw_h.ssl->write_completed(transferred);
			}

			if (rw_h.ssl->m_want_read)
			{
				auto a = io_parameters_t<RawSocket, byte_io::stream_read_t>{};
				a.buffers = rw_h.ssl->get_read_buffer();

				vsm_try(transferred, detail::submit_io(
					m,
					rw_h,
					rw_c,
					s.m_raw_state.emplace<_raw_read>(),
					a,
					handler));

				rw_h.ssl->read_completed(transferred);
			}

#if 0
			switch (r.error())
			{
			case openssl_request_kind::read:
				{
					vsm_try(transferred, submit_io(
						m,
						rw_h,
						rw_c,
						s.m_raw_state.emplace<_raw_read>(),
						make_args<io_parameters_t<RawSocket, byte_io::stream_read_t>>(
							rw_h.ssl->get_read_buffer())(),
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
						s.m_raw_state.emplace<_raw_write>(),
						make_args<io_parameters_t<RawSocket, byte_io::stream_write_t>>(
							rw_h.ssl->get_write_buffer())(),
						handler));

					rw_h.ssl->write_completed(transferred);
				}
				break;
			}
#endif
		}
	}


	template<typename RawState>
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		RawState& raw_state)
	{
		return notify_io(m, h, c, raw_state, a, status);
	}

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		_raw_read& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);

		auto a = io_parameters_t<RawSocket, byte_io::stream_read_t>{};
		a.buffers = rw_h.ssl->get_read_buffer();

		vsm_try(transferred, notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			a,
			vsm_move(status)));
		rw_h.ssl->read_completed(transferred);
		return {};
	}

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		_raw_write& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);

		auto a = io_parameters_t<RawSocket, byte_io::stream_write_t>{};
		a.buffers = rw_h.ssl->get_write_buffer();

		vsm_try(transferred, notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			a,
			vsm_move(status)));
		rw_h.ssl->write_completed(transferred);
		return {};
	}

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		_raw_close<RawSocket>& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			h,
			c,
			raw_state,
			make_args<io_parameters_t<RawSocket, close_t>>(),
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

	static io_result<void> _on_error(
		M& m,
		H& h,
		C& c,
		S& s,
		A const&,
		io_handler<M>& handler,
		std::error_code const e)
	{
		S::_delete(h);

		if (h.platform_handle != native_platform_handle::null)
		{
			vsm_try_void(submit_io(
				m,
				h,
				c,
				s.m_raw_state.emplace<_raw_close<RawSocket>>(e),
				make_args<io_parameters_t<RawSocket, close_t>>(),
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
struct async_operation<M, openssl_socket_t, connect_t>
	: openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, connect_t>,
		async_operation_t<M, raw_socket_t, connect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, connect_t>,
		async_operation_t<M, raw_socket_t, connect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>;

	using H = native_handle<openssl_socket_t>;
	using C = async_connector_t<M, openssl_socket_t>;
	using S = async_operation_t<M, openssl_socket_t, connect_t>;
	using A = io_parameters_t<openssl_socket_t, connect_t>;

	using _raw_connect = async_operation_t<M, raw_socket_t, connect_t>;

	static vsm::result<void> _connect_completed(H& h, A const& a)
	{
		vsm_assert(a.security_context != nullptr);
		vsm_assert(h.ssl == nullptr);

		// The client TLS context is created after successful raw connect.
		vsm_try_assign(h.ssl, openssl_state::create(
			detail::openssl_get_ssl_ctx(*a.security_context)));

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
			s.m_raw_state.emplace<_raw_connect>(),
			a,
			handler));

		vsm_try_void(_connect_completed(h, a));

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
		_raw_connect& raw_state)
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
struct async_operation<M, openssl_socket_t, disconnect_t>
	: openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, disconnect_t>,
		async_operation_t<M, raw_socket_t, disconnect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>
{
	using _base = openssl_operation<
		raw_socket_t,
		async_operation<M, openssl_socket_t, disconnect_t>,
		async_operation_t<M, raw_socket_t, disconnect_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_read_t>,
		async_operation_t<M, raw_socket_t, byte_io::stream_write_t>,
		async_operation_t<M, raw_socket_t, close_t>>;

	using H = native_handle<openssl_socket_t>;
	using C = async_connector_t<M, openssl_socket_t>;
	using S = async_operation_t<M, openssl_socket_t, disconnect_t>;
	using A = io_parameters_t<openssl_socket_t, disconnect_t>;

	using _raw_disconnect = async_operation_t<M, raw_socket_t, disconnect_t>;

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
				s.m_raw_state.emplace<_raw_disconnect>(),
				make_args<io_parameters_t<raw_socket_t, disconnect_t>>(),
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

	using H = native_handle<openssl_socket_t> const;
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

		vsm_try_void(_base::_synchronize(h));

		return _continue(m, h, c, s, a, handler);
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		auto const buffers = a.buffers.buffers();
		while (s.buffer_index < buffers.size())
		{
			auto const buffer = buffers[s.buffer_index];
			vsm_assert(s.buffer_offset < buffer.size());

			auto const remaining_data = buffer.data() + s.buffer_offset;
			auto const remaining_size = buffer.size() - s.buffer_offset;

			vsm_try(transferred, _base::_enter(m, h, c, s, handler, [&]()
			{
				if constexpr (std::is_same_v<Operation, byte_io::stream_read_t>)
				{
					return h.ssl->read(read_buffer(remaining_data, remaining_size));
					//return h.ssl->read(remaining_data, remaining_size);
				}
				else
				{
					return h.ssl->write(write_buffer(remaining_data, remaining_size));
					//return h.ssl->write(remaining_data, remaining_size);
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

	static io_result<void> _on_error(
		M&,
		H&,
		C&,
		S& s,
		A const&,
		io_handler<M>&,
		std::error_code const e)
	{
		if (s.transferred != 0)
		{
			return {};
		}

		return vsm::unexpected(e);
	}

	static io_result<size_t> _get_result(M&, S& s)
	{
		return s.transferred;
	}
};

template<multiplexer M>
struct async_operation<M, openssl_socket_t, close_t>
	: async_operation_t<M, raw_socket_t, close_t>
{
	using H = native_handle<openssl_socket_t>;
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
			static_cast<native_handle<raw_socket_t>&>(h),
			static_cast<async_connector_t<M, raw_socket_t>&>(c),
			static_cast<async_operation_t<M, raw_socket_t, close_t>&>(s),
			a,
			handler);
	}
};

} // namespace allio::detail
