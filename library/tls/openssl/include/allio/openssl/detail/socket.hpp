#pragma once

#include <allio/detail/byte_io_buffer_range.hpp>
#include <allio/detail/byte_io_state.hpp>
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
	static constexpr auto& blocking_io = uniplexer_handle::blocking_io<openssl_socket_t, Operation>;
};

template<typename RawSocket>
struct native_handle<openssl_socket_t<RawSocket>> : native_handle<RawSocket>
{
	openssl_tunnel_state* openssl;
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

template<typename M, typename Socket, typename Operation, typename RawSocket, typename... RawStates>
class openssl_operation
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

	using internal_result_type = void;

	std::variant<std::monostate, raw_state<RawStates>...> m_raw_state;

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

		// Having completed the operation, successfully or not, _finalize produces the final result.
		//
		// Some operations submit a raw close as part of finalization, which may cause the tunneled
		// operation to enter a pending state once more. Notification in the case of close does not
		// invoke _continue again, however.
		//
		// Some operations discard errors during finalization in case of partial success.
		return S::_finalize(m, h, c, s, a, handler, vsm_move(r));
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

		auto const visitor = [&]<typename RawState>(RawState& raw_state)
			-> io_result<typename S::internal_result_type>
		{
			if constexpr (std::is_same_v<RawState, std::monostate>)
			{
				vsm_unreachable();
			}
			else
			{
				// First, the underlying raw operation is notified and upon completion its non-void
				// result (if any) is handled.
				vsm_try_void(S::_notify(m, h, c, s, a, handler, vsm_move(status), raw_state));

				if constexpr (vsm::is_instance_of_v<RawState, openssl_raw_close>)
				{
					static_assert(std::is_void_v<typename S::internal_result_type>);
					return {};
				}
				else
				{
					// Upon successful completion of the underlying raw operation, for any non-close
					// operation, the asynchronous loop body represented by _continue is entered. In
					// doing so, the operation may be completed or further raw operations may be
					// submitted, potentially causing the operation to enter the submitted state and
					// waiting to be notified again.
					return S::_continue(m, h, c, s, a, handler);
				}
			}
		};

		auto r = std::visit(visitor, s.m_raw_state);

		if (detail::get_io_notify_status(r) == io_notify_status::submitted)
		{
			return vsm::unexpected(r.error());
		}

