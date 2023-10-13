#pragma once

#include <allio/detail/handles/platform_handle.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

enum class event_reset_mode : uint8_t
{
	/// @brief In manual reset mode, once signaled, an event object remains
	///        signaled until manually reset using @ref reset.
	manual_reset,

	/// @brief In automatic reset mode an event object automatically becomes
	///        unsignaled having been observed in the signaled state by a wait operation.
	auto_reset,
};

inline constexpr event_reset_mode manual_reset_event = event_reset_mode::manual_reset;
inline constexpr event_reset_mode auto_reset_event = event_reset_mode::auto_reset;

struct signal_event_t
{
	struct parameter_t
	{
		bool signal;
	};

	struct argument_t
	{
		bool signal;

		friend void tag_invoke(set_argument_t, parameter_t& args, argument_t const& value)
		{
			args.signal = value.signal;
		}
	};

	argument_t vsm_static_operator_invoke(bool const signal)
	{
		return { signal };
	}

	friend void tag_invoke(set_argument_t, parameter_t& args, signal_event_t)
	{
		args.signal = true;
	}
};
inline constexpr signal_event_t signal_event = {};

class event_handle_t : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	allio_handle_flags
	(
		auto_reset,
	);


	using reset_mode = event_reset_mode;
	using enum reset_mode;

	[[nodiscard]] reset_mode get_reset_mode() const
	{
		vsm_assert(*this); //PRECONDITION
		return get_flags()[flags::auto_reset] ? auto_reset : manual_reset;
	}


	struct create_t
	{
		static constexpr bool producer = true;
	
		using handle_type = event_handle_t;
		using result_type = void;

		struct required_params_type
		{
			event_handle_t::reset_mode reset_mode;
		};

		using optional_params_type = signal_event_t::parameter_t;
	};


	/// @brief Signal the event object.
	///        * If the event object is already signaled, this operation has no effect.
	///        * Otherwise if there are no pending waits, the event object becomes signaled.
	///        * Otherwise if the reset mode is @ref event_handle::auto_reset,
	///          one pending wait is completed and the event object remains unsignaled.
	///        * Otherwise if the reset mode is @ref event_handle::manual_reset,
	///          all pending waits are completed and the event object becomes signaled.
	[[nodiscard]] vsm::result<void> signal() const;

	/// @brief Reset the event object.
	///        * Any currently pending waits are not affected.
	///        * Any new waits will pend until the event object is signaled again.
	[[nodiscard]] vsm::result<void> reset() const;


	struct wait_t
	{
		using handle_type = event_handle_t const;
		using result_type = void;
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	[[nodiscard]] vsm::result<void> blocking_wait(auto&&... args) const
	{
		return do_blocking_io(
			*this,
			no_result,
			io_args<wait_t>()(vsm_forward(args)...));
	}


	using asynchronous_operations = type_list_cat
	<
		base_type::asynchronous_operations,
		type_list<wait_t>
	>;

protected:
	allio_detail_default_lifetime(event_handle_t);

	template<typename H, typename M>
	struct interface : base_type::interface<H, M>
	{
		/// @brief Wait for the event object to be signaled.
		///        See @ref signal for more information.
		[[nodiscard]] auto wait(auto&&... args) const
		{
			return handle::invoke(
				static_cast<H const&>(*this),
				io_args<wait_t>()(vsm_forward(args)...));
		}
	};

	static vsm::result<void> do_blocking_io(
		event_handle_t& h,
		io_result_ref_t<create_t> r,
		io_parameters_t<create_t> const& args);

	static vsm::result<void> do_blocking_io(
		event_handle_t const& h,
		io_result_ref_t<wait_t> r,
		io_parameters_t<wait_t> const& args);
};

template<typename Multiplexer>
using basic_event_handle = basic_handle<event_handle_t, Multiplexer>;


vsm::result<basic_event_handle<void>> create_event(
	event_reset_mode const reset_mode,
	auto&&... args)
{
	vsm::result<basic_event_handle<void>> r(vsm::result_value);
	vsm_try_void(blocking_io(
		*r,
		no_result,
		io_args<event_handle_t::create_t>(reset_mode)(vsm_forward(args)...)));
	return r;
}

template<multiplexer_for<event_handle_t> Multiplexer>
vsm::result<basic_event_handle<multiplexer_handle_t<Multiplexer>>> create_event(
	Multiplexer&& multiplexer,
	event_reset_mode const reset_mode,
	auto&&... args)
{
	vsm_try(event, create_event(reset_mode, vsm_forward(args)...));
	return event.with_multiplexer(vsm_forward(multiplexer));
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/event_handle.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/event_handle.hpp>
#endif
