#pragma once

#include <allio/detail/handle.hpp>

namespace allio::detail {

template<detail::observer Operation, detail::handle Handle>
[[nodiscard]] vsm::result<io_result_t<Handle, Operation>> _block(Handle& handle, auto&&... args)
{
	using object_type = typename Handle::object_type;

	return detail::blocking_io<Operation>(
		handle,
		detail::make_args<io_parameters_t<object_type, Operation>>(vsm_forward(args)...));
}

} // namespace allio::detail
