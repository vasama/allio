#include <allio/impl/win32/directory_handle.hpp>

#include <allio/impl/win32/encoding.hpp>
#include <allio/impl/win32/kernel.hpp>
#include <allio/impl/win32/sync_filesystem_handle.hpp>
#include <allio/win32/nt_error.hpp>

#include <vsm/concepts.hpp>

#include <bit>

using namespace allio;
using namespace allio::win32;

result<void> detail::directory_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	return sync_open<directory_handle>(args, open_kind::directory);
}

allio_HANDLE_IMPLEMENTATION(directory_handle);


using directory_stream_entry = FILE_ID_EXTD_DIR_INFORMATION;
using directory_entry_attributes = FILE_NETWORK_OPEN_INFORMATION;

static constexpr size_t directory_stream_buffer_size = sizeof(directory_stream_entry) + 0x7FFF * sizeof(wchar_t);
static constexpr size_t directory_stream_size = offsetof(directory_stream, buffer) + directory_stream_buffer_size;

result<directory_stream*> win32::get_or_create_directory_stream(directory_stream_native_handle& handle)
{
	directory_stream* stream = unwrap_directory_stream(handle);
	if (stream == nullptr)
	{
		void* const memory = operator new(directory_stream_size);
		if (memory == nullptr)
		{
			return allio_ERROR(error::not_enough_memory);
		}
		stream = ::new (memory) directory_stream;
		handle = wrap_directory_stream(stream);
	}
	return stream;
}

static directory_stream_entry& get_entry(directory_stream& stream, size_t const offset)
{
	allio_ASSERT(stream.any_ready);
	allio_ASSERT(offset < directory_stream_buffer_size - sizeof(directory_stream_entry));
	return *reinterpret_cast<directory_stream_entry*>(stream.buffer + offset);
}

static result<directory_entry_attributes> query_file_information(HANDLE const handle, directory_stream_entry const& entry)
{
	UNICODE_STRING unicode_string;
	unicode_string.Buffer = const_cast<wchar_t*>(entry.FileName);
	unicode_string.Length = entry.FileNameLength * sizeof(wchar_t);
	unicode_string.MaximumLength = unicode_string.Length + sizeof(wchar_t);

	OBJECT_ATTRIBUTES object_attributes = {};
	object_attributes.Length = sizeof(object_attributes);
	object_attributes.RootDirectory = handle;
	object_attributes.ObjectName = &unicode_string;

	directory_entry_attributes attributes;
	NTSTATUS const status = NtQueryFullAttributesFile(&object_attributes, &attributes);

	if (!NT_SUCCESS(status))
	{
		return allio_ERROR(static_cast<nt_error>(status));
	}

	return attributes;
}


static std::wstring_view get_entry_name(directory_stream_entry const& entry)
{
	return std::wstring_view(entry.FileName, entry.FileNameLength / sizeof(wchar_t));
}

