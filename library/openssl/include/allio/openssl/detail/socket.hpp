#pragma once

#include <allio/detail/byte_io_buffer_range.hpp>
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

template<typename RawSocket>
struct openssl_socket_t : socket_base_t<object_t>
{
	using base_type = object_t;

	using security_context_type = openssl_socket_security_context;

	template<operation_c Operation>
	static vsm::result<io_result_t<basic_detached_handle<openssl_socket_t>, Operation>> blocking_io(
		handle_const_t<Operation, native_handle<openssl_socket_t>>& h,
		io_parameters_t<openssl_socket_t, Operation> const& a)
	{
		return uniplexer_handle::blocking_io<openssl_socket_t, Operation>(h, a);
	}
};

template<typename RawSocket>
struct native_handle<openssl_socket_t<RawSocket>> : native_handle<RawSocket>
{
	openssl_socket* openssl;
};

template<typename M, typename RawSocket>
struct async_connector<M, openssl_socket_t<RawSocket>> : async_connector<M, RawSocket>
{
};

template<typename M, typename RawSocket>
struct openssl_raw_close : async_operation<M, RawSocket, close_t>
{
	std::error_code error;

	explicit openssl_raw_close(std::error_code const error)
		: error(error)
	{
	}
};

template<typename RawState>
struct openssl_raw_state
{
	using type = RawState;
};

template<typename M, typename RawSocket>
struct openssl_raw_state<async_operation<M, RawSocket, close_t>>
{
	using type = openssl_raw_close<M, RawSocket>;
};

template<
	typename M,
	typename Socket,
	typename Operation,
	typename RawSocket,
	typename... RawStates>
class openssl_operation : protected openssl_operation_base
{
protected:
	using H = handle_const_t<Operation, native_handle<Socket>>;
	using C = handle_const_t<Operation, async_connector_t<M, Socket>>;
	using S = async_operation<M, Socket, Operation>;
	using A = io_parameters_t<Socket, Operation>;
	using R = io_result_t<basic_attached_handle<Socket, multiplexer_handle_t<M>>, Operation>;

	using raw_read = async_operation_t<M, RawSocket, byte_io::stream_read_t>;
	using raw_write = async_operation_t<M, RawSocket, byte_io::stream_write_t>;

	template<typename RawState>
	using raw_state = typename openssl_raw_state<RawState>::type;

	std::variant<std::monostate, raw_read, raw_write, raw_state<RawStates>...> m_raw_state;

public:
	static io_result<R> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		// _submit checks the arguments, initializes the operation state and may submit underlying
		// raw operations.
		auto r = S::_submit(m, h, c, s, a, handler);

		if (r)
		{
			// _continue represents the body of the asynchronous loop. If the initial submission
			// completed synchronously, the loop body is entered to either complete the operation or
			// to submit further raw operations.
			r = S::_continue(m, h, c, s, a, handler);
		}

		// If an underlying raw operation is pending, the error is returned to the user and further
		// processing is continued once notified.
		if (detail::get_io_notify_status(r) == io_notify_status::submitted)
		{
			return vsm::unexpected(r.error());
		}

		if (!r)
		{
			// If after either the initial submission or after continuing, the operation completed
			// erroneously, _on_error is used to clean up the operation state (which may involve
			// submitting a close operation) and either propagate or discard the error.
			vsm_try_void(S::_on_error(m, h, c, s, a, handler, r.error()));
		}

		// Having completed the operation successfully, _get_result produces the final result.
		return S::_get_result(m, s);
	}

	static io_result<R> notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status)
	{
		vsm_assert(!std::holds_alternative<std::monostate>(s.m_raw_state));

		auto const visitor = [&]<typename RawState>(RawState& raw_state) -> io_result<void>
		{
			if constexpr (std::is_same_v<RawState, std::monostate>)
			{
				vsm_unreachable();
			}
			else
			{
				// First, the underlying raw operation is notified and upon completion its non-void
				// result (if any) is handled.
				auto r = S::_notify(m, h, c, s, a, vsm_move(status), raw_state);

				if constexpr (!vsm::is_instance_of_v<RawState, openssl_raw_close>)
				{
					if (r)
					{
						// Upon successful completion of the underlying raw operation, for any
						// non-close operation, the asynchronous loop body represented by _continue
						// is entered. In doing so, the operation may be completed or further raw
						// operations may be submitted, potentially causing the operation to enter
						// the submitted state and waiting to be notified again.
						r = S::_continue(m, h, c, s, a, handler);
					}
				}

				if (!r && r.error().get_io_notify_status() != io_notify_status::submitted)
				{
					// TODO: Ensure that errors from close are either not possible, or are handled
					//       appropriately.

					// Upon erroneous completion, _on_error is used to clean up the operation state
					// (which may involve submitting a close operation) and either propagate or
					// discard the error.
					r = S::_on_error(m, h, c, s, a, handler, r.error());
				}

				return r;
			}
		};

		if (auto const r = std::visit(visitor, s.m_raw_state))
		{
			return S::_get_result(m, s);
		}
		else
		{
			return vsm::unexpected(r.error());
		}
	}

	static void cancel(M& m, H const& h, C const& c, S& s)
	{
		// TODO: Implement openssl cancellation support.
	}

