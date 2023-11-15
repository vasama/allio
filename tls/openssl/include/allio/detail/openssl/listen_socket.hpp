#pragma once

#include <allio/detail/handles/listen_socket.hpp>
#include <allio/detail/openssl/openssl.hpp>

#include <variant>

namespace allio::detail {

struct openssl_context;

struct openssl_listen_socket_t : basic_listen_socket_t<object_t>
{
	using base_type = basic_listen_socket_t<object_t>;

	struct native_type : raw_listen_socket_t::native_type
	{
		openssl_context* context;
	};

	static vsm::result<void> blocking_io(
		close_t,
		native_type& h,
		io_parameters_t<close_t> const& args);

	static vsm::result<void> blocking_io(
		listen_t,
		native_type& h,
		io_parameters_t<connect_t> const& args);

	static vsm::result<void> blocking_io(
		accept_t,
		native_type const& h,
		io_parameters_t<accept_t> const& args);
};

template<typename M>
struct operation_impl<M, openssl_listen_socket_t, openssl_listen_socket_t::listen_t>
{
	using H = openssl_listen_socket_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using O = H::listen_t;
	using S = operation_t<M, H, O>;

	operation_t<M, raw_listen_socket_t, raw_listen_socket_t::listen_t> state;

	static io_result2<void> submit(M& m, N& h, C& c, S& s)
	{
		return submit_io(m, h, c, s.state);
	}

	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		return notify_io(m, h, c, s.state, status);
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		cancel_io(m, h, c, s.state);
	}
};

template<typename M>
struct operation_impl<M, openssl_listen_socket_t, openssl_listen_socket_t::accept_t>
{
	using H = openssl_listen_socket_t;
	using N = H::native_type const;
	using C = connector_t<M, H>;
	using O = H::accept_t;
	using S = operation_t<M, H, O>;
	using R = accept_result<async_handle<openssl_socket_t, multiplexer_handle_t<M>>>;

	using raw_accept = operation_t<M, raw_listen_socket_t, raw_listen_socket_t::accept_t>;
	using raw_read_some = operation_t<M, raw_socket_t, raw_socket_t::read_some_t>;
	using raw_write_some = operation_t<M, raw_socket_t, raw_socket_t::write_some_t>;

	struct raw_close : operation_t<M, raw_socket_t, raw_socket_t::close_t>
	{
		std::error_code error;
	};

	using socket_handle_type = async_handle<openssl_socket_t, multiplexer_handle_t<M>>;

	std::variant
	<
		raw_accept,
		raw_read_some,
		raw_write_some,
		raw_close
	>
	variant;

	openssl_socket_t::native_type h;
	vsm_no_unique_address connector_t<M, raw_socket_t> c;
	network_endpoint endpoint;

	static io_result2<void> _enter(M& m, S& s)
	{
		vsm_try(data_request, openssl_accept(*h.context));
		switch (data_request)
		{
		case openssl_data_request::read:
			return submit_io(m, s.h, s.c, s.variant.emplace<raw_read_some>());

		case openssl_data_request::write:
			return submit_io(m, s.h, s.c, s.variant.emplace<raw_write_some>());
		}
		return {};
	}

	static io_result2<void> _enter_or_close(M& m, S& s)
	{
		auto r = _enter(m, h, c, s);

		if (r.failed())
		{
			openssl_delete_context(s.h.context);
			r = submit_io(m, s.h, s.c, s.variant.emplace<raw_close>());
			vsm_assert(r); // Close does not fail.
		}

		return r;
	}

	static io_result2<void> _close_on_error(M& m, N& h, C& c, S& s, io_result2<void> r)
	{
		if (r.failed())
		{
			openssl_delete_context(h.context);
			h.context = nullptr;

			r = submit_io(m, h, c, s.variant.emplace<raw_close>(r.error()));
			vsm_assert(r);
		}
		return r;
	}

	static vsm::result<void> _process_accept_result(S& s, auto& r)
	{
		vsm_try_assign(s.h.context, openssl_create_context());

		r.socket.release(s.h, s.c);
		s.endpoint = r.endpoint;

		return {};
	}

	static io_result2<R> _compose_accept_result(M& m, S& s)
	{
		return vsm_lazy(R
		{
			socket_handle_type
			(
				adopt_handle_t(),
				vsm_move(s.h),
				m,
				vsm_move(s.c),
			),
			s.network_endpoint,
		});
	}

	static io_result2<R> submit(M& m, N& h, C& c, S& s)
	{
		vsm_try(r, submit_io(m, h, c, std::get<raw_accept>(s.variant)));
		vsm_try_void(_process_accept_result(s, r));
		return _compose_accept_result(m, s);
	}

	static io_result2<R> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		vsm_try_void(std::visit([&]<typename State>(State& state) -> io_result2<void>
		{
			if constexpr (std::is_same_v<State, raw_accept>)
			{
				vsm_try(r, notify_io(m, h, c, state, status));
				vsm_try_void(_process_accept_result(s, r));
				return _close_on_error(_enter(m, s, 0));
			}
			else
			{
				auto const r = notify_io(m, s.h, s.c, state, status);
				if constexpr (std::is_same_v<State, raw_close>)
				{
					return r;
				}
				else
				{
					return _close_on_error([&]() -> io_result2<void>
					{
						if (r)
						{
							return _enter(m, s, *r);
						}
						else
						{
							return vsm::unexpected(r.error());
						}
					}());
				}
			}
		}, s.variant));
		return _compose_accept_result(m, s);
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		std::visit([&]<typename State>(State& state)
		{
			if constexpr (std::is_same_v<State, raw_accept>)
			{
				cancel_io(m, h, c, state);
			}
			else if constexpr (!std::is_same_v<State, raw_close>)
			{
				cancel_io(m, s.h, s.c, state);
			}
		}, s.variant);
	}
};

} // namespace allio::detail
