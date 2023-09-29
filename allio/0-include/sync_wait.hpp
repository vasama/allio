#pragma once

#include <allio/detail/execution.hpp>
#include <allio/detail/manual_scheduler.hpp>
#include <allio/multiplexer.hpp>

#include <vsm/type_traits.hpp>
#include <vsm/utility.hpp>

#include <exception>
#include <optional>
#include <tuple>
#include <variant>

namespace allio {
namespace detail {

struct sync_wait_pending_tag {};
struct sync_wait_stopped_tag {};

template<bool Except>
struct sync_wait_storage;

template<>
struct sync_wait_storage<0>
{
	template<typename Value>
	using type = std::variant<
		sync_wait_pending_tag,
		sync_wait_stopped_tag,
		Value
	>;
};

template<>
struct sync_wait_storage<1>
{
	template<typename Value>
	using type = std::variant<
		sync_wait_pending_tag,
		sync_wait_stopped_tag,
		Value,
		std::exception_ptr
	>;
};

template<typename Value, bool Except>
class sync_wait_context : public manual_scheduler
{
	typename sync_wait_storage<Except>::template type<Value> m_state;

public:
	sync_wait_context()
		: m_state(sync_wait_pending_tag{})
	{
	}

	sync_wait_context(sync_wait_context const&) = delete;
	sync_wait_context& operator=(sync_wait_context const&) = delete;

	bool is_pending() const
	{
		return std::holds_alternative<sync_wait_pending_tag>(m_state);
	}

	std::optional<Value> get_value()
	{
		if (std::holds_alternative<sync_wait_stopped_tag>(m_state))
		{
			return std::nullopt;
		}

		if constexpr (Except)
		{
			if (std::holds_alternative<std::exception_ptr>(m_state))
			{
				std::rethrow_exception(std::get<std::exception_ptr>(vsm_move(m_state)));
			}
		}

		return std::get<Value>(vsm_move(m_state));
	}

	void allio_detail_set_stopped() noexcept
	{
		vsm_assume(std::holds_alternative<sync_wait_pending_tag>(m_state));
		m_state.template emplace<sync_wait_stopped_tag>();
	}

	void set_value(auto&&... values)
	{
		vsm_assume(std::holds_alternative<sync_wait_pending_tag>(m_state));
		m_state.template emplace<Value>(vsm_forward(values)...);
	}

	void set_error(std::exception_ptr exception) noexcept requires Except
	{
		vsm_assume(std::holds_alternative<sync_wait_pending_tag>(m_state));
		m_state.template emplace<std::exception_ptr>(vsm_move(exception));
	}
};

template<typename Value, bool Except>
class sync_wait_receiver
{
	using promise_type = sync_wait_context<Value, Except>;

	promise_type* m_context;

public:
	explicit sync_wait_receiver(promise_type& context)
		: m_context(&context)
	{
	}

	void allio_detail_set_stopped() && noexcept
	{
		m_context->allio_detail_set_stopped();
	}

	void set_value(auto&&... values) &&
	{
		m_context->set_value(vsm_forward(values)...);
	}

	void set_error(auto&& error) && noexcept
	{
		m_context->set_error(vsm_forward(error));
	}

	friend scheduler_ref<manual_scheduler> tag_invoke(
		decltype(detail::execution::get_scheduler),
		sync_wait_receiver const& receiver) noexcept
	{
		return *receiver.m_context;
	}
};

template<typename Value, typename Sender>
auto sync_wait_impl(multiplexer& multiplexer, Sender&& sender)
{
	sync_wait_context<Value, 1> context;
	auto operation = detail::execution::connect(
		vsm_forward(sender),
		sync_wait_receiver<Value, 1>(context));
	detail::execution::start(operation);

	while (context.is_pending())
	{
		while (context.pump());

		if (!context.is_pending())
		{
			break;
		}

		// There is no good way to handle an error here, but pump should not fail
		// unless the multiplexer is in some invalid state (e.g. moved from).
		vsm_verify(multiplexer.pump());

		//TODO: Wake up the multiplexer if scheduling from other threads.
	}

	return context.get_value();
}

} // namespace detail

template<typename Sender>
auto sync_wait_with_variant(multiplexer& multiplexer, Sender&& sender)
{
	//TODO: Handle sender of nothing.
	//using value_type = detail::execution::sender_single_value_result_t<std::remove_cvref_t<Sender>>;
	//using value_or_void_type = std::conditional_t<std::is_same_v<value_type, void>, void, value_type>;
	//return detail::sync_wait_impl<value_or_void_type>(multiplexer, static_cast<Sender&&>(sender));

	//return detail::sync_wait_impl<1>(multiplexer, vsm_forward(sender));
}

template<typename T, typename... Ts>
	requires (sizeof...(Ts) == 0)
using sync_wait_single_variant = T;

template<typename Sender>
auto sync_wait(multiplexer& multiplexer, Sender&& sender)
{
	using sender_type = std::remove_cvref_t<Sender>;
	using tuple_type = unifex::sender_value_types_t<sender_type, sync_wait_single_variant, std::tuple>;
	return detail::sync_wait_impl<tuple_type>(multiplexer, vsm_forward(sender));
}

template<typename Sender>
auto sync_wait_single(multiplexer& multiplexer, Sender&& sender);

} // namespace allio
