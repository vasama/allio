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

#if 0
template<typename Traits>
class _read_to_end_f
{
	any_byte_buffer m_buffer;
	deadline m_deadline;

public:
	using operations = type_list<stream_read_t>;

	explicit _read_to_end_f(any_byte_buffer const buffer, auto&&... args)
		: m_buffer(buffer)
		, m_deadline(detail::make_args<deadline_t>(vsm_forward(args)...).deadline)
	{
	}

	template<typename Handle, typename Loop>
	vsm::result<std::optional<size_t>> operator()(Handle const& h, Loop& loop)
	{
		if (auto* const r = loop.template get_if<stream_read_t>())
		{
			return *r;
		}

		vsm_try(current_offset, nothrow::block<tell_t>(h));
		vsm_try(maximum_extent, nothrow::block<get_maximum_extent_t>(h));

		fs_size const remaining_size = maximum_extent - current_offset;
		if (remaining_size > std::numeric_limits<size_t>::max())
		{
			return vsm::unexpected(error::io_size_out_of_range);
		}

		vsm_try(storage, buffer.resize(static_cast<size_t>(remaining_size)));
		return loop.template submit_io<stream_read_t>(storage, m_deadline, greedy_byte_io);
	}
};

template<typename Traits>
class _read_to_end_s
{
	any_byte_buffer m_buffer;
	step_deadline m_deadline;
	size_t m_transferred = 0;

public:
	using operations = type_list<stream_read_t>;

	explicit _read_to_end_s(any_byte_buffer const buffer, auto&&... args)
		: m_buffer(buffer)
		, m_deadline(detail::make_args<deadline_t>(vsm_forward(args)...).deadline)
	{
	}

	template<typename Handle, typename Loop>
	vsm::result<std::optional<size_t>> operator()(Handle&&, Loop& loop)
	{
		size_t buffer_size_growth = 0;
		if (auto* const r = loop.template get_if<stream_read_t>())
		{
			if (*r)
			{
				m_transferred += **r;

				if (m_transferred == static_cast<size_t>(-1))
				{
					return vsm::unexpected(error::io_size_out_of_range);
				}

				buffer_size_growth = std::min(
					m_transferred / 2,
					static_cast<size_t>(-1) - m_transferred);
			}
			else if (detail::is_error_code(r.error(), error::end_of_stream))
			{
				vsm_try_discard(m_buffer.resize(m_transferred, m_transferred));
				return m_transferred;
			}
			else
			{
				return vsm::unexpected(r.error());
			}
		}

		vsm_try(deadline, m_deadline.step());

		size_t const min_buffer_size = m_transferred + buffer_size_growth;
		vsm_try(storage, m_buffer.resize(min_buffer_size, static_cast<size_t>(-1)));

		auto const buffer = storage.data().subspan(m_transferred);
		return loop.template submit<stream_read_t>(buffer, deadline);
	}
};

template<typename Traits, typename Handle>
auto _read_to_end(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	constexpr bool is_file = std::is_same_v<typename Handle::object_type, file_t>;
	using loop_type = vsm::select_t<is_file, _read_to_end_f, _read_to_end_s>;
	return Traits::template loop<loop_type>(h, buffer, vsm_forward(args)...);
}
#endif

template<typename Handle>
vsm::result<size_t> _read_to_end_f(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	auto const a = detail::make_args<deadline_t>(vsm_forward(args)...);

	vsm_try(maximum_extent, detail::_block<file_t::get_maximum_extent_t>(h));
	vsm_try(current_offset, detail::_block<file_t::tell_t>(h));

	fs_size const remaining_size = maximum_extent - current_offset;
	if (remaining_size > std::numeric_limits<size_t>::max())
	{
		return vsm::unexpected(error::io_size_out_of_range);
	}

	vsm_try(storage, buffer.resize(static_cast<size_t>(remaining_size)));

	return detail::_block<byte_io::stream_read_t>(
		storage,
		a.deadline,
		set_io_flags_t(io_flags::greedy_byte_io));
}

template<typename Handle>
vsm::result<size_t> _read_to_end_s(Handle const& h, any_byte_buffer const buffer, auto&&... args)
{
	step_deadline deadline = detail::make_args<deadline_t>(vsm_forward(args)...).deadline;

	size_t transferred = 0;
	size_t grow_buffer = 1;

	while (true)
	{
		vsm_try(local_deadline, deadline.step());

		size_t const min_buffer_size = transferred + grow_buffer;
		vsm_try(storage, buffer.resize(min_buffer_size, static_cast<size_t>(-1)));

		auto a = io_parameters_t<typename Handle::object_type, byte_io::stream_read_t>{};
		a.buffers = storage.subspan(transferred);
		a.deadline = local_deadline;
		auto const r = detail::blocking_io<byte_io::stream_read_t>(h, a);

		if (!r)
		{
			if (detail::is_error_code(r.error(), error::end_of_stream))
			{
				break;
			}

			return vsm::unexpected(r.error());
		}

		transferred += *r;
		grow_buffer = std::min(transferred / 2, static_cast<size_t>(-1) - transferred);
	}

	vsm_try_discard(buffer.resize(transferred));

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
				//TODO: Use a proper error code:
				r = vsm::unexpected(error::unknown_failure);
			}
		}
	}
	return r;
}

} // namespace allio::detail
