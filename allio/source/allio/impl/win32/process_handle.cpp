#include <allio/impl/win32/process_handle.hpp>

#include <allio/detail/dynamic_buffer.hpp>
#include <allio/impl/win32/encoding.hpp>
#include <allio/impl/win32/error.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/kernel_path.hpp>
#include <allio/win32/nt_error.hpp>

#include <vsm/lazy.hpp>
#include <vsm/utility.hpp>

#include <algorithm>
#include <memory>

#include <win32.h>

using namespace allio;
using namespace allio::detail;
using namespace allio::win32;

namespace {

class command_line_builder;

class command_line_storage
{
	static constexpr size_t static_buffer_size = 508;

	std::unique_ptr<wchar_t[]> m_dynamic;
	wchar_t m_static[static_buffer_size];

public:
	command_line_storage() = default;

	command_line_storage(command_line_storage const&) = delete;
	command_line_storage& operator=(command_line_storage const&) = delete;

private:
	friend class command_line_builder;
};

class command_line_builder
{
	static constexpr size_t max_command_line_size = 0x7fff;

	command_line_storage& m_storage;

	wchar_t* m_buffer_pos;
	wchar_t* m_buffer_end;

public:
	static vsm::result<wchar_t*> make_command_line(command_line_storage& buffer,
		input_string_view const program, std::span<input_string_view const> const arguments)
	{
		return command_line_builder(buffer).make_command_line(program, arguments);
	}

private:
	explicit command_line_builder(command_line_storage& buffer)
		: m_storage(buffer)
	{
		m_buffer_pos = buffer.m_static;
		m_buffer_end = buffer.m_static + command_line_storage::static_buffer_size;
	}


	vsm::result<wchar_t*> resize_buffer(size_t const new_data_size)
	{
		if (m_storage.m_dynamic != nullptr)
		{
			return vsm::unexpected(error::process_arguments_too_long);
		}

		wchar_t* const new_buffer_beg = new (std::nothrow) wchar_t[max_command_line_size];

		if (new_buffer_beg == nullptr)
		{
			return vsm::unexpected(error::not_enough_memory);
		}

		m_storage.m_dynamic.reset(new_buffer_beg);

		size_t const old_data_size = m_buffer_pos - m_storage.m_static;
		memcpy(new_buffer_beg, m_storage.m_static, old_data_size * sizeof(wchar_t));

		wchar_t* const new_data_pos = new_buffer_beg + old_data_size;

		m_buffer_pos = new_data_pos + new_data_size;
		m_buffer_end = new_buffer_beg + max_command_line_size;

		return new_data_pos;
	}

	vsm::result<wchar_t*> reserve(size_t const size)
	{
		wchar_t* const pos = m_buffer_pos;
		if (size > static_cast<size_t>(m_buffer_end - pos))
		{
			return resize_buffer(size);
		}
		m_buffer_pos = pos + size;
		return pos;
	}


	struct argument_info
	{
		size_t extra_size;
		bool requires_quotes;
		bool requires_escaping;
	};

	static argument_info classify_argument(wchar_t const* beg, wchar_t const* const end)
	{
		if (beg == end)
		{
			return
			{
				.extra_size = 2,
				.requires_quotes = true,
			};
		}

		argument_info info = {};

		while (beg != end)
		{
			switch (*beg++)
			{
			case L'\\':
			case L'"':
				++info.extra_size;
				break;

			case L' ':
				info.requires_quotes = true;
				break;
			}
		}

		if (info.extra_size > 0)
		{
			info.requires_escaping = true;
		}

		if (info.requires_quotes)
		{
			info.extra_size += 2;
		}

		return info;
	}

	vsm::result<void> push_argument(std::wstring_view const argument)
	{
		wchar_t const* beg = argument.data();
		wchar_t const* const end = beg + argument.size();

		argument_info const info = classify_argument(beg, end);
		size_t const required_size = (end - beg) + info.extra_size + 1;

		vsm_try(buffer_beg, reserve(required_size));
		wchar_t* const buffer_end = buffer_beg + required_size;

		if (info.requires_quotes)
		{
			*buffer_beg++ = L'"';
		}

		if (info.requires_escaping)
		{
			while (beg != end)
			{
				wchar_t const character = *beg++;
				switch (character)
				{
				case L'\\':
				case L'"':
					*buffer_beg++ = L'\\';
				}
				*buffer_beg++ = character;
			}
		}
		else
		{
			buffer_beg = std::copy(beg, end, buffer_beg);
		}

		if (info.requires_quotes)
		{
			*buffer_beg++ = L'"';
		}

		*buffer_beg++ = L' ';

		vsm_assert(buffer_beg == buffer_end);

		return {};
	}