protected:
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

			if (rw_h.openssl->want_write())
			{
				auto rw_args = io_parameters_t<RawSocket, byte_io::stream_write_t>{};
				rw_args.buffers = rw_h.openssl->get_write_buffer();

				vsm_try(transferred, detail::submit_io(
					m,
					rw_h,
					rw_c,
					s.m_raw_state.template emplace<raw_write>(),
					rw_args,
					handler));

				rw_h.openssl->write_completed(transferred);
			}

			if (rw_h.openssl->want_read())
			{
				auto rw_args = io_parameters_t<RawSocket, byte_io::stream_read_t>{};
				rw_args.buffers = rw_h.openssl->get_read_buffer();

				vsm_try(transferred, detail::submit_io(
					m,
					rw_h,
					rw_c,
					s.m_raw_state.template emplace<raw_read>(),
					rw_args,
					handler));

				rw_h.openssl->read_completed(transferred);
			}
		}
	}


	template<typename RawState>
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		RawState& raw_state)
	{
		return detail::notify_io(m, h, c, raw_state, a, status);
	}

	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		raw_read& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);

		auto rw_args = io_parameters_t<RawSocket, byte_io::stream_read_t>{};
		rw_args.buffers = rw_h.openssl->get_read_buffer();

		vsm_try(transferred, detail::notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			rw_args,
			handler,
			vsm_move(status)));

		rw_h.openssl->read_completed(transferred);
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
		raw_write& raw_state)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);

		auto rw_args = io_parameters_t<RawSocket, byte_io::stream_write_t>{};
		rw_args.buffers = rw_h.openssl->get_write_buffer();

		vsm_try(transferred, detail::notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			rw_args,
			handler,
			vsm_move(status)));

		rw_h.openssl->write_completed(transferred);
		return {};
	}

	template<typename RawSocketOrListenSocket>
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		openssl_raw_close<M, RawSocketOrListenSocket>& raw_state) = delete;

	static io_result<void> _get_result(M&, S&)
	{
		static_assert(std::is_void_v<R>);
		return {};
	}
};

template<typename M, typename RawSocket, typename Operation, typename... RawOperations>
using openssl_socket_operation = openssl_operation<
	M,
	openssl_socket_t<RawSocket>,
	Operation,
	RawSocket,
	async_operation<M, RawSocket, RawOperations>...>;

