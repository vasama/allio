#include <allio/detail/handles/process.hpp>
#include <allio/impl/win32/handles/process.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/error_encoding.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/command_line.hpp>
#include <allio/impl/win32/environment_block.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/path.hpp>
#include <allio/win32/kernel_error.hpp>

#include <vsm/intrusive/forward_list.hpp>
#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
#include <vsm/utility.hpp>

#include <bit>
#include <memory>
#include <optional>

#include <Windows.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

struct attribute_list_deleter
{
	vsm_static_operator void operator()(LPPROC_THREAD_ATTRIBUTE_LIST const list) vsm_static_operator_const
	{
		DeleteProcThreadAttributeList(list);
	}
};
using unique_attribute_list = std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, attribute_list_deleter>;

template<size_t Capacity>
using attribute_list_storage = basic_dynamic_buffer<std::byte, alignof(std::max_align_t), Capacity>;

class attribute_list_builder
{
public:
	class attribute : public vsm::intrusive::forward_list_link
	{
		DWORD_PTR m_attribute;
		void const* m_value;
		size_t m_value_size;

	public:
		attribute() = default;

		explicit attribute(
			attribute_list_builder& builder,
			DWORD_PTR const attribute,
			void const* const value,
			size_t const value_size)
			: m_attribute(attribute)
			, m_value(value)
			, m_value_size(value_size)
		{
			builder.m_list.push_back(*this);
			++builder.m_list_size;
		}

		attribute(attribute const&) = delete;
		attribute& operator=(attribute const&) = delete;

	private:
		friend attribute_list_builder;
	};

private:
	vsm::intrusive::forward_list<attribute> m_list;
	size_t m_list_size = 0;

public:
	template<size_t Capacity>
	[[nodiscard]] vsm::result<unique_attribute_list> build(
		attribute_list_storage<Capacity>& storage) const
	{
		vsm::result<unique_attribute_list> r(vsm::result_value);
		if (auto const attribute_count = vsm::truncate<DWORD>(m_list_size))
		{
			SIZE_T buffer_size = 0;
			(void)InitializeProcThreadAttributeList(
				/* lpAttributeList: */ nullptr,
				attribute_count,
				/* dwFlags: */ 0,
				&buffer_size);

			vsm_try(byte_buffer, storage.reserve(buffer_size));
			auto const list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(byte_buffer);

			if (!InitializeProcThreadAttributeList(
				list,
				attribute_count,
				/* dwFlags: */ 0,
				&buffer_size))
			{
				return vsm::unexpected(allio_error(get_last_error()));
			}

			r.emplace(list);

			for (attribute const& a : m_list)
			{
				if (!UpdateProcThreadAttribute(
					list,
					/* dwFlags: */ 0,
					a.m_attribute,
					const_cast<void*>(a.m_value),
					a.m_value_size,
					/* lpPreviousValue: */ nullptr,
					/* lpReturnSize: */ nullptr))
				{
					return vsm::unexpected(allio_error(get_last_error()));
				}
			}
		}
		return r;
	}
};
using attribute = attribute_list_builder::attribute;


template<size_t Capacity>
using inherit_handles_storage = basic_dynamic_buffer<
	std::byte,
	alignof(HANDLE),
	Capacity * sizeof(HANDLE)>;

template<size_t Capacity>
static vsm::result<attribute> make_inherit_handles_attribute(
	attribute_list_builder& builder,
	basic_dynamic_buffer<std::byte, alignof(HANDLE), Capacity>& storage,
	std::span<HANDLE const> const internal_handles,
	platform_handles_view const user_handles)
{
	using native_handle_type = native_handle<platform_object_t>;
	using pointer_type = native_handle_type const*;

	union user_handle_union
	{
		pointer_type pointer;
		HANDLE handle;
	};

	HANDLE const* handle_array = internal_handles.data();
	size_t handle_count = internal_handles.size();

	if (size_t const user_handle_count = user_handles.copy_to(nullptr))
	{
		size_t const new_handle_count = handle_count + user_handle_count;
		vsm_try(buffer, storage.reserve(new_handle_count * sizeof(HANDLE)));

		vsm_verify(user_handle_count == user_handles.copy_to(
			vsm::start_lifetime_as_array<pointer_type>(buffer, user_handle_count)));

		auto const user_handle_array = vsm::start_lifetime_as_array<user_handle_union>(
			buffer,
			user_handle_count);

		for (auto& u : std::span(user_handle_array, user_handle_count))
		{
			u.handle = unwrap_handle(u.pointer->platform_handle);
		}

		auto const new_handle_array = vsm::start_lifetime_as_array<HANDLE>(
			buffer,
			new_handle_count);

		std::memcpy(
			new_handle_array + user_handle_count,
			handle_array,
			handle_count * sizeof(HANDLE));

		handle_array = new_handle_array;
		handle_count = new_handle_count;
	}

	if (handle_count != 0)
	{
		return vsm::result<attribute>(
			vsm::result_value,
			builder,
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			handle_array,
			handle_count * sizeof(HANDLE));
	}

	return {};
}