	vsm::result<void> push_argument(std::string_view const argument)
	{
		wchar_t* wide_beg = m_buffer_pos;
		vsm_try(wide_size, [&]() -> vsm::result<size_t>
		{
			while (true)
			{
				auto r = utf8_to_wide(argument, std::span(wide_beg, m_buffer_end));

				// Handle the no buffer space error by allocating and looping.
				if (r || r.error().default_error_condition() != make_error_code(std::errc::no_buffer_space))
				{
					return r;
				}

				vsm_try_assign(wide_beg, resize_buffer(0));
			}
		}());
		wchar_t* wide_end = wide_beg + wide_size;

		argument_info const info = classify_argument(wide_beg, wide_end);

		if (info.extra_size != 0)
		{
			if (info.extra_size > m_buffer_end - wide_end)
			{
				vsm_try_assign(wide_beg, resize_buffer(0));
				wide_end = wide_beg + wide_size;
			}

			wchar_t* shift_end = wide_end + info.extra_size;

			if (info.requires_quotes)
			{
				*--shift_end = L'"';
			}

			if (info.requires_escaping)
			{
				while (wide_beg != wide_end)
				{
					wchar_t const character = *--wide_end;
					*--shift_end = character;
					switch (character)
					{
					case L'\\':
					case L'"':
						*--shift_end = L'\\';
					}
				}
			}
			else
			{
				vsm_assert(info.extra_size == 2 && info.requires_quotes);
				shift_end = std::shift_right(wide_beg, shift_end, 1);
			}

			if (info.requires_quotes)
			{
				*--shift_end = L'"';
			}

			vsm_assert(shift_end == m_buffer_pos);

			wide_end += info.extra_size;
		}

		*wide_end++ = L' ';

		m_buffer_pos = wide_end;

		return {};
	}

	vsm::result<void> push_argument(input_string_view const argument)
	{
		vsm_assert(!argument.is_too_long());
		return argument.is_wide()
			? push_argument(argument.wide_view())
			: push_argument(argument.utf8_view());
	}

	vsm::result<wchar_t*> make_command_line(input_string_view const program, std::span<input_string_view const> const arguments)
	{
		vsm_try_void(push_argument(program));
		for (input_string_view const argument : arguments)
		{
			if (argument.is_too_long())
			{
				return vsm::unexpected(error::invalid_argument);
			}

			vsm_try_void(push_argument(argument));
		}

		wchar_t* const buffer_beg =
			m_storage.m_dynamic != nullptr
				? m_storage.m_dynamic.get()
				: m_storage.m_static;

		vsm_assert(m_buffer_pos != buffer_beg);
		m_buffer_pos[-1] = L'\0';

		return buffer_beg;
	}
};

static vsm::result<wchar_t*> make_command_line(command_line_storage& buffer,
	input_string_view const program, std::span<input_string_view const> const arguments)
{
	return command_line_builder::make_command_line(buffer, program, arguments);
}

} // namespace

#if 0
template<typename Char>
static size_t count_escape_chars(std::basic_string_view<Char> const string)
{
	Char const* beg = string.data();
	Char const* const end = beg + string.size();

	size_t escape_count = 0;

	while (beg != end)
	{
		switch (*beg++)
		{
		case Char('\\'):
		case Char('\"'):
			++escape_count;
		}
	}

	return escape_count;
}

static size_t copy_and_escape(std::wstring_view const string, wchar_t* out)
{
	wchar_t const* const out_beg = out;

	wchar_t const* beg = string.data();
	wchar_t const* const end = beg + string.size();

	while (beg != end)
	{
		wchar_t const character = *beg++;
		switch (character)
		{
		case L'\\':
		case L'\"':
			*out++ = L'\\';
		}
		*out++ = character;
	}

	return out - out_beg;
}

