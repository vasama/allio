#pragma once

#include <allio/any_string_buffer.hpp>
#include <allio/detail/path.hpp>

namespace allio {
namespace detail {

template<typename Path>
concept any_mutable_path = _any_mutable_string<path_string_t<Path>>;

} // namespace detail

class any_path_buffer
{
	any_string_buffer m_string_buffer;

public:
	any_path_buffer() = default;

	template<typename... Args>
		requires std::constructible_from<any_string_buffer, Args...>
	explicit any_path_buffer(Args&&... args)
		: m_string_buffer(vsm_forward(args)...)
	{
	}

	template<detail::any_mutable_path Path, typename Encoding = detail::default_encoding_t>
		requires detail::explicit_container_encoding_for<Encoding, path_string_t<Path>>
	any_path_buffer(Path& path, Encoding const encoding = Encoding())
		: m_string_buffer(get_path_string(path), encoding)
	{
	}


	[[nodiscard]] any_string_buffer string() const
	{
		return m_string_buffer;
	}
};

} // namespace allio
