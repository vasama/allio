#include <allio/handles/directory.hpp>

#include <allio/impl/transcode.hpp>
#include <allio/impl/linux/api_string.hpp>
#include <allio/impl/linux/error.hpp>
#include <allio/impl/linux/handles/fs_object.hpp>
#include <allio/linux/platform.hpp>

#include <vsm/lazy.hpp>
#include <vsm/numeric.hpp>
#include <vsm/standard.hpp>

#include <bit>
#include <memory>

#include <dirent.h>
#include <sys/syscall.h>
#include <unistd.h>

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

	vsm_gnu_diagnostic(push)
	vsm_gnu_diagnostic(ignored "-Wpedantic")
	char d_name[];
	vsm_gnu_diagnostic(pop)
};

static ssize_t getdents64(int const fd, linux_dirent64* const dirent, size_t count)
{
	return syscall(
		__NR_getdents64,
		static_cast<unsigned>(fd),
		dirent,
		// The syscall takes unsigned int for count, but later converts it to int.
		static_cast<unsigned>(vsm::saturate<int>(count)));
}

} // namespace


using directory_stream_entry = linux_dirent64;

static fs_entry_type get_entry_type(directory_stream_entry const& entry)
{
	switch (entry.d_type)
	{
	case DT_BLK:
		return fs_entry_type::block_device;
		
	case DT_CHR:
		return fs_entry_type::character_device;
		
	case DT_DIR:
		return fs_entry_type::directory;
		
	case DT_FIFO:
		return fs_entry_type::pipe;

	case DT_LNK:
		return fs_entry_type::symbolic_link;
		
	case DT_REG:
		return fs_entry_type::regular;
		
	case DT_SOCK:
		return fs_entry_type::socket;
	}

	return fs_entry_type::unknown;
}

static std::string_view get_entry_name(directory_stream_entry const& entry)
{
	static constexpr size_t name_offset = offsetof(directory_stream_entry, d_name);
	size_t const max_name_size = static_cast<size_t>(entry.d_reclen) - name_offset;
	std::string_view const name(entry.d_name, strnlen(entry.d_name, max_name_size));
	vsm_assert(name.size() < max_name_size);
	return name;
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
	
	if (size_t const next_offset = entry->d_off)
	{
		return reinterpret_cast<directory_stream_entry const*>(
			reinterpret_cast<std::byte const*>(entry) + next_offset);
	}

	return nullptr;
}


vsm::result<size_t> directory_entry::get_name(any_string_buffer const buffer) const
{
	return transcode_string(name.view<char>(), buffer);
}

directory_entry directory_entry_view::get_entry(directory_stream_native_handle const handle)
{
	vsm_assert(handle != directory_stream_native_handle::end);
	directory_stream_entry const& entry = *unwrap_stream(handle);
	
	return
	{
		.type = get_entry_type(entry),
		.node_id = std::bit_cast<fs_node_id>(entry.d_ino),
		.name = any_string_view(get_entry_name(entry), null_terminated),
	};
}

directory_stream_native_handle directory_stream_iterator::advance(directory_stream_native_handle const handle)
{
	return wrap_stream(next_entry(unwrap_stream(handle)));
}


static bool filter_entry(directory_stream_entry const& entry)
{
	char const* const name = entry.d_name;

	if (name[0] == '.')
	{
		if (name[1] == '\0')
		{
			return false;
		}

		if (name[1] == '.' && name[2] == '\0')
		{
			return false;
		}
	}

	return true;
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
	
		if (filter_entry(entry))
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

	// It is possible that p_last_offset still points to first_offset.
	// Get a pointer to the first entry before potentially zeroing the offset.
	directory_stream_entry* const first_entry = get_entry(first_offset);

	// Finally set the relative offset of the last entry to zero to indicate the end of the list.
	*p_last_offset = 0;

	return first_entry;
}


vsm::result<void> directory_t::open(
	native_type& h,
	io_parameters_t<directory_t, open_t> const& a)
{
	vsm_try_bind((flags, mode), make_open_info(open_kind::directory, a));

	flags |= O_DIRECTORY;

	api_string_storage path_storage;
	vsm_try(path, make_api_c_string(path_storage, a.path.path.string()));

	vsm_try(fd, linux::open_file(
		unwrap_handle(a.path.base),
		path,
		flags,
		mode));

	h = fs_object_t::native_type
	{
		platform_object_t::native_type
		{
			object_t::native_type
			{
				flags::not_null,
			},
			wrap_handle(fd.release()),
		},
		//TODO: Validate flags
		a.flags,
	};

	return {};
}

vsm::result<directory_stream_view> directory_t::read(
	native_type const& h,
	io_parameters_t<directory_t, read_t> const& a)
{
	//TODO: Align the buffer first.
	auto buffer = a.buffer;

	memset(a.buffer.data(), 0xCD, a.buffer.size());
	ssize_t const count = getdents64(
		unwrap_handle(h.platform_handle),
		reinterpret_cast<directory_stream_entry*>(buffer.data()),
		buffer.size());

	if (count == -1)
	{
		return vsm::unexpected(get_last_error());
	}

	if (count == 0)
	{
		return directory_stream_view(directory_stream_native_handle::directory_end);
	}

	// Create the iterable entry list by linking the entries together
	// with relative offsets and discarding . and .. entries.
	directory_stream_entry const* const entry = create_entry_list(buffer, count);

	return directory_stream_view(wrap_stream(entry));
}


vsm::result<size_t> this_process::get_current_directory(any_path_buffer const buffer)
{
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

	return transcode_string(std::string_view(c_string), buffer);
}

vsm::result<void> this_process::set_current_directory(fs_path const path)
{
	api_string_storage storage;
	vsm_try(path_string, make_api_string(storage, path.path.string()));

	if (path.base == native_platform_handle::null)
	{
		if (chdir(path_string.data()) == -1)
		{
			return vsm::unexpected(get_last_error());
		}
	}
	else
	{
		int fd = unwrap_handle(path.base);
		unique_fd new_fd;

		//TODO: Use lexically_equivalent(path, ".")?
		if (!path_string.empty())
		{
			vsm_try(new_fd, linux::open_file(
				fd,
				path_string.data(),
				O_PATH | O_DIRECTORY | O_CLOEXEC));

			fd = new_fd.get();
		}

		if (fchdir(fd) == -1)
		{
			return vsm::unexpected(get_last_error());
		}
	}

	return {};
}

vsm::result<blocking::directory_handle> this_process::open_current_directory()
{
	vsm_try(fd, linux::open_file(
		/* dir_fd: */ -1,
		"",
		O_RDONLY | O_DIRECTORY | O_CLOEXEC));

	return vsm_lazy(blocking::directory_handle(
		adopt_handle,
		fs_object_t::native_type
		{
			platform_object_t::native_type
			{
				object_t::native_type
				{
					object_t::flags::not_null,
				},
				wrap_handle(fd.release()),
			},
			file_flags::none,
		}
	));
}
