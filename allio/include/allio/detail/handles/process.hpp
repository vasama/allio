#pragma once

#include <allio/any_string.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/integer_id.hpp>
#include <allio/detail/io_sender.hpp>

namespace allio::detail {

struct unix_process_reaper;


struct process_t;

/// Windows: Valid process IDs have a range of [1, 2^32).
/// Linux: Valid process IDs have a range of [1, 2^22].
using process_id = integer_id<process_t, uint32_t>;

using process_exit_code = int32_t;


enum class process_options : uint8_t
{
	launch_detached                     = 1 << 0,
	inherit_handles                     = 1 << 1,
	set_environment                     = 1 << 2,
	wait_on_close                       = 1 << 3,
};

struct inherit_handles_implicit_t
{
	bool inherit_handles;
};

struct inherit_handles_explicit_t
{
	//TODO: Use a type-erased view like any_string_span over stream handles
};

struct inherit_handles_t
{
	inherit_handles_implicit_t vsm_static_operator_invoke(bool const inherit_handles)
	{
		return { inherit_handles };
	}

	inherit_handles_explicit_t vsm_static_operator_invoke()
	{
		//TODO
	}
};
inline constexpr inherit_handles_t inherit_handles = {};

struct wait_on_close_t
{
	bool wait_on_close;
};
inline constexpr explicit_parameter<wait_on_close_t> wait_on_close = {};

struct process_arguments_t
{
	any_string_span arguments;
};
inline constexpr explicit_parameter<process_arguments_t> process_arguments = {};

struct working_directory_t
{
	fs_path path;
};
inline constexpr explicit_parameter<working_directory_t> working_directory = {};

struct process_environment_t
{
	any_string_span arguments;
};
inline constexpr explicit_parameter<process_environment_t> process_environment = {};

template<int Stream>
struct redirect_stream_t
{
	native_platform_handle stream;
};

inline constexpr explicit_parameter<redirect_stream_t<0>> redirect_stdin = {};
inline constexpr explicit_parameter<redirect_stream_t<1>> redirect_stdout = {};
inline constexpr explicit_parameter<redirect_stream_t<2>> redirect_stderr = {};

struct with_exit_code_t
{
	process_exit_code exit_code;
};
inline constexpr explicit_parameter<with_exit_code_t> with_exit_code = {};

namespace process_io {

struct open_parameters : io_flags_t
{
	process_id id = {};

	friend void tag_invoke(set_argument_t, launch_parameters& args, process_id const value)
	{
		args.id = value;
	}
};

struct launch_parameters : io_flags_t
{
	process_options options = {};
	any_string_span arguments = {};
	any_string_span environment = {};
	fs_path working_directory = {};
	native_platform_handle redirect_stdin = {};
	native_platform_handle redirect_stdout = {};
	native_platform_handle redirect_stderr = {};
	//TODO: Explicit handle inheritance

	friend void tag_invoke(set_argument_t, launch_parameters& args, inherit_handles_t)
	{
		args.options |= process_options::inherit_handles;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, inherit_handles_implicit_t const value)
	{
		if (value.inherit_handles)
		{
			args.options |= process_options::inherit_handles;
		}
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, wait_on_close_t const value)
	{
		if (value.wait_on_close)
		{
			args.options |= process_options::wait_on_close;
		}
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, process_arguments_t const value)
	{
		args.arguments = value.arguments;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, process_environment_t const value)
	{
		args.environment = value.environment;
		args.options |= process_options::set_environment;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, working_directory_t const value)
	{
		args.working_directory = value.path;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, redirect_stream_t<0> const value)
	{
		args.redirect_stdin = value.stream;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, redirect_stream_t<1> const value)
	{
		args.redirect_stdout = value.stream;
	}

	friend void tag_invoke(set_argument_t, launch_parameters& args, redirect_stream_t<2> const value)
	{
		args.redirect_stderr = value.stream;
	}
};

struct teminate_parameters
{
	bool user_exit_code = false;
	process_exit_code exit_code;

	friend void tag_invoke(set_argument_t, launch_parameters& args, with_exit_code const value)
	{
		args.user_exit_code = true;
		args.exit_coee = value.exit_code;
	}
};


struct open_t
{
	using operation_concept = producer_t;
	using params_type = open_parameters;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, open_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, open_t> const& a)
		requires requires { Object::open(h, a); }
	{
		return Object::open(h, a);
	}
};

struct launch_t
{
	using operation_concept = producer_t;
	using params_type = launch_parameters;
	using result_type = void;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, launch_t>,
		typename Object::native_type& h,
		io_parameters_t<Object, launch_t> const& a)
		requires requires { Object::launch(h, a); }
	{
		return Object::launch(h, a);
	}
};

struct terminate_t
{
	using operation_concept = void;
	using params_type = teminate_parameters;
	using result_type = void;
	using runtime_concept = bounded_runtime_t;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, terminate_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, terminate_t> const& a)
		requires requires { Object::terminate(h, a); }
	{
		return Object::terminate(h, a);
	}
};

struct wait_t
{
	using operation_concept = void;
	using params_type = deadline_t;
	using result_type = process_exit_code;

	template<object Object>
	friend vsm::result<process_exit_code> tag_invoke(
		blocking_io_t<Object, wait_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, wait_t> const& a)
		requires requires { Object::wait(h, a); }
	{
		return Object::wait(h, a);
	}
};

} // namespace process_io

struct process_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		wait_on_close,
	);

	struct native_type : base_type::native_type
	{
		process_id parent_id;
		process_id id;

		unix_process_reaper* reaper;
	};

	using open_t = process_io::open_t;
	using launch_t = process_io::launch_t;
	using terminate_t = process_io::terminate_t;
	using wait_t = process_io::wait_t;
	
	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, launch_t
		, terminate_t
		, wait_t
	>;

	static vsm::result<void> open(
		native_type& h,
		io_parameters_t<process_t, open_t> const& args);

	static vsm::result<void> launch(
		native_type& h,
		io_parameters_t<process_t, launch_t> const& args);

	static vsm::result<void> terminate(
		native_type const& h,
		io_parameters_t<process_t, terminate_t> const& args);

	static vsm::result<process_exit_code> wait(
		native_type const& h,
		io_parameters_t<process_t, wait_t> const& args);

	static vsm::result<void> close(
		native_type& h,
		io_parameters_t<process_t, close_t> const& args);

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] process_id get_id() const
		{
			return static_cast<Handle const&>(*this).native().native_type::id;
		}
	};

	template<typename Handle>
	struct concrete_interface : base_type::concrete_interface<Handle>
	{
		[[nodiscard]] auto wait(auto&&... args) const
		{
			return generic_io<wait_t>(
				static_cast<Handle const&>(*this),
				make_io_args<process_t, wait_t>()(vsm_forward(args)...));
		}

		[[nodiscard]] auto terminate(auto&&... args) const
		{
			return generic_io<terminate_t>(
				static_cast<Handle const&>(*this),
				make_io_args<process_t, terminate_t>()(vsm_forward(args)...));
		}
	};
};

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/process.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/process.hpp>
#endif
