#pragma once

#include <allio/handles/filesystem_handle2.hpp>

namespace allio {

using process_id = int32_t;
using process_exit_code = int32_t;

namespace detail {

class process_handle_impl : public platform_handle
{
protected:
	using base_type = platform_handle;

private:
	vsm::linear<process_id> m_id;

public:
	allio_handle_flags
	(
		current,
	);


	struct native_handle_type : base_type::native_handle_type
	{
		process_id id;
	};

	[[nodiscard]] native_handle_type get_native_handle() const
	{
		return
		{
			base_type::get_native_handle(),
			m_id.value,
			m_exit_code.value,
		};
	}


	[[nodiscard]] process_id get_process_id() const
	{
		vsm_assert(*this); //PRECONDITION
		return m_id.value;
	}

	[[nodiscard]] bool is_current() const
	{
		vsm_assert(*this); //PRECONDITION
		return get_flags()[flags::current];
	}


	struct open_tag;
	struct open_parameters = no_parameters;

	struct launch_tag;
	#define allio_process_handle_launch_parameters(type, data, ...) \
		type(allio::process_handle, launch_parameters) \
		allio_platform_handle_create_parameters(__VA_ARGS__, __VA_ARGS__) \
		data(::std::span<::allio::input_string_view const>, arguments) \
		data(::std::span<::allio::input_string_view const>, environment) \
		data(::std::optional<::allio::input_path>, working_directory) \

	allio_interface_parameters(allio_process_handle_launch_parameters);


	struct wait_tag;
	using wait_parameters = deadline_parameters;


	struct duplicate_handle_tag;
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
			wait_tag
		>
	>;

protected:
	allio_detail_default_lifetime(process_handle);


	[[nodiscard]] static bool check_native_handle(native_handle_type const& native)
	{
		return base_type::check_native_handle(native) && check_native_handle_impl(native);
	}

	[[nodiscard]] static bool check_native_handle_impl(native_handle_type const& native);

	void set_native_handle(native_handle_type const& native)
	{
		base_type::set_native_handle(native);
		m_id.value = native.id;
	}


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

	template<typename H>
	struct async_interface : base_type::async_interface<H>
	{
		template<parameters<wait_parameters> P = wait_parameters::interface>
		basic_sender<wait_tag> wait_async(P const& args = {}) const;	
	};
};

} // namespace detail

using process_handle = basic_handle<detail::process_handle_impl>;

template<typename Multiplexer>
using basic_process_handle = basic_async_handle<detail::process_handle_impl, Multiplexer>;


template<parameters<process_handle::open_parameters> P = process_handle::open_parameters::interface>
vsm::result<process_handle> open_process(process_id const id, P const& args = {})
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->open(id, args));
	return r;
}

template<parameters<process_handle::launch_parameters> P = process_handle::launch_parameters::interface>
vsm::result<process_handle> launch_process(path_descriptor const path, P const& args = {})
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(r->launch(path, args));
	return r;
}


namespace this_process {

process_handle const& get_handle();

} // namespace this_process


template<>
struct io_operation_traits<process_handle::open_tag>
{
	using handle_type = process_handle;
	using result_type = void;

	process_id id;
};

template<>
struct io_operation_traits<process_handle::launch_tag>
{
	using handle_type = process_handle;
	using result_type = void;

	path_descriptor path;
};

template<>
struct io_operation_traits<process_handle::wait_tag>
{
	using handle_type = process_handle const;
	using result_type = process_exit_code;
};

} // namespace allio