		return S::_finalize(m, h, c, s, a, handler, vsm_move(r));
	}

	static void cancel(M& m, H const& h, C const& c, S& s, A const&, io_handler<M> const&)
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

	static io_result<void> _submit_rw(
		M& m,
		H& h,
		C& c,
		S& s,
		io_handler<M>& handler)
	{
		auto& rw_h = S::_get_rw_h(h, s);
		auto& rw_c = S::_get_rw_c(c, s);

		while (rw_h.openssl->write_needed())
		{
			auto rw_args = io_parameters_t<RawSocket, byte_io::stream_write_t>{};
			rw_args.buffers = rw_h.openssl->get_write_buffer();

			auto const r = detail::submit_io(
				m,
				rw_h,
				rw_c,
				s.m_raw_state.template emplace<raw_write>(),
				rw_args,
				handler);

			if (detail::get_io_notify_status(r) == io_notify_status::submitted)
			{
				return vsm::unexpected(r.error());
			}

			rw_h.openssl->write_completed(r);
		}

		if (rw_h.openssl->read_needed())
		{
			auto rw_args = io_parameters_t<RawSocket, byte_io::stream_read_t>{};
			rw_args.buffers = rw_h.openssl->get_read_buffer();

			auto const r = detail::submit_io(
				m,
				rw_h,
				rw_c,
				s.m_raw_state.template emplace<raw_read>(),
				rw_args,
				handler);

			if (detail::get_io_notify_status(r) == io_notify_status::submitted)
			{
				return vsm::unexpected(r.error());
			}

			rw_h.openssl->read_completed(r);
		}

		return {};
	}

	using enter_result = vsm::result<tunnel_result<void>>;

	template<typename Callable>
	static io_result<void> _enter(
		M& m,
		H& h,
		C& c,
		S& s,
		io_handler<M>& handler,
		Callable&& callable)
	{
		static_assert(std::is_same_v<std::invoke_result_t<Callable>, enter_result>);

		while (true)
		{
			vsm_try(r_enter, callable());

			vsm_try_void(_submit_rw(m, h, c, s, handler));

			if (r_enter)
			{
				return {};
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
		return detail::notify_io(
			m,
			h,
			c,
			raw_state,
			a,
			handler,
			vsm_move(status));
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

		auto const r = detail::notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			rw_args,
			handler,
			vsm_move(status));

		if (detail::get_io_notify_status(r) == io_notify_status::submitted)
		{
			return vsm::unexpected(r.error());
		}

		rw_h.openssl->read_completed(r);

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

		auto const r = detail::notify_io(
			m,
			rw_h,
			rw_c,
			raw_state,
			rw_args,
			handler,
			vsm_move(status));

		if (detail::get_io_notify_status(r) == io_notify_status::submitted)
		{
			return vsm::unexpected(r.error());
		}

		rw_h.openssl->write_completed(r);

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

	template<typename T>
	static io_result<T> _finalize(
		M&,
		H&,
		C&,
		S&,
		A const&,
		io_handler<M>&,
		io_result<T>&& r)
	{
		return vsm_move(r);
	}
};

template<typename M, typename Socket, typename Operation, typename RawSocket, typename... RawStates>
using openssl_rw_operation = openssl_operation<
	M,
	Socket,
	Operation,
	RawSocket,
	RawStates...,
	async_operation_t<M, RawSocket, byte_io::stream_read_t>,
	async_operation_t<M, RawSocket, byte_io::stream_write_t>>;

template<typename M, typename RawSocket, typename Operation, typename... RawOperations>
using openssl_socket_operation = openssl_rw_operation<
	M,
	openssl_socket_t<RawSocket>,
	Operation,
	RawSocket,
	async_operation<M, RawSocket, RawOperations>...>;

template<typename M, typename RawSocket>
struct async_operation<M, openssl_socket_t<RawSocket>, connect_t>
	: openssl_socket_operation<M, RawSocket, connect_t, connect_t, close_t>
{
private:
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

		vsm_try(tunnel, detail::make_unique<openssl_tunnel_state>());

		vsm_try_void(tunnel->allocate_buffers(4096, 4096));
		vsm_try_void(tunnel->initialize(
			tunnel_mode::client,
			detail::openssl_get_ssl_ctx(*a.security_context)));

		h.openssl = tunnel.release();

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
	// Notify while a raw read or write operation is active:
	using base::_notify;

	// Notify while the raw connect operation is active:
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
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
			handler,
			vsm_move(status)));

		// Handle successful raw connect completion:
		vsm_try_void(_connect_completed(h, a));

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
			h,
			c,
			raw_state,
			make_args<io_parameters_t<RawSocket, close_t>>(),
			handler,
			vsm_move(status)));

		return vsm::unexpected(raw_state.error);
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const&, io_handler<M>& handler)
	{
		vsm_assert(h.openssl != nullptr);

		return base::_enter(m, h, c, s, handler, [&]() -> typename base::enter_result
		{
			if (h.openssl->is_tunnel_connected())
			{
				return {};
			}

			return h.openssl->connect(tunnel_mode::client);
		});
	}

	static io_result<void> _finalize(
		M& m,
		H& h,
		C& c,
		S& s,
		A const&,
		io_handler<M>& handler,
		io_result<void> const& r)
	{
		if (!r)
		{
			if (h.openssl != nullptr)
			{
				detail::delete_object(h.openssl);
				h.openssl = nullptr;
			}

			if (h.flags[object_t::flags::not_null])
			{
				vsm_try_void(detail::submit_io(
					m,
					h,
					c,
					s.m_raw_state.template emplace<raw_close>(r.error()),
					make_args<io_parameters_t<RawSocket, close_t>>(),
					handler));
			}
		}

		return r;
	}

	friend base;
};

