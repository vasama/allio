#include <allio/event_handle.hpp>

#include <vsm/atomic.hpp>

#include <catch2/catch_all.hpp>

#include <thread>
#include <vector>

using namespace allio;
//namespace ex = allio::detail::ex;

#if 0
namespace {

template<typename T, typename... Ts>
	requires (sizeof...(Ts) == 0)
using root_variant = T;

template<typename>
struct root_value;

template<typename T>
struct root_value<type_list<T>>
{
	using type = T;
};

template<>
struct root_value<type_list<>>
{
	using type = void;
};

template<typename Sender>
class root_t
{
	using tuple_type = detail::execution::sender_value_types_t<Sender, root_variant, type_list>;
	using value_type = typename root_value<tuple_type>::type;

	using optional_type = std::optional<vsm::result<value_type>>;

	class receiver
	{
		optional_type* m_result;

	public:
		receiver(optional_type& result)
			: m_result(&result)
		{
		}

		void set_value() &&
		{
			*m_result = vsm::result<void>();
		}

		void set_value(auto&& value) &&
		{
			*m_result = vsm_forward(value);
		}

		void set_error(auto&& value) &&
		{
			*m_result = vsm::unexpected(vsm_forward(value));
		}
	};

	optional_type m_optional;
	detail::execution::connect_result_t<Sender, receiver> m_operation;

public:
	explicit root_t(auto&& sender)
		: m_operation(detail::execution::connect(vsm_forward(sender), receiver(m_optional)))
	{
	}

	[[nodiscard]] bool is_done() const
	{
		return m_optional.has_value();
	}

	[[nodiscard]] vsm::result<value_type> const& get() const
	{
		return *m_optional;
	}
};

template<typename Sender>
auto root(Sender&& sender)
{
	return root_t<std::remove_cvref_t<Sender>>(vsm_forward(sender));
}

} // namespace
#endif

static vsm::result<bool> wait(event_handle const& event, deadline const deadline)
{
	auto const r = event.wait(
	{
		.deadline = deadline,
	});

	if (r)
	{
		return true;
	}

	if (r.error().default_error_condition() == std::errc::timed_out)
	{
		return false;
	}

	return vsm::unexpected(r.error());
}

static event_handle::reset_mode get_reset_mode(bool const manual_reset)
{
	return manual_reset
		? event_handle::manual_reset
		: event_handle::auto_reset;
}

TEST_CASE("basic signaling", "[event_handle]")
{
	bool const manual_reset = GENERATE(0, 1);
	bool is_signaled = GENERATE(0, 1);

	event_handle const event = create_event(get_reset_mode(manual_reset),
	{
		.signal = is_signaled,
	}).value();

	REQUIRE(event);

	auto const wait = [&]()
	{
		return ::wait(event, deadline::instant());
	};

	if (GENERATE(0, 1))
	{
		event.signal().value();
		is_signaled = true;
	}

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	event.reset().value();
	is_signaled = false;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	if (GENERATE(0, 1))
	{
		event.signal().value();
		is_signaled = true;
	}

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;

	REQUIRE(wait() == is_signaled);
	is_signaled &= manual_reset;
}

TEST_CASE("concurrent signaling", "[event_handle]")
{
	event_handle const event = create_event(event_handle::manual_reset).value();

	std::jthread const signal_thread([&]()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		event.signal().value();
	});

	REQUIRE(wait(event, std::chrono::milliseconds(1000)));
}

TEST_CASE("concurrent fighting", "[event_handle]")
{
	//TODO: Pick thread count based on CPU count.
	static constexpr size_t const thread_count = 4;
	static constexpr size_t const signal_count = 100'000;

	event_handle const event = create_event(event_handle::auto_reset).value();

	std::vector<std::jthread> wait_threads;
	std::vector<std::jthread> reset_threads;
	vsm::atomic<bool> observed = false;

	vsm::atomic<bool> stop_requested = false;
	vsm::atomic<size_t> stop_acknowledged = 0;
	vsm::atomic<size_t> signals_observed = 0;

	// Create waiting threads.
	for (size_t i = 0; i < thread_count; ++i)
	{
		wait_threads.push_back(std::jthread([&]()
		{
			size_t count = 0;

			while (true)
			{
				event.wait().value();

				if (stop_requested.load(std::memory_order_acquire))
				{
					break;
				}

				observed.store(true, std::memory_order_release);
				++count;
			}

			(void)signals_observed.fetch_add(count, std::memory_order_release);
			(void)stop_acknowledged.fetch_add(1, std::memory_order_release);
		}));
	}

	// Create resetting threads.
	for (size_t i = 0; i < thread_count; ++i)
	{
		reset_threads.push_back(std::jthread([&]()
		{
			while (!stop_requested.load(std::memory_order_acquire))
			{
				event.reset().value();
				std::this_thread::yield();
			}
		}));
	}

	// Signal the event in a loop, waiting for confirmation each time.
	for (size_t i = 0; i < signal_count; ++i)
	{
		event.signal().value();
		while (!observed.load(std::memory_order_acquire))
		{
			std::this_thread::yield();
		}
		observed.store(false, std::memory_order_release);
	}

	// Request and wait for all the threads to stop.
	stop_requested.store(true, std::memory_order_release);
	while (stop_acknowledged.load(std::memory_order_acquire) < thread_count)
	{
		event.signal().value();
		std::this_thread::yield();
	}

	REQUIRE(signals_observed.load(std::memory_order_acquire) == signal_count);
}


