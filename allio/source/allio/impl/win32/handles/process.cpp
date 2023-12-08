#include <allio/handles/process.hpp>
#include <allio/impl/win32/handles/process.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/bounded_vector.hpp>
#include <allio/impl/transcode.hpp>
#include <allio/impl/win32/command_line.hpp>
#include <allio/impl/win32/environment_block.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/path.hpp>
#include <allio/win32/kernel_error.hpp>
#include <allio/win32/platform.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
#include <vsm/utility.hpp>

#include <bit>
#include <optional>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

struct attribute_list_deleter
{
	void vsm_static_operator_invoke(LPPROC_THREAD_ATTRIBUTE_LIST const list)
	{
		DeleteProcThreadAttributeList(list);
	}
};
using unique_attribute_list = std::unique_ptr<_PROC_THREAD_ATTRIBUTE_LIST, attribute_list_deleter>;

template<size_t Capacity>
using attribute_storage = basic_dynamic_buffer<std::byte, alignof(std::max_align_t), Capacity>;

} // namespace

template<size_t Capacity>
static vsm::result<unique_attribute_list> create_attribute_list(
	attribute_storage<Capacity>& storage,
	size_t const size)
{
	auto const attribute_count = vsm::saturate<DWORD>(size);

	SIZE_T buffer_size = 0;
	(void)InitializeProcThreadAttributeList(
		/* lpAttributeList: */ nullptr,
		attribute_count,
		/* dwFlags: */ 0,
		&buffer_size);

	vsm_try(byte_buffer, storage.reserve(buffer_size));
	auto const buffer = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(byte_buffer);

	if (!InitializeProcThreadAttributeList(
		buffer,
		attribute_count,
		/* dwFlags: */ 0,
		&buffer_size))
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_attribute_list(buffer));
}

static vsm::result<void> update_attribute_list(
	PPROC_THREAD_ATTRIBUTE_LIST const list,
	DWORD_PTR const attribute,
	void const* const value,
	size_t const value_size)
{
	if (!UpdateProcThreadAttribute(
		list,
		/* dwFlags: */ 0,
		attribute,
		const_cast<void*>(value),
		value_size,
		/* lpPreviousValue: */ nullptr,
		/* lpReturnSize: */ 0))
	{
		return vsm::unexpected(get_last_error());
	}
	return {};
}


vsm::result<unique_handle> win32::open_process(DWORD const id)
{
	HANDLE const handle = OpenProcess(
		SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
		/* bInheritHandle: */ false,
		id);

	if (!handle)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm_lazy(unique_handle(handle));
}

vsm::result<process_info> win32::launch_process(
	io_parameters_t<process_t, process_t::launch_t> const& a)
{
	if (a.path.base != native_platform_handle::null)
	{
		return vsm::unexpected(error::unsupported_operation);
	}

	if (a.working_directory)
	{
		if (a.working_directory->base != native_platform_handle::null)
		{
			return vsm::unexpected(error::unsupported_operation);
		}
	}

	api_string_storage string_storage(4);
	//TODO: Use a smarter path specific function over make_api_string.
	vsm_try(path, make_api_string(string_storage, a.path.path.string()));
	vsm_try(command_line, make_command_line(string_storage, a.path.path.string(), a.command_line));

	wchar_t* environment = nullptr;
	if (a.environment)
	{
		vsm_try_assign(environment, make_environment_block(string_storage, *a.environment));
	}

	wchar_t const* working_directory = nullptr;
	if (a.working_directory)
	{
		vsm_try_assign(working_directory, make_api_string(
			string_storage,
			a.working_directory->path.string()));
	}

	SECURITY_ATTRIBUTES process_attributes = {};
	process_attributes.nLength = sizeof(process_attributes);

	SECURITY_ATTRIBUTES thread_attributes = {};
	thread_attributes.nLength = sizeof(thread_attributes);

	STARTUPINFOEXW startup_info = {};
	startup_info.StartupInfo.cb = sizeof(startup_info);

	bool inherit_handles = a.inherit_handles_count != 0;

	size_t attribute_count = 0;
	size_t inherit_handles_count = 0;

	bounded_vector<std::pair<void const*, size_t>, 2> inherit_handles_sets;

	auto const push_inherit_handles_set = [&](void const* const data, size_t const size)
	{
		attribute_count += inherit_handles_count == 0;
		vsm_verify(inherit_handles_sets.try_push_back({ data, size }));
		inherit_handles_count += size;
	};

	if (a.inherit_handles_array != nullptr)
	{
		push_inherit_handles_set(a.inherit_handles_array, a.inherit_handles_count);
	}

	// Standard stream handle redirection:
	{
		bool redirect_standard_streams = false;

		if (a.std_input != native_platform_handle::null)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdInput = unwrap_handle(a.std_input);
		}
		if (a.std_output != native_platform_handle::null)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdOutput = unwrap_handle(a.std_output);
		}
		if (a.std_error != native_platform_handle::null)
		{
			redirect_standard_streams = true;
			startup_info.StartupInfo.hStdError = unwrap_handle(a.std_error);
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

			// If the user did not request inheritance,
			// the inheritance is restricted to just the standard stream handles.
			push_inherit_handles_set(&startup_info.StartupInfo.hStdInput, 3);
		}
	}

	attribute_storage<256> attribute_storage;
	unique_attribute_list attribute_list;

	if (attribute_count != 0)
	{
		vsm_try_assign(attribute_list, create_attribute_list(
			attribute_storage,
			attribute_count));
	}

	dynamic_buffer<HANDLE, 8> inherit_handles_storage;

	if (inherit_handles_sets.size() != 0)
	{
		void const* inherit_handles_array;

		if (inherit_handles_sets.size() == 1)
		{
			inherit_handles_array = inherit_handles_sets.front().first;
			vsm_assert(inherit_handles_count == inherit_handles_sets.front().second);
		}
		else
		{
			vsm_try(array, inherit_handles_storage.reserve(inherit_handles_count));
			size_t index = 0;

			for (auto const& set : inherit_handles_sets)
			{
				vsm_assert(index <= inherit_handles_count);
				memcpy(array + index, set.first, set.second * sizeof(HANDLE));
				index += set.second;
			}

			vsm_assert(index == inherit_handles_count);
			inherit_handles_array = array;
		}

		vsm_try_void(update_attribute_list(
			attribute_list.get(),
			PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
			inherit_handles_array,
			inherit_handles_count * sizeof(HANDLE)));
	}

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
		return vsm::unexpected(get_last_error());
	}

	unique_handle process(process_information.hProcess);
	unique_handle thread(process_information.hThread);

	return vsm_lazy(process_info
	{
		.handle = vsm_move(process),
		.id = process_information.dwProcessId,
	});
}

