[[nodiscard]] auto open_file(detail::fs_path const& path, auto&&... args)
{
	using namespace detail;
	io_parameters_t<file_t, fs_io::open_t> a = {};
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<file_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_temp_file(auto&&... args)
{
	using namespace detail;
	io_parameters_t<file_t, fs_io::open_t> a = {};
	a.special = open_options::temporary;
	a.path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<file_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_unique_file(
	detail::handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<file_t, fs_io::open_t> a = {};
	a.special = open_options::unique_name;
	a.path.base = base.native().fs_object_t::native_type::platform_handle;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<file_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_anonymous_file(
	detail::handle_for<fs_object_t> auto const& base,
	auto&&... args)
{
	using namespace detail;
	io_parameters_t<file_t, fs_io::open_t> a = {};
	a.special = open_options::anonymous;
	a.path.base = base.native().fs_object_t::native_type::platform_handle;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<file_t, fs_io::open_t>(a);
}

[[nodiscard]] auto open_null_device(auto&&... args)
{
	using namespace detail;
	io_parameters_t<file_t, fs_io::open_t> a = {};
	a.path = detail::get_null_device_path();
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<file_t, fs_io::open_t>(a);
}
