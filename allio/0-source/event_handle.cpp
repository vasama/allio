#include <allio/event_handle.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> event_handle_base::block_create(reset_mode const reset_mode, create_parameters const& args)
{
	return block<io::event_create>(static_cast<event_handle&>(*this), args, reset_mode);
}

vsm::result<void> event_handle_base::block_wait(wait_parameters const& args) const
{
	return block<io::event_wait>(static_cast<event_handle const&>(*this), args);
}


vsm::result<event_handle> detail::block_create_event(
	event_handle::reset_mode const reset_mode,
	event_handle::create_parameters const& args)
{
	vsm::result<event_handle> r(vsm::result_value);
	vsm_try_void(r->create(reset_mode, args));
	return r;
}
