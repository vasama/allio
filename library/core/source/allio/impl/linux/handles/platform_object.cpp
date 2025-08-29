#include <allio/impl/linux/handles/platform_object.hpp>

#include <allio/detail/serialization.hpp>
#include <allio/detail/unique_handle.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/proc.hpp>
#include <allio/impl/linux/readlink.hpp>
#include <allio/impl/linux/stat.hpp>

#include <unistd.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static bool consume_front(std::string_view& string, std::string_view const pattern)
{
	bool const r = string.substr(0, pattern.size()) == pattern;

	if (r)
	{
		string.remove_prefix(pattern.size());
	}

	return r;
}

static vsm::result<bool> _verify_anon_inode_type(int const fd, std::string_view const type)
{
	auto const path = make_proc_path("/proc/self/fd/%d", fd);

	char link_path_data[64];
	vsm_try(link_path_size, read_link_path(
		/* dirfd: */ -1,
		path.data(),
		string_buffer(link_path_data)));

	std::string_view link_path(link_path_data, link_path_size);

	return
		consume_front(link_path, "anon_inode:[") &&
		consume_front(link_path, type) &&
		consume_front(link_path, "]") &&
		link_path.empty();
}

bool linux::verify_anon_inode_type(int const fd, std::string_view const type)
{
	return _verify_anon_inode_type(fd, type).value_or(false);
}

static vsm::result<bool> _verify_file_stat_mode(int const fd, mode_t const mode)
{
	vsm_try(stat, fstat(fd));
	return (stat.st_mode & S_IFMT) == mode;
}

bool linux::verify_file_stat_mode(int const fd, mode_t const mode)
{
	return _verify_file_stat_mode(fd, mode).value_or(false);
}


vsm::result<void> platform_object_t::close(
	native_handle<platform_object_t>& h,
	io_parameters_t<platform_object_t, close_t> const&)
{
	if (h.platform_handle != native_platform_handle::null)
	{
		close_platform_handle(unwrap_handle(h.platform_handle));
	}
	h = {};
	return {};
}

vsm::result<void> platform_object_t::serializer_visit(
	native_handle<platform_object_t>& h,
	serialization_context& serializer)
{
	vsm_try_void(base_type::serializer_visit(h, serializer));
	vsm_try_void(serializer.visit(h.platform_handle));
	return {};
}
