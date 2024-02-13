#pragma once

#include <allio/any_string.hpp>
#include <allio/impl/transcode.hpp>

#include <vsm/assert.h>
#include <vsm/result.hpp>

#include <memory>
#include <span>

#include <cstring>

namespace allio::win32 {

template<bool Forward>
class basic_api_string_builder;

class api_string_storage_base
{
protected:
	std::unique_ptr<wchar_t[]> m_dynamic;

	// Offset of the storage for the next string in the current buffer.
	uint32_t m_index : 24;

	// The number of strings for which space is dynamically allocated.
	uint32_t m_count : 8;

	explicit api_string_storage_base(uint8_t const count)
		: m_index(0)
		, m_count(count)
	{
	}

private:
	template<bool Forward>
	friend class basic_api_string_builder;
};

class api_string_storage : public api_string_storage_base
{
	wchar_t m_static[1024 - sizeof(api_string_storage_base) / sizeof(wchar_t)];

public:
	explicit api_string_storage(uint8_t const count = 1)
		: api_string_storage_base(count)
	{
		vsm_assert(count > 0);
	}

	api_string_storage(api_string_storage const&) = delete;
	api_string_storage& operator=(api_string_storage const&) = delete;

private:
	template<bool Forward>
	friend class basic_api_string_builder;
};

template<bool Forward>
class basic_api_string_builder
{
	// Limited by the range of UNICODE_STRING::Length.
	static constexpr size_t max_string_size =
		std::numeric_limits<unsigned short>::max() / sizeof(wchar_t);

	api_string_storage& m_storage;
	bool m_insert_null_terminator;

	wchar_t* m_beg;
	wchar_t* m_pos;
	wchar_t* m_end;

public:
	using iterator_pair = std::pair<wchar_t*, wchar_t*>;

	explicit basic_api_string_builder(api_string_storage& storage, bool const insert_null_terminator)
		: m_storage(storage)
		, m_insert_null_terminator(insert_null_terminator)
	{
		auto const set_buffer = [&](wchar_t* const buffer, size_t const buffer_size)
		{
			m_beg = buffer + storage.m_index;
			m_pos = buffer + storage.m_index;
			m_end = buffer + buffer_size - insert_null_terminator;
		};

		if (storage.m_dynamic != nullptr)
		{
			set_buffer(storage.m_dynamic.get(), max_string_size - storage.m_index);
		}
		else
		{
			set_buffer(storage.m_static, std::size(m_storage.m_static) - storage.m_index);
		}
	}

	basic_api_string_builder(basic_api_string_builder const&) = delete;
	basic_api_string_builder& operator=(basic_api_string_builder const&) = delete;


