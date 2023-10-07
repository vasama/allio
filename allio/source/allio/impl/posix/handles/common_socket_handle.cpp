#include <allio/detail/handles/common_socket_handle.hpp>

#include <allio/impl/posix/socket.hpp>

using namespace allio;
using namespace allio::detail;
using namespace allio::posix;

void common_socket_handle::close()
{
	auto const native_handle = get_platform_handle();
	vsm_assert(native_handle != native_platform_handle::null);
	vsm_verify(posix::close_socket(unwrap_socket(native_handle)));
}

vsm::result<void> common_socket_handle::do_blocking_io(
	common_socket_handle& h,
	io_result_ref_t<create_t>,
	io_parameters_t<create_t> const& args)
{
	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(address_family, get_address_family(args.address_kind));
	vsm_try(socket, create_socket(address_family));

	platform_handle::native_handle_type const native =
	{
		{
			flags::not_null | socket.flags,
		},
		wrap_socket(socket.socket.get())
	};

	vsm_assert(check_native_handle(native));
	h.set_native_handle(native);

	(void)socket.socket.release();

	return {};
}
