#include <allio/impl/linux/byte_io.hpp>

#include <allio/impl/linux/error.hpp>

#include <vsm/numeric.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

//TODO: Detect the iovec layout automatically.
static constexpr auto layout = new_io_buffer_layout::data_size;

template<auto Syscall>
static vsm::result<size_t> do_byte_io_2(
	native_handle<platform_object_t> const& h,
	auto const& a,
	std::same_as<off_t> auto const... offset)
{
	new_io_buffers_storage storage;
	vsm_try(buffers, get_io_buffers(storage, a.buffers, layout));

	ssize_t const r = Syscall(
		unwrap_handle(h.platform_handle),
		reinterpret_cast<iovec const*>(buffers.buffers_data),
		vsm::saturating(buffers.buffers_size),
		offset...);

	if (r == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	if (r == 0 && !io_buffers_is_empty(a.buffers))
	{
		return vsm::unexpected(error::end_of_stream);
	}

	return static_cast<size_t>(r);
}

template<auto Syscall>
static vsm::result<size_t> do_byte_io(native_handle<platform_object_t> const& h, auto const& a)
{
	if constexpr (requires { a.offset; })
	{
		vsm_try(offset, vsm::try_truncate<off_t>(a.offset, error::invalid_argument));
		return do_byte_io_2<Syscall>(h, a, offset);
	}
	else
	{
		return do_byte_io_2<Syscall>(h, a);
	}
}

vsm::result<size_t> linux::random_read(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte> const& a)
{
	return do_byte_io<preadv>(h, a);
}

vsm::result<size_t> linux::random_write(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte const> const& a)
{
	return do_byte_io<pwritev>(h, a);
}

vsm::result<size_t> linux::stream_read(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte> const& a)
{
	return do_byte_io<readv>(h, a);
}

vsm::result<size_t> linux::stream_write(
	detail::native_handle<platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a)
{
	return do_byte_io<writev>(h, a);
}
