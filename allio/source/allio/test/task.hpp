#pragma once

#include <allio/async.hpp>

#if allio_config_unifex
#	include <unifex/task.hpp>
#else
#	include <exec/task.hpp>
#endif
