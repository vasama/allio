#include "example-library.h"

#include <allio/blocking/event.hpp>
#include <allio/default_multiplexer.hpp>
#include <allio/senders/event.hpp>
#include <allio/senders/raw_listen_socket.hpp>

#include <exec/task.hpp>

#include <mutex>

namespace abi = example::library::abi;
namespace ex = stdexec;
namespace io = allio::senders;

namespace {

class client_context : abi::client_context
{
	io::socket_handle const m_socket;

	std::mutex m_send_list_mutex;
	io::event_handle m_send_list_event;
	std::list<std::string> m_send_list;

	abi::client_handler& m_handler;

public:
	explicit client_context(io::socket_handle&& socket, abi::server_handler& server_handler)
		: m_socket(vsm_move(socket))
		, m_send_list_event(make_event(m_socket.get_multiplexer()))
		, m_handler(server_handler.on_new_connection(*this))
	{
	}

	client_context(client_context const&) = delete;
	client_context& operator=(client_context const&) = delete;

	auto loop()
	{
		return ex::when_all(receive_loop(), send_loop());
	}

private:
	io::event_handle make_event(allio::default_multiplexer_handle const& handle)
	{
		return allio::blocking::create_event(allio::auto_reset_event).via(handle);
	}

	exec::task<void> receive_loop()
	{
		std::vector<char> buffer;
		buffer.resize(1024);

		for (size_t buffer_data_size = 0;;)
		{
			uint32_t message_size;
			while (buffer_data_size < sizeof(message_size))
			{
				buffer_data_size += co_await m_socket.read_some(as_read_buffer(
					buffer.data() + buffer_data_size,
					buffer.size() - buffer_data_size));
			}
			std::memcpy(&message_size, buffer.data(), sizeof(message_size));

			size_t buffer_message_size = message_size + sizeof(message_size);
			if (buffer_message_size > buffer.size())
			{
				buffer.resize(buffer_message_size);
			}

			while (buffer_data_size < buffer_message_size)
			{
				buffer_data_size += co_await m_socket.read_some(as_read_buffer(
					buffer.data() + buffer_data_size,
					buffer.size() - buffer_data_size));
			}

			std::string_view const message(buffer.data() + sizeof(message_size), message_size);
			m_handler.on_message_received(message);

			buffer.erase(buffer.begin(), buffer.begin() + buffer_message_size);
			buffer_data_size -= buffer_message_size;
		}
	}

	exec::task<void> send_loop()
	{

	}
};

static exec::task<void> client_loop(
	io::socket_handle const socket,
	abi::server_handler& server_handler)
{
	client_context context(socket);
	client_handler& handler = server_handler.on_new_connection(context);

}

static exec::task<void> server_loop(abi::server_handler& handler)
{
	auto const listen_socket = co_await io::listen();

	exec::async_scope scope;

	while (true)
	{
		auto socket = co_await listen_socket.accept();

		scope.spawn(
			ex::just(vsm_move(socket))
			| ex::let_value([&](io::socket_handle socket)
			{
			})
			| ex::upon_error([&](auto&& error)
			{
				
			}));
	}
}

struct server : allio_abi_object_v1
{
	allio::default_multiplexer m_multiplexer;

	server(abi::server_handler& handler)
	{
		allio_abi_object_v1::version = allio_abi_v1;
		allio_abi_object_v1::functions = &functions;
	}

	allio_abi_result notify()
	{
		m_multiplexer.poll();
	}

	static allio_abi_object_functions_v1 const functions;
};

allio_abi_object_functions_v1 const server::functions =
{
	.close = [](allio_abi_object* const object)
	{
		delete static_cast<server*>(object);
	},

	.notify = [](allio_abi_object* const object, uintptr_t)
	{
		return static_cast<server*>(object)->notify(informaion);
	},
};

} // namespace

allio_abi_object_v1* abi::create_server(abi::server_handler& handler)
{
	return new server(handler);
}
