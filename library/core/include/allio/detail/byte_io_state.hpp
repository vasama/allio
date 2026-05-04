#pragma once

#include <allio/detail/byte_io.hpp>

namespace allio::detail {

enum class byte_io_status : bool
{
	success,
	end_of_stream,
};

class byte_io_state
{
	size_t m_transferred;

public:
	byte_io_state() = default;

	[[nodiscard]] size_t transferred() const
	{
		return m_transferred;
	}

	void transfer_completed(size_t const transferred)
	{
		m_transferred += transferred;
	}

	template<typename Result, typename Arguments>
	[[nodiscard]] vsm::result<Result> finalize(
		Arguments const& a,
		vsm::result<byte_io_status> const& r)
	{
		std::same_as<io_flags> auto const flags = a.flags;

		if (r)
		{
			if (*r == byte_io_status::end_of_stream)
			{
				// End of stream is conservatively upgraded into an error when
				// * the result type is not read_result, or
				// * a request for greedy I/O was partially successful.

				if constexpr (std::is_same_v<Result, read_result>)
				{
					if (m_transferred == 0 || vsm::no_flags(flags, io_flags::greedy_byte_io))
					{
						return end_of_stream;
					}
				}

				return vsm::unexpected(error::end_of_stream);
			}
		}
		else
		{
			// The error is ignored if some data was transferred and greedy I/O was not requested.
			if (m_transferred == 0 || vsm::any_flags(flags, io_flags::greedy_byte_io))
			{
				return vsm::unexpected(r.error());
			}
		}

		return m_transferred;
	}
};

} // namespace allio::detail
