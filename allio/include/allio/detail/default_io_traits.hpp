#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct exception_io_traits;

template<object Object>
using blocking_handle = basic_detached_handle<Object, exception_io_traits>;

template<object Object, multiplexer_handle_for<Object> MultiplexerHandle>
using sender_handle = basic_attached_handle<Object, exception_io_traits, MultiplexerHandle>;

struct exception_io_traits : exception_error_traits
{
	template<observer Operation, object Object>
	static io_result_t<Object, Operation> observe(blocking_handle<Object> const& h, auto&& args)
	{
		return throw_on_error(blocking_io<Object, Operation>(h, vsm_forward(args)));
	}

	template<observer Operation, object Object, multiplexer_handle_for<Object> MultiplexerHandle>
	static ex::sender auto observe(sender_handle<Object, MultiplexerHandle> const& h, auto&& args)
	{
		return io_sender<Object, exception_io_traits, MultiplexerHandle, Operation>(h, vsm_forward(args));
	}
};

struct blocking_io_traits : exception_io_traits
{
	template<object Object, producer Operation>
	static blocking_handle<Object> produce(auto&& args)
	{
		blocking_handle<Object> h;
		throw_on_error(blocking_io<Object, Operation>(h, vsm_forward(args)));
		return h;
	}
};

struct sender_io_traits : exception_io_traits
{
	template<object Object, producer Operation>
	static ex::sender auto produce(auto&& args)
	{
		return io_handle_sender<Object, exception_io_traits, Operation>(vsm_forward(args));
	}
};

} // namespace allio::detail
