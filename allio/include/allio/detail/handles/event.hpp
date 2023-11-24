#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

//TODO: Create separate types for manual and auto reset events.

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

struct signal_event_t
{
	bool signal;

	struct tag_t
	{
		struct argument_t
		{
			bool value;

			friend void tag_invoke(set_argument_t, signal_event_t& args, argument_t const& value)
			{
				args.signal = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(bool const signal)
		{
			return { signal };
		}

		friend void tag_invoke(set_argument_t, signal_event_t& args, tag_t)
		{
			args.signal = true;
		}
	};
};
inline constexpr signal_event_t::tag_t signal_event = {};

namespace event_io {

struct create_t
{
	using operation_concept = producer_t;
	struct required_params_type
	{
		event_mode mode;
	};
	using optional_params_type = signal_event_t;
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
	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
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
	using required_params_type = no_parameters_t;
	using optional_params_type = no_parameters_t;
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
	using required_params_type = no_parameters_t;
	using optional_params_type = deadline_t;
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


	struct native_type : base_type::native_type
	{
	};


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
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] vsm::result<void> signal() const
		{
			return event_t::signal(
				static_cast<Handle const&>(*this).native(),
				io_parameters_t<event_t, signal_t>());
		}

		[[nodiscard]] vsm::result<void> reset() const
		{
			return event_t::reset(
				static_cast<Handle const&>(*this).native(),
				io_parameters_t<event_t, reset_t>());
		}
	};

	template<typename Handle, optional_multiplexer_handle_for<event_t> MultiplexerHandle>
	struct concrete_interface : base_type::concrete_interface<Handle, MultiplexerHandle>
	{
		[[nodiscard]] auto wait(auto&&... args) const
		{
			return generic_io<wait_t>(
				static_cast<Handle const&>(*this),
				make_io_args<typename Handle::object_type, wait_t>()(vsm_forward(args)...));
		}
	};
};
using abstract_event_handle = abstract_handle<event_t>;

namespace _event_b {

using event_handle = blocking_handle<event_t>;

vsm::result<event_handle> create_event(event_mode const mode, auto&&... args)
{
	vsm::result<event_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<event_t, event_io::create_t>(
		*r,
		make_io_args<event_t, event_io::create_t>(mode)(vsm_forward(args)...)));
	return r;
}

} // namespace _event_b

namespace _event_a {

template<multiplexer_handle_for<event_t> MultiplexerHandle>
using basic_event_handle = async_handle<event_t, MultiplexerHandle>;

ex::sender auto create_event(event_mode const mode, auto&&... args)
{
	return io_handle_sender<event_t, event_io::create_t>(
		make_io_args<event_t, event_io::create_t>(mode)(vsm_forward(args)...));
}

} // namespace _event_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/event.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/event.hpp>
#endif
