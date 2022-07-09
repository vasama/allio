#pragma once

#include <allio/detail/api.hpp>
#include <allio/multiplexer.hpp>

#include <memory>

namespace allio {

using unique_multiplexer = std::unique_ptr<multiplexer>;

struct default_multiplexer_options
{
	bool enable_concurrent_submission = false;
	bool enable_concurrent_completion = false;
};

allio_API result<unique_multiplexer> create_default_multiplexer();
allio_API result<unique_multiplexer> create_default_multiplexer(default_multiplexer_options const& options);

} // namespace allio
