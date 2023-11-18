#pragma once

#include <allio/detail/handles/socket.hpp>
#include <allio/openssl/detail/openssl.hpp>

#include <variant>

namespace allio::detail {

class blocking_multiplexer
{
public:
	using is_multiplexer = void;

	using connector_type = connector_base;
	using operation_type = operation_base;
	struct io_status_type {};

	template<typename H, typename O>
	static vsm::result<io_result_t<O, H>> do_io(
		handle_const_t<O, typename H::native_type>& h,
		io_parameters_t<O> const& args)
	{
		blocking_multiplexer m;
		connector_t<blocking_multiplexer, H> c;
		operation_t<blocking_multiplexer, H, O> s;
		basic_io_handler<io_status_type> handler(io_callback);
		auto r = submit_io(m, h, c, s, args, handler);
		vsm_assert(!r.is_pending() && !r.is_cancelled());

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<O, H>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<io_result_t<O, H>>(vsm_move(*r));
		}
	}

private:
	static void io_callback(basic_io_handler<io_status_type>& handler, io_status_type&& status) noexcept
	{
		vsm_unreachable();
	}
};

class blocking_multiplexer_handle
{
public:
	using is_multiplexer_handle = void;

	using multiplexer_type = blocking_multiplexer;
};

template<std::same_as<blocking_multiplexer> M, typename H>
struct connector<M, H> : blocking_multiplexer::connector_type
{
	using N = typename H::native_type const;
	using C = connector;

	static vsm::result<void> attach(M&, N&, C&)
	{
		return {};
	}

	static vsm::result<void> detach(M&, N&, C&)
	{
		return {};
	}
};

template<std::same_as<blocking_multiplexer> M, typename H, typename O>
struct operation<M, H, O> : blocking_multiplexer::operation_type
{
	using N = handle_const_t<O, typename H::native_type>;
	using C = handle_const_t<O, connector_t<M, H>>;
	using S = operation;
	using A = io_parameters_t<O>;

	static io_result<io_result_t<O, H, blocking_multiplexer_handle>> submit(M&, N& h, C&, S&, A const& args, io_handler<M>&)
	{
		auto r = blocking_io<O>(h, args);

		if (!r)
		{
			return vsm::unexpected(r.error());
		}

		if constexpr (std::is_void_v<io_result_t<O, H>>)
		{
			return {};
		}
		else
		{
			return rebind_handle<io_result_t<O, H, blocking_multiplexer_handle>>(vsm_move(*r));
		}
	}

	static io_result<io_result_t<O, H, blocking_multiplexer_handle>> notify(M&, N& h, C&, S&, A const& args, M::io_status_type&&)
	{
		vsm_unreachable();
	}

	static void cancel(M&, N const&, C const&, S&)
	{
	}
};


struct openssl_socket_t : basic_socket_t<object_t>
{
	using base_type = basic_socket_t<object_t>;

	struct native_type : raw_socket_t::native_type
	{
		openssl_context* context;

		template<typename O>
		static vsm::result<io_result_t<O, openssl_socket_t>> tag_invoke(
			blocking_io_t<O>,
			handle_const_t<O, native_type>& h,
			io_parameters_t<O> const& args)
		{
			return blocking_multiplexer::do_io<openssl_socket_t, O>(h, args);
		}
	};
};

template<typename M>
struct connector<M, openssl_socket_t>
	: connector<M, raw_socket_t>
{
};

template<typename RawSocket, typename Implementation, typename... RawOperations>
struct openssl_operation_impl;

