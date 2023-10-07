#pragma once

#include <allio/byte_io_buffers.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/io_parameters.hpp>
#include <allio/filesystem.hpp>
#include <allio/handle.hpp>

namespace allio {

struct random_access_byte_io_parameters
{
	untyped_buffers_storage buffers;
	file_size offset;
};

struct scatter_read_at_t
{
	using handle_type = handle const;
	using result_type = size_t;

	using required_params_type = random_access_byte_io_parameters;
	using optional_params_type = detail::deadline_t;
};

struct gather_write_at_t
{
	using handle_type = handle const;
	using result_type = size_t;

	using required_params_type = random_access_byte_io_parameters;
	using optional_params_type = detail::deadline_t;
};


struct stream_byte_io_parameters
{
	untyped_buffers_storage buffers;
};

struct scatter_read_t
{
	using handle_type = handle const;
	using result_type = size_t;

	using required_params_type = stream_byte_io_parameters;
	using optional_params_type = detail::deadline_t;
};

struct gather_write_t
{
	using handle_type = handle const;
	using result_type = size_t;

	using required_params_type = stream_byte_io_parameters;
	using optional_params_type = detail::deadline_t;
};

} // namespace allio
