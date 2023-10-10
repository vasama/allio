#pragma once

#include <allio/detail/handles/socket_handle.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct backlog_t
{
	std::optional<uint32_t> backlog;

	struct tag_t
	{
		struct argument_t
		{
			uint32_t value;

			friend void tag_invoke(set_argument_t, backlog_t& args, argument_t const& value)
			{
				args.backlog = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(uint32_t const backlog)
		{
			return { backlog };
		}
	};
};
inline constexpr backlog_t::tag_t backlog = {};

template<typename SocketHandle>
struct basic_accept_result
{
	SocketHandle socket;
	network_endpoint endpoint;
};

template<typename SecurityProvider = default_security_provider>
class abstract_listen_handle : public socket_handle_base<SecurityProvider>
{
	using abstract_socket_handle_type = abstract_socket_handle<SecurityProvider>;

	template<typename M>
	using socket_handle = basic_handle<abstract_socket_handle_type, M>;

protected:
	using base_type = socket_handle_base<SecurityProvider>;

public:
	struct listen_t
	{
		using handle_type = abstract_listen_handle;
		using result_type = void;

		using required_params_type = network_endpoint_t;
		using optional_params_type = no_parameters_t;
	};

	struct accept_t
	{
		using handle_type = abstract_listen_handle;

		template<typename M>
		using result_type_template = basic_accept_result<socket_handle<M>>;

		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	using asynchronous_operations = type_list_cat<
		typename base_type::asynchronous_operations,
		type_list<listen_t, accept_t>
	>;

protected:
	template<typename H>
	struct interface : base_type::template interface<H>
	{
		[[nodiscard]] auto accept(auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<accept_t>()(vsm_forward(args)...));
		}
	};

	static vsm::result<void> do_blocking_io(
		abstract_listen_handle& h,
		io_result_ref_t<listen_t> r,
		io_parameters_t<listen_t> const& args);

	static vsm::result<void> do_blocking_io(
		abstract_listen_handle const& h,
		io_result_ref_t<accept_t> r,
		io_parameters_t<accept_t> const& args);
};

template<typename Multiplexer>
using basic_listen_handle = basic_handle<abstract_listen_handle<>, Multiplexer>;

vsm::result<basic_listen_handle<void>> listen(
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_listen_handle<void>;

	h_type h;
	//vsm_try_void(blocking_io(
	(tag_invoke(blocking_io,
		h,
		no_result,
		io_args<h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

template<typename Multiplexer>
vsm::result<basic_listen_handle<Multiplexer>> listen(
	Multiplexer&& multiplexer,
	network_endpoint const& endpoint,
	auto&&... args)
{
	using h_type = basic_listen_handle<Multiplexer>;

	h_type h;
	vsm_try_void(blocking_io(
		h,
		no_result,
		io_args<h_type::listen_t>(endpoint)(vsm_forward(args)...)));
	return h;
}

} // namespace allio::detail
