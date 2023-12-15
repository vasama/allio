[[nodiscard]] auto open_directory(detail::fs_path const& path, auto&&... args)
{
	using namespace detail;
	io_parameters_t<directory_t, fs_io::open_t> a = {};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<directory_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_temp_directory(auto&&... args)
{
	using namespace detail;
	io_parameters_t<directory_t, fs_io::open_t> a = {};
	a.special = open_options::temporary;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<directory_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_unique_directory(auto&&... args)
{
	using namespace detail;
	io_parameters_t<directory_t, fs_io::open_t> a = {};
	a.special = open_options::unique_name;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<directory_t, fs_io::open_t>(a);
}
