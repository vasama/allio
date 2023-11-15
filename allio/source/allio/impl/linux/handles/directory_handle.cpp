#include <allio/directory.hpp>

#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/fs_object.hpp>

#include <vsm/standard.hpp>

#include <bit>
#include <memory>

#include <dirent.h>
#include <sys/syscall.h>

#include <allio/linux/detail/undef.i>

using namespace allio;
using namespace allio::detail;
using namespace allio::linux;

namespace {

struct linux_dirent64
{
	ino64_t d_ino;
	off64_t d_off;
	unsigned short d_reclen;
	unsigned char d_type;
	char d_name[];
};

} // namespace

static ssize_t getdents64(int const fd, linux_dirent64* const dirent, size_t count)
{
	// The syscall takes unsigned int for count, but later converts it to int.

	if (count > static_cast<size_t>(std::numeric_limits<int>::max()))
	{
		count = static_cast<size_t>(std::numeric_limits<int>::max());
	}

	return syscall(__NR_getdents64, static_cast<unsigned int>(fd), dirent, static_cast<unsigned int>(count));
}


using directory_stream_entry = linux_dirent64;

static file_kind get_entry_kind(directory_stream_entry const& entry)
{
	switch (entry.d_type)
	{
	case DT_BLK:
		return file_kind::block_device;
		
	case DT_CHR:
		return file_kind::character_device;
		
	case DT_DIR:
		return file_kind::directory;
		
	case DT_FIFO:
		return file_kind::pipe;

	case DT_LNK:
		return file_kind::symbolic_link;
		
	case DT_REG:
		return file_kind::regular;
		
	case DT_SOCK:
		return file_kind::socket;
	}

	return file_kind::unknown;
}

static std::string_view get_entry_name(directory_stream_entry const& entry)
{
	static constexpr size_t name_offset = offsetof(directory_stream_entry, d_name);

	size_t name_size = entry.d_reclen - name_offset;

	// Scan backwards to discard any padding zeros.
	while (name_size > 0 && entry.d_name[name_size - 1] == '\0')
	{
		--name_size;
	}

	vsm_assert(name_size == strlen(entry.d_name));

	return std::string_view(entry.d_name, name_size);
}


static directory_stream_native_handle wrap_stream(directory_stream_entry const* const stream)
{
	return static_cast<directory_stream_native_handle>(reinterpret_cast<uintptr_t>(stream));
}

static directory_stream_entry const* unwrap_stream(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::directory_end);
	return reinterpret_cast<directory_stream_entry const*>(static_cast<uintptr_t>(handle));
}

static directory_stream_entry const* next_entry(directory_stream_entry const* const entry)
{
	vsm_assert(entry != nullptr);
	size_t const next_offset = entry->d_off;
	return next_offset != 0
		? reinterpret_cast<directory_stream_entry const*>(
			reinterpret_cast<std::byte const*>(entry) + next_offset)
		: nullptr;
}


directory_entry directory_entry_view::get_entry(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::end);
	directory_stream_entry const& entry = *unwrap_stream(handle);
	
	return
	{
		.kind = get_entry_kind(entry),
		.node_id = std::bit_cast<file_id_64>(entry.d_ino),
		.name = input_string_view(get_entry_name(entry)),
	};
}

directory_stream_native_handle directory_stream_iterator::advance(directory_stream_native_handle const handle)
{
	return wrap_stream(next_entry(unwrap_stream(handle)));
}


static bool discard_entry(directory_stream_entry const& entry)
{
	std::string_view const name = get_entry_name(entry);
	
	if (name[0] == '.')
	{
		size_t const size = name.size();
		
		if (size == 1)
		{
			return true;
		}
		
		if (size == 2 && name[1] == '.')
		{
			return true;
		}
	}
	
	return false;
}

