#pragma once

#include <allio/detail/byte_io.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/platform.hpp>

namespace allio::detail {

struct standard_stream_t : object_t
{
	using base_type = object_t;

	struct impl_type;

	using operations = type_list_append
	<
		base_type::operations
		, byte_io::stream_read_t
		, byte_io::stream_write_t
	>;

	static vsm::result<void> stream_read(
		native_handle<standard_stream_t> const& h,
		io_parameters_t<standard_stream_t, stream_read_t> const& args);

	static vsm::result<void> stream_write(
		native_handle<standard_stream_t> const& h,
		io_parameters_t<standard_stream_t, stream_write_t> const& args);


	template<typename Handle, typename Traits>
	struct facade
		: base_type::facade<Handle, Traits>
		, byte_io::stream_facade<Handle, Traits>
	{
	};
};

template<>
struct native_handle<standard_stream_t> : standard_stream_t::base_type
{
	native_platform_handle platform_handle;
};

} // namespace allio::detail
