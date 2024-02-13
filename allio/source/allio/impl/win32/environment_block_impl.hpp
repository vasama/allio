#pragma once

#include <allio/impl/win32/environment_block.hpp>

#include <vsm/lift.hpp>

namespace allio::win32 {

class environment_block_builder
{
	// Reserve one extra character for the null terminator.
	static constexpr size_t max_environment_block_size = 0x7ffe;
	static constexpr size_t dynamic_buffer_size = max_environment_block_size + 1;

	environment_block_storage& m_storage;

	wchar_t* m_buffer_beg;
	wchar_t* m_buffer_pos;
	wchar_t* m_buffer_end;

public:
	environment_block_builder(environment_block_builder const&) = delete;
	environment_block_builder& operator=(environment_block_builder const&) = delete;

	static [[nodiscard]] vsm::result<wchar_t*> make(
		environment_block_storage& storage,
		any_string_span const environment)
	{
		return environment_block_builder(storage)._make(environment);
	}

private:
	explicit environment_block_builder(environment_block_storage& storage)
		: m_storage(storage)
	{
	}

	[[nodiscard]] vsm::result<void> allocate_dynamic_buffer()
	{
		wchar_t* const new_buffer_beg = new (std::nothrow) wchar_t[dynamic_buffer_size];
		if (new_buffer_beg == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}
		m_storage.m_dynamic.reset(new_buffer_beg);

		size_t const old_data_size = m_buffer_pos - m_buffer_beg;
		memcpy(new_buffer_beg, m_buffer_beg, old_data_size * sizeof(wchar_t));

		m_buffer_beg = new_buffer_beg;
		m_buffer_pos = new_buffer_beg + old_data_size;
		m_buffer_end = new_buffer_beg + dynamic_buffer_size;

		return {};
	}

	[[nodiscard]] vsm::result<void> push_variable(std::wstring_view const variable)
	{
		if (variable.find(L'=') == std::wstring_view::npos)
		{
			return vsm::unexpected(error::invalid_environment_variable);
		}

		vsm_try(out, reserve(variable.size() + 1));
		memcpy(out, variables.data(), variables.size() * sizeof(wchar_t));
		out[variables.size()] = L'\0';

		return {};
	}

	[[nodiscard]] vsm::result<void> push_variable(std::string_view const variable)
	{
		if (variable.find('=') == std::string_view::npos)
		{
			return vsm::unexpected(error::invalid_environment_variable);
		}

		auto const r1 = transcode<wchar_t>(
			variable,
			std::span(m_buffer_pos, m_buffer_end));

		size_t encoded = r1.encoded;

		if (r1.ec != transcode_error{})
		{
			if (r1.ec != transcode_error::no_buffer_space)
			{
				return vsm::unexpected(error::invalid_encoding);
			}

			if (m_storage.m_dynamic != nullptr)
			{
				return vsm::unexpected(error::environment_variables_too_long);
			}

			auto const r2 = transcode_size<wchar_t>(
				variable.substr(r1.decoded),
				max_environment_block_size - r1.encoded - (m_buffer_pos - m_buffer_beg) - 1);

			if (r2.ec != transcode_error{})
			{
				return r2.ec == transcode_error::no_buffer_space
					? vsm::unexpected(error::environment_variables_too_long)
					: vsm::unexpected(error::invalid_encoding);
			}
			
			encoded += r2.encoded;
			
			
			vsm_try_void(allocate_dynamic_buffer());
			
			transcode_unchecked<wchar_t>(
				variable.substr(r1.decoded),
				std::span(m_buffer_pos + r1.encoded, m_buffer_end - 1));
		}

		m_buffer_pos += encoded;
		*m_buffer_pos++ = L'\0';

		return {};
	}

	[[nodiscard]] vsm::result<void> push_variable(std::basic_string_view<char8_t> const variable)
	{
		return push_variable(std::basic_string_view<char>(
			reinterpret_cast<char const*>(variable.data()),
			variable.size()));
	}

	[[nodiscard]] vsm::result<void> push_variable(std::basic_string_view<char16_t> const variable)
	{
		//TODO: This is technically UB.
		return push_variable(std::basic_string_view<wchar_t>(
			reinterpret_cast<wchar_t const*>(variable.data()),
			variable.size()));
	}

	[[nodiscard]] vsm::result<void> push_variable(std::basic_string_view<char32_t>)
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	[[nodiscard]] vsm::result<void> push_variable(detail::string_length_out_of_range_t)
	{
		return vsm::unexpected(error::environment_variables_too_long);
	}

	[[nodiscard]] vsm::result<void> push_variable(any_string_view const variable)
	{
		return argument.visit(vsm_lift_borrow(push_variable));
	}

	[[nodiscard]] vsm::result<wchar_t*> _make(any_string_span const environment)
	{
		auto const set_buffer = [&](wchar_t* const beg, size_t const size)
		{
			m_buffer_beg = beg;
			m_buffer_pos = beg;
			m_buffer_end = beg + size - 1;
		};
		
		if (m_storage.m_dynamic != nullptr)
		{
			set_buffer(m_storage.m_dynamic.get(), dynamic_buffer_size);
		}
		else
		{
			set_buffer(m_storage.m_static, std::size(m_storage.m_static));
		}
	
		for (any_string_view const variable : environment)
		{
			vsm_try_void(push_variable(variable));
		}

		*m_buffer_end = L'\0';
		return m_buffer_beg;
	}
};

} // namespace allio::win32
