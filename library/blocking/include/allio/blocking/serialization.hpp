#pragma once

#include <allio/blocking/traits.hpp>
#include <allio/detail/handle.hpp>
#include <allio/serialization.hpp>

namespace allio::blocking {

template<detail::handle Handle>
[[nodiscard]] size_t encode_handle(Handle const& h, any_string_buffer const buffer)
{
	return detail::throw_on_error(detail::_encode_handle(h, buffer));
}

template<typename String, detail::handle Handle>
[[nodiscard]] String encode_handle(Handle const& h)
{
	return detail::throw_on_error(detail::_encode_handle<String>(h));
}

template<detail::detached_handle Handle>
[[nodiscard]] Handle decode_handle(any_string_view const string)
{
	return detail::throw_on_error(detail::_decode_handle<Handle>(string));
}

} // namespace allio::blocking
