#pragma once

#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

#if vsm_os_linux
struct platform_handle::impl_type : base_type::impl_type
{
};
#endif

} // namespace allio::detail
