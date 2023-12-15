template<detail::datagram_socket_object Object = datagram_socket_t>
[[nodiscard]] auto bind(network_endpoint const& endpoint, auto&&... args)
{
	using namespace detail;
	io_parameters_t<Object, socket_io::bind_t> a = {};
	a.endpoint = endpoint;
	(set_argument(a, vsm_forward(args)), ...);
	return io_traits::produce<Object, socket_io::bind_t>(a);
}

[[nodiscard]] auto raw_bind(network_endpoint const& endpoint, auto&&... args)
{
	return bind<raw_datagram_socket_t>(endpoint, vsm_forward(args)...);
}
