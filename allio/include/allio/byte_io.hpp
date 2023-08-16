#pragma once

#include <allio/byte_io_buffers.hpp>

namespace allio {
namespace detail {

struct scatter_gather_parameters
{
	using handle_type = handle const;
	using result_type = size_t;
	using params_type = deadline_parameters;

	detail::untyped_buffers_storage buffers;
	file_size offset;
};

} // namespace detail


struct scatter_read_at_tag;
struct gather_write_at_tag;

using random_access_scatter_gather_tags = type_list
<
	scatter_read_at_tag,
	gather_write_at_tag
>;



struct stream_scatter_read_tag;
struct stream_gather_write_tag;

using stream_scatter_gather_tags = type_list
<
	stream_scatter_read_tag,
	stream_gather_write_tag
>;

} // namespace allio
