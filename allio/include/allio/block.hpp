#pragma once

#include <allio/detail/exceptions.hpp>
#include <allio/detail/handle.hpp>

namespace allio {

template<detail::observer Operation, detail::handle Handle>
detail::io_result_t<typename Handle::object_type, Operation> block(Handle& handle, auto&&... args)
{
	return detail::throw_on_error(detail::blocking_io<Operation>(
		handle,
		detail::make_args<detail::io_parameters_t<typename Handle::object_type, Operation>>(vsm_forward(args)...)));
}

template<detail::observer Operation, detail::handle Handle>
vsm::result<detail::io_result_t<typename Handle::object_type, Operation>> try_block(Handle& handle, auto&&... args)
{
	return detail::blocking_io<Operation>(
		handle,
		detail::make_args<detail::io_parameters_t<typename Handle::object_type, Operation>>(vsm_forward(args)...));
}

} // namespace allio
