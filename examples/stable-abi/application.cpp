#include <allio/default_multiplexer.hpp>
#include <allio/socket_handle.hpp>

#include "library.hpp"

using namespace example;

int main()
{
	static constexpr std::string_view endpoint_string = "127.0.0.1:51234";
	auto const endpoint = allio::ipv4_endpoint::parse(endpoint_string).value();

	auto listen_socket = allio::listen(endpoint).value();

	auto connection = library::connection(endpoint_string);
}
