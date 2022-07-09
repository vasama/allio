#include <allio/file_handle.hpp>

#include "api_string.hpp"
#include "error.hpp"
#include "filesystem_handle.hpp"

#include <fcntl.h>
#include <sys/uio.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::linux;

result<void> detail::file_handle_base::open_sync(filesystem_handle const* const base, path_view const path, file_parameters const& args)
{
	allio_ASSERT(!*this);
	allio_TRY(file, create_file(base, path, args));
	return consume_platform_handle(*this, { args.handle_flags }, std::move(file));
}

result<size_t> detail::file_handle_base::read_at_sync(file_offset const offset, read_buffers const buffers)
{
	allio_ASSERT(*this);

	int const result = preadv(
		unwrap_handle(get_platform_handle()),
		reinterpret_cast<iovec const*>(buffers.data()),
		static_cast<int>(buffers.size()),
		offset);

	if (result == -1)
	{
		return allio_ERROR(get_last_error_code());
	}

	return static_cast<size_t>(result);
}

result<size_t> detail::file_handle_base::write_at_sync(file_offset const offset, write_buffers const buffers)
{
	allio_ASSERT(*this);

	int const result = pwritev(
		unwrap_handle(get_platform_handle()),
		reinterpret_cast<iovec const*>(buffers.data()),
		static_cast<int>(buffers.size()),
		offset);

	if (result == -1)
	{
		return allio_ERROR(get_last_error_code());
	}

	return static_cast<size_t>(result);
}
