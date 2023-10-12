#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/socket_handle_base.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

template<typename SecurityProvider = default_security_provider>
class socket_handle_t;

template<typename SecurityProvider>
class _socket_handle : public socket_handle_base<SecurityProvider>
{
	using socket_handle_type = socket_handle_t<SecurityProvider>;

protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct connect_t
	{
		static constexpr bool producer = true;

		using handle_type = socket_handle_t;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = deadline_t;
	};

	struct disconnect_t
	{
		using handle_type = socket_handle_t;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;
	};

	struct read_some_t
	{
		using handle_type = socket_handle_type const;
		using result_type = size_t;

		struct required_params_type
		{
			untyped_buffers_storage buffers;
		};

		using optional_params_type = deadline_t;
	};

	struct write_some_t
	{
		using handle_type = socket_handle_type const;
		using result_type = size_t;

		struct required_params_type
		{
			untyped_buffers_storage buffers;
		};

		using optional_params_type = deadline_t;
	};

	using asynchronous_operations = type_list_cat<
		typename base_type::asynchronous_operations,
		type_list<connect_t, disconnect_t, read_some_t, write_some_t>
	>;

protected:
	template<typename H, typename M>
	struct interface : base_type::template interface<H, M>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto read_some(read_buffers const buffers, auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<read_some_t>(buffers)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffers const buffers, auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<write_some_t>(buffers)(vsm_forward(args)...));
		}
	};
};

template<>
class socket_handle_t<void> : public _socket_handle<void>
{
protected:
	static vsm::result<void> do_blocking_io(
		socket_handle_t& h,
		io_result_ref_t<connect_t> r,
		io_parameters_t<connect_t> const& args);

	static vsm::result<void> do_blocking_io(
		socket_handle_t& h,
		io_result_ref_t<disconnect_t> r,
		io_parameters_t<disconnect_t> const& args);

	static vsm::result<void> do_blocking_io(
		socket_handle_t const& h,
		io_result_ref_t<read_some_t> r,
		io_parameters_t<read_some_t> const& args);

	static vsm::result<void> do_blocking_io(
		socket_handle_t const& h,
		io_result_ref_t<write_some_t> r,
		io_parameters_t<write_some_t> const& args);
};

template<typename SecurityProvider>
class socket_handle_t : public _socket_handle<SecurityProvider>
{
public:
};

using raw_socket_handle_t = socket_handle_t<void>;

template<typename Multiplexer>
//using basic_socket_handle = basic_handle<socket_handle_t<>, Multiplexer>;
using basic_socket_handle = basic_handle<socket_handle_t<void>, Multiplexer>;

template<typename Multiplexer>
using basic_raw_socket_handle = basic_handle<raw_socket_handle_t, Multiplexer>;

vsm::result<basic_socket_handle<void>> connect(
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_socket_handle<void>;

	h_type h;
	vsm_try_void(blocking_io(
		h,
		no_result,
		io_args<h_type::connect_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
auto connect(
	Multiplexer&& multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_socket_handle<Multiplexer>;

	return io_handle_sender<h_type::connect_t, h_type>(
		multiplexer,
		io_args<h_type::connect_t>(endpoint)(vsm_forward(args)...));
}

} // namespace allio::detail
