#pragma once

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/detail/filesystem.hpp>

#include <cstring>

namespace allio {

void memzero_explicit(void* data, size_t size);

template<typename Container>
class basic_secret
{
	Container m_container;

public:
	basic_secret() = default;

	basic_secret(basic_secret&& other)
		: m_container(vsm_move(other.m_container))
	{
		if (other.m_container.size() != 0)
		{
			zero_container(other.m_container);
		}
	}

	basic_secret& operator=(basic_secret&& other) &
	{
		m_container = vsm_move(other.m_container);
		if (other.m_container.size() != 0)
		{
			zero_container(other.m_container);
		}
		return *this;
	}

	~basic_secret()
	{
		zero_container(m_container);
	}

	[[nodiscard]] Container& get()
	{
		return m_container;
	}

	[[nodiscard]] Container const& get() const
	{
		return m_container;
	}

	[[nodiscard]] Container* operator->()
	{
		return &m_container;
	}

	[[nodiscard]] Container const* operator->() const
	{
		return &m_container;
	}

private:
	static void zero_container(Container& container)
	{
		memzero_explicit(
			container.data(),
			container.size() * sizeof(typename Container::value_type));
	}
};

using secret = basic_secret<detail::dynamic_buffer<std::byte>>;

vsm::result<secret> read_secret_from_file(detail::fs_path path);

} // namespace allio
