#pragma once

#include <allio/detail/concepts.hpp>

#include <vsm/result.hpp>
#include <vsm/utility.hpp>

namespace allio::detail {

[[noreturn]] void throw_error(std::error_code error);

template<any_cvref_of_template<vsm::expected> R>
	requires std::is_same_v<typename std::remove_cvref_t<R>::error_type, std::error_code>
vsm::copy_cvref_t<R, typename std::remove_cvref_t<R>::value_type> throw_on_error(R&& r)
{
	if (r)
	{
		return *vsm_forward(r);
	}
	else
	{
		throw_error(r.error());
	}
}

} // namespace allio::detail