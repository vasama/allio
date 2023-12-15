#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

//TODO: Create separate types for manual and auto reset events?

enum class event_mode : uint8_t
{
	/// @brief In manual reset mode, once signaled, an event object remains
	///        signaled until manually reset using @ref reset.
	manual_reset,

	/// @brief In automatic reset mode an event object automatically becomes
	///        unsignaled having been observed in the signaled state by a wait operation.
	auto_reset,
};

inline constexpr event_mode manual_reset_event = event_mode::manual_reset;
inline constexpr event_mode auto_reset_event = event_mode::auto_reset;

enum class event_options : uint8_t
{
	auto_reset                          = 1 << 0,
	initially_signaled                  = 1 << 1,
};
vsm_flag_enum(event_options);

struct initially_signaled_t
{
	bool initially_signaled;
};
inline constexpr explicit_parameter<initially_signaled_t> initially_signaled = {};

struct event_mode_t
{
	event_mode mode;
};

namespace event_io {

struct create_t
{
	using operation_concept = producer_t;

	struct params_type : io_flags_t
	{
		event_options options = {};

		friend void tag_invoke(set_argument_t, params_type& args, event_mode_t const value)
		{
			if (value.mode == auto_reset_event)
			{
				args.options |= event_options::auto_reset;
			}
		}

		friend void tag_invoke(set_argument_t, params_type& args, initially_signaled_t const value)
		{
			if (value.initially_signaled)
			{
				args.options |= event_options::initially_signaled;
			}
		}

		friend void tag_invoke(set_argument_t, params_type& args, explicit_parameter<initially_signaled_t>)
		{
			args.options |= event_options::initially_signaled;
		}
	};

	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, create_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, create_t> const& args)
		requires requires { Object::create(h, args); }
	{
		return Object::create(h, args);
	}
};

struct signal_t
{
	using operation_concept = void;
	using params_type = no_parameters_t;
	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, signal_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, signal_t> const& args)
		requires requires { Object::signal(h, args); }
	{
		return Object::signal(h, args);
	}
};

struct reset_t
{
	using operation_concept = void;
	using params_type = no_parameters_t;
	using result_type = void;
	using runtime_tag = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, reset_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, reset_t> const& args)
		requires requires { Object::reset(h, args); }
	{
		return Object::reset(h, args);
	}
};

struct wait_t
{
	using operation_concept = void;
	using params_type = deadline_t;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, wait_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, wait_t> const& args)
		requires requires { Object::wait(h, args); }
	{
		return Object::wait(h, args);
	}
};

} // namespace event_io

struct event_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		auto_reset,
	);

	using create_t = event_io::create_t;
	using signal_t = event_io::signal_t;
	using reset_t = event_io::reset_t;
	using wait_t = event_io::wait_t;

	using operations = type_list_append
	<
		base_type::operations
		, create_t
		, signal_t
		, reset_t
		, wait_t
	>;


	static vsm::result<void> create(
		native_type& h,
		io_parameters_t<event_t, create_t> const& args);

	static vsm::result<void> signal(
		native_type const& h,
		io_parameters_t<event_t, signal_t> const& args);

	static vsm::result<void> reset(
		native_type const& h,
		io_parameters_t<event_t, reset_t> const& args);

	static vsm::result<void> wait(
		native_type const& h,
		io_parameters_t<event_t, wait_t> const& args);


	template<typename Handle>
	struct concrete_interface : base_type::concrete_interface<Handle>
	{
		[[nodiscard]] auto signal() const
		{
			return Handle::io_traits_type::unwrap_result(
				blocking_io<typename Handle::object_type, signal_t>(
					static_cast<Handle const&>(*this),
					io_parameters_t<typename Handle::object_type, signal_t>()));
		}

		[[nodiscard]] auto reset() const
		{
			return Handle::io_traits_type::unwrap_result(
				blocking_io<typename Handle::object_type, reset_t>(
					static_cast<Handle const&>(*this),
					io_parameters_t<typename Handle::object_type, reset_t>()));
		}

		[[nodiscard]] auto wait(auto&&... args) const
		{
			return Handle::io_traits_type::template observe<wait_t>(
				static_cast<Handle const&>(*this),
				io_parameters_t<typename Handle::object_type, wait_t>(vsm_forward(args)...));
		}
	};
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/event.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/event.hpp>
#endif
