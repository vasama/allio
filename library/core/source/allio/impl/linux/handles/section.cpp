#include <allio/detail/handles/section.hpp>

#include <allio/impl/error_encoding.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

static protection get_file_protection(handle_flags const flags)
{
	protection protection = {};
	if (flags[fs_object_t::flags::readable])
	{
		protection |= protection::read;
	}
	if (flags[fs_object_t::flags::writable])
	{
		protection |= protection::write;
	}
	return protection;
}

static int get_file_protection_flags(protection const protection)
{
	int open_flags = 0;

	vsm_gnu_diagnostic(push)
	vsm_gnu_diagnostic(ignored "-Wswitch")
	switch (protection & protection::read_write)
	{
	case protection::read:
		open_flags |= O_RDONLY;
		break;

	case protection::write:
		open_flags |= O_WRONLY;
		break;

	case protection::read_write:
		open_flags |= O_RDWR;
		break;
	}
	vsm_gnu_diagnostic(pop)

	return open_flags;
}

static vsm::result<native_handle<fs_object_t> const*> get_default_backing_directory()
{
	//TODO: Implement default backing directory.
	return vsm::unexpected(allio_error(error::unsupported_operation));
}

static vsm::result<void> _create_with_backing_file(
	native_handle<section_t>& h,
	io_parameters_t<section_t, section_io::create_t> const& a)
{
	native_handle<fs_object_t> const& backing_h = *a.backing_storage;

	auto const default_protection = get_file_protection(backing_h.flags);

	auto maximum_protection = default_protection;

	// Any file that can be read can also be executed, but execute permissions are not enabled by
	// default.
	if (vsm::all_flags(default_protection, protection::read))
	{
		maximum_protection |= protection::execute;
	}

	auto const protection = a.protection != detail::protection(0)
		? a.protection
		: default_protection;

	if (!vsm::all_flags(maximum_protection, protection))
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	int open_flags = get_file_protection_flags(protection);

	if (vsm::no_flags(a.flags, io_flags::create_inheritable))
	{
		open_flags |= O_CLOEXEC;
	}

	vsm_try(fd, linux::reopen_file(
		unwrap_handle(backing_h.platform_handle),
		open_flags));

	h = native_handle<section_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		protection,
	};

	return {};
}

static vsm::result<void> _create_with_backing_directory(
	native_handle<section_t>& h,
	io_parameters_t<section_t, section_io::create_t> const& a,
	native_handle<fs_object_t> const& backing_h)
{
	auto const protection = a.protection != detail::protection(0)
		? a.protection
		: protection::read_write;

	if (!vsm::all_flags(protection, protection::write))
	{
		// O_TMPFILE must be specified in combination with either O_RDWR or O_WRONLY.
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	int open_flags = O_TMPFILE | O_RDWR;

	if (vsm::no_flags(a.flags, io_flags::create_inheritable))
	{
		open_flags |= O_CLOEXEC;
	}

	vsm_try(fd, linux::open_file(
		unwrap_handle(backing_h.platform_handle),
		"",
		open_flags));

	h = native_handle<section_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		protection,
	};

	return {};
}

#if 0
static vsm::result<void> _create_anonymous(
	native_handle<section_t>& h,
	io_parameters_t<section_t, section_io::create_t> const& a)
{
	if (a.backing_directory == nullptr)
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	auto const& backing_directory_h = *a.backing_directory;
	if (!backing_directory_h.flags[object_t::flags::not_null])
	{
		return vsm::unexpected(allio_error(error::invalid_argument));
	}

	auto const protection = a.protection.value_or(protection::read_write);

	if (!vsm::all_flags(protection, protection::write))
	{
		//TODO: Is there any point in creating a read only anonymous section?
		return vsm::unexpected(allio_error(error::invalid_argument));
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

	h = native_handle<section_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				object_t::flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		protection,
	};

	return {};
}
#endif

vsm::result<void> section_t::create(
	native_handle<section_t>& h,
	io_parameters_t<section_t, create_t> const& a)
{
	static constexpr auto storage_options =
		section_options::backing_file |
		section_options::backing_directory;

	if (vsm::any_flags(a.options, storage_options))
	{
		if (vsm::all_flags(a.options, storage_options))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (a.backing_storage == nullptr)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (!a.backing_storage->flags[object_t::flags::not_null])
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}

		if (vsm::any_flags(a.options, section_options::backing_file))
		{
			return _create_with_backing_file(h, a);
		}
		else
		{
			return _create_with_backing_directory(h, a, *a.backing_storage);
		}
	}

	vsm_try_ptr(backing_h, get_default_backing_directory());
	return _create_with_backing_directory(h, a, backing_h);
}
