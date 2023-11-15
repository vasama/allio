#pragma once

#include <allio/detail/handles/socket.hpp>
#include <allio/detail/openssl/openssl.hpp>

#include <variant>

namespace allio::detail {

struct openssl_socket_t : basic_socket_t<object_t>
{
	using base_type = basic_socket_t<object_t>;

	struct native_type : raw_socket_t::native_type
	{
		openssl_context* context;
	};

	static vsm::result<void> blocking_io(
		close_t,
		native_type& h,
		io_parameters_t<close_t> const& args);

	static vsm::result<void> blocking_io(
		connect_t,
		native_type& h,
		io_parameters_t<connect_t> const& args);

	static vsm::result<void> blocking_io(
		disconnect_t,
		native_type& h,
		io_parameters_t<disconnect_t> const& args);

	static vsm::result<void> blocking_io(
		read_some_t,
		native_type const& h,
		io_parameters_t<read_some_t> const& args);

	static vsm::result<void> blocking_io(
		write_some_t,
		native_type const& h,
		io_parameters_t<write_some_t> const& args);
};

template<typename M>
struct operation_impl<M, openssl_socket_t, openssl_socket_t::connect_t>
{
	using H = openssl_socket_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using O = H::connect_t;
	using S = operation_t<M, H, O>;

	using raw_connect = operation_t<M, raw_socket_t, raw_socket_t::connect_t>;
	using raw_read_some = operation_t<M, raw_socket_t, raw_socket_t::read_some_t>;
	using raw_write_some = operation_t<M, raw_socket_t, raw_socket_t::write_some_t>;
	using raw_close = operation_t<M, raw_socket_t, raw_socket_t::close_t>;

	std::variant
	<
		raw_connect,
		raw_read_some,
		raw_write_some,
		raw_close
	>
	variant;

	static io_result2<void> _enter(M& m, N& h, C& c, S& s, size_t const transferred)
	{
		vsm_try(data_request, openssl_connect(*h.context, transferred));
		switch (data_request)
		{
		case openssl_data_request::read:
			return submit_io(m, h, c, s.variant.emplace<raw_read_some>());

		case openssl_data_request::write:
			return submit_io(m, h, c, s.variant.emplace<raw_write_some>());
		}
		return {};
	}

	static io_result2<void> _close_on_error(M& m, N& h, C& c, S& s, io_result2<void> r)
	{
		if (r.failed())
		{
			openssl_delete_context(h.context);
			h.context = nullptr;

			r = submit_io(m, h, c, s.variant.emplace<raw_close>());
			vsm_assert(r);
		}
		return r;
	}

	static io_result2<void> submit(M& m, N& h, C& c, S& s)
	{
		vsm_try_void(submit_io(m, h, c, std::get<raw_connect>(s.variant)));
		return _close_on_error(_enter(m, h, c, s, 0));
	}

	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		return std::visit(s.variant, [&]<typename State>(State& state) -> io_result2<void>
		{
			auto const r = notify_io(m, h, c, s, status);
			if constexpr (std::is_same_v<State, raw_close> || std::is_same_v<State, raw_connect>)
			{
				return r;
			}
			else
			{
				if (r)
				{
					return _close_on_error(_enter(m, h, c, s, *r));
				}
				else
				{
					return vsm::unexpected(r.error());
				}
			}
		});
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		std::visit(s.variant, [&]<typename State>(State& state)
		{
			if constexpr (!std::is_same_v<State, raw_close>)
			{
				cancel_io(m, h, c, s);
			}
		});
	}
};

template<typename M>
struct operation_impl<M, openssl_socket_t, openssl_socket_t::disconnect_t>
{
	using H = openssl_socket_t;
	using N = H::native_type;
	using C = connector_t<M, H>;
	using O = H::connect_t;
	using S = operation_t<M, H, O>;

	using raw_read_some = operation_t<M, raw_socket_t, raw_socket_t::read_some_t>;
	using raw_write_some = operation_t<M, raw_socket_t, raw_socket_t::write_some_t>;
	using raw_disconnect = operation_t<M, raw_socket_t, raw_socket_t::disconnect_t>;
	using raw_close = operation_t<M, raw_socket_t, raw_socket_t::close_t>;

	std::variant
	<
		std::monostate,
		raw_read_some,
		raw_write_some,
		raw_disconnect,
		raw_close
	>
	variant;

	static io_result2<void> _enter(M& m, N& h, C& c, S& s, size_t const transferred)
	{
		vsm_try(data_request, openssl_connect(*h.context, transferred));
		switch (data_request)
		{
		case openssl_data_request::read:
			return submit_io(m, h, c, s.variant.emplace<raw_read_some>());

		case openssl_data_request::write:
			return submit_io(m, h, c, s.variant.emplace<raw_write_some>());
		}
		return {};
	}

	static io_result2<void> _close_on_error(M& m, N& h, C& c, S& s, io_result2<void> r)
	{
		if (r.failed())
		{
			openssl_delete_context(h.context);
			h.context = nullptr;

			r = submit_io(m, h, c, s.variant.emplace<raw_close>());
			vsm_assert(r);
		}
		return r;
	}

