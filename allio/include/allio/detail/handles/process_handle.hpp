#pragma once

#include <allio/detail/handles/filesystem_handle.hpp>

namespace allio {
namespace detail {

using process_id = int32_t;
using process_exit_code = int32_t;

class _process_handle : public platform_handle
{
protected:
	using base_type = platform_handle;

public:
	allio_handle_flags
	(
		current,
	);


	[[nodiscard]] vsm::result<process_id> get_process_id() const;


	struct open_t;
	struct open_parameters = no_parameters;

	struct launch_t;
	#define allio_process_handle_launch_parameters(type, data, ...) \
		type(allio::process_handle, launch_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::std::span<::allio::input_string_view const>, arguments) \
		data(::std::span<::allio::input_string_view const>, environment) \
		data(::std::optional<::allio::input_path>, working_directory) \

	allio_interface_parameters(allio_process_handle_launch_parameters);


	struct wait_t;
	using wait_parameters = deadline_parameters;


	struct duplicate_handle_t;
	struct duplicate_handle_parameters = no_parameters;

	template<typename Handle, parameters<duplicate_handle_parameters> P = duplicate_handle_parameters::interface>
	vsm::result<void> duplicate_handle(Handle& handle, typename Handle::native_handle_type const& native) const;

	template<typename Handle, parameters<duplicate_handle_parameters> P = duplicate_handle_parameters::interface>
	vsm::result<Handle> duplicate_handle(typename Handle::native_handle_type const& native) const;


	using async_operations = type_list_cat
	<
		base_type::async_operations,
		type_list
		<
			wait_t
		>
	>;

protected:
	allio_detail_default_lifetime(process_handle);

	template<typename H>
	struct sync_interface : base_type::sync_interface<H>
	{
		template<parameters<open_parameters> P = open_parameters::interface>
		vsm::result<void> open(process_id const id, P const& args = {}) &;
		
		template<parameters<open_parameters> P = open_parameters::interface>
		vsm::result<void> launch(path_descriptor const path, P const& args = {}) &;

		template<parameters<wait_parameters> P = wait_parameters::interface>
		vsm::result<process_exit_code> wait(P const& args = {}) const;
	};

	template<typename M, typename H>
	struct async_interface : base_type::async_interface<M, H>
	{
		template<parameters<wait_parameters> P = wait_parameters::interface>
		basic_sender<M, H, wait_t> wait_async(P const& args = {}) const;	
	};
};

} // namespace detail

template<>
struct io_operation_traits<process_handle::open_t>
{
	using handle_type = process_handle;
	using result_type = void;

	process_id id;
};

template<>
struct io_operation_traits<process_handle::launch_t>
{
	using handle_type = process_handle;
	using result_type = void;

	path_descriptor path;
};

template<>
struct io_operation_traits<process_handle::wait_t>
{
	using handle_type = process_handle const;
	using result_type = process_exit_code;
};

} // namespace allio
