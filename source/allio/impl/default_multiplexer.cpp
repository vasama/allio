#include <allio/default_multiplexer.hpp>

using namespace allio;

result<unique_multiplexer_ptr> allio::create_default_multiplexer()
{
	return create_default_multiplexer({});
}
