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


struct inherit_handles_t
{
	native_platform_handle const* inherit_handles_array = nullptr;
	size_t inherit_handles_count = 0;
};
class explicit_inherit_handles
{
	struct argument
	{
		native_platform_handle const* array;
		size_t count;
	};

	friend void tag_invoke(set_argument_t, inherit_handles_t& args, explicit_inherit_handles)
	{
		args.inherit_handles_array = nullptr;
		args.inherit_handles_count = static_cast<size_t>(-1);
	}

	friend void tag_invoke(set_argument_t, inherit_handles_t& args, argument const& value)
	{
		args.inherit_handles_array = value.array;
		args.inherit_handles_count = value.count;
	}

public:
	argument vsm_static_operator_invoke(bool const inherit_handles)
	{
		return { nullptr, inherit_handles ? static_cast<size_t>(-1) : 0 };
	}

	argument vsm_static_operator_invoke(std::span<native_platform_handle const> const inherit_handles)
	{
		return { inherit_handles.data(), inherit_handles.size() };
	}
};
inline constexpr explicit_inherit_handles inherit_handles = {};

struct command_line_t
{
	any_string_span command_line;
};
inline constexpr explicit_parameter<command_line_t> command_line = {};

struct environment_t
{
	std::optional<any_string_span> environment;
};
inline constexpr explicit_parameter<environment_t> environment = {};

struct working_directory_t
{
	std::optional<fs_path> working_directory;
};
inline constexpr explicit_parameter<working_directory_t> working_directory = {};

template<typename T>
concept byte_stream_handle = true;

template<typename P>
class stream_parameter
{
	struct argument
	{
		native_platform_handle value;

		friend void tag_invoke(
			set_argument_t,
			P& arguments,
			argument const& argument)
		{
			parameter_value(arguments) = argument.value;
		}
	};

public:
	constexpr argument vsm_static_operator_invoke(byte_stream_handle auto const& value)
	{
		return { value.native().platform_handle };
	}
};

struct std_input_t
{
	native_platform_handle std_input = native_platform_handle::null;
};
inline constexpr stream_parameter<std_input_t> std_input = {};

struct std_output_t
{
	native_platform_handle std_output = native_platform_handle::null;
};
inline constexpr stream_parameter<std_output_t> std_output = {};

struct std_error_t
{
	native_platform_handle std_error = native_platform_handle::null;
};
inline constexpr stream_parameter<std_error_t> std_error = {};

struct launch_detached_t
{
	bool launch_detached = false;

	friend void tag_invoke(set_argument_t, launch_detached_t& args, explicit_parameter<launch_detached_t>)
	{
		args.launch_detached = true;
	}
};
inline constexpr explicit_parameter<launch_detached_t> launch_detached = {};

struct wait_on_close_t
{
	bool wait_on_close = false;

	friend void tag_invoke(set_argument_t, wait_on_close_t& args, explicit_parameter<wait_on_close_t>)
	{
		args.wait_on_close = true;
	}
};
inline constexpr explicit_parameter<wait_on_close_t> wait_on_close = {};

struct exit_code_t
{
	std::optional<process_exit_code> exit_code;
};
inline constexpr explicit_parameter<exit_code_t, process_exit_code> exit_code = {};

namespace process_io {

struct open_t
{
	using operation_concept = producer_t;
	struct required_params_type
	{
		process_id id;
	};
	using optional_params_type = inheritable_t;
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
	struct required_params_type
	{
		fs_path path;
	};
	using optional_params_type = parameters_t
	<
		launch_detached_t,
		wait_on_close_t,

		inheritable_t,
		inherit_handles_t,

		command_line_t,
		environment_t,
		working_directory_t,

		std_input_t,
		std_output_t,
		std_error_t
	>;
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
	using required_params_type = no_parameters_t;
	using optional_params_type = exit_code_t;
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
	using required_params_type = no_parameters_t;
	using optional_params_type = deadline_t;
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

	template<typename Handle, optional_multiplexer_handle_for<process_t> MultiplexerHandle>
	struct concrete_interface : base_type::concrete_interface<Handle, MultiplexerHandle>
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
using abstract_process_handle = abstract_handle<process_t>;


namespace _process_b {

using process_handle = blocking_handle<process_t>;


vsm::result<process_handle> open_process(process_id const id, auto&&... args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<process_t, process_io::open_t>(
		*r,
		make_io_args<process_t, process_io::open_t>(id)(vsm_forward(args)...)));
	return r;
}

vsm::result<process_handle> launch_process(fs_path const& executable, auto&&... args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<process_t, process_io::launch_t>(
		*r,
		make_io_args<process_t, process_io::launch_t>(executable)(vsm_forward(args)...)));
	return r;
}

} // namespace _process_b

namespace _process_a {

template<multiplexer_handle_for<process_t> MultiplexerHandle>
using basic_process_handle = async_handle<process_t, MultiplexerHandle>;


ex::sender auto open_process(process_id const id, auto&&... args)
{
	return io_handle_sender<process_t, process_io::open_t>(
		make_io_args<process_t, process_io::open_t>(id)(vsm_forward(args)...));
}

ex::sender auto launch_process(fs_path const& path, auto&&... args)
{
	return io_handle_sender<process_t, process_io::launch_t>(
		make_io_args<process_t, process_io::launch_t>(path)(vsm_forward(args)...));
}

} // namespace _process_a

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/process.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/process.hpp>
#endif
