#include <allio/socket_handle.hpp>

#include "dynamic_storage.hpp"

int main()
{
	auto const r = []() -> allio::result<void>
	{
		// Create a default multiplexer.
		allio_TRY(multiplexer, allio::create_default_multiplexer());

		// Create null listening socket handle and associate the multiplexer.
		allio::listen_socket_handle listen_socket;
		allio_TRYV(listen_socket.set_multiplexer(multiplexer.get()));

		allio::network_address const address = allio::local_address("./allio.sock");

		// Start listening on the listen socket.
		allio_TRYV(listen_socket.listen(address);


		// Create null incoming socket handle and associate the multiplexer.
		allio::socket_handle incoming_socket;
		allio_TRYV(incoming_socket.set_multiplexer(multiplexer.get()));

		// Create null outgoing socket handle and associate the multiplexer.
		allio::socket_handle outgoing_socket;
		allio_TRYV(outgoing_socket.set_multiplexer(multiplexer.get()));


		allio::dynamic_storage accept_storage(listen_socket.get_async_operation_storage_requirements());
		{
			allio::accept_result result;

			allio::io::parameters<allio::io::accept> args;
			args.handle = &listen_socket;
			args.result = &result;

			allio_TRY(operation, allio::construct_and_start(listen_socket, accept_storage.get(), args, false));
		}

		allio::dynamic_storage connect_storage(outgoing_socket.get_async_operation_storage_requirements());
		{
			allio::io::parameters<allio::io::connect> args;
			args.handle = &outgoing_socket;
			args.address = &address;

			allio_TRY(operation, allio::construct_and_start(outgoing_socket, connect_storage.get(), args, false));
		}

		while (!)
		{
			multiplexer->submit_and_poll();
		}
	}();

	if (!r)
	{
		puts(r.error().message().c_str());
		return 1;
	}

	return 0;
}
