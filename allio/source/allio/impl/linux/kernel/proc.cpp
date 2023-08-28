#include <allio/linux/impl/kernel/proc.hpp>

#include <vsm/lazy.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

vsm::result<proc_scanner> proc_scanner::open(char const* const path)
{
	int const fd = open(path, O_RDONLY | O_CLOEXEC, 0);

	if (fd == -1)
	{
		return vsm::unexpected(get_last_error());
	}
	
	return vsm_lazy(proc_scanner(fd));
}

vsm::result<std::string_view> proc_scanner::_scan(std::string_view const field)
{
	while (true)
	{
		vsm_try(match, scan_string(field));
		if (match)
		{
			// Skip over the colon.
			{
				vsm_try(data, read_some());
				if (data.front() != ':')
				{
					return vsm::unexpected(error::unknown);
				}
				advance(1);
			}
			
			// Skip over the whitespace.
			while (true)
			{
				vsm_try(data, read_some());
				advance(std::ranges::count(data, ' '));
			}
			
			size_t value_size = 0;

			// Read until end of the line.
			{
				vsm_try(data, read_some());
				size_t const offset = data.find('\n');
				if (offset == std::string_view::npos)
				{
					value_size += data.size();
					while (true)
					{
						vsm_try(data, read_more());
						size_t const offset = data.find('\n');
						if (offset == std::string_view::npos)
						{
							value_size += data.size();
						}
						else
						{
							value_size += offset;
							break;
						}
					}
				}
				else
				{
					value_size += offset;
				}
			}
			
			size_t const value_offset = advance(value_size + 1);
			return std::string_view(get_buffer().subspan(value_offset, value_size));
		}
		else
		{
			// Skip until end of the line.
			while (true)
			{
				vsm_try(data, read_some());
				size_t const offset = data.find('\n');
				if (offset != std::string_view::npos)
				{
					advance(offset);
					break;
				}
				advance(data.size());
			}
		}
	}
}

vsm::result<bool> proc_scanner::scan_string(std::string_view const string)
{
	std::span<char> const buffer(m_buffer);

	size_t equal_offset = 0;
	while (true)
	{
		std::string_view const data = std::string_view(buffer.sunspan(m_data_beg, m_data_end));
		size_t const equal_prefix_size = equal_prefix(data, string.substring(equal_offset));

		if (equal_prefix_size < data.size())
		{
			equal_offset = 0;
			continue;
		}
		equal_offset = equal_prefix_size;

		vsm_try(read_size, read(buffer));

		m_data_beg = 0;
		m_data_end = read_size;
	}
}

vsm::result<std::string_view> proc_scanner::read_some()
{
	auto const buffer = get_buffer();

	if (m_data_beg != m_data_end)
	{
		return buffer.subspan(m_data_beg, m_data_end - m_data_beg);
	}

	vsm_try(read_size, read(buffer.subspan(m_data_end)))
	m_data_end += read_size;

	return std::string_view(buffer.subspan(m_data_beg, m_data_end - m_data_beg));
}

vsm::result<std::string_view> proc_scanner::read_more()
{
	auto buffer = get_buffer();

	size_t const data_size = m_data_end - m_data_beg;

	if (m_data_beg != 0)
	{
		memmove(buffer.data(), buffer.data() + m_data_beg, data_size);
		m_data_beg = 0;
		m_data_end = data_size;
	}
	else if (data_size == buffer.size())
	{
		size_t const new_buffer_size = buffer.size() * 2;
		void* const new_buffer_storage = operator new(new_buffer_size, std::nothrow);

		if (new_buffer_storage == nullptr)
		{
			return vsm::unexpected(error::out_of_memory);
		}

		char* const new_buffer = new (new_buffer_storage) char[new_buffer_size];
		memcpy(new_buffer, buffer.data(), data_size);

		m_dynamic = std::unique_ptr<char>(new_buffer);
		m_dynamic_size = new_buffer_size;

		buffer = std::span<char>(new_buffer, new_buffer_size);
	}

	vsm_assert(m_data_beg == 0);
	vsm_assert(m_data_end == data_size);

	vsm_try(read_size, read(buffer.subspan(m_data_end)));
	m_data_end += read_size;

	return std::string_view(buffer.subspan(data_size));
}

std::span<char> proc_scanner::get_buffer()
{
	return m_dynamic
		? std::span<char>(m_dynamic.get(), m_dynamic_size)
		: std::span<char>(m_static);
}

void proc_scanner::advance(size_t const size)
{
	vsm_assert(size < m_data_end - m_data_beg);
	return std::exchange(m_data_beg, m_data_beg + size);
}

vsm::result<size_t> proc_scanner::read(std::span<char> const buffer) const
{
	ssize_t const size = ::read(
		m_fd.get(),
		buffer.data(),
		buffer.size());

	if (size == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return static_cast<size_t>(size);
}
