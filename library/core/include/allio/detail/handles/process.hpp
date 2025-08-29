#pragma once

#include <allio/any_path_buffer.hpp>
#include <allio/any_string.hpp>
#include <allio/detail/deadline.hpp>
#include <allio/detail/filesystem.hpp>
#include <allio/detail/handles/platform_object.hpp>
#include <allio/detail/integer_id.hpp>
#include <allio/detail/serialization.hpp>

#include <optional>

namespace allio::detail {

struct unix_process_reaper;


struct process_t;

/// Windows:    Valid process IDs have a range of [1, 2^32).
/// Linux:      Valid process IDs have a range of [1, 2^22].
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
inline constexpr explicit_parameter<inherit_handles_t> inherit_handles = {};

//TODO: Implement explicit inherit_handles(a, b, c) or equivalent
#if 0
struct inherit_handles_parameter
{

};
inline constexpr inherit_handles_parameter inherit_handles = {};
#endif

struct wait_on_close_t : explicit_argument<wait_on_close_t, bool> {};
inline constexpr explicit_parameter<wait_on_close_t> wait_on_close = {};

struct process_arguments_t : explicit_argument<process_arguments_t, any_string_span> {};
inline constexpr explicit_parameter<process_arguments_t> process_arguments = {};

struct working_directory_t : explicit_argument<working_directory_t, fs_path> {};
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
	vsm_static_operator redirect_stream_t<Stream>
	operator()(handle_for<platform_object_t> auto const& handle) vsm_static_operator_const
	{
		return { { &handle.native() } };
	}
};

inline constexpr redirect_stream_parameter<0> redirect_stdin = {};
inline constexpr redirect_stream_parameter<1> redirect_stdout = {};
inline constexpr redirect_stream_parameter<2> redirect_stderr = {};

struct with_exit_code_t : explicit_argument<with_exit_code_t, process_exit_code> {};
inline constexpr explicit_parameter<with_exit_code_t> with_exit_code = {};

class process_wait_result
{
	process_exit_code m_exit_code;

public:
	explicit process_wait_result(process_exit_code const exit_code)
		: m_exit_code(exit_code)
	{
	}

	[[nodiscard]] bool has_exit_code() const
	{
		return _get(m_exit_code).has_value();
	}

	[[nodiscard]] process_exit_code get_exit_code() const
	{
		auto const exit_code = _get(m_exit_code);
		vsm_assert(exit_code);
		return *exit_code;
	}

	[[nodiscard]] operator std::optional<process_exit_code>() const
	{
		return _get(m_exit_code);
	}

	[[nodiscard]] std::optional<process_exit_code> operator->() const
	{
		return _get(m_exit_code);
	}

private:
	static std::optional<process_exit_code> _get(process_exit_code exit_code);
};


template<platform_handle Handle>
[[nodiscard]] native_handle<platform_object_t> const& _get_platform_handle(Handle const& h)
{
	return h.native();
}

template<platform_object Object>
[[nodiscard]] native_handle<platform_object_t> const& _get_platform_handle(
	native_handle<Object> const& h)
{
	return h;
}

template<typename T>
concept _get_platform_handle_concept = requires (T const& value)
{
	detail::_get_platform_handle(value);
};


template<platform_handle Handle>
[[nodiscard]] static auto _get_platform_handle_range(Handle const& h)
{
	return std::span<Handle const, 1>(&h, 1);
}

template<platform_object Object>
[[nodiscard]] static auto _get_platform_handle_range(native_handle<Object> const& h)
{
	return std::span<native_handle<Object> const, 1>(&h, 1);
}

template<_get_platform_handle_concept Handle, size_t Size>
[[nodiscard]] static auto _get_platform_handle_range(Handle(&handles)[Size])
{
	return std::span<Handle, Size>(handles);
}

template<std::ranges::forward_range Range>
	requires _get_platform_handle_concept<std::ranges::range_value_t<Range> const&>
[[nodiscard]] static Range const& _get_platform_handle_range(Range const& range)
{
	return range;
}

template<typename T>
using _platform_handle_range_t = decltype(_get_platform_handle_range(std::declval<T const&>()));

template<typename Range>
concept _get_platform_handle_range_concept = requires
{
	typename _platform_handle_range_t<Range>;
};


template<typename Range>
[[nodiscard]] size_t _copy_platform_handles(
	Range const& range,
	native_handle<platform_object_t> const** out)
{
	size_t handle_count = 0;

	if (out != nullptr)
	{
		for (auto const& handle : range)
		{
			*out++ = &detail::_get_platform_handle(handle);
			++handle_count;
		}
	}
	else
	{
		handle_count += std::ranges::distance(range);
	}

	return handle_count;
}

template<typename... Ranges>
class handle_set
{
	std::tuple<Ranges...> m_ranges;

public:
	template<_get_platform_handle_range_concept... Args>
	handle_set(Args&&... ranges)
		: m_ranges(detail::_get_platform_handle_range(vsm_forward(ranges))...)
	{
	}

