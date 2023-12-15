template<detail::socket_object Object = socket_t>
[[nodiscard]] auto connect(network_endpoint const& endpoint, auto&&... args)
{
	using namespace detail;
	io_parameters_t<Object, socket_io::connect_t> a = {};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<Object, socket_io::connect_t>(a);
}

[[nodiscard]] auto raw_connect(network_endpoint const& endpoint, auto&&... args)
{
	return connect<raw_socket_t>(endpoint, vsm_forward(args)...);
}
