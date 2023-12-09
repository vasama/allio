#include <allio/impl/handles/fs_object.hpp>

using namespace allio;
using namespace allio::detail;

open_parameters open_parameters::make(
	open_kind const kind,
	fs_io::open_t::optional_params_type const& args)
{
	file_mode default_mode = file_mode::none;
	switch (kind)
	{
	case open_kind::file:
		default_mode = file_mode::read_write;
		break;

	case open_kind::directory:
		default_mode = file_mode::read;
		break;
	}

	file_creation default_creation = file_creation::open_existing;
	if (vsm::any_flags(default_mode, file_mode::write_data))
	{
		default_creation = file_creation::open_or_create;
	}

	return open_parameters
	{
		.kind = kind,
		.inheritable = args.inheritable,
		.mode = args.mode.value_or(default_mode),
		.creation = args.creation.value_or(default_creation),
		.sharing = args.sharing,
		.caching = args.caching,
		.flags = args.flags,
	};
}
