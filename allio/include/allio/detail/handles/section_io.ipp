[[nodiscard]] auto create_section(
	detail::fs_size const max_size,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<section_t, section_io::create_t> a = {};
	a.max_size = max_size;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<section_t, section_io::create_t>(a);
}

[[nodiscard]] auto create_section(
	detail::handle_for<detail::file_t> auto const& file,
	detail::fs_size const max_size,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<section_t, section_io::create_t> a = {};
	a.options = section_options::backing_file;
	a.backing_storage = file.native().file_t::native_type::platform_handle;
	a.max_size = max_size;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<section_t, section_io::create_t>(a);
}
