#pragma once

#define allio_config_unifex 1
#if allio_config_unifex
#	include <unifex/let_value.hpp>
#	include <unifex/receiver_concepts.hpp>
#	include <unifex/scheduler_concepts.hpp>
#	include <unifex/sender_concepts.hpp>
#	include <unifex/when_all.hpp>

#	define allio_detail_set_stopped set_done
#else
#	include <stdexec/execution.hpp>

#	define allio_detail_set_stopped set_stopped
#endif

namespace allio::detail::execution {

#if allio_config_unifex
using namespace unifex;
#else
using namespace stdexec;
#endif

} // namespace allio::detail::execution

