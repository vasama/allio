#pragma once

#include <allio/detail/exceptions.hpp>

#include <vsm/concepts.hpp>
#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <span>
#include <string_view>

namespace allio {
namespace detail {

template<typename Encoding>
struct path_encoding_base
{
	using encoding = Encoding;
};

template<>
struct path_encoding_base<void>
{
};

} // namespace detail

struct get_path_string_t
{
	template<typename Path>
	vsm_static_operator decltype(auto) operator()(Path&& path) vsm_static_operator_const
		requires vsm::tag_invocable<get_path_string_t, Path&&>
	{
		return vsm::tag_invoke(get_path_string_t(), vsm_forward(path));
	}
};
inline constexpr get_path_string_t get_path_string = {};

template<typename Path>
using path_string_t = std::remove_cvref_t<decltype(get_path_string(std::declval<Path>()))>;

template<typename Path>
concept path_like = requires { typename path_string_t<Path>; };

template<typename Char>
struct path_combine_result_base
{
	std::basic_string_view<Char> m_lhs;
	std::basic_string_view<Char> m_rhs;
	bool m_requires_separator;

	[[nodiscard]] constexpr size_t size() const
	{
		return m_lhs.size() + m_rhs.size() + m_requires_separator;
	}

	[[nodiscard]] constexpr std::basic_string_view<Char> first() const
	{
		return m_lhs;
	}

	[[nodiscard]] constexpr std::basic_string_view<Char> second() const
	{
		return m_rhs;
	}

	[[nodiscard]] constexpr bool requires_separator() const
	{
		return m_requires_separator;
	}

	[[nodiscard]] constexpr std::basic_string_view<Char> copy(
		Char* const out,
		Char const separator) const
	{
		Char* const out_beg = out;
		Char* out_end = out_beg;

		out_end = std::copy(m_lhs.data(), m_lhs.data() + m_lhs.size(), out_end);
		if (m_requires_separator)
		{
			*out_end++ = separator;
		}
		out_end = std::copy(m_rhs.data(), m_rhs.data() + m_rhs.size(), out_end);

		return std::basic_string_view<Char>(out_beg, out_end);
	}
};

} // namespace allio
