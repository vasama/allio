#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

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

struct event_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		auto_reset,
	);

	struct create_t
	{
		using mutation_tag = producer_t;

		using handle_type = event_t;
		using result_type = void;

		struct required_params_type
		{
			event_mode mode;
		};

		using optional_params_type = signal_event_t;

		using runtime_tag = bounded_runtime_t;
	};

	struct signal_t
	{
		using handle_type = event_t const;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;

		using runtime_tag = bounded_runtime_t;
	};

	struct reset_t
	{
		using handle_type = event_t const;
		using result_type = void;

		using required_params_type = no_parameters_t;
		using optional_params_type = no_parameters_t;

		using runtime_tag = bounded_runtime_t;
	};

	struct wait_t
	{
		using handle_type = event_t const;
		using result_type = void;
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	using operations = type_list_cat<
		base_type::operations,
		type_list<wait_t, signal_t, reset_t, wait_t>
	>;


	struct native_type : base_type::native_type
	{
		friend vsm::result<void> tag_invoke(
			blocking_io_t<create_t>,
			native_type& h,
			io_parameters_t<create_t> const& args)
		{
			return event_t::create(h, args);
		}
		
		friend vsm::result<void> tag_invoke(
			blocking_io_t<signal_t>,
			native_type const& h,
			io_parameters_t<signal_t> const& args)
		{
			return event_t::signal(h, args);
		}
		
		friend vsm::result<void> tag_invoke(
			blocking_io_t<reset_t>,
			native_type const& h,
			io_parameters_t<reset_t> const& args)
		{
			return event_t::reset(h, args);
		}
		
		friend vsm::result<void> tag_invoke(
			blocking_io_t<wait_t>,
			native_type const& h,
			io_parameters_t<wait_t> const& args)
		{
			return event_t::wait(h, args);
		}
	};


	static vsm::result<void> create(
		native_type& h,
		io_parameters_t<create_t> const& args);

	static vsm::result<void> signal(
		native_type const& h,
		io_parameters_t<signal_t> const& args);

	static vsm::result<void> reset(
		native_type const& h,
		io_parameters_t<reset_t> const& args);

	static vsm::result<void> wait(
		native_type const& h,
		io_parameters_t<wait_t> const& args);


	template<typename H>
	struct abstract_interface : base_type::abstract_interface<H>
	{
		[[nodiscard]] vsm::result<void> signal() const
		{
			return event_t::signal(
				static_cast<H const&>(*this).native(),
				io_parameters_t<signal_t>());
		}

		[[nodiscard]] vsm::result<void> reset() const
		{
			return event_t::reset(
				static_cast<H const&>(*this).native(),
				io_parameters_t<reset_t>());
		}
	};

	template<typename H, typename M>
	struct concrete_interface : base_type::concrete_interface<H, M>
	{
		[[nodiscard]] auto wait(auto&&... args) const
		{
			return generic_io<wait_t>(
				static_cast<H const&>(*this),
				io_args<wait_t>()(vsm_forward(args)...));
		}
	};
};

using abstract_event_handle = abstract_handle<event_t>;

namespace _event_b {

using event_handle = blocking_handle<event_t>;

vsm::result<event_handle> create_event(event_mode const mode, auto&&... args)
{
	vsm::result<event_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<event_t::create_t>(
		*r,
		io_args<event_t::create_t>(mode)(vsm_forward(args)...)));
	return r;
}

} // namespace _event_b

namespace _event_a {

template<typename MultiplexerHandle>
using basic_event_handle = async_handle<event_t, MultiplexerHandle>;

ex::sender auto create_event(event_mode const mode, auto&&... args)
{
	return io_handle_sender<event_t, event_t::create_t>(
		io_args<event_t::create_t>(mode)(vsm_forward(args)...));
}

} // namespace _event_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/event.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/event.hpp>
#endif
