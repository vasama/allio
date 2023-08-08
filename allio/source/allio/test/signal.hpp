#pragma once

#include <vsm/function_view.hpp>

namespace allio {

enum class signal
{
	access_violation,
};

signal capture_signal(signal signal, vsm::function_view<void()> callback);

} // namespace allio
