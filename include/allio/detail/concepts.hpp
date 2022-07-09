#pragma once

#include <type_traits>

namespace allio::detail {

template<typename T, typename Base>
concept pointer_interconvertibly_derived_from =
#if 0 && defined(__cpp_lib_is_pointer_interconvertible)
	std::is_pointer_interconvertible_base_of_v<Base, T>
#else
	std::is_base_of_v<Base, T>
#endif
;

} // namespace allio::detail
