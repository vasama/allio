#pragma once

#include <allio/byte_io.hpp>
#include <allio/platform_handle.hpp>
#include <allio/linux/platform.hpp>

#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

namespace allio {

template<std::derived_from<platform_handle> Handle>
struct synchronous_operation_implementation<Handle, io::scatter_read_at>
{
	result<void> execute(io::parameters_with_result<io::scatter_read_at> const& args)
	{
		file_handle const& h = static_cast<file_handle const&>(*args.handle);

		if (!h)
		{
			return allio_ERROR(error::handle_is_null);
		}

		int const result = preadv(
			unwrap_handle(h.get_platform_handle()),
			reinterpret_cast<iovec const*>(args.buffers.data()),
			static_cast<int>(args.buffers.size()),
			args.offset);

		if (result == -1)
		{
			return allio_ERROR(get_last_error());
		}

		*args.result = static_cast<size_t>(result);
		return {};
	}
};

template<std::derived_from<platform_handle> Handle>
struct synchronous_operation_implementation<Handle, io::gather_write_at>
{
	result<void> execute(io::parameters_with_result<io::gather_write_at> const& args)
	{
		file_handle const& h = static_cast<file_handle const&>(*args.handle);

		if (!h)
		{
			return allio_ERROR(error::handle_is_null);
		}

		int const result = pwritev(
			unwrap_handle(h.get_platform_handle()),
			reinterpret_cast<iovec const*>(args.buffers.data()),
			static_cast<int>(args.buffers.size()),
			args.offset);

		if (result == -1)
		{
			return allio_ERROR(get_last_error());
		}

		*args.result = static_cast<size_t>(result);
		return {};
	}
};

} // namespace allio

#include <allio/linux/detail/undef.i>