class small_wide_path_container
{
	size_t m_size = MAX_PATH;

	union
	{
		wchar_t m_data[MAX_PATH];
		wchar_t* m_data_ptr;
	};

public:
	using value_type = wchar_t;

	small_wide_path_container() = default;

	small_wide_path_container(small_wide_path_container const&) = delete;
	small_wide_path_container& operator=(small_wide_path_container const&) = delete;

	~small_wide_path_container()
	{
		if (m_size > MAX_PATH)
		{
			::delete[] m_data_ptr;
		}
	}

	[[nodiscard]] wchar_t* begin()
	{
		return m_size <= MAX_PATH ? m_data : m_data_ptr;
	}

	[[nodiscard]] wchar_t* end()
	{
		return begin() + m_size;
	}

private:
	[[nodiscard]] friend vsm::result<size_t> tag_invoke(
		resize_container_t,
		small_wide_path_container& self,
		size_t const min_size,
		size_t const max_size)
	{
		static constexpr size_t max_path_size = 0x7FFF;

		if (min_size > max_size)
		{
			if (self.m_size > MAX_PATH)
			{
				return vsm::unexpected(allio_error(error::invariant_violation));
			}

			wchar_t* const ptr = ::new (std::nothrow) wchar_t[max_path_size];

			if (ptr == nullptr)
			{
				return vsm::unexpected(allio_error(error::not_enough_memory));
			}

			self.m_data_ptr = ptr;
			self.m_size = max_path_size;
		}

		return self.m_size;
	}
};

} // namespace


vsm::result<unique_handle> win32::open_process(DWORD const id)
{
	HANDLE const handle = OpenProcess(
		SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
		/* bInheritHandle: */ false,
		id);

	if (!handle)
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	return vsm_lazy(unique_handle(handle));
}

vsm::result<process_exit_code> win32::get_process_exit_code(HANDLE const handle)
{
	DWORD exit_code;
	if (!GetExitCodeProcess(handle, &exit_code))
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}
	return static_cast<process_exit_code>(exit_code);
}


std::optional<process_exit_code> process_wait_result::_get(process_exit_code const exit_code)
{
	return exit_code;
}


vsm::result<void> process_t::open(
	native_handle<process_t>& h,
	io_parameters_t<process_t, open_t> const& a)
{
	vsm_try(handle, win32::open_process(a.id.integer()));

	h.flags = flags::not_null;
	h.platform_handle = wrap_handle(handle.release());
	h.id = a.id;
	h.reaper = nullptr;

	return {};
}

