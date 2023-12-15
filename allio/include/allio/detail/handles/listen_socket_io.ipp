template<detail::listen_socket_object Object = listen_socket_t>
[[nodiscard]] auto listen(network_endpoint const& endpoint, auto&&... args)
{
	using namespace detail;
	io_parameters_t<Object, socket_io::listen_t> a = {};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<Object, socket_io::listen_t>(a);
}

[[nodiscard]] auto raw_listen(network_endpoint const& endpoint, auto&&... args)
{
	return listen<raw_listen_socket_t>(endpoint, vsm_forward(args)...);
}
