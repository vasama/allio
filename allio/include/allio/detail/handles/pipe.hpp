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
		native_handle<pipe_t> const& h,
		io_parameters_t<pipe_t, stream_read_t> const& a);

	static vsm::result<size_t> stream_write(
		native_handle<pipe_t> const& h,
		io_parameters_t<pipe_t, stream_write_t> const& a);

	template<typename Handle, typename Traits>
	struct facade
		: base_type::facade<Handle, Traits>
		, byte_io::stream_facade<Handle, Traits>
	{
	};
};

struct pipe_pair_t : object_t
{
	using base_type = object_t;

	struct create_pair_t
	{
		using operation_concept = producer_t;
		using params_type = io_flags_t;
		using result_type = void;
		using runtime_concept = bounded_runtime_t;

		template<object Object>
		friend vsm::result<void> tag_invoke(
			blocking_io_t<create_pair_t>,
			native_handle<Object>& h,
			io_parameters_t<Object, create_pair_t> const& a)
			requires requires { Object::create_pair(h, a); }
		{
			return Object::create_pair(h, a);
		}
	};

	using operations = type_list_append
	<
		base_type::operations
		, create_pair_t
	>;

	static vsm::result<void> create_pair(
		native_handle<pipe_pair_t>& h,
		io_parameters_t<pipe_t, create_pair_t> const& a);

	static vsm::result<void> close(
		native_handle<pipe_pair_t>& h,
		io_parameters_t<pipe_t, close_t> const& a);
};

template<>
struct native_handle<pipe_pair_t> : native_handle<pipe_pair_t::base_type>
{
	native_handle<pipe_t> r_h;
	native_handle<pipe_t> w_h;
};

template<typename Traits>
[[nodiscard]] vsm::result<basic_pipe_pair<typename Traits::template handle<pipe_t>>> _create_pipe(
	io_parameters_t<pipe_pair_t, pipe_pair_t::create_pair_t> const& a)
{
	using handle_type = typename Traits::template handle<pipe_t>;

	basic_detached_handle<pipe_pair_t> h;
	vsm_try_void(blocking_io<pipe_pair_t::create_pair_t>(h, a));

	native_handle<pipe_pair_t> const n = h.release();
	return vsm::result<basic_pipe_pair<handle_type>>(
		vsm::result_value,
		handle_type(adopt_handle, n.r_h),
		handle_type(adopt_handle, n.w_h));
}

template<typename Traits>
[[nodiscard]] auto create_pipe(auto&&... args)
{
	auto a = io_parameters_t<pipe_pair_t, pipe_pair_t::create_pair_t>{};
	(set_argument(a, vsm_forward(args)), ...);
	auto r = _create_pipe<Traits>(a);

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}
} // namespace allio::detail