vsm::result<void> process_t::create(
	native_handle<process_t>& h,
	io_parameters_t<process_t, create_t> const& a)
{
	if (a.executable_path.base != nullptr)
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	api_string_storage string_storage(4);

	//TODO: Use a smarter path specific function over make_api_string.
	vsm_try(path, make_api_string(
		string_storage,
		a.executable_path.path.string()));

	vsm_try(command_line, make_command_line(
		string_storage,
		a.executable_path.path.string(),
		a.arguments));

	wchar_t* environment = nullptr;
	if (vsm::any_flags(a.options, process_options::set_environment))
	{
		vsm_try_assign(environment, make_environment_block(
			string_storage,
			a.environment));
	}

	//TODO: Only error if the path is relative.
	if (a.working_directory.base != nullptr)
	{
		return vsm::unexpected(allio_error(error::unsupported_operation));
	}

	wchar_t const* working_directory = nullptr;
	if (!a.working_directory.path.empty())
	{
		vsm_try_assign(working_directory, make_api_string(
			string_storage,
			a.working_directory.path.string()));
	}

	SECURITY_ATTRIBUTES process_attributes = {};
	process_attributes.nLength = sizeof(process_attributes);

	SECURITY_ATTRIBUTES thread_attributes = {};
	thread_attributes.nLength = sizeof(thread_attributes);

	STARTUPINFOEXW startup_info = {};
	startup_info.StartupInfo.cb = sizeof(startup_info);

	attribute_list_builder attributes;

	bool inherit_handles = vsm::any_flags(a.options, process_options::inherit_handles);
	std::span<HANDLE const> inherit_handles_internal;

	// Standard stream handle redirection:
	{
		bool redirect_standard_streams = false;

		if (a.redirect_stdin != nullptr)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdInput = unwrap_handle(a.redirect_stdin->platform_handle);
		}
		if (a.redirect_stdout != nullptr)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdOutput = unwrap_handle(a.redirect_stdout->platform_handle);
		}
		if (a.redirect_stderr != nullptr)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdError = unwrap_handle(a.redirect_stderr->platform_handle);
		}

		if (redirect_standard_streams)
		{
			startup_info.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;

			if (startup_info.StartupInfo.hStdInput == NULL)
			{
				startup_info.StartupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			}
			if (startup_info.StartupInfo.hStdOutput == NULL)
			{
				startup_info.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			}
			if (startup_info.StartupInfo.hStdError == NULL)
			{
				startup_info.StartupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
			}

			// Redirecting standard streams requires inheritance of handles.
			inherit_handles = true;

			// If the user did not request inheritance, the inheritance is restricted to just the
			// standard stream handles.
			inherit_handles_internal = std::span(&startup_info.StartupInfo.hStdInput, 3);
		}
	}

	inherit_handles_storage<8> inherit_handles_storage;
	vsm_try(inherit_handles_attribute, make_inherit_handles_attribute(
		attributes,
		inherit_handles_storage,
		inherit_handles_internal,
		a.inherit_handles));

	// The attribute is automatically used as part of the attribute list, but must be kept alive
	// until the full attribute list is built.
	(void)inherit_handles_attribute;

	attribute_list_storage<256> attribute_list_storage;
	vsm_try(attribute_list, attributes.build(attribute_list_storage));
	startup_info.lpAttributeList = attribute_list.get();

	PROCESS_INFORMATION process_information;
	if (!CreateProcessW(
		path,
		command_line,
		&process_attributes,
		&thread_attributes,
		inherit_handles,
		CREATE_UNICODE_ENVIRONMENT | EXTENDED_STARTUPINFO_PRESENT,
		environment,
		working_directory,
		&startup_info.StartupInfo,
		&process_information))
	{
		return vsm::unexpected(allio_error(get_last_error()));
	}

	unique_handle process(process_information.hProcess);
	unique_handle thread(process_information.hThread);

	handle_flags flags = handle_flags::none;

	if (vsm::any_flags(a.options, process_options::wait_on_close))
	{
		flags |= flags::wait_on_close;
	}

	h = native_handle<process_t>
	{
		native_handle<platform_object_t>
		{
			native_handle<object_t>
			{
				flags::not_null | flags,
			},
			wrap_handle(process.release()),
		},
		process_id(process_information.dwProcessId),
	};

	return {};
}

vsm::result<void> process_t::terminate(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, terminate_t> const& a)
{
	HANDLE const handle = unwrap_handle(h.platform_handle);

	process_exit_code const exit_code = a.set_exit_code
		? a.exit_code
		: EXIT_FAILURE;

	NTSTATUS status = NtTerminateProcess(
		handle,
		//TODO: Does this require some kind of transformation?
		static_cast<NTSTATUS>(exit_code));

	switch (status)
	{
	case STATUS_ACCESS_DENIED:
		//TODO: Check if this is really necessary.
		//      It seems the documentation might have been incorrect.
		if (get_handle_access(handle).value_or(0) & PROCESS_TERMINATE)
		{
			// Windows returns STATUS_ACCESS_DENIED when attempting to
			// terminate an already terminated process. Check that the
			// handle was actually granted the terminate access right.
			status = STATUS_SUCCESS;
		}
		break;

	case STATUS_PROCESS_IS_TERMINATING:
		{
			status = STATUS_SUCCESS;
		}
		break;
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return {};
}

vsm::result<process_wait_result> process_t::wait(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, wait_t> const& a)
{
	if (h.id.integer() == GetCurrentProcessId())
	{
		return vsm::unexpected(allio_error(error::process_is_current_process));
	}

	HANDLE const handle = unwrap_handle(h.platform_handle);

	NTSTATUS const status = win32::NtWaitForSingleObject(
		handle,
		/* Alertable: */ false,
		kernel_timeout(a.deadline));

	if (status == STATUS_TIMEOUT)
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	vsm_try(exit_code, get_process_exit_code(handle));

	return process_wait_result(exit_code);
}

vsm::result<native_platform_handle> process_t::duplicate_handle(
	native_handle<process_t> const& h,
	io_parameters_t<process_t, duplicate_handle_t> const& a)
{
	//TODO: Handle create_synchronized, create_non_blocking somehow?

	ULONG handle_attributes = 0;

	if (vsm::any_flags(a.flags, io_flags::create_inheritable))
	{
		handle_attributes |= OBJ_INHERIT;
	}

	HANDLE new_handle;
	NTSTATUS const status = win32::NtDuplicateObject(
		/* SourceProcessHandle: */ unwrap_handle(h.platform_handle),
		/* SourceHandle: */ unwrap_handle(a.platform_handle),
		/* TargetProcessHandle: */ GetCurrentProcess(),
		/* TargetHandle: */ &new_handle,
		/* DesiredAccess: */ 0,
		/* HandleAttributes: */ handle_attributes,
		/* Options: */ DUPLICATE_SAME_ACCESS);

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
	}

	return wrap_handle(new_handle);
}

