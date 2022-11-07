#pragma once

#include <allio/detail/api.hpp>
#include <allio/multiplexer.hpp>

#include <memory>

namespace allio {

using unique_multiplexer_ptr = std::unique_ptr<multiplexer>;

struct default_multiplexer_options
{
};

allio_API result<unique_multiplexer_ptr> create_default_multiplexer();
allio_API result<unique_multiplexer_ptr> create_default_multiplexer(default_multiplexer_options const& options);

} // namespace allio