static file_kind get_entry_kind(directory_stream_entry const& entry)
{
	if (entry.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
	{
		if (entry.ReparsePointTag == IO_REPARSE_TAG_SYMLINK)
		{
			return file_kind::symbolic_link;
		}

		if (entry.ReparsePointTag == IO_REPARSE_TAG_MOUNT_POINT)
		{
			return file_kind::ntfs_junction;
		}
	}

	if (entry.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
	{
		return file_kind::directory;
	}
	else
	{
		return file_kind::regular;
	}
}

static void convert_directory_entry(directory_stream_entry const& stream_entry, directory_stream_handle::entry& out_entry)
{
	out_entry.kind = get_entry_kind(stream_entry);
	out_entry.id = std::bit_cast<file_id>(stream_entry.FileId);
}

static directory_stream_iterator_native_handle wrap_iterator(size_t const offset)
{
	allio_ASSERT(offset < directory_stream_buffer_size);
	return std::bit_cast<directory_stream_iterator_native_handle>(offset + 1);
}

static size_t unwrap_iterator(directory_stream_iterator_native_handle const iterator)
{
	size_t const offset = std::bit_cast<size_t>(iterator) - 1;
	allio_ASSERT(offset < directory_stream_buffer_size);
	return offset;
}

void detail::directory_stream_handle_base::iterator_init(entry& iterator) const
{
	iterator.m_handle = [&]()
	{
		if (directory_stream* const stream = unwrap_directory_stream(m_stream.value))
		{
			if (stream->any_ready)
			{
				size_t const entry_offset = stream->entry_offset;
				directory_stream_entry const& stream_entry = get_entry(*stream, entry_offset);
				convert_directory_entry(stream_entry, iterator);
				return wrap_iterator(entry_offset);
			}
		}
		return directory_stream_iterator_native_handle::end;
	}();
}

void detail::directory_stream_handle_base::iterator_next(entry& iterator) const
{
	iterator.m_handle = [&]()
	{
		directory_stream* const stream = unwrap_directory_stream(m_stream.value);
		allio_ASSERT(stream != nullptr && stream->any_ready);

		size_t const entry_offset = unwrap_iterator(iterator.m_handle);
		directory_stream_entry const& stream_entry = get_entry(*stream, entry_offset);

		if (size_t const next_offset = stream_entry.NextEntryOffset)
		{
			size_t const next_entry_offset = entry_offset + next_offset;
			directory_stream_entry const& next_entry = get_entry(*stream, next_entry_offset);
			convert_directory_entry(next_entry, iterator);
			return wrap_iterator(next_entry_offset);
		}
		return directory_stream_iterator_native_handle::end;
	}();
}


result<void> detail::directory_stream_handle_base::restart()
{
	if (directory_stream* const stream = unwrap_directory_stream(m_stream.value))
	{
		stream->restart = true;
	}
	return {};
}


static bool is_dot_or_dot_dot(std::wstring_view const name)
{
	if (name[0] == L'.')
	{
		size_t const size = name.size();

		if (size == 1)
		{
			return true;
		}

		if (size == 2 && name[1] == L'.')
		{
			return true;
		}
	}

	return false;
}

NTSTATUS win32::query_directory_file_start(
	directory_stream& stream, HANDLE const handle,
	PIO_APC_ROUTINE const apc_routine, PVOID const apc_context,
	IO_STATUS_BLOCK& io_status_block)
{
	uint32_t flags = 0;

	if (stream.restart)
	{
		flags |= SL_RESTART_SCAN;
	}
	else
	{
		allio_ASSERT(stream.can_fetch);
	}

	stream.any_ready = false;
	stream.can_fetch = false;

	return NtQueryDirectoryFileEx(
		handle,
		NULL,
		apc_routine,
		apc_context,
		&io_status_block,
		stream.buffer,
		directory_stream_buffer_size,
		FileIdExtdDirectoryInformation,
		flags,
		nullptr);
}

NTSTATUS win32::query_directory_file_completed(
	directory_stream& stream,
	IO_STATUS_BLOCK const& io_status_block)
{
	NTSTATUS const status = io_status_block.Status;
	bool const restart = std::exchange(stream.restart, false);

	if (!NT_SUCCESS(status))
	{
		if (status == STATUS_NO_MORE_FILES)
		{
			return STATUS_SUCCESS;
		}

		return status;
	}

	if (io_status_block.Information == 0)
	{
		return STATUS_BUFFER_TOO_SMALL;
	}

	stream.any_ready = true;
	stream.can_fetch = stream.any_ready;

	size_t entry_offset = 0;

	if (restart)
	{
		while (true)
		{
			directory_stream_entry const& entry = get_entry(stream, entry_offset);

			if (!is_dot_or_dot_dot(get_entry_name(entry)))
			{
				break;
			}

			if (entry.NextEntryOffset == 0)
			{
				stream.any_ready = false;
				break;
			}

			entry_offset += entry.NextEntryOffset;
		}
	}

	stream.entry_offset = static_cast<uint16_t>(entry_offset);

	return status;
}

static result<void> query_directory_file(HANDLE const handle, directory_stream& stream)
{
	IO_STATUS_BLOCK io_status_block = make_io_status_block();

	NTSTATUS status = query_directory_file_start(
		stream, handle, nullptr, nullptr, io_status_block);

	if (status == STATUS_PENDING)
	{
		status = io_wait(handle, &io_status_block, deadline::never());
	}

	status = query_directory_file_completed(stream, io_status_block);

	if (!NT_SUCCESS(status))
	{
		return allio_ERROR(static_cast<nt_error>(status));
	}

	return {};
}


//TODO: Move to some shared header.
static result<size_t> copy_string(std::wstring_view const string, output_string_ref const output)
{
	bool const c_string = output.is_c_string();

	if (output.is_wide())
	{
		size_t const wide_size = string.size();
		size_t const wide_buffer_size = wide_size + c_string;
		allio_TRY(buffer, output.resize_wide(wide_buffer_size));
		buffer[wide_buffer_size - 1] = L'\0';
		memcpy(buffer.data(), string.data(), wide_size * sizeof(wchar_t));
		return wide_size;
	}
	else
	{
		//TODO: Improve the encoding API and transcode into preallocated buffer if available.
		allio_TRY(utf8_size, wide_to_utf8(string));
		size_t const utf8_buffer_size = utf8_size + c_string;
		allio_TRY(buffer, output.resize_utf8(utf8_buffer_size));
		buffer[utf8_buffer_size - 1] = '\0';
		allio_VERIFY(wide_to_utf8(string, buffer));
		return utf8_size;
	}
}


static result<void> sync_open_directory_stream(directory_stream_handle& h, unique_handle_with_flags&& directory)
{
	return initialize_platform_handle(h, std::move(directory.handle),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					directory.flags | handle::flags::not_null,
				},
				handle,
			};
		}
	);
}

