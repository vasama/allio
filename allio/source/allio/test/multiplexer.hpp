#pragma once

#include <allio/default_multiplexer.hpp>

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

namespace allio {

inline void maybe_set_multiplexer(unique_multiplexer_ptr const& multiplexer, auto& handle)
{
	if (GENERATE(0, 1))
	{
		CAPTURE("Bound multiplexer");
		handle.set_multiplexer(multiplexer.get()).value();
	}
}

inline unique_multiplexer_ptr generate_multiplexer(bool const required = false)
{
	if (required || GENERATE(0, 1))
	{
		return create_default_multiplexer().value();
	}
	return nullptr;
}

} // namespace allio