vsm::result<void> process_t::close(
	native_handle<process_t>& h,
	io_parameters_t<process_t, close_t> const& a)
{
	vsm_assert(h.reaper == nullptr);

	if (h.flags[process_t::flags::wait_on_close])
	{
		if (h.id.integer() == GetCurrentProcessId())
		{
			return vsm::unexpected(allio_error(error::process_is_current_process));
		}

		NTSTATUS const status = win32::NtWaitForSingleObject(
			unwrap_handle(h.platform_handle),
			/* Alertable: */ false,
			/* Timeout: */ nullptr);

		if (!NT_SUCCESS(status))
		{
			return vsm::unexpected(allio_error(static_cast<kernel_error>(status)));
		}
	}

	return base_type::close(h, a);
}


static vsm::result<size_t> get_current_executable_path(string_buffer<wchar_t> const buffer)
{
	size_t min_string_size = 2;

	while (true)
	{
		vsm_try(path_string, buffer.resize(min_string_size, static_cast<DWORD>(-1)));

		DWORD const path_size = GetModuleFileNameW(
			/* hModule: */ NULL,
			path_string.data(),
			vsm::truncating(path_string.size()));

		if (path_size == 0)
		{
			return vsm::unexpected(allio_error(get_last_error()));
		}

		if (path_size < path_string.size())
		{
			vsm_try_discard(buffer.resize(path_size));
			return path_size;
		}

		min_string_size = path_string.size() + 1;
	}
}

template<typename Char>
static vsm::result<size_t> get_current_executable_path(string_buffer<Char> const buffer)
{
	small_wide_path_container container;
	vsm_try(size, ::get_current_executable_path(container));
	return copy_or_transcode_string(std::wstring_view(container.begin(), size), buffer);
}

vsm::result<size_t> detail::get_current_executable_path(any_path_buffer const buffer)
{
	return detail::visit_as<char8_t, wchar_t, char32_t>(buffer.string(), [](auto const buffer)
	{
		return ::get_current_executable_path(buffer);
	});
}


process_id _this_process::get_id() noexcept
{
	return process_id(GetCurrentProcessId());
}

#if 0
basic_detached_handle<process_t> detail::_get_current_process_pseudo_handle()
{
	return basic_detached_handle<process_t>(
		adopt_handle,
		native_handle<process_t>
		{
			native_handle<platform_object_t>
			{
				native_handle<object_t>
				{
					handle_flags(object_t::flags::not_null)
						| process_t::impl_type::flags::pseudo_handle,
				},
				wrap_handle(GetCurrentProcess()),
			},
			process_id(GetCurrentProcessId()),
		}
	);
}
#endif

#if 0 //TODO: this_process
blocking::process_handle const& this_process::get_handle()
{
	static constexpr auto make_handle = []()
	{
		return blocking::process_handle(
			adopt_handle,
			process_t::native_type
			{
				platform_object_t::native_type
				{
					object_t::native_type
					{
						handle_flags(object_t::flags::not_null)
							| process_t::impl_type::flags::pseudo_handle,
					},
					wrap_handle(GetCurrentProcess()),
				},
				process_id{},
				process_id(GetCurrentProcessId()),
			}
		);
	};

	static auto const handle = make_handle();
	return handle;
}

vsm::result<blocking::process_handle> this_process::open()
{
	vsm_try(handle, duplicate_handle(GetCurrentProcess()));

	return vsm_lazy(blocking::process_handle(
		adopt_handle,
		process_t::native_type
		{
			platform_object_t::native_type
			{
				object_t::native_type
				{
					object_t::flags::not_null,
				},
				wrap_handle(handle.release()),
			},
			process_id{},
			process_id(GetCurrentProcessId()),
		}
	));
}
#endif