template<typename RawSocket, typename M, typename H, typename O, typename... RawOperations>
struct openssl_operation_impl<RawSocket, operation<M, H, O>, RawOperations...>
	: operation_base
{
	using I = operation<M, H, O>;
	using N = handle_const_t<O, typename H::native_type>;
	using C = handle_const_t<O, connector_t<M, H>>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;
	using R = io_result_t<O, H, multiplexer_handle_t<M>>;

	using _raw_read = operation_t<M, raw_socket_t, socket_io::stream_read_t>;
	using _raw_write = operation_t<M, raw_socket_t, socket_io::stream_write_t>;

	template<typename RawObject>
	struct _raw_close : operation_t<M, RawObject, object_t::close_t>
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
	struct _raw<operation_t<M, RawObject, object_t::close_t>>
	{
		using type = _raw_close<RawObject>;
	};

	io_handler<M>* _handler;
	std::variant<
		std::monostate,
		typename _raw<RawOperations>::type...
	> _raw_state;


	static bool _is_close(S const& s)
	{
		return std::visit(
			[]<typename RawState>(RawState const&)
			{
				return vsm::is_instance_of_v<RawState, _raw_close>;
			},
			s._raw_state);
	}

	static io_result<R> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		s._handler = &handler;

		vsm_try_void([&]() -> io_result<void>
		{
			auto r = I::_submit(m, h, c, s, args, handler);

			vsm_assert(!_is_close(s));

			if (r)
			{
				r = I::_continue(m, h, c, s, args, handler);
			}

			if (!r)
			{
				r = I::_on_error(m, h, c, s, args, handler, r.error());
			}

			return r;
		}());

		return I::_get_result(m, s);
	}

	static io_result<R> notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status)
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
				auto r = I::_notify(m, h, c, s, args, vsm_move(status), raw_state);

				if constexpr (!vsm::is_instance_of_v<RawState, _raw_close>)
				{
					if (r)
					{
						r = I::_continue(m, h, c, s, args, handler);
					}

					if (!r)
					{
						r = I::_on_error(m, h, c, s, args, handler, r.error());
					}
				}

				return r;
			}
		}, s._raw_state));

		return I::_get_result(m, s);
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
#if 0
		//TODO: Thread safety.
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


	static auto const& _get_rw_h(N& h, S& s)
	{
		return h;
	}

	static auto const& _get_rw_c(C& c, S& s)
	{
		return c;
	}

	template<typename Callable>
	static auto _enter(M& m, N& h, C& c, S& s, io_handler<M>& handler, Callable&& callable)
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

			switch (r.error())
			{
			case openssl_data_request::read:
				{
					vsm_try(transferred, submit_io(
						m,
						I::_get_rw_h(h, s),
						I::_get_rw_c(c, s),
						s._raw_state.emplace<_raw_read>(),
						io_args<socket_io::stream_read_t>(h.context->get_read_buffer())(),
						handler));
					h.context->read_completed(transferred);
				}

			case openssl_data_request::write:
				{
					vsm_try(transferred, submit_io(
						m,
						I::_get_rw_h(h, s),
						I::_get_rw_c(c, s),
						s._raw_state.emplace<_raw_write>(),
						io_args<socket_io::stream_write_t>(h.context->get_write_buffer())(),
						handler));
					h.context->write_completed(transferred);
				}
			}
		}
	}


	template<typename RawState>
	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, RawState& raw_state)
	{
		return notify_io(m, h, c, raw_state, args, status);
	}

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_read& raw_state)
	{
		vsm_try(transferred, notify_io(
			m,
			I::_get_rw_h(h, s),
			I::_get_rw_c(c, s),
			raw_state,
			io_args<socket_io::stream_read_t>(h.context->get_read_buffer())(),
			vsm_move(status)));
		h.context->read_completed(transferred);
		return {};
	}

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_write& raw_state)
	{
		vsm_try(transferred, notify_io(
			m,
			I::_get_rw_h(h, s),
			I::_get_rw_c(c, s),
			raw_state,
			io_args<socket_io::stream_write_t>(h.context->get_write_buffer())(),
			vsm_move(status)));
		h.context->write_completed(transferred);
		return {};
	}

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_close<RawSocket>& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			h,
			c,
			raw_state,
			io_args<object_t::close_t>()(),
			vsm_move(status)));

		h.platform_handle = native_platform_handle::null;

		return vsm::unexpected(raw_state.error);
	}

	static io_result<void> _on_error(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler, std::error_code const e)
	{
		if (h.context != nullptr)
		{
			h.context->delete_context();
			h.context = nullptr;
		}

		if (h.platform_handle != native_platform_handle::null)
		{
			vsm_try_void(submit_io(
				m,
				h,
				c,
				s._raw_state.emplace<_raw_close<RawSocket>>(e),
				io_args<object_t::close_t>()(),
				handler));

			h.platform_handle = native_platform_handle::null;
		}

		return vsm::unexpected(e);
	}

	static io_result<void> _get_result(M& m, S& s) requires std::is_void_v<R>
	{
		return {};
	}
};

