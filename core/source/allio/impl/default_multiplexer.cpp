#include <allio/default_multiplexer.hpp>

using namespace allio;

vsm::result<unique_multiplexer_ptr> allio::create_default_multiplexer()
{
	return create_default_multiplexer({});
}
