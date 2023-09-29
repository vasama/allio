#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/detail/io_sender.hpp>

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
		///        unsignaled having been observed in the signaled state by a wait operation.
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


	struct create_t
	{

	};

	#define allio_event_handle_create_parameters(type, data, ...) \
		type(allio::event_handle, create_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(bool, signal, false) \

	allio_interface_parameters(allio_event_handle_create_parameters);


	/// @brief Signal the event object.
	///        * If the event object is already signaled, this operation has no effect.
	///        * Otherwise if there are no pending waits, the event object becomes signaled.
	///        * Otherwise if the reset mode is @ref allio::event_handle::auto_reset,
	///          one pending wait is completed and the event object remains unsignaled.
	///        * Otherwise if the reset mode is @ref allio::event_handle::manual_reset,
	///          all pending waits are completed and the event object becomes signaled.
	[[nodiscard]] vsm::result<void> signal() const;

	/// @brief Reset the event object.
	///        * Any currently pending waits are not affected.
	///        * Any new waits will pend until the event object is signaled again.
	[[nodiscard]] vsm::result<void> reset() const;


	struct wait_t
	{
		using handle_type = _event_handle const;
		using result_type = void;
		using params_type = deadline_parameters;
	};


	using wait_parameters = deadline_parameters;

	/// @brief Wait for the event object to be signaled.
	///        See @ref signal for more information.
	template<parameters<wait_parameters> P = wait_parameters::interface>
	[[nodiscard]] vsm::result<void> wait(P const& args = {}) const
	{
		return _wait(args);
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
		[[nodiscard]] vsm::result<void> create(reset_mode const reset_mode, P const& args = {}) &
		{
			return handle::initialize(static_cast<H&>(*this), [&](auto& h)
			{
				return h._event_handle::_create(reset_mode, args);
			});
		}
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		/// @brief Wait for the event object to become signaled.
		template<parameters<wait_parameters> P = wait_parameters::interface>
		[[nodiscard]] io_sender<H, wait_t> wait_async(P const& args = {}) const
		{
			return io_sender<H, wait_t>(static_cast<H const&>(*this));
		}
	};

private:
	vsm::result<void> _create(reset_mode reset_mode, create_parameters const& args);
	vsm::result<void> _wait(wait_parameters const& args) const;

	friend vsm::result<void> tag_invoke(blocking_io_t, std::derived_from<_event_handle> auto& h, io_parameters<create_t> const& args)
	{
		return handle::initialize(h, [&](auto& h)
		{
			return blocking_io(static_cast<_event_handle&>(h), args);
		});
	}

	//static vsm::result<void> _create(_event_handle& h, io_parameters<create_t> const& args);
	static vsm::result<void> _wait(_event_handle& h, io_parameters<wait_t> const& args);
};

template<>
struct io_operation_traits<_event_handle::wait_t>
{
	using handle_type = _event_handle const;
	using result_type = void;
	using params_type = _event_handle::wait_parameters;
};

using event_handle = basic_handle<_event_handle>;

template<typename Multiplexer>
using basic_event_handle = basic_async_handle<detail::_event_handle, Multiplexer>;

} // namespace allio::detail