	[[nodiscard]] vsm::result<void> allocate_dynamic_buffer()
	{
		if (m_storage.m_dynamic != nullptr)
		{
			return vsm::unexpected(error::argument_string_too_long);
		}

		size_t const max_size = m_storage.m_count * max_string_size;
		wchar_t* const new_beg = new (std::nothrow) wchar_t[max_size];

		if (new_beg == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		m_storage.m_dynamic.reset(new_beg);
		m_storage.m_index = 0;

		size_t const old_size = m_pos - m_beg;
		memcpy(new_beg, m_beg, old_size * sizeof(wchar_t));

		m_beg = new_beg;
		m_pos = new_beg + old_size;
		m_end = new_beg + max_string_size - m_insert_null_terminator;

		return {};
	}

	[[nodiscard]] vsm::result<iterator_pair> reserve_space(size_t const size)
	{
		if (size > static_cast<size_t>(m_end - m_pos))
		{
			size_t const old_size = static_cast<size_t>(m_pos - m_beg);

			if (old_size + size > max_string_size - m_insert_null_terminator)
			{
				return vsm::unexpected(error::argument_string_too_long);
			}

			vsm_try_void(allocate_dynamic_buffer());
		}

		wchar_t* const beg = m_pos;
		wchar_t* const end = m_pos + size;
		m_pos = end;

		return std::pair<wchar_t*, wchar_t*>{ beg, end };
	}

	[[nodiscard]] vsm::result<void> push(std::wstring_view const string)
	{
		vsm_try_bind((out_beg, out_end), reserve_space(string.size()));
		memcpy(out_beg, string.data(), string.size() * sizeof(wchar_t));
		return {};
	}

	[[nodiscard]] vsm::result<iterator_pair> transcode_temporary(
		std::string_view const string,
		size_t const reserve_suffix = 0)
	{
		auto const r1 = transcode<wchar_t>(
			string,
			std::span(m_pos, m_end));

		size_t encoded = r1.encoded;

		if (r1.ec != transcode_error{})
		{
			if (r1.ec != transcode_error::no_buffer_space)
			{
				return vsm::unexpected(error::invalid_encoding);
			}

			if (m_storage.m_dynamic != nullptr)
			{
				return vsm::unexpected(error::invalid_argument);
			}

			size_t const max_size = max_string_size - m_insert_null_terminator - reserve_suffix;
			vsm_assert(static_cast<size_t>(m_pos - m_beg) + r1.encoded <= max_size);

			auto const r2 = transcode_size<wchar_t>(
				string.substr(r1.decoded),
				max_size - (static_cast<size_t>(m_pos - m_beg) + r1.encoded));

			if (r2.ec != transcode_error{})
			{
				return r2.ec == transcode_error::no_buffer_space
					? vsm::unexpected(error::invalid_argument)
					: vsm::unexpected(error::invalid_encoding);
			}

			vsm_try_void(allocate_dynamic_buffer());

			transcode_unchecked<wchar_t>(
				string.substr(r1.decoded),
				std::span(m_pos + r1.encoded, m_end));

			encoded += r2.encoded;
		}

		return iterator_pair{ m_pos, m_pos + encoded };
	}

	[[nodiscard]] vsm::result<void> push(std::string_view const string)
	{
		vsm_try_bind((out_beg, out_end), transcode_temporary(string));
		m_pos = out_end;
		return {};
	}

	[[nodiscard]] iterator_pair finalize()
	{
		if (m_insert_null_terminator)
		{
			*m_pos++ = L'\0';
		}

		m_storage.m_index += static_cast<uint32_t>(m_pos - m_beg);

		return { m_beg, m_pos };
	}


	[[nodiscard]] static vsm::result<void> visit(any_string_view const string, auto&& callable)
	{
		return string.visit([&](auto&& argument)
		{
			return _visit(vsm_forward(callable), vsm_forward(argument));
		});
	}

private:
	[[nodiscard]] static vsm::result<void> _visit(auto&& callable, std::basic_string_view<char> const string)
	{
		return vsm_forward(callable)(string);
	}

	[[nodiscard]] static vsm::result<void> _visit(auto&& callable, std::basic_string_view<wchar_t> const string)
	{
		return vsm_forward(callable)(string);
	}

	[[nodiscard]] static vsm::result<void> _visit(auto&& callable, std::basic_string_view<char8_t> const string)
	{
		return vsm_forward(callable)(std::string_view(reinterpret_cast<char const*>(string.data()), string.size()));
	}

	[[nodiscard]] static vsm::result<void> _visit(auto&& callable, std::basic_string_view<char16_t> const string)
	{
		//TODO: This is technically UB.
		return vsm_forward(callable)(std::wstring_view(reinterpret_cast<wchar_t const*>(string.data()), string.size()));
	}

	[[nodiscard]] static vsm::result<void> _visit(auto&&, std::basic_string_view<char32_t>)
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	[[nodiscard]] static vsm::result<void> _visit(auto&&, detail::string_length_out_of_range_t)
	{
		return vsm::unexpected(error::argument_string_too_long);
	}
};

using api_string_builder = basic_api_string_builder<true>;
using api_string_reverse_builder = basic_api_string_builder<false>;

vsm::result<wchar_t const*> make_api_string(api_string_storage& storage, any_string_view string);

} // namespace allio::win32