	[[nodiscard]] size_t copy_to(native_handle<platform_object_t> const** out) const
	{
		size_t handle_count = 0;

		auto const visit_one = [&](auto const& range)
		{
			size_t const range_handle_count = detail::_copy_platform_handles(range, out);

			if (out != nullptr)
			{
				out += range_handle_count;
			}

			handle_count += range_handle_count;
		};

		auto const visit_all = [&](auto const&... ranges)
		{
			(visit_one(ranges), ...);
		};

		std::apply(visit_all, m_ranges);

		return handle_count;
	}
};

template<typename... Ranges>
handle_set(Ranges&&...) -> handle_set<_platform_handle_range_t<Ranges>...>;


class platform_handles_view
{
	using copy_to_t = size_t(void const* context, native_handle<platform_object_t> const** out);

	copy_to_t* m_copy_to;
	void const* m_context;

public:
	platform_handles_view()
		: m_copy_to(_copy_none)
		, m_context(nullptr)
	{
	}

	template<_get_platform_handle_concept Handle>
	platform_handles_view(Handle const& handle)
		: m_copy_to(_copy_single_handle<Handle>)
		, m_context(std::addressof(handle))
	{
	}

	template<_get_platform_handle_range_concept Range>
	platform_handles_view(Range const& range)
		: m_copy_to(_copy_single_range<Range>)
		, m_context(std::addressof(range))
	{
	}

	template<typename... Ranges>
	platform_handles_view(handle_set<Ranges...> const& ranges)
		: m_copy_to(_copy_member_function<handle_set<Ranges...>>)
		, m_context(&ranges)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_context != nullptr;
	}

	[[nodiscard]] size_t copy_to(native_handle<platform_object_t> const** const out) const
	{
		return m_copy_to(m_context, out);
	}

private:
	static size_t _copy_none(
		void const* const context,
		native_handle<platform_object_t> const** const out)
	{
		return 0;
	}

	template<typename Context>
	static size_t _copy_single_handle(
		void const* const context,
		native_handle<platform_object_t> const** const out)
	{
		if (out != nullptr)
		{
			*out = &detail::_get_platform_handle(*static_cast<Context const*>(context));
		}

		return 1;
	}

	template<typename Context>
	static size_t _copy_single_range(
		void const* const context,
		native_handle<platform_object_t> const** out)
	{
		return detail::_copy_platform_handles(
			detail::_get_platform_handle_range(*static_cast<Context const*>(context)),
			out);
	}

	template<typename Context>
	static size_t _copy_member_function(
		void const* const context,
		native_handle<platform_object_t> const** const out)
	{
		return static_cast<Context const*>(context)->copy_to(out);
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

			using io_flags_t::set_argument;

			void set_argument(process_id const value)
			{
				id = value;
			}
		};

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
			platform_handles_view inherit_handles;
			native_handle<platform_object_t> const* redirect_stdin;
			native_handle<platform_object_t> const* redirect_stdout;
			native_handle<platform_object_t> const* redirect_stderr;

			using io_flags_t::set_argument;

			void set_argument(explicit_parameter<inherit_handles_t>)
			{
				options |= process_options::inherit_handles;
			}

			void set_argument(inherit_handles_t const value)
			{
				if (value.value)
				{
					options |= process_options::inherit_handles;
				}
			}

			void set_argument(explicit_parameter<wait_on_close_t>)
			{
				options |= process_options::wait_on_close;
			}

			void set_argument(wait_on_close_t const value)
			{
				if (value.value)
				{
					options |= process_options::wait_on_close;
				}
			}

			void set_argument(process_arguments_t const value)
			{
				arguments = value.value;
			}

			void set_argument(process_environment_t const value)
			{
				options |= process_options::set_environment;
				environment = value.value;
			}

			void set_argument(working_directory_t const value)
			{
				working_directory = value.value;
			}

			void set_argument(redirect_stream_t<0> const value)
			{
				redirect_stdin = value.value;
			}

			void set_argument(redirect_stream_t<1> const value)
			{
				redirect_stdout = value.value;
			}

			void set_argument(redirect_stream_t<2> const value)
			{
				redirect_stderr = value.value;
			}
		};

		using result_type = void;

		template<object Object>
		static vsm::result<void> blocking_io(
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

			void set_argument(with_exit_code_t const value)
			{
				set_exit_code = true;
				exit_code = value.value;
			}
		};

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
		using params_type = deadline_t;
		using result_type = process_wait_result;

		template<object Object>
		static vsm::result<process_wait_result> blocking_io(
			native_handle<Object> const& h,
			io_parameters_t<Object, wait_t> const& a)
			requires requires { Object::wait(h, a); }
		{
			return Object::wait(h, a);
		}
	};

	struct duplicate_handle_t
	{
		using operation_concept = void;

		struct params_type : io_flags_t
		{
			native_platform_handle platform_handle;
		};

		using result_type = native_platform_handle;

		template<object Object>
		static vsm::result<native_platform_handle> blocking_io(
			native_handle<Object> const& h,
			io_parameters_t<Object, duplicate_handle_t> const& a)
			requires requires { Object::duplicate_handle(h, a); }
		{
			return Object::duplicate_handle(h, a);
		}
	};

	template<platform_object TargetObject>
	struct duplicate_handle_template_t
	{
		using operation_concept = void;

		struct params_type : io_flags_t
		{
			any_string_view serialized_handle;
		};

		template<handle Handle>
		using result_type_template = typename Handle::template rebind_object<TargetObject>;

		template<object Object>
		static vsm::result<basic_detached_handle<TargetObject>> blocking_io(
			native_handle<Object> const& h,
			io_parameters_t<Object, duplicate_handle_template_t<TargetObject>> const& a)
			requires requires { Object::template duplicate_handle_template<TargetObject>(h, a); }
		{
			return Object::template duplicate_handle_template<TargetObject>(h, a);
		}
	};

	using operations = type_list_append
	<
		base_type::operations
		, open_t
		, create_t
		, terminate_t
		, wait_t
		, duplicate_handle_t
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

	static vsm::result<process_wait_result> wait(
		native_handle<process_t> const& h,
		io_parameters_t<process_t, wait_t> const& args);

	static vsm::result<native_platform_handle> duplicate_handle(
		native_handle<process_t> const& h,
		io_parameters_t<process_t, duplicate_handle_t> const& args);

	template<typename TargetObject>
	static vsm::result<basic_detached_handle<TargetObject>> duplicate_handle_template(
		native_handle<process_t> const& h,
		io_parameters_t<process_t, duplicate_handle_template_t<TargetObject>> const& args);

	static vsm::result<void> close(
		native_handle<process_t>& h,
		io_parameters_t<process_t, close_t> const& args);


	using is_serializable = process_t;

	static vsm::result<void> serializer_visit(
		native_handle<process_t>& h,
		serialization_context& serializer);

	static vsm::result<void> serialize(
		native_handle<process_t>& h,
		serialization_context& serializer);


	template<typename Handle, typename Traits>
	struct facade : base_type::facade<Handle, Traits>
	{
		[[nodiscard]] process_id get_id() const
		{
			return static_cast<Handle const&>(*this).native().id;
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

		template<typename TargetHandle>
		[[nodiscard]] auto duplicate_handle(
			any_string_view const serialized_handle,
			auto&&... args) const
		{
			using object_type = typename TargetHandle::object_type;

			auto a = io_parameters_t<process_t, duplicate_handle_template_t<object_type>>{};
			a.serialized_handle = serialized_handle;
			(set_argument(a, vsm_forward(args)), ...);

			return Traits::template observe<duplicate_handle_template_t<object_type>>(
				static_cast<Handle const&>(*this),
				a);
		}
	};
};

