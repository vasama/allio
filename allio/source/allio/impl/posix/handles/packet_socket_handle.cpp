
vsm::result<void> packet_socket_handle_base::sync_impl(io::parameters_with_result<io::socket_create> const& args)
{
	return sync_create_socket<packet_socket_handle>(args);
}

vsm::result<void> packet_socket_handle_base::sync_impl(io::parameters_with_result<io::packet_scatter_read> const& args)
{
	return packet_scatter_read(args);
}

vsm::result<void> packet_socket_handle_base::sync_impl(io::parameters_with_result<io::packet_gather_write> const& args)
{
	return packet_gather_write(args);
}

allio_handle_implementation(packet_socket_handle);
