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
vsm_flag_enum(process_options);

struct inherit_handles_t : explicit_argument<inherit_handles_t, bool> {};

struct inherit_handles_parameter
{

};
inline constexpr inherit_handles_parameter inherit_handles = {};

struct wait_on_close_t : explicit_argument<wait_on_close_t, bool> {};
inline constexpr explicit_parameter<wait_on_close_t> wait_on_close = {};

struct process_arguments_t : explicit_argument<process_arguments_t, any_string_span> {};
inline constexpr explicit_parameter<process_arguments_t> process_arguments = {};

struct working_directory_t : explicit_argument<working_directory_t, fs_path const&> {};
inline constexpr explicit_parameter<working_directory_t> working_directory = {};

struct process_environment_t : explicit_argument<process_environment_t, any_string_span> {};
inline constexpr explicit_parameter<process_environment_t> process_environment = {};

template<int Stream>
struct redirect_stream_t : explicit_argument<
	redirect_stream_t<Stream>,
	native_handle<platform_object_t> const*> {};

template<int Stream>
struct redirect_stream_parameter
{
	redirect_stream_t<Stream> vsm_static_operator_invoke(handle_for<platform_object_t> auto const& handle)
	{
		return { { &handle.native() } };
	}
};

inline constexpr redirect_stream_parameter<0> redirect_stdin = {};
inline constexpr redirect_stream_parameter<1> redirect_stdout = {};
inline constexpr redirect_stream_parameter<2> redirect_stderr = {};

struct with_exit_code_t : explicit_argument<with_exit_code_t, process_exit_code> {};
inline constexpr explicit_parameter<with_exit_code_t> with_exit_code = {};

template<object BaseObject>
class any_handle_span
{
	using native_handle_type = native_handle<BaseObject>;

	using copy_type = size_t(
		void const* data,
		size_t data_size,
		size_t data_offset,
		native_handle_type const** buffer,
		size_t buffer_size);

	void const* m_data;
	size_t m_size;
	copy_type* m_copy;

public:
	any_handle_span()
		: m_data(nullptr)
		, m_size(0)
		, m_copy(nullptr)
	{
	}

	template<handle_for<BaseObject> Handle>
	explicit any_handle_span(Handle* const data, size_t const size)
		: m_data(data)
		, m_size(size)
		, m_copy(_copy<Handle>)
	{
	}

	template<std::ranges::contiguous_range Range>
		requires handle_for<std::ranges::range_value_t<Range>, BaseObject>
	any_handle_span(Range&& range)
		: any_handle_span(std::ranges::data(range), std::ranges::size(range))
	{
	}

	[[nodiscard]] bool empty() const
	{
		return m_size == 0;
	}

	[[nodiscard]] size_t size() const
	{
		return m_size;
	}

	native_handle_type const** copy(
		size_t const offset,
		size_t const count,
		native_handle_type const** const buffer) const
	{
		return buffer + m_copy(m_data, m_size, offset, buffer, count);
	}

private:
	template<typename T>
	static size_t _copy(
		void const* const data,
		size_t const data_size,
		size_t const data_offset,
		native_handle_type** const buffer,
		size_t const buffer_size)
	{
		vsm_assert(data_offset <= data_size);

		size_t const size = std::min(
			buffer_size,
			data_size - data_offset);

		for (size_t i = 0; i < size; ++i)
		{
			T const& object = data[data_offset + i];

			if constexpr (std::is_convertible_v<T const&, native_handle_type const&>)
			{
				buffer[i] = &static_cast<native_handle_type const&>(object);
			}
			else
			{
				buffer[i] = &static_cast<native_handle_type const&>(object.native());
			}
		}

		return size;
	}
};

struct process_t : platform_object_t
{
	using base_type = platform_object_t;

	allio_handle_flags
	(
		wait_on_close,
	);

	struct open_t
	{
		using operation_concept = producer_t;

		struct params_type : io_flags_t
		{
			process_id id;

			friend void tag_invoke(set_argument_t, params_type& args, process_id const value)
			{
				args.id = value;
			}
		};

		using result_type = void;
		using runtime_concept = bounded_runtime_t;

		template<object Object>
		friend vsm::result<void> tag_invoke(
			blocking_io_t<open_t>,
			native_handle<Object>& h,
			io_parameters_t<Object, open_t> const& a)
			requires requires { Object::open(h, a); }
		{
			return Object::open(h, a);
		}
	};

	struct create_t
	{
		using operation_concept = producer_t;

		struct params_type : io_flags_t
		{
			process_options options;
			fs_path executable_path;
			any_string_span arguments;
			any_string_span environment;
			fs_path working_directory;
			any_handle_span<platform_object_t> inherit_handles;
			native_handle<platform_object_t> const* redirect_stdin;
			native_handle<platform_object_t> const* redirect_stdout;
			native_handle<platform_object_t> const* redirect_stderr;

