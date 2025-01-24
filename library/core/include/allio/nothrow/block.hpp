#pragma once

#include <allio/detail/handle.hpp>

namespace allio::nothrow {

template<detail::observer Operation, detail::handle Handle>
[[nodiscard]] vsm::result<detail::io_result_t<Handle, Operation>> block(Handle& handle, auto&&... args)
{
	return detail::blocking_io<Operation>(
		handle,
		detail::make_args<detail::io_parameters_t<typename Handle::object_type, Operation>>(vsm_forward(args)...));
}

} // namespace allio::nothrow
