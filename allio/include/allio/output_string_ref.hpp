#pragma once

#include <allio/detail/platform.h>
#include <allio/detail/string_ref.hpp>
#include <allio/error.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>

#include <span>
#include <string>

namespace allio {
namespace detail {

template<typename Container, typename Char>
concept output_string_container = requires (Container& container, size_t const size)
{
	{ container.data() } -> std::same_as<Char*>;
	{ container.size() } -> std::same_as<size_t>;
	container.resize(size);
};

class output_string_ref
{
	struct buffer
	{
		void* data;
		size_t size;
	};

	typedef vsm::result<buffer> resize_callback(output_string_ref const& self, size_t size, bool oversize);

	static constexpr size_t wide_flag = ~(static_cast<size_t>(-1) >> 1);
	static constexpr size_t cstr_flag = wide_flag >> 1;

	static constexpr size_t size_mask = cstr_flag - 1;

	void* m_context;
	size_t m_size_and_flags;
	resize_callback* m_resize;

public:
	bool is_c_string() const
	{
		return (m_size_and_flags & cstr_flag) != 0;
	}


	output_string_ref(std::span<char> const buffer)
		: output_string_ref(buffer, false)
	{
	}

	explicit output_string_ref(std::span<char> const buffer, bool const c_string)
		: m_context(buffer.data())
		, m_size_and_flags(std::min(buffer.size(), size_mask) | (c_string ? cstr_flag : 0))
		, m_resize(resize_buffer<char>)
	{
	}

	template<output_string_container<char> Container>
	output_string_ref(Container& container)
		: output_string_ref(container, false)
	{
	}

	template<output_string_container<char> Container>
	explicit output_string_ref(Container& container, bool const c_string)
		: m_context(&container)
		, m_size_and_flags((c_string ? cstr_flag : 0))
		, m_resize(resize_container<Container>)
	{
	}

	bool is_utf8() const
	{
		return (m_size_and_flags & wide_flag) == 0;
	}

	vsm::result<std::span<char>> resize_utf8(size_t const size, bool const oversize = false) const
	{
		vsm_assert(is_utf8());
		return resize_internal<char>(size, oversize);
	}


	output_string_ref(std::span<wchar_t> const buffer)
		: output_string_ref(buffer, false)
	{
	}

	explicit output_string_ref(std::span<wchar_t> const buffer, bool const c_string)
		: m_context(buffer.data())
		, m_size_and_flags(std::min(buffer.size(), size_mask) | (c_string ? cstr_flag : 0) | wide_flag)
		, m_resize(resize_buffer<wchar_t>)
	{
	}

	template<output_string_container<wchar_t> Container>
	output_string_ref(Container& container)
		: output_string_ref(container, false)
	{
	}

	template<output_string_container<wchar_t> Container>
	explicit output_string_ref(Container& container, bool const c_string)
		: m_context(&container)
		, m_size_and_flags((c_string ? cstr_flag : 0) | wide_flag)
		, m_resize(resize_container<Container>)
	{
	}

	bool is_wide() const
	{
		return (m_size_and_flags & wide_flag) != 0;
	}

	vsm::result<std::span<wchar_t>> resize_wide(size_t const size, bool const oversize = false) const
	{
		vsm_assert(is_wide());
		return resize_internal<wchar_t>(size, oversize);
	}

private:
	template<typename Char>
	vsm::result<std::span<Char>> resize_internal(size_t const size, bool const oversize) const
	{
		vsm_try(buffer, m_resize(*this, size, oversize));
		return std::span(reinterpret_cast<Char*>(buffer.data), buffer.size);
	}

	template<typename Char>
	static vsm::result<buffer> resize_buffer(output_string_ref const& self, size_t const size, bool const oversize)
	{
		size_t const buffer_size = self.m_size_and_flags & size_mask;

		if (size > buffer_size)
		{
			return vsm::unexpected(error::no_buffer_space);
		}

		return buffer{ self.m_context, oversize ? buffer_size : size };
	}

	//TODO: Use customization point instead.
	template<typename Container>
	static vsm::result<buffer> resize_container(output_string_ref const& self, size_t const size, bool const oversize)
	{
		Container& container = *reinterpret_cast<Container*>(self.m_context);

		try
		{
			auto const resize = [&](size_t const size)
			{
				if constexpr (requires (size_t(f)(...)) { container.resize_and_overwrite(0, f); })
				{
					container.resize_and_overwrite(size, [](auto* const data, size_t const size)
					{
						return size;
					});
				}
				else
				{
					container.resize(size);
				}
			};

			resize(size);

			if (oversize && container.capacity() > container.size())
			{
				resize(container.capacity());
			}
		}
		catch (std::bad_alloc const&)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		return buffer{ container.data(), container.size() };
	}
};

} // namespace detail

using detail::output_string_ref;

} // namespace allio
