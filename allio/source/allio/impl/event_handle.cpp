#include <allio/event_handle.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> event_handle_base::block_create(create_parameters const& args)
{
	return block<io::event_create>(static_cast<event_handle&>(*this), args);
}

vsm::result<void> event_handle_base::block_wait(wait_parameters const& args) const
{
	return block<io::event_wait>(static_cast<event_handle const&>(*this), args);
}
