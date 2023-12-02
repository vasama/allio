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

	using read_some_t = byte_io::stream_read_t;
	using write_some_t = byte_io::stream_write_t;

	using operations = type_list_append
	<
		base_type::operations
		, read_some_t
		, write_some_t
	>;

	static vsm::result<void> create(
		native_type& r_h,
		native_type& w_h);

	static vsm::result<size_t> stream_read(
		native_type const& h,
		io_parameters_t<pipe_t, read_some_t> const& a);

	static vsm::result<size_t> stream_write(
		native_type const& h,
		io_parameters_t<pipe_t, write_some_t> const& a);

	template<typename Handle, optional_multiplexer_handle_for<pipe_t> MultiplexerHandle>
	struct concrete_interface : base_type::concrete_interface<Handle, MultiplexerHandle>
	{
		[[nodiscard]] auto read_some(read_buffer const buffer, auto&&... args) const
		{
			return generic_io<read_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<pipe_t, read_some_t>(buffer)(vsm_forward(args)...));
		}

		[[nodiscard]] auto write_some(write_buffer const buffer, auto&&... args) const
		{
			return generic_io<write_some_t>(
				static_cast<Handle const&>(*this),
				make_io_args<pipe_t, write_some_t>(buffer)(vsm_forward(args)...));
		}
	};
};
using abstract_pipe_handle = abstract_handle<pipe_t>;


namespace _pipe_b {

using pipe_handle = blocking_handle<pipe_t>;
using pipe_pair = basic_pipe_pair<pipe_handle>;

vsm::result<pipe_pair> create_pipe(auto&&... args)
{
	pipe_t::native_type r_h;
	pipe_t::native_type w_h;

	vsm_try_void(pipe_t::create(r_h, w_h));

	return vsm_lazy(pipe_pair
	{
		pipe_handle(adopt_handle, vsm_move(r_h)),
		pipe_handle(adopt_handle, vsm_move(w_h)),
	});
}

} // namespace _pipe_b

} // namespace allio::detail