template<>
struct native_handle<process_t> : native_handle<process_t::base_type>
{
	process_id id;
	unix_process_reaper* reaper;
};


template<typename TargetObject>
vsm::result<basic_detached_handle<TargetObject>> process_t::duplicate_handle_template(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, process_t::duplicate_handle_template_t<TargetObject>> const& a)
{
	native_handle<TargetObject> new_h;
	vsm_try_void(detail::decode_handle<TargetObject>(new_h, a.serialized_handle));

	vsm_try_assign(new_h.platform_handle, blocking_io<process_t::duplicate_handle_t>(
		h,
		io_parameters_t<process_t, process_t::duplicate_handle_t>
		{
			io_flags_t{ a.flags },
			new_h.platform_handle,
		}));

	verify_handle(new_h);

	return vsm::result<basic_detached_handle<TargetObject>>(vsm::result_value, adopt_handle, new_h);
}


//TODO: Prefix with underscores to avoid accidental ADL-matching.
//      Do the same elsewhere.
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


vsm::result<size_t> get_current_executable_path(any_path_buffer buffer);

template<typename Path>
vsm::result<Path> get_current_executable_path()
{
	vsm::result<Path> r(vsm::result_value);
	if (auto const r2 = detail::get_current_executable_path(*r); !r2)
	{
		r = vsm::unexpected(r2.error());
	}
	return r;
}


template<typename Traits>
[[nodiscard]] auto _get_current_executable_path(any_path_buffer const buffer)
{
	auto r = detail::get_current_executable_path(buffer);

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}

template<typename Path, typename Traits>
[[nodiscard]] auto _get_current_executable_path()
{
	auto r = detail::get_current_executable_path<Path>();

	if constexpr (Traits::has_transform_result)
	{
		return Traits::transform_result(vsm_move(r));
	}
	else
	{
		return r;
	}
}


namespace _this_process {

[[nodiscard]] process_id get_id() noexcept;

} // namespace _this_process
} // namespace allio::detail

#if vsm_os_win32
#	include <allio/win32/detail/iocp/process.hpp>
#endif

#if vsm_os_linux
#	include <allio/linux/detail/io_uring/process.hpp>
#endif
