#include <allio/detail/handles/section.hpp>

#include <allio/impl/linux/handles/fs_object.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static protection get_file_protection(handle_flags const flags)
{
	allio::protection protection = {};
	if (flags[fs_object_t::flags::readable])
	{
		protection |= allio::protection::read;
	}
	if (flags[fs_object_t::flags::writable])
	{
		protection |= allio::protection::write;
	}
	return protection;
}

static vsm::result<void> _create_backed(
	section_t::native_type& h,
	io_parameters_t<section_t, section_io::create_t> const& a)
{
	if (a.backing_directory != nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const& backing_file_h = *a.backing_file;
	if (!backing_file_h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const default_protection = get_file_protection(backing_file_h.flags);

	auto maximum_protection = default_protection;
	if (vsm::any_flags(default_protection, protection::read))
	{
		maximum_protection |= protection::execute;
	}

	auto const protection = a.protection.value_or(default_protection);

	if (!vsm::all_flags(maximum_protection, protection))
	{
		return vsm::unexpected(error::invalid_argument);
	}

	int open_flags = 0;

	switch (protection & allio::protection::read_write)
	{
	case allio::protection::read:
		open_flags |= O_RDONLY;
		break;

	case allio::protection::write:
		open_flags |= O_WRONLY;
		break;

	case allio::protection::read_write:
		open_flags |= O_RDWR;
		break;
	}

	if (!a.inheritable)
	{
		open_flags |= O_CLOEXEC;
	}

	vsm_try(fd, linux::reopen_file(
		unwrap_handle(backing_file_h.platform_handle),
		open_flags));

	h = section_t::native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				object_t::flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		protection,
	};

	return {};
}

static vsm::result<void> _create_anonymous(
	section_t::native_type& h,
	io_parameters_t<section_t, section_io::create_t> const& a)
{
	if (a.backing_directory == nullptr)
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const& backing_directory_h = *a.backing_directory;
	if (!backing_directory_h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(error::invalid_argument);
	}

	auto const protection = a.protection.value_or(protection::read_write);

	if (vsm::no_flags(protection, protection::write))
	{
		//TODO: Is there any point in creating a read only anonymous section?
		return vsm::unexpected(error::invalid_argument);
	}

	int open_flags = O_TMPFILE | O_RDWR;

	if (!a.inheritable)
	{
		open_flags |= O_CLOEXEC;
	}

	vsm_try(fd, linux::open_file(
		unwrap_handle(backing_directory_h.platform_handle),
		"",
		open_flags));

	h = section_t::native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				object_t::flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		protection,
	};

	return {};
}

vsm::result<void> section_t::create(
	native_type& h,
	io_parameters_t<section_t, create_t> const& a)
{
	if (a.backing_file != nullptr)
	{
		return _create_backed(h, a);
	}
	else
	{
		return _create_anonymous(h, a);
	}
}