vsm::result<process_exit_code> win32::get_process_exit_code(HANDLE const handle)
{
	DWORD exit_code;
	if (!GetExitCodeProcess(handle, &exit_code))
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<process_exit_code>(exit_code);
}


vsm::result<void> process_t::open(
	native_type& h,
	io_parameters_t<process_t, open_t> const& a)
{
	vsm_try(handle, win32::open_process(std::bit_cast<DWORD>(a.id)));

	handle_flags flags = flags::none;

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null | flags,
			},
			wrap_handle(handle.release()),
		},
		process_id{},
		a.id,
	};

	return {};
}

vsm::result<void> process_t::launch(
	native_type& h,
	io_parameters_t<process_t, launch_t> const& a)
{
	vsm_try_bind((handle, id), win32::launch_process(a));

	h = native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null,
			},
			wrap_handle(handle.release()),
		},
		process_id{},
		std::bit_cast<process_id>(id),
	};

	return {};
}

vsm::result<void> process_t::terminate(
	native_type const& h,
	io_parameters_t<process_t, terminate_t> const& a)
{
	HANDLE const handle = unwrap_handle(h.platform_handle);

	NTSTATUS status = NtTerminateProcess(
		handle,
		//TODO: Does this require some kind of transformation?
		static_cast<NTSTATUS>(a.exit_code));

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
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return {};
}

vsm::result<process_exit_code> process_t::wait(
	native_type const& h,
	io_parameters_t<process_t, wait_t> const& a)
{
	if (std::bit_cast<DWORD>(h.id) == GetCurrentProcessId())
	{
		return vsm::unexpected(error::process_is_current_process);
	}

	HANDLE const handle = unwrap_handle(h.platform_handle);

	NTSTATUS const status = win32::NtWaitForSingleObject(
		handle,
		/* Alertable: */ false,
		kernel_timeout(a.deadline));

	if (status == STATUS_TIMEOUT)
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	if (!NT_SUCCESS(status))
	{
		return vsm::unexpected(static_cast<kernel_error>(status));
	}

	return get_process_exit_code(handle);
}


blocking::process_handle const& this_process::get_handle()
{
	using flags = process_t::flags;
	using i_flags = process_t::impl_type::flags;

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
						handle_flags(flags::not_null) | i_flags::pseudo_handle,
					},
					wrap_handle(GetCurrentProcess()),
				},
				process_id{},
				std::bit_cast<process_id>(GetCurrentProcessId()),
			}
		);
	};

	static auto const handle = make_handle();
	return handle;
}

vsm::result<blocking::process_handle> this_process::open()
{
	using flags = process_t::flags;

	vsm_try(handle, duplicate_handle(GetCurrentProcess()));

	return vsm_lazy(blocking::process_handle(
		adopt_handle,
		process_t::native_type
		{
			platform_object_t::native_type
			{
				object_t::native_type
				{
					flags::not_null,
				},
				wrap_handle(handle.release()),
			},
			process_id{},
			std::bit_cast<process_id>(GetCurrentProcessId()),
		}
	));
}
