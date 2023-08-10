#pragma once

#include <allio/platform_handle.hpp>

namespace allio {

namespace io {

struct event_create;
struct event_wait;

} // namespace io

namespace detail {

//TODO: Is there a better name for this than event?
class event_handle_base : public platform_handle
{
	using final_handle_type = final_handle<event_handle_base>;

public:
	using base_type = platform_handle;

	allio_handle_flags
	(
		auto_reset,
	);

	//TODO: Allow asynchronous reset?
	//      It is implementable asynchronously using io_uring...
	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::event_create,
			io::event_wait
		>
	>;


	#define allio_event_handle_create_parameters(type, data, ...) \
		type(allio::event_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(bool, auto_reset, false) \
		data(bool, signal, false) \

	allio_interface_parameters(allio_event_handle_create_parameters);

	using wait_parameters = basic_parameters;


	template<parameters<create_parameters> P = create_parameters::interface>
	vsm::result<void> create(P const& args = {})
	{
		return block_create(args);
	}

	/// @brief Wait for the event object to become signaled.
	/// @note Internally synchronized with @ref signal.
	template<parameters<wait_parameters> P = wait_parameters::interface>
	vsm::result<void> wait(P const& args = {}) const
	{
		return block_wait(args);
	}

	/// @brief Reset the event object. This has no effect on any currently waiting threads,
	///        but any new waiters will be blocked until the event object is signaled again.
	///        Behaves the same whether or not the event object is currently signaled.
	/// @note Internally synchronized with @ref signal.
	vsm::result<void> reset() const;

	/// @brief Signal the event object. This causes any waiting threads to be woken up.
	/// @note Internally synchronized with @ref wait and @ref reset.
	///       In particular this means that signal, and signal alone,
	///       may be called concurrently with @ref wait or @ref reset.
	vsm::result<void> signal() const;

protected:
	using base_type::base_type;

private:
	vsm::result<void> block_create(create_parameters const& args);
	vsm::result<void> block_wait(wait_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::event_create> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::event_wait> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<event_handle_base>> block_create_event(event_handle_base::create_parameters const& args);

} // namespace detail

using event_handle = final_handle<detail::event_handle_base>;


template<parameters<event_handle::create_parameters> P = event_handle::create_parameters::interface>
vsm::result<event_handle> create_event(P const& args = {})
{
	return detail::block_create_event(args);
}


template<>
struct io::parameters<io::event_create>
	: event_handle::create_parameters
{
	using handle_type = event_handle;
	using result_type = void;
};

template<>
struct io::parameters<io::event_wait>
	: event_handle::wait_parameters
{
	using handle_type = event_handle const;
	using result_type = void;
};

allio_detail_api extern allio_handle_implementation(event_handle);

} // namespace allio
