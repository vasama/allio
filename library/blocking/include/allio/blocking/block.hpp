#pragma once

#include <allio/detail/block.hpp>
#include <allio/detail/exceptions.hpp>

namespace allio::blocking {

template<detail::observer Operation, detail::handle Handle>
[[nodiscard]] detail::io_result_t<Handle, Operation> block(Handle& handle, auto&&... args)
{
	return detail::throw_on_error(detail::_block<Operation>(handle, vsm_forward(args)...));
}

} // namespace allio::blocking
