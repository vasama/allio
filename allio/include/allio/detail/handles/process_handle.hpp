#pragma once

#include <allio/any_string.hpp>
#include <allio/detail/handles/filesystem_handle.hpp>
#include <allio/detail/io_sender.hpp>

#include <vsm/standard.hpp>

namespace allio::detail {

/// Windows: Process IDs on Windows may be as high as 2^32 - 1.
/// Linux: Valid process IDs have a range of [1, 2^22].
using process_id = uint32_t;

using process_exit_code = int32_t;

inline constexpr process_exit_code process_exit_code_unavailable = -1;


struct command_line_t
{
	any_string_span command_line;

	struct tag_t
	{
		struct argument_t
		{
			any_string_span value;

			friend void tag_invoke(set_argument_t, command_line_t& args, argument_t const& value)
			{
				args.command_line = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(any_string_span const command_line)
		{
			return { command_line };
		}
	};
};
inline constexpr command_line_t::tag_t command_line = {};

struct environment_t
{
	any_string_span environment;

	struct tag_t
	{
		struct argument_t
		{
			any_string_span value;

			friend void tag_invoke(set_argument_t, environment_t& args, argument_t const& value)
			{
				args.environment = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(any_string_span const environment)
		{
			return { environment };
		}
	};
};
inline constexpr environment_t::tag_t environment = {};

struct working_directory_t
{
	std::optional<path_descriptor> working_directory;

	struct tag_t
	{
		struct argument_t
		{
			path_descriptor value;

			friend void tag_invoke(set_argument_t, working_directory_t& args, argument_t const& value)
			{
				args.working_directory = value.value;
			}
		};

		argument_t vsm_static_operator_invoke(path_descriptor const working_directory)
		{
			return { working_directory };
		}
	};
};
inline constexpr working_directory_t::tag_t working_directory = {};


class _process_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	struct impl_type;

	allio_handle_flags
	(
		/// @brief This handle refers to the current process.
		current,
	);


	[[nodiscard]] bool is_current() const
	{
		return get_flags()[flags::current];
	}

	[[nodiscard]] vsm::result<process_id> get_process_id() const;


	struct open_t
	{
		using handle_type = _process_handle;
		using result_type = void;

		struct required_params_type
		{
			detail::process_id process_id;
		};

		using optional_params_type = no_parameters_t;
	};

	struct launch_t
	{
		using handle_type = _process_handle;
		using result_type = void;

		struct required_params_type
		{
			path_descriptor path;
		};

		using optional_params_type = parameters_t<
			command_line_t,
			environment_t,
			working_directory_t>;
	};


	struct wait_t
	{
		using handle_type = _process_handle const;
		using result_type = process_exit_code;
		using required_params_type = no_parameters_t;
		using optional_params_type = deadline_t;
	};

	vsm::result<process_exit_code> wait(auto&&... args) const
	{
		vsm::result<process_exit_code> r(vsm::result_value);
		vsm_try_void(do_blocking_io(*this, r, io_arguments_t<wait_t>()(vsm_forward(args)...)));
		return r;
	}


#if 0
	struct duplicate_handle_t;
	struct duplicate_handle_parameters = no_parameters;

	template<typename Handle, parameters<duplicate_handle_parameters> P = duplicate_handle_parameters::interface>
	vsm::result<void> duplicate_handle(Handle& handle, typename Handle::native_handle_type const& native) const;

	template<typename Handle, parameters<duplicate_handle_parameters> P = duplicate_handle_parameters::interface>
	vsm::result<Handle> duplicate_handle(typename Handle::native_handle_type const& native) const;
#endif


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			wait_t
		>
	>;

protected:
	allio_detail_default_lifetime(_process_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
	};

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		io_sender<H, wait_t> wait_async(auto&&... args) const;
	};

protected:
	static vsm::result<void> do_blocking_io(
		_process_handle& h,
		io_result_ref_t<open_t> result,
		io_parameters_t<open_t> const& args);

	static vsm::result<void> do_blocking_io(
		_process_handle& h,
		io_result_ref_t<launch_t> result,
		io_parameters_t<launch_t> const& args);

	static vsm::result<void> do_blocking_io(
		_process_handle const& h,
		io_result_ref_t<wait_t> result,
		io_parameters_t<wait_t> const& args);
};

using blocking_process_handle = basic_blocking_handle<_process_handle>;

template<typename Multiplexer>
using basic_process_handle = basic_async_handle<_process_handle, Multiplexer>;


vsm::result<blocking_process_handle> open_process(process_id const process_id, auto&&... args)
{
	vsm::result<blocking_process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_process_handle::open_t>(process_id)(vsm_forward(args)...)));
	return r;
}

template<typename Multiplexer>
vsm::result<blocking_process_handle> open_process(Multiplexer&& multiplexer, process_id const process_id, auto&&... args)
{
	vsm::result<basic_process_handle<Multiplexer>> r(vsm::result_value);
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_process_handle::open_t>(process_id)(vsm_forward(args)...)));
	return r;
}


vsm::result<blocking_process_handle> launch_process(path_descriptor const path, auto&&... args)
{
	vsm::result<blocking_process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_process_handle::launch_t>(path)(vsm_forward(args)...)));
	return r;
}

template<typename Multiplexer>
vsm::result<blocking_process_handle> launch_process(Multiplexer&& multiplexer, path_descriptor const path, auto&&... args)
{
	vsm::result<basic_process_handle<Multiplexer>> r(vsm::result_value);
	vsm_try_void(blocking_io(*r, no_result, io_arguments_t<_process_handle::launch_t>(path)(vsm_forward(args)...)));
	return r;
}

namespace _this_process {

blocking_process_handle const& get_handle();
vsm::result<blocking_process_handle> open();

} // namespace _this_process
} // namespace allio::detail
