#pragma once

#include <allio/multiplexer.hpp>

#include <unifex/manual_lifetime.hpp>
#include <unifex/manual_lifetime_union.hpp>
#include <unifex/sender_concepts.hpp>

#include <exception>
#include <optional>

namespace allio {
namespace detail {

struct sync_wait_void {};

template<typename T>
using sync_wait_void_lift = std::conditional_t<std::is_void_v<T>, sync_wait_void, T>;

enum class sync_wait_state
{
	pending,
	done,
	value,
	error,
};

template<typename Result>
class sync_wait_promise
{
	sync_wait_state m_state;
	union
	{
		unifex::manual_lifetime<Result> m_value;
		unifex::manual_lifetime<std::exception_ptr> m_error;
	};

public:
	sync_wait_promise()
		: m_state(sync_wait_state::pending)
	{
	}

	sync_wait_promise(sync_wait_promise const&) = delete;
	sync_wait_promise& operator=(sync_wait_promise const&) = delete;

	~sync_wait_promise()
	{
		switch (m_state)
		{
		case sync_wait_state::value:
			unifex::deactivate_union_member(m_value);
			break;

		case sync_wait_state::error:
			unifex::deactivate_union_member(m_error);
			break;
		}
	}

	sync_wait_state state() const
	{
		return m_state;
	}

	void set_done()
	{
		allio_ASSERT(m_state == sync_wait_state::pending);
		m_state = sync_wait_state::done;
	}

	template<typename... Values>
	void set_value(Values&&... values)
	{
		allio_ASSERT(m_state == sync_wait_state::pending);
		unifex::activate_union_member(m_value, static_cast<Values&&>(values)...);
		m_state = sync_wait_state::value;
	}

	void set_error(std::exception_ptr&& error)
	{
		allio_ASSERT(m_state == sync_wait_state::pending);
		unifex::activate_union_member(m_error, static_cast<std::exception_ptr&&>(error));
		m_state = sync_wait_state::error;
	}

	Result&& get_value() &&
	{
		allio_ASSERT(m_state == sync_wait_state::value);
		return static_cast<Result&&>(m_value.get());
	}

	std::exception_ptr&& get_error() &&
	{
		allio_ASSERT(m_state == sync_wait_state::error);
		return static_cast<std::exception_ptr&&>(m_error.get());
	}
};

template<typename Result>
class sync_wait_receiver
{
	using promise_type = sync_wait_promise<Result>;

	promise_type* m_promise;

public:
	explicit sync_wait_receiver(promise_type& promise)
		: m_promise(&promise)
	{
	}

	void set_done() && noexcept
	{
		m_promise->set_done();
	}

	template<typename... Values>
	void set_value(Values&&... values) && noexcept
	{
		m_promise->set_value(static_cast<Values&&>(values)...);
	}

	void set_error(std::exception_ptr&& error) && noexcept
	{
		m_promise->set_error(static_cast<std::exception_ptr&&>(error));
	}

	void set_error(std::error_code const error) && noexcept
	{
		static_cast<sync_wait_receiver&&>(*this).set_error(
			std::make_exception_ptr(std::system_error{ error, "allio::sync_wait" }));
	}

	template<typename Error>
	void set_error(Error&& error) && noexcept
	{
		static_cast<sync_wait_receiver&&>(*this).set_error(
			std::make_exception_ptr(static_cast<Error&&>(error)));
	}
};

template<typename Result, typename Sender>
result<Result> sync_wait_impl(multiplexer& multiplexer, Sender&& sender)
{
	sync_wait_promise<Result> promise;
	auto operation = unifex::connect(
		static_cast<Sender&&>(sender),
		sync_wait_receiver<Result>(promise));
	unifex::start(operation);

	while (true)
	{
		switch (promise.state())
		{
		case sync_wait_state::done:
			return allio_ERROR(error::async_operation_cancelled);

		case sync_wait_state::value:
			return static_cast<sync_wait_promise<Result>&&>(promise).get_value();

		case sync_wait_state::error:
			std::rethrow_exception(static_cast<sync_wait_promise<Result>&&>(promise).get_error());
		}

		if (auto const r = multiplexer.submit_and_poll(); !r)
		{
			//TODO: cancel operation
			return allio_ERROR(r.error());
		}
	}
}

template<typename Sender>
auto sync_wait(multiplexer& multiplexer, Sender&& sender)
{
	using Result = unifex::sender_single_value_result_t<std::remove_cvref_t<Sender>>;
	return sync_wait_impl<Result>(multiplexer, static_cast<Sender&&>(sender));
}

} // namespace detail

using detail::sync_wait;

} // namespace allio
