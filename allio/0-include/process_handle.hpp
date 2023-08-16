#pragma once

#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>

#include <vsm/flags.hpp>
#include <vsm/linear.hpp>

#include <optional>
#include <span>
#include <string_view>

namespace allio {

namespace io {

struct process_open;
struct process_launch;
struct process_wait;
struct process_duplicate_handle;

} // namespace io


using process_id = int32_t;
using process_exit_code = int32_t;

namespace detail {

class process_handle_base : public platform_handle
{
	using final_handle_type = final_handle<process_handle_base>;

	vsm::linear<process_exit_code> m_exit_code;
	vsm::linear<process_id> m_pid;

public:
	struct implementation;

	using base_type = platform_handle;

	struct native_handle_type : base_type::native_handle_type
	{
		process_id pid;
		process_exit_code exit_code;
	};

	allio_handle_flags
	(
		current,
		exited,
	);
	
	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::process_open,
			io::process_launch,
			io::process_wait
		>
	>;


	using open_parameters = create_parameters;
	using wait_parameters = basic_parameters;

	#define allio_process_handle_launch_parameters(type, data, ...) \
		type(allio::process_handle, launch_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::std::span<::allio::input_string_view const>, arguments) \
		data(::std::span<::allio::input_string_view const>, environment) \
		data(::std::optional<::allio::input_path>, working_directory) \

	allio_interface_parameters(allio_process_handle_launch_parameters);


	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_pid.value,
			m_exit_code.value,
		};
	}


	[[nodiscard]] process_id get_process_id() const
	{
		vsm_assert(*this);
		return m_pid.value;
	}

	[[nodiscard]] bool is_current() const
	{
		vsm_assert(*this);
		return get_flags()[flags::current];
	}


	template<parameters<open_parameters> Parameters = open_parameters::interface>
	vsm::result<void> open(process_id const pid, Parameters const& args = {})
	{
		return block_open(pid, args);
	}

	template<parameters<launch_parameters> Parameters = launch_parameters::interface>
	vsm::result<void> launch(input_path_view const path, Parameters const& args = {})
	{
		return block_launch(nullptr, path, args);
	}

	template<parameters<launch_parameters> Parameters = launch_parameters::interface>
	vsm::result<void> launch(filesystem_handle const& base, input_path_view const path, Parameters const& args = {})
	{
		return block_launch(&base, path, args);
	}

	template<parameters<wait_parameters> Parameters = wait_parameters::interface>
	vsm::result<process_exit_code> wait(Parameters const& args = {})
	{
		return block_wait(args);
	}

#if 0
	template<std::derived_from<platform_handle> Handle>
	vsm::result<Handle> duplicate_handle(typename Handle::native_handle_type handle) const
	{
		platform_handle::native_handle_type const& platform_handle = handle;
		vsm_try(new_handle, duplicate_handle_internal(platform_handle.handle));

		vsm::result<Handle> r(vsm::result_value);
		vsm_verify(r->set_native_handle(new_handle));
		return r;
	}
#endif


	static final_handle_type current();

protected:
	using base_type::base_type;

	//vsm::result<void> close_sync(basic_parameters const& args);


	static bool check_native_handle(native_handle_type const& handle);
	void set_native_handle(native_handle_type const& handle);

	native_handle_type release_native_handle();

private:
	vsm::result<void> block_open(process_id pid, open_parameters const& args);
	vsm::result<void> block_launch(filesystem_handle const* base, input_path_view path, launch_parameters const& args);
	vsm::result<process_exit_code> block_wait(wait_parameters const& args);

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::process_open> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::process_launch> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::process_wait> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::close> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
	
	//TODO: Use a CPO to enable duplication of non-platform handles like TLS socket handles?
	vsm::result<native_platform_handle> duplicate_handle(native_platform_handle handle) const;

#if 0
	vsm::result<void> duplicate_handle_internal(native_platform_handle handle,
		function_view<vsm::result<void>(native_platform_handle handle)> consume);
#endif
};

} // namespace detail

using process_handle = final_handle<detail::process_handle_base>;


vsm::result<process_handle> open_process(process_id pid, process_handle::open_parameters::interface const& args = {});
vsm::result<process_handle> launch_process(input_path_view path, process_handle::launch_parameters::interface const& args = {});


template<>
struct io::parameters<io::process_open>
	: process_handle::open_parameters
{
	using handle_type = process_handle;
	using result_type = void;

	process_id pid;
};

template<>
struct io::parameters<io::process_launch>
	: process_handle::launch_parameters
{
	using handle_type = process_handle;
	using result_type = void;

	filesystem_handle const* base;
	input_path_view path;
};

template<>
struct io::parameters<io::process_wait>
	: process_handle::wait_parameters
{
	using handle_type = process_handle;
	using result_type = process_exit_code;
};

allio_detail_api extern allio_handle_implementation(process_handle);

} // namespace allio
