#pragma once

#include <allio/detail/deadline.hpp>
#include <allio/detail/handle.hpp>
#include <allio/detail/handles/platform_object.hpp>

namespace allio::detail {

enum class event_options : uint8_t
{
	auto_reset                          = 1 << 0,
	initially_signaled                  = 1 << 1,
};
vsm_flag_enum(event_options);

struct initially_signaled_t : explicit_argument<initially_signaled_t, bool> {};
inline constexpr explicit_parameter<initially_signaled_t> initially_signaled = {};

struct event_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		auto_reset,
	);

	struct create_t
	{
		using operation_concept = producer_t;

		struct params_type : io_flags_t
		{
			event_options options;

			friend void tag_invoke(
				set_argument_t,
				params_type& args,
				initially_signaled_t const value)
			{
				if (value.value)
				{
					args.options |= event_options::initially_signaled;
				}
			}

			friend void tag_invoke(
				set_argument_t,
				params_type& args,
				explicit_parameter<initially_signaled_t>)
			{
				args.options |= event_options::initially_signaled;
			}
		};

		using result_type = void;
		using runtime_tag = bounded_runtime_t;
	
		template<object Object>
		friend vsm::result<void> tag_invoke(
			blocking_io_t<create_t>,
			native_handle<Object>& h,
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
			blocking_io_t<signal_t>,
			native_handle<Object> const& h,
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
			blocking_io_t<reset_t>,
			native_handle<Object> const& h,
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
			blocking_io_t<wait_t>,
			native_handle<Object> const& h,
			io_parameters_t<Object, wait_t> const& args)
			requires requires { Object::wait(h, args); }
		{
			return Object::wait(h, args);
		}
	};

	using operations = type_list_append
	<
		base_type::operations
		, create_t
		, signal_t
		, reset_t
		, wait_t
	>;

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] auto signal() const
		{
			auto r = blocking_io<signal_t>(
				static_cast<Handle const&>(*this),
				io_parameters_t<event_t, event_t::signal_t>{});
	
			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}
	
		[[nodiscard]] auto reset() const
		{
			auto r = blocking_io<reset_t>(
				static_cast<Handle const&>(*this),
				io_parameters_t<event_t, event_t::reset_t>{});
	
			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}
	
		[[nodiscard]] auto wait(auto&&... args) const
		{
			auto a = io_parameters_t<event_t, event_t::wait_t>{};
			(set_argument(a, vsm_forward(args)), ...);
	
			return Traits::template observe<wait_t>(
				static_cast<Handle const&>(*this),
				a);
		}
	};

	static vsm::result<void> create(
		native_handle<event_t>& h,
		io_parameters_t<event_t, create_t> const& args);

	static vsm::result<void> signal(
		native_handle<event_t> const& h,
		io_parameters_t<event_t, signal_t> const& args);

	static vsm::result<void> reset(
		native_handle<event_t> const& h,
		io_parameters_t<event_t, reset_t> const& args);

	static vsm::result<void> wait(
		native_handle<event_t> const& h,
		io_parameters_t<event_t, wait_t> const& args);
};


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

template<typename Traits>
[[nodiscard]] auto create_event(event_mode const mode, auto&&... args)
{
	auto a = io_parameters_t<event_t, event_t::create_t>{};
	if (mode == event_mode::auto_reset)
	{
		a.options |= event_options::auto_reset;
	}
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<event_t, event_t::create_t>(a);
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/event.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/event.hpp>
#endif
