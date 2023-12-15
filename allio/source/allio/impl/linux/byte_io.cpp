#include <allio/impl/linux/byte_io.hpp>

#include <allio/impl/linux/error.hpp>

#include <vsm/numeric.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

template<auto Syscall>
static vsm::result<size_t> do_byte_io(
	platform_object_t::native_type const& h,
	auto const& a)
{
	auto const buffers = a.buffers.buffers();

	ssize_t result;
	if constexpr (requires { a.offset; })
	{
		result = Syscall(
			unwrap_handle(h.platform_handle),
			reinterpret_cast<iovec const*>(buffers.data()),
			vsm::saturating(buffers.size()),
			a.offset);
	}
	else
	{
		result = Syscall(
			unwrap_handle(h.platform_handle),
			reinterpret_cast<iovec const*>(buffers.data()),
			vsm::saturating(buffers.size()));
	}

	if (result == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return static_cast<size_t>(result);
}

vsm::result<size_t> linux::random_read(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<preadv>(h, a);
}

vsm::result<size_t> linux::random_write(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<pwritev>(h, a);
}

vsm::result<size_t> linux::stream_read(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<readv>(h, a);
}

vsm::result<size_t> linux::stream_write(
	detail::platform_object_t::native_type const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<writev>(h, a);
}
