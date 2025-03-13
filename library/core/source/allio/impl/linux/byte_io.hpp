#pragma once

#include <allio/byte_io.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/impl/byte_io_buffers.hpp>
#include <allio/impl/storage_provider.hpp>

#include <limits.h>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

namespace allio::linux {

static_assert(sizeof(detail::new_io_buffer) == sizeof(iovec));
static_assert(alignof(detail::new_io_buffer) == alignof(iovec));

static_assert(sizeof(detail::new_io_buffer::m0) == sizeof(iovec::iov_base));
static_assert(sizeof(detail::new_io_buffer::m1) == sizeof(iovec::iov_len));

static_assert(offsetof(detail::new_io_buffer, m0) == offsetof(iovec, iov_base));
static_assert(offsetof(detail::new_io_buffer, m1) == offsetof(iovec, iov_len));

//TODO: Detect the iovec layout automatically.
static constexpr auto io_vector_layout = detail::new_io_buffer_layout::data_size;

inline vsm::result<void> check_io_vectors_size(detail::new_io_buffers_base const& buffers)
{
	if (buffers.was_truncated() || buffers.get_buffers_size() > IOV_MAX)
	{
		//TODO: Return a more specific error code.
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	return {};
}

template<vsm::any_cv_of<std::byte> T>
[[nodiscard]] vsm::result<std::span<iovec const>> get_io_vectors(
	detail::new_io_buffers<T> const& buffers,
	storage_provider_ref const storage_provider)
{
	vsm_try(io_vectors, get_io_buffers(buffers, io_vector_layout, storage_provider));

	return std::span(
		reinterpret_cast<iovec const*>(io_vectors.buffers_data),
		io_vectors.buffers_size);
}

using dynamic_io_vector_storage = dynamic_storage_provider<16 * sizeof(iovec)>;


vsm::result<size_t> random_read(
	detail::native_handle<detail::platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte> const& a);

vsm::result<size_t> random_write(
	detail::native_handle<detail::platform_object_t> const& h,
	detail::byte_io::random_parameters_t<std::byte const> const& a);

vsm::result<size_t> stream_read(
	detail::native_handle<detail::platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte> const& a);

vsm::result<size_t> stream_write(
	detail::native_handle<detail::platform_object_t> const& h,
	detail::byte_io::stream_parameters_t<std::byte const> const& a);

} // namespace allio::linux

#include <allio/linux/detail/undef.i>
