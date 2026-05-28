#include <allio/detail/handles/filesystem.hpp>

#include <allio/detail/handles/fs_object.hpp>
#include <allio/impl/filesystem.hpp>

using namespace allio;
using namespace allio::detail;

vsm::result<fs_info_view> filesystem_t::query_path_info(
	native_handle<filesystem_t> const& h,
	io_parameters_t<filesystem_t, query_path_info_t> const& a)
{
	alignas(fs_info_alignment) std::byte local_byte_buffer[fs_info_storage_size];

	auto const mask = a.mask.value_or(fs_info_mask::standard);

	auto const buffer = a.buffer.is_byte_buffer()
		? a.buffer.as_byte_buffer()
		: any_byte_buffer_ref(local_byte_buffer);

	platform_handle_type const base_handle = a.path.base == nullptr
		? null_platform_handle
		: unwrap_handle(a.path.base->platform_handle);

	vsm_try(info, detail::query_path_info(base_handle, a.path.path, mask, buffer));

	if (a.buffer.is_fs_info_ref())
	{
		a.buffer.as_fs_info_ref().assign(std::exchange(info, fs_info_view()));
	}

	return info;
}
