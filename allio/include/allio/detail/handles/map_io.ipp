[[nodiscard]] auto map_memory(
	size_t const size,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<map_t, map_io::map_memory_t> a = {};
	a.options = map_options::initial_commit;
	a.size = size;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<map_t, map_io::map_memory_t>(a);
}

[[nodiscard]] auto map_section(
	detail::handle_for<detail::section_t> auto const& section,
	detail::fs_size const offset,
	size_t const size,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<map_t, map_io::map_memory_t> a = {};
	a.options = map_options::initial_commit | map_options::backing_section;
	a.section = &section.native();
	a.section_offset = offset;
	a.size = size;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<map_t, map_io::map_memory_t>(a);
}
