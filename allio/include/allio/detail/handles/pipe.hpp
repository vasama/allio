#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/platform_object.hpp>

#include <vsm/lazy.hpp>

namespace allio::detail {

template<typename PipeHandle>
struct basic_pipe_pair
{
	PipeHandle read_pipe;
	PipeHandle write_pipe;
};

namespace pipe_io {

struct create_pair_t
{
	using operation_concept = producer_t;
	using params_type = io_flags_t;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, create_pair_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, create_pair_t> const& a)
		requires requires { Object::create_pair(h, a); }
	{
		return Object::create_pair(h, a);
	}
};

} // namespace pipe_io

struct pipe_t : platform_object_t
{
	using base_type = platform_object_t;

	using stream_read_t = byte_io::stream_read_t;
	using stream_write_t = byte_io::stream_write_t;

	using operations = type_list_append
	<
		base_type::operations
		, stream_read_t
		, stream_write_t
	>;

	static vsm::result<size_t> stream_read(
		native_type const& h,
		io_parameters_t<pipe_t, stream_read_t> const& a);

	static vsm::result<size_t> stream_write(
		native_type const& h,
		io_parameters_t<pipe_t, stream_write_t> const& a);

	template<typename Handle>
	struct concrete_interface
		: base_type::concrete_interface<Handle>
		, byte_io::stream_interface<Handle>
	{
	};
};

struct pipe_pair_t : object_t
{
	using base_type = object_t;

	struct native_type : base_type::native_type
	{
		pipe_t::native_type r_h;
		pipe_t::native_type w_h;
	};

	using create_pair_t = pipe_io::create_pair_t;

	using operations = type_list_append
	<
		base_type::operations
		, create_pair_t
	>;

	static vsm::result<void> create_pair(
		native_type& h,
		io_parameters_t<pipe_t, create_pair_t> const& a);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<pipe_t, close_t> const& a);
};

template<typename IoTraits>
[[nodiscard]] vsm::result<basic_pipe_pair<basic_detached_handle<pipe_t, IoTraits>>> _create_pipe(
	io_parameters_t<pipe_pair_t, pipe_io::create_pair_t> const& a)
{
	basic_detached_handle<pipe_pair_t, IoTraits> h;
	vsm_try_void(blocking_io<pipe_pair_t, pipe_io::create_pair_t>(h, a));

	pipe_pair_t::native_type const n = h.release();
	return vsm::result<basic_pipe_pair<basic_detached_handle<pipe_t, IoTraits>>>(
		vsm::result_value,
		basic_detached_handle<pipe_t, IoTraits>(adopt_handle, n.r_h),
		basic_detached_handle<pipe_t, IoTraits>(adopt_handle, n.w_h));
}

} // namespace allio::detail
