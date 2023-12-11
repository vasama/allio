#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

open_parameters open_parameters::make(
	open_kind const kind,
	fs_io::open_t::optional_params_type const& args)
{
	auto default_mode = file_mode::none;
	switch (kind)
	{
	case open_kind::file:
		default_mode = file_mode::read_write;
		break;

	case open_kind::directory:
		default_mode = file_mode::read;
		break;
	}
	auto const mode = args.mode.value_or(default_mode);

	auto default_creation = file_creation::open_existing;
	if (vsm::any_flags(default_mode, file_mode::write_data))
	{
		default_creation = file_creation::open_or_create;
	}
	auto const creation = args.creation.value_or(default_creation);

	auto h_flags = detail::handle_flags::none;
	if (vsm::any_flags(mode, file_mode::read))
	{
		h_flags |= fs_object_t::flags::readable;
	}
	if (vsm::any_flags(mode, file_mode::write))
	{
		h_flags |= fs_object_t::flags::writable;
	}

	return open_parameters
	{
		.kind = kind,
		.inheritable = args.inheritable,
		.mode = mode,
		.creation = creation,
		.sharing = args.sharing,
		.caching = args.caching,
		.flags = args.flags,
		.handle_flags = h_flags,
	};
}
