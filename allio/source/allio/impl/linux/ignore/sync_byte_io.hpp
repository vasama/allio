#pragma once

#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>
#include <allio/linux/platform.hpp>

#include <allio/impl/linux/error.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct synchronous_operation_implementation<Handle, io::scatter_read_at>
{
	vsm::result<void> execute(io::parameters_with_result<io::scatter_read_at> const& args)
	{
		file_handle const& h = static_cast<file_handle const&>(*args.handle);

		if (!h)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		auto const buffers = args.buffers.buffers();

		int const result = preadv(
			linux::unwrap_handle(h.get_platform_handle()),
			reinterpret_cast<iovec const*>(buffers.data()),
			static_cast<int>(buffers.size()),
			args.offset);

		if (result == -1)
		{
			return vsm::unexpected(linux::get_last_error());
		}

		*args.result = static_cast<size_t>(result);
		return {};
	}
};

template<std::derived_from<platform_handle> Handle>
struct synchronous_operation_implementation<Handle, io::gather_write_at>
{
	vsm::result<void> execute(io::parameters_with_result<io::gather_write_at> const& args)
	{
		file_handle const& h = static_cast<file_handle const&>(*args.handle);

		if (!h)
		{
			return vsm::unexpected(error::handle_is_null);
		}

		auto const buffers = args.buffers.buffers();

		int const result = pwritev(
			linux::unwrap_handle(h.get_platform_handle()),
			reinterpret_cast<iovec const*>(buffers.data()),
			static_cast<int>(buffers.size()),
			args.offset);

		if (result == -1)
		{
			return vsm::unexpected(linux::get_last_error());
		}

		*args.result = static_cast<size_t>(result);
		return {};
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
