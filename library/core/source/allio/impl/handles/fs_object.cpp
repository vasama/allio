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

vsm::result<handle_with_flags> detail::open_unique_file(open_parameters const& a)
{
	// Checked before calling this function.
	vsm_assert(vsm::any_flags(a.special, open_options::unique_name));
	vsm_assert(a.path.path.empty());
	vsm_assert(a.opening == file_opening(0));

	open_parameters local_a = a;
	local_a.special &= ~open_options::unique_name;
	local_a.opening = file_opening::create_only;

	//TODO: Deadline
	deadline const relative_deadline = deadline::never();
	step_deadline absolute_deadline(relative_deadline);

	unique_file_name name;

	while (true)
	{
		vsm_try_discard(absolute_deadline.step());
		vsm_try_assign(local_a.path.path, name.generate());

		if (auto r = detail::open_file(local_a))
		{
			return r;
		}
	}
}

static vsm::result<handle_with_flags> _open(open_parameters& a)
{
	basic_detached_handle<directory_t> directory;

	if (vsm::any_flags(a.special, open_options::unique_name | open_options::anonymous))
	{
		if (!a.path.path.empty())
		{
			open_parameters const directory_args =
			{
				.path = a.path,
			};

			vsm_try_void(blocking_io<fs_io::open_t>(directory, directory_args));

			a.path.base = &directory.native();
			a.path.path = {};
		}
	}

	if (vsm::any_flags(a.special, open_options::unique_name))
	{
		return open_unique_file(a);
	}

	if (vsm::any_flags(a.special, open_options::anonymous))
	{
		if (a.opening != file_opening(0))
		{
			return vsm::unexpected(allio_error(error::invalid_argument));
		}
	}
	else
	{
		if (a.opening == file_opening(0))
		{
			if (vsm::any_flags(a.mode.value_or_zero(), file_mode::write_data))
			{
				a.opening = file_opening::open_or_create;
			}
			else
			{
				a.opening = file_opening::open_existing;
			}
		}
	}

	return open_file(a);
}

vsm::result<void> detail::open_fs_object(
	native_handle<fs_object_t>& h,
	io_parameters_t<fs_object_t, fs_io::open_t> const& a_ref,
	open_options const kind)
{
	auto a = a_ref;

	vsm_assert(vsm::no_flags(a.special, open_kind::mask)); //PRECONDITION
	a.special |= kind;

	if (!a.mode)
	{
		vsm_msvc_warning(push)
		vsm_msvc_warning(disable: 4063) // Disable C4063: Case is not a valid value for switch of enum.
		vsm_msvc_warning(disable: 4062) // TODO: Move the open kinds into its own enum and re-enable this warning.

		vsm_gnu_diagnostic(push)
		vsm_gnu_diagnostic(ignored "-Wswitch")

		switch (kind)
		{
		case open_kind::path:
			a.mode = file_mode::none;
			break;

		case open_kind::file:
			a.mode = file_mode::read_write;
			break;

		case open_kind::directory:
			a.mode = file_mode::read;
			break;
		}
		vsm_msvc_warning(pop)
		vsm_gnu_diagnostic(pop)
	}

	if (!a.sharing)
	{
		a.sharing = file_sharing::all;
	}

	if (vsm::any_flags(a.special, open_options::temporary))
	{
		if (a.path.base == nullptr)
		{
			//TODO: Set the default temp directory handle
			//a.path.base = get_default_temp_directory_handle();
		}
	}

	vsm_try_bind((handle, flags), _open(a));

	if (vsm::any_flags(a.mode.value_or_zero(), file_mode::read_data))
	{
		flags |= fs_object_t::flags::readable;
	}
	if (vsm::any_flags(a.mode.value_or_zero(), file_mode::write_data))
	{
		flags |= fs_object_t::flags::writable;
	}

	h.flags = object_t::flags::not_null | flags;
	h.platform_handle = wrap_handle(handle.release());

	return {};
}