			friend void tag_invoke(set_argument_t, params_type& args, inherit_handles_t const value)
			{
				if (value.value)
				{
					args.options |= process_options::inherit_handles;
				}
			}

			friend void tag_invoke(set_argument_t, params_type& args, explicit_parameter<inherit_handles_t>)
			{
				args.options |= process_options::inherit_handles;
			}

			friend void tag_invoke(set_argument_t, params_type& args, wait_on_close_t const value)
			{
				if (value.value)
				{
					args.options |= process_options::wait_on_close;
				}
			}

			friend void tag_invoke(set_argument_t, params_type& args, explicit_parameter<wait_on_close_t>)
			{
				args.options |= process_options::wait_on_close;
			}

			friend void tag_invoke(set_argument_t, params_type& args, process_arguments_t const value)
			{
				args.arguments = value.value;
			}

			friend void tag_invoke(set_argument_t, params_type& args, process_environment_t const value)
			{
				args.options |= process_options::set_environment;
				args.environment = value.value;
			}

			friend void tag_invoke(set_argument_t, params_type& args, working_directory_t const value)
			{
				args.working_directory = value.value;
			}

			friend void tag_invoke(set_argument_t, params_type& args, redirect_stream_t<0> const value)
			{
				args.redirect_stdin = value.value;
			}

			friend void tag_invoke(set_argument_t, params_type& args, redirect_stream_t<1> const value)
			{
				args.redirect_stdout = value.value;
			}

			friend void tag_invoke(set_argument_t, params_type& args, redirect_stream_t<2> const value)
			{
				args.redirect_stderr = value.value;
			}
		};

		using result_type = void;

		template<object Object>
		friend vsm::result<void> tag_invoke(
			blocking_io_t<create_t>,
			native_handle<Object>& h,
			io_parameters_t<Object, create_t> const& a)
			requires requires { Object::create(h, a); }
		{
			return Object::create(h, a);
		}
	};

	struct terminate_t
	{
		using operation_concept = void;

		struct params_type
		{
			bool set_exit_code;
			process_exit_code exit_code;

			friend void tag_invoke(set_argument_t, params_type& args, with_exit_code_t const value)
			{
				args.set_exit_code = true;
				args.exit_code = value.value;
			}
		};

		using result_type = void;
		using runtime_concept = bounded_runtime_t;

		template<object Object>
		friend vsm::result<void> tag_invoke(
			blocking_io_t<terminate_t>,
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
		using params_type = deadline_t;
		using result_type = process_exit_code;

		template<object Object>
		friend vsm::result<process_exit_code> tag_invoke(
			blocking_io_t<wait_t>,
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
		, create_t
		, terminate_t
		, wait_t
	>;

	static vsm::result<void> open(
		native_handle<process_t>& h,
		io_parameters_t<process_t, open_t> const& args);

	static vsm::result<void> create(
		native_handle<process_t>& h,
		io_parameters_t<process_t, create_t> const& args);

	static vsm::result<void> terminate(
		native_handle<process_t> const& h,
		io_parameters_t<process_t, terminate_t> const& args);

	static vsm::result<process_exit_code> wait(
		native_handle<process_t> const& h,
		io_parameters_t<process_t, wait_t> const& args);

	static vsm::result<void> close(
		native_handle<process_t>& h,
		io_parameters_t<process_t, close_t> const& args);

	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] process_id get_id() const
		{
			using native_handle_type = native_handle<process_t>;
			return static_cast<Handle const&>(*this).native().native_handle_type::id;
		}

		[[nodiscard]] auto wait(auto&&... args) const
		{
			auto a = io_parameters_t<process_t, wait_t>{};
			(set_argument(a, vsm_forward(args)), ...);
			return Traits::template observe<wait_t>(static_cast<Handle const&>(*this), a);
		}

		[[nodiscard]] auto terminate(auto&&... args) const
		{
			auto a = io_parameters_t<process_t, terminate_t>{};
			(set_argument(a, vsm_forward(args)), ...);
			auto r = blocking_io<terminate_t>(static_cast<Handle const&>(*this), a);

			if constexpr (Traits::has_transform_result)
			{
				return Traits::transform_result(vsm_move(r));
			}
			else
			{
				return r;
			}
		}
	};
};

template<>
struct native_handle<process_t> : native_handle<process_t::base_type>
{
	process_id id;
	unix_process_reaper* reaper;
};


template<typename Traits>
[[nodiscard]] auto open_process(process_id const id, auto&&... args)
{
	auto a = io_parameters_t<process_t, process_t::open_t>{};
	a.id = id;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<process_t, process_t::open_t>(a);
}

template<typename Traits>
[[nodiscard]] auto create_process(fs_path const& path, auto&&... args)
{
	auto a = io_parameters_t<process_t, process_t::create_t>{};
	a.executable_path = path;
	(set_argument(a, vsm_forward(args)), ...);
	return Traits::template produce<process_t, process_t::create_t>(a);
}

} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/process.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/process.hpp>
#endif
