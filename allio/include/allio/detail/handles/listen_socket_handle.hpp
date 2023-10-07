#pragma once

#include <allio/detail/handles/stream_socket_handle.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

#include <optional>

namespace allio::detail {

template<typename SocketHandle>
struct basic_accept_result
{
	SocketHandle socket;
	network_endpoint endpoint;
};

using _accept_result = basic_accept_result<blocking_stream_socket_handle>;


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

class _listen_socket_handle : public common_socket_handle
{
protected:
	using base_type = common_socket_handle;

public:
	struct listen_t
	{
		using handle_type = _listen_socket_handle;
		using result_type = void;
		
		struct required_params_type
		{
			network_endpoint endpoint;
		};

		using optional_params_type = backlog_t;
	};

	struct accept_t
	{
		using handle_type = _listen_socket_handle const;
		using result_type = _accept_result;
		
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			accept_t
		>
	>;

protected:
	allio_detail_default_lifetime(_listen_socket_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		vsm::result<_accept_result> accept(auto&&... args) const
		{
			vsm::result<_accept_result> r(vsm::result_value);
			vsm_try_void(do_blocking_io(
				static_cast<H const&>(*this),
				r,
				io_arguments_t<accept_t>(&r->socket)(vsm_forward(args)...)));
			return r;
		}
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		io_sender<H, accept_t> accept_async(auto&&... args) const
		{
			return io_sender<H, accept_t>(
				static_cast<H const&>(*this),
				io_arguments_t<accept_t>()(vsm_forward(args)...));
		}
	};

protected:
	static vsm::result<void> do_blocking_io(
		_listen_socket_handle& h,
		io_result_ref_t<listen_t> result,
		io_parameters_t<listen_t> const& args);

	static vsm::result<void> do_blocking_io(
		_listen_socket_handle const& h,
		io_result_ref_t<accept_t> result,
		io_parameters_t<accept_t> const& args);
};

using blocking_listen_socket_handle = basic_blocking_handle<_listen_socket_handle>;

template<typename Multiplexer>
using basic_listen_socket_handle = basic_async_handle<_listen_socket_handle, Multiplexer>;


vsm::result<blocking_listen_socket_handle> listen(network_endpoint const& endpoint, auto&&... args)
{
	vsm::result<blocking_listen_socket_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(
		*r,
		no_result,
		io_arguments_t<_listen_socket_handle::listen_t>(endpoint)(vsm_forward(args)...)));
	return r;
}

} // namespace allio::detail
