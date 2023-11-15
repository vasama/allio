#if 0

#include <allio/any_multiplexer.hpp>
#include <allio/default_multiplexer.hpp>
#include <allio/event.hpp>

#include <exec/async_scope.hpp>

#include <catch2/catch_all.hpp>

using namespace allio;
namespace ex = stdexec;

#if 0

class abstract_event_handle
{
	...
};

// no multiplexer
using a = event_handle<void>;

// default multiplexer
using b = event_handle<>;

// my_multiplexer
using c = event_handle<my_multiplexer>;


template<typename SecurityProvider = default_security_provider>
class abstract_secure_socket_handle
{
	...
};

template<typename Multiplexer = default_multiplexer&>
using secure_socket_handle = basic_handle<abstract_secure_socket_handle<>, Multiplexer>;

// no multiplexer
using a = secure_socket_handle<void>;

// default multiplexer
using b = secure_socket_handle<>;

// my_multiplexer
using c = secure_socket_handle<my_multiplexer>;


using x = basic_secure_socket_handle<my_security_provider>;

template<typename Multiplexer = default_multiplexer&>
using my_secure_socket_handle = basic_handle<x, Multiplexer>;

// no multiplexer
using a = my_secure_socket_handle<void>;

// default multiplexer
using b = my_secure_socket_handle<>;

// my_multiplexer
using c = my_secure_socket_handle<my_multiplexer>;

#endif

TEST_CASE("event_handle with any_multiplexer", "[any_multiplexer]")
{
	using any = any_multiplexer_handle<detail::_event_handle>;

	auto multiplexer = default_multiplexer::create().value();
	auto const event = create_event(any(multiplexer), auto_reset_event);

	exec::async_scope scope;

	bool signaled = false;
	scope.spawn_future(event.wait_async() | ex::then([&]()
	{
		signaled = true;
	}));

	event.signal().value();
	multiplexer.poll().value();

	REQUIRE(signaled);
}
#endif
