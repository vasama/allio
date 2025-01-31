#include <allio/detail/handles/pipe.hpp>

#include <allio/impl/error_encoding.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<void> pipe_pair_t::close(
	native_handle<pipe_pair_t>& h,
	io_parameters_t<pipe_pair_t, close_t> const& a)
{
	if (!h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(allio_error(error::handle_is_null));
	}

	unrecoverable(blocking_io<close_t>(h.r_h, a));
	unrecoverable(blocking_io<close_t>(h.w_h, a));

	h.flags = {};

	return {};
}