template<typename M, typename RawSocket>
struct async_operation<M, openssl_socket_t<RawSocket>, disconnect_t>
	: openssl_socket_operation<M, RawSocket, disconnect_t, disconnect_t>
{
private:
	using base = openssl_socket_operation<M, RawSocket, disconnect_t, disconnect_t>;

	using H = native_handle<openssl_socket_t<RawSocket>> const;
	using C = async_connector_t<M, openssl_socket_t<RawSocket>> const;
	using S = async_operation_t<M, openssl_socket_t<RawSocket>, disconnect_t>;
	using A = io_parameters_t<openssl_socket_t<RawSocket>, disconnect_t>;

	using raw_disconnect = async_operation_t<M, RawSocket, disconnect_t>;

	static io_parameters_t<RawSocket, disconnect_t> make_raw_disconnect_args(
		H const& h,
		A const& a)
	{
		auto raw_a = static_cast<io_parameters_t<RawSocket, disconnect_t> const&>(a);

		auto const mode_mask = h.openssl->is_tunnel_connected()
			? disconnect_mode::write
			: disconnect_mode::write | disconnect_mode::read;

		raw_a.mode = mode_mask & raw_a.mode.value_or(disconnect_mode::both);
		vsm_assert(raw_a.mode.value_or_zero() != disconnect_mode());

		return raw_a;
	}

	static io_result<void> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		vsm_assert(h.openssl != nullptr);

		return {};
	}

	static io_result<void> _continue(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		auto const mode = a.mode.value_or(disconnect_mode::both);
		bool const disconnect_read = vsm::any_flags(mode, disconnect_mode::read);
		bool const disconnect_write = vsm::any_flags(mode, disconnect_mode::write);

		if (std::holds_alternative<raw_disconnect>(s.m_raw_state))
		{
			if (!h.openssl->is_transport_readable() == disconnect_read &&
				!h.openssl->is_transport_writable() == disconnect_write)
			{
				return {};
			}
		}

		if (h.openssl->is_tunnel_connected())
		{
			vsm_try_void(base::_enter(m, h, c, s, handler, [&]() -> typename base::enter_result
			{
				return h.openssl->disconnect(disconnect_read, disconnect_write);
			}));
		}

		auto const raw_a = make_raw_disconnect_args(h, a);

		vsm_try_void(detail::submit_io(
			m,
			h,
			c,
			s.m_raw_state.template emplace<raw_disconnect>(),
			raw_a,
			handler));

		auto const raw_mode = raw_a.mode.value_or_zero();
		h.openssl->disconnect_completed(
			vsm::any_flags(raw_mode, disconnect_mode::read),
			vsm::any_flags(raw_mode, disconnect_mode::write));

		return {};
	}

	// Notify while a raw read or write operation is active:
	using base::_notify;

	// Notify while the raw disconnect operation is active:
	static io_result<void> _notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type&& status,
		raw_disconnect& raw_state)
	{
		auto const raw_a = make_raw_disconnect_args(h, a);

		// Notify the underlying raw operation:
		vsm_try_void(detail::notify_io(
			m,
			h,
			c,
			raw_state,
			raw_a,
			handler,
			vsm_move(status)));

		auto const raw_mode = raw_a.mode.value_or_zero();
		h.openssl->disconnect_completed(
			vsm::any_flags(raw_mode, disconnect_mode::read),
			vsm::any_flags(raw_mode, disconnect_mode::write));

		return {};
	}

	friend base;
};

template<
	typename M,
	typename RawSocket,
	vsm::any_of<byte_io::stream_read_t, byte_io::stream_write_t> Operation>
