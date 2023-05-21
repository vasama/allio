#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>
#include <allio/output_string_ref.hpp>

#include <vsm/box.hpp>

#include <span>

namespace allio {

namespace io {

struct directory_read;

} // namespace io


namespace detail {

class directory_stream_view;


class directory_handle_base : public filesystem_handle
{
	using final_handle_type = final_handle<directory_handle_base>;

public:
	using base_type = filesystem_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::directory_read
		>
	>;


	#define allio_directory_handle_fetch_parameters(type, data, ...) \
		type(allio::detail::directory_handle_base, read_parameters) \
		data(bool, restart, false) \

	allio_interface_parameters(allio_directory_handle_fetch_parameters);

	using get_name_parameters = basic_parameters;


	constexpr directory_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}


	template<parameters<read_parameters> P = read_parameters::interface>
	vsm::result<directory_stream_view> read(std::span<std::byte> const buffer, P const& args = {})
	{
		return block_read(buffer, args);
	}

	template<parameters<directory_handle_base::read_parameters> P = read_parameters::interface>
	basic_sender<io::directory_read> read_async(std::span<std::byte> buffer, P const& args = {});


private:
	vsm::result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);

	vsm::result<directory_stream_view> block_read(std::span<std::byte> buffer, read_parameters const& args);


protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::filesystem_open> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_read> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

using directory_handle = final_handle<detail::directory_handle_base>;


enum class directory_stream_native_handle : uintptr_t
{
	end                                 = 0,
	directory_end                       = -1,
};

struct directory_entry
{
	file_kind kind;
	file_id_64 node_id;
	input_string_view name;

	vsm::result<size_t> get_name(output_string_ref output) const;

	template<typename String = std::string>
	vsm::result<String> get_name() const
	{
		String string = {};
		vsm_try_discard(get_name(string));
		return static_cast<String&&>(string);
	}
};

class directory_entry_view
{
	directory_stream_native_handle m_handle;

public:
	explicit directory_entry_view(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] directory_entry get_entry() const
	{
		return get_entry(m_handle);
	}

	[[nodiscard]] operator directory_entry() const
	{
		return get_entry(m_handle);
	}

	vsm::result<file_attributes> get_attributes(directory_handle const& directory) const;

private:
	static directory_entry get_entry(directory_stream_native_handle handle);
};

class directory_stream_sentinel {};

class directory_stream_iterator
{
	directory_stream_native_handle m_handle;

public:
	explicit directory_stream_iterator(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] directory_entry_view operator*() const
	{
		return directory_entry_view(m_handle);
	}

	[[nodiscard]] vsm::box<directory_entry_view> operator->() const
	{
		return directory_entry_view(m_handle);
	}

	directory_stream_iterator& operator++()
	{
		m_handle = advance(m_handle);
		return *this;
	}

	[[nodiscard]] directory_stream_iterator operator++(int) &
	{
		auto it = *this;
		m_handle = advance(m_handle);
		return *this;
	}

	[[nodiscard]] bool operator==(directory_stream_sentinel) const
	{
		return m_handle == directory_stream_native_handle::end;
	}

	[[nodiscard]] bool operator!=(directory_stream_sentinel) const
	{
		return m_handle != directory_stream_native_handle::end;
	}

private:
	static directory_stream_native_handle advance(directory_stream_native_handle handle);
};

class directory_stream_view
{
	directory_stream_native_handle m_handle;

public:
	directory_stream_view() = default;

	explicit directory_stream_view(directory_stream_native_handle const handle)
		: m_handle(handle)
	{
	}

	[[nodiscard]] explicit operator bool() const
	{
		return m_handle != directory_stream_native_handle::directory_end;
	}

	[[nodiscard]] directory_stream_iterator begin() const
	{
		vsm_assert(m_handle != directory_stream_native_handle::directory_end);
		return directory_stream_iterator(m_handle);
	}

	[[nodiscard]] directory_stream_sentinel end() const
	{
		vsm_assert(m_handle != directory_stream_native_handle::directory_end);
		return {};
	}
};

class directory_stream
{
	std::span<std::byte> m_buffer;
	directory_stream_view m_stream;

public:
	explicit directory_stream(std::span<std::byte> const buffer)
		: m_buffer(buffer)
	{
	}

	template<parameters<directory_handle::read_parameters> P = directory_handle::read_parameters::interface>
	vsm::result<bool> read(directory_handle& directory, P const& args = {})
	{
		vsm_try_assign(m_stream, directory.read(m_buffer));
		return static_cast<bool>(m_stream);
	}

