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


class _event_handle : public platform_handle
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
		using handle_type = _event_handle;
		using result_type = void;

		struct required_params_type
		{
			_event_handle::reset_mode reset_mode;
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
		using handle_type = _event_handle const;
		using result_type = void;
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	/// @brief Wait for the event object to be signaled.
	///        See @ref signal for more information.
	[[nodiscard]] vsm::result<void> wait(auto&&... args) const
	{
		return do_blocking_io(*this, no_result, io_arguments_t<wait_t>()(vsm_forward(args)...));
	}


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
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		/// @brief Wait for the event object to become signaled.
		[[nodiscard]] io_sender<H, wait_t> wait_async(auto&&... args) const
		{
			return io_sender<H, wait_t>(
				static_cast<H const&>(*this),
				io_arguments_t<wait_t>()(vsm_forward(args)...));
		}
	};

private:
	friend vsm::result<void> tag_invoke(blocking_io_t, std::derived_from<_event_handle> auto& h, io_parameters_t<create_t> const& args)
	{
		return handle::initialize(h, [&](_event_handle& h)
		{
			return do_blocking_io(h, args);
		});
	}

protected:
	static vsm::result<void> do_blocking_io(
		_event_handle& h,
		io_result_ref_t<create_t> result,
		io_parameters_t<create_t> const& args);

	static vsm::result<void> do_blocking_io(
		_event_handle const& h,
		io_result_ref_t<wait_t> result,
		io_parameters_t<wait_t> const& args);
};

using blocking_event_handle = basic_blocking_handle<_event_handle>;

template<typename Multiplexer>
using basic_event_handle = basic_async_handle<_event_handle, Multiplexer>;


vsm::result<blocking_event_handle> create_event(event_reset_mode const reset_mode, auto&&... args)
{
	vsm::result<blocking_event_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_event_handle::create_t>(reset_mode)(vsm_forward(args)...)));
	return r;
}

template<typename Multiplexer>
vsm::result<basic_event_handle<Multiplexer>> create_event(Multiplexer&& multiplexer, event_reset_mode const reset_mode, auto&&... args)
{
	vsm::result<basic_event_handle<Multiplexer>> r(vsm::result_value, vsm_forward(multiplexer));
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_event_handle::create_t>(reset_mode)(vsm_forward(args)...)));
	return r;
}

} // namespace allio::detail