	static io_result2<void> submit(M& m, N& h, C& c, S& s)
	{
		return _close_on_error(_enter(m, h, c, s, 0));
	}

	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		return std::visit(s.variant, [&]<typename State>(State& state) -> io_result2<void>
		{
			auto const r = notify_io(m, h, c, state, status);
			if constexpr (std::is_same_v<State, raw_close>)
			{
				return r;
			}
			else
			{
				return _close_on_error([&]() -> io_result2<void>
				{
					if constexpr (std::is_same_v<State, raw_disconnect>)
					{
						return r;
					}
					else
					{
						if (r)
						{
							return _enter(m, h, c, s, *r);
						}
						else
						{
							return vsm::unexpected(r.error());
						}
					}
				}());
			}
		});
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		std::visit(s.variant, [&]<typename State>(State& state)
		{
			if constexpr (!std::is_same_v<State, raw_close>)
			{
				cancel_io(m, h, c, s);
			}
		});
	}
};

template<typename M>
struct operation_impl<M, openssl_socket_t, openssl_socket_t::read_some_t>
{
	using H = openssl_socket_t;
	using N = H::native_type const;
	using C = connector_t<M, H>;
	using O = H::read_some_t;
	using S = operation_t<M, H, O>;

	using raw_read_some = operation_t<M, raw_socket_t, raw_socket_t::read_some_t>;
	using raw_write_some = operation_t<M, raw_socket_t, raw_socket_t::write_some_t>;

	size_t buffer_index;

	std::variant
	<
		raw_read_some,
		raw_write_some
	>
	variant;

	static io_result2<void> _enter(M& m, N& h, C& c, S& s, size_t const transferred)
	{
		auto const buffers = s.args.buffers.buffers();
		for (; s.buffer_index < buffers.size(); ++s.buffer_index)
		{
			auto const buffer = buffers[s.buffer_index];
			vsm_try(data_request, openssl_read(*h.context, buffer.data(), buffer.size(), transferred));
			switch (data_request)
			{
			case openssl_data_request::read:
				return submit_io(m, h, c, s.variant.emplace<raw_read_some>());
	
			case openssl_data_request::write:
				return submit_io(m, h, c, s.variant.emplace<raw_write_some>());
			}
		}
		return {};
	}

	static io_result2<void> submit(M& m, N& h, C& c, S& s)
	{
		if (s.args.buffers.empty())
		{
			return vsm::unexpected(error::invalid_argument);
		}

		s.buffer_index = 0;
		return _enter(m, h, c, s);
	}

	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		vsm_try(transferred, std::visit(s.variant, [&](auto& state)
		{
			return notify_io(m, h, c, s, status);
		}));
		return _enter(m, h, c, s, transferred);
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		std::visit(s.variant, [&](auto& state)
		{
			cancel_io(m, h, c, s);
		});
	}
};

template<typename M>
struct operation_impl<M, openssl_socket_t, openssl_socket_t::write_some_t>
{
	using H = openssl_socket_t;
	using N = H::native_type const;
	using C = connector_t<M, H>;
	using O = H::write_some_t;
	using S = operation_t<M, H, O>;

	using raw_read_some = operation_t<M, raw_socket_t, raw_socket_t::read_some_t>;
	using raw_write_some = operation_t<M, raw_socket_t, raw_socket_t::write_some_t>;

	size_t buffer_index;
	std::variant
	<
		raw_read_some,
		raw_write_some
	>
	variant;

	static io_result2<void> _enter(M& m, N& h, C& c, S& s, size_t const transferred)
	{
		auto const buffers = s.args.buffers.buffers();
		for (; s.buffer_index < buffers.size(); ++s.buffer_index)
		{
			auto const buffer = buffers[s.buffer_index];
			vsm_try(data_request, openssl_write(*h.context, buffer.data(), buffer.size(), transferred));
			switch (data_request)
			{
			case openssl_data_request::read:
				return submit_io(m, h, c, s.variant.emplace<raw_read_some>());
	
			case openssl_data_request::write:
				return submit_io(m, h, c, s.variant.emplace<raw_write_some>());
			}
		}
		return {};
	}

	static io_result2<void> submit(M& m, N& h, C& c, S& s)
	{
		if (s.args.buffers.empty())
		{
			return vsm::unexpected(error::invalid_argument);
		}

		s.buffer_index = 0;
		return _enter(m, h, c, s);
	}

	static io_result2<void> notify(M& m, N& h, C& c, S& s, io_status const status)
	{
		vsm_try(transferred, std::visit(s.variant, [&](auto& state)
		{
			return notify_io(m, h, c, s, status);
		}));
		return _enter(m, h, c, s, transferred);
	}

	static void cancel(M& m, N const& h, C const& c, S& s)
	{
		std::visit(s.variant, [&](auto& state)
		{
			cancel_io(m, h, c, s);
		});
	}
};

} // namespace allio::detail
