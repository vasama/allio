#pragma once

#include <allio/impl/win32/completion_port.hpp>

#include <catch2/catch_all.hpp>

#include <algorithm>
#include <span>

namespace allio::test {

template<size_t Size>
class completion_storage
{
	win32::FILE_IO_COMPLETION_INFORMATION m_completions[Size];
	size_t m_completion_count = 0;

public:
	size_t remove(
		detail::unique_handle const& completion_port,
		size_t const max_completions = static_cast<size_t>(-1))
	{
		size_t const count = win32::remove_io_completions(
			completion_port.get(),
			std::span(m_completions).subspan(m_completion_count, max_completions),
			std::chrono::milliseconds(100)).value();
		m_completion_count += count;
		return count;
	}

	win32::FILE_IO_COMPLETION_INFORMATION const& get(void const* const apc_context) const
	{
		auto const beg = m_completions;
		auto const end = m_completions + m_completion_count;
		auto const it = std::find_if(beg, end, [&](auto const& completion)
		{
			return completion.ApcContext == apc_context;
		});
		REQUIRE(it != end);
		return *it;
	}

	win32::FILE_IO_COMPLETION_INFORMATION const& get(IO_STATUS_BLOCK const& io_status_block) const
	{
		return get(&io_status_block);
	}

	win32::FILE_IO_COMPLETION_INFORMATION const& get(OVERLAPPED const& overlapped) const
	{
		return get(&overlapped);
	}
};

} // namespace allio::test
