#pragma once

#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/io_sender.hpp>

#include <thread>

namespace allio::detail {

class thread_id
{
	uint32_t m_id;
};

//TODO: Use the same type for process and thread.
using thread_exit_code = int32_t;

namespace thread_io {

struct open_t
{
	using operation_concept = producer_t;
	struct required_params_type
	{
		thread_id id;
	};
	using optional_params_type = no_parameters_t;
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

struct terminate_t
{
	using operation_concept = void;
	struct required_params_type
	{
		thread_exit_code exit_code;
	};
	using optional_params_type = no_parameters_t;
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
	using result_type = thread_exit_code;

	template<object Object>
	friend vsm::result<void> tag_invoke(
		blocking_io_t<Object, wait_t>,
		typename Object::native_type const& h,
		io_parameters_t<Object, wait_t> const& a)
		requires requires { Object::wait(h, a); }
	{
		return Object::wait(h, a);
	}
};

} // namespace thread_io

//TODO: std::thread adoption
struct thread_t : platform_object_t
{
	using base_type = platform_object_t;

	using open_t = thread_io::open_t;
	using terminate_t = thread_io::terminate_t;
	using wait_t = thread_io::wait_t;

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, terminate_t
		, wait_t
	>;
	
	static thread_id get_id(native_type const& h);

	static vsm::result<void> open(
		native_type& h,
		io_parameters_t<thread_t, open_t> const& args);

	static vsm::result<void> terminate(
		native_type const& h,
		io_parameters_t<thread_t, terminate_t> const& args);

	static vsm::result<thread_exit_code> wait(
		native_type const& h,
		io_parameters_t<thread_t, wait_t> const& args);

	template<typename Handle>
	struct abstract_interface : base_type::abstract_interface<Handle>
	{
		[[nodiscard]] thread_id get_id() const;
	};
	
	template<typename Handle>
	struct concrete_interface : base_type::concrete_interface<Handle>
	{
		[[nodiscard]] auto wait(auto&&... args) const
		{
			return generic_io<wait_t>(
				static_cast<Handle const&>(*this),
				make_io_args<thread_t, wait_t>()(vsm_forward(args)...));
		}
	};
};
using abstract_thread_handle = abstract_handle<thread_t>;

template<typename Thread>
concept _std_thread = vsm::any_of<Thread, std::thread, std::jthread>;

namespace _thread_b {

using thread_handle = blocking_handle<thread_t>;

[[nodiscard]] vsm::result<thread_handle> open_thread(thread_id const id, auto&&... args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<thread_t, thread_io::open_t>(
		*r,
		make_io_args<thread_t, thread_io::open_t>(id)(vsm_forward(args)...)));
	return r;
}

[[nodiscard]] vsm::result<thread_handle> open_thread(_std_thread auto&& thread, auto&&... args);

} // namespace _thread_b

namespace _thread_a {

template<multiplexer_handle_for<thread_t> MultiplexerHandle>
using basic_thread_handle = async_handle<thread_t, MultiplexerHandle>;

[[nodiscard]] ex::sender auto open_thread(thread_id const id, auto&&... args)
{
	return io_handle_sender<thread_t, thread_io::open_t>(
		make_io_args<thread_t, thread_io::open_t>(id)(vsm_forward(args)...));
}

[[nodiscard]] ex::sender auto open_thread(_std_thread auto&& thread, auto&&... args);

} // namespace _thread_a

} // namespace allio::detail