	[[nodiscard]] directory_stream_iterator begin() const
	{
		return m_stream.begin();
	}

	[[nodiscard]] directory_stream_sentinel end() const
	{
		return m_stream.end();
	}
};

class directory_stream_buffer
{
public:
};


vsm::result<directory_handle> block_open_directory(filesystem_handle const* base, input_path_view path, directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_unique_directory(filesystem_handle const& base, directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_temporary_directory(directory_handle_base::open_parameters const& args);
vsm::result<directory_handle> block_open_anonymous_directory(filesystem_handle const* base, directory_handle_base::open_parameters const& args);

} // namespace detail

using detail::directory_handle;

using detail::directory_entry;
using detail::directory_entry_view;
using detail::directory_stream_view;
using detail::directory_stream_buffer;
using detail::directory_stream;

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_directory(input_path_view const path, P const& args = {})
{
	return detail::block_open_directory(nullptr, path, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_directory(filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::block_open_directory(&base, path, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_unique_directory(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_unique_directory(base, args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_temporary_directory(P const& args = {})
{
	return detail::block_open_temporary_directory(args);
}

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
vsm::result<directory_handle> open_anonymous_directory(filesystem_handle const& base, P const& args = {})
{
	return detail::block_open_anonymous_directory(base, args);
}


template<>
struct io::parameters<io::directory_read>
	: directory_handle::read_parameters
{
	using handle_type = directory_handle;
	using result_type = directory_stream_view;

	std::span<std::byte> buffer;
};

allio_detail_api extern allio_handle_implementation(directory_handle);


namespace this_process {

vsm::result<size_t> get_current_directory(output_path_ref output);

template<typename Path = path>
vsm::result<Path> get_current_directory()
{
	Path path = {};
	vsm_try_discard(get_current_directory(path));
	return static_cast<Path&&>(path);
}

vsm::result<void> set_current_directory(input_path_view path);

vsm::result<directory_handle> open_current_directory();

//TODO: open_current_directory_async(multiplexer& multiplexer);

} // namespace this_process

#if 0
namespace detail {

enum class directory_stream_native_handle : uintptr_t {};

enum class directory_stream_iterator_native_handle : uintptr_t
{
	end = 0
};

class directory_stream_handle_base : public platform_handle
{
	using final_handle_type = final_handle<directory_stream_handle_base>;

	vsm::linear<directory_stream_native_handle> m_stream;

public:
	using base_type = platform_handle;

	using async_operations = type_list_cat<
		base_type::async_operations,
		type_list<
			io::directory_stream_open_path,
			io::directory_stream_open_handle,
			io::directory_stream_fetch,
			io::directory_stream_get_name
		>
	>;


	using open_parameters = base_type::create_parameters;
	using read_parameters = basic_parameters;
	using get_name_parameters = basic_parameters;


	class entry
	{
		directory_stream_iterator_native_handle m_handle;

	public:
		file_kind kind;
		file_id id;

	private:
		entry() = default;
		entry(entry const&) = default;
		entry& operator=(entry const&) = default;
		~entry() = default;

		friend class directory_stream_handle_base;
	};

	class entry_sentinel
	{
	};

	class entry_iterator
	{
		directory_stream_handle_base const* m_stream;
		entry m_entry;

	public:
		explicit entry_iterator(directory_stream_handle_base const& stream)
			: m_stream(&stream)
		{
			stream.iterator_init(m_entry);
		}

		[[nodiscard]] entry const& operator*() const
		{
			vsm_assert(m_entry.m_handle != directory_stream_iterator_native_handle::end);
			return m_entry;
		}

		[[nodiscard]] entry const* operator->() const
		{
			vsm_assert(m_entry.m_handle != directory_stream_iterator_native_handle::end);
			return &m_entry;
		}

		entry_iterator& operator++() &
		{
			m_stream->iterator_next(m_entry);
			return *this;
		}

		[[nodiscard]] entry_iterator operator++(int) &
		{
			entry_iterator iterator = *this;
			m_stream->iterator_next(m_entry);
			return iterator;
		}

		[[nodiscard]] bool operator==(entry_sentinel) const
		{
			return m_entry.m_handle == directory_stream_iterator_native_handle::end;
		}

		[[nodiscard]] bool operator!=(entry_sentinel) const
		{
			return m_entry.m_handle != directory_stream_iterator_native_handle::end;
		}
	};

	class entry_range
	{
		directory_stream_handle_base const* m_stream;

	public:
		explicit entry_range(directory_stream_handle_base const& stream)
			: m_stream(&stream)
		{
		}

		[[nodiscard]] entry_iterator begin() const
		{
			return entry_iterator(*m_stream);
		}

		[[nodiscard]] entry_sentinel end() const
		{
			return {};
		}
	};


	constexpr directory_stream_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	vsm::result<void> open(directory_handle const& handle, P const& args = {})
	{
		return block_open(handle, args);
	}

	template<parameters<directory_stream_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(input_path_view path, P const& args = {});

	template<parameters<directory_stream_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(filesystem_handle const& base, input_path_view path, P const& args = {});

	template<parameters<directory_stream_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(filesystem_handle const* base, input_path_view path, P const& args = {});

	template<parameters<directory_stream_handle_base::open_parameters> P = open_parameters::interface>
	basic_sender<io::filesystem_open> open_async(directory_handle const& handle, P const& args = {});


	vsm::result<void> restart();

	template<parameters<read_parameters> P = read_parameters::interface>
	vsm::result<bool> fetch(P const& args = {})
	{
		return block_read(args);
	}

	template<parameters<directory_stream_handle_base::read_parameters> P = read_parameters::interface>
	basic_sender<io::directory_stream_fetch> fetch_async(P const& args = {});

	[[nodiscard]] entry_range entries() const
	{
		return entry_range(*this);
	}


	template<typename String = std::string, parameters<get_name_parameters> P = get_name_parameters::interface>
	vsm::result<String> get_name(entry const& entry, P const& args = {}) const
	{
		String string = {};
		vsm_try_discard(block_get_name(entry, string, args));
		return static_cast<String&&>(string);
	}

	template<parameters<get_name_parameters> P = get_name_parameters::interface>
	vsm::result<size_t> get_name(entry const& entry, output_string_ref const& output, P const& args = {}) const
	{
		return block_get_name(entry, output, args);
	}

	//template<typename String, parameters<get_name_parameters> P = get_name_parameters::interface>
	//basic_sender<io::directory_stream_get_name> get_name_async(entry const& entry, P const& args = {}) const;

	template<parameters<get_name_parameters> P = get_name_parameters::interface>
	basic_sender<io::directory_stream_get_name> get_name_async(entry const& entry, output_string_ref const& output, P const& args = {}) const;

private:
	void iterator_init(entry& entry) const;
	void iterator_next(entry& entry) const;

	vsm::result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);
	vsm::result<void> block_open(directory_handle const& handle, open_parameters const& args);

	vsm::result<bool> block_read(read_parameters const& args);
	vsm::result<size_t> block_get_name(entry const& entry, output_string_ref const& output, get_name_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_stream_open_path> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_stream_open_handle> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_stream_fetch> const& args);
	static vsm::result<void> sync_impl(io::parameters_with_result<io::directory_stream_get_name> const& args);

	template<typename H, typename O>
	friend vsm::result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

vsm::result<final_handle<directory_stream_handle_base>> block_open_directory_stream(filesystem_handle const* base, input_path_view path, directory_stream_handle_base::open_parameters const& args);
vsm::result<final_handle<directory_stream_handle_base>> block_open_directory_stream(directory_handle const& directory, directory_stream_handle_base::open_parameters const& args);

} // namespace detail

using directory_stream_handle = final_handle<detail::directory_stream_handle_base>;

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
vsm::result<directory_stream_handle> open_directory_stream(input_path_view const path, P const& args = {})
{
	return detail::block_open_directory_stream(nullptr, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
vsm::result<directory_stream_handle> open_directory_stream(filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::block_open_directory_stream(&base, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
vsm::result<directory_stream_handle> open_directory_stream(directory_handle const& directory, P const& args = {})
{
	return detail::block_open_directory_stream(directory, args);
}


template<>
struct io::parameters<io::directory_stream_open_path>
	: directory_stream_handle::open_parameters
{
	using handle_type = directory_stream_handle;
	using result_type = void;

	filesystem_handle const* base;
	input_path_view path;
};

template<>
struct io::parameters<io::directory_stream_open_handle>
	: directory_stream_handle::open_parameters
{
	using handle_type = directory_stream_handle;
	using result_type = void;

	directory_handle const* directory;
};

template<>
struct io::parameters<io::directory_stream_fetch>
	: directory_stream_handle::read_parameters
{
	using handle_type = directory_stream_handle;
	using result_type = bool;
};

template<>
struct io::parameters<io::directory_stream_get_name>
	: directory_stream_handle::get_name_parameters
{
	using handle_type = directory_stream_handle const;
	using result_type = size_t;

	directory_stream_handle::entry const* entry;
	output_string_ref output;
};

allio_detail_api extern allio_handle_implementation(directory_stream_handle);
#endif

} // namespace allio
