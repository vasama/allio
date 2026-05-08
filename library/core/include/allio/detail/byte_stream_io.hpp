#pragma once

#include <allio/any_byte_buffer.hpp>
#include <allio/detail/block.hpp>
#include <allio/detail/byte_io.hpp>
#include <allio/error.hpp>
#include <allio/step_deadline.hpp>

#include <optional>

namespace allio::detail {

struct file_t;

namespace file_io {

struct tell_t;
struct get_maximum_extent_t;

} // namespace file_io

template<typename Handle>
vsm::result<size_t> _read_to_end_f(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	auto const a = detail::make_args<deadline_t>(vsm_forward(args)...);

	vsm_try(maximum_extent, detail::_block<file_io::get_maximum_extent_t>(h));
	vsm_try(current_offset, detail::_block<file_io::tell_t>(h));

	fs_size const remaining_size = maximum_extent - current_offset;
	if (remaining_size > std::numeric_limits<size_t>::max())
	{
		return vsm::unexpected(error::io_size_out_of_range);
	}

	vsm_try(storage, buffer.resize(static_cast<size_t>(remaining_size)));

	return detail::_block<byte_io::stream_read_t>(
		storage,
		a.deadline,
		set_io_flags_t{ io_flags::greedy_byte_io });
}

template<typename Handle>
vsm::result<size_t> _read_to_end_s(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	step_deadline deadline = detail::make_args<deadline_t>(vsm_forward(args)...).deadline;

	size_t transferred = 0;
	vsm_try(storage, buffer.resize(1, static_cast<size_t>(-1)));

	while (true)
	{
		vsm_try(local_deadline, deadline.step());

		auto a = io_parameters_t<typename Handle::object_type, byte_io::stream_read_t>{};
		a.buffers = storage.subspan(transferred);
		a.deadline = local_deadline;

		vsm_try(result, detail::blocking_io<byte_io::stream_read_t>(h, a));

		if (result.is_end_of_stream())
		{
			break;
		}

		transferred += result.size();

		if (transferred == storage.size())
		{
			size_t const add_size = std::max(transferred / 2, static_cast<size_t>(1));
			size_t const min_size = transferred <= std::numeric_limits<size_t>::max() - add_size
				? transferred + add_size
				: static_cast<size_t>(-1);

			vsm_try_assign(storage, buffer.resize(min_size, static_cast<size_t>(-1)));
		}
	}

	if (transferred != storage.size())
	{
		vsm_try_discard(buffer.resize(transferred));
	}

	return transferred;
}

template<typename Handle>
vsm::result<size_t> _read_to_end_x(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	if constexpr (std::is_same_v<typename Handle::object_type, file_t>)
	{
		return detail::_read_to_end_f(h, buffer, vsm_forward(args)...);
	}
	else
	{
		return detail::_read_to_end_s(h, buffer, vsm_forward(args)...);
	}
}

template<typename Container, typename Handle>
vsm::result<Container> _read_to_end_c(Handle const& h, auto&&... args)
{
	vsm::result<Container> r(vsm::result_value);
	if (auto const r2 = detail::_read_to_end_x(h, *r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	else
	{
		if constexpr (sizeof(typename Container::value_type) != 1)
		{
			if (*r2 % sizeof(typename Container::value_type) != 0)
			{
				r = vsm::unexpected(error::invalid_data_format);
			}
		}
	}
	return r;
}

} // namespace allio::detail
