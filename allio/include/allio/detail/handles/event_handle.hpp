#pragma once

#include <allio/detail/handles/platform_handle.hpp>

namespace allio::detail {

class _event_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	allio_handle_flags
	(
		auto_reset,
	);


	enum class reset_mode : uint8_t
	{
		/// @brief In manual reset mode, once signaled, an event object remains
		///        signaled until manually reset using @ref reset.
		manual_reset,

		/// @brief In automatic reset mode an event object automatically becomes
		///        unsignaled having been observed in the signaled state by a waiter.
		auto_reset,
	};

	using enum reset_mode;

	[[nodiscard]] reset_mode get_reset_mode() const
	{
		vsm_assert(*this); //PRECONDITION
		return get_flags()[flags::auto_reset]
			? reset_mode::auto_reset
			: reset_mode::manual_reset;
	}


	#define allio_event_handle_create_parameters(type, data, ...) \
		type(allio::event_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(bool, signal, false) \

	allio_interface_parameters(allio_event_handle_create_parameters);


	/// @brief Reset the event object. This has no effect on any currently waiting threads,
	///        but any new waiters will be blocked until the event object is signaled again.
	///        Behaves the same whether or not the event object is currently signaled.
	vsm::result<void> reset() const;

	/// @brief Signal the event object. This causes any waiting threads to be woken up.
	vsm::result<void> signal() const;


	struct wait_t;
	using wait_parameters = deadline_parameters;

	/// @brief Wait for the event object to become signaled.
	/// @note Internally synchronized with @ref signal.
	template<parameters<wait_parameters> P = wait_parameters::interface>
	vsm::result<void> wait(P const& args = {}) const
	{
		return wait_impl(args);
	}


	//TODO: Allow asynchronous reset?
	//      It is implementable asynchronously using io_uring...
	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			wait_t
		>
	>;

protected:
	allio_detail_default_lifetime(_event_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		//TODO: Explicit this could eliminate the need for the interface classes...
		template<parameters<create_parameters> P = create_parameters::interface>
		vsm::result<void> create(reset_mode const reset_mode, P const& args = {}) &
		{
			return handle::initialize(static_cast<H&>(*this), [&](auto& h)
			{
				return h._event_handle::_create(reset_mode, args);
			});
		}
	};

	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		/// @brief Wait for the event object to become signaled.
		/// @note Internally synchronized with @ref signal.
		template<parameters<wait_parameters> P = wait_parameters::interface>
		basic_sender<M, H, wait_t> wait_async(P const& args = {}) const;
	};

private:
	vsm::result<void> _create(reset_mode reset_mode, create_parameters const& args);
	vsm::result<void> _wait(wait_parameters const& args) const;
};

using event_handle = basic_handle<_event_handle>;

#if 0
template<>
struct io_operation_traits<event_handle::create_t>
{
	using handle_type = event_handle;
	using result_type = void;
	using params_type = event_handle::create_parameters;
};
#endif

template<>
struct io_operation_traits<event_handle::wait_t>
{
	using handle_type = event_handle const;
	using result_type = void;
	using params_type = event_handle::wait_parameters;
};

} // namespace allio::detail