struct async_operation<M, openssl_socket_t<RawSocket>, Operation>
	: openssl_socket_operation<M, RawSocket, Operation>
{
private:
	using base = openssl_socket_operation<M, RawSocket, Operation>;

	using H = native_handle<openssl_socket_t<RawSocket>> const;
	using C = async_connector_t<M, openssl_socket_t<RawSocket>> const;
	using S = async_operation_t<M, openssl_socket_t<RawSocket>, Operation>;
	using A = io_parameters_t<openssl_socket_t<RawSocket>, Operation>;

	using internal_result_type = byte_io_status;

	byte_io_state io_state;
	size_t buffer_index;
	size_t buffer_offset;

	static io_result<byte_io_status> _submit(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		s.io_state = {};
		s.buffer_index = 0;
		s.buffer_offset = 0;

		return byte_io_status::success;
	}

	vsm_msvc_warning(push)
	vsm_msvc_warning(disable: 4102) // unreferenced label

	vsm_gcc_diagnostic(push)
	vsm_gcc_diagnostic(ignored "-Wunused-label")

	static io_result<byte_io_status> _continue(M& m, H& h, C& c, S& s, A const& a, io_handler<M>& handler)
	{
		using byte_type = vsm::select_t<
			std::is_same_v<Operation, byte_io::stream_read_t>,
			std::byte,
			std::byte const>;

		auto const buffer_layout = a.buffers.get_layout();
		auto const buffers = read_io_buffers(a.buffers.get_buffers());
		auto const buffers_size = static_cast<size_t>(buffers.size());

		auto status = byte_io_status::success;

		while (s.buffer_index < buffers_size)
		{
			auto const buffer = get_io_buffer_span<byte_type>(
				buffers[static_cast<ptrdiff_t>(s.buffer_index)],
				buffer_layout);

			vsm_assert(s.buffer_offset <= buffer.size());

			// TODO: Is this correct?
			//       Consider SSL_MODE_ENABLE_PARTIAL_WRITE.

			auto const remaining_data = buffer.data() + s.buffer_offset;
			auto const remaining_size = buffer.size() - s.buffer_offset;

			while (true)
			{
				tunnel_result<typename Operation::result_type> r;
				if constexpr (std::is_same_v<Operation, byte_io::stream_read_t>)
				{
					vsm_try_assign(
						r,
						h.openssl->read_some(read_buffer(remaining_data, remaining_size)));
				}
				else
				{
					vsm_try_assign(
						r,
						h.openssl->write_some(write_buffer(remaining_data, remaining_size)));
				}

				if (r)
				{
					size_t transferred;
					if constexpr (std::is_same_v<Operation, byte_io::stream_read_t>)
					{
						if (r->is_end_of_stream())
						{
							status = byte_io_status::end_of_stream;
							goto outer_break;
						}

						transferred = r->size();
					}
					else
					{
						transferred = *r;
					}

					vsm_assert(transferred <= remaining_size);
					s.io_state.transfer_completed(transferred);

					if (transferred == remaining_size)
					{
						s.buffer_index += 1;
						s.buffer_offset = 0;
					}
					else
					{
						s.buffer_offset += transferred;
					}

					break;
				}
				else
				{
					vsm_try_void(base::_submit_rw(m, h, c, s, handler));
				}
			}
		}

	outer_break:
		vsm_try_void(base::_submit_rw(m, h, c, s, handler));

		vsm_assert(!h.openssl->read_needed());
		vsm_assert(!h.openssl->write_needed());

		return status;
	}

	vsm_msvc_warning(pop)
	vsm_gcc_diagnostic(pop)

	static io_result<typename Operation::result_type> _finalize(
		M&,
		H&,
		C&,
		S& s,
		A const& a,
		io_handler<M>&,
		io_result<byte_io_status> const& r)
	{
		return s.io_state.finalize<typename Operation::result_type>(a, r);
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

		detail::delete_object(h.openssl);
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
