#include <allio/impl/handles/fs_object.hpp>

#include <allio/detail/handles/directory.hpp>
#include <allio/impl/hexadecimal.hpp>
#include <allio/impl/random.hpp>
#include <allio/step_deadline.hpp>

using namespace allio;
using namespace allio::detail;

namespace {

class unique_file_name
{
	using char_type = platform_path_view::value_type;

	static constexpr size_t data_bits = 128;
	static constexpr size_t data_size = data_bits / CHAR_BIT;
	static constexpr size_t name_size = hexadecimal_size(data_size);

	char_type m_name[name_size + 1];

public:
	unique_file_name() = default;

	unique_file_name(unique_file_name const&) = delete;
	unique_file_name& operator=(unique_file_name const&) = delete;

	[[nodiscard]] vsm::result<any_path_view> generate() &
	{
		std::byte data[data_size];
		vsm_try_void(secure_random_fill(data));

		vsm_verify(make_hexadecimal(data, std::span(m_name, name_size)) == name_size);
		m_name[name_size] = char_type('\0');

		//TODO: Set null terminated flag.
		return any_path_view(platform_path_view(m_name, name_size));
	}
};

} // namespace

vsm::result<handle_with_flags> detail::open_unique_file(
	HANDLE const base,
	platform_open_options const& options)
{
	//TODO: Deadline
	deadline const relative_deadline = deadline::never();
	step_deadline absolute_deadline(relative_deadline);

	unique_file_name name;

	while (true)
	{
		vsm_try_discard(absolute_deadline.step());
		vsm_try(random_file_name, name.generate());

		if (auto r = detail::open_file(base, random_file_name, options))
		{
			return r;
		}
	}
}

static vsm::result<handle_with_flags> _open(
	fs_path const& user_path,
	generic_open_options const& options)
{
	platform_handle_type base = user_path.base == nullptr
		? null_platform_handle
		: unwrap_handle(user_path.base->platform_handle);

	any_path_view path = user_path.path;
	unique_handle new_base_handle;

	if (vsm::any_flags(options.special, open_options::unique_name | open_options::anonymous))
	{
		// TODO: Check if path is lexically equivalent to empty?

		if (!path.empty())
		{
			vsm_try_assign(new_base_handle, open_path_base(base, path));

			base = new_base_handle.get();
			path = {};
		}
	}

	vsm_try(platform_options, platform_open_options::make(options));

	/**/ if (vsm::any_flags(options.special, open_options::unique_name))
	{
		return detail::open_unique_file(base, platform_options);
	}
	else if (vsm::any_flags(options.special, open_options::anonymous))
	{
		return detail::open_anonymous_file(base, platform_options);
	}
	else
	{
		return detail::open_file(base, path, platform_options);
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

static file_sharing default_file_sharing(open_options const options)
{
	if (vsm::any_flags(options, open_options::anonymous))
	{
		return file_sharing::none;
	}
	else
	{
		return file_sharing::all;
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

	auto const options = make_open_options(kind, args);
	vsm_try_bind((handle, flags), _open(args.path, options));

	if (vsm::any_flags(options.mode, file_mode::read_data))
	{
		flags |= fs_object_t::flags::readable;
	}
	if (vsm::any_flags(options.mode, file_mode::write_data))
	{
		flags |= fs_object_t::flags::writable;
	}

	h.flags = object_t::flags::not_null | flags;
	h.platform_handle = wrap_handle(handle.release());

	return {};
}