static filesystem_handle::open_parameters make_stream_open_args(directory_stream_handle::open_parameters const& args)
{
	filesystem_handle::open_parameters open_args = {};
	static_cast<directory_stream_handle::open_parameters&>(open_args) = args;
	open_args.mode = file_mode::read;
	open_args.creation = file_creation::open_existing;
	open_args.caching = file_caching::none;
	open_args.flags = file_flags::none;
	return open_args;
}

result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_open_path> const& args)
{
	directory_stream_handle& h = *args.handle;

	if (h)
	{
		return allio_ERROR(error::handle_is_not_null);
	}

	allio_TRY(directory, create_file(args.base, args.path, make_stream_open_args(args), open_kind::directory));

	return sync_open_directory_stream(h, std::move(directory));
}

result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_open_handle> const& args)
{
	directory_stream_handle& h = *args.handle;

	if (h)
	{
		return allio_ERROR(error::handle_is_not_null);
	}

	if (!*args.directory)
	{
		return allio_ERROR(error::invalid_argument);
	}

	allio_TRY(directory, reopen_file(*args.directory, make_stream_open_args(args), open_kind::directory));

	return sync_open_directory_stream(h, std::move(directory));
}

result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_fetch> const& args)
{
	directory_stream_handle& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	allio_TRY(stream, get_or_create_directory_stream(h.m_stream.value));

	if (!stream->can_fetch && !stream->restart)
	{
		return allio_ERROR(error::directory_stream_at_end);
	}

	allio_TRYV(query_directory_file(unwrap_handle(h.get_platform_handle()), *stream));

	*args.result = stream->any_ready || stream->can_fetch;

	return {};
}

result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_get_name> const& args)
{
	directory_stream_handle const& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	directory_stream* const stream = unwrap_directory_stream(h.m_stream.value);
	allio_ASSERT(stream != nullptr && stream->any_ready);
	directory_stream_entry const& stream_entry = get_entry(*stream, unwrap_iterator(args.entry->m_handle));

	return args.produce(copy_string(get_entry_name(stream_entry), args.output));
}

#if 0
struct detail::directory_stream_handle_base::sync_impl_helper
{
	static directory_stream_entry const& get_entry(
		directory_stream_handle_base const& h, entry const& entry)
	{
		directory_stream* const stream = unwrap_directory_stream(h.m_stream.value);
		allio_ASSERT(stream != nullptr && stream->any_ready);
		return ::get_entry(*stream, unwrap_iterator(entry.m_handle));
	}
};

template<typename Char>
result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_get_name<Char>> const& args)
{
	directory_stream_handle& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	std::wstring_view const wide_name =
		get_entry_name(sync_impl_helper::get_entry(h, *args.entry));

	std::basic_string<Char>& buffer = args.result->string();

	allio_TRY(size, [&]() -> result<size_t>
	{
		if constexpr (std::is_same_v<Char, char>)
		{
			return wide_to_utf8(wide_name);
		}
		else
		{
			return wide_name.size();
		}
	}());

	try
	{
		buffer.resize(size);
	}
	catch (std::bad_alloc const&)
	{
		return allio_ERROR(error::not_enough_memory);
	}

	if constexpr (std::is_same_v<Char, char>)
	{
		allio_VERIFY(wide_to_utf8(wide_name, buffer));
	}
	else
	{
		memcpy(buffer.data(), wide_name.data(), wide_name.size() * sizeof(wchar_t));
	}

	return {};
}

template<typename Char>
result<void> detail::directory_stream_handle_base::sync_impl(io::parameters_with_result<io::directory_stream_copy_name<Char>> const& args)
{
	directory_stream_handle& h = *args.handle;

	if (!h)
	{
		return allio_ERROR(error::handle_is_null);
	}

	std::wstring_view const wide_name =
		get_entry_name(sync_impl_helper::get_entry(h, *args.entry));

	allio_TRY(size, [&]() -> result<size_t>
	{
		if constexpr (std::is_same_v<Char, char>)
		{
			return wide_to_utf8(wide_name, args.buffer);
		}
		else
		{
			if (wide_name.size() > args.buffer.size())
			{
				return allio_ERROR(error::no_buffer_space);
			}

			memcpy(args.buffer.data(), wide_name.data(), wide_name.size() * sizeof(wchar_t));
			return wide_name.size();
		}
	}());

	*args.result = basic_path_view<Char>(args.buffer.data(), size);

	return {};
}
#endif

allio_HANDLE_IMPLEMENTATION(directory_stream_handle);
