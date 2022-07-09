#pragma once

#include <allio/handles/directory.hpp>
#include <allio/senders.hpp>

namespace allio::senders {
inline namespace directories {

template<detail::multiplexer_handle_for<directory_t> MultiplexerHandle>
using basic_directory_handle = traits_type::handle<directory_t, MultiplexerHandle>;

[[nodiscard]] detail::ex::sender auto open_directory(detail::fs_path const& path, auto&&... args)
{
	return detail::open_directory<traits_type>(path, vsm_forward(args)...);
}

[[nodiscard]] detail::ex::sender auto open_temp_directory(auto&&... args)
{
	return detail::open_temp_directory<traits_type>(vsm_forward(args)...);
}

[[nodiscard]] detail::ex::sender auto open_unique_directory(auto&&... args)
{
	return detail::open_unique_directory<traits_type>(vsm_forward(args)...);
}

} // inline namespace directories
} // namespace allio::senders