template<typename M>
struct operation<M, openssl_socket_t, socket_io::connect_t>
	: openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, socket_io::connect_t>,
		operation_t<M, raw_socket_t, socket_io::connect_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>
{
	using _base = openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, socket_io::connect_t>,
		operation_t<M, raw_socket_t, socket_io::connect_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>;

	using H = openssl_socket_t;
	using N = H::native_type;
	using O = socket_io::connect_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	using _raw_connect = operation_t<M, raw_socket_t, socket_io::connect_t>;

	static vsm::result<void> _connect_completed(N& h)
	{
		// The client TLS context is created after successful raw connect.
		vsm_try_assign(h.context, openssl_context::create(openssl_mode::client));

		return {};
	}

	static io_result<void> _submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		h = {};

		vsm_try_void(submit_io(
			m,
			h,
			c,
			s._raw_state.emplace<_raw_connect>(),
			args,
			handler));

		vsm_try_void(_connect_completed(h));

		return {};
	}

	using _base::_notify;

	static io_result<void> _notify(M& m, N& h, C& c, S& s, A const& args, M::io_status_type&& status, _raw_connect& raw_state)
	{
		vsm_try_void(notify_io(
			m,
			h,
			c,
			raw_state,
			args,
			vsm_move(status)));

		vsm_try_void(_connect_completed(h));

		return {};
	}

	static io_result<void> _continue(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		vsm_assert(h.context != nullptr);

		vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
		{
			return h.context->connect();
		}));

		//TODO: ktls

		return {};
	}
};

template<typename M>
struct operation<M, openssl_socket_t, socket_io::disconnect_t>
	: openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, socket_io::disconnect_t>,
		operation_t<M, raw_socket_t, socket_io::disconnect_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>
{
	using _base = openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, socket_io::disconnect_t>,
		operation_t<M, raw_socket_t, socket_io::disconnect_t>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>,
		operation_t<M, raw_socket_t, object_t::close_t>>;

	using H = openssl_socket_t;
	using N = H::native_type;
	using O = socket_io::disconnect_t;
	using C = connector_t<M, H>;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	using _raw_disconnect = operation_t<M, raw_socket_t, socket_io::disconnect_t>;

	static io_result<void> _submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		vsm_assert(h.context != nullptr);

		return {};
	}

	static io_result<void> _continue(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		if (h.context != nullptr)
		{
			//TODO: ktls

			vsm_try_void(_base::_enter(m, h, c, s, handler, [&]()
			{
				return h.context->disconnect();
			}));
		}

		if (h.flags[object_t::flags::not_null])
		{
			vsm_try_void(submit_io(
				m,
				h,
				c,
				s._raw_state.emplace<_raw_disconnect>(),
				io_args<socket_io::disconnect_t>()(),
				handler));
		}

		return {};
	}
};

template<typename M, vsm::any_of<socket_io::stream_read_t, socket_io::stream_write_t> O>
struct operation<M, openssl_socket_t, O>
	: openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, O>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>>
{
	using _base = openssl_operation_impl<
		raw_socket_t,
		operation<M, openssl_socket_t, O>,
		operation_t<M, raw_socket_t, socket_io::stream_read_t>,
		operation_t<M, raw_socket_t, socket_io::stream_write_t>>;

	using H = openssl_socket_t;
	using N = H::native_type const;
	using C = connector_t<M, H> const;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	size_t buffer_index;
	size_t buffer_offset;
	size_t transferred;

	static io_result<void> _submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		//TODO: ktls

		s.buffer_index = 0;
		s.buffer_offset = 0;
		s.transferred = 0;

		return _continue(m, h, c, s, args, handler);
	}

	static io_result<void> _continue(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		auto const buffers = args.buffers.buffers();
		for (; s.buffer_index < buffers.size(); ++s.buffer_index)
		{
			auto const buffer = buffers[s.buffer_index];
			vsm_assert(s.buffer_offset < buffer.size());

			auto const remaining_data = buffer.data() + s.buffer_offset;
			auto const remaining_size = buffer.size() - s.buffer_offset;

			vsm_try(transferred, _base::_enter(m, h, c, s, handler, [&]()
			{
				if constexpr (std::is_same_v<O, socket_io::stream_read_t>)
				{
					return h.context->read(remaining_data, remaining_size);
				}
				else
				{
					return h.context->write(remaining_data, remaining_size);
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

	static io_result<void> _on_error(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler, std::error_code const e)
	{
		if (s.transferred != 0)
		{
			return {};
		}

		return vsm::unexpected(e);
	}

	static size_t _get_result(M& m, S& s)
	{
		return s.transferred;
	}
};

template<typename M>
struct operation<M, openssl_socket_t, openssl_socket_t::close_t>
	: operation_t<M, raw_socket_t, raw_socket_t::close_t>
{
	using H = openssl_socket_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using O = H::close_t;
	using S = operation_t<M, H, O>;
	using A = io_parameters_t<O>;

	static io_result<void> submit(M& m, N& h, C& c, S& s, A const& args, io_handler<M>& handler)
	{
		vsm_assert(h.context != nullptr);

		h.context->delete_context();
		h.context = nullptr;

		return operation_t<M, raw_socket_t, raw_socket_t::close_t>::submit_io(m, h, c, s, args, handler);
	}
};

} // namespace allio::detail
