#pragma once

#include <allio/async_fwd.hpp>
#include <allio/detail/api.hpp>
#include <allio/filesystem_handle.hpp>
#include <allio/multiplexer.hpp>
#include <allio/output_string_ref.hpp>

#include <span>

namespace allio {

namespace io {

struct directory_stream_open_path;
struct directory_stream_open_handle;
struct directory_stream_fetch;
struct directory_stream_get_name;

} // namespace io


namespace detail {

class directory_handle_base : public filesystem_handle
{
	using final_handle_type = final_handle<directory_handle_base>;

public:
	using base_type = filesystem_handle;


	constexpr directory_handle_base()
		: base_type(type_of<final_handle_type>())
	{
	}


	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}


private:
	result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);


protected:
	using base_type::sync_impl;

	static result<void> sync_impl(io::parameters_with_result<io::filesystem_open> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

result<final_handle<directory_handle_base>> block_open_directory(filesystem_handle const* base, input_path_view path, directory_handle_base::open_parameters const& args);
result<final_handle<directory_handle_base>> block_open_unique_directory(filesystem_handle const& base, directory_handle_base::open_parameters const& args);
result<final_handle<directory_handle_base>> block_open_temporary_directory(directory_handle_base::open_parameters const& args);
result<final_handle<directory_handle_base>> block_open_anonymous_directory(filesystem_handle const* base, directory_handle_base::open_parameters const& args);

} // namespace detail

using directory_handle = final_handle<detail::directory_handle_base>;

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
result<directory_handle> open_directory(input_path_view const path, P const& args = {});

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
result<directory_handle> open_directory(filesystem_handle const& base, input_path_view const path, P const& args = {});

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
result<directory_handle> open_unique_directory(filesystem_handle const& base, P const& args = {});

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
result<directory_handle> open_temporary_directory(P const& args = {});

template<parameters<directory_handle::open_parameters> P = directory_handle::open_parameters::interface>
result<directory_handle> open_anonymous_directory(filesystem_handle const& base, P const& args = {});


namespace detail {

enum class directory_stream_native_handle : uintptr_t {};

enum class directory_stream_iterator_native_handle : uintptr_t
{
	end = 0
};

class directory_stream_handle_base : public platform_handle
{
	using final_handle_type = final_handle<directory_stream_handle_base>;

	detail::linear<directory_stream_native_handle> m_stream;

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
	using fetch_parameters = basic_parameters;
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
			allio_ASSERT(m_entry.m_handle != directory_stream_iterator_native_handle::end);
			return m_entry;
		}

		[[nodiscard]] entry const* operator->() const
		{
			allio_ASSERT(m_entry.m_handle != directory_stream_iterator_native_handle::end);
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
	result<void> open(input_path_view const path, P const& args = {})
	{
		return block_open(nullptr, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(filesystem_handle const& base, input_path_view const path, P const& args = {})
	{
		return block_open(&base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(filesystem_handle const* const base, input_path_view const path, P const& args = {})
	{
		return block_open(base, path, args);
	}

	template<parameters<open_parameters> P = open_parameters::interface>
	result<void> open(directory_handle const& handle, P const& args = {})
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


	result<void> restart();

	template<parameters<fetch_parameters> P = fetch_parameters::interface>
	result<bool> fetch(P const& args = {})
	{
		return block_fetch(args);
	}

	template<parameters<directory_stream_handle_base::fetch_parameters> P = fetch_parameters::interface>
	basic_sender<io::directory_stream_fetch> fetch_async(P const& args = {});

	[[nodiscard]] entry_range entries() const
	{
		return entry_range(*this);
	}


	template<typename String = std::string, parameters<get_name_parameters> P = get_name_parameters::interface>
	result<String> get_name(entry const& entry, P const& args = {}) const
	{
		String string = {};
		allio_TRYD(block_get_name(entry, string, args));
		return static_cast<String&&>(string);
	}

	template<parameters<get_name_parameters> P = get_name_parameters::interface>
	result<size_t> get_name(entry const& entry, output_string_ref const& output, P const& args = {}) const
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

	result<void> block_open(filesystem_handle const* base, input_path_view path, open_parameters const& args);
	result<void> block_open(directory_handle const& handle, open_parameters const& args);

	result<bool> block_fetch(fetch_parameters const& args);
	result<size_t> block_get_name(entry const& entry, output_string_ref const& output, get_name_parameters const& args) const;

protected:
	using base_type::sync_impl;

	static result<void> sync_impl(io::parameters_with_result<io::directory_stream_open_path> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::directory_stream_open_handle> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::directory_stream_fetch> const& args);
	static result<void> sync_impl(io::parameters_with_result<io::directory_stream_get_name> const& args);

	template<typename H, typename O>
	friend result<void> allio::synchronous(io::parameters_with_result<O> const& args);
};

result<final_handle<directory_stream_handle_base>> block_open_directory_stream(filesystem_handle const* base, input_path_view path, directory_stream_handle_base::open_parameters const& args);
result<final_handle<directory_stream_handle_base>> block_open_directory_stream(directory_handle const& directory, directory_stream_handle_base::open_parameters const& args);

} // namespace detail

using directory_stream_handle = final_handle<detail::directory_stream_handle_base>;

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
result<directory_stream_handle> open_directory_stream(input_path_view const path, P const& args = {})
{
	return detail::block_open_directory_stream(nullptr, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
result<directory_stream_handle> open_directory_stream(filesystem_handle const& base, input_path_view const path, P const& args = {})
{
	return detail::block_open_directory_stream(&base, path, args);
}

template<parameters<directory_stream_handle::open_parameters> P = directory_stream_handle::open_parameters::interface>
result<directory_stream_handle> open_directory_stream(directory_handle const& directory, P const& args = {})
{
	return detail::block_open_directory_stream(directory, args);
}


namespace this_process {

result<directory_handle> open_current_directory();

template<typename Char = char>
result<basic_path<Char>> get_current_directory();

template<typename Char = char>
result<basic_path_view<Char>> copy_current_directory(std::span<Char> buffer);

result<void> set_current_directory(input_path_view path);

} // namespace this_process


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
	: directory_stream_handle::fetch_parameters
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

allio_API extern allio_HANDLE_IMPLEMENTATION(directory_handle);
allio_API extern allio_HANDLE_IMPLEMENTATION(directory_stream_handle);

} // namespace allio