static directory_stream_entry* create_entry_list(std::span<std::byte> const buffer, size_t const size)
{
	auto const get_entry = [&](size_t const offset) -> directory_stream_entry*
	{
		vsm_assert(offset < buffer.size());
		return reinterpret_cast<directory_stream_entry*>(buffer.data() + offset);
	};

	using offset_type = decltype(directory_stream_entry::d_off);

	// d_off of an imaginary entry at index -1.
	// Holds both absolute and relative offset of the first entry.
	offset_type first_offset = 0;

	// Pointer to d_off of the previous entry in each iteration.
	offset_type* p_last_offset = &first_offset;

	for (size_t offset = 0; offset < size;)
	{
		directory_stream_entry& entry = *get_entry(offset);
	
		if (!discard_entry(entry))
		{
			// Store the relative offset between the two chosen entries in d_off of the previous entry.
			*p_last_offset = offset - *p_last_offset;

			// Temporarily store the absolute offset of this entry in its d_off.
			entry.d_off = offset;

			// Store the address of this entry's d_off to be updated in the next iteration.
			p_last_offset = &entry.d_off;
		}

		offset += entry.d_reclen;
	}

	// Get a pointer to the first entry before potentially zeroing the offset.
	directory_stream_entry* const first_entry = get_entry(first_offset);

	// Finally set the relative offset of the last entry to zero to indicate the end of the list.
	*p_last_offset = 0;

	return first_entry;
}


vsm::result<void> directory_handle_base::sync_impl(io::parameters_with_result<io::filesystem_open> const& args)
{
	directory_handle& h = static_cast<directory_handle&>(*args.handle);

	if (h)
	{
		return vsm::unexpected(error::handle_is_not_null);
	}

	vsm_try(open_args, linux::open_parameters::make(args));
	open_args.flags |= O_DIRECTORY;

	vsm_try(h_fd, create_file(args.base, args.path, open_args));

	return initialize_platform_handle(
		h, vsm_move(h_fd),
		[&](native_platform_handle const handle)
		{
			return platform_handle::native_handle_type
			{
				handle::native_handle_type
				{
					handle::flags::not_null,
				},
				handle,
			};
		}
	);
}

vsm::result<void> directory_handle_base::sync_impl(io::parameters_with_result<io::directory_read> const& args)
{
	directory_handle const& h = *args.handle;
	
	if (!h)
	{
		return vsm::unexpected(error::handle_is_null);
	}
	
	//TODO: Align buffer first.
	auto buffer = args.buffer;

	ssize_t const count = getdents64(
		unwrap_handle(h.get_platform_handle()),
		reinterpret_cast<directory_stream_entry*>(buffer.data()),
		buffer.size());

	if (count == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	// Create the iterable entry list by linking the entries together
	// with relative offsets and discarding . and .. entries.
	directory_stream_entry const* const entry = create_entry_list(buffer, count);

	*args.result = directory_stream_view(wrap_stream(entry));
	return {};
}

allio_handle_implementation(directory_handle);


vsm::result<size_t> this_process::get_current_directory(output_path_ref const output)
{
	if (output.is_wide())
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	char const* const c_string = getcwd(nullptr, 0);

	if (c_string == nullptr)
	{
		return vsm::unexpected(get_last_error());
	}

	struct cwd_deleter
	{
		void vsm_static_operator_invoke(char const* const c_string)
		{
			free(const_cast<void*>(static_cast<void const*>(c_string)));
		}
	};
	std::unique_ptr<char const, cwd_deleter> const unique_c_string(c_string);

	size_t const string_size = std::strlen(c_string);
	size_t const output_size = string_size + output.is_c_string();

	vsm_try(buffer, output.resize_utf8(output_size));
	memcpy(buffer.data(), c_string, output_size);

	return string_size;
}

vsm::result<void> this_process::set_current_directory(input_path_view const path)
{
	if (path.is_wide())
	{
		return vsm::unexpected(error::unsupported_encoding);
	}

	api_string_storage storage;
	vsm_try(c_string, make_api_c_string(storage, path));

	if (chdir(c_string) == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	return {};
}

vsm::result<directory_handle> this_process::open_current_directory()
{
	//int const fd = open(".", O_RDONLY | O_DIRECTORY, 0);
	
	return vsm::unexpected(error::unsupported_operation);
}
