#pragma once

#include <allio/serialization.hpp>

#include <vsm/platform.h>

namespace allio::nothrow {

template<handle Handle>
[[nodiscard]] vsm_always_inline vsm::result<size_t> encode_handle(
	Handle const& h,
	any_string_buffer const buffer)
{
	return detail::_encode_handle(h, buffer);
}

template<typename String, handle Handle>
[[nodiscard]] vsm_always_inline vsm::result<String> encode_handle(Handle const& h)
{
	return detail::_encode_handle<String>(h);
}

template<detached_handle Handle>
[[nodiscard]] vsm_always_inline vsm::result<Handle> _decode_handle(any_string_view const string)
{
	return detail::_decode_handle<Handle>(string);
}

} // namespace allio::nothrow
