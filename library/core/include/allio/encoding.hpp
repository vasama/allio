#pragma once

#include <vsm/concepts.hpp>
#include <vsm/platform.h>
#include <vsm/utility.hpp>

#include <span>
#include <string_view>

#include <cstdint>

namespace allio {

enum class encoding_family : uint8_t
{
	none,
	utf,
};

struct no_encoding_t
{
	explicit no_encoding_t() = default;
};
inline constexpr no_encoding_t no_encoding{};

struct utf_encoding_t
{
	explicit utf_encoding_t() = default;
};
inline constexpr utf_encoding_t utf_encoding{};


enum class char_type : uint8_t
{
	_char,
	_wchar_t,
	_char8_t,
	_char16_t,
	_char32_t,
};

namespace detail {

using vsm::character;


inline size_t get_char_type_size(char_type const type)
{
	switch (type)
	{
	case char_type::_char:
		return sizeof(char);
	case char_type::_wchar_t:
		return sizeof(wchar_t);
	case char_type::_char8_t:
		return sizeof(char8_t);
	case char_type::_char16_t:
		return sizeof(char16_t);
	case char_type::_char32_t:
		return sizeof(char32_t);
	}
	vsm_unreachable();
}


template<typename Char>
struct char_traits;

template<>
struct char_traits<char>
{
	static constexpr char_type type = char_type::_char;
};

template<>
struct char_traits<wchar_t>
{
	static constexpr char_type type = char_type::_wchar_t;
};

template<>
struct char_traits<char8_t>
{
	static constexpr char_type type = char_type::_char8_t;
};

template<>
struct char_traits<char16_t>
{
	static constexpr char_type type = char_type::_char16_t;
};

template<>
struct char_traits<char32_t>
{
	static constexpr char_type type = char_type::_char32_t;
};

template<typename Char>
inline constexpr char_type char_type_of = char_traits<Char>::type;


struct default_encoding_t
{
	explicit default_encoding_t() = default;
};

template<bool HasEncoding>
struct container_encoding;

template<>
struct container_encoding<0>
{
	template<typename Container, typename DefaultEncoding>
	using type = DefaultEncoding;
};

template<>
struct container_encoding<1>
{
	template<typename Container, typename DefaultEncoding>
	using type = typename Container::encoding;
};

template<typename Container>
concept container_has_encoding = requires { typename Container::encoding; };

template<typename Container, typename DefaultEncoding>
using container_encoding_or_t =
	container_encoding<container_has_encoding<Container>>
		::template type<Container, DefaultEncoding>;

template<typename Container, typename Encoding>
concept container_encoding_none_or =
	std::is_same_v<container_encoding_or_t<Container, Encoding>, Encoding>;

template<vsm::any_of<char, wchar_t> Char, typename Container = void>
	requires container_encoding_none_or<Container, no_encoding_t>
constexpr encoding_family get_encoding(no_encoding_t)
{
	return encoding_family::none;
}

template<typename Char, typename Container = void>
	requires container_encoding_none_or<Container, utf_encoding_t>
constexpr encoding_family get_encoding(utf_encoding_t)
{
	return encoding_family::utf;
}

template<typename Char, typename Container = void>
constexpr encoding_family get_encoding(default_encoding_t)
{
	return detail::get_encoding<Char>(container_encoding_or_t<Container, utf_encoding_t>());
}

template<typename Encoding, typename Char, typename... Context>
concept explicit_encoding_for = requires (Encoding const& encoding)
{
	detail::get_encoding<Char, Context...>(encoding);
};

template<typename Encoding, typename Container>
concept explicit_container_encoding_for = explicit_encoding_for<
	Encoding,
	typename Container::value_type,
	Container>;

template<typename Container, typename Encoding>
constexpr encoding_family get_container_encoding(Encoding const& encoding)
{
	return detail::get_encoding<typename Container::value_type, Container>(encoding);
}


template<vsm::character To, vsm::character From>
	requires (sizeof(To) == sizeof(From))
[[nodiscard]] std::basic_string_view<To> reinterpret(std::basic_string_view<From> const string)
{
	return std::basic_string_view<To>(reinterpret_cast<To const*>(string.data()), string.size());
}

template<vsm::character To, vsm::character From>
	requires (sizeof(To) == sizeof(From))
[[nodiscard]] std::span<To> reinterpret(std::span<From> const string)
{
	using to_type = vsm::copy_cv_t<From, To>;
	return std::span<to_type>(reinterpret_cast<to_type*>(string.data()), string.size());
}


template<size_t Size>
struct map_char_to;

template<>
struct map_char_to<1>
{
	template<typename T, typename, typename>
	using type = T;
};

template<>
struct map_char_to<2>
{
	template<typename, typename T, typename>
	using type = T;
};

template<>
struct map_char_to<4>
{
	template<typename, typename, typename T>
	using type = T;
};

template<typename Char, typename... Chars>
using map_char_to_t = typename map_char_to<sizeof(Char)>::template type<Chars...>;

template<typename Char>
using as_utf_char_t = map_char_to_t<Char, char8_t, char16_t, char32_t>;


template<vsm::character... Chars, typename String>
[[nodiscard]] auto reinterpret_as(String const& string)
{
	return reinterpret<map_char_to_t<typename String::value_type, Chars...>>(string);
}

template<typename String>
[[nodiscard]] auto reinterpret_as_utf(String const& string)
{
	return reinterpret<as_utf_char_t<typename String::value_type>>(string);
}


template<vsm::character... Chars, typename String, typename Visitor>
[[nodiscard]] decltype(auto) visit_as(String const& string, Visitor&& visitor)
{
	return string.visit([&]<typename VisitString>(VisitString const& string)
	{
		if constexpr (std::is_empty_v<VisitString>)
		{
			return vsm_forward(visitor)(string);
		}
		else
		{
			return vsm_forward(visitor)(detail::reinterpret_as<Chars...>(string));
		}
	});
}

template<typename String, typename Visitor>
[[nodiscard]] decltype(auto) visit_as_utf(String const& string, Visitor&& visitor)
{
	return string.visit([&]<typename VisitString>(VisitString const& string)
	{
		if constexpr (std::is_empty_v<VisitString>)
		{
			return vsm_forward(visitor)(string);
		}
		else
		{
			return vsm_forward(visitor)(detail::reinterpret_as_utf(string));
		}
	});
}

} // namespace detail
} // namespace allio
