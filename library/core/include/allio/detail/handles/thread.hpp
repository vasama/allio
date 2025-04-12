#pragma once

#error This header is not yet migrated to the new core/defaults split

#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/integer_id.hpp>

#include <thread>

namespace allio::detail {

struct thread_t;

using thread_id = integer_id<thread_t, uint32_t>;

//TODO: Use the same type for process and thread.
using thread_exit_code = int32_t;

namespace thread_io {

} // namespace thread_io

//TODO: std::thread adoption
struct thread_t : platform_object_t
{
	using base_type = platform_object_t;

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
		static vsm::result<void> blocking_io(
			native_handle<Object>& h,
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
		static vsm::result<void> blocking_io(
			native_handle<Object> const& h,
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
		static vsm::result<void> blocking_io(
			native_handle<Object> const& h,
			io_parameters_t<Object, wait_t> const& a)
			requires requires { Object::wait(h, a); }
		{
			return Object::wait(h, a);
		}
	};

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


	using is_serializable = thread_t;

	static vsm::result<void> serializer_visit(
		native_handle<thread_t>& h,
		serialization_context& serializer);

	static vsm::result<void> serialize(
		native_handle<thread_t>& h,
		serialization_context& serializer);


	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] thread_id get_id() const
		{
			return static_cast<Handle const&>(*this).native().id;
		}

		[[nodiscard]] auto wait(auto&&... args) const
		{
			auto a = io_parameters_t<thread_t, wait_t>{};
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<wait_t>(static_cast<Handle const&>(*this), a);
		}
	};
};

template<>
struct native_handle<thread_t> : native_handle<thread_t::base_type>
{
	thread_id id;
};


[[nodiscard]] vsm::result<thread_handle> open_thread(thread_id const id, auto&&... args)
{
	vsm::result<process_handle> r(vsm::result_value);
	vsm_try_void(blocking_io<thread_t, thread_io::open_t>(
		*r,
		make_io_args<thread_t, thread_io::open_t>(id)(vsm_forward(args)...)));
	return r;
}

template<vsm::any_of<Thread, std::thread, std::jthread> Thread>
[[nodiscard]] vsm::result<thread_handle> open_thread(Thread auto&& thread, auto&&... args);

} // namespace allio::detail
