#pragma once

#include <allio/detail/block.hpp>

namespace allio::nothrow {

template<detail::observer Operation, detail::handle Handle>
[[nodiscard]] vsm_always_inline vsm::result<detail::io_result_t<Handle, Operation>> block(
	Handle& handle,
	auto&&... args)
{
	return detail::_block<Operation>(handle, vsm_forward(args)...);
}

} // namespace allio::nothrow
