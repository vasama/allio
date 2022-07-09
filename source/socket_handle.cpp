#include <allio/socket_handle.hpp>

using namespace allio;

result<detail::common_socket_handle_base::native_handle_type> detail::common_socket_handle_base::release_native_handle()
{
	return platform_handle::release_native_handle();
}

result<void> detail::common_socket_handle_base::set_native_handle(native_handle_type const handle)
{
	return platform_handle::set_native_handle(handle);
}

result<void> detail::common_socket_handle_base::create(network_address_kind const address_kind, socket_parameters const& args)
{
	if (*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor)); //TODO: use better error
	}

	socket_parameters args_copy = args;
	if (multiplexer* const multiplexer = get_multiplexer())
	{
		args_copy.handle_flags |= flags::multiplexable;

		if (!is_synchronous<io::socket>(*this))
		{
			return block<io::socket>(*this, address_kind, args_copy);
		}
	}

	return create_sync(address_kind, args_copy);
}

result<void> detail::socket_handle_base::connect(network_address const& address)
{
	if (!*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	if (multiplexer* const multiplexer = get_multiplexer())
	{
		return block<io::connect>(static_cast<socket_handle&>(*this), address);
	}

	return connect_sync(address);
}

result<void> detail::listen_socket_handle_base::listen(network_address const& address, listen_parameters const& args)
{
	if (!*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor)); //TODO: use better error
	}

	if (multiplexer* const multiplexer = get_multiplexer())
	{
		if (!is_synchronous<io::listen>(*this))
		{
			return block<io::listen>(static_cast<listen_socket_handle&>(*this), address, args);
		}
	}

	return listen_sync(address, args);
}

result<accept_result> detail::listen_socket_handle_base::accept(socket_parameters const& create_args) const
{
	if (!*this)
	{
		return allio_ERROR(make_error_code(std::errc::bad_file_descriptor));
	}

	if (multiplexer* const multiplexer = get_multiplexer())
	{
		return block<io::accept>(static_cast<listen_socket_handle const&>(*this), create_args);
	}

	return accept_sync(create_args);
}

result<socket_handle> allio::connect(network_address const& address, socket_parameters const& create_args)
{
	result<socket_handle> r = result_value;
	allio_TRYV(r->create(address.kind(), create_args));
	allio_TRYV(r->connect(address));
	return r;
}

result<listen_socket_handle> allio::listen(network_address const& address, listen_parameters const& args, socket_parameters const& create_args)
{
	result<listen_socket_handle> r = result_value;
	allio_TRYV(r->create(address.kind(), create_args));
	allio_TRYV(r->listen(address, args));
	return r;
}

allio_TYPE_ID(socket_handle);
allio_TYPE_ID(listen_socket_handle);
