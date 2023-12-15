[[nodiscard]] auto create_pipe(auto&&... args)
{
	using namespace detail;
	io_parameters_t<pipe_pair_t, pipe_io::create_pair_t> a = {};
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::unwrap_result(_create_pipe(a));
}
