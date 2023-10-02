#pragma once

#include <allio/detail/event_queue.hpp>
#include <allio/detail/execution.hpp>
#include <allio/detail/get_multiplexer.hpp>
#include <allio/detail/handles/event_handle.hpp>

#include <vsm/standard.hpp>
#include <vsm/tag_invoke.hpp>
#include <vsm/utility.hpp>

#include <optional>

namespace allio::detail {
namespace _sync_wait {

template<typename Multiplexer>
class event
{
	basic_event_handle<Multiplexer*> m_event;

public:
	/*explicit*/ event(Multiplexer& multiplexer)
		: m_event(create_event(&multiplexer, auto_reset_event).value())
	{
	}

	void operator()() const
	{
		m_event.signal();
	}
};

template<typename Multiplexer>
using event_queue_t = basic_event_queue<event<Multiplexer>>;

template<typename Multiplexer>
class env
{
	Multiplexer* m_multiplexer;
	event_queue_t<Multiplexer>* m_event_queue;

public:
	explicit env(Multiplexer& multiplexer, event_queue_t<Multiplexer>& event_queue)
		: m_multiplexer(&multiplexer)
		, m_event_queue(&event_queue)
	{
	}

	friend auto tag_invoke(ex::get_scheduler_t, env const& self) noexcept
	{
		return self.m_event_queue->get_scheduler();
	}

	friend auto tag_invoke(ex::get_delegatee_scheduler_t, env const& self) noexcept
	{
		return self.m_event_queue->get_scheduler();
	}

	friend Multiplexer& tag_invoke(get_multiplexer_t, env const& self) noexcept
	{
		return *self.m_multiplexer;
	}
};

template<typename Multiplexer, typename Sender, typename Continuation>
using _result =
	ex::__try_value_types_of_t<
		Sender,
		env<Multiplexer>,
		ex::__transform<ex::__q<ex::__decay_t>, Continuation>,
		ex::__q<ex::__msingle>>;

template<typename Multiplexer, typename Sender>
using result =
	ex::__mtry_eval<
	_result,
		Multiplexer,
		Sender,
		ex::__q<std::tuple>>;

template<typename Result>
using variant = std::variant<
	std::monostate,
	Result,
	ex::set_stopped_t,
	std::exception_ptr>;

template<typename Multiplexer, typename Result>
struct receiver
{
	class type
	{
		using event_queue_type = event_queue_t<Multiplexer>;
		using variant_type = variant<Result>;

		Multiplexer* m_multiplexer;
		event_queue_type* m_event_queue;
		variant_type* m_variant;

	public:
		explicit type(Multiplexer& multiplexer, event_queue_type& event_queue, variant_type& variant)
			: m_multiplexer(&multiplexer)
			, m_event_queue(&event_queue)
			, m_variant(&variant)
		{
		}

		friend void tag_invoke(ex::set_value_t, type&& self, auto&&... values) noexcept
		{
			vsm_assert(self.m_variant->index() == 0);
			try
			{
				self.m_variant->template emplace<1>(vsm_forward(values)...);
			}
			catch (...)
			{
				self.set_error(std::current_exception());
			}
		}

		friend void tag_invoke(ex::set_error_t, type&& self, auto&& error) noexcept
		{
			vsm_assert(self.m_variant->index() == 0);
			self.set_error(vsm_forward(error));
		}

		friend void tag_invoke(ex::set_stopped_t, type&& self) noexcept
		{
			vsm_assert(self.m_variant->index() == 0);
			self.m_variant->template emplace<2>();
		}

		friend env<Multiplexer> tag_invoke(ex::get_env_t, type const& self) noexcept
		{
			return env<Multiplexer>(*self.m_multiplexer, *self.m_event_queue);
		}

	private:
		void set_error(auto&& error) noexcept
		{
			using error_type = std::decay_t<decltype(error)&&>;
			if constexpr (std::is_same_v<error_type, std::exception_ptr>)
			{
				m_variant->template emplace<3>(vsm_forward(error));
			}
			else if constexpr (std::is_same_v<error_type, std::error_code>)
			{
				m_variant->template emplace<3>(std::make_exception_ptr(std::system_error(error)));
			}
			else
			{
				m_variant->template emplace<3>(std::make_exception_ptr(vsm_forward(error)));
			}
		}
	};
	static_assert(ex::receiver<type>);
};

template<typename Multiplexer, typename Result>
using receiver_t = typename receiver<Multiplexer, Result>::type;

struct sync_wait_t
{
	template<typename Multiplexer, typename Sender>
	auto vsm_static_operator_invoke(Multiplexer& multiplexer, Sender&& sender)
		-> std::optional<result<Multiplexer, Sender&&>>
	{
		using result_type = result<Multiplexer, Sender&&>;

		event_queue_t<Multiplexer> event_queue(multiplexer);
		variant<result_type> variant{};

		auto operation_state = ex::connect(
			vsm_forward(sender),
			receiver_t<Multiplexer, result_type>(multiplexer, event_queue, variant));

		ex::start(operation_state);

		while (variant.index() == 0)
		{
			vsm_verify(multiplexer.poll());
		}

		if (variant.index() == 2)
		{
			return std::nullopt;
		}

		if (variant.index() == 3)
		{
			std::rethrow_exception(vsm_move(std::get<3>(variant)));
		}

		return vsm_move(std::get<1>(variant));
	}
};

} // namespace _sync_wait

using _sync_wait::sync_wait_t;
inline constexpr sync_wait_t sync_wait = {};

} // namespace allio::detail