static size_t escape_in_place(std::span<wchar_t> const string, size_t const escape_count)
{
	wchar_t* const beg = string.data();
	wchar_t* end = beg + string.size();

	wchar_t* out = end + escape_count;

	while (beg != end)
	{
		wchar_t const character = *--end;
		*--out = character;
		switch (character)
		{
		case L'\\':
		case L'\"':
			*--out = L'\\';
		}
	}

	vsm_assert(out == beg);

	return string.size() + escape_count;
}

static vsm::result<wchar_t*> make_command_line(command_line_storage& buffer, std::span<input_string_view const> const arguments)
{
	size_t total_size = 3; // Empty string for the program name.

	static constexpr size_t utf8_escape_cache_size = 32;
	uint16_t utf8_escape_cache[utf8_escape_cache_size];

	for (size_t utf8_index = 0; input_string_view const argument : arguments)
	{
		if (argument.is_wide())
		{
			std::wstring_view const wide_argument = argument.wide_view();
			total_size += wide_argument.size() + count_escape_chars(wide_argument);
		}
		else
		{
			std::string_view const utf8_argument = argument.utf8_view();
			vsm_try(wide_size, utf8_to_wide(utf8_argument));

			size_t const escape_count = count_escape_chars(utf8_argument);
			total_size += wide_size + escape_count;

			if (utf8_index < utf8_escape_cache_size)
			{
				utf8_escape_cache[utf8_index++] = static_cast<uint16_t>(escape_count);
			}
		}

		// Two quotes and a space or a null terminator.
		total_size += 3;
	}

	if (total_size > 0x7fff)
	{
		return vsm::unexpected(error::process_arguments_too_long);
	}

	vsm_try(buffer_beg, buffer.reserve(total_size));

	wchar_t* buffer_pos = buffer_beg;
	wchar_t* const buffer_end = buffer_beg + total_size;

	*buffer_pos++ = L'"';
	*buffer_pos++ = L'"';
	*buffer_pos++ = L' ';

	for (size_t utf8_index = 0; input_string_view const argument : arguments)
	{
		*buffer_pos++ = L'"';
		if (argument.is_wide())
		{
			std::wstring_view const wide_argument = argument.wide_view();
			buffer_pos += copy_and_escape(wide_argument, buffer_pos);
		}
		else
		{
			std::string_view const utf8_argument = argument.utf8_view();
			size_t const wide_size = *utf8_to_wide(utf8_argument, std::span(buffer_pos, buffer_end));

			size_t const escape_count =
				utf8_index < utf8_escape_cache_size
					? utf8_escape_cache[utf8_index++]
					: count_escape_chars(utf8_argument);

			buffer_pos += escape_in_place(std::span(buffer_pos, wide_size), escape_count);
		}
		*buffer_pos++ = L'"';
		*buffer_pos++ = L' ';
	}

	// Replace the last space with a null terminator.
	buffer_pos[-1] = L'\0';

	vsm_assert(static_cast<size_t>(buffer_pos - buffer_beg) == total_size);

	return buffer_beg;
}
#endif

static vsm::result<process_exit_code> get_process_exit_code(HANDLE const handle)
{
	DWORD exit_code;
	if (!GetExitCodeProcess(handle, &exit_code))
	{
		return vsm::unexpected(get_last_error());
	}
	return static_cast<int32_t>(exit_code);
}

vsm::result<unique_handle> win32::open_process(process_id const pid, process_handle::open_parameters const& args)
{
	DWORD desired_access = SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION;

	HANDLE const handle = OpenProcess(desired_access, false, pid);

	if (handle == NULL)
	{
		return vsm::unexpected(get_last_error());
	}

	return vsm::result<unique_handle>(vsm::result_value, handle);
}

