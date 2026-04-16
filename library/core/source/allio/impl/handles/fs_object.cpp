#include <allio/impl/handles/fs_object.hpp>

#include <allio/detail/handles/directory.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/filesystem.hpp>

using namespace allio;
using namespace allio::detail;

static vsm::result<unique_handle> _open(
	fs_path const& user_path,
	open_options const special,
	platform_open_options const& options)
{
	platform_handle_type base = user_path.base == nullptr
		? null_platform_handle
		: unwrap_handle(user_path.base->platform_handle);

	any_path_view path = user_path.path;
	unique_handle new_base_handle;

	// TODO: Pass the path to open_anonymous_file and let platform-specific code decide whether the
	//       child directory (if any) should be opened separately - this is only needed on Windows.
	if (vsm::any_flags(special, open_options::unique_name | open_options::anonymous))
	{
		// TODO: Check if path is lexically equivalent to empty?

		if (!path.empty())
		{
			vsm_try_assign(new_base_handle, open_anchor_file(
				base,
				path,
				/* may_open_leaf: */ false));

			base = new_base_handle.get();
			path = {};
		}
	}

	/**/ if (vsm::any_flags(special, open_options::unique_name))
	{
		return detail::open_unique_file(base, options);
	}
	else if (vsm::any_flags(special, open_options::anonymous))
	{
		return detail::open_anonymous_file(base, options);
	}
	else
	{
		return detail::open_file(base, path, options);
	}
}

static file_mode default_file_mode(open_kind const kind)
{
	switch (kind)
	{
	case open_kind::path:
		return file_mode::none;

	case open_kind::file:
		return file_mode::read_write;

	case open_kind::directory:
		return file_mode::read;
	}

	vsm_unreachable();
}

static file_opening default_file_opening(open_options const options, file_mode const mode)
{
	if (vsm::any_flags(options, open_options::unique_name | open_options::anonymous))
	{
		return file_opening::create_only;
	}

	if (vsm::any_flags(mode, file_mode::write_data))
	{
		return file_opening::open_or_create;
	}
	else
	{
		return file_opening::open_existing;
	}
}

static generic_open_options make_open_options(open_kind const kind, fs_open_params_type const& args)
{
	file_mode const mode = args.mode
		? *args.mode
		: default_file_mode(kind);

	file_opening const opening = args.opening != file_opening(0)
		? args.opening
		: default_file_opening(args.special, mode);

	file_sharing const sharing = args.sharing
		? *args.sharing
		: default_file_sharing(args.special);

	return
	{
		.kind = kind,
		.mode = mode,
		.opening = opening,
		.sharing = sharing,
		.caching = args.caching,
		.special = args.special,
		.flags = args.flags,
	};
}

vsm::result<void> detail::open_fs_object(
	native_handle<fs_object_t>& h,
	open_kind const kind,
	fs_open_params_type const& args)
{
	if (vsm::any_flags(args.special, open_options::anonymous))
	{
		if (kind != open_kind::file)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
	}

	if (vsm::any_flags(args.special, open_options::unique_name | open_options::anonymous))
	{
		if (args.opening != file_opening(0) && args.opening != file_opening::create_only)
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
	}

	auto const generic_options = make_open_options(kind, args);
	vsm_try(platform_options, make_platform_open_options(generic_options));
	vsm_try(handle, _open(args.path, generic_options.special, platform_options));

	vsm_try(flags, configure_file(handle.get(), platform_options));

	if (vsm::any_flags(generic_options.mode, file_mode::read_data))
	{
		flags |= fs_object_t::flags::readable;
	}
	if (vsm::any_flags(generic_options.mode, file_mode::write_data))
	{
		flags |= fs_object_t::flags::writable;
	}

	h.flags = object_t::flags::not_null | flags;
	h.platform_handle = wrap_handle(handle.release());

	return {};
}

vsm::result<void> fs_object_t::link_at(
	native_handle<fs_object_t> const& h,
	io_parameters_t<fs_object_t, link_at_t> const& a)
{
	platform_handle_type const base_handle = a.path.base == nullptr
		? null_platform_handle
		: unwrap_handle(a.path.base->platform_handle);

	return link_file_at(
		unwrap_handle(h.platform_handle),
		base_handle,
		a.path.path,
		a.replace_existing_file);
}
