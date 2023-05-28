#include <allio/impl/sync_byte_io.hpp>

#include <allio/impl/linux/error.hpp>
#include <allio/linux/platform.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

static vsm::result<size_t> sync_scatter_gather_io(
	platform_handle const& h, io::scatter_gather_parameters const& args,
	decltype(preadv) const syscall)
{
	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	auto const buffers = args.buffers.buffers();

	int const result = syscall(
		linux::unwrap_handle(h.get_platform_handle()),
		reinterpret_cast<iovec const*>(buffers.data()),
		static_cast<int>(buffers.size()),
		args.offset);

	if (result == -1)
	{
		return vsm::unexpected(linux::get_last_error());
	}

	return static_cast<size_t>(result);
}

vsm::result<size_t> allio::sync_scatter_read(platform_handle const& h, io::scatter_gather_parameters const& args)
{
	return sync_scatter_gather_io(h, args, preadv);
}

vsm::result<size_t> allio::sync_gather_write(platform_handle const& h, io::scatter_gather_parameters const& args)
{
	return sync_scatter_gather_io(h, args, pwritev);
}