vsm::result<process_info> win32::launch_process(input_path_view const path, process_handle::launch_parameters const& args)
{
	kernel_path_storage path_storage;
	vsm_try(kernel_path, make_kernel_path(path_storage,
	{
		.path = path,
		.win32_api_form = true,
	}));

	command_line_storage command_line_storage;
	vsm_try(command_line, make_command_line(command_line_storage, path, args.arguments));

	DWORD creation_flags = 0;

	STARTUPINFOW startup_info = {};

	PROCESS_INFORMATION process_info;

	if (!CreateProcessW(
		kernel_path.path.data(),
		command_line,
		nullptr,
		nullptr,
		true,
		creation_flags,
		nullptr,
		nullptr,
		&startup_info,
		&process_info))
	{
		return vsm::unexpected(get_last_error());
	}

	unique_handle const thread = unique_handle(process_info.hThread);

	return vsm_lazy(win32::process_info
	{
		unique_handle(process_info.hProcess),
		static_cast<process_id>(process_info.dwProcessId),
	});
}

#if 0
static vsm::result<unique_handle> duplicate_handle(HANDLE const source_process, HANDLE const source_handle)
{
	HANDLE target_handle;
	if (!DuplicateHandle(source_process, source_handle,
		GetCurrentProcess(), &target_handle, 0, false, DUPLICATE_SAME_ACCESS))
	{
		return vsm::unexpected(get_last_error());
	}
	return vsm::result<unique_handle>(vsm::result_value, target_handle);
}
#endif


process_handle process_handle_base::current()
{
	static native_handle_type const native =
	{
		platform_handle::native_handle_type
		{
			handle::native_handle_type
			{
				handle_flags(flags::not_null) | flags::current | implementation::flags::pseudo_handle,
			},
			wrap_handle(GetCurrentProcess()),
		},
		static_cast<process_id>(GetCurrentProcessId()),
	};

	process_handle h;
	vsm_verify(h.set_native_handle(native));
	return h;
}


vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::close> const& args)
{
	process_handle& h = static_cast<process_handle&>(*args.handle);
	vsm_assert(h);
	
	vsm_try_void(base_type::sync_impl(args));

	h.m_pid.reset();
	h.m_exit_code.reset();
	
	return {};
}

#if 0
vsm::result<void> process_handle_base::close_sync(basic_parameters const& args)
{
	if (get_flags()[implementation::flags::pseudo_handle])
	{
		(void)base_type::release_native_handle();
	}
	else
	{
		vsm_try_void(base_type::close_sync(args));
	}

	m_exit_code.reset();
	m_pid.reset();

	return {};
}
#endif


vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_open> const& args)
{
	vsm_try_void(kernel_init());


	process_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(process, win32::open_process(args.pid, args));

	return initialize_platform_handle(h, vsm_move(process),
		[&](native_platform_handle const handle)
		{
			return process_handle::native_handle_type
			{
				platform_handle::native_handle_type
				{
					handle::native_handle_type
					{
						flags::not_null,
					},
					handle,
				},
				args.pid,
			};
		}
	);
}

vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_launch> const& args)
{
	vsm_try_void(kernel_init());


	process_handle& h = *args.handle;

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try_bind((process, pid), win32::launch_process(args.path, args));

	return initialize_platform_handle(h, vsm_move(process),
		[&](native_platform_handle const handle)
		{
			return process_handle::native_handle_type
			{
				platform_handle::native_handle_type
				{
					handle::native_handle_type
					{
						flags::not_null,
					},
					handle,
				},
				pid,
			};
		}
	);
}

vsm::result<void> process_handle_base::sync_impl(io::parameters_with_result<io::process_wait> const& args)
{
	process_handle& h = *args.handle;

	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}

	if (h.is_current())
	{
		return vsm::unexpected(error::process_is_current_process);
	}

	if (!h.get_flags()[flags::exited])
	{
		HANDLE const handle = unwrap_handle(h.get_platform_handle());

		NTSTATUS const status = win32::NtWaitForSingleObject(
			handle, false, kernel_timeout(args.deadline));

		if (!NT_SUCCESS(status) || status == STATUS_TIMEOUT)
		{
			return vsm::unexpected(static_cast<nt_error>(status));
		}

		vsm_try_assign(h.m_exit_code.value, get_process_exit_code(handle));
		h.set_flags(h.get_flags() | flags::exited);
	}

	*args.result = h.m_exit_code.value;
	return {};
}

allio_handle_implementation(process_handle);