template<typename M, typename RawSocket>
class async_operation<M, openssl_socket_t<RawSocket>, connect_t>
	: public openssl_socket_operation<M, RawSocket, connect_t, connect_t, close_t>
{
	using base = openssl_socket_operation<M, RawSocket, connect_t, connect_t, close_t>;

	using H = native_handle<openssl_socket_t<RawSocket>>;
	using C = async_connector_t<M, openssl_socket_t<RawSocket>>;
	using S = async_operation_t<M, openssl_socket_t<RawSocket>, connect_t>;
	using A = io_parameters_t<openssl_socket_t<RawSocket>, connect_t>;

	using raw_connect = async_operation_t<M, RawSocket, connect_t>;
	using raw_close = openssl_raw_close<M, RawSocket>;

	static vsm::result<void> _connect_completed(H& h, A const& a)
	{
		vsm_assert(a.security_context != nullptr);
		vsm_assert(h.openssl == nullptr);

		// The client TLS context is created after successful raw connect.
		vsm_try_assign(
			h.openssl,
			detail::new_openssl_socket(detail::openssl_get_ssl_ctx(*a.security_context)));

		return {};
	}

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		if (a.security_context == nullptr)
		{
			// TODO: Use a more specific error code?
			return vsm::unexpected(error::invalid_argument);
		}

		h = {};

		vsm_try_void(detail::submit_io(
			m,
			h,
			c,
			s.m_raw_state.template emplace<raw_connect>(),
			a,
			handler));

		vsm_try_void(_connect_completed(h, a));

		return {};
	}

	// TODO: Is disconnecting needed when an error occurs?
	// Notify while the raw close operation is active:
	using base::_notify;

	// Notify while the raw connect operation is active:
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		M::io_status_type&& status,
		raw_connect& raw_state)
	{
		// Notify the underlying raw operation:
		vsm_try_void(detail::notify_io(
			m,
			h,
			c,
			raw_state,
			a,
			vsm_move(status)));

		// Handle successful raw connect completion:
		vsm_try_void(_connect_completed(h, a));

		return {};
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler)
	{
		vsm_assert(h.openssl != nullptr);

		return base::_enter(m, h, c, s, handler, [&]()
		{
			return h.openssl->connect();
		});
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
		if (h.openssl != nullptr)
		{
			detail::delete_openssl_socket(h.openssl);
			h.openssl = nullptr;
		}
	
		if (h.flags[object_t::flags::not_null])
		{
			vsm_try_void(detail::submit_io(
				m,
				h,
				c,
				s.m_raw_state.template emplace<raw_close>(e),
				make_args<io_parameters_t<RawSocket, close_t>>(),
				handler));
		}

		return vsm::unexpected(e);
	}

	friend base;
};

template<
	typename M,
	typename RawSocket,
	vsm::any_of<byte_io::stream_read_t, byte_io::stream_write_t> Operation>
class async_operation<M, openssl_socket_t<RawSocket>, Operation>
	: public openssl_socket_operation<M, RawSocket, Operation>
{
	using base = openssl_socket_operation<M, RawSocket, Operation>;

	using H = native_handle<openssl_socket_t<RawSocket>> const;
	using C = async_connector_t<M, openssl_socket_t<RawSocket>> const;
	using S = async_operation_t<M, openssl_socket_t<RawSocket>, Operation>;
	using A = io_parameters_t<openssl_socket_t<RawSocket>, Operation>;

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
		using byte_type = vsm::select_t<
			std::is_same_v<Operation, byte_io::stream_read_t>,
			std::byte,
			std::byte const>;

		auto const buffer_layout = a.buffers.get_layout();
		auto const buffers = read_io_buffers(a.buffers.get_buffers());
		auto const buffers_size = static_cast<size_t>(buffers.size());

		while (s.buffer_index < buffers_size)
		{
			auto const buffer = get_io_buffer_span<byte_type>(
				buffers[static_cast<ptrdiff_t>(s.buffer_index)],
				buffer_layout);

			vsm_assert(s.buffer_offset < buffer.size());

			auto const remaining_data = buffer.data() + s.buffer_offset;
			auto const remaining_size = buffer.size() - s.buffer_offset;

			vsm_try(transferred, base::_enter(m, h, c, s, handler, [&]()
			{
				if constexpr (std::is_same_v<Operation, byte_io::stream_read_t>)
				{
					return h.openssl->read_some(read_buffer(remaining_data, remaining_size));
				}
				else
				{
					return h.openssl->write_some(write_buffer(remaining_data, remaining_size));
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

	friend base;
};

template<multiplexer M, typename RawSocket>
struct async_operation<M, openssl_socket_t<RawSocket>, close_t>
	: async_operation_t<M, RawSocket, close_t>
{
	using H = native_handle<openssl_socket_t<RawSocket>>;
	using C = async_connector_t<M, openssl_socket_t<RawSocket>>;
	using S = async_operation_t<M, openssl_socket_t<RawSocket>, close_t>;
	using A = io_parameters_t<openssl_socket_t<RawSocket>, close_t>;

	static io_result<void> submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		vsm_assert(h.openssl != nullptr);

		detail::delete_openssl_socket(h.openssl);
		h.openssl = nullptr;

		return detail::submit_io(
			m,
			static_cast<native_handle<RawSocket>&>(h),
			static_cast<async_connector_t<M, RawSocket>&>(c),
			static_cast<async_operation_t<M, RawSocket, close_t>&>(s),
			a,
			handler);
	}
};

} // namespace allio::detail
