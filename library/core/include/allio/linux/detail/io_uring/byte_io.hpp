#pragma once

#include <allio/linux/detail/io_uring/multiplexer.hpp>

namespace allio::detail {

struct io_uring_byte_io_state_2
	: async_extension
	, io_uring_multiplexer::operation_type
{
	using M = io_uring_multiplexer;
	using H = native_handle<platform_object_t> const;
	using C = io_uring_multiplexer::connector_type const;
	using S = io_uring_byte_io_state_2;

	M::timeout timeout;

	static void cancel(M& m, H const& h, C const& c, S& s);
};

template<typename Operation>
struct io_uring_byte_io_state_1 : io_uring_byte_io_state_2
{
	using A = typename Operation::params_type;

	static io_result<size_t> submit(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler);

	static io_result<size_t> notify(
		M& m,
		H& h,
		C& c,
		S& s,
		A const& a,
		io_handler<M>& handler,
		M::io_status_type status);
};

template<typename Object, typename Operation>
struct io_uring_byte_io_state : io_uring_byte_io_state_1<Operation>
{
	static_assert(std::is_same_v<
		typename Operation::params_type,
		io_parameters_t<Object, Operation>>);
};

} // namespace allio::detail