TEST_CASE("temp async")
{
	auto multiplexer = create_default_multiplexer().value();
	async_event_handle event = create_event(event_handle::auto_reset, { .multiplexable = true }).value();
	event.set_multiplexer(multiplexer);


	struct listener_t : detail::io_listener
	{
	};
	listener_t listener;

	detail::operation_state<default_multiplexer, detail::_event_handle, event_handle::wait_t> state;
	detail::create_io(multiplexer, event, state, &listener).value();
}

#if 0
#include <tuple>
#include <variant>

namespace __detach_future {

template<typename... Ts>
class detach_future
{
	using tuple_type = std::tuple<Ts...>;

	using variant_type = std::variant
	<
		std::monostate,
		tuple_type,
		std::error_code
	>;

	std::shared_ptr<variant_type> m_variant;

public:
	explicit detach_future(auto&& sender)
		: m_variant(std::make_shared<variant_type>())
	{
		execution::start_detached(
			vsm_forward(sender)
			| future_exec::then([variant = m_variant](auto&& value)
			{
				variant->emplace<T>(vsm_forward(value));
			})
		);
	}

	tuple_type& get()
	{
		vsm_assert(std::holds_alternative<tuple_type>(*m_variant));
		return std::get<tuple_type>(*m_variant);
	}

	tuple_type const& get() const
	{
		vsm_assert(std::holds_alternative<tuple_type>(*m_variant));
		return std::get<tuple_type>(*m_variant);
	}

	explicit operator bool() const
	{
		return !std::holds_alternative<std::monostate>(*m_variant);
	}
};

template<typename Sender, typename Continuation>
using x = __value_types_of_t<Sender, no_env, __transform<__q<__decay_t>, __q<Continuation>>, __q<__msingle>>;

template<typename Sender>
static auto detach(Sender&& sender)
{
	using future_type = x<Sender&&, __q<detach_future>>;
	return future_type(vsm_forward(sender));
}

} // namespace __detach_future

using  __detach_future::detach;
#endif

template<typename Sender>
static auto detach(async_scope& scope, Sender&& sender)
{
	

	scope.detach(
		vsm_forward(sender) |
		exec::then([](auto&&...)
		{

		})
	);
}

TEST_CASE("async signaling", "[event_handle]")
{
	bool const manual_reset = GENERATE(0, 1);

	auto multiplexer = create_default_multiplexer().value();
	auto event = create_event(get_reset_mode(manual_reset), { .multiplexable = true }).value();
	event.set_multiplexer(multiplexer).value();

	{
		async_scope scope;
		auto future = detach(scope, event.wait_async());
		REQUIRE(!future);

		multiplexer.poll().value();
		REQUIRE(!future);

		event.signal().value();
		REQUIRE(!future);

		multiplexer.poll().value();
		REQUIRE(future);

		multiplexer.poll().value();
		REQUIRE(future);
	}

	{
		async_scope scope;
		auto future = detach(scope, event.wait_async());
		REQUIRE(!future);

		multiplexer.poll().value();
		REQUIRE(static_cast<bool>(future) == manual_reset);

		if (!manual_reset)
		{
			event.signal().value();
			REQUIRE(!future);
		}

		multiplexer.poll().value();
		REQUIRE(future);

		multiplexer.poll().value();
		REQUIRE(future);
	}

	{
		event.reset().value();

		async_scope scope;
		auto future = detach(scope, event.wait_async());
		REQUIRE(!future);

		multiplexer.poll().value();
		REQUIRE(!future);

		event.signal().value();
		REQUIRE(!future);

		multiplexer.poll().value();
		REQUIRE(future);

		multiplexer.poll().value();
		REQUIRE(future);
	}
}
#endif